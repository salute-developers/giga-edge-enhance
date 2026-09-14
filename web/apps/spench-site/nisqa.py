#!/usr/bin/env python3
"""Compute NISQA overall MOS for a single .webm (or any ffmpeg-readable) audio file.

Usage:
  python nisqa.py /path/to/audio.webm

Notes:
- Requires: numpy, torch, torchaudio, torchmetrics
- Prints ONLY the MOS (a single float) to stdout.
"""

from __future__ import annotations

import argparse
import json
import os
import subprocess
from pathlib import Path

if nisqa_weights_home := os.environ.get("NISQA_WEIGHTS_HOME"):
    os.environ["HOME"] = nisqa_weights_home

import numpy as np

import torch
import torchaudio

# Use TorchMetrics' functional API so we can compute per-chunk easily
from torchmetrics.functional.audio.nisqa import (
    non_intrusive_speech_quality_assessment as _nisqa_fn,
)


def _to_mono_clipped(x: np.ndarray) -> np.ndarray:
    """Ensure 1D float32 mono in [-1, 1]."""
    x = np.asarray(x, dtype=np.float32)
    if x.ndim == 2:  # (C, T) -> mono
        x = x.mean(axis=0)
    elif x.ndim > 2:
        x = x.reshape(-1)
    return np.clip(x, -1.0, 1.0)


@torch.no_grad()
def _nisqa_overall_mos(
    wav_np: np.ndarray,
    src_sr: int,
    target_fs: int,
    chunk_seconds: float | None,
) -> float:
    """Compute overall MOS.

    - Try full clip first.
    - On 'too long' RuntimeError, fall back to chunking into fixed-size windows
      and return length-weighted average over chunks.

    Returns:
        Overall MOS (float).
    """
    # resample if needed
    wav_t = torch.from_numpy(wav_np).unsqueeze(0)  # [1, T] at src_sr
    if target_fs != src_sr:
        wav_t = torchaudio.functional.resample(wav_t, src_sr, target_fs)
    wav_t = wav_t.squeeze(0)  # [T] at target_fs

    # try full clip directly
    try:
        out = _nisqa_fn(wav_t.unsqueeze(0), target_fs)  # shape [1,5]
        return float(torch.as_tensor(out).squeeze()[0].item())
    except RuntimeError as e:
        msg = str(e)
        # Too long -> segment; too short -> let it propagate
        if "Maximum number of mel spectrogram windows exceeded" not in msg:
            raise

    # fallback: chunking
    if not chunk_seconds or chunk_seconds <= 0:
        raise RuntimeError(
            "Audio too long for NISQA and chunking is disabled. "
            "Set --chunk-seconds > 0."
        )

    T = wav_t.numel()
    chunk_len = int(target_fs * float(chunk_seconds))
    if chunk_len <= 0:
        raise ValueError("--chunk-seconds must be > 0")

    start = 0
    num = 0.0
    den = 0.0
    while start < T:
        end = min(start + chunk_len, T)
        seg = wav_t[start:end]
        try:
            out = _nisqa_fn(seg.unsqueeze(0), target_fs)  # [1,5]
            mos_overall = float(torch.as_tensor(out).squeeze()[0].item())
            w = float(end - start)
            num += mos_overall * w
            den += w
        except RuntimeError as e:
            # tiny tails can be too short; ignore those
            if "number of mel spectrogram windows to be zero" in str(e):
                pass
            else:
                raise
        start = end

    if den == 0.0:
        raise RuntimeError("NISQA failed on all chunks (too short).")
    return num / den


def _ffprobe_sample_rate(path: str) -> int:
    """Return sample rate of the first audio stream using ffprobe."""
    cmd = [
        "ffprobe",
        "-v",
        "error",
        "-select_streams",
        "a:0",
        "-show_entries",
        "stream=sample_rate",
        "-of",
        "json",
        path,
    ]
    proc = subprocess.run(cmd, capture_output=True, text=True, check=True)
    info = json.loads(proc.stdout)
    sr = int(info["streams"][0]["sample_rate"])
    if sr <= 0:
        raise RuntimeError("ffprobe returned invalid sample rate")
    return sr


def load_webm_mono_np(path: str) -> tuple[np.ndarray, int]:
    """Load audio into mono float32 numpy array and return (audio, sr).

    Tries torchaudio first; falls back to ffmpeg.
    """
    # 1) torchaudio (works when ffmpeg/sox backends are available)
    try:
        wav_t, sr = torchaudio.load(path)  # [C,T]
        wav_np = wav_t.numpy()
        return _to_mono_clipped(wav_np), int(sr)
    except Exception:
        pass

    # 2) ffmpeg fallback: decode to mono f32le, keep original SR
    sr = _ffprobe_sample_rate(path)
    cmd = [
        "ffmpeg",
        "-v",
        "error",
        "-i",
        path,
        "-ac",
        "1",
        "-f",
        "f32le",
        "-",
    ]
    proc = subprocess.run(cmd, capture_output=True, check=True)
    audio = np.frombuffer(proc.stdout, dtype=np.float32)
    return _to_mono_clipped(audio), int(sr)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("webm", type=str, help="Path to input .webm (or any audio file ffmpeg can read)")
    ap.add_argument(
        "--target-fs",
        type=int,
        default=24000,
        help="Sample rate (Hz) used for MOS computation; input is resampled if needed (default: 24000)",
    )
    ap.add_argument(
        "--chunk-seconds",
        type=float,
        default=20.0,
        help="Chunk size (seconds) used when audio is too long for NISQA (default: 20.0)",
    )
    args = ap.parse_args()

    path = str(Path(args.webm))
    wav_np, src_sr = load_webm_mono_np(path)
    mos = _nisqa_overall_mos(wav_np, src_sr=src_sr, target_fs=int(args.target_fs), chunk_seconds=args.chunk_seconds)

    # Print only the score
    print(f"{mos:.6f}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
