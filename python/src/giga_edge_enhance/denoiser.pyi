from dataclasses import dataclass
from pathlib import Path
from typing import Any, Iterable, Iterator
import numpy as np
from numpy.typing import NDArray

Audio = NDArray[np.float32] | NDArray[np.int16]

class ProcessingError(RuntimeError):
    """A native processor could not complete an operation."""

@dataclass(frozen=True)
class ModelInfo:
    """Native model metadata; frame_size is the hop length in model samples."""

    backend: str
    model_type: str
    sample_rate: int
    frame_size: int
    engine_threads: int
    latency_frames: int

class Denoiser:
    info: ModelInfo
    def __init__(
        self, path: str | Path, *, backend: str = ..., engine_threads: int = ...
    ) -> None: ...
    @classmethod
    def from_config(
        cls, yaml: str, *, base_directory: str | Path, backend: str = ..., engine_threads: int = ...
    ) -> Denoiser: ...
    @classmethod
    def from_pretrained(
        cls,
        repo_or_path: str | Path,
        *,
        model: str = ...,
        backend: str = ...,
        revision: str | None = ...,
        cache_dir: str | Path | None = ...,
        local_files_only: bool = ...,
        token: str | bool | None = ...,
        subfolder: str | None = ...,
        engine_threads: int = ...,
    ) -> Denoiser: ...
    def stream(
        self, *, sample_rate: int, output_sample_rate: int | None = ..., channels: int = ...
    ) -> DenoiserStream: ...
    def process(
        self, audio: Audio, *, sample_rate: int, output_sample_rate: int | None = ...
    ) -> Audio: ...
    def process_chunks(
        self,
        chunks: Iterable[Audio],
        *,
        sample_rate: int,
        output_sample_rate: int | None = ...,
        channels: int = ...,
    ) -> Iterator[Audio]: ...
    def process_file(self, source: str | Path, destination: str | Path, **options: Any) -> Path: ...
    def close(self) -> None: ...
    def __enter__(self) -> Denoiser: ...
    def __exit__(self, *args: object) -> None: ...

class DenoiserStream:
    sample_rate: int
    output_sample_rate: int
    channels: int
    input_frame_size: int
    latency_samples: int
    def __init__(
        self,
        module: Any,
        model: Any,
        info: ModelInfo,
        sample_rate: int,
        output_rate: int,
        channels: int,
    ) -> None: ...
    def process_chunk(self, audio: Audio) -> Audio: ...
    def flush(self) -> Audio: ...
    def reset(self) -> None: ...
    def fresh(self) -> DenoiserStream: ...
    def close(self) -> None: ...
    @property
    def snr(self) -> tuple[float, ...]: ...
    def __enter__(self) -> DenoiserStream: ...
    def __exit__(self, *args: object) -> None: ...
