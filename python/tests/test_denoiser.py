from concurrent.futures import ThreadPoolExecutor
import os
from pathlib import Path
import struct
import subprocess

import numpy as np
import pytest

from giga_edge_enhance import Denoiser

pytestmark = pytest.mark.native


def signal(size, rate=48000):
    time = np.arange(size, dtype=np.float32) / np.float32(rate)
    return (0.12 * np.sin(2 * np.pi * 613 * time) + 0.03 * np.sin(2 * np.pi * 191 * time)).astype(
        np.float32
    )


def test_metadata_and_relative_paths(config, backend, tmp_path, monkeypatch):
    monkeypatch.chdir(tmp_path)
    with Denoiser(config, backend=backend) as model:
        assert model.info.sample_rate == 48000
        assert model.info.frame_size == 768
        assert model.info.backend == backend
        assert model.info.engine_threads == 1
        assert model.process(signal(2001), sample_rate=48000).shape == (2001,)


@pytest.mark.parametrize(
    "name,engine",
    [
        ("quality48", "onnx"),
        ("quality48", "tflite"),
        ("hybrid48", "onnx"),
        ("hybrid48", "tflite"),
        ("fast8", "onnx"),
    ],
)
def test_all_available_model_backend_pairs(root, name, engine):
    with Denoiser(root / "models" / name / engine, backend=engine) as model:
        audio = signal(model.info.frame_size * 5 + 13, model.info.sample_rate)
        output = model.process(audio, sample_rate=model.info.sample_rate)
        assert output.shape == audio.shape
        assert output.dtype == np.float32
        assert np.isfinite(output).all()
        assert np.any(output != 0)


@pytest.mark.parametrize("size", [0, 1, 127, 768, 2001])
def test_arbitrary_recording_lengths(config, backend, size):
    with Denoiser(config, backend=backend) as model:
        output = model.process(signal(size), sample_rate=48000)
        assert output.shape == (size,)
        assert np.isfinite(output).all()


def test_stream_chunk_boundaries_and_reset(config, backend):
    with Denoiser(config, backend=backend) as model:
        audio = signal(768 * 7 + 109)
        reference = model.process(audio, sample_rate=48000)
        with model.stream(sample_rate=48000) as stream:
            assert stream.snr == (0.0,)
            pieces = [stream.process_chunk(chunk) for chunk in np.array_split(audio, 19)]
            pieces.append(stream.flush())
            actual = np.concatenate(pieces)[
                stream.latency_samples : stream.latency_samples + audio.size
            ]
            np.testing.assert_allclose(actual, reference, atol=1e-6)
            iterated = np.concatenate(
                list(model.process_chunks(iter(np.array_split(audio, 19)), sample_rate=48000))
            )
            np.testing.assert_allclose(iterated, np.concatenate(pieces), atol=1e-6)
            assert list(model.process_chunks(iter(()), sample_rate=48000)) == []
            assert stream.flush().size == 0
            with pytest.raises(RuntimeError, match="flushed"):
                stream.process_chunk(audio[:768])
            stream.reset()
            reset = np.concatenate([stream.process_chunk(audio), stream.flush()])
            np.testing.assert_allclose(
                reset[stream.latency_samples : stream.latency_samples + audio.size],
                reference,
                atol=1e-6,
            )
            assert len(stream.snr) == 1


