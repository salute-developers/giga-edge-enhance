# Error Handling

How GigaDenoiserAudioKit reports failures.

## Overview

All Swift APIs throw ``DenoiserError``. The enum bridges to
`Foundation.NSError` with the domain ``DenoiserError/errorDomain`` so
existing Objective-C code paths see a typed `NSError` while pure
Swift call sites get pattern-matched cases.

## Catch by case

```swift
do {
    let unit = try DenoiserAVAudioUnit(features: features, settings: settings)
} catch DenoiserError.invalidArgument(let message) {
    showAlert("Invalid configuration: \(message)")
} catch DenoiserError.outOfRange(let message) {
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
    assert(nsError.domain == DenoiserError.errorDomain)
    if nsError.code == DenoiserError.Code.invalidArgument.rawValue {
        // Surface the message to UI.
    }
}
```

## Common cases

- ``DenoiserError/invalidArgument(_:)`` — bad parameter (e.g. zero
  sample rate, missing model bundle ID).
- ``DenoiserError/outOfRange(_:)`` — value outside a supported range.
- ``DenoiserError/unknown(_:)`` — unexpected pipeline failure surfaced
  from the underlying ObjC++ bridge.
