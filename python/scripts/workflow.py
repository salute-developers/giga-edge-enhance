"""Python package actions, invoked by tools/build, tools/test and tools/package."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import shutil
import subprocess
import sys
import tempfile
import venv


ROOT = Path(__file__).resolve().parents[2]


def execute(command: list[str], *, cwd: Path = ROOT, environment: dict | None = None) -> None:
    subprocess.run(command, cwd=cwd, env=environment, check=True)


def interpreter_key(abi: str | None = None) -> str:
    abi = abi or f"cp{sys.version_info.major}{sys.version_info.minor}"
    return f"{abi}-{sys.platform}-{platform.machine().lower()}"


def environment_python(directory: Path) -> Path:
    return directory / ("Scripts/python.exe" if os.name == "nt" else "bin/python")


def select_wheel(directory: Path) -> Path:
    version = (ROOT / "VERSION_NUMBER").read_text().strip()
    abi = f"cp{sys.version_info.major}{sys.version_info.minor}"
    wheels = list(directory.glob(f"giga_edge_enhance-{version}-{abi}-*.whl"))
    if len(wheels) != 1:
        raise SystemExit(f"Expected one built {abi} wheel in {directory}; run tools/build first.")
    return wheels[0]


def sanitizer_python(environment: dict, reference_root: Path, directory: Path) -> str:
    if sys.platform != "darwin":
        raise SystemExit("Automatic Python ASAN runtime setup currently supports macOS only.")
    launcher = reference_root / "onnx/python-reference/enhance-python-sanitized"
    if not launcher.is_file():
        raise SystemExit("Rebuild the sanitized wheel to prepare its matching test interpreter.")
    executable = directory / "bin/enhance-python-sanitized"
    shutil.copy2(launcher, executable)
    environment["ASAN_OPTIONS"] = "detect_leaks=0:halt_on_error=1"
    environment["PYTHONMALLOC"] = "malloc"
    return str(executable)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("action", choices=("build", "test", "package"))
    parser.add_argument("target")
    args = parser.parse_args()
    if not (3, 10) <= sys.version_info[:2] < (3, 15):
        raise SystemExit("Python SDK workflows require CPython 3.10 through 3.14.")
    workspace = ROOT / "out/python" / args.target
    artifacts = ROOT / "out/artifacts" / args.target
    environment = os.environ.copy()
    environment.pop("PYTHONPATH", None)
    environment["PYTHONNOUSERSITE"] = "1"
    environment.setdefault("CMAKE_BUILD_PARALLEL_LEVEL", "2")

    if args.action == "build":
        directory = workspace / "build-env"
        if not environment_python(directory).is_file():
            venv.create(directory, with_pip=True)
        python = str(environment_python(directory))
        execute(
            [python, "-m", "pip", "install", "-r", "python/requirements-dev.txt"],
            environment=environment,
        )
        environment["PATH"] = str(Path(python).parent) + os.pathsep + environment.get("PATH", "")
        artifacts.mkdir(parents=True, exist_ok=True)
        execute(
            [python, "-m", "build", "--wheel", "--no-isolation", "--outdir", str(artifacts)],
            environment=environment,
        )
        wheel = select_wheel(artifacts)
        (workspace / "build-info.json").write_text(
            json.dumps(
                {
                    "sanitized": environment.get("GEE_PYTHON_SANITIZE") == "1",
                    "wheel_sha256": hashlib.sha256(wheel.read_bytes()).hexdigest(),
                }
            )
            + "\n"
        )
    elif args.action == "test":
        wheel = select_wheel(artifacts)
        wheel_tag = "-".join(wheel.stem.split("-")[-3:])
        reference_root = ROOT / "out/python/build" / wheel_tag
        environment["GEE_PYTHON_REFERENCE_ROOT"] = str(reference_root)
        state = json.loads((workspace / "build-info.json").read_text())
        if hashlib.sha256(wheel.read_bytes()).hexdigest() != state["wheel_sha256"]:
            raise SystemExit(
                "Wheel does not match the last successful build; rebuild before testing."
            )
        directory = workspace / "test-env"
        if not environment_python(directory).is_file():
            venv.create(directory, with_pip=True)
        python = str(environment_python(directory))
        numpy = "numpy==2.3.4" if sys.version_info[:2] >= (3, 14) else "numpy==2.2.6"
        execute(
            [
                python,
                "-m",
                "pip",
                "install",
                "pytest==8.4.2",
                "mypy==1.18.2",
                "soundfile==0.14.0",
                "huggingface-hub==1.32.0",
                numpy,
                str(wheel),
                "--force-reinstall",
            ],
            environment=environment,
        )
        if state["sanitized"]:
            python = sanitizer_python(environment, reference_root, directory)
        with tempfile.TemporaryDirectory(prefix="enhance-python-tests-") as temporary:
            execute(
                [python, "-m", "pytest", str(ROOT / "python/tests"), "-q"],
                cwd=Path(temporary),
                environment=environment,
            )
            execute(
                [
                    python,
                    "-m",
                    "mypy",
                    "--strict",
                    "--follow-imports=silent",
                    "--cache-dir",
                    str(workspace / "mypy"),
                    str(ROOT / "python/tests/typing_consumer.py"),
                ],
                cwd=Path(temporary),
                environment=environment,
            )
            execute(
                [
                    python,
                    "-m",
                    "mypy.stubtest",
                    "giga_edge_enhance",
                    "--ignore-missing-stub",
                    "--concise",
                ],
                cwd=Path(temporary),
                environment=environment,
            )
    else:
        wheel = select_wheel(artifacts)
        state = json.loads((workspace / "build-info.json").read_text())
        digest = hashlib.sha256(wheel.read_bytes()).hexdigest()
        if digest != state["wheel_sha256"]:
            raise SystemExit(
                "Wheel does not match the last successful build; rebuild before packaging."
            )
        if state["sanitized"]:
            raise SystemExit(
                "Sanitized wheels are for local checks. Rebuild without GEE_PYTHON_SANITIZE before packaging."
            )
        manifest = {
            "format": 1,
            "target": args.target,
            "version": (ROOT / "VERSION_NUMBER").read_text().strip(),
            "artifacts": {wheel.name: digest},
        }
        (artifacts / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
        products = [wheel, artifacts / "manifest.json"]
        (artifacts / "SHA256SUMS").write_text(
            "".join(
                f"{hashlib.sha256(path.read_bytes()).hexdigest()}  {path.name}\n"
                for path in products
            )
        )


if __name__ == "__main__":
    main()
