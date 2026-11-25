"""File-independent denoising and stateful streaming over the C++ SDK."""

from __future__ import annotations

from dataclasses import dataclass
from importlib import import_module
from pathlib import Path
from threading import Lock
from typing import Iterable, Iterator

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


def _positive_int(value: int, name: str) -> int:
    if (
        isinstance(value, bool)
        or not isinstance(value, (int, np.integer))
        or not 0 < value <= 100000
    ):
        raise ValueError(f"{name} must be an integer in [1, 100000]")
    return int(value)


def _audio(value: Audio, channels: int | None = None) -> Audio:
    if not isinstance(value, np.ndarray):
        raise TypeError("audio must be a NumPy array")
    if value.dtype not in (np.dtype("float32"), np.dtype("int16")):
        raise TypeError("audio dtype must be float32 or int16")
    if value.ndim not in (1, 2) or (value.ndim == 2 and value.shape[1] == 0):
        raise ValueError("audio shape must be (samples,) or (samples, channels)")
    count = 1 if value.ndim == 1 else value.shape[1]
    if channels is not None and channels != count:
        raise ValueError(f"expected {channels} channels, received {count}")
    if not np.isfinite(value).all():
        raise ValueError("audio samples must be finite")
    return np.ascontiguousarray(value)


def _pcm(output: NDArray[np.float32], dtype: np.dtype) -> Audio:
    if dtype == np.dtype("int16"):
        # Match the SDK's symmetric PCM16 scale and rounding away from zero.
        scaled = np.clip(output * np.float32(32767), -32767, 32767)
        return np.copysign(np.floor(np.abs(scaled) + np.float32(0.5)), scaled).astype(np.int16)
    return output


