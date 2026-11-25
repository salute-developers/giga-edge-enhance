"""Prepare mono 16 kHz audio for an external ASR pipeline; ASR is optional."""

import argparse
from giga_edge_enhance import Denoiser, Limiter, Pipeline, read_audio, write_audio


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("model")
    parser.add_argument("input")
    parser.add_argument("output")
    parser.add_argument(
        "--torch", action="store_true", help="also demonstrate optional PyTorch conversion"
    )
    args = parser.parse_args()
    recording = read_audio(args.input)
    if recording.samples.ndim != 1:
        raise ValueError("ASR example expects mono; choose a channel explicitly")
    with Denoiser(args.model) as model:
        with (
            model.stream(sample_rate=recording.sample_rate, output_sample_rate=16000) as stream,
            Limiter(sample_rate=16000) as limiter,
        ):
            with Pipeline([stream, limiter]) as pipeline:
                enhanced = pipeline.process(recording.samples)
    write_audio(args.output, enhanced, 16000)
    if args.torch:
        import torch

        tensor = torch.from_numpy(enhanced)
        print("ASR tensor", tensor.shape, "at 16000 Hz")


if __name__ == "__main__":
    main()
