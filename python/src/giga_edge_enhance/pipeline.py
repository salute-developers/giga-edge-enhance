"""Explicit audio composition with ordered flush, rates and sample positions."""

from __future__ import annotations

from dataclasses import dataclass
from fractions import Fraction
from threading import Lock
from typing import Iterable, Iterator

import numpy as np

from .aec import EchoCanceller
from .denoiser import Audio, DenoiserStream, ProcessingError, _audio
from .dsp import (
    AudioProcessor,
    LoudnessMeter,
    MuteFrameDetector,
    VoiceActivityDetector,
    _AudioComponent,
)


@dataclass(frozen=True)
class AudioChunk:
    """Causal output position; subtract latency_samples for its media-time origin."""

    audio: Audio
    start_sample: int
    sample_rate: int
    latency_samples: int

    @property
    def time_seconds(self) -> float:
        return (self.start_sample - self.latency_samples) / self.sample_rate


class Observer(_AudioComponent):
    """Observe mono audio without changing samples; stores only the last call's results."""

    def __init__(self, detector: VoiceActivityDetector | LoudnessMeter):
        if not isinstance(detector, (VoiceActivityDetector, LoudnessMeter)):
            raise TypeError("Observer requires VoiceActivityDetector or LoudnessMeter")
        self.sample_rate = self.output_sample_rate = detector.sample_rate
        self.channels = self.frame_size = 1
        self.latency_samples = 0
        self._kind = "observer"
        self._lock = Lock()
        self._closed = False
        self._native = [detector.fresh()]
        self._reset_state()

    def _reset_state(self) -> None:
        super()._reset_state()
        self.observations = ()

    def fresh(self):
        if self._closed:
            raise RuntimeError("Observer is closed")
        return Observer(self._native[0])

    def process_chunk(self, audio: Audio) -> Audio:
        audio = _audio(audio, 1)
        if audio.ndim != 1:
            raise ValueError("Observer expects a one-dimensional mono array")
        self._enter()
        try:
            if self._flushed:
                raise RuntimeError("Observer has been flushed; call reset()")
            self._dtype = audio.dtype
            detector = self._native[0]
            result = detector.process_chunk(audio)
            self.observations = (
                tuple(result) if isinstance(detector, VoiceActivityDetector) else (detector.state,)
            )
            return audio.copy()
        finally:
            self._lock.release()

    def flush(self) -> Audio:
        self._enter()
        try:
            if not self._flushed:
                self._flushed = True
                result = self._native[0].flush()
                if isinstance(self._native[0], VoiceActivityDetector):
                    self.observations = tuple(result)
            return np.empty(0, dtype=self._dtype or np.float32)
        finally:
            self._lock.release()


