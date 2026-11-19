# Quick Start

## Create an audio unit

```swift
import AVFoundation
import EnhanceAudioKit

let settings = EnhanceSettings(minLatency: 0.08,
                               latencyIncreaseStep: 0.04,
                               telemetryRoundingMilliseconds: 0.25,
                               maxProcessingChunkSeconds: 0.032)

let features = EnhanceFeatures(normalization: true,
                               noiseSuppression: .quality48)

let unit = try EnhanceAVAudioUnit(features: features, settings: settings)
```

## Update features later

```swift
try unit.setFeatures(EnhanceFeatures(normalization: true, noiseSuppression: nil))
```

## Observe low-performance events

```swift
unit.onLowPerformance = { event in
    print("GigaEdge Enhance underrun count: \(event.refillCount)")
}
```

## Read telemetry

```swift
let baseline = unit.telemetry
// ... after processing some audio ...
let delta = unit.telemetry - baseline
print(delta.audioProcessingMeanMilliseconds as Any)
```

> Tip: Read ``EnhanceSettings/telemetryRoundingMilliseconds`` to know
> the bucket size used by the histograms in ``EnhanceTelemetry``.

## Direct buffer processing

For tests, batch jobs, or non-`AVAudioEngine` flows:

```swift
let processor = try EnhanceProcessor(sampleRate: 48_000, settings: .default)
try processor.setFeatures(.standard)

var buffer = makeMonoFloatBuffer()
buffer.withUnsafeMutableBufferPointer { pointer in
    processor.processInPlace(pointer.baseAddress!, frameCount: pointer.count)
}
```

`maxProcessingChunkSeconds` controls only the real-time audio-unit worker;
direct processing keeps the buffer boundaries supplied by the caller.
