# Threading

The threading contract is precise and important. This page states exactly what is safe to call
concurrently and how callbacks are delivered.

## `process(...)`

- Call it from a **single thread** — typically your audio/capture thread.
- It is **non-blocking with respect to DSP**: it moves samples through lock-free SPSC ring buffers
  and **zero-fills** any output the native worker has not produced yet. It never waits for
  processing to complete.
- It delivers pending low-performance events before returning (see below).

## Concurrent calls

`process`, `setFeatures`, `reset`, and reading `telemetry` **may be called concurrently from
different threads** and do **not** serialize against each other:

- The Kotlin layer uses a read/write lock; these four operations take the **read** lock, so they run
  concurrently.
- The native layer is internally thread-safe.
- Reading `telemetry` never waits for an in-progress processing chunk. Polling telemetry from a UI or
  background thread therefore **cannot stall the audio thread**.

A typical pattern: drive `process(...)` from the audio thread while a UI thread flips features via
`setFeatures(...)` and polls `telemetry` for metering.

## `close()`

- `close()` is the **only exclusive operation**. It takes the **write** lock, so it waits for any
  in-flight `process` / `setFeatures` / `reset` / `telemetry` call to finish, then destroys the
  native processor.
- It is **idempotent** — calling it more than once is safe.
- Using the processor after `close()` throws `IllegalStateException`.

Because `close()` waits for in-flight calls, do not call it from a thread that is simultaneously
inside another processor call on the same instance.

## Low-performance callback delivery

- Callbacks are delivered **after `process(...)` drains pending native events**. Native code never
  calls Kotlin callbacks directly.
- With `callbackExecutor = null`, callbacks run **synchronously on the `process(...)` caller
  thread**. Keep them short to avoid stalling the audio thread.
- With an executor supplied, each event is delivered via `callbackExecutor.execute { ... }`.

Example that keeps callback work off the audio thread by using the main executor:

```kotlin
import androidx.core.content.ContextCompat

val processor = SpenchProcessor(
    context = context,
    sampleRateHz = 48_000,
    callbackExecutor = ContextCompat.getMainExecutor(context),
)
processor.onLowPerformance = { event ->
    // Runs on the main thread; safe to touch UI here.
    updateLatencyLabel(event.currentLatencySeconds)
}
```

## Telemetry lifetime

`telemetry` is **cumulative for the processor lifetime** and is **not cleared by `reset()`**.
Compute a per-interval delta via snapshot subtraction:

```kotlin
val before = processor.telemetry
processor.process(input, output, input.size)
val delta = processor.telemetry - before
```

See [telemetry-and-performance.md](telemetry-and-performance.md) for details.