class Pipeline(_AudioComponent):
    """Own independent copies of explicit stages; AEC may appear only first.

    Adjacent rates and channel counts must match. Include a Resampler explicitly
    when they differ. Denoiser stages are configured DenoiserStream instances.
    """

    def __init__(self, stages: Iterable[_AudioComponent | DenoiserStream]):
        stages = tuple(stages)
        if not stages:
            raise ValueError("Pipeline requires at least one stage")
        for index, stage in enumerate(stages):
            if not isinstance(stage, (_AudioComponent, DenoiserStream)) or isinstance(
                stage, (VoiceActivityDetector, MuteFrameDetector)
            ):
                raise TypeError("Pipeline requires audio stages; wrap VAD in Observer")
            if isinstance(stage, EchoCanceller) and index != 0:
                raise ValueError("AEC must be the first stage so reference timing stays explicit")
            if index:
                previous = stages[index - 1]
                if (
                    stage.sample_rate != previous.output_sample_rate
                    or stage.channels != previous.channels
                ):
                    raise ValueError(
                        "Adjacent pipeline sample rates/channels must match; add an explicit Resampler"
                    )
        self.sample_rate = stages[0].sample_rate
        self.output_sample_rate = stages[-1].output_sample_rate
        self.channels = stages[0].channels
        self.frame_size = 1
        delay = Fraction(0)
        for stage in stages:
            delay = (
                delay * Fraction(stage.output_sample_rate, stage.sample_rate)
                + stage.latency_samples
            )
        self.latency_samples = int(delay + Fraction(1, 2))
        self.requires_reference = isinstance(stages[0], EchoCanceller)
        self._kind = "pipeline"
        self._lock = Lock()
        self._closed = False
        self._native = []
        try:
            for stage in stages:
                self._native.append(stage.fresh())
        except Exception:
            self.close()
            raise
        self._reset_state()

    @property
    def stages(self) -> tuple:
        """Owned stages, for inspecting state/observer results between calls."""
        return tuple(self._native)

    def fresh(self):
        if self._closed:
            raise RuntimeError("Pipeline is closed")
        return Pipeline(self._native)

    def flush(self) -> Audio:
        return AudioProcessor.flush(self)

    @property
    def state(self) -> tuple:
        self._enter()
        try:
            return tuple(
                stage.snr if isinstance(stage, DenoiserStream) else stage.state
                for stage in self._native
            )
        finally:
            self._lock.release()

    @property
    def input_position(self) -> int:
        return self._input_count

    @property
    def output_position(self) -> int:
        return self._output_count

    def process_chunk(self, audio: Audio, reference: Audio | None = None) -> Audio:
        audio = _audio(audio, self.channels)
        if self.requires_reference and reference is None:
            raise ValueError("AEC pipeline requires a synchronized reference")
        if not self.requires_reference and reference is not None:
            raise ValueError("Pipeline has no AEC stage for this reference")
        self._enter()
        try:
            if self._flushed:
                raise RuntimeError("Pipeline has been flushed; call reset()")
            if self._dtype is not None and (
                audio.dtype != self._dtype or (audio.ndim == 2) != self._matrix
            ):
                raise ValueError("Pipeline dtype and channel layout must remain constant")
            self._dtype = audio.dtype
            self._matrix = audio.ndim == 2
            count = len(audio)
            for index, stage in enumerate(self._native):
                audio = (
                    stage.process_chunk(audio, reference)
                    if index == 0 and self.requires_reference
                    else stage.process_chunk(audio)
                )
            self._input_count += count
            self._output_count += len(audio)
            return audio
        finally:
            self._lock.release()

    def process_timed_chunk(self, audio: Audio, reference: Audio | None = None) -> AudioChunk:
        start = self.output_position
        output = self.process_chunk(audio, reference)
        return AudioChunk(output, start, self.output_sample_rate, self.latency_samples)

    def process(self, audio: Audio, reference: Audio | None = None) -> Audio:
        audio = _audio(audio, self.channels)
        with self.fresh() as session:
            if session.requires_reference:
                if reference is None:
                    raise ValueError("AEC pipeline requires a synchronized reference")
                EchoCanceller._validate_pair(audio, reference)
            if not len(audio):
                return audio.copy()
            output = np.concatenate((session.process_chunk(audio, reference), session.flush()))
            count = (
                len(audio) * self.output_sample_rate + self.sample_rate - 1
            ) // self.sample_rate
            if not count:
                return output[:0].copy()
            start = session.latency_samples
            if len(output) < start + count:
                raise ProcessingError("Pipeline finalization returned an incomplete recording")
            return output[start : start + count].copy()

    def process_chunks(self, chunks: Iterable) -> Iterator[Audio]:
        """AEC input iterators yield (microphone, reference); others yield audio."""
        with self.fresh() as session:
            for chunk in chunks:
                output = (
                    session.process_chunk(*chunk)
                    if self.requires_reference
                    else session.process_chunk(chunk)
                )
                if len(output):
                    yield output
            tail = session.flush()
            if len(tail):
                yield tail
