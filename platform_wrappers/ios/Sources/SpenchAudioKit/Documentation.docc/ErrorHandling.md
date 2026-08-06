# Error Handling

How SpenchAudioKit reports failures.

## Overview

All Swift APIs throw ``SpenchError``. The enum bridges to
`Foundation.NSError` with the domain ``SpenchError/errorDomain`` so
existing Objective-C code paths see a typed `NSError` while pure
Swift call sites get pattern-matched cases.

## Catch by case

```swift
do {
    let unit = try SpenchAVAudioUnit(features: features, settings: settings)
} catch SpenchError.invalidArgument(let message) {
    showAlert("Invalid configuration: \(message)")
} catch SpenchError.outOfRange(let message) {
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
    assert(nsError.domain == SpenchError.errorDomain)
    if nsError.code == SpenchError.Code.invalidArgument.rawValue {
        // Surface the message to UI.
    }
}
```

## Common cases

- ``SpenchError/invalidArgument(_:)`` — bad parameter (e.g. zero
  sample rate, missing model bundle ID).
- ``SpenchError/outOfRange(_:)`` — value outside a supported range.
- ``SpenchError/unknown(_:)`` — unexpected pipeline failure surfaced
  from the underlying ObjC++ bridge.
