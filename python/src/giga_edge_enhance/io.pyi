from dataclasses import dataclass
from pathlib import Path
from typing import Literal
from .denoiser import Audio, Denoiser
from .dsp import _AudioComponent

@dataclass(frozen=True)
class AudioData:
    samples: Audio
    sample_rate: int

def read_audio(path: str | Path, *, dtype: Literal["float32", "int16"] = ...) -> AudioData: ...
def write_audio(
    path: str | Path, audio: Audio, sample_rate: int, *, subtype: str | None = ...
) -> Path: ...
def process_file(
    processor: Denoiser | _AudioComponent,
    source: str | Path,
    destination: str | Path,
    *,
    reference: str | Path | None = ...,
    output_sample_rate: int | None = ...,
    dtype: Literal["float32", "int16"] = ...,
    subtype: str | None = ...,
) -> Path: ...
