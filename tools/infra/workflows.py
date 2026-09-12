"""CMake and platform workflow orchestration."""

import json
import os
from pathlib import Path
import platform
import shutil

from .config import (
    ANDROID_ABIS,
    ANDROID_DEPENDENCY_TARGETS,
    HOST_ONLY_PRESETS,
    NATIVE_PRESETS,
    PRESET_STAGES,
    ROOT,
)
from .dependencies import (
    find_prebuilt_dependency,
    prebuilt_cmake_arguments,
    prepare_android_tflite,
    prepare_dependency,
    write_artifact_checksums,
)
from .execution import compiler_cache_environment, print_compiler_cache_stats, run
from .identity import dependency_tag


def run_web_node(command: list[str], *, container_stage: str | None,
                 environment: dict[str, str], workdir: Path,
                 isolate_node_modules: bool = True) -> None:
    if container_stage is None:
        if not (Path.home() / ".nvm" / "nvm.sh").is_file():
            run(command, workdir=workdir)
            return
        nvm_script = 'source "$HOME/.nvm/nvm.sh"; nvm install; nvm use; exec "$@"'
    else:
        nvm_script = (
            'export NVM_DIR="$HOME/.nvm"; source /usr/local/lib/nvm/nvm.sh; '
            'nvm install; nvm use; exec "$@"'
        )

    run(["bash", "-lc", nvm_script, "bash", *command], container_stage=container_stage,
        environment=environment, workdir=workdir,
        isolate_web_node_modules=container_stage is not None and isolate_node_modules)


