# API reference

The public API lives in the package `com.gigaedge.spench` (SDK) and
`com.gigaedge.spench.models` (model name constants). Every member carries KDoc in source; this page
mirrors it for quick reference.

Units used throughout: **latencies in seconds**, **telemetry buckets and means in milliseconds**,
**sample rate in Hz**.

## `SpenchProcessor`

```kotlin
class SpenchProcessor(
    context: Context,
    sampleRateHz: Int,
    settings: SpenchSettings = SpenchSettings(),
    callbackExecutor: Executor? = null,
) : Closeable
```

Real-time mono audio processor backed by one native instance and a native worker thread. All
feature stages start disabled.

### Constructor parameters

| Parameter | Type | Default | Notes |
| --- | --- | --- | --- |
| `context` | `Context` | — | The application context and its `AssetManager` are retained for loading model assets. |
| `sampleRateHz` | `Int` | — | Input sample rate in Hz; must be `> 0`. |
| `settings` | `SpenchSettings` | `SpenchSettings()` | Immutable configuration; fixed for the processor lifetime. |
| `callbackExecutor` | `Executor?` | `null` | Dispatches `onLowPerformance`. `null` = run on the `process(...)` caller thread. |

Throws `IllegalArgumentException` if `sampleRateHz <= 0`.

### Members

- `val settings: SpenchSettings` — the configuration captured at construction.
- `var onLowPerformance: ((SpenchLowPerformanceEvent) -> Unit)?` — nullable, `@Volatile`. Listener
  for low-performance events; assignable at any time. See
  [telemetry-and-performance.md](telemetry-and-performance.md).
- `val telemetry: SpenchTelemetry` — a cumulative snapshot of stage timing histograms. Never blocks
  on an in-progress chunk. Throws `IllegalStateException` if closed.
- `fun process(input: FloatArray, output: FloatArray, numFrames: Int)` — process one chunk.
  - `numFrames` must be `>= 0`; `0` is a no-op.
  - `input` and `output` must each be at least `numFrames` long; larger buffers are allowed and only
    the first `numFrames` elements are used.
  - Non-blocking with respect to DSP; zero-fills output not yet produced by the worker.
  - Delivers any pending `onLowPerformance` events before returning.
  - Throws `IllegalArgumentException` if `numFrames < 0` or a buffer is too small;
    `IllegalStateException` if closed.
- `fun reset()` — clears buffered audio and streaming state; rebuilds enabled stages but keeps the
  current feature selection. Does **not** clear telemetry. Throws `IllegalStateException` if closed.
- `fun setFeatures(features: SpenchFeatures)` — enable/disable stages at runtime; only changed
  stages are rebuilt. Enabling noise suppression loads the model asset. Throws
  `IllegalArgumentException` for an invalid or missing model; `IllegalStateException` if closed.
- `override fun close()` — releases native resources; waits for in-flight calls; idempotent. Any
  further use throws `IllegalStateException`.

See [threading.md](threading.md) for concurrency guarantees across these members.

## `SpenchSettings`

```kotlin
data class SpenchSettings(
    val lowPerformanceWindowSeconds: Double = 1.0,
    val lowPerformanceMaxRefillTimes: Int = 4,
    val lowPerformanceMaxRefillTimeFactor: Double = 3.0,
    val minLatency: Double = 0.064,
    val latencyIncreaseStep: Double = 0.032,
    val telemetryRoundingMilliseconds: Double = 0.25,
    val useVadNormalization: Boolean = false,
    val unstableReverseProcessingOrder: Boolean = false,
    val unstableCustomNormalizationConfig: String? = null,
    val maxProcessingChunkSeconds: Double = 0.016,
)
```

Immutable configuration. **Invariants are enforced in `init`**, so an invalid instance can never
exist — constructing or `copy(...)`-ing invalid values throws `IllegalArgumentException`. This is the
single source of truth for settings validity: a held instance is always valid.

| Field | Type | Default | Unit | Constraint |
| --- | --- | --- | --- | --- |
| `lowPerformanceWindowSeconds` | `Double` | `1.0` | seconds | finite, `> 0` |
| `lowPerformanceMaxRefillTimes` | `Int` | `4` | count | `> 0` |
| `lowPerformanceMaxRefillTimeFactor` | `Double` | `3.0` | factor | finite, `>= 0` |
| `minLatency` | `Double` | `0.064` | seconds | finite, `>= 0` |
| `latencyIncreaseStep` | `Double` | `0.032` | seconds | finite, `>= 0` |
| `maxProcessingChunkSeconds` | `Double` | `0.016` | seconds | finite, `> 0`; effective value floors to 16 ms steps, is bounded by `minLatency` and 256 ms, and has a 16 ms minimum |
| `telemetryRoundingMilliseconds` | `Double` | `0.25` | milliseconds | finite, `> 0` |
| `useVadNormalization` | `Boolean` | `false` | — | ignored if a custom config is set |
| `unstableReverseProcessingOrder` | `Boolean` | `false` | — | experimental |
| `unstableCustomNormalizationConfig` | `String?` | `null` | — | experimental; overrides built-in normalization config |

## `SpenchFeatures`

```kotlin
data class SpenchFeatures(
    val normalization: Boolean = false,
    val noiseSuppression: String? = null,
)
```

The set of stages to run. The default disables both, matching a fresh processor.

- `normalization` — enables the normalization stage.
- `noiseSuppression` — model name, or `null` to disable. Must resolve to a merged asset
  `spench/models/{name}.spench` and may contain only letters, digits, `.`, `_`, and `-`.

## `SpenchTelemetry`

```kotlin
data class SpenchTelemetry(
    val normalizationCounters: Map<Double, Int>,
    val noiseSuppressionCounters: Map<Double, Int>,
)
```

Cumulative processing-time histograms. Keys are bucket midpoints **in milliseconds** (multiples of
the processor's `telemetryRoundingMilliseconds`); values are chunk counts.

- `val normalizationMeanMilliseconds: Double?` — count-weighted mean in ms, or `null` if no samples.
- `val noiseSuppressionMeanMilliseconds: Double?` — count-weighted mean in ms, or `null` if no
  samples.
- `operator fun minus(other: SpenchTelemetry): SpenchTelemetry` — per-bucket delta `this - other`,
  keeping only strictly positive counts. Use it to measure work between two snapshots.

## `SpenchLowPerformanceEvent`

```kotlin
data class SpenchLowPerformanceEvent(
    val sampleRateHz: Int,
    val currentLatencySeconds: Double,
    val meanProcessingTimeMilliseconds: Double?,
    val refillCount: Int,
)
```

Emitted when refills within the window reach `SpenchSettings.lowPerformanceMaxRefillTimes`.

- `sampleRateHz` — processor input sample rate in Hz.
- `currentLatencySeconds` — current adaptive output latency in seconds.
- `meanProcessingTimeMilliseconds` — recent mean chunk time in ms, or `null` when unavailable.
- `refillCount` — refills observed in the window when the event fired.

## `SpenchModels`

```kotlin
object SpenchModels {
    fun available(context: Context): List<String>
}
```

- `available(context)` — returns the packaged model names, **sorted**, with the `.spench` suffix
  stripped. Returns an empty list when no model assets are present. A returned name can be used as
  `SpenchFeatures.noiseSuppression`.

## Model name constants (`com.gigaedge.spench.models`)

- `SpenchQuality48Model.NAME` == `"quality48"` (from `:spench-model-quality48`).

## Internal (not public API)

`SpenchValidation` and `SpenchNativeTelemetry` are `internal` and not part of the supported surface;
do not depend on them.