class Denoiser:
    """A loaded model shared by independent audio streams.

    Load a configuration file, model directory, or packed .enhance model. Model
    paths in YAML resolve against its containing directory. No network is used.
    """

    def __init__(self, path: str | Path, *, backend: str = "onnx", engine_threads: int = 1):
        path = Path(path).expanduser().resolve()
        if path.is_dir():
            path = path / "config.yaml"
        if not path.is_file():
            raise FileNotFoundError(path)
        if path.suffix not in (".yaml", ".yml", ".enhance"):
            raise ValueError(
                "pass a YAML configuration or .enhance model; raw weights require a configuration"
            )
        self._load(
            str(path),
            "packed" if path.suffix == ".enhance" else "file",
            str(path.parent),
            backend,
            engine_threads,
        )

    @classmethod
    def from_config(
        cls,
        yaml: str,
        *,
        base_directory: str | Path,
        backend: str = "onnx",
        engine_threads: int = 1,
    ) -> Denoiser:
        """Load YAML text with an explicit directory for relative weight paths."""
        if not isinstance(yaml, str):
            raise TypeError("yaml must be a string")
        model = cls.__new__(cls)
        model._load(
            yaml, "yaml", str(Path(base_directory).expanduser().resolve()), backend, engine_threads
        )
        return model

    def _load(self, source: str, mode: str, base: str, backend: str, threads: int) -> None:
        if backend not in ("onnx", "tflite"):
            raise ValueError("backend must be 'onnx' or 'tflite'")
        threads = _positive_int(threads, "engine_threads")
        try:
            self._module = import_module(f"giga_edge_enhance._{backend}")
        except ModuleNotFoundError as error:
            if error.name != f"giga_edge_enhance._{backend}":
                raise
            raise ImportError(
                f"The installed SDK does not include the {backend} backend; install a built wheel"
            ) from error
        from .logging import _configure_native

        _configure_native(self._module)
        self._native = self._module.Model(source, mode, base, threads)
        self.info = ModelInfo(
            backend,
            self._native.model_type,
            self._native.sample_rate,
            self._native.frame_size,
            self._native.engine_threads,
            self._native.latency_frames,
        )
        self._lock = Lock()

    def stream(
        self, *, sample_rate: int, output_sample_rate: int | None = None, channels: int = 1
    ) -> DenoiserStream:
        """Create an independent stream; the output rate defaults to the input rate."""
        sample_rate = _positive_int(sample_rate, "sample_rate")
        output_sample_rate = (
            sample_rate
            if output_sample_rate is None
            else _positive_int(output_sample_rate, "output_sample_rate")
        )
        channels = _positive_int(channels, "channels")
        with self._lock:
            if self._native is None:
                raise RuntimeError("Denoiser is closed")
            return DenoiserStream(
                self._module, self._native, self.info, sample_rate, output_sample_rate, channels
            )

    def process(
        self, audio: Audio, *, sample_rate: int, output_sample_rate: int | None = None
    ) -> Audio:
        """Enhance a recording in new state, compensating model delay and padding."""
        audio = _audio(audio)
        channels = 1 if audio.ndim == 1 else audio.shape[1]
        with self.stream(
            sample_rate=sample_rate, output_sample_rate=output_sample_rate, channels=channels
        ) as stream:
            stream._matrix = audio.ndim == 2
            if audio.shape[0] == 0:
                return audio.copy()
            # Fixed hops keep future-context behavior independent of file length.
            parts = [
                stream.process_chunk(audio[offset : offset + stream.input_frame_size])
                for offset in range(0, audio.shape[0], stream.input_frame_size)
            ]
            parts.append(stream.flush())
            output = np.concatenate(parts, axis=0)
            expected = (
                audio.shape[0] * stream.output_sample_rate + stream.sample_rate - 1
            ) // stream.sample_rate
            start = stream.latency_samples
            if output.shape[0] < start + expected:
                raise ProcessingError(
                    f"Native finalization produced {output.shape[0]} samples; need {start + expected}"
                )
            return output[start : start + expected].copy()

    def process_chunks(
        self,
        chunks: Iterable[Audio],
        *,
        sample_rate: int,
        output_sample_rate: int | None = None,
        channels: int = 1,
    ) -> Iterator[Audio]:
        """Yield available stream output and its final tail without removing delay."""
        with self.stream(
            sample_rate=sample_rate, output_sample_rate=output_sample_rate, channels=channels
        ) as stream:
            for chunk in chunks:
                output = stream.process_chunk(chunk)
                if output.shape[0]:
                    yield output
            tail = stream.flush()
            if tail.shape[0]:
                yield tail

    @classmethod
    def from_pretrained(
        cls,
        repo_or_path: str | Path,
        *,
        model: str = "quality48",
        backend: str = "onnx",
        revision: str | None = None,
        cache_dir: str | Path | None = None,
        local_files_only: bool = False,
        token: str | bool | None = None,
        subfolder: str | None = None,
        engine_threads: int = 1,
    ) -> Denoiser:
        """Load a local model or verified catalog exports from a caller-selected Hub repo."""
        from .models import load_pretrained

        return load_pretrained(
            cls,
            repo_or_path,
            model=model,
            backend=backend,
            revision=revision,
            cache_dir=cache_dir,
            local_files_only=local_files_only,
            token=token,
            subfolder=subfolder,
            engine_threads=engine_threads,
        )

    def process_file(self, source: str | Path, destination: str | Path, **options) -> Path:
        """Read/enhance/write with optional SoundFile support and explicit output rate."""
        from .io import process_file

        return process_file(self, source, destination, **options)

    def close(self) -> None:
        """Prevent new streams; existing streams retain their native model."""
        with self._lock:
            self._native = None

    def __enter__(self) -> Denoiser:
        if self._native is None:
            raise RuntimeError("Denoiser is closed")
        return self

    def __exit__(self, *_args: object) -> None:
        self.close()


