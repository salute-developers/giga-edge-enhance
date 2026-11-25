"""Synchronized mono microphone/playback echo cancellation over native Speex."""

from __future__ import annotations

from dataclasses import asdict
from typing import Iterable, Iterator

import numpy as np

from .config import EchoCancellerConfig
from .denoiser import Audio, _audio, _positive_int
from .dsp import _AudioComponent, _bounded


class EchoCanceller(_AudioComponent):
    """One adaptive session, with an explicit playback reference and no hidden resampling.

    ``process`` uses fresh state for a recording; ``process_chunk`` retains filter
    adaptation. Both inputs must be mono, synchronized, equal length and dtype.
    Float inputs use normalized PCM; timings are seconds and lengths are samples.
    """

    requires_reference = True

    def __init__(self, *, sample_rate: int = 16000, config: EchoCancellerConfig | None = None):
        self.config = EchoCancellerConfig() if config is None else config
        if not isinstance(self.config, EchoCancellerConfig):
            raise TypeError("config must be EchoCancellerConfig")
        values = asdict(self.config)
        for key in ("frame_size", "filter_length"):
            values[key] = _positive_int(values[key], key)
            if values[key] > 65535:
                raise ValueError(f"{key} must fit the native uint16 range")
        if values["filter_length"] < values["frame_size"]:
            raise ValueError("filter_length must be at least frame_size")
        for key in ("delay_compensation", "residual_suppression"):
            if not isinstance(values[key], bool):
                raise TypeError(f"{key} must be bool")
        values["premute_seconds"] = _bounded(values["premute_seconds"], "premute_seconds", 0, 3600)
        self._options = dict(sample_rate=sample_rate, config=self.config)
        self._initialize("echo_canceller", sample_rate, 1, values)

    def _reset_state(self) -> None:
        super()._reset_state()
        self._reference_pending = np.empty(0, np.float32)

    def process_chunk(self, microphone: Audio, reference: Audio) -> Audio:
        microphone, reference = self._validate_pair(microphone, reference)
        self._enter()
        try:
            if self._flushed:
                raise RuntimeError("AEC has been flushed; call reset()")
            if self._dtype is not None and self._dtype != microphone.dtype:
                raise ValueError("AEC dtype must remain constant")
            if not len(microphone):
                return microphone.copy()
            self._dtype = microphone.dtype
            mic = microphone.astype(np.float32)
            ref = reference.astype(np.float32)
            if microphone.dtype == np.int16:
                mic /= np.float32(32767)
                ref /= np.float32(32767)
            mic = np.concatenate((self._pending[:, 0], mic))
            ref = np.concatenate((self._reference_pending, ref))
            count = len(mic) // self.frame_size * self.frame_size
            parts = []
            for offset in range(0, count, self.frame_size):
                output, _, _ = self._native[0].process(
                    mic[offset : offset + self.frame_size], ref[offset : offset + self.frame_size]
                )
                parts.append(output)
            self._pending = mic[count:, None].copy()
            self._reference_pending = ref[count:].copy()
            self._input_count += len(microphone)
            output = np.concatenate(parts) if parts else np.empty(0, np.float32)
            self._output_count += len(output)
            return self._format(output[:, None])
        finally:
            self._lock.release()

    @staticmethod
    def _validate_pair(microphone: Audio, reference: Audio) -> tuple[Audio, Audio]:
        microphone, reference = _audio(microphone, 1), _audio(reference, 1)
        if microphone.ndim != 1 or reference.ndim != 1:
            raise ValueError("AEC supports one-dimensional mono microphone and reference arrays")
        if microphone.shape != reference.shape:
            raise ValueError("microphone and reference must have equal lengths")
        if microphone.dtype != reference.dtype:
            raise ValueError("microphone and reference must have the same dtype")
        if microphone.dtype == np.float32:
            for values in (microphone, reference):
                if np.any(values > 1) or np.any(values < np.float32(-32768 / 32767)):
                    raise ValueError(
                        "AEC float audio must be in normalized PCM range [-32768/32767, 1]"
                    )
        return microphone, reference

    def flush(self) -> Audio:
        self._enter()
        try:
            if self._flushed:
                return self._format(np.empty((0, 1), np.float32))
            self._flushed = True
            count = len(self._pending)
            result = np.empty(0, np.float32)
            if count:
                mic = np.pad(self._pending[:, 0], (0, self.frame_size - count))
                ref = np.pad(self._reference_pending, (0, self.frame_size - count))
                result, _, _ = self._native[0].process(mic, ref)
                result = result[:count]
            self._pending = np.empty((0, 1), np.float32)
            self._reference_pending = np.empty(0, np.float32)
            self._native[0].flush()
            self._output_count += len(result)
            return self._format(result[:, None])
        finally:
            self._lock.release()

    def process(self, microphone: Audio, reference: Audio) -> Audio:
        microphone, reference = self._validate_pair(microphone, reference)
        with self.fresh() as session:
            if not len(microphone):
                return microphone.copy()
            return np.concatenate((session.process_chunk(microphone, reference), session.flush()))

    def process_chunks(self, chunks: Iterable[tuple[Audio, Audio]]) -> Iterator[Audio]:
        with self.fresh() as session:
            for microphone, reference in chunks:
                output = session.process_chunk(microphone, reference)
                if len(output):
                    yield output
            tail = session.flush()
            if len(tail):
                yield tail

    def close(self) -> None:
        super().close()
        self._reference_pending = np.empty(0, np.float32)
