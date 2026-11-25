"""Build a source archive, then build/install/test its wheel from extracted sources."""

from __future__ import annotations

import argparse
import json
import os
from pathlib import Path, PurePosixPath
import platform
import shlex
import sys
import tarfile
import tempfile
import venv

from wheels import (
    PRODUCTS,
    ROOT,
    WORKSPACE,
    dependency_toolchain_identity,
    digest,
    environment_python,
    execute,
    reference_root,
    presets,
    run,
    source_state,
    target_platform,
    test_one,
    write_dependency_cache,
)


def archive_path() -> Path:
    return PRODUCTS / f"giga_edge_enhance-{(ROOT/'VERSION_NUMBER').read_text().strip()}.tar.gz"


def audit_archive(path: Path) -> None:
    with tarfile.open(path) as archive:
        names = archive.getnames()
        for name in names:
            parts = PurePosixPath(name).parts
            if (
                ".." in parts
                or name.startswith("/")
                or any(part in ("out", ".git", "__pycache__", ".venv") for part in parts)
            ):
                raise SystemExit(f"Unintended source archive entry: {name}")
            if name.endswith(
                (".onnx", ".tflite", ".enhance", ".pyc", ".dll", ".so", ".dylib", ".exe")
            ):
                raise SystemExit(f"Source archive contains a model/binary: {name}")
        for required in (
            "pyproject.toml",
            "python/CMakeLists.txt",
            "CMakeLists.txt",
            "VERSION_NUMBER",
            "package-lock.cmake",
            "core/CMakeLists.txt",
            "python/src/giga_edge_enhance/models.json",
        ):
            if not any(name.endswith("/" + required) for name in names):
                raise SystemExit(f"Source archive is missing {required}")


def build() -> None:
    PRODUCTS.mkdir(parents=True, exist_ok=True)
    execute([sys.executable, "-m", "build", "--sdist", "--no-isolation", "--outdir", str(PRODUCTS)])
    archive = archive_path()
    audit_archive(archive)
    WORKSPACE.mkdir(parents=True, exist_ok=True)
    (WORKSPACE / "sdist-build.json").write_text(
        json.dumps(dict(sha256=digest(archive), **source_state())) + "\n"
    )


def test_inside() -> None:
    archive = archive_path()
    audit_archive(archive)
    state = json.loads((WORKSPACE / "sdist-build.json").read_text())
    if state["sha256"] != digest(archive):
        raise SystemExit("Source archive changed since build")
    # A checkout may validate macOS and Linux archives in succession. Never
    # reuse a virtualenv or output directory from a different native interpreter.
    interpreter = f"{sys.platform}-{platform.machine().lower()}-{sys.implementation.cache_tag}"
    workspace = ROOT / "out/python/sdist" / interpreter
    workspace.mkdir(parents=True, exist_ok=True)
    directory = workspace / "build-env"
    if not environment_python(directory).is_file():
        venv.create(directory, with_pip=True)
    python = str(environment_python(directory))
    execute([python, "-m", "pip", "install", "-r", "python/requirements-dev.txt"])
    environment = os.environ.copy()
    environment.pop("PYTHONPATH", None)
    environment["PATH"] = str(Path(python).parent) + os.pathsep + environment.get("PATH", "")
    environment["CPM_SOURCE_CACHE"] = str(ROOT / "out/cache/cpm")
    extra = environment.get("GEE_PYTHON_SDIST_CMAKE_ARGS")
    if extra:
        arguments = shlex.split(extra) + ["-DGEE_PYTHON_BUILD_TEST_REFERENCE=ON"]
    else:
        cache = workspace / "dependencies.cmake"
        write_dependency_cache(cache)
        arguments = ["-C", str(cache)]
    environment["CMAKE_ARGS"] = shlex.join(arguments)
    with tempfile.TemporaryDirectory(prefix="source-", dir=workspace) as temporary:
        with tarfile.open(archive) as package:
            for member in package.getmembers():
                if member.issym() or member.islnk():
                    raise SystemExit("Source archive must not contain symlinks")
            if sys.version_info >= (3, 12):
                package.extractall(temporary, filter="data")
            else:
                package.extractall(temporary)
        source = next(Path(temporary).iterdir())
        output = workspace / "products"
        output.mkdir(exist_ok=True)
        execute(
            [python, "-m", "build", "--wheel", "--no-isolation", "--outdir", str(output)],
            cwd=source,
            environment=environment,
        )
        abi = f"cp{sys.version_info.major}{sys.version_info.minor}"
        candidates = list(output.glob(f"giga_edge_enhance-*-{abi}-*.whl"))
        if len(candidates) != 1:
            raise SystemExit("Expected exactly one source-built wheel for this interpreter")
        test_one(candidates[0], references=reference_root(source))
    (WORKSPACE / "sdist-test.json").write_text(
        json.dumps({"sha256": state["sha256"], "passed": True, "python": sys.version.split()[0]})
        + "\n"
    )


def test() -> None:
    # A failed rerun (including a container startup failure) must not leave an
    # earlier success available to the packaging gate for the same archive.
    (WORKSPACE / "sdist-test.json").unlink(missing_ok=True)
    if target_platform() == "linux":
        run(
            ["/opt/python/cp313-cp313/bin/python", "python/scripts/sdist.py", "test-inside"],
            container_stage="python",
            additional_mounts=((ROOT, "/project"),),
            environment={
                "GEE_TOOLCHAIN_IDENTITY": dependency_toolchain_identity(presets("linux")[0])
            },
        )
    else:
        test_inside()


def package() -> None:
    path = archive_path()
    result = json.loads((WORKSPACE / "sdist-test.json").read_text())
    if not result["passed"] or result["sha256"] != digest(path):
        raise SystemExit("Source archive has not passed its matching build/install tests")
    (PRODUCTS / "SDIST_SHA256SUMS").write_text(f"{digest(path)}  {path.name}\n")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=("build", "test", "test-inside", "package"))
    args = parser.parse_args()
    {"build": build, "test": test, "test-inside": test_inside, "package": package}[args.action]()


if __name__ == "__main__":
    main()
