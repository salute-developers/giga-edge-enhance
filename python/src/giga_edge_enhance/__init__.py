"""GigaEdge Enhance: native speech enhancement for NumPy pipelines."""

import os
from pathlib import Path
import sys

# Keep handles alive for private runtime DLL lookup on Python 3.8+ for Windows.
_dll_directories = []
if sys.platform == "win32":
    _runtime = Path(__file__).parent / "lib"
    if _runtime.is_dir():
        _dll_directories.append(os.add_dll_directory(str(_runtime)))

from .denoiser import Audio, Denoiser, DenoiserStream, ModelInfo, ProcessingError
from .aec import EchoCanceller
from .pipeline import AudioChunk, Observer, Pipeline
from .models import ModelDescriptor, ModelIntegrityError, available_models
from .io import AudioData, read_audio, write_audio, process_file
from .logging import set_log_level
from .config import (
    AGCConfig,
    CompressorConfig,
    EchoCancellerConfig,
    LimiterConfig,
    LoudnessControlConfig,
    LoudnessControlState,
    LoudnessState,
    NoiseGateConfig,
    RangeControlConfig,
    VadResult,
)
from .dsp import (
    AGC,
    AudioProcessor,
    Compressor,
    Limiter,
    LoudnessControl,
    LoudnessMeter,
    MuteFrameDetector,
    NoiseGate,
    RangeControl,
    Resampler,
    RNNoise,
    VoiceActivityDetector,
)

__all__ = [
    "Denoiser",
    "Audio",
    "DenoiserStream",
    "ModelInfo",
    "ProcessingError",
    "AGC",
    "AGCConfig",
    "AudioProcessor",
    "RangeControl",
    "RangeControlConfig",
    "LoudnessControl",
    "LoudnessControlConfig",
    "NoiseGate",
    "NoiseGateConfig",
    "Limiter",
    "LimiterConfig",
    "Compressor",
    "CompressorConfig",
    "RNNoise",
    "VoiceActivityDetector",
    "VadResult",
    "LoudnessMeter",
    "LoudnessState",
    "LoudnessControlState",
    "Resampler",
    "MuteFrameDetector",
    "EchoCancellerConfig",
    "EchoCanceller",
    "Pipeline",
    "Observer",
    "AudioChunk",
    "ModelDescriptor",
    "ModelIntegrityError",
    "available_models",
    "AudioData",
    "read_audio",
    "write_audio",
    "process_file",
    "set_log_level",
]
