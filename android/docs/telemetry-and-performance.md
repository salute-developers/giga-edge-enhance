# Telemetry and performance

The SDK exposes two performance signals: cumulative **timing telemetry** (`Telemetry`) and an
adaptive-latency **low-performance event** (`LowPerformanceEvent`).

## Telemetry histograms

`Processor.telemetry` returns a `Telemetry` snapshot with two histograms:

- `normalizationCounters: Map<Double, Int>`
- `noiseSuppressionCounters: Map<Double, Int>`

Each key is a bucket midpoint **in milliseconds** — a multiple of the processor's
`telemetryRoundingMilliseconds` (default `0.25` ms). Each value is the number of processed chunks
whose stage duration fell in that bucket. A chunk duration is selected by
`maxProcessingChunkSeconds`, so telemetry entries are not necessarily 16 ms of audio. Only stages that
actually ran contribute counts, so with
normalization off the normalization histogram stays empty.

Telemetry is **cumulative for the processor lifetime** and is **not** cleared by `reset()`.

## Snapshot subtraction for deltas

To measure the work done over an interval, capture a snapshot, do work, capture again, and subtract:

```kotlin
val before = processor.telemetry
repeat(100) { processor.process(input, output, input.size) }
val delta = processor.telemetry - before
```

`Telemetry.minus` computes a per-bucket difference and keeps **only strictly positive counts**,
so the result is a clean "what happened during this interval" histogram.

## Mean helpers

`normalizationMeanMilliseconds` and `noiseSuppressionMeanMilliseconds` return the **count-weighted
mean** duration in milliseconds, or `null` when the corresponding histogram is empty. They work on
both absolute snapshots and deltas:

```kotlin
val delta = processor.telemetry - before
val normMeanMs: Double? = delta.normalizationMeanMilliseconds
val nsMeanMs: Double? = delta.noiseSuppressionMeanMilliseconds
```

Reading telemetry never blocks on an in-progress chunk, so polling it from a UI/background thread is
safe and will not stall the audio thread (see [threading.md](threading.md)).

## Low-performance / adaptive-latency signal

When the native worker cannot keep up, the output ring buffer underruns and must be "refilled". The
processor tracks refills over `lowPerformanceWindowSeconds` and **raises output latency** in steps of
`latencyIncreaseStep` (starting from `minLatency`) to stabilize playback. When the number of refills
in the window reaches `lowPerformanceMaxRefillTimes`, it emits a `LowPerformanceEvent`:

```kotlin
data class LowPerformanceEvent(
    val sampleRateHz: Int,
    val currentLatencySeconds: Double,
    val meanProcessingTimeMilliseconds: Double?,
    val refillCount: Int,
)
```

- `currentLatencySeconds` — the raised, current output latency in seconds.
- `meanProcessingTimeMilliseconds` — recent mean chunk processing time in ms, or `null` if no
  measurement is available yet.
- `refillCount` — how many refills were counted in the window when the event fired.

`lowPerformanceMaxRefillTimeFactor` bounds how long the worker may take to refill (relative to the
current latency) before the refill requirement is relaxed.

### Receiving events

Set `onLowPerformance`. Delivery follows the [threading contract](threading.md):

```kotlin
processor.onLowPerformance = { event ->
    Log.i("GigaDenoiser", "latency=${event.currentLatencySeconds}s refills=${event.refillCount}")
}
```

- Events are delivered **after `process(...)` drains them** — never directly from native code.
- With `callbackExecutor = null` they run on the `process(...)` caller thread; with an executor they
  run via `callbackExecutor.execute { ... }`.

Keep synchronous callbacks short; move any heavy work off the audio thread (for example by supplying
a `callbackExecutor`).
