"""Stable identities and paths for toolchains and dependency artifacts."""

import hashlib
import json
import os
from pathlib import Path
import platform
import re
import shutil
import subprocess

from .config import (
    ANDROID_DEPENDENCY_TARGETS,
    CACHE_INPUTS,
    DEPENDENCY_BUNDLE_FORMAT,
    DEPENDENCY_RECIPE_INPUTS,
    ENGINE_PACKAGE,
    ENGINE_PATCH,
    PRESET_STAGES,
    ROOT,
    TOOLCHAIN_DOCKERFILES,
    WEB_IMAGE_INPUTS,
    WEB_PACKAGE_LOCK,
)


def update_digest_with_file(digest, path: Path) -> None:
    """Add an unambiguous repository file entry to a content digest."""
    digest.update(path.relative_to(ROOT).as_posix().encode())
    digest.update(b"\0")
    digest.update(path.read_bytes())
    digest.update(b"\0")


def toolchain_recipe_digest(container_stage: str) -> str:
    """Return the stable compiler identity for one container toolchain."""
    try:
        dockerfile = TOOLCHAIN_DOCKERFILES[container_stage]
    except KeyError as error:
        raise ValueError(f"Unsupported container stage: {container_stage}") from error
    digest = hashlib.sha256()
    update_digest_with_file(digest, ROOT / "dockerfiles/Dockerfile.base")
    update_digest_with_file(digest, dockerfile)
    return digest.hexdigest()[:16]


def playwright_version() -> str:
    """Resolve the Playwright version baked into the Web image from the lockfile."""
    lock = json.loads(WEB_PACKAGE_LOCK.read_text())
    entry = lock.get("packages", {}).get("node_modules/playwright", {})
    version = entry.get("version")
    if not version:
        raise SystemExit("Could not resolve the Playwright version from package-lock.json.")
    return version


def container_image_digest(container_stage: str) -> str:
    """Return the content-derived tag for a toolchain image."""
    image_digest = hashlib.sha256()
    if container_stage == "android-ui":
        update_digest_with_file(image_digest, TOOLCHAIN_DOCKERFILES[container_stage])
        image_digest.update(b"android-toolchain=")
        image_digest.update(container_image_digest("android").encode())
        return image_digest.hexdigest()[:16]
    try:
        dockerfile = TOOLCHAIN_DOCKERFILES[container_stage]
    except KeyError as error:
        raise ValueError(f"Unsupported container stage: {container_stage}") from error
    update_digest_with_file(image_digest, ROOT / "dockerfiles/Dockerfile.base")
    update_digest_with_file(image_digest, dockerfile)
    if container_stage == "web":
        for image_input in WEB_IMAGE_INPUTS:
            update_digest_with_file(image_digest, image_input)
        image_digest.update(b"playwright=")
        image_digest.update(playwright_version().encode())
    return image_digest.hexdigest()[:16]


def container_image_name(container_stage: str) -> str:
    """Return the immutable local image name for a toolchain stage."""
    return f"ges-dev:{container_stage}-{container_image_digest(container_stage)}"


def dependency_cache_digest() -> str:
    digest = hashlib.sha256()
    inputs = [*CACHE_INPUTS, *sorted((ROOT / "cmake/patches").glob("*.patch"))]
    for cache_input in inputs:
        update_digest_with_file(digest, cache_input)
    return digest.hexdigest()[:16]


def engine_git_tag(engine: str) -> str:
    """Return the pinned upstream tag for an engine from package-lock.cmake."""
    try:
        package = ENGINE_PACKAGE[engine]
    except KeyError as error:
        raise ValueError(f"Unsupported dependency bundle engine: {engine}") from error
    lock_text = (ROOT / "package-lock.cmake").read_text()
    declaration = re.search(
        rf"CPMDeclarePackage\({re.escape(package)}\s.*?\n\)",
        lock_text,
        flags=re.DOTALL,
    )
    if declaration is None:
        raise SystemExit(f"Could not find the {engine} declaration in package-lock.cmake.")
    tag = re.search(r'GIT_TAG\s+"?([^"\s]+)"?', declaration.group(0))
    if tag is None:
        raise SystemExit(f"Could not find GIT_TAG for {engine} in package-lock.cmake.")
    return tag.group(1)


def dependency_bundle_digest(engine: str) -> str:
    """Return the identity of an engine's compiled binary bundle."""
    if engine not in ENGINE_PATCH:
        raise ValueError(f"Unsupported dependency bundle engine: {engine}")
    digest = hashlib.sha256()
    digest.update(f"format={DEPENDENCY_BUNDLE_FORMAT}".encode())
    digest.update(b"\0")
    digest.update(os.environ.get("GES_TOOLCHAIN_IDENTITY", "local-host").encode())
    digest.update(b"\0")
    digest.update(engine.encode())
    digest.update(b"\0")
    digest.update(engine_git_tag(engine).encode())
    digest.update(b"\0")
    digest.update(hashlib.sha256(ENGINE_PATCH[engine].read_bytes()).hexdigest().encode())
    digest.update(b"\0")
    for recipe in DEPENDENCY_RECIPE_INPUTS:
        update_digest_with_file(digest, recipe)
    return digest.hexdigest()[:16]


