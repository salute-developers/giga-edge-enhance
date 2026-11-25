"""Canonical repaired-wheel, installed-package and source-distribution workflows."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import shlex
import shutil
import subprocess
import sys
import tarfile
import tempfile
import venv
import zipfile

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
from infra.dependencies import find_prebuilt_dependency, prepare_dependency
from infra.execution import ensure_container_image, run
from infra.identity import dependency_toolchain_identity
from workflow import environment_python, interpreter_key

WORKSPACE = ROOT / "out/python/wheels"
PRODUCTS = ROOT / "out/artifacts/python-wheels"
RELEASE = ROOT / "out/artifacts/python-release"


def digest(path: Path) -> str:
    checksum = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            checksum.update(chunk)
    return checksum.hexdigest()


def execute(command: list[str], *, cwd: Path = ROOT, environment: dict | None = None) -> None:
    logs = ROOT / "out/python/logs"
    logs.mkdir(parents=True, exist_ok=True)
    with (logs / "wheels.log").open("a") as log:
        with subprocess.Popen(
            command,
            cwd=cwd,
            env=environment,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
        ) as process:
            for line in process.stdout:
                print(line, end="", flush=True)
                log.write(line)
            status = process.wait()
        if status:
            raise subprocess.CalledProcessError(status, command)


def target_platform() -> str:
    value = os.environ.get("GEE_PYTHON_WHEEL_PLATFORM", sys.platform)
    value = {"darwin": "macos", "win32": "windows"}.get(value, value)
    if value not in ("linux", "macos", "windows"):
        raise SystemExit(f"Unsupported wheel platform: {value}")
    if value != "linux" and value != {"darwin": "macos", "win32": "windows"}.get(sys.platform):
        raise SystemExit(f"{value} wheels require their matching host toolchain")
    return value


def presets(target: str) -> tuple[str, str]:
    if target == "linux":
        prefix = "python-release"
    elif target == "windows":
        prefix = "windows-x64-release"
    else:
        arch = "arm64" if platform.machine().lower() in ("arm64", "aarch64") else "x86_64"
        prefix = f"macos-{arch}-release"
    return tuple(f"{prefix}-{engine}" for engine in ("onnx", "tflite"))


def source_state() -> dict:
    names = subprocess.check_output(
        ["git", "ls-files", "--cached", "--others", "--exclude-standard", "-z"], cwd=ROOT
    ).split(b"\0")
    tree = hashlib.sha256()
    for name in sorted(set(names)):
        if not name:
            continue
        path = ROOT / os.fsdecode(name)
        tree.update(name + b"\0")
        tree.update(digest(path).encode() if path.is_file() else b"deleted")
    return {
        "source_commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], cwd=ROOT, text=True
        ).strip(),
        "source_dirty": bool(
            subprocess.check_output(["git", "status", "--porcelain"], cwd=ROOT, text=True).strip()
        ),
        "source_tree_sha256": tree.hexdigest(),
    }


def write_dependency_cache(destination: Path) -> None:
    """Write exact native prefixes without modifying another build's workspace."""
    target = {"darwin": "macos", "win32": "windows"}.get(sys.platform, sys.platform)
    lines = ['set(GEE_PYTHON_BUILD_TEST_REFERENCE ON CACHE BOOL "" FORCE)']
    for engine, preset in zip(("ORT", "TFLITE"), presets(target)):
        prefix = find_prebuilt_dependency(preset, False)
        if prefix is None:
            raise SystemExit(
                f"Missing exact dependency bundle for {preset}; run tools/ci prepare-deps {preset}"
            )
        lines.append(f'set(GEE_PREBUILT_{engine}_ROOT "{prefix.as_posix()}" CACHE PATH "" FORCE)')
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_text("\n".join(lines) + "\n")


def configure() -> None:
    """Called inside cibuildwheel's build interpreter before every wheel."""
    target = {"darwin": "macos", "win32": "windows"}.get(sys.platform, sys.platform)
    write_dependency_cache(WORKSPACE / f"dependencies-{target}.cmake")
    interpreters = WORKSPACE / "interpreters"
    interpreters.mkdir(exist_ok=True)
    (interpreters / f"{interpreter_key()}.txt").write_text(
        str(getattr(sys, "_base_executable", sys.executable))
    )
    if sys.platform == "win32":
        execute([sys.executable, "-m", "pip", "install", "delvewheel==1.11.2"])


