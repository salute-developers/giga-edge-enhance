"""Cancel echo from synchronized mono microphone and playback-reference WAVs."""

import argparse
from giga_edge_enhance import EchoCanceller, EchoCancellerConfig, read_audio


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("microphone")
    parser.add_argument("reference")
    parser.add_argument("output")
    args = parser.parse_args()
    rate = read_audio(args.microphone).sample_rate
    config = EchoCancellerConfig(frame_size=rate // 100, filter_length=4096)
    with EchoCanceller(sample_rate=rate, config=config) as aec:
        aec.process_file(args.microphone, args.output, reference=args.reference)


if __name__ == "__main__":
    main()