@pytest.mark.parametrize("rate", [8000, 16000, 44100, 48000])
def test_resampling_length_and_tail(config, backend, rate):
    with Denoiser(config, backend=backend) as model:
        audio = signal(rate // 5 + 17, rate)
        result = model.process(audio, sample_rate=rate, output_sample_rate=48000)
        assert result.shape == ((audio.size * 48000 + rate - 1) // rate,)
        assert np.isfinite(result).all()


@pytest.mark.parametrize("output_rate", [8000, 16000, 44100])
@pytest.mark.parametrize("size", [1, 768, 2304, 2401])
def test_output_resampling_counts(config, backend, output_rate, size):
    with Denoiser(config, backend=backend) as model:
        result = model.process(signal(size), sample_rate=48000, output_sample_rate=output_rate)
        assert result.shape == ((size * output_rate + 48000 - 1) // 48000,)
        assert np.isfinite(result).all()


def test_resampled_stream_chunk_boundaries(config, backend):
    audio = signal(44100 // 5 + 17, 44100)
    with Denoiser(config, backend=backend) as model:
        expected = model.process(audio, sample_rate=44100, output_sample_rate=16000)
        with model.stream(sample_rate=44100, output_sample_rate=16000) as stream:
            pieces = [stream.process_chunk(part) for part in np.array_split(audio, 23)]
            pieces.append(stream.flush())
            actual = np.concatenate(pieces)[
                stream.latency_samples : stream.latency_samples + expected.size
            ]
            np.testing.assert_allclose(actual, expected, atol=1e-6)


def test_pcm16_and_independent_channels(config, backend):
    with Denoiser(config, backend=backend) as model:
        left = (signal(2309) * 32767).astype(np.int16)
        stereo = np.column_stack([left, -left])
        output = model.process(stereo, sample_rate=48000)
        assert output.dtype == np.int16
        assert output.shape == stereo.shape
        for channel in range(2):
            np.testing.assert_array_equal(
                output[:, channel], model.process(stereo[:, channel], sample_rate=48000)
            )
        assert model.process(left[:, None], sample_rate=48000).shape == (left.size, 1)


def test_model_close_retains_existing_stream(config, backend):
    model = Denoiser(config, backend=backend)
    stream = model.stream(sample_rate=48000)
    model.close()
    model.close()
    with pytest.raises(RuntimeError, match="closed"):
        model.stream(sample_rate=48000)
    assert np.isfinite(stream.process_chunk(signal(1536))).all()
    stream.flush()
    stream.close()
    stream.close()
    with pytest.raises(RuntimeError, match="closed"):
        stream.process_chunk(signal(768))


def test_concurrent_independent_streams(config, backend):
    with Denoiser(config, backend=backend) as model:
        arrays = [signal(768 * count + 29) for count in (3, 5)]
        expected = [model.process(audio, sample_rate=48000) for audio in arrays]
        with ThreadPoolExecutor(max_workers=2) as pool:
            actual = list(pool.map(lambda audio: model.process(audio, sample_rate=48000), arrays))
        for output, reference in zip(actual, expected):
            np.testing.assert_allclose(output, reference, atol=1e-6)


def test_both_backends_execute_in_one_process(root):
    with (
        Denoiser(root / "models/quality48/onnx", backend="onnx") as onnx,
        Denoiser(root / "models/quality48/tflite", backend="tflite") as tflite,
    ):
        audio = signal(768 * 5 + 29)
        expected = [model.process(audio, sample_rate=48000) for model in (onnx, tflite)]
        with ThreadPoolExecutor(max_workers=2) as pool:
            jobs = [
                pool.submit(model.process, audio, sample_rate=48000) for model in (onnx, tflite)
            ]
            for job, reference in zip(jobs, expected):
                np.testing.assert_allclose(job.result(), reference, atol=1e-6)


def test_configuration_text_and_packed_export(config, backend, tmp_path):
    yaml = config.read_text()
    with Denoiser.from_config(yaml, base_directory=config.parent, backend=backend) as model:
        expected = model.process(signal(2401), sample_rate=48000)
    config_bytes = yaml.encode()
    weights = config.with_name("model.onnx" if backend == "onnx" else "model.tflite").read_bytes()
    packed = tmp_path / "test.enhance"
    packed.write_bytes(
        struct.pack("<I", len(config_bytes))
        + config_bytes
        + struct.pack("<I", len(weights))
        + weights
    )
    with Denoiser(packed, backend=backend) as model:
        np.testing.assert_allclose(
            model.process(signal(2401), sample_rate=48000), expected, atol=1e-6
        )


def test_invalid_audio_and_stream_contracts(config, backend):
    with Denoiser(config, backend=backend) as model:
        for invalid in (
            [],
            np.zeros(20, dtype=np.float64),
            np.zeros((2, 2, 2), dtype=np.float32),
            np.array([np.nan], dtype=np.float32),
        ):
            with pytest.raises((TypeError, ValueError)):
                model.process(invalid, sample_rate=48000)
        with pytest.raises(ValueError):
            model.stream(sample_rate=0)
        with model.stream(sample_rate=48000) as stream:
            assert stream.process_chunk(signal(4)).size == 0
            with pytest.raises(ValueError, match="dtype"):
                stream.process_chunk(np.zeros(8, dtype=np.int16))
            with pytest.raises(ValueError, match="channels"):
                stream.process_chunk(np.zeros((8, 2), dtype=np.float32))
            assert stream._lock.acquire(blocking=False)
            try:
                with pytest.raises(RuntimeError, match="Concurrent"):
                    stream.process_chunk(signal(4))
            finally:
                stream._lock.release()


def test_unsupported_fast8_tflite(root):
    with pytest.raises(ValueError, match="ONNX"):
        Denoiser(root / "models/fast8/onnx", backend="tflite")


def test_non_contiguous_audio(config, backend):
    with Denoiser(config, backend=backend) as model:
        view = signal(4800)[::2]
        assert not view.flags.c_contiguous
        np.testing.assert_allclose(
            model.process(view, sample_rate=48000),
            model.process(view.copy(), sample_rate=48000),
            atol=1e-6,
        )


def test_identity_model_preserves_alignment_and_tail(root, backend):
    config = root / "models/hybrid48" / backend / "config.yaml"
    extension = "onnx" if backend == "onnx" else "tflite"
    weights = root / "core/tests/data/models" / f"hybrid_identity.{extension}"
    yaml = config.read_text().replace(f"model.{extension}", str(weights))
    with Denoiser.from_config(yaml, base_directory=root, backend=backend) as model:
        audio = signal(768 * 4 + 97)
        audio[-1] = 0.21  # A distinctive tail catches truncation/shift errors.
        output = model.process(audio, sample_rate=48000)
        np.testing.assert_allclose(output, audio, atol=2e-5)


@pytest.mark.parametrize(
    "name,backend",
    [
        ("quality48", "onnx"),
        ("quality48", "tflite"),
        ("hybrid48", "onnx"),
        ("hybrid48", "tflite"),
        ("fast8", "onnx"),
    ],
)
def test_matches_independent_cpp_consumer(root, name, backend, tmp_path):
    reference_root = os.environ.get("GEE_PYTHON_REFERENCE_ROOT")
    if reference_root is None:
        pytest.skip("Run the canonical tools/test action to compare the C++ consumer")
    executable = (
        Path(reference_root)
        / backend
        / "python-reference"
        / ("enhance-python-reference.exe" if os.name == "nt" else "enhance-python-reference")
    )
    assert executable.is_file(), "Rebuild with tools/build to prepare the C++ reference consumer"
    config = root / "models" / name / backend / "config.yaml"
    with Denoiser(config, backend=backend) as model:
        rate = model.info.sample_rate
        audio = signal(model.info.frame_size * 5 + 19, rate)
        source = tmp_path / "input.f32"
        destination = tmp_path / "output.f32"
        audio.tofile(source)
        subprocess.run(
            [str(executable), str(config), str(source), str(destination), str(rate), str(rate)],
            check=True,
            timeout=60,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )
        np.testing.assert_allclose(
            model.process(audio, sample_rate=rate),
            np.fromfile(destination, dtype=np.float32),
            atol=1e-6,
        )