def cmake_workflow(action: str, preset: str, host: bool) -> None:
    use_host = host or preset in HOST_ONLY_PRESETS
    stage = None if use_host else PRESET_STAGES[preset]
    build_dir_host = (ROOT / "out" / "build" / preset if use_host else
                      ROOT / "out" / "build" / "container-clang-llvm-tools" / preset)
    build_dir = str(build_dir_host) if use_host else f"/workspace/{build_dir_host.relative_to(ROOT)}"
    environment = compiler_cache_environment(stage is not None)
    if not use_host:
        environment["SPENCH_BUILD_DIR"] = build_dir

    if action == "build":
        prebuilt_prefix = find_prebuilt_dependency(preset, host)
        configure_command = ["cmake", "--preset", preset]
        build_command = ["cmake", "--build", "--preset", preset]
        configure_command.extend(prebuilt_cmake_arguments(
            preset, host, prebuilt_prefix, stage is not None))
        if not use_host:
            configure_command.extend(("-B", build_dir))
            build_command = ["cmake", "--build", build_dir]
        if stage in {"native", "web"} and preset != "native-model-packer":
            configure_command.extend(("-DCMAKE_AR=/usr/bin/llvm-ar",
                                      "-DCMAKE_RANLIB=/usr/bin/llvm-ranlib"))
        run(configure_command, container_stage=stage, environment=environment)
        run(build_command, container_stage=stage, environment=environment)
        print_compiler_cache_stats(stage, environment)
        return

    if not build_dir_host.is_dir():
        raise SystemExit(
            f"Build output for {preset} does not exist at {build_dir_host.relative_to(ROOT)}. "
            f"Run 'tools/spench build {preset}{' --host' if host else ''}' first.")

    if action == "test":
        if preset in NATIVE_PRESETS:
            test_command = ["ctest", "--preset", preset]
            if not use_host:
                test_command = ["ctest", "--test-dir", build_dir, "--output-on-failure"]
            run(test_command, container_stage=stage, environment=environment)
        else:
            run(["bash", "platform_wrappers/web/packages/web-sdk/wasm/scripts/test.sh", preset],
                container_stage=stage, environment=environment)
    elif action == "package":
        if preset not in {"native-release-onnx", "native-release-tflite"}:
            raise SystemExit(f"Packaging is not supported for {preset}.")
        package_dir = ROOT / "out" / "artifacts" / preset
        package_dir.mkdir(parents=True, exist_ok=True)
        package_output_dir = (str(package_dir) if stage is None
                              else f"/workspace/out/artifacts/{preset}")
        package_command = ["cpack", "--preset", preset, "-G", "TGZ", "-B", package_output_dir]
        if not use_host:
            package_command = [
                "cpack", "--config", f"{build_dir}/CPackConfig.cmake", "-G", "TGZ",
                "-B", package_output_dir,
            ]
        run(package_command, container_stage=stage, environment=environment)
    elif action == "install":
        install_dir = ROOT / "out" / "artifacts" / "install" / preset
        if install_dir.exists():
            shutil.rmtree(install_dir)
        install_dir.mkdir(parents=True, exist_ok=True)
        prebuilt_prefix = find_prebuilt_dependency(preset, host)
        if prebuilt_prefix is not None:
            shutil.copytree(prebuilt_prefix, install_dir, dirs_exist_ok=True)
        configured_build_dir = build_dir_host if stage is None else Path(build_dir)
        install_output = (str(install_dir) if stage is None
                          else f"/workspace/{install_dir.relative_to(ROOT)}")
        install_command = ["cmake", "--install", str(configured_build_dir), "--prefix", install_output]
        if preset.startswith(("windows-", "ios-")):
            config = "Debug" if "debug" in preset or "asan" in preset else "Release"
            install_command.extend(("--config", config))
        run(install_command, container_stage=stage, environment=environment)
        manifest = {
            "format": 1,
            "preset": preset,
            "dependency_bundle": (dependency_tag(preset, host)
                                  if prebuilt_prefix is not None else None),
        }
        (install_dir / "manifest.json").write_text(
            json.dumps(manifest, indent=2, sort_keys=True) + "\n")
        write_artifact_checksums(install_dir)

        consumer_build_dir_host = ROOT / "out" / "build" / "package-consumer" / preset
        if consumer_build_dir_host.exists():
            shutil.rmtree(consumer_build_dir_host)
        consumer_source = ROOT / "spench" / "tests" / "package_consumer"
        consumer_build = (str(consumer_build_dir_host) if use_host else
                          f"/workspace/{consumer_build_dir_host.relative_to(ROOT)}")
        consumer_source_path = (str(consumer_source) if use_host else
                                f"/workspace/{consumer_source.relative_to(ROOT)}")
        target_is_windows = use_host and os.name == "nt"
        target_is_macos = use_host and platform.system() == "Darwin"
        consumer_configure = ["cmake", "-S", consumer_source_path, "-B", consumer_build,
                              f"-DCMAKE_PREFIX_PATH={install_output}"]
        consumer_build_command = ["cmake", "--build", consumer_build]
        executable_directory = consumer_build
        if target_is_windows:
            consumer_configure.extend(("-G", "Visual Studio 17 2022", "-A", "x64"))
            config = "Debug" if "debug" in preset or "asan" in preset else "Release"
            consumer_build_command.extend(("--config", config))
            executable_directory = f"{consumer_build}/{config}"
        else:
            consumer_configure.extend(("-G", "Ninja"))
        if target_is_macos:
            architecture = "x86_64" if "x86_64" in preset else "arm64"
            consumer_configure.append(f"-DCMAKE_OSX_ARCHITECTURES={architecture}")

        run(consumer_configure,
            container_stage=stage, environment=environment)
        run(consumer_build_command,
            container_stage=stage, environment=environment)
        consumer_environment = dict(environment)
        library_variable = ("PATH" if target_is_windows else
                            "DYLD_LIBRARY_PATH" if target_is_macos else "LD_LIBRARY_PATH")
        library_dir = f"{install_output}/{'bin' if target_is_windows else 'lib'}"
        consumer_environment[library_variable] = library_dir
        executable = f"{executable_directory}/spench_package_consumer"
        if target_is_windows:
            executable += ".exe"
        run([executable], container_stage=stage, environment=consumer_environment)


def android_workflow(action: str, host: bool) -> None:
    stage = None if host else "android"
    script = "test-all.sh" if action == "test" else "build.sh"
    environment = compiler_cache_environment(stage is not None)
    if artifact_version := os.environ.get("SPENCH_ARTIFACT_VERSION"):
        environment["SPENCH_ARTIFACT_VERSION"] = artifact_version
    dependency_root = "/workspace/out/artifacts/dependencies" if not host else str(
        ROOT / "out" / "artifacts" / "dependencies")
    environment["SPENCH_PREBUILT_DEPENDENCIES_ROOT"] = dependency_root
    if not host:
        environment["GRADLE_OPTS"] = (
            "-Dorg.gradle.vfs.watch=false -Dorg.gradle.workers.max=2 "
            "-Dorg.gradle.jvmargs=-Xmx3g"
        )
    run([f"platform_wrappers/android/scripts/{script}"], container_stage=stage,
        environment=environment)


