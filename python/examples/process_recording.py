"""Enhance a WAV recording; install the optional [io] extra."""

import argparse
from giga_edge_enhance import Denoiser


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("model")
    parser.add_argument("input")
    parser.add_argument("output")
    parser.add_argument("--backend", default="onnx", choices=("onnx", "tflite"))
    args = parser.parse_args()
    with Denoiser(args.model, backend=args.backend) as model:
        model.process_file(args.input, args.output)


if __name__ == "__main__":
    main()
