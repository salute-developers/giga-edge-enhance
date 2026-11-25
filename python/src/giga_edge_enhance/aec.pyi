from typing import Iterable, Iterator
from .denoiser import Audio
from .dsp import _AudioComponent
from .config import EchoCancellerConfig

class EchoCanceller(_AudioComponent):
    config: EchoCancellerConfig
    requires_reference: bool
    def __init__(
        self, *, sample_rate: int = ..., config: EchoCancellerConfig | None = ...
    ) -> None: ...
    def process_chunk(self, microphone: Audio, reference: Audio) -> Audio: ...  # type: ignore[override]
    def process(self, microphone: Audio, reference: Audio) -> Audio: ...  # type: ignore[override]
    def process_chunks(self, chunks: Iterable[tuple[Audio, Audio]]) -> Iterator[Audio]: ...  # type: ignore[override]