def build() -> None:
    target = target_platform()
    for preset in presets(target):
        if find_prebuilt_dependency(preset, False) is None:
            if os.environ.get("CI"):
                raise SystemExit(f"CI prerequisite is missing: {preset}")
            prepare_dependency(preset, False)
    environment = os.environ.copy()
    environment.setdefault(
        "GEE_TOOLCHAIN_IDENTITY", dependency_toolchain_identity(presets(target)[0])
    )
    project = Path("/project") if target == "linux" else ROOT
    cmake_args = shlex.split(environment.get("CMAKE_ARGS", ""))
    cmake_args.extend(["-C", str(project / f"out/python/wheels/dependencies-{target}.cmake")])
    environment["CMAKE_ARGS"] = shlex.join(cmake_args)
    environment["CPM_SOURCE_CACHE"] = str(project / "out/cache/cpm")
    environment["CIBW_TEST_SKIP"] = (
        "*"  # The separate tools/test action tests these exact repaired bytes.
    )
    environment["CIBW_ENVIRONMENT_PASS_LINUX"] = (
        "CMAKE_ARGS CPM_SOURCE_CACHE GEE_TOOLCHAIN_IDENTITY"
    )
    if target == "linux":
        # cibuildwheel builds as root, while cached sources belong to the runner.
        # Keep Git's exception scoped to this container's environment.
        environment.update(
            GIT_CONFIG_COUNT="1", GIT_CONFIG_KEY_0="safe.directory", GIT_CONFIG_VALUE_0="*"
        )
        environment[
            "CIBW_ENVIRONMENT_PASS_LINUX"
        ] += " GIT_CONFIG_COUNT GIT_CONFIG_KEY_0 GIT_CONFIG_VALUE_0"
        environment["CIBW_MANYLINUX_X86_64_IMAGE"] = ensure_container_image("python")
    PRODUCTS.mkdir(parents=True, exist_ok=True)
    WORKSPACE.mkdir(parents=True, exist_ok=True)
    source = source_state()
    with tempfile.TemporaryDirectory(prefix="cibw-", dir=WORKSPACE) as temporary:
        project_directory = ROOT
        output_directory = Path(temporary) / "wheels"
        if target == "linux":
            # cibuildwheel copies its whole working directory into Docker. Feed
            # it the audited source archive, never the checkout's build caches.
            from sdist import audit_archive

            source_directory = Path(temporary) / "source"
            execute(
                [
                    sys.executable,
                    "-m",
                    "build",
                    "--sdist",
                    "--no-isolation",
                    "--outdir",
                    str(source_directory),
                ]
            )
            archive_path = next(source_directory.glob("*.tar.gz"))
            audit_archive(archive_path)
            with tarfile.open(archive_path) as archive:
                if any(member.issym() or member.islnk() for member in archive.getmembers()):
                    raise SystemExit("Source archive must not contain symlinks")
                if sys.version_info >= (3, 12):
                    archive.extractall(source_directory, filter="data")
                else:
                    archive.extractall(source_directory)
            project_directory = next(path for path in source_directory.iterdir() if path.is_dir())
            # Exact dependencies, generated consumers and compiler caches stay in
            # one explicit shared mount, outside the copied source inputs.
            mount = shlex.quote(f"{ROOT / 'out'}:/project/out")
            environment["CIBW_CONTAINER_ENGINE"] = f"docker; create_args: --volume {mount}"
        execute(
            [
                sys.executable,
                "-m",
                "cibuildwheel",
                "--platform",
                target,
                "--output-dir",
                str(output_directory),
            ],
            cwd=project_directory,
            environment=environment,
        )
        if source_state() != source:
            raise SystemExit("Sources changed during the wheel build; rebuild before testing")
        wheels = []
        for path in sorted(output_directory.glob("*.whl")):
            audit_wheel(path)
            destination = PRODUCTS / path.name
            shutil.copy2(path, destination)
            wheels.append(destination)
    if not wheels:
        raise SystemExit("cibuildwheel did not produce wheels")
    state = dict(
        platform=target,
        toolchain_identity=environment["GEE_TOOLCHAIN_IDENTITY"],
        artifacts={path.name: digest(path) for path in wheels},
        **source,
    )
    WORKSPACE.mkdir(parents=True, exist_ok=True)
    (WORKSPACE / "build.json").write_text(json.dumps(state, indent=2) + "\n")


