"""Regenerate inference-export metadata without copying weights into the package."""

import argparse
import hashlib
import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
CATALOG = ROOT / "python/src/giga_edge_enhance/models.json"


def content() -> str:
    entries = []
    for name, model_type, sample_rate, backends in (
        ("quality48", "denoiser_discr_web_model", 48000, ("onnx", "tflite")),
        ("hybrid48", "denoiser_hybrid_web_model", 48000, ("onnx", "tflite")),
        ("fast8", "denoiser_discr_hq_model", 8000, ("onnx",)),
    ):
        for backend in backends:
            directory = ROOT / "models" / name / backend
            files = {}
            for filename in ("config.yaml", f"model.{backend}"):
                data = (directory / filename).read_bytes()
                if data.startswith(b"version https://git-lfs.github.com/spec/v1"):
                    raise SystemExit(
                        f"Restore Git LFS before updating catalog: {directory / filename}"
                    )
                files[filename] = hashlib.sha256(data).hexdigest()
            entries.append(
                dict(
                    name=name,
                    backend=backend,
                    model_type=model_type,
                    sample_rate=sample_rate,
                    subfolder=f"{name}/{backend}",
                    minimum_sdk_version="1.0.0",
                    maximum_sdk_major=1,
                    files=files,
                )
            )
    return json.dumps(dict(format=1, models=entries), indent=2) + "\n"


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    generated = content()
    if args.check:
        if not CATALOG.is_file() or CATALOG.read_text() != generated:
            raise SystemExit(
                "Model catalog differs from checked-out exports; regenerate and review it."
            )
    else:
        CATALOG.write_text(generated)


if __name__ == "__main__":
    main()
