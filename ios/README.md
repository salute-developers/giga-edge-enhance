# GigaEdgeEnhanceSDK

iOS wrapper and demo app for GigaEdge Enhance audio processing, with:

- C++20 `GigaEdge Enhance` core (CMake static library)
- Objective-C++ exception-safe wrapper with private framework module
- Swift API + custom `AUAudioUnit`/`AVAudioUnit` factory
- Unified `EnhanceError` Swift error type (bridged to `NSError`)
- DocC documentation catalog with static-site generation
- XCFramework packaging
- Demo app (`AVAudioEngine`) for live mic + realtime file processing
- Unit tests + XCUITest

The SDK version comes from `VERSION_NUMBER` at the repository root and is applied to the
framework as `MARKETING_VERSION` in `project.yml`.

## Stack

- Swift 5.9+
- iOS 15+
- C++20
- CMake
- XcodeGen
- Xcode / xcodebuild

## Project structure

- `../core/` C++ core library used by `scripts/build_core.sh`
- `Sources/EnhanceAudioKit/` framework source (ObjC++ wrapper + Swift API + Audio Unit)
- `Sources/AudioDemoApp/` demo app (single-screen UI)
- `Tests/EnhanceAudioKitTests/` unit tests for wrappers and audio unit factory
- `Tests/AudioDemoAppUITests/` UI test + fixture wav
- `scripts/` build/test/lint helpers
- `project.yml` XcodeGen spec

## Prerequisites

1. Install Xcode.
2. Install XcodeGen:

```bash
brew install xcodegen
```

3. (Optional, linting) Install SwiftLint:

```bash
brew install swiftlint
```

## Generate project

```bash
./scripts/generate_project.sh
```

This creates `GigaEdgeEnhanceSDK.xcodeproj`.

## Build each part

### 1) Build C++ static libraries (CMake)

```bash
./scripts/build_core.sh
```

Outputs:

- Simulator fat static library (`arm64 + x86_64`):
  `build/core/simulator/libgiga-edge-enhance.a`
- Device static library (`arm64`):
  `build/core/device/libgiga-edge-enhance.a`

### 2) Build XCFramework

```bash
./scripts/build_xcframework.sh
```

Output:

- `build/xcframework/EnhanceAudioKit.xcframework`
- `build/xcframework/EnhanceModelFast8.bundle`
- `build/xcframework/EnhanceModelQuality48.bundle`
- `build/xcframework/EnhanceModelHybrid48.bundle`

### 3) Build demo app (simulator)

```bash
./scripts/build_app.sh
```

### 4) Build DocC documentation

```bash
./scripts/build_docs.sh
```

Outputs:

- `build/docs/.../EnhanceAudioKit.doccarchive` — open in Xcode for the
  Quick Help-style browsable archive.
- `build/docs/static-site/` — static HTML site ready for any web host.

To host under a sub-path (e.g. GitHub Pages project site), set
`DOCC_HOSTING_BASE_PATH`:

```bash
DOCC_HOSTING_BASE_PATH=gigaedge-enhance-ios ./scripts/build_docs.sh
```

Preview locally:

```bash
python3 -m http.server --directory build/docs/static-site 8000
```

## Run app in Xcode

1. Generate project: `./scripts/generate_project.sh`
2. Open `GigaEdgeEnhanceSDK.xcodeproj`
3. Select `AudioDemoApp` scheme
4. Select simulator (e.g. iPhone 17)
5. Run

## App features

- **Live demo**: microphone -> custom gain audio unit -> 1 second delay -> speaker
- **File mode**: pick file -> realtime processing through same custom audio unit -> write output WAV -> share sheet export
- Main screen with model selection, processing toggle, live/file actions, save action, status label (`OK` green / `FAIL` red), low-performance indicator, 1-second mean telemetry labels for audio processing and denoising, and error text area
- EnhanceSettings screen for configuring low-performance detection and normalization behavior used by the next live/file processing start

## SDK settings

`EnhanceSettings` configures the iOS wrapper at audio-unit creation time.

Available fields:

- `lowPerformanceWindowSeconds`
- `lowPerformanceMaxRefillTimes`
- `lowPerformanceMaxRefillTimeFactor`
- `minLatency`
- `latencyIncreaseStep`
- `maxProcessingChunkSeconds`
- `telemetryRoundingMilliseconds`
- `useVadNormalization`
- `unstableReverseProcessingOrder`
- `unstableCustomNormalizationConfig`

Usage example:

```swift
let settings = EnhanceSettings(minLatency: 0.08,
                               latencyIncreaseStep: 0.04,
                               telemetryRoundingMilliseconds: 0.25,
                               useVadNormalization: true,
                               maxProcessingChunkSeconds: 0.032)

let unit = try EnhanceAVAudioUnit(features: .standard, settings: settings)
```

Notes:

- EnhanceSettings are init-time only for a given `EnhanceAUAudioUnit` / `EnhanceAVAudioUnit` instance.
- To apply different settings, create a new audio unit or restart the processing flow that owns it.
- `maxProcessingChunkSeconds` defaults to 16 ms. The AU worker floors it to 16 ms steps, caps it by
  `minLatency` and 256 ms, and never processes less than 16 ms. It does not change direct
  `EnhanceProcessor` buffer processing.

## Models

Built-in model descriptors:

- `EnhanceModelDescriptor.fast8`
- `EnhanceModelDescriptor.quality48`
- `EnhanceModelDescriptor.hybrid48`

