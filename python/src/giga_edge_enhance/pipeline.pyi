from dataclasses import dataclass
from typing import Iterable, Iterator
from .denoiser import Audio, DenoiserStream
from .dsp import _AudioComponent, VoiceActivityDetector, LoudnessMeter
from .config import VadResult, LoudnessState

@dataclass(frozen=True)
class AudioChunk:
    audio: Audio
    start_sample: int
    sample_rate: int
    latency_samples: int
    @property
    def time_seconds(self) -> float: ...

class Observer(_AudioComponent):
    observations: tuple[VadResult | LoudnessState, ...]
    def __init__(self, detector: VoiceActivityDetector | LoudnessMeter) -> None: ...

class Pipeline(_AudioComponent):
    requires_reference: bool
    def __init__(self, stages: Iterable[_AudioComponent | DenoiserStream]) -> None: ...
    @property
    def stages(self) -> tuple[_AudioComponent | DenoiserStream, ...]: ...
    @property
    def input_position(self) -> int: ...
    @property
    def output_position(self) -> int: ...
    def process_chunk(self, audio: Audio, reference: Audio | None = ...) -> Audio: ...
    def process_timed_chunk(self, audio: Audio, reference: Audio | None = ...) -> AudioChunk: ...
    def process(self, audio: Audio, reference: Audio | None = ...) -> Audio: ...
    def process_chunks(
        self, chunks: Iterable[Audio] | Iterable[tuple[Audio, Audio]]
    ) -> Iterator[Audio]: ...
