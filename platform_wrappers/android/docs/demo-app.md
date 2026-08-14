# Demo app

The `:demo-app` module is a small sample app that uses the SDK only through its public Android API.
It is a reference, not a production audio path.

## What it shows

- **Live mic monitor with a 0.5 s delay.** Captures mono PCM16 from the microphone, converts to
  float, runs it through `SpenchProcessor.process(...)`, buffers the processed stream in a 0.5-second
  delay line, converts back to PCM16, and plays it through `AudioTrack`. Low-performance events are
  surfaced in the UI (delivered via the main executor).
- **Real-time-paced WAV file processing.** Lets you pick a WAV file and an output destination, then
  processes it chunk-by-chunk through the same `process(...)` API — **wall-clock paced** to the audio
  duration so it behaves like a live stream rather than an offline batch conversion. There is no
  speaker playback in this mode; it writes a WAV file.
- **Settings screen.** Edits every `SpenchSettings` field. Invalid input surfaces the
  `IllegalArgumentException` message inline (settings validity is enforced by `SpenchSettings`
  itself). Saved settings apply to the next live/file run.

The live and file paths share the same processing shape: create a processor, `setFeatures(...)`,
`process(...)` in chunks, `close()`.

## Running it

1. Open the project in Android Studio and let Gradle sync.
2. Install `demo-app` on a device or emulator.
3. Choose whether normalization is enabled and pick a noise-suppression model (or `Off`). The
   picker is populated at runtime from `SpenchModels.available(context)`.
4. Optionally enable **Record live input/output WAV dumps** before starting live mode.
5. Tap **Start live demo** to toggle mic capture, processing, and 0.5-second delayed playback.
6. Tap **Select file and process** to pick a WAV file and output path and run the real-time-paced
   file flow.

Supported file input is **mono 16-bit PCM WAV**; output is WAV. The demo requires the
`RECORD_AUDIO` permission for live mode.

## Downloading live dump files

When **Record live input/output WAV dumps** is enabled, the demo writes to app-internal storage:

- `files/spench-live-input.wav`
- `files/spench-live-output.wav`

Copy them off the device with:

```bash
adb exec-out run-as com.gigaedge.spench.demo cat files/spench-live-input.wav > spench-live-input.wav
adb exec-out run-as com.gigaedge.spench.demo cat files/spench-live-output.wav > spench-live-output.wav
```

## Emulator microphone caveats

The Android emulator does not connect to your host microphone by default and feeds `AudioRecord` a
synthesized sine tone. If live mode plays a steady tone instead of your voice:

- Open the emulator's **Extended Controls** (the `...` button) → **Microphone** → enable **Virtual
  microphone uses host audio input**. This setting resets on every emulator restart.
- Or run `adb emu avd hostmicon` after each boot.
- On macOS, grant microphone access to Android Studio under **System Settings → Privacy & Security →
  Microphone**, then relaunch.

This is an emulator/host limitation, not an SDK or demo bug; physical devices capture real input
directly. See [troubleshooting.md](troubleshooting.md).

## Scope / limitations

- The live path is intentionally simple: no foreground service, no audio-focus/route-change handling,
  minimal error recovery. It is **not** a production monitoring path.
- The file path supports only mono 16-bit PCM WAV in and writes WAV out.
