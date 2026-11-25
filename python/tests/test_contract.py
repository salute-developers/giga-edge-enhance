from pathlib import Path
import re


def test_python_version_comes_from_release_source(root):
    if Path("VERSION_NUMBER").exists():
        from scikit_build_core.metadata.regex import dynamic_metadata

        version = dynamic_metadata(
            "version", {"input": "VERSION_NUMBER", "regex": r"^(?P<value>\d+\.\d+\.\d+)"}
        )
        assert version == (root / "VERSION_NUMBER").read_text().strip()
    else:
        from importlib.metadata import version as installed_version

        assert (
            installed_version("giga-edge-enhance") == (root / "VERSION_NUMBER").read_text().strip()
        )


def test_source_directory_names(root):
    for directory in (root / "python").rglob("*"):
        if directory.is_dir() and "__pycache__" not in directory.parts:
            assert re.fullmatch(r"[a-z_][a-z0-9_]*", directory.name), directory


def test_all_model_exports_are_real_lfs_objects(root):
    for name, backend, extension in (
        ("quality48", "onnx", "onnx"),
        ("hybrid48", "onnx", "onnx"),
        ("fast8", "onnx", "onnx"),
        ("quality48", "tflite", "tflite"),
        ("hybrid48", "tflite", "tflite"),
    ):
        path = root / "models" / name / backend / f"model.{extension}"
        with path.open("rb") as handle:
            assert not handle.read(128).startswith(
                b"version https://git-lfs.github.com/spec/v1"
            ), path
        assert path.stat().st_size > 1024, path
