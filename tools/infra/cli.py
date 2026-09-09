"""Command-line interface for supported Spench development workflows."""

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


ACTIONS = (
    "build", "test", "package", "install", "core", "shell", "cache-key",
    "prepare-deps", "image-key", "recipe-key", "dependency-key",
    "dependency-archive", "restore-deps",
)
TARGETS = (
    *PRESET_STAGES,
    *HOST_ONLY_PRESETS,
    *ANDROID_DEPENDENCY_TARGETS,
    "android",
    "android-ui",
    "ios",
    "web",
    "web-playwright",
    "macos",
    "windows",
    "native",
)


def create_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Run supported local and containerized Spench development workflows.")
    parser.add_argument("action", choices=ACTIONS)
    parser.add_argument("target", choices=TARGETS,
                        help="CMake preset or platform workflow")
    parser.add_argument("artifact", nargs="?", help="Archive path used by restore-deps.")
    parser.add_argument("--host", action="store_true",
                        help="Use local tools instead of Docker for Native, Web, or Android.")
    return parser


def main(argv: list[str] | None = None) -> None:
    args = create_parser().parse_args(argv)

    if args.action == "shell":
        if args.target != "native":
            raise SystemExit("shell expects the native target.")
        if args.host:
            raise SystemExit("shell always uses the native Linux container; --host is not supported.")
        if args.artifact:
            raise SystemExit("shell does not accept an artifact argument.")
        shell_environment = compiler_cache_environment(True)
        shell_environment["PS1"] = r"spench:\w\$ "
        run(["bash", "--noprofile", "--norc"], container_stage="native",
            environment=shell_environment, interactive=True, mount_host_home=True)
        return

    if args.action == "image-key":
        if args.target not in {"native", "web", "android", "android-ui"}:
            raise SystemExit("image-key expects native, web, android, or android-ui.")
        print(container_image_digest(args.target))
        return
    if args.action == "recipe-key":
        if args.target not in {"native", "web", "android"}:
            raise SystemExit("recipe-key expects native, web, or android.")
        print(toolchain_recipe_digest(args.target))
        return
    if args.action == "dependency-key":
        print(dependency_tag(args.target, args.host))
        return
    if args.action == "dependency-archive":
        archive = dependency_archive_for_target(args.target, args.host)
        if not archive.is_file() or not Path(f"{archive}.sha256").is_file():
            raise SystemExit(
                f"Dependency archive for {args.target} is missing. Run prepare-deps first.")
        print(archive)
        return
    if args.action == "restore-deps":
        if not args.artifact:
            raise SystemExit("restore-deps requires an archive path.")
        restore_dependency_bundle(args.target, args.host, Path(args.artifact))
        return
    if args.action == "cache-key":
        print_cache_keys(args.target, args.host)
        return
    if args.action == "prepare-deps":
        if args.target in {"native", "web"}:
            raise SystemExit("prepare-deps expects a preset or platform workflow, not a Docker stage.")
        prepare_platform_dependencies(args.target, args.host)
        return

    if args.target in PRESET_STAGES or args.target in HOST_ONLY_PRESETS:
        if args.action == "core":
            raise SystemExit("The core action is only valid for the iOS workflow.")
        cmake_workflow(args.action, args.target, args.host)
    elif args.target == "android":
        if args.action not in {"build", "test"}:
            raise SystemExit("Android supports build and test.")
        android_workflow(args.action, args.host)
    elif args.target == "android-ui":
        if args.action != "test":
            raise SystemExit("Android UI supports test only.")
        android_ui_workflow()
    elif args.target == "ios":
        if args.action not in {"build", "test", "core"}:
            raise SystemExit("iOS supports build, core, and test.")
        ios_workflow(args.action)
    elif args.target == "web":
        if args.action not in {"build", "test", "package"}:
            raise SystemExit("The web SDK supports build, test, and package.")
        web_workflow(args.action, args.host)
    elif args.target == "web-playwright":
        if args.action != "test":
            raise SystemExit("The Web Playwright target supports test only.")
        web_workflow("playwright", args.host)
    else:
        raise SystemExit(f"{args.target} is only valid with a metadata or prepare-deps action.")
