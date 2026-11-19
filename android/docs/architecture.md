# Architecture

This describes how audio flows through the SDK and where responsibilities live. It reflects the
implementation in `sdk/src/main/cpp` and the Kotlin API.

## Layers

1. **Kotlin API** (`com.gigaedge.enhance.Processor`) — the public surface. It validates
   arguments and lifecycle state, holds a read/write lock for the native handle, and marshals
   results (telemetry, low-performance events) into Kotlin types.
2. **Thin JNI bridge** (`jni.cpp`) — maps each `nativeXxx` call to the matching native
   instance, converts arguments, translates C++ exceptions into Java exceptions, and constructs the
   `NativeTelemetry` and `LowPerformanceEvent` objects. It holds no long-lived state.
3. **Native processor** (`processor.cpp` / `processor.h`) — owns the DSP, a dedicated
   worker thread, the ring buffers, and the adaptive-latency monitor. The GigaEdge Enhance C++ core is linked
   **statically** into `libgigaedge-enhance-sdk.so`.

There is **one native instance per `Processor`** (one worker thread, one pair of ring
buffers). The native handle is never exposed to app code.

## Data flow through `process(...)`

1. The app passes a mono `FloatArray` input buffer to `Processor.process(...)`.
2. Kotlin validates `numFrames` and buffer sizes and checks the processor is open.
3. JNI forwards the call to the native instance for the handle.
4. Native `process(...)` pushes the input into a lock-free single-producer / single-consumer (SPSC)
   ring buffer and pops whatever processed output is currently available from a second ring buffer.
5. A dedicated **worker thread** wakes when enough input is queued, pops a full processing chunk,
   runs the enabled stages, and pushes results into the output ring buffer.
6. If the worker has not produced enough output yet, `process(...)` **zero-fills** the remaining
   output samples rather than blocking the caller.
7. Back in Kotlin, `process(...)` drains any queued low-performance events and delivers them (see
   [threading.md](threading.md)).

`maxProcessingChunkSeconds` defaults to 16 ms. The effective duration is the requested maximum capped
by `minLatency` and 256 ms, floored to 16 ms steps, with a 16 ms minimum. The worker derives frame
counts as `ceil(sample_rate_hz * effective_seconds)`, so a 48 kHz stream uses 768-frame chunks by
default and 2,304 frames for a 48 ms chunk.

## Ring buffers

Both buffers are SPSC and lock-free (`ring_buffer.cpp`): the audio thread is the sole producer of
input / consumer of output, and the worker thread is the sole consumer of input / producer of
output. Capacities are powers of two sized for the sample rate and at least two processing chunks. This decoupling is what lets
`process(...)` return without waiting for DSP.

## Feature stages

Stages are built lazily by `setFeatures(...)` and only rebuilt when their configuration changes:

- **Normalization** — an audio-processing chain of loudness control, noise gate, and limiter
  (`LC + NG + Lim`), configured from the sample rate (or a custom config via
  `unstableCustomNormalizationConfig`).
- **Noise suppression** — a denoiser created from a packed model loaded from app assets.

By default the worker runs normalization then noise suppression; `unstableReverseProcessingOrder`
swaps that. With no stage enabled, the worker copies input straight to output (pass-through).

## Adaptive latency

The native processor tracks output underruns ("refills") over a sliding window and raises the output
latency in steps to stabilize playback. When refills in the window reach the configured threshold, it
enqueues a `LowPerformanceEvent`. See
[telemetry-and-performance.md](telemetry-and-performance.md).

## Where validation lives

Argument validation (sample rate, frame count, buffer sizes, model-name format) lives primarily in
the Kotlin layer (`Validation`, and `Settings.init` for settings). The native layer
re-checks its own invariants and reports failures as exceptions that JNI maps to
`IllegalArgumentException` or `IllegalStateException`. `Settings` is the single source of truth
for settings validity — an invalid instance cannot be constructed.

## Source map

- `sdk/src/main/java/com/gigaedge/enhance/Processor.kt` — public API and lock.
- `sdk/src/main/cpp/jni.cpp` — JNI bridge and exception mapping.
- `sdk/src/main/cpp/processor.cpp` — worker thread, stages, adaptive latency.
- `sdk/src/main/cpp/ring_buffer.cpp` — lock-free SPSC ring buffer.
- `sdk/CMakeLists.txt` — native build; links the GigaEdge Enhance core statically.
