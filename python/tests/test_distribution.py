import ast
import importlib.util
import io
import json
from pathlib import Path
import sys
import tarfile
import zipfile

import pytest

import giga_edge_enhance


@pytest.fixture
def distribution_tools(root, monkeypatch):
    monkeypatch.syspath_prepend(str(root / "python/scripts"))
    specification = importlib.util.spec_from_file_location(
        "enhance_wheel_tools", root / "python/scripts/wheels.py"
    )
    module = importlib.util.module_from_spec(specification)
    specification.loader.exec_module(module)
    return module


def test_host_wheel_uses_its_own_interpreter(distribution_tools, monkeypatch, tmp_path):
    tools = distribution_tools
    monkeypatch.setattr(tools, "WORKSPACE", tmp_path)
    monkeypatch.setattr(tools, "PRODUCTS", tmp_path)
    abi = "cp310"
    wheel = tmp_path / f"giga_edge_enhance-2.0.0-{abi}-{abi}-win_amd64.whl"
    wheel.write_bytes(b"wheel fixture")
    (tmp_path / "build.json").write_text(
        json.dumps({"platform": "windows", "artifacts": {wheel.name: tools.digest(wheel)}})
    )
    monkeypatch.setattr(sys, "platform", "win32")
    monkeypatch.setattr(tools.platform, "machine", lambda: "AMD64")
    markers = tmp_path / "interpreters"
    markers.mkdir()
    executable = tmp_path / "python310.exe"
    executable.touch()
    (markers / "cp310-win32-arm64.txt").write_text("unrelated.exe")
    (markers / "cp310-win32-amd64.txt").write_text(str(executable))
    commands = []
    monkeypatch.setattr(tools, "execute", commands.append)

    tools.test()

    assert commands == [[str(executable), "python/scripts/wheels.py", "test-one", str(wheel)]]


def test_repaired_wheel_audit_rejects_development_files(distribution_tools, tmp_path):
    for extra in (
        None,
        "giga_edge_enhance/model.onnx",
        "giga_edge_enhance/model.enhance",
        "python-reference/tool",
        "giga_edge_enhance/lib/cmake/onnxruntimeConfig.cmake",
    ):
        wheel = tmp_path / "test.whl"
        with zipfile.ZipFile(wheel, "w") as archive:
            for module in ("_onnx", "_tflite", "_dsp"):
                archive.writestr(f"giga_edge_enhance/{module}.cpython.so", b"test-only-fixture")
            for filename in ("py.typed", "__init__.pyi", "models.json"):
                archive.writestr(f"giga_edge_enhance/{filename}", "")
            if extra:
                archive.writestr(extra, "")
        if extra is None:
            distribution_tools.audit_wheel(wheel)
        else:
            with pytest.raises(SystemExit, match="model files, build metadata or test executables"):
                distribution_tools.audit_wheel(wheel)


def test_missing_backend_cannot_be_promoted(distribution_tools, tmp_path):
    wheel = tmp_path / "missing.whl"
    with zipfile.ZipFile(wheel, "w") as archive:
        archive.writestr("giga_edge_enhance/py.typed", "")
    with pytest.raises(SystemExit, match="missing _onnx"):
        distribution_tools.audit_wheel(wheel)


def test_linux_cibuildwheel_copies_clean_sources_and_mounts_build_outputs(
    distribution_tools, monkeypatch, tmp_path
):
    tools = distribution_tools
    monkeypatch.setattr(tools, "ROOT", tmp_path)
    monkeypatch.setattr(tools, "PRODUCTS", tmp_path / "out/products")
    monkeypatch.setattr(tools, "WORKSPACE", tmp_path / "out/state")
    monkeypatch.setattr(tools, "target_platform", lambda: "linux")
    monkeypatch.setattr(tools, "find_prebuilt_dependency", lambda *args: tmp_path / "out/deps")
    monkeypatch.setattr(tools, "ensure_container_image", lambda stage: "test-manylinux")
    monkeypatch.setattr(tools, "dependency_toolchain_identity", lambda preset: "test-toolchain")
    monkeypatch.setattr(tools, "source_state", lambda: {"source_tree_sha256": "unchanged"})
    (tmp_path / "out").mkdir()
    sentinel = tmp_path / "out/private-build-output"
    sentinel.write_text("must not be copied into the project")
    built = []

    def execute(command, *, cwd=None, environment=None):
        output = Path(command[command.index("--outdir") + 1]) if "--outdir" in command else None
        if output is not None:
            output.mkdir(parents=True)
            with tarfile.open(output / "source.tar.gz", "w:gz") as archive:
                for filename in (
                    "pyproject.toml",
                    "python/CMakeLists.txt",
                    "CMakeLists.txt",
                    "VERSION_NUMBER",
                    "package-lock.cmake",
                    "core/CMakeLists.txt",
                    "python/src/giga_edge_enhance/models.json",
                ):
                    data = b"source fixture"
                    member = tarfile.TarInfo("source/" + filename)
                    member.size = len(data)
                    archive.addfile(member, io.BytesIO(data))
            return
        assert "cibuildwheel" in command
        assert cwd != tmp_path and (cwd / "pyproject.toml").is_file()
        assert not (cwd / "out").exists()
        assert f"{tmp_path / 'out'}:/project/out" in environment["CIBW_CONTAINER_ENGINE"]
        output = Path(command[command.index("--output-dir") + 1])
        output.mkdir()
        wheel = output / "test.whl"
        with zipfile.ZipFile(wheel, "w") as archive:
            for module in ("_onnx", "_tflite", "_dsp"):
                archive.writestr(f"giga_edge_enhance/{module}.cpython.so", b"fixture")
            for name in ("py.typed", "__init__.pyi", "models.json"):
                archive.writestr("giga_edge_enhance/" + name, "")
        built.append(wheel.name)

    monkeypatch.setattr(tools, "execute", execute)
    tools.build()
    assert built == ["test.whl"]
    assert sentinel.read_text() == "must not be copied into the project"
    assert (tools.PRODUCTS / "test.whl").is_file()


