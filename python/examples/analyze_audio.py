"""Inspect VAD and loudness for a mono recording without denoiser models."""

import argparse
from giga_edge_enhance import LoudnessMeter, VoiceActivityDetector, read_audio


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input")
    args = parser.parse_args()
    recording = read_audio(args.input)
    if recording.samples.ndim != 1:
        raise ValueError("Pass a mono recording; select a channel explicitly for analysis")
    with (
        VoiceActivityDetector(sample_rate=recording.sample_rate) as vad,
        LoudnessMeter(recording.sample_rate) as meter,
    ):
        print(meter.update(recording.samples))
        for frame in vad.analyze(recording.samples):
            print(frame.start_sample, frame.sample_count, frame.score, frame.speech)


if __name__ == "__main__":
    main()
