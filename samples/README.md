# Listening sample

[Noisy input](input-48k.wav) · [15-second comparison](compare-48k.wav)

The comparison plays the same phrase three times, starting at 0:00 (input), 0:05 (`quality48`) and 0:10 (`hybrid48`). Both models ran through the Python SDK with ONNX and default settings. All three clips use the same gain.

The source is Garth Comira reading chapter 2 of *The Ashiel Mystery: A Detective Story*, LibriSpeech utterance `5703-47212-0000`. The first ten seconds were resampled to 48 kHz and mixed with filtered noise and hum at a speech-to-noise ratio of 5 dB. The source recording and these edited samples are licensed under CC BY 4.0. [Recording](https://librosa.org/data/audio/5703-47212-0000.hq.ogg) · [Attribution](https://librosa.org/data/audio/5703-47212-0000.txt) · [License](https://creativecommons.org/licenses/by/4.0/)