def reference_root(root: Path = ROOT) -> Path:
    key = interpreter_key()
    marker = root / "out/python/references" / f"{key}.txt"
    if not marker.is_file():
        raise SystemExit(f"Missing C++ consumers for {key}; rebuild before testing")
    return root / marker.read_text().strip()


def audit_wheel(wheel: Path) -> None:
    with zipfile.ZipFile(wheel) as archive:
        names = archive.namelist()
        for module in ("_onnx", "_tflite", "_dsp"):
            if not any(
                name.startswith(f"giga_edge_enhance/{module}.") and name.endswith((".so", ".pyd"))
                for name in names
            ):
                raise SystemExit(f"Wheel is missing {module}: {wheel.name}")
        for required in ("py.typed", "__init__.pyi", "models.json"):
            if f"giga_edge_enhance/{required}" not in names:
                raise SystemExit(f"Wheel is missing {required}: {wheel.name}")
        if any(
            name.endswith((".onnx", ".tflite", ".enhance", ".cmake"))
            or "python-reference" in name
            or "sanitized" in name
            for name in names
        ):
            raise SystemExit("Wheel contains model files, build metadata or test executables")


def test_one(wheel: Path, *, references: Path | None = None) -> None:
    audit_wheel(wheel)
    checksum = digest(wheel)
    results = WORKSPACE / "results"
    results.mkdir(parents=True, exist_ok=True)
    result_path = results / f"{checksum}.json"
    result_path.unlink(missing_ok=True)
    environments = WORKSPACE / "test-envs"
    environments.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix=checksum[:16] + "-", dir=environments) as temporary:
        numpy_versions = test_environment(wheel, Path(temporary), references)
    result_path.write_text(
        json.dumps(
            {
                "sha256": checksum,
                "python": platform.python_version(),
                "numpy_versions": numpy_versions,
                "passed": True,
            }
        )
        + "\n"
    )