class DenoiserStream:
    """Stateful, channel-independent denoising with bounded partial-hop buffering."""

    def __init__(
        self, module, model, info: ModelInfo, sample_rate: int, output_rate: int, channels: int
    ):
        self._module, self._model, self._info = module, model, info
        self.sample_rate = sample_rate
        self.output_sample_rate = output_rate
        self.channels = channels
        self.input_frame_size = max(
            1, (info.frame_size * sample_rate + info.sample_rate - 1) // info.sample_rate
        )
        self.latency_samples = info.latency_frames * (
            (info.frame_size * output_rate + info.sample_rate // 2) // info.sample_rate
        )
        self._streams = []
        try:
            for _ in range(channels):
                self._streams.append(module.Stream(model, sample_rate, output_rate))
        except Exception:
            for stream in self._streams:
                stream.close()
            raise
        self._lock = Lock()
        self._closed = False
        self._reset_state()

    def _reset_state(self) -> None:
        self._pending = np.empty((0, self.channels), dtype=np.float32)
        self._dtype = None
        self._matrix = self.channels != 1
        self._flushed = False
        self._started = False

    def _enter_operation(self) -> None:
        if not self._lock.acquire(blocking=False):
            raise RuntimeError("Concurrent use of one stream is not supported")
        if self._closed:
            self._lock.release()
            raise RuntimeError("Stream is closed")

    def _output(self, matrix: NDArray[np.float32]) -> Audio:
        result = matrix if self._matrix else matrix[:, 0]
        return _pcm(result, self._dtype or np.dtype("float32"))

    def _process_frame(self, frame: NDArray[np.float32]) -> NDArray[np.float32]:
        outputs = [
            stream.process(np.ascontiguousarray(frame[:, channel]))
            for channel, stream in enumerate(self._streams)
        ]
        return np.column_stack(outputs)

    def process_chunk(self, audio: Audio) -> Audio:
        """Process a block, retaining an incomplete input hop for the next call."""
        audio = _audio(audio, self.channels)
        self._enter_operation()
        try:
            if self._flushed:
                raise RuntimeError("Stream has been flushed; call reset() before processing")
            if self._dtype is not None and (
                audio.dtype != self._dtype or (audio.ndim == 2) != self._matrix
            ):
                raise ValueError("Stream dtype and channel layout must remain constant")
            if audio.shape[0] == 0:
                shape = (0, self.channels) if audio.ndim == 2 else (0,)
                return np.empty(shape, dtype=audio.dtype)
            self._dtype = audio.dtype
            self._matrix = audio.ndim == 2
            values = audio.astype(np.float32)
            if audio.dtype == np.int16:
                values /= np.float32(32767)
            values = values.reshape(-1, self.channels)
            values = np.concatenate((self._pending, values), axis=0)
            complete = values.shape[0] // self.input_frame_size * self.input_frame_size
            parts = [
                self._process_frame(values[offset : offset + self.input_frame_size])
                for offset in range(0, complete, self.input_frame_size)
            ]
            self._pending = values[complete:].copy()
            self._started |= bool(parts)
            output = (
                np.concatenate(parts, axis=0)
                if parts
                else np.empty((0, self.channels), dtype=np.float32)
            )
            return self._output(output)
        except RuntimeError as error:
            if isinstance(error, ProcessingError) or "Stream has been flushed" in str(error):
                raise
            raise ProcessingError(str(error)) from error
        finally:
            self._lock.release()

    def flush(self) -> Audio:
        """Flush once; later calls return empty arrays until reset()."""
        self._enter_operation()
        try:
            if self._flushed:
                return self._output(np.empty((0, self.channels), dtype=np.float32))
            self._flushed = True
            parts = []
            if self._pending.shape[0]:
                padding = self.input_frame_size - self._pending.shape[0]
                parts.append(self._process_frame(np.pad(self._pending, ((0, padding), (0, 0)))))
                self._started = True
                self._pending = np.empty((0, self.channels), dtype=np.float32)
            if self._started:
                parts.append(np.column_stack([stream.flush() for stream in self._streams]))
            output = (
                np.concatenate(parts, axis=0)
                if parts
                else np.empty((0, self.channels), dtype=np.float32)
            )
            return self._output(output)
        except RuntimeError as error:
            raise ProcessingError(str(error)) from error
        finally:
            self._lock.release()

    @property
    def snr(self) -> tuple[float, ...]:
        """Current native SNR estimate for each independent channel."""
        self._enter_operation()
        try:
            return tuple(stream.snr for stream in self._streams)
        finally:
            self._lock.release()

    def reset(self) -> None:
        self._enter_operation()
        try:
            for stream in self._streams:
                stream.reset()
            self._reset_state()
        finally:
            self._lock.release()

    def fresh(self) -> DenoiserStream:
        """Create independent state with the same model/rates/channel layout."""
        self._enter_operation()
        try:
            return DenoiserStream(
                self._module,
                self._model,
                self._info,
                self.sample_rate,
                self.output_sample_rate,
                self.channels,
            )
        finally:
            self._lock.release()

    def close(self) -> None:
        if not self._lock.acquire(blocking=False):
            raise RuntimeError("Concurrent use of one stream is not supported")
        try:
            if not self._closed:
                for stream in self._streams:
                    stream.close()
                self._streams.clear()
                self._model = None
                self._closed = True
                self._pending = np.empty((0, self.channels), dtype=np.float32)
        finally:
            self._lock.release()

    def __enter__(self) -> DenoiserStream:
        if self._closed:
            raise RuntimeError("Stream is closed")
        return self

    def __exit__(self, *_args: object) -> None:
        self.close()
