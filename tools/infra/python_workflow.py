"""Build and verify the Python distribution through the canonical tools interface."""

import os
import platform
import shlex
import sys
import venv

from .config import ROOT
from .dependencies import find_prebuilt_dependency, prebuilt_cmake_arguments
from .execution import compiler_cache_environment, run


def python_workflow(action: str, target: str, host: bool) -> None:
    if target in {"python-wheels", "python-sdist", "python-lint"}:
        if sys.version_info < (3, 11):
            raise SystemExit(
                "Distribution and lint tooling requires Python 3.11+; SDK runtime supports 3.10+."
            )
        if target == "python-lint" and action != "test":
            raise SystemExit("Use tools/test python-lint.")
        directory = ROOT / "out/python/wheels/driver-env"
        python = directory / ("Scripts/python.exe" if os.name == "nt" else "bin/python")
        if not python.is_file():
            venv.create(directory, with_pip=True)
        run([str(python), "-m", "pip", "install", "-r", "python/requirements-wheels.txt"])
        script = {
            "python-wheels": "wheels.py",
            "python-sdist": "sdist.py",
            "python-lint": "lint.py",
        }[target]
        arguments = [] if target == "python-lint" else [action]
        run([str(python), f"python/scripts/{script}", *arguments])
        return
    if target == "python-macos" and platform.system() != "Darwin":
        raise SystemExit("python-macos requires the macOS host toolchain.")
    if target == "python-windows" and os.name != "nt":
        raise SystemExit("python-windows requires the Windows host toolchain.")
    use_host = host or target != "python"
    stage = None if use_host else "native"
    environment = compiler_cache_environment(not use_host)
    if jobs := os.environ.get("CMAKE_BUILD_PARALLEL_LEVEL"):
        environment["CMAKE_BUILD_PARALLEL_LEVEL"] = jobs
    arguments = shlex.split(os.environ.get("CMAKE_ARGS", ""))
    arguments.append("-DGEE_PYTHON_BUILD_TEST_REFERENCE=ON")
    sanitizer = os.environ.get("GEE_PYTHON_SANITIZE", "0")
    if sanitizer not in ("0", "1"):
        raise SystemExit("GEE_PYTHON_SANITIZE must be 0 or 1.")
    arguments.append(f"-DGEE_PYTHON_SANITIZE={'ON' if sanitizer == '1' else 'OFF'}")
    environment["GEE_PYTHON_SANITIZE"] = sanitizer
    for backend in ("onnx", "tflite"):
        if target == "python-macos":
            arch = "arm64" if platform.machine().lower() in {"arm64", "aarch64"} else "x86_64"
            preset = f"macos-{arch}-release-{backend}"
        elif target == "python-windows":
            preset = f"windows-x64-release-{backend}"
        else:
            preset = f"native-release-{backend}"
        prefix = find_prebuilt_dependency(preset, use_host)
        arguments.extend(prebuilt_cmake_arguments(preset, use_host, prefix, not use_host))
    # These are build-frontend arguments, never shell command text.
    if arguments:
        environment["CMAKE_ARGS"] = shlex.join(arguments)
    run(
        [sys.executable if use_host else "python3", "python/scripts/workflow.py", action, target],
        container_stage=stage,
        environment=environment,
        workdir=ROOT,
    )