def test_environment(wheel: Path, directory: Path, references: Path | None) -> list[str]:
    checksum = digest(wheel)
    venv.create(directory, with_pip=True)
    python = str(environment_python(directory))
    execute(
        [
            python,
            "-m",
            "pip",
            "install",
            "-r",
            str(ROOT / "python/requirements-test.txt"),
            str(wheel),
            "--force-reinstall",
        ]
    )
    environment = os.environ.copy()
    for name in (
        "PYTHONPATH",
        "PYTHONHOME",
        "LD_LIBRARY_PATH",
        "DYLD_LIBRARY_PATH",
        "DYLD_FALLBACK_LIBRARY_PATH",
    ):
        environment.pop(name, None)
    environment["PYTHONNOUSERSITE"] = "1"
    environment["GEE_PYTHON_REFERENCE_ROOT"] = str(references or reference_root())
    with tempfile.TemporaryDirectory(prefix="enhance-installed-tests-") as temporary:
        execute(
            [
                python,
                "-m",
                "pytest",
                str(ROOT / "python/tests"),
                "-q",
                f"--junitxml={ROOT / 'out/python/logs' / (checksum[:16] + '.xml')}",
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
        numpy_versions = ["2.3.4" if sys.version_info[:2] >= (3, 14) else "2.2.6"]
        if sys.version_info[:2] == (3, 10):
            execute(
                [python, "-m", "pip", "install", "numpy==1.26.4", "--force-reinstall"],
                environment=environment,
            )
            execute(
                [python, "-m", "pytest", str(ROOT / "python/tests"), "-q"],
                cwd=Path(temporary),
                environment=environment,
            )
            numpy_versions.append("1.26.4")
        execute(
            [
                python,
                "-m",
                "mypy",
                "--strict",
                "--follow-imports=silent",
                "--cache-dir",
                str(WORKSPACE / "mypy"),
                str(ROOT / "python/tests/typing_consumer.py"),
            ],
            cwd=Path(temporary),
            environment=environment,
        )
    return numpy_versions


def test() -> None:
    state = json.loads((WORKSPACE / "build.json").read_text())
    for name, checksum in state["artifacts"].items():
        wheel = PRODUCTS / name
        if digest(wheel) != checksum:
            raise SystemExit("Wheel changed after its recorded build")
        abi = name.split("-")[-3]
        if state["platform"] == "linux":
            python = f"/opt/python/{abi}-{abi}/bin/python"
            run(
                [python, "python/scripts/wheels.py", "test-one", str(wheel.relative_to(ROOT))],
                container_stage="python",
                environment={"GEE_TOOLCHAIN_IDENTITY": state["toolchain_identity"]},
                additional_mounts=((ROOT, "/project"),),
            )
        else:
            marker = WORKSPACE / "interpreters" / f"{interpreter_key(abi)}.txt"
            executable = Path(marker.read_text()) if marker.is_file() else None
            if executable is None or not executable.is_file():
                if abi != f"cp{sys.version_info.major}{sys.version_info.minor}":
                    raise SystemExit(f"Missing test interpreter for {abi}; rebuild its wheel")
                executable = Path(sys.executable)
            execute([str(executable), "python/scripts/wheels.py", "test-one", str(wheel)])


def package() -> None:
    state = json.loads((WORKSPACE / "build.json").read_text())
    current = source_state()
    if current != {key: state[key] for key in current}:
        raise SystemExit("Source commit/state changed since the wheel build")
    if os.environ.get("CI") and {name.split("-")[-3] for name in state["artifacts"]} != {
        f"cp3{minor}" for minor in range(10, 15)
    }:
        raise SystemExit("CI products must cover CPython 3.10 through 3.14")
    for name, checksum in state["artifacts"].items():
        if digest(PRODUCTS / name) != checksum:
            raise SystemExit(f"Wheel changed after build: {name}")
        result = WORKSPACE / "results" / f"{checksum}.json"
        if not result.is_file() or not json.loads(result.read_text())["passed"]:
            raise SystemExit(f"Wheel has not passed installed-package tests: {name}")
    manifest = dict(format=1, version=(ROOT / "VERSION_NUMBER").read_text().strip(), **state)
    manifest["validation"] = {
        name: json.loads((WORKSPACE / "results" / f"{checksum}.json").read_text())
        for name, checksum in state["artifacts"].items()
    }
    source_archive = PRODUCTS / f"giga_edge_enhance-{manifest['version']}.tar.gz"
    if state["platform"] == "linux":
        verified = json.loads((WORKSPACE / "sdist-test.json").read_text())
        source_build = json.loads((WORKSPACE / "sdist-build.json").read_text())
        if (
            not verified["passed"]
            or digest(source_archive) != verified["sha256"]
            or any(source_build[key] != current[key] for key in current)
        ):
            raise SystemExit("Source archive changed after its wheel/consumer test")
        manifest["artifacts"][source_archive.name] = verified["sha256"]
    if RELEASE.exists():
        shutil.rmtree(RELEASE)
    RELEASE.mkdir(parents=True)
    for name in manifest["artifacts"]:
        shutil.copy2(PRODUCTS / name, RELEASE / name)
    (RELEASE / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    paths = [RELEASE / name for name in manifest["artifacts"]] + [RELEASE / "manifest.json"]
    (RELEASE / "SHA256SUMS").write_text("".join(f"{digest(path)}  {path.name}\n" for path in paths))


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=("build", "test", "package", "configure", "test-one"))
    parser.add_argument("wheel", nargs="?")
    args = parser.parse_args()
    if args.action == "test-one":
        if args.wheel is None:
            raise SystemExit("test-one requires a wheel path")
        test_one(Path(args.wheel).resolve())
    else:
        {"build": build, "test": test, "package": package, "configure": configure}[args.action]()


if __name__ == "__main__":
    main()
