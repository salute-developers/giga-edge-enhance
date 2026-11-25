# GigaEdge Enhance for Python

Process speech recordings and NumPy audio streams with the native GigaEdge Enhance engine.

## Quick start

From the SDK checkout, fetch the model files and build the package with WAV support.
The source build requires Git LFS and a C++17 toolchain.

```sh
git lfs pull
python -m pip install ".[io]"
```

```python
from giga_edge_enhance import Denoiser

with Denoiser.from_pretrained("models") as model:
    model.process_file("samples/input-48k.wav", "enhanced.wav")
```

The default is `quality48` with ONNX. Select `model="hybrid48"` for full-band speech refinement or `model="fast8"` for 8 kHz speech. Both 48 kHz models also support `backend="tflite"`. To load weights from the Hub, install `.[io,hub]` and pass the repository ID to `from_pretrained`. [Models on Hugging Face](https://huggingface.co/ai-sage/GigaEdge-Enhance)

[Streaming and DSP examples](examples/) · [API reference](reference.md) · [Build and test](../tools/README.md#python-sdk)
