"""Static consumer contract, checked against installed public stubs."""

from pathlib import Path
import numpy as np
from numpy.typing import NDArray
from giga_edge_enhance import (
    AGC,
    AGCConfig,
    AudioChunk,
    AudioProcessor,
    Denoiser,
    EchoCanceller,
    EchoCancellerConfig,
    Limiter,
    LimiterConfig,
    LoudnessMeter,
    ModelDescriptor,
    Observer,
    Pipeline,
    Resampler,
    VoiceActivityDetector,
    available_models,
)


def use_sdk(path: Path, audio: NDArray[np.float32], reference: NDArray[np.float32]) -> None:
    catalog: tuple[ModelDescriptor, ...] = available_models()
    assert catalog[0].files
    with (
        AGC(config=AGCConfig(max_gain_db=6)) as gain,
        Limiter(config=LimiterConfig(threshold_db=-6)) as limiter,
    ):
        with AudioProcessor([gain, limiter]) as processor:
            result = processor.process(audio)
            assert result.shape == audio.shape
    with Denoiser(path) as model:
        with (
            model.stream(sample_rate=16000) as denoiser,
            EchoCanceller(config=EchoCancellerConfig(frame_size=256)) as aec,
        ):
            with Pipeline([aec, denoiser, Limiter(sample_rate=16000)]) as pipeline:
                chunk: AudioChunk = pipeline.process_timed_chunk(audio, reference)
                assert chunk.sample_rate == 16000
    with Resampler(16000, 48000) as resampler:
        resampler.process(audio)
    with LoudnessMeter(16000) as meter:
        value: float | None = meter.update(audio).short_term_lufs
        assert value is None or isinstance(value, float)
    with VoiceActivityDetector() as vad, Observer(vad) as observer:
        observer.process_chunk(audio)
        frames = vad.analyze(audio)
        assert all(0 <= frame.score <= 1 for frame in frames)
