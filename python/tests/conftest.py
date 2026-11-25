from pathlib import Path

import pytest


ROOT = Path(__file__).resolve().parents[2]


@pytest.fixture
def root():
    return ROOT


@pytest.fixture(params=["onnx", "tflite"])
def backend(request):
    return request.param


@pytest.fixture
def config(root, backend):
    return root / "models/quality48" / backend / "config.yaml"
