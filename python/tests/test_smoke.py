import ctypes
import importlib
from pathlib import Path
import subprocess
import sys

import pytest

import giga_edge_enhance

pytestmark = pytest.mark.native


def test_installed_package_is_outside_source(root):
    package = Path(giga_edge_enhance.__file__).resolve().parent
    assert not package.is_relative_to(root / "python/src")
    assert (package / "py.typed").is_file()
    assert not any(path.suffix in (".onnx", ".tflite", ".enhance") for path in package.rglob("*"))
    assert not any(
        "python-reference" in path.name or "sanitized" in path.name for path in package.rglob("*")
    )


@pytest.mark.parametrize("order", [("onnx", "tflite"), ("tflite", "onnx")])
def test_both_backend_import_orders(order, tmp_path):
    code = "\n".join(
        f"from giga_edge_enhance import _{backend}; assert _{backend}.backend == {backend!r}"
        for backend in order
    )
    subprocess.run([sys.executable, "-c", code], cwd=tmp_path, check=True, timeout=60)


def test_backend_exports_are_module_local(backend):
    module = importlib.import_module(f"giga_edge_enhance._{backend}")
    assert module.Model.__module__ == f"giga_edge_enhance._{backend}"
    assert module.Stream.__module__ == f"giga_edge_enhance._{backend}"
    library = ctypes.CDLL(module.__file__)
    assert getattr(library, f"PyInit__{backend}")
    with pytest.raises(AttributeError):
        getattr(library, "GigaEdgeEnhanceDenoiseInit")
