"""Configure SDK-owned loggers without changing Python or host-native root loggers."""

import sys

_level = 3
_levels = {"trace": 0, "debug": 1, "info": 2, "warning": 3, "error": 4, "off": 5}


def _configure_native(module) -> None:
    module.set_log_level(_level)


def set_log_level(level: str | int = "warning") -> None:
    """Set all loaded SDK modules and the default for future backend imports."""
    global _level
    if isinstance(level, str):
        if level not in _levels:
            raise ValueError(f"level must be one of {tuple(_levels)}")
        level = _levels[level]
    if isinstance(level, bool) or not isinstance(level, int) or not 0 <= level <= 5:
        raise ValueError("level must be an SDK level name or integer in [0, 5]")
    _level = level
    for name in ("giga_edge_enhance._dsp", "giga_edge_enhance._onnx", "giga_edge_enhance._tflite"):
        if name in sys.modules:
            _configure_native(sys.modules[name])
