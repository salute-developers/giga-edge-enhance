"""Optional SoundFile helpers; no codec dependency is imported until a call."""

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path

import numpy as np

from .denoiser import Audio, Denoiser, _audio, _pcm


@dataclass(frozen=True)
class AudioData:
    samples: Audio
    sample_rate: int


def _soundfile():
    try:
        import soundfile
    except ImportError as error:
        raise ImportError("Install giga-edge-enhance[io] to read/write audio files") from error
    return soundfile


def read_audio(path: str | Path, *, dtype: str = "float32") -> AudioData:
    """Read normalized mono/channel-last audio with its actual sample rate."""
    if dtype not in ("float32", "int16"):
        raise ValueError("dtype must be 'float32' or 'int16'")
    soundfile = _soundfile()
    stored_float = soundfile.info(str(path)).subtype in ("FLOAT", "DOUBLE")
    read_dtype = "float32" if stored_float else dtype
    samples, sample_rate = soundfile.read(str(path), dtype=read_dtype, always_2d=False)
    samples = _audio(samples)
    if dtype == "int16" and samples.dtype == np.float32:
        samples = _pcm(samples, np.dtype(dtype))
    return AudioData(samples, int(sample_rate))


def write_audio(
    path: str | Path, audio: Audio, sample_rate: int, *, subtype: str | None = None
) -> Path:
    """Write WAV as FLOAT/PCM_16 by input dtype; other formats require an explicit subtype."""
    audio = _audio(audio)
    if (
        isinstance(sample_rate, bool)
        or not isinstance(sample_rate, (int, np.integer))
        or not 0 < sample_rate < 2**31
    ):
        raise ValueError("sample_rate must be a positive 32-bit integer")
    path = Path(path).expanduser().resolve()
    if subtype is None:
        if path.suffix.lower() not in (".wav", ".wave"):
            raise ValueError("Specify subtype explicitly for formats other than WAV")
        subtype = "PCM_16" if audio.dtype == np.int16 else "FLOAT"
    _soundfile().write(str(path), audio, sample_rate, subtype=subtype)
    return path


def process_file(
    processor,
    source: str | Path,
    destination: str | Path,
    *,
    reference: str | Path | None = None,
    output_sample_rate: int | None = None,
    dtype: str = "float32",
    subtype: str | None = None,
) -> Path:
    """Process a recording; configured DSP/AEC/pipeline rates must match the file."""
    recording = read_audio(source, dtype=dtype)
    playback = None if reference is None else read_audio(reference, dtype=dtype)
    if playback is not None and playback.sample_rate != recording.sample_rate:
        raise ValueError("Microphone/reference file sample rates must match")
    if isinstance(processor, Denoiser):
        if playback is not None:
            raise ValueError("Use an AEC stage to supply a playback reference")
        result = processor.process(
            recording.samples,
            sample_rate=recording.sample_rate,
            output_sample_rate=output_sample_rate,
        )
        output_rate = recording.sample_rate if output_sample_rate is None else output_sample_rate
    else:
        if processor.sample_rate != recording.sample_rate:
            raise ValueError(
                "File sample_rate must match the configured processor; add explicit resampling"
            )
        output_rate = processor.output_sample_rate
        if output_sample_rate is not None and output_sample_rate != output_rate:
            raise ValueError("output_sample_rate must match the configured processor")
        if getattr(processor, "requires_reference", False):
            if playback is None:
                raise ValueError("AEC requires a playback-reference file")
            result = processor.process(recording.samples, playback.samples)
        else:
            if playback is not None:
                raise ValueError("Processor has no AEC stage for this reference")
            result = processor.process(recording.samples)
    return write_audio(destination, result, output_rate, subtype=subtype)
