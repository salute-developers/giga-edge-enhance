import hashlib
import logging
from pathlib import Path
import subprocess
import sys
from types import SimpleNamespace

import numpy as np
import pytest

from giga_edge_enhance import (
    Denoiser,
    EchoCanceller,
    Limiter,
    ModelIntegrityError,
    ProcessingError,
    available_models,
    process_file,
    read_audio,
    set_log_level,
    write_audio,
)


def test_catalog_matches_every_checked_out_export(root):
    entries = available_models()
    assert len(entries) == 5
    for entry in entries:
        for name, digest in entry.files.items():
            path = root / "models" / entry.subfolder / name
            assert hashlib.sha256(path.read_bytes()).hexdigest() == digest


def test_local_pretrained_needs_no_hub_and_resolves_paths(root, tmp_path, monkeypatch):
    monkeypatch.chdir(tmp_path)
    monkeypatch.setitem(sys.modules, "huggingface_hub", None)
    with pytest.raises(FileNotFoundError):
        Denoiser.from_pretrained(tmp_path / "missing")
    for path in (root / "models/quality48/onnx", root / "models"):
        with Denoiser.from_pretrained(path) as model:
            assert model.process(np.zeros(773, np.float32), sample_rate=48000).shape == (773,)


def test_hub_forwards_revision_offline_cache_token_and_never_rewrites_snapshot(
    root, tmp_path, monkeypatch
):
    directory = root / "models/quality48/onnx"
    original = (directory / "config.yaml").read_bytes()
    calls = []

    def download(**kwargs):
        calls.append(kwargs)
        return str(directory / Path(kwargs["filename"]).name)

    monkeypatch.setitem(sys.modules, "huggingface_hub", SimpleNamespace(hf_hub_download=download))
    with Denoiser.from_pretrained(
        "example/repository",
        revision="fixed-commit",
        cache_dir=tmp_path,
        local_files_only=True,
        token="test-only-token",
    ) as model:
        assert model.info.sample_rate == 48000
    assert (directory / "config.yaml").read_bytes() == original
    assert len(calls) == 2
    for call in calls:
        assert call["revision"] == "fixed-commit" and call["local_files_only"] is True
        assert call["cache_dir"] == tmp_path and call["token"] == "test-only-token"
        assert call["repo_id"] == "example/repository"


def test_bad_hash_fails_before_native_loading(tmp_path, monkeypatch):
    path = tmp_path / "config.yaml"
    path.write_text("unexpected configuration")
    monkeypatch.setitem(
        sys.modules, "huggingface_hub", SimpleNamespace(hf_hub_download=lambda **_: str(path))
    )
    with pytest.raises(ModelIntegrityError, match="SHA-256"):
        Denoiser.from_pretrained("example/repository", revision="fixed-commit")


def test_offline_miss_does_not_retry_or_expose_token(monkeypatch):
    calls = []

    def download(**kwargs):
        calls.append(kwargs)
        raise RuntimeError("offline cache miss")

    monkeypatch.setitem(sys.modules, "huggingface_hub", SimpleNamespace(hf_hub_download=download))
    with pytest.raises(ProcessingError) as error:
        Denoiser.from_pretrained(
            "example/repository", local_files_only=True, token="private-test-token"
        )
    assert len(calls) == 1 and calls[0]["local_files_only"]
    assert "private-test-token" not in str(error.value)


def test_catalog_selection_and_subfolder_validation():
    with pytest.raises(ValueError, match="ONNX"):
        Denoiser.from_pretrained("example/repository", model="fast8", backend="tflite")
    with pytest.raises(ValueError, match="subfolder"):
        Denoiser.from_pretrained("example/repository", subfolder="../escape")


@pytest.mark.parametrize("dtype", [np.float32, np.int16])
def test_wav_read_write_preserves_rate_layout_and_dtype(tmp_path, dtype):
    audio = np.random.default_rng(318).normal(0, 0.1, (1603, 2)).astype(np.float32)
    if dtype == np.int16:
        audio = (audio * 32767).astype(np.int16)
    path = write_audio(tmp_path / "input.wav", audio, 16000)
    recording = read_audio(path, dtype=np.dtype(dtype).name)
    assert recording.sample_rate == 16000 and recording.samples.dtype == dtype
    np.testing.assert_array_equal(recording.samples, audio)


def test_float_wav_to_pcm16_is_scaled_explicitly(tmp_path):
    audio = np.array([-1, -0.5, 0, 0.5, 1], np.float32)
    recording = read_audio(write_audio(tmp_path / "float.wav", audio, 48000), dtype="int16")
    np.testing.assert_array_equal(recording.samples, [-32767, -16384, 0, 16384, 32767])
    with pytest.raises(ValueError, match="subtype"):
        write_audio(tmp_path / "float.flac", audio, 48000)


def test_process_file_checks_rates_and_reference(root, tmp_path):
    audio = np.zeros(1607, np.float32)
    source = write_audio(tmp_path / "input.wav", audio, 16000)
    with Denoiser(root / "models/quality48/onnx") as model:
        output = model.process_file(source, tmp_path / "enhanced.wav", output_sample_rate=48000)
        recording = read_audio(output)
        assert recording.sample_rate == 48000 and len(recording.samples) == len(audio) * 3
    with Limiter(sample_rate=48000) as limiter:
        with pytest.raises(ValueError, match="sample_rate"):
            process_file(limiter, source, tmp_path / "incorrect.wav")
    with EchoCanceller() as aec:
        with pytest.raises(ValueError, match="reference"):
            aec.process_file(source, tmp_path / "missing-reference.wav")
        output = aec.process_file(source, tmp_path / "aec.wav", reference=source)
        assert read_audio(output).samples.shape == audio.shape


def test_sdk_logging_does_not_modify_python_root_and_validates_levels():
    root = logging.getLogger()
    before = (root.level, tuple(root.handlers))
    set_log_level("off")
    with Limiter() as limiter:
        limiter.process(np.zeros(12, np.float32))
    assert before == (root.level, tuple(root.handlers))
    with pytest.raises(ValueError):
        set_log_level("invalid")
    set_log_level("warning")


def test_base_import_has_no_optional_or_native_import_side_effects(tmp_path):
    code = "import sys; import giga_edge_enhance; assert not any(name in sys.modules for name in ('soundfile','huggingface_hub','giga_edge_enhance._onnx','giga_edge_enhance._tflite','giga_edge_enhance._dsp'))"
    subprocess.run([sys.executable, "-c", code], cwd=tmp_path, check=True, timeout=60)
