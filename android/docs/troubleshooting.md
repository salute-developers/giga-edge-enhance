# Troubleshooting

Common issues when integrating or running the SDK and demo.

## Emulator microphone plays a tone instead of your voice

The Android emulator feeds a synthesized sine tone unless it is wired to your host mic. Enable
**Extended Controls → Microphone → Virtual microphone uses host audio input** (resets on restart),
or run `adb emu avd hostmicon` after each boot. On macOS also grant microphone access to Android
Studio under **System Settings → Privacy & Security → Microphone**. This is an emulator limitation,
not an SDK bug; physical devices capture real input. See [demo-app.md](demo-app.md).

## Unsupported WAV file (file processing)

The demo file path supports **mono, 16-bit PCM WAV** only. Selecting anything else fails with a
"supported WAV audio file" message. Non-PCM, multi-channel, or non-16-bit files are rejected while
reading the header. Re-encode to mono 16-bit PCM WAV, e.g.:

```bash
ffmpeg -i input.ext -ac 1 -ar 48000 -c:a pcm_s16le output.wav
```

## `UnsatisfiedLinkError`

The native library `libgigaedge-enhance-sdk.so` failed to load or a JNI method could not be resolved. Check:

- Your device/emulator ABI is one of `arm64-v8a`, `armeabi-v7a`, `x86_64`, `x86` (all are built by
  default).
- The SDK module actually built its native library (build from source needs **NDK 29.0.14206865**
  and **CMake 3.22.1**).
- You have **not** overridden the SDK's consumer ProGuard rules. In minified builds the JNI-facing
  types and native methods must be kept — the SDK ships these keeps automatically. See
  [proguard-r8.md](proguard-r8.md).

## `IllegalStateException` after `close()`

Using a processor after `close()` throws `IllegalStateException` ("Processor is already
closed"). `close()` is idempotent, but any `process` / `setFeatures` / `reset` / `telemetry` call
after it fails. Manage the lifecycle with `use { }` or construct a new processor. See
[threading.md](threading.md).

## `IllegalArgumentException` for invalid settings

`Settings` validates its invariants in `init`, so constructing (or `copy(...)`-ing) invalid
values throws `IllegalArgumentException` before you ever reach a processor. Check the message for the
offending field (for example `telemetryRoundingMilliseconds must be greater than zero`). Common
mistakes: non-positive window/rounding values, negative latency, or `NaN`/infinite doubles. See the
[API reference](api-reference.md#settings) for the exact constraints.

## `IllegalArgumentException` for a missing or invalid model

`setFeatures(...)` throws `IllegalArgumentException` when the requested noise-suppression model asset
is not packaged ("model asset not found"), or when the name contains characters outside letters,
digits, `.`, `_`, and `-`. Ensure the matching `:gigaedge-enhance-model-*` module is an `implementation`
dependency and select names from `Models.available(context)`. See [models.md](models.md).

## `IllegalArgumentException` for buffers or frame count

`process(...)` requires `numFrames >= 0` and both buffers to be at least `numFrames` long, and the
constructor requires `sampleRateHz > 0`. Violations throw `IllegalArgumentException`. Audio must be
mono.

## Output is silent or zero at the start

The native worker runs asynchronously, so the first `process(...)` calls can return zero-filled
output until processed audio is available. This is expected warm-up behavior, not an error. See
[architecture.md](architecture.md).