@pytest.mark.parametrize("changed", [None, "source", "wheel", "test", "missing-test"])
def test_release_package_requires_matching_sources_bytes_and_tests(
    distribution_tools, monkeypatch, tmp_path, changed
):
    tools = distribution_tools
    monkeypatch.delenv("CI", raising=False)
    monkeypatch.setattr(tools, "ROOT", tmp_path)
    monkeypatch.setattr(tools, "PRODUCTS", tmp_path / "build")
    monkeypatch.setattr(tools, "RELEASE", tmp_path / "release")
    monkeypatch.setattr(tools, "WORKSPACE", tmp_path / "state")
    tools.PRODUCTS.mkdir()
    (tools.WORKSPACE / "results").mkdir(parents=True)
    (tmp_path / "VERSION_NUMBER").write_text("2.0.0\n")
    source = dict(source_commit="abc", source_dirty=False, source_tree_sha256="tree")
    monkeypatch.setattr(tools, "source_state", lambda: source.copy())
    wheel = tools.PRODUCTS / "giga_edge_enhance-2.0.0-cp313-cp313-macosx_13_0_arm64.whl"
    wheel.write_bytes(b"tested fixture bytes")
    checksum = tools.digest(wheel)
    (tools.PRODUCTS / "unverified.whl").write_bytes(b"must not be promoted")
    state = dict(platform="macos", artifacts={wheel.name: checksum}, **source)
    (tools.WORKSPACE / "build.json").write_text(json.dumps(state))
    if changed != "missing-test":
        (tools.WORKSPACE / "results" / f"{checksum}.json").write_text(
            json.dumps(dict(sha256=checksum, passed=changed != "test"))
        )
    if changed == "source":
        source["source_tree_sha256"] = "changed"
    elif changed == "wheel":
        wheel.write_bytes(b"changed bytes")
    if changed:
        with pytest.raises(SystemExit):
            tools.package()
        assert not tools.RELEASE.exists()
    else:
        tools.package()
        assert {path.name for path in tools.RELEASE.iterdir()} == {
            wheel.name,
            "manifest.json",
            "SHA256SUMS",
        }
        assert (tools.RELEASE / wheel.name).read_bytes() == wheel.read_bytes()
        assert checksum in (tools.RELEASE / "SHA256SUMS").read_text()


def test_every_public_export_has_a_stub():
    package = Path(giga_edge_enhance.__file__).parent
    tree = ast.parse((package / "__init__.pyi").read_text())
    declared = {
        alias.asname or alias.name
        for node in tree.body
        if isinstance(node, ast.ImportFrom)
        for alias in node.names
    }
    assert set(giga_edge_enhance.__all__) <= declared


def test_source_archive_audit_rejects_caches_and_weights(root, monkeypatch, tmp_path):
    monkeypatch.syspath_prepend(str(root / "python/scripts"))
    specification = importlib.util.spec_from_file_location(
        "enhance_source_tools", root / "python/scripts/sdist.py"
    )
    module = importlib.util.module_from_spec(specification)
    specification.loader.exec_module(module)
    for forbidden in ("out/cache/private.dat", "models/model.tflite", "../outside"):
        path = tmp_path / "invalid.tar.gz"
        with tarfile.open(path, "w:gz") as archive:
            data = b"test fixture"
            info = tarfile.TarInfo("sdk/" + forbidden)
            info.size = len(data)
            archive.addfile(info, io.BytesIO(data))
        with pytest.raises(SystemExit):
            module.audit_archive(path)


def test_failed_source_archive_test_invalidates_previous_success(
    distribution_tools, monkeypatch, tmp_path
):
    import sdist

    monkeypatch.setattr(sdist, "WORKSPACE", tmp_path)
    monkeypatch.setattr(sdist, "target_platform", lambda: "linux")
    result = tmp_path / "sdist-test.json"
    result.write_text(json.dumps({"passed": True, "sha256": "previous-success"}))

    def unavailable(*args, **kwargs):
        raise RuntimeError("container startup failed")

    monkeypatch.setattr(sdist, "run", unavailable)
    with pytest.raises(RuntimeError, match="container startup"):
        sdist.test()
    assert not result.exists()
