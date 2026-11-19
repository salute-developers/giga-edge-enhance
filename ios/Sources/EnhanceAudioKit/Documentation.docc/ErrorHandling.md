# Error Handling

How EnhanceAudioKit reports failures.

## Overview

All Swift APIs throw ``EnhanceError``. The enum bridges to
`Foundation.NSError` with the domain ``EnhanceError/errorDomain`` so
existing Objective-C code paths see a typed `NSError` while pure
Swift call sites get pattern-matched cases.

## Catch by case

```swift
do {
    let unit = try EnhanceAVAudioUnit(features: features, settings: settings)
} catch EnhanceError.invalidArgument(let message) {
    showAlert("Invalid configuration: \(message)")
} catch EnhanceError.outOfRange(let message) {
    showAlert("Out of range: \(message)")
} catch {
    log("Unexpected: \(error)")
}
```

## Bridge to NSError

```swift
do {
    _ = try settings.validated()
} catch {
    let nsError = error as NSError
    assert(nsError.domain == EnhanceError.errorDomain)
    if nsError.code == EnhanceError.Code.invalidArgument.rawValue {
        // Surface the message to UI.
    }
}
```

## Common cases

- ``EnhanceError/invalidArgument(_:)`` — bad parameter (e.g. zero
  sample rate, missing model bundle ID).
- ``EnhanceError/outOfRange(_:)`` — value outside a supported range.
- ``EnhanceError/unknown(_:)`` — unexpected pipeline failure surfaced
  from the underlying ObjC++ bridge.
