# Telemetry

Inspect Spench processing histograms to understand pipeline cost.

## Overview

`SpenchAUAudioUnit` and `SpenchProcessor` continuously record per-stage
processing durations into ``SpenchTelemetry``. Buckets are quantized by
``SpenchSettings/telemetryRoundingMilliseconds`` so the histograms stay
compact even on long-running sessions.

## Read counters

```swift
let snapshot = unit.telemetry
let normalisationMean = snapshot.audioProcessingMeanMilliseconds ?? 0
let denoiseMean = snapshot.denoiserMeanMilliseconds ?? 0
print("normalize=\(normalisationMean)ms denoise=\(denoiseMean)ms")
```

## Sample windows with subtraction

Telemetry is cumulative, but the `-` operator returns the positive
delta between two snapshots. This is the recommended way to compute
windowed statistics during a recording session.

```swift
let baseline = unit.telemetry
// ... process audio for a while ...
let window = unit.telemetry - baseline
print(window.audioProcessingCounters)
```

## Choose bucket size

Bucket size trades off memory for resolution:

- 0.25 ms (default) is fine for 16 ms render quanta on modern devices.
- 1 ms bucketing keeps very long sessions cheap if you only need
  coarse trends.
- Sub-millisecond bucketing is rarely useful because per-chunk timing
  jitter dominates at that scale.
