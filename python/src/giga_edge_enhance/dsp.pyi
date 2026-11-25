from pathlib import Path
from typing import Any, Iterable, Iterator, TypeVar
import numpy as np
from numpy.typing import NDArray
from .denoiser import Audio
from .config import (
    AGCConfig,
    CompressorConfig,
    LimiterConfig,
    LoudnessControlConfig,
    LoudnessControlState,
    LoudnessState,
    NoiseGateConfig,
    RangeControlConfig,
    VadResult,
)

_T = TypeVar("_T", bound=_AudioComponent)

class _AudioComponent:
    sample_rate: int
    output_sample_rate: int
    channels: int
    frame_size: int
    latency_samples: int
    def process_chunk(self, audio: Audio) -> Audio: ...
    def flush(self) -> Audio: ...
    def process(self, audio: Audio) -> Audio: ...
    def process_chunks(self, chunks: Iterable[Audio]) -> Iterator[Audio]: ...
    def process_file(self, source: str | Path, destination: str | Path, **options: Any) -> Path: ...
    def fresh(self: _T) -> _T: ...
    def reset(self) -> None: ...
    def close(self) -> None: ...
    @property
    def gain(self) -> NDArray[np.float32]: ...
    @property
    def state(self) -> object: ...
    def __enter__(self: _T) -> _T: ...
    def __exit__(self, *args: object) -> None: ...

class AGC(_AudioComponent):
    config: AGCConfig
    def __init__(
        self, *, sample_rate: int = ..., channels: int = ..., config: AGCConfig | None = ...
    ) -> None: ...

class RangeControl(_AudioComponent):
    config: RangeControlConfig
    def __init__(
        self,
        *,
        sample_rate: int = ...,
        channels: int = ...,
        config: RangeControlConfig | None = ...,
    ) -> None: ...

class LoudnessControl(_AudioComponent):
    config: LoudnessControlConfig
    def __init__(
        self,
        *,
        sample_rate: int = ...,
        channels: int = ...,
        config: LoudnessControlConfig | None = ...,
    ) -> None: ...
    @property
    def state(self) -> tuple[LoudnessControlState, ...]: ...

class NoiseGate(_AudioComponent):
    config: NoiseGateConfig
    def __init__(
        self, *, sample_rate: int = ..., channels: int = ..., config: NoiseGateConfig | None = ...
    ) -> None: ...

class Limiter(_AudioComponent):
    config: LimiterConfig
    def __init__(
        self, *, sample_rate: int = ..., channels: int = ..., config: LimiterConfig | None = ...
    ) -> None: ...

class Compressor(_AudioComponent):
    config: CompressorConfig
    def __init__(
        self, *, sample_rate: int = ..., channels: int = ..., config: CompressorConfig | None = ...
    ) -> None: ...

class AudioProcessor(_AudioComponent):
    @property
    def stages(self) -> tuple[_AudioComponent, ...]: ...
    def __init__(
        self,
        stages: Iterable[AGC | RangeControl | LoudnessControl | NoiseGate | Limiter | Compressor],
    ) -> None: ...

class RNNoise(_AudioComponent):
    def __init__(self, *, sample_rate: int = ..., channels: int = ...) -> None: ...
    @property
    def state(self) -> tuple[dict[str, float | None], ...]: ...

class Resampler(_AudioComponent):
    def __init__(
        self, sample_rate: int, output_sample_rate: int, *, channels: int = ...
    ) -> None: ...
    def set_io_ratio(self, ratio: float) -> None: ...
    @property
    def state(self) -> tuple[dict[str, float | None], ...]: ...

class MuteFrameDetector(_AudioComponent):
    def __init__(
        self,
        chunk_length: int,
        *,
        sample_rate: int = ...,
        threshold_db: float = ...,
        left_right_range_db: float = ...,
        minimum_rms_db: float = ...,
    ) -> None: ...
    def process(self, audio: Audio) -> NDArray[np.bool_]: ...  # type: ignore[override]

class LoudnessMeter(_AudioComponent):
    def __init__(self, sample_rate: int = ...) -> None: ...
    @property
    def state(self) -> LoudnessState: ...
    def update(self, audio: Audio) -> LoudnessState: ...

class VoiceActivityDetector(_AudioComponent):
    def __init__(self, *, sample_rate: int = ..., threshold: float = ...) -> None: ...
    def process_chunk(self, audio: Audio) -> list[VadResult]: ...  # type: ignore[override]
    def process(self, audio: Audio) -> list[VadResult]: ...  # type: ignore[override]
    def flush(self) -> list[VadResult]: ...  # type: ignore[override]
    def analyze(self, audio: Audio) -> list[VadResult]: ...
    def process_chunks(self, chunks: Iterable[Audio]) -> Iterator[VadResult]: ...  # type: ignore[override]