def android_ui_workflow() -> None:
    environment = compiler_cache_environment(True)
    environment["SPENCH_PREBUILT_DEPENDENCIES_ROOT"] = "/workspace/out/artifacts/dependencies"
    environment["GRADLE_OPTS"] = (
        "-Dorg.gradle.vfs.watch=false -Dorg.gradle.workers.max=2 "
        "-Dorg.gradle.jvmargs=-Xmx3g"
    )
    run(["platform_wrappers/android/scripts/test-release-ui.sh"],
        container_stage="android-ui", environment=environment,
        devices=("/dev/kvm",))


def ios_workflow(action: str) -> None:
    scripts = {
        "build": "build_xcframework.sh",
        "test": "test.sh",
        "core": "build_core.sh",
    }
    device_root = find_prebuilt_dependency("ios-device-release", True)
    simulator_root = find_prebuilt_dependency("ios-simulator-release", True)
    environment = compiler_cache_environment(False)
    if device_root and simulator_root:
        environment.update({
            "SPENCH_PREBUILT_ORT_DEVICE_ROOT": str(device_root),
            "SPENCH_PREBUILT_ORT_SIMULATOR_ROOT": str(simulator_root),
        })
    run([f"platform_wrappers/ios/scripts/{scripts[action]}"], environment=environment)


def web_workflow(action: str, host: bool) -> None:
    web_dir = ROOT / "platform_wrappers/web"
    environment = compiler_cache_environment(not host)
    if site_base := os.environ.get("SPENCH_SITE_BASE"):
        environment["SPENCH_SITE_BASE"] = site_base

    if action == "test":
        required_outputs = (
            web_dir / "packages/web-sdk/dist",
            web_dir / "packages/web-sdk/generated/wasm",
        )
        missing = [path.relative_to(ROOT) for path in required_outputs if not path.exists()]
        if missing:
            raise SystemExit(
                "Web build output is missing: " + ", ".join(map(str, missing)) +
                ". Run 'tools/spench build web' first.")
        test_stage = None if host else "web"
        run_web_node(["npm", "ci"], container_stage=test_stage,
                     environment=environment, workdir=web_dir)
        run_web_node(["npm", "test", "--workspace=@gigaedge/spench-web-sdk"],
                     container_stage=test_stage, environment=environment, workdir=web_dir)
        return

    if action == "package":
        package_dir = ROOT / "out/artifacts/web"
        package_dir.mkdir(parents=True, exist_ok=True)
        stage = None if host else "web"
        run_web_node([
            "npm", "pack", "--workspace=@gigaedge/spench-web-sdk",
            "--pack-destination", "../../out/artifacts/web",
        ], container_stage=stage, environment=environment, workdir=web_dir)
        return

    if action == "playwright":
        installed_sdk = web_dir / "apps/spench-site/node_modules/@gigaedge/spench-web-sdk/package.json"
        if not installed_sdk.is_file():
            raise SystemExit(
                "The packed Web SDK is not installed. Install the downloaded package into "
                "@gigaedge/spench-site before running Playwright.")
        stage = None if host else "web"
        browser_platform = (f"{platform.system().lower()}-{platform.machine().lower()}"
                            if host else "linux-amd64")
        browser_cache_host = ROOT / "out/cache/playwright" / browser_platform
        browser_cache_host.mkdir(parents=True, exist_ok=True)
        browser_cache = (str(browser_cache_host) if host else
                         f"/workspace/{browser_cache_host.relative_to(ROOT)}")
        playwright_environment = {"PLAYWRIGHT_BROWSERS_PATH": browser_cache}
        run_web_node(["npx", "playwright", "install", "chromium", "firefox", "webkit"],
                     container_stage=stage, environment=playwright_environment,
                     workdir=web_dir, isolate_node_modules=False)
        run_web_node(["npm", "run", "build", "--workspace=@gigaedge/spench-site"],
                     container_stage=stage,
                     environment={**playwright_environment, "SPENCH_SITE_BASE": "/"},
                     workdir=web_dir, isolate_node_modules=False)
        run_web_node(["npm", "run", "pw:test", "--workspace=@gigaedge/spench-site"],
                     container_stage=stage,
                     environment={**playwright_environment, "PLAYWRIGHT_WORKERS": "1"},
                     workdir=web_dir, isolate_node_modules=False)
        return

    stage = None if host else "web"
    prebuilt_prefix = find_prebuilt_dependency("wasm-release", host)
    build_dir = None if host else "/workspace/out/build/container-clang-llvm-tools/wasm-release"
    if build_dir is not None:
        environment["SPENCH_BUILD_DIR"] = build_dir
    run_web_node(["npm", "ci"], container_stage=stage, environment=environment, workdir=web_dir)
    configure_command = ["cmake", "--preset", "wasm-release"]
    configure_command.extend(prebuilt_cmake_arguments(
        "wasm-release", host, prebuilt_prefix, stage is not None))
    build_command = ["cmake", "--build", "out/build/wasm-release"]
    if build_dir is not None:
        configure_command.extend((
            "-B", build_dir,
            "-DCMAKE_AR=/usr/bin/llvm-ar",
            "-DCMAKE_RANLIB=/usr/bin/llvm-ranlib",
        ))
        build_command = ["cmake", "--build", build_dir]

    run(configure_command, container_stage=stage, environment=environment)
    run(build_command, container_stage=stage, environment=environment)

    dsp_build_dir = None if host else "/workspace/out/build/container-clang-llvm-tools/wasm-dsp-release"
    dsp_environment = compiler_cache_environment(stage is not None)
    if dsp_build_dir is not None:
        dsp_environment["SPENCH_BUILD_DIR"] = dsp_build_dir
    dsp_configure_command = ["cmake", "--preset", "wasm-dsp-release"]
    dsp_build_command = ["cmake", "--build", "out/build/wasm-dsp-release"]
    if dsp_build_dir is not None:
        dsp_configure_command.extend((
            "-B", dsp_build_dir,
            "-DCMAKE_AR=/usr/bin/llvm-ar",
            "-DCMAKE_RANLIB=/usr/bin/llvm-ranlib",
        ))
        dsp_build_command = ["cmake", "--build", dsp_build_dir]

    run(dsp_configure_command, container_stage=stage, environment=dsp_environment)
    run(dsp_build_command, container_stage=stage, environment=dsp_environment)

    stage_environment = {
        "SPENCH_WASM_FULL_BUILD_DIR": build_dir or str(ROOT / "out/build/wasm-release"),
        "SPENCH_WASM_DSP_BUILD_DIR": dsp_build_dir or str(ROOT / "out/build/wasm-dsp-release"),
    }
    run_web_node(["npm", "run", "stage:wasm:release"], container_stage=stage,
                 environment=stage_environment, workdir=web_dir)
    run_web_node(["npm", "run", "build", "--workspace=@gigaedge/spench-web-sdk"],
                 container_stage=stage, environment=environment, workdir=web_dir)
    run_web_node(["npm", "run", "build", "--workspace=@gigaedge/spench-site"],
                 container_stage=stage, environment=environment, workdir=web_dir)


