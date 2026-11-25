"""Enhance NumPy blocks from an iterator; output retains causal model delay."""

import argparse
import numpy as np
from giga_edge_enhance import Denoiser


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("model")
    parser.add_argument("input")
    parser.add_argument("output")
    parser.add_argument("--sample-rate", required=True, type=int)
    args = parser.parse_args()
    audio = np.load(args.input, allow_pickle=False)
    channels = 1 if audio.ndim == 1 else audio.shape[1]
    chunks = (audio[start : start + 1024] for start in range(0, len(audio), 1024))
    with Denoiser(args.model) as model:
        parts = list(model.process_chunks(chunks, sample_rate=args.sample_rate, channels=channels))
        output = np.concatenate(parts) if parts else audio[:0].copy()
        np.save(args.output, output, allow_pickle=False)


if __name__ == "__main__":
    main()