```swift
let features = EnhanceFeatures(normalization: true,
                               noiseSuppression: .quality48)
```

Use `.hybrid48` to select the hybrid 48 kHz model. Include the corresponding
model bundle in your application resources.

`EnhanceModelCatalog.availableModels()` discovers every bundle whose
`Info.plist` declares a `EnhanceModelID`. Discovery is cached for the
process lifetime; call `EnhanceModelCatalog.invalidateCache()` if your
app loads new bundles dynamically.

## Errors

All Swift APIs throw `EnhanceError`:

```swift
do {
    let unit = try EnhanceAVAudioUnit(features: features, settings: settings)
} catch EnhanceError.invalidArgument(let message) {
    log.warning("Invalid GigaEdge Enhance configuration: \(message)")
} catch {
    log.error("GigaEdge Enhance failed to start: \(error)")
}
```

Bridged `NSError` instances use the domain `EnhanceError.errorDomain` and
match the C `EnhanceErrorCode` values.

## EnhanceTelemetry

`EnhanceAVAudioUnit` and `EnhanceAUAudioUnit` expose cumulative processing telemetry snapshots via `telemetry`.

EnhanceTelemetry contains two histograms:

- `audioProcessingCounters: [Double: Int]`
- `denoiserCounters: [Double: Int]`

Each key is a rounded processing time in milliseconds, and each value is the number of observed calls in that bucket.
Rounding granularity is controlled by `EnhanceSettings.telemetryRoundingMilliseconds`.

`EnhanceTelemetry` also provides:

- histogram subtraction with `-`, clamped at zero for bucket counts
- `audioProcessingMeanMilliseconds`
- `denoiserMeanMilliseconds`

Example:

```swift
let before = unit.telemetry
// ... wait or process more audio ...
let delta = unit.telemetry - before

print(delta.audioProcessingMeanMilliseconds as Any)
print(delta.denoiserMeanMilliseconds as Any)
```

## Low performance event

The custom audio unit emits a low-performance event when repeated output underruns indicate that the processing pipeline cannot keep the output buffer filled reliably.

Delivery options:

- Direct callback via `EnhanceAVAudioUnit.onLowPerformance` / `EnhanceAUAudioUnit.onLowPerformance`
- `NotificationCenter` notification: `Notification.Name.gigaEdgeEnhanceLowPerformance`

Event payload (`EnhanceLowPerformanceEvent`):

- `sampleRate`
- `currentLatency`
- `meanProcessingTimeMilliseconds`
- `refillCount`

Callback example:

```swift
unit.onLowPerformance = { event in
    print("Low performance at \(event.sampleRate) Hz, latency \(event.currentLatency)s")
}
```

Notification example:

```swift
NotificationCenter.default.addObserver(forName: .gigaEdgeEnhanceLowPerformance,
                                       object: unit.auAudioUnit,
                                       queue: .main) { notification in
    let event = notification.userInfo?[NotificationUserInfoKey.lowPerformanceEvent] as? EnhanceLowPerformanceEvent
    print(event?.refillCount as Any)
}
```

## Tests

### Unit tests (framework wrappers)

```bash
xcodebuild -project GigaEdgeEnhanceSDK.xcodeproj -scheme EnhanceAudioKit -destination "platform=iOS Simulator,name=iPhone 17" test
```

Covers:

- Swift wrapper processing correctness
- Swift wrapper error paths
- Custom audio unit factory instantiation

### UI tests (feature #2: file processing + save)

Yes, `XCUITest` is the correct tool here.

```bash
xcodebuild -project GigaEdgeEnhanceSDK.xcodeproj -scheme AudioDemoApp -destination "platform=iOS Simulator,name=iPhone 17" test
```

`AudioDemoAppUITests` validates:

1. Start file processing flow
2. Wait for completion
3. Trigger save action
4. Verify `OK` status and no error text

### All tests via script

```bash
./scripts/test.sh
```

### Optional knobs

Environment variables to customize test execution:

| Variable | Default | Description |
|----------|---------|-------------|
| `TEST_DEVICE_NAME` | Automatic | First available iPhone in the selected Xcode SDK runtime; set to require a specific device name |
| `ERASE_SIMULATOR` | `1` | Erase simulator before tests (`1` = erase, `0` = skip erase) |
| `GEE_TEST_RETRIES` | `1` | Retry count for `EnhanceAudioKit` unit tests |
| `APP_TEST_RETRIES` | `3` | Retry count for `AudioDemoApp` UI tests |

Simulator preparation uses the runtime matching `xcrun --sdk iphonesimulator --show-sdk-version`.
Only the selected device is reset. Commands and boot progress are logged, each preparation command
has a three-minute timeout, and a missing runtime or failed boot stops the test run immediately.

Examples:

```bash
# Run tests on a different simulator without erasing state
TEST_DEVICE_NAME="iPhone 15" ERASE_SIMULATOR=0 ./scripts/test.sh

# Increase retries for flaky CI environment
GEE_TEST_RETRIES=3 APP_TEST_RETRIES=5 ./scripts/test.sh
```

## UI test fixture audio

Current fixture file:

- `Tests/AudioDemoAppUITests/Fixtures/UITestInput.wav`

If you want to replace it, keep the same filename and format (`wav`), or adjust the test resource name in `Tests/AudioDemoAppUITests/AudioDemoAppUITests.swift`.

## Lint

```bash
./scripts/lint.sh
```

Uses `.swiftlint.yml`.