def prepare_platform_dependencies(target: str, host: bool) -> None:
    if target in ANDROID_DEPENDENCY_TARGETS:
        prepare_android_tflite(ANDROID_DEPENDENCY_TARGETS[target], "Release", host)
        return
    if target == "macos":
        for preset in (
                "macos-x86_64-debug-onnx", "macos-x86_64-release-onnx",
                "macos-x86_64-release-static-onnx",
                "macos-x86_64-debug-tflite", "macos-x86_64-release-tflite",
                "macos-arm64-debug-onnx", "macos-arm64-release-onnx",
                "macos-arm64-release-static-onnx",
                "macos-arm64-debug-tflite", "macos-arm64-release-tflite"):
            prepare_dependency(preset, True)
        return
    if target == "windows":
        for preset in ("windows-x64-debug-onnx", "windows-x64-release-onnx",
                       "windows-x64-debug-tflite", "windows-x64-release-tflite",
                       "windows-x64-release-static-tflite"):
            prepare_dependency(preset, True)
        return
    if target == "ios":
        for preset in ("ios-device-debug", "ios-device-release",
                       "ios-simulator-debug", "ios-simulator-release"):
            prepare_dependency(preset, True)
        return
    if target == "web":
        for preset in ("wasm-debug", "wasm-release"):
            prepare_dependency(preset, host)
        return
    if target == "android":
        for abi in ANDROID_ABIS:
            for build_type in ("Debug", "Release"):
                prepare_android_tflite(abi, build_type, host)
        return
    prepare_dependency(target, host)
