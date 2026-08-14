# Quick Start

## Create an audio unit

```swift
import AVFoundation
import SpenchAudioKit

let settings = SpenchSettings(minLatency: 0.08,
                               latencyIncreaseStep: 0.04,
                               telemetryRoundingMilliseconds: 0.25,
                               maxProcessingChunkSeconds: 0.032)

let features = SpenchFeatures(normalization: true,
                              noiseSuppression: .lite24k)

let unit = try SpenchAVAudioUnit(features: features, settings: settings)
```

## Update features later

```swift
try unit.setFeatures(SpenchFeatures(normalization: true, noiseSuppression: nil))
```

## Observe low-performance events

```swift
unit.onLowPerformance = { event in
    print("Spench underrun count: \(event.refillCount)")
}
```

## Read telemetry

```swift
let baseline = unit.telemetry
// ... after processing some audio ...
let delta = unit.telemetry - baseline
print(delta.audioProcessingMeanMilliseconds as Any)
```

> Tip: Read ``SpenchSettings/telemetryRoundingMilliseconds`` to know
> the bucket size used by the histograms in ``SpenchTelemetry``.

## Direct buffer processing

For tests, batch jobs, or non-`AVAudioEngine` flows:

```swift
let processor = try SpenchProcessor(sampleRate: 48_000, settings: .default)
try processor.setFeatures(.standard)

var buffer = makeMonoFloatBuffer()
buffer.withUnsafeMutableBufferPointer { pointer in
    processor.processInPlace(pointer.baseAddress!, frameCount: pointer.count)
}
```

`maxProcessingChunkSeconds` controls only the real-time audio-unit worker;
direct processing keeps the buffer boundaries supplied by the caller.
