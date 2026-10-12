"""Command-line interfaces for supported development and CI workflows."""

import argparse
from pathlib import Path

from .config import ANDROID_DEPENDENCY_TARGETS, HOST_ONLY_PRESETS, PRESET_STAGES
from .dependencies import restore_dependency_bundle
from .execution import compiler_cache_environment, run
from .identity import (
    container_image_digest,
    dependency_archive_for_target,
    dependency_tag,
    print_cache_keys,
    toolchain_recipe_digest,
)
from .workflows import (
    android_ui_workflow,
    android_workflow,
    cmake_workflow,
    ios_workflow,
    prepare_platform_dependencies,
    web_workflow,
)


CMAKE_PRESETS = (*PRESET_STAGES, *HOST_ONLY_PRESETS)
NATIVE_PRESETS = tuple(preset for preset in PRESET_STAGES if preset.startswith("native-"))
WASM_PRESETS = tuple(preset for preset in PRESET_STAGES if preset.startswith("wasm-"))
DESKTOP_PRESETS = tuple(
    preset for preset in HOST_ONLY_PRESETS if preset.startswith(("macos-", "windows-")))
IOS_PRESETS = tuple(preset for preset in HOST_ONLY_PRESETS if preset.startswith("ios-"))

BUILD_TARGETS = (*CMAKE_PRESETS, "android", "ios", "web")
TEST_TARGETS = (
    *(preset for preset in NATIVE_PRESETS if preset != "native-model-packer"),
    *WASM_PRESETS,
    *DESKTOP_PRESETS,
    "android", "android-ui", "ios", "web", "web-playwright",
)
PACKAGE_TARGETS = ("native-release-onnx", "native-release-tflite", "web")
INSTALL_TARGETS = (
    *(preset for preset in NATIVE_PRESETS if preset != "native-model-packer"),
    *DESKTOP_PRESETS,
)

DEPENDENCY_TARGETS = (
    *(preset for preset in NATIVE_PRESETS if preset != "native-model-packer"),
    *(preset for preset in WASM_PRESETS if not preset.startswith("wasm-dsp-")),
    *DESKTOP_PRESETS,
    *IOS_PRESETS,
    *ANDROID_DEPENDENCY_TARGETS,
)
DEPENDENCY_GROUPS = ("android", "web", "ios", "macos", "windows")
CACHE_TARGETS = (*CMAKE_PRESETS, *ANDROID_DEPENDENCY_TARGETS, "android", "ios")


def _workflow_parser(command: str, targets: tuple[str, ...]) -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=f"{command.capitalize()} a supported GigaEdgeSpench target.")
    parser.add_argument("target", choices=targets)
    parser.add_argument("--host", action="store_true",
                        help="Use local tools instead of Docker where supported.")
    return parser


def _validate_host(target: str, host: bool) -> None:
    if host and target not in {
            *PRESET_STAGES, *ANDROID_DEPENDENCY_TARGETS, "android", "web", "web-playwright"}:
        raise SystemExit(f"{target} already uses its required host toolchain; --host is not supported.")


def _run_workflow(action: str, target: str, host: bool) -> None:
    _validate_host(target, host)
    if target in CMAKE_PRESETS:
        cmake_workflow(action, target, host)
    elif target == "android":
        android_workflow(action, host)
    elif target == "android-ui":
        android_ui_workflow()
    elif target == "ios":
        ios_workflow(action)
    elif target == "web":
        web_workflow(action, host)
    elif target == "web-playwright":
        web_workflow("playwright", host)


def build_main(argv: list[str] | None = None) -> None:
    args = _workflow_parser("build", BUILD_TARGETS).parse_args(argv)
    _run_workflow("build", args.target, args.host)


def test_main(argv: list[str] | None = None) -> None:
    args = _workflow_parser("test", TEST_TARGETS).parse_args(argv)
    _run_workflow("test", args.target, args.host)


def package_main(argv: list[str] | None = None) -> None:
    args = _workflow_parser("package", PACKAGE_TARGETS).parse_args(argv)
    _run_workflow("package", args.target, args.host)


def install_main(argv: list[str] | None = None) -> None:
    args = _workflow_parser("install", INSTALL_TARGETS).parse_args(argv)
    _run_workflow("install", args.target, args.host)


def shell_main(argv: list[str] | None = None) -> None:
    parser = argparse.ArgumentParser(description="Open the native Linux development container.")
    parser.parse_args(argv)
    environment = compiler_cache_environment(True)
    environment["PS1"] = r"gigaedge-spench:\w\$ "
    run(["bash", "--noprofile", "--norc"], container_stage="native",
        environment=environment, interactive=True, mount_host_home=True)


def _add_ci_target(subparsers, action: str, targets: tuple[str, ...], *, artifact: bool = False,
                   host: bool = False) -> None:
    parser = subparsers.add_parser(action)
    parser.add_argument("target", choices=targets)
    if artifact:
        parser.add_argument("artifact")
    if host:
        parser.add_argument("--host", action="store_true")


def ci_main(argv: list[str] | None = None) -> None:
    parser = argparse.ArgumentParser(description="Run CI infrastructure operations.")
    subparsers = parser.add_subparsers(dest="action", required=True)
    _add_ci_target(subparsers, "image-key", ("native", "web", "android", "android-ui"))
    _add_ci_target(subparsers, "recipe-key", ("native", "web", "android"))
    _add_ci_target(subparsers, "cache-key", CACHE_TARGETS, host=True)
    _add_ci_target(subparsers, "prepare-deps", (*DEPENDENCY_TARGETS, *DEPENDENCY_GROUPS), host=True)
    _add_ci_target(subparsers, "dependency-key", DEPENDENCY_TARGETS, host=True)
    _add_ci_target(subparsers, "dependency-archive", DEPENDENCY_TARGETS, host=True)
    _add_ci_target(subparsers, "restore-deps", DEPENDENCY_TARGETS, artifact=True, host=True)
    args = parser.parse_args(argv)
    host = getattr(args, "host", False)

    if args.action == "image-key":
        print(container_image_digest(args.target))
    elif args.action == "recipe-key":
        print(toolchain_recipe_digest(args.target))
    elif args.action == "cache-key":
        _validate_host(args.target, host)
        print_cache_keys(args.target, host)
    elif args.action == "prepare-deps":
        _validate_host(args.target, host)
        prepare_platform_dependencies(args.target, host)
    elif args.action == "dependency-key":
        _validate_host(args.target, host)
        print(dependency_tag(args.target, host))
    elif args.action == "dependency-archive":
        _validate_host(args.target, host)
        archive = dependency_archive_for_target(args.target, host)
        if not archive.is_file() or not Path(f"{archive}.sha256").is_file():
            raise SystemExit(
                f"Dependency archive for {args.target} is missing. Run prepare-deps first.")
        print(archive)
    elif args.action == "restore-deps":
        _validate_host(args.target, host)
        restore_dependency_bundle(args.target, host, Path(args.artifact))
