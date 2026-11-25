"""Enhance a NumPy recording without adding file-codec dependencies."""

import argparse

import numpy as np

from giga_edge_enhance import Denoiser


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("model", help="YAML configuration, model directory, or .enhance")
    parser.add_argument("input", help="float32 or int16 .npy recording")
    parser.add_argument("output", help="enhanced .npy recording")
    parser.add_argument("--sample-rate", type=int, required=True)
    parser.add_argument("--backend", choices=("onnx", "tflite"), default="onnx")
    args = parser.parse_args()
    audio = np.load(args.input, allow_pickle=False)
    with Denoiser(args.model, backend=args.backend) as model:
        np.save(args.output, model.process(audio, sample_rate=args.sample_rate), allow_pickle=False)


if __name__ == "__main__":
    main()
