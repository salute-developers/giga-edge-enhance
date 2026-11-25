# Python API reference

## Audio and streams

Pass NumPy arrays with dtype `float32` (normalized PCM) or `int16`. Use shape `(samples,)` for mono and `(samples, channels)` for multichannel audio. Keep the dtype and channel count constant within a stream. Sample rates are in Hz.

| Operation | When to use it |
| --- | --- |
| `process(audio, …)` | Process a complete recording with fresh state. A denoiser compensates its model delay and returns audio at the requested rate. |
| `process_chunk(audio)` | Feed a stream. A call can return an empty block while the processor buffers input. |
| `flush()` | Read the remaining buffered output at end of input. Call `reset()` before reusing that stream. |
| `process_chunks(chunks, …)` | Iterate over a recording in chunks with fresh state. The iterator includes the final tail and preserves streaming delay. |
| `close()` or `with` | Release the model, stream or processor. |

Create a separate stream per recording. Streams can run concurrently and share model weights. Create models and streams outside real-time audio callbacks. [Streaming example](examples/stream_audio.py)

## Denoising

| Entry point | Input |
| --- | --- |
| `Denoiser(path, *, backend="onnx", engine_threads=1)` | Local YAML, a directory containing `config.yaml`, or a packed `.enhance` model. Relative weight paths resolve against the YAML directory. |
| `Denoiser.from_config(yaml, *, base_directory, …)` | YAML text and a directory for resolving relative paths. |
| `Denoiser.from_pretrained(repo_or_path, *, model="quality48", backend="onnx", …)` | A local export tree or Hugging Face repository. For a model repository root, the loader verifies weights and YAML against the package catalog. |

Use `available_models()` to list supported model/backend pairs. For Hub downloads, install the `hub` extra. Use `revision` to pin a model commit, `cache_dir` to select a cache and `local_files_only=True` for offline use. Passing a local YAML, packed file or individual export directory loads that model directly, with the same behavior as `Denoiser(path)`.

Pass the input rate to `model.process(audio, sample_rate=…)` or `model.stream(sample_rate=…, channels=1)`. Both accept `output_sample_rate`, which defaults to the input rate. A stream retains the model until the stream closes.

`model.info` reports the backend, model type, native rate, hop size in samples, engine thread count and delay in frames. `stream.latency_samples` reports model delay at the output rate. Resampling can buffer additional input. [NumPy example](examples/denoise_array.py)

## Files

Install the `io` extra for `read_audio`, `write_audio` and `process_file`. These helpers process a whole recording in memory. `read_audio(path)` returns an `AudioData` with `samples` and `sample_rate`. WAV output defaults to FLOAT for `float32` and PCM_16 for `int16`. Pass `subtype="PCM_16"` for a 16-bit PCM file from floating-point input. [File example](examples/process_recording.py)

## Level control

The six processors below accept `sample_rate=48000`, `channels=1` and a frozen configuration. For example, `Limiter(config=LimiterConfig(threshold_db=-6))` sets a −6 dB threshold. Times are in seconds except where specified. [Configuration fields and defaults](src/giga_edge_enhance/config.py)

| Processor | Controls |
| --- | --- |
| `AGC` | Target linear power, maximum gain in dB, averaging length in samples and per-sample adaptation steps |
| `RangeControl` | Target level, maximum gain and attack, hold and release times |
| `LoudnessControl` | Target in LUFS, gain limit, bypass time and optional VAD |
| `NoiseGate` | Threshold and attack, hold and release times |
| `Limiter` | Threshold, knee width and attack/release times |
| `Compressor` | Threshold, knee width, compression ratio and attack/release times |

`AudioProcessor(stages)` applies level-control stages in order. Rates and channel counts must match. Inspect `.gain` or `.state` after a streaming call. Recording methods process audio with fresh state.

## Analysis, resampling and echo cancellation

| Class | Behavior |
| --- | --- |
| `RNNoise` | Denoises speech and exposes a VAD score per channel. Supports 8, 16, 24, 32, 44.1 and 48 kHz. |
| `VoiceActivityDetector` | Scores mono frames at the same rates as RNNoise. Each `VadResult` includes the decision, score, first sample index and valid sample count. |
| `LoudnessMeter` | `update(audio)` returns momentary and short-term LUFS and sample peak in dBFS. Early readings include the startup window. |
| `MuteFrameDetector` | Detects relative level gaps across three adjacent windows of `chunk_length` samples. |
| `Resampler` | Converts from `sample_rate` to `output_sample_rate`. Flush the stream for the final filter tail. |
| `EchoCanceller` | Requires synchronized mono microphone and playback arrays of equal length, dtype and rate. Pass the playback track as `reference` when processing files. |

Use `EchoCancellerConfig` to set frame size and filter length in samples, premute time in seconds, delay compensation and residual suppression. [Echo cancellation](examples/cancel_echo.py) · [Analysis example](examples/analyze_audio.py)

## Pipelines

`Pipeline(stages)` owns fresh copies of its stages. Adjacent rates and channel counts must match. Put `EchoCanceller` first when using a playback reference, and insert a `Resampler` wherever the sample rate changes. An `Observer` passes audio through and exposes the most recent measurements in `pipeline.stages`.

`process_timed_chunk` returns an `AudioChunk`. Its timestamp is `(start_sample - latency_samples) / sample_rate`, so the first output timestamp can be negative. [Pipeline example](examples/asr_pipeline.py)

Use `set_log_level("warning")` to change native SDK logging. A catalog checksum mismatch raises `ModelIntegrityError`, a subclass of `ProcessingError`.
