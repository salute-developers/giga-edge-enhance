"""Independent native DSP state, without importing inference engines."""

from __future__ import annotations

from dataclasses import asdict
from importlib import import_module
from math import isfinite
from threading import Lock
from typing import Iterable, Iterator

import numpy as np

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
from .denoiser import Audio, ProcessingError, _audio, _pcm, _positive_int


def _rate(value: int) -> int:
    value = _positive_int(value, "sample_rate")
    if value < 100:
        raise ValueError("sample_rate must be in [100, 100000]")
    return value


def _bounded(value: float, name: str, low: float, high: float) -> float:
    if isinstance(value, (bool, str)) or not isinstance(value, (int, float, np.number)):
        raise TypeError(f"{name} must be a number")
    value = float(value)
    if not isfinite(value) or not low <= value <= high:
        raise ValueError(f"{name} must be finite and in [{low}, {high}]")
    return value


def _parameters(config: object, sample_rate: int) -> dict[str, float]:
    values = asdict(config)
    for name, value in values.items():
        if name in ("use_vad", "post_processing"):
            if not isinstance(value, bool):
                raise TypeError(f"{name} must be bool")
        elif name == "average_length":
            values[name] = _positive_int(sample_rate // 4 if value is None else value, name)
        elif name == "ratio":
            values[name] = _positive_int(value, name)
            _bounded(value, name, 1, 50)
        elif name == "threshold_db":
            _bounded(value, name, -140 if isinstance(config, NoiseGateConfig) else -50, 0)
        elif name == "knee_width_db":
            _bounded(value, name, 0, 20)
        elif name in ("attack_time", "release_time", "hold_time"):
            _bounded(value, name, 0, 3600 if isinstance(config, RangeControlConfig) else 4)
        elif name in ("target_power_level", "rise_step", "fall_step"):
            _bounded(value, name, np.finfo(np.float32).tiny, 10000)
        elif name == "bypass_time":
            _bounded(value, name, 0, 3600)
        elif name == "max_gain_db":
            _bounded(value, name, 0, 120)
        elif name == "target_level_db":
            _bounded(value, name, -80, 0)
    if isinstance(config, LoudnessControlConfig) and config.post_processing:
        _bounded(config.target_level_db, "target_level_db with post_processing", -50, -6)
    return values


class _AudioComponent:
    """Common stream lifecycle; each channel owns a separate native component."""

    latency_samples = 0

    def _initialize(self, kind: str, sample_rate: int, channels: int, parameters: dict) -> None:
        self.sample_rate = _rate(sample_rate)
        self.output_sample_rate = int(parameters.get("output_sample_rate", sample_rate))
        self.channels = _positive_int(channels, "channels")
        self._kind = kind
        self._parameters = parameters
        self._module = import_module("giga_edge_enhance._dsp")
        from .logging import _configure_native

        _configure_native(self._module)
        self._native = []
        try:
            for _ in range(self.channels):
                self._native.append(self._module.Component(kind, self.sample_rate, parameters))
        except Exception:
            for native in self._native:
                native.close()
            raise
        self.frame_size = self._native[0].frame_size
        self.latency_samples = self._native[0].latency_samples
        self._lock = Lock()
        self._closed = False
        self._reset_state()

    def _reset_state(self) -> None:
        self._pending = np.empty((0, self.channels), dtype=np.float32)
        self._dtype = None
        self._matrix = self.channels != 1
        self._flushed = False
        self._input_count = 0
        self._output_count = 0
        self._gain = np.empty((0, self.channels), dtype=np.float32)

    def _enter(self) -> None:
        if not self._lock.acquire(blocking=False):
            raise RuntimeError("Concurrent use of one component is not supported")
        if self._closed:
            self._lock.release()
            raise RuntimeError("Component is closed")

    def _format(self, output: np.ndarray) -> Audio:
        result = output if self._matrix else output[:, 0]
        return _pcm(result, self._dtype or np.dtype("float32"))

    def _run(self, frame: np.ndarray) -> np.ndarray:
        results = [
            native.process(np.ascontiguousarray(frame[:, channel]))
            for channel, native in enumerate(self._native)
        ]
        self._gain = np.column_stack([result[1] for result in results])
        return np.column_stack([result[0] for result in results])

    def process_chunk(self, audio: Audio) -> Audio:
        """Preserve state; incomplete native frames wait for the next call."""
        audio = _audio(audio, self.channels)
        self._enter()
        try:
            if self._flushed:
                raise RuntimeError("Component has been flushed; call reset()")
            if self._dtype is not None and (
                audio.dtype != self._dtype or (audio.ndim == 2) != self._matrix
            ):
                raise ValueError("Stream dtype and channel layout must remain constant")
            if not len(audio):
                return audio.copy()
            self._dtype = audio.dtype
            self._matrix = audio.ndim == 2
            self._input_count += len(audio)
            values = audio.astype(np.float32).reshape(-1, self.channels)
            if audio.dtype == np.int16:
                values /= np.float32(32767)
            values = np.concatenate((self._pending, values))
            count = len(values) // self.frame_size * self.frame_size
            parts = []
            gains = []
            step = self.frame_size if self.frame_size > 1 else max(1, count)
            for offset in range(0, count, step):
                parts.append(self._run(values[offset : offset + step]))
                gains.append(self._gain)
            self._pending = values[count:].copy()
            self._gain = (
                np.concatenate(gains) if gains else np.empty((0, self.channels), np.float32)
            )
            output = np.concatenate(parts) if parts else np.empty((0, self.channels), np.float32)
            self._output_count += len(output)
            return self._format(output)
        finally:
            self._lock.release()

    def flush(self) -> Audio:
        """Finish buffered frames once; processing again requires reset()."""
        self._enter()
        try:
            if self._flushed:
                return self._format(np.empty((0, self.channels), np.float32))
            self._flushed = True
            parts = []
            if len(self._pending):
                parts.append(
                    self._run(
                        np.pad(self._pending, ((0, self.frame_size - len(self._pending)), (0, 0)))
                    )
                )
                self._pending = np.empty((0, self.channels), np.float32)
            if self._kind == "rnnoise" and self._input_count:
                frames = (self.latency_samples + self.frame_size - 1) // self.frame_size + 1
                parts.extend(
                    self._run(np.zeros((self.frame_size, self.channels), np.float32))
                    for _ in range(frames)
                )
            if self._kind == "resampler" and self._input_count:
                padding = (
                    self.sample_rate + self.output_sample_rate - 1
                ) // self.output_sample_rate
                parts.append(self._run(np.zeros((padding, self.channels), np.float32)))
            parts.append(np.column_stack([native.flush() for native in self._native]))
            output = np.concatenate(parts)
            if self._kind == "resampler" and not getattr(self, "_variable_ratio", False):
                expected = (
                    self._input_count * self.output_sample_rate + self.sample_rate - 1
                ) // self.sample_rate
                output = output[: max(0, expected - self._output_count)]
                if len(output) != max(0, expected - self._output_count):
                    raise ProcessingError("Resampler returned an incomplete tail")
            self._output_count += len(output)
            return self._format(output)
        finally:
            self._lock.release()

    def process(self, audio: Audio) -> Audio:
        """Process a recording in fresh state, preserving dtype and duration."""
        audio = _audio(audio, self.channels)
        with self.fresh() as stream:
            stream._matrix = audio.ndim == 2
            if not len(audio):
                return audio.copy()
            output = np.concatenate((stream.process_chunk(audio), stream.flush()))
            count = (
                len(audio) * self.output_sample_rate + self.sample_rate - 1
            ) // self.sample_rate
            if len(output) < stream.latency_samples + count:
                raise ProcessingError("Native component returned an incomplete recording")
            return output[stream.latency_samples : stream.latency_samples + count].copy()

    def process_chunks(self, chunks: Iterable[Audio]) -> Iterator[Audio]:
        """Yield causal stream output and its tail from fresh state."""
        with self.fresh() as stream:
            for chunk in chunks:
                output = stream.process_chunk(chunk)
                if len(output):
                    yield output
            tail = stream.flush()
            if len(tail):
                yield tail

    def process_file(self, source, destination, **options):
        """Process a file with optional codecs; its rate must match this configuration."""
        from .io import process_file

        return process_file(self, source, destination, **options)

    def fresh(self):
        if self._closed:
            raise RuntimeError("Component is closed")
        return type(self)(**self._options)

    @property
    def gain(self) -> np.ndarray:
        """Linear gain for the last call, or an empty array if unavailable."""
        self._enter()
        try:
            gain = self._gain if self._matrix else self._gain[:, 0]
            return gain.copy()
        finally:
            self._lock.release()

    @property
    def state(self) -> tuple[dict, ...]:
        self._enter()
        try:
            return tuple(native.state for native in self._native)
        finally:
            self._lock.release()

    def reset(self) -> None:
        self._enter()
        try:
            for native in self._native:
                native.reset()
            self._reset_state()
            if self._kind == "resampler":
                self._variable_ratio = False
        finally:
            self._lock.release()

    def close(self) -> None:
        if not self._lock.acquire(blocking=False):
            raise RuntimeError("Concurrent use of one component is not supported")
        try:
            for native in self._native:
                native.close()
            self._native.clear()
            self._pending = np.empty((0, self.channels), np.float32)
            self._gain = np.empty((0, self.channels), np.float32)
            self._closed = True
        finally:
            self._lock.release()

    def __enter__(self):
        if self._closed:
            raise RuntimeError("Component is closed")
        return self

    def __exit__(self, *_args: object) -> None:
        self.close()


class _ConfiguredComponent(_AudioComponent):
    def __init__(self, *, sample_rate: int = 48000, channels: int = 1, config=None):
        self.config = config if config is not None else self._config_type()
        if not isinstance(self.config, self._config_type):
            raise TypeError(f"config must be {self._config_type.__name__}")
        self._options = dict(sample_rate=sample_rate, channels=channels, config=self.config)
        self._initialize(
            self._component_kind,
            sample_rate,
            channels,
            _parameters(self.config, _rate(sample_rate)),
        )


class AGC(_ConfiguredComponent):
    """Native AGC: linear target power, max gain in dB, per-sample adaptation steps."""

    _config_type, _component_kind = AGCConfig, "agc"


class RangeControl(_ConfiguredComponent):
    """Native peak range control with attack/release/hold in seconds."""

    _config_type, _component_kind = RangeControlConfig, "range_control"


class LoudnessControl(_ConfiguredComponent):
    """Native loudness control, simple or RNNoise VAD guided, in fixed 10 ms steps."""

    _config_type, _component_kind = LoudnessControlConfig, "loudness_control"

    @property
    def state(self) -> tuple[LoudnessControlState, ...]:
        values = super().state
        return tuple(
            LoudnessControlState(**{**item, "bypass_active": bool(item["bypass_active"])})
            for item in values
        )


class NoiseGate(_ConfiguredComponent):
    """Native noise gate with threshold in dBFS."""

    _config_type, _component_kind = NoiseGateConfig, "noise_gate"


class Limiter(_ConfiguredComponent):
    """Native limiter with threshold/knee in dB and timing in seconds."""

    _config_type, _component_kind = LimiterConfig, "limiter"


class Compressor(_ConfiguredComponent):
    """Native dynamic range compressor with integer ratio in [1, 50]."""

    _config_type, _component_kind = CompressorConfig, "compressor"


class AudioProcessor(_AudioComponent):
    """Ordered native DSP stages; owns fresh copies of the supplied configurations."""

    def __init__(self, stages: Iterable[_ConfiguredComponent]):
        stages = tuple(stages)
        if not stages or not all(isinstance(stage, _ConfiguredComponent) for stage in stages):
            raise ValueError("AudioProcessor requires at least one AGC/RC/LC/NG/Limiter/Compressor")
        first = stages[0]
        if any(
            stage.sample_rate != first.sample_rate or stage.channels != first.channels
            for stage in stages
        ):
            raise ValueError("DSP stages must use the same sample_rate and channels")
        self.sample_rate = self.output_sample_rate = first.sample_rate
        self.channels = first.channels
        self.frame_size = 1
        self.latency_samples = 0
        self._kind = "audio_processor"
        self._lock = Lock()
        self._closed = False
        self._native = []
        try:
            self._native = [stage.fresh() for stage in stages]
        except Exception:
            self.close()
            raise
        self._reset_state()

    def fresh(self):
        if self._closed:
            raise RuntimeError("AudioProcessor is closed")
        return type(self)(self._native)

    @property
    def stages(self) -> tuple:
        """Owned stages, for inspecting gains/state between calls."""
        return tuple(self._native)

    def process_chunk(self, audio: Audio) -> Audio:
        audio = _audio(audio, self.channels)
        self._enter()
        try:
            if self._flushed:
                raise RuntimeError("AudioProcessor has been flushed; call reset()")
            self._matrix = audio.ndim == 2
            self._dtype = audio.dtype
            self._input_count += len(audio)
            for stage in self._native:
                audio = stage.process_chunk(audio)
            self._output_count += len(audio)
            return audio
        finally:
            self._lock.release()

    def flush(self) -> Audio:
        self._enter()
        try:
            if self._flushed:
                return self._format(np.empty((0, self.channels), np.float32))
            self._flushed = True
            parts = []
            for index, stage in enumerate(self._native):
                tail = stage.flush()
                for following in self._native[index + 1 :]:
                    tail = following.process_chunk(tail)
                if len(tail):
                    parts.append(tail)
            result = (
                np.concatenate(parts)
                if parts
                else self._format(np.empty((0, self.channels), np.float32))
            )
            self._output_count += len(result)
            return result
        finally:
            self._lock.release()


class RNNoise(_AudioComponent):
    """Standalone native RNNoise and its FIR rate conversion; state includes VAD score."""

    def __init__(self, *, sample_rate: int = 48000, channels: int = 1):
        if sample_rate not in (8000, 16000, 24000, 32000, 44100, 48000):
            raise ValueError(
                "RNNoise sample_rate must be 8000, 16000, 24000, 32000, 44100, or 48000"
            )
        self._options = dict(sample_rate=sample_rate, channels=channels)
        self._initialize("rnnoise", sample_rate, channels, {})


class Resampler(_AudioComponent):
    """Native soxr resampling; float32/PCM16, independent channel state, full tail."""

    def __init__(self, sample_rate: int, output_sample_rate: int, *, channels: int = 1):
        output_sample_rate = _rate(output_sample_rate)
        self._options = dict(
            sample_rate=sample_rate, output_sample_rate=output_sample_rate, channels=channels
        )
        self._variable_ratio = False
        self._initialize(
            "resampler", sample_rate, channels, {"output_sample_rate": output_sample_rate}
        )

    def set_io_ratio(self, ratio: float) -> None:
        """Set input/output ratio where native soxr VR supports it (44.1 kHz paths)."""
        ratio = _bounded(ratio, "ratio", np.finfo(float).tiny, 1000)
        self._enter()
        try:
            if self._flushed:
                raise RuntimeError("Resampler has been flushed; call reset()")
            for native in self._native:
                native.set_io_ratio(ratio)
            self._variable_ratio = True
        finally:
            self._lock.release()


class MuteFrameDetector(_AudioComponent):
    """Causal gap decisions over three adjacent windows; not a silence/VAD detector."""

    def __init__(
        self,
        chunk_length: int,
        *,
        sample_rate: int = 48000,
        threshold_db: float = 20,
        left_right_range_db: float = 6,
        minimum_rms_db: float = -30,
    ):
        chunk_length = _positive_int(chunk_length, "chunk_length")
        values = dict(
            chunk_length=chunk_length,
            threshold_db=_bounded(threshold_db, "threshold_db", 0, 140),
            left_right_range_db=_bounded(left_right_range_db, "left_right_range_db", 0, 140),
            minimum_rms_db=_bounded(minimum_rms_db, "minimum_rms_db", -140, 0),
        )
        self._options = dict(sample_rate=sample_rate, **values)
        self._initialize("mute_frame_detector", sample_rate, 1, values)

    def _format(self, output: np.ndarray) -> np.ndarray:
        return output[:, 0].astype(np.bool_)

    def process(self, audio: Audio) -> np.ndarray:
        return super().process(audio).astype(np.bool_)


class LoudnessMeter(_AudioComponent):
    """Native mono K-weighted loudness; peak is a sample peak, not oversampled true peak."""

    def __init__(self, sample_rate: int = 48000):
        self._options = dict(sample_rate=sample_rate)
        self._initialize("loudness_meter", sample_rate, 1, {})

    @property
    def state(self) -> LoudnessState:
        return LoudnessState(**super().state[0])

    def update(self, audio: Audio) -> LoudnessState:
        self.process_chunk(audio)
        return self.state


class VoiceActivityDetector(_AudioComponent):
    """RNNoise VAD frames with input sample ranges and native score/decision."""

    def __init__(self, *, sample_rate: int = 16000, threshold: float = 0.5):
        threshold = _bounded(threshold, "threshold", 0, 1)
        if sample_rate not in (8000, 16000, 24000, 32000, 44100, 48000):
            raise ValueError("VAD sample_rate must be 8000, 16000, 24000, 32000, 44100, or 48000")
        self.threshold = threshold
        self._options = dict(sample_rate=sample_rate, threshold=threshold)
        self._initialize("vad", sample_rate, 1, {"threshold": threshold})
        self.latency_samples = self.sample_rate // 100

    def process_chunk(self, audio: Audio) -> list[VadResult]:
        audio = _audio(audio, 1)
        if audio.ndim != 1:
            raise ValueError("VAD expects a one-dimensional mono array")
        self._enter()
        try:
            if self._flushed:
                raise RuntimeError("VAD has been flushed; call reset()")
            values = audio.astype(np.float32)
            if audio.dtype == np.int16:
                values /= np.float32(32767)
            start = self._input_count - len(self._pending)
            self._input_count += len(values)
            values = np.concatenate((self._pending[:, 0], values))
            count = len(values) // self.frame_size * self.frame_size
            results = []
            for offset in range(0, count, self.frame_size):
                _, _, state = self._native[0].process(
                    np.ascontiguousarray(values[offset : offset + self.frame_size])
                )
                results.append(
                    VadResult(
                        start + offset, self.frame_size, state["score"], bool(state["speech"])
                    )
                )
            self._pending = values[count:, None].copy()
            return results
        finally:
            self._lock.release()

    def flush(self) -> list[VadResult]:
        self._enter()
        try:
            if self._flushed:
                return []
            self._flushed = True
            if not len(self._pending):
                return []
            count = len(self._pending)
            frame = np.pad(self._pending[:, 0], (0, self.frame_size - count))
            _, _, state = self._native[0].process(frame)
            self._pending = np.empty((0, 1), np.float32)
            return [
                VadResult(self._input_count - count, count, state["score"], bool(state["speech"]))
            ]
        finally:
            self._lock.release()

    def analyze(self, audio: Audio) -> list[VadResult]:
        with self.fresh() as detector:
            return detector.process_chunk(audio) + detector.flush()

    def process(self, audio: Audio) -> list[VadResult]:
        return self.analyze(audio)

    def process_chunks(self, chunks: Iterable[Audio]) -> Iterator[VadResult]:
        """Yield individual scored input ranges from a fresh analysis session."""
        with self.fresh() as detector:
            for chunk in chunks:
                yield from detector.process_chunk(chunk)
            yield from detector.flush()