def cache_target(preset: str, host: bool) -> tuple[str, str]:
    if preset in ANDROID_DEPENDENCY_TARGETS:
        return "android", ANDROID_DEPENDENCY_TARGETS[preset]
    if preset.startswith("macos-x86_64-"):
        return "macos", "x86_64"
    if preset.startswith("macos-arm64-"):
        return "macos", "arm64"
    if preset.startswith("windows-x64-"):
        return "windows", "x86_64"
    if preset.startswith("wasm-"):
        return "emscripten", "wasm32"
    if preset.startswith("ios-device-"):
        return "ios", "arm64"
    if preset.startswith("ios-simulator-"):
        return "ios", "arm64-x86_64"
    if preset == "android":
        return "android", "multi"
    if preset == "ios":
        return "ios", "multi"
    if not host and preset in PRESET_STAGES:
        return "linux", "x86_64"

    system = platform.system().lower() or "unknown"
    machine = platform.machine().lower() or "unknown"
    architecture_aliases = {
        "amd64": "x86_64",
        "aarch64": "arm64",
    }
    return system, architecture_aliases.get(machine, machine)


def print_cache_keys(preset: str, host: bool) -> None:
    dependency_digest = dependency_cache_digest()
    target_system, target_arch = cache_target(preset, host)
    source_system = platform.system().lower() or "unknown"
    if source_system == "darwin":
        source_system = "macos"
    compiler_prefix = (
        f"ges-compiler-v1-{target_system}-{target_arch}-{preset}-{dependency_digest}"
    )
    print("source_cache_path=out/cache/cpm")
    print(f"source_cache_key=ges-sources-v2-{source_system}-{dependency_digest}")
    print(f"source_cache_restore_key=ges-sources-v2-{source_system}-")
    print("compiler_cache_path=out/cache/compiler")
    print(f"compiler_cache_key_prefix={compiler_prefix}")
    print(
        "compiler_cache_restore_key="
        f"ges-compiler-v1-{target_system}-{target_arch}-{preset}-"
    )


def engine_requirement(preset: str) -> tuple[str, str] | None:
    """Return the engine and dependency build type required by a preset."""
    if preset in ANDROID_DEPENDENCY_TARGETS:
        return "tflite", "Release"
    if preset in {"native-model-packer", "wasm-dsp-release", "wasm-dsp-debug"}:
        return None
    if "tflite" in preset:
        return "tflite", "Debug" if "debug" in preset else "Release"
    if "onnx" in preset or preset.startswith(("wasm-", "ios-")):
        return "onnx", "Debug" if "debug" in preset or "asan" in preset else "Release"
    return None


def bundle_linkage(preset: str) -> str:
    if preset.startswith(("wasm-", "ios-")):
        return "static"
    return "static" if "-static-" in preset else "shared"


def artifact_preset(preset: str) -> str:
    """ASAN consumes the matching non-instrumented Debug engine bundle."""
    return preset.replace("asan", "debug")


def dependency_tag(preset: str, host: bool) -> str:
    """Return a stable GHCR tag for one concrete dependency bundle."""
    requirement = engine_requirement(preset)
    if requirement is None:
        raise SystemExit(f"{preset} does not use a prebuilt engine dependency.")
    engine, build_type = requirement
    target_system, target_arch = cache_target(preset, host)
    parts = (
        engine,
        target_system,
        target_arch,
        build_type.lower(),
        bundle_linkage(preset),
        dependency_bundle_digest(engine),
    )
    return "-".join(parts).replace("_", "-").lower()


def dependency_artifact_root(preset: str, host: bool) -> Path | None:
    requirement = engine_requirement(preset)
    if requirement is None:
        return None
    engine, build_type = requirement
    target_system, target_arch = cache_target(preset, host)
    identity = f"v{DEPENDENCY_BUNDLE_FORMAT}-{dependency_bundle_digest(engine)}"
    return (ROOT / "out" / "artifacts" / "dependencies" / target_system /
            target_arch / engine / build_type.lower() / bundle_linkage(preset) / identity)


def dependency_artifact_for_target(target: str, host: bool) -> Path:
    if target in ANDROID_DEPENDENCY_TARGETS:
        abi = ANDROID_DEPENDENCY_TARGETS[target]
        identity = f"v{DEPENDENCY_BUNDLE_FORMAT}-{dependency_bundle_digest('tflite')}"
        return (ROOT / "out" / "artifacts" / "dependencies" / "android" / abi /
                "tflite" / "release" / "shared" / identity)
    artifact_root = dependency_artifact_root(target, host)
    if artifact_root is None:
        raise SystemExit(f"{target} does not produce a dependency bundle.")
    return artifact_root


def dependency_archive_for_target(target: str, host: bool) -> Path:
    return Path(f"{dependency_artifact_for_target(target, host)}.tar.gz")


def executable_version(command: str) -> str:
    executable = shutil.which(command)
    if executable is None:
        return "not-found"
    result = subprocess.run([executable, "--version"], text=True, capture_output=True,
                            check=False)
    return result.stdout.splitlines()[0] if result.stdout else executable


def dependency_manifest(preset: str, host: bool, prefix: Path) -> dict[str, object]:
    engine, build_type = engine_requirement(preset) or ("none", "None")
    target_system, target_arch = cache_target(preset, host)
    return {
        "format": DEPENDENCY_BUNDLE_FORMAT,
        "engine": engine,
        "build_type": build_type,
        "linkage": bundle_linkage(preset),
        "preset": artifact_preset(preset),
        "target_system": target_system,
        "target_architecture": target_arch,
        "dependency_digest": dependency_bundle_digest(engine),
        "toolchain_identity": os.environ.get("GES_TOOLCHAIN_IDENTITY", "local-host"),
        "prefix": str(prefix.relative_to(ROOT)),
    }
