"""Dependency bundle construction, validation, and restoration."""

import contextlib
import hashlib
import json
import os
from pathlib import Path
import shutil
import tarfile

from .config import (
    ANDROID_NDK_VERSION,
    DEPENDENCY_BUNDLE_FORMAT,
    HOST_ONLY_PRESETS,
    PRESET_STAGES,
    ROOT,
)
from .execution import compiler_cache_environment, run
from .identity import (
    artifact_preset,
    bundle_linkage,
    cache_target,
    dependency_artifact_for_target,
    dependency_artifact_root,
    dependency_bundle_digest,
    dependency_manifest,
    engine_requirement,
)


def copy_onnx_headers(prefix: Path) -> None:
    source_roots = sorted((ROOT / "out" / "cache" / "cpm" / "onnxruntime").glob("*/include"))
    if not source_roots:
        raise SystemExit("ONNX Runtime source headers were not found in out/cache/cpm after CPM preparation.")
    destination = prefix / "include" / "onnxruntime"
    shutil.copytree(source_roots[-1] / "onnxruntime", destination, dirs_exist_ok=True)


def copy_header_tree(source: Path, destination: Path) -> None:
    """Copy compiler-consumed headers without packaging dependency sources."""
    header_suffixes = {".h", ".hh", ".hpp", ".inc"}
    copied = 0
    for path in source.rglob("*"):
        if not path.is_file() or path.suffix not in header_suffixes:
            continue
        output = destination / path.relative_to(source)
        output.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(path, output)
        copied += 1
    if copied == 0:
        raise SystemExit(f"No public headers were found below {source}.")
    print(f"Copied {copied} public headers from {source}.")


def copy_tflite_headers(prefix: Path, build_dir: Path | None = None) -> None:
    """Copy TFLite's public headers and its header-only public dependencies."""
    source_roots = []
    if build_dir is not None:
        source_roots.extend((build_dir / "_deps").glob("tflite-src/tensorflow"))
    source_roots.extend((ROOT / "out" / "cache" / "cpm" / "tflite").glob("*/tensorflow"))
    source_roots = sorted(set(source_roots))
    if not source_roots:
        raise SystemExit("TensorFlow Lite headers were not found after CPM preparation.")
    copy_header_tree(source_roots[-1] / "lite", prefix / "include" / "tensorflow" / "lite")
    if build_dir is None:
        return

    header_dependencies = {
        "flatbuffers": ("flatbuffers.h", 0),
        "absl": ("config.h", 1),
    }
    for directory, (header, parents) in header_dependencies.items():
        candidates = sorted(build_dir.rglob(f"{directory}/**/{header}"))
        if not candidates:
            raise SystemExit(
                f"TensorFlow Lite preparation did not produce public {directory} headers below {build_dir}.")
        source = candidates[0]
        for _ in range(parents):
            source = source.parent
        source = source.parent
        copy_header_tree(source, prefix / "include" / directory)


def copy_static_archives(prefix: Path, build_dir: Path) -> list[Path]:
    """Copy archives without merging them, so duplicate object names remain distinct."""
    archive_root = prefix / "lib" / "archives"
    archives = sorted({archive for pattern in ("*.a", "*.lib")
                       for archive in build_dir.rglob(pattern)})
    if not archives:
        raise SystemExit(f"No static archives were produced below {build_dir}.")

    copied = []
    for archive in archives:
        destination = archive_root / archive.relative_to(build_dir)
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(archive, destination)
        copied.append(destination)
    return copied


def copy_engine_library(prefix: Path, engine: str, build_dir: Path) -> Path:
    """Copy a shared engine library, loader aliases, and Windows import library."""
    library_dir = prefix / "lib"
    library_dir.mkdir(parents=True, exist_ok=True)
    stem = "onnxruntime" if engine == "onnx" else "tensorflow-lite"
    candidates = sorted({
        *build_dir.rglob(f"lib{stem}*.dylib"),
        *build_dir.rglob(f"lib{stem}*.so"),
        *build_dir.rglob(f"lib{stem}*.so.*"),
        *build_dir.rglob(f"{stem}*.dll"),
    })
    if not candidates:
        raise SystemExit(f"No shared {engine} library was produced below {build_dir}.")
    for source in candidates:
        shutil.copy2(source, library_dir / source.name)
    if any(candidate.suffix == ".dll" for candidate in candidates):
        import_libraries = sorted(build_dir.rglob(f"{stem}*.lib"))
        if not import_libraries:
            raise SystemExit(f"No import library was produced for shared {engine} below {build_dir}.")
        for source in import_libraries:
            shutil.copy2(source, library_dir / source.name)
    return library_dir / candidates[0].name


def write_engine_package_config(prefix: Path, engine: str, library: Path | None, linkage: str,
                                target_name: str | None = None,
                                static_archives: list[Path] | None = None) -> None:
    """Write a relocatable config independent of upstream engine dependencies."""
    package = "onnxruntime" if engine == "onnx" else "tensorflow-lite"
    target = target_name or ("onnxruntime::onnxruntime" if engine == "onnx"
                             else "tensorflow-lite::tensorflow-lite")
    config_dir = prefix / "lib" / "cmake" / package
    config_dir.mkdir(parents=True, exist_ok=True)
    prefix_variable = "_spench_ort_prefix" if engine == "onnx" else "_spench_tflite_prefix"
    if linkage == "static":
        if not static_archives:
            raise ValueError("Static engine package configuration requires copied archives.")
        archive_paths = [archive.relative_to(prefix).as_posix() for archive in static_archives]
        archive_entries = "\n".join(
            f"  \"${{{prefix_variable}}}/{archive}\"" for archive in archive_paths)
        config = (
            "include(CMakeFindDependencyMacro)\n"
            f"get_filename_component({prefix_variable} \"${{CMAKE_CURRENT_LIST_DIR}}/../../..\" ABSOLUTE)\n"
            "find_dependency(Threads)\n"
            f"set(_spench_engine_archives\n{archive_entries}\n)\n"
            "if(CMAKE_SYSTEM_NAME STREQUAL \"Linux\")\n"
            "  string(JOIN \",\" _spench_engine_archive_group ${_spench_engine_archives})\n"
            "  set(_spench_engine_link_libraries \"$<LINK_GROUP:RESCAN,${_spench_engine_archive_group}>\")\n"
            "else()\n"
            "  set(_spench_engine_link_libraries \"${_spench_engine_archives}\")\n"
            "endif()\n"
            f"if(NOT TARGET {target})\n"
            f"  add_library({target} INTERFACE IMPORTED GLOBAL)\n"
            f"  set_target_properties({target} PROPERTIES\n"
            f"    INTERFACE_INCLUDE_DIRECTORIES \"${{{prefix_variable}}}/include\"\n"
            "    INTERFACE_LINK_LIBRARIES \"${_spench_engine_link_libraries};Threads::Threads;${CMAKE_DL_LIBS};m\"\n"
            "  )\n"
            "endif()\n")
    else:
        if library is None:
            raise ValueError("Shared engine package configuration requires its shared library.")
        import_library = prefix / "lib" / f"{library.stem}.lib"
        imported_implib = (
            f"    IMPORTED_IMPLIB \"${{{prefix_variable}}}/lib/{import_library.name}\"\n"
            if library.suffix == ".dll" and import_library.is_file() else "")
        config = (
            "include(CMakeFindDependencyMacro)\n"
            f"get_filename_component({prefix_variable} \"${{CMAKE_CURRENT_LIST_DIR}}/../../..\" ABSOLUTE)\n"
            f"if(NOT TARGET {target})\n"
            f"  add_library({target} SHARED IMPORTED GLOBAL)\n"
            f"  set_target_properties({target} PROPERTIES\n"
            f"    IMPORTED_LOCATION \"${{{prefix_variable}}}/lib/{library.name}\"\n"
            f"{imported_implib}"
            f"    INTERFACE_INCLUDE_DIRECTORIES \"${{{prefix_variable}}}/include\"\n"
            "  )\n"
            "endif()\n")
    (config_dir / f"{package}Config.cmake").write_text(config)


@contextlib.contextmanager
def artifact_lock(artifact_root: Path):
    """Prevent two invocations from configuring the same nested engine sources."""
    lock_path = artifact_root.with_name(f"{artifact_root.name}.lock")
    lock_path.parent.mkdir(parents=True, exist_ok=True)
    try:
        descriptor = os.open(lock_path, os.O_CREAT | os.O_EXCL | os.O_WRONLY)
    except FileExistsError as error:
        raise SystemExit(
            f"Dependency preparation is already running for {artifact_root.relative_to(ROOT)}. "
            f"Wait for it to finish before retrying.") from error
    try:
        os.write(descriptor, f"pid={os.getpid()}\n".encode())
        yield
    finally:
        os.close(descriptor)
        lock_path.unlink(missing_ok=True)


def file_sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def write_artifact_checksums(artifact_root: Path) -> None:
    """Write deterministic SHA-256 entries for every deliberate artifact file."""
    checksum_path = artifact_root / "checksums.sha256"
    files = sorted(path for path in artifact_root.rglob("*")
                   if path.is_file() and path != checksum_path)
    checksum_path.write_text("".join(
        f"{file_sha256(path)}  {path.relative_to(artifact_root).as_posix()}\n"
        for path in files))


def write_dependency_bundle(artifact_root: Path, manifest: dict[str, object]) -> None:
    artifact_root.mkdir(parents=True, exist_ok=True)
    (artifact_root / "manifest.json").write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n")
    archive = Path(shutil.make_archive(str(artifact_root), "gztar", root_dir=artifact_root))
    archive_digest = file_sha256(archive)
    archive.with_suffix(archive.suffix + ".sha256").write_text(
        f"{archive_digest}  {archive.name}\n")


def restore_dependency_bundle(target: str, host: bool, archive: Path) -> None:
    """Verify and restore a dependency archive pulled from the OCI registry."""
    archive = archive.resolve()
    checksum_path = Path(f"{archive}.sha256")
    if not archive.is_file() or not checksum_path.is_file():
        raise SystemExit(f"Dependency archive or checksum is missing: {archive}")
    expected_checksum = checksum_path.read_text().split()[0]
    actual_checksum = file_sha256(archive)
    if actual_checksum != expected_checksum:
        raise SystemExit(
            f"Dependency checksum mismatch for {archive.name}: "
            f"expected {expected_checksum}, got {actual_checksum}.")

    artifact_root = dependency_artifact_for_target(target, host)
    artifact_root.mkdir(parents=True, exist_ok=True)
    root = artifact_root.resolve()
    with tarfile.open(archive, "r:gz") as bundle:
        members = bundle.getmembers()
        for member in members:
            destination = (root / member.name).resolve()
            if destination != root and root not in destination.parents:
                raise SystemExit(f"Unsafe path in dependency archive: {member.name}")
            if member.issym() or member.islnk():
                raise SystemExit(f"Links are not allowed in dependency archive: {member.name}")
        bundle.extractall(root, members=members)

    manifest_path = artifact_root / "manifest.json"
    prefix = artifact_root / "prefix"
    if not manifest_path.is_file() or not prefix.is_dir():
        raise SystemExit(f"Restored dependency bundle is incomplete: {artifact_root}")
    manifest = json.loads(manifest_path.read_text())
    engine = str(manifest.get("engine"))
    if manifest.get("dependency_digest") != dependency_bundle_digest(engine):
        raise SystemExit(f"Restored dependency manifest does not match {target}.")
    requirement = engine_requirement(target)
    target_system, target_arch = cache_target(target, host)
    expected = {
        "engine": requirement[0] if requirement else None,
        "build_type": requirement[1] if requirement else None,
        "linkage": bundle_linkage(target),
        "target_system": target_system,
        "target_architecture": target_arch,
        "toolchain_identity": os.environ.get("SPENCH_TOOLCHAIN_IDENTITY", "local-host"),
    }
    mismatches = {
        key: (manifest.get(key), value)
        for key, value in expected.items()
        if manifest.get(key) != value
    }
    if mismatches:
        raise SystemExit(f"Restored dependency manifest has incompatible fields: {mismatches}")
    print(f"Restored dependency bundle: {artifact_root.relative_to(ROOT)}")


def write_tflite_package_config(prefix: Path) -> None:
    """Replace TFLite's non-relocatable dependency config with a bundle config."""
    candidates = sorted({
        *prefix.glob("lib/libtensorflow-lite*.a"),
        *prefix.glob("lib/libtensorflow-lite*.dylib"),
        *prefix.glob("lib/libtensorflow-lite*.so"),
        *prefix.glob("lib/libtensorflow-lite*.so.*"),
        *prefix.glob("lib/tensorflow-lite*.lib"),
    })
    if len(candidates) != 1:
        raise SystemExit(
            f"Expected exactly one installed TensorFlow Lite library under {prefix / 'lib'}, "
            f"found {len(candidates)}.")
    library_name = candidates[0].name
    config_dir = prefix / "lib" / "cmake" / "tensorflow-lite"
    config_dir.mkdir(parents=True, exist_ok=True)
    (config_dir / "tensorflow-liteConfig.cmake").write_text(
        "get_filename_component(_spench_tflite_prefix \"${CMAKE_CURRENT_LIST_DIR}/../../..\" ABSOLUTE)\n"
        "if(NOT TARGET tensorflow-lite::tensorflow-lite)\n"
        "  add_library(tensorflow-lite::tensorflow-lite UNKNOWN IMPORTED)\n"
        "  set_target_properties(tensorflow-lite::tensorflow-lite PROPERTIES\n"
        f"    IMPORTED_LOCATION \"${{_spench_tflite_prefix}}/lib/{library_name}\"\n"
        "    INTERFACE_INCLUDE_DIRECTORIES \"${_spench_tflite_prefix}/include\")\n"
        "endif()\n")


def write_wasm_onnx_package_config(prefix: Path, build_dir: Path) -> None:
    """Export ORT's manually bundled Emscripten archive as an imported target."""
    candidates = sorted(build_dir.rglob("libonnxruntime_webassembly.a"))
    if len(candidates) != 1:
        raise SystemExit(
            f"Expected exactly one WebAssembly ONNX Runtime archive below {build_dir}, "
            f"found {len(candidates)}.")
    library_dir = prefix / "lib"
    library_dir.mkdir(parents=True, exist_ok=True)
    library_name = candidates[0].name
    shutil.copy2(candidates[0], library_dir / library_name)
    config_dir = library_dir / "cmake" / "onnxruntime"
    config_dir.mkdir(parents=True, exist_ok=True)
    (config_dir / "onnxruntimeConfig.cmake").write_text(
        "get_filename_component(_spench_ort_prefix \"${CMAKE_CURRENT_LIST_DIR}/../../..\" ABSOLUTE)\n"
        "if(NOT TARGET onnxruntime_webassembly)\n"
        "  add_library(onnxruntime_webassembly STATIC IMPORTED GLOBAL)\n"
        "  set_target_properties(onnxruntime_webassembly PROPERTIES\n"
        f"    IMPORTED_LOCATION \"${{_spench_ort_prefix}}/lib/{library_name}\"\n"
        "    INTERFACE_INCLUDE_DIRECTORIES \"${_spench_ort_prefix}/include\")\n"
        "endif()\n")


def prepare_dependency(preset: str, host: bool) -> Path | None:
    """Build the selected engine target and export a dependency bundle."""
    requirement = engine_requirement(preset)
    artifact_root = dependency_artifact_root(preset, host)
    if requirement is None or artifact_root is None:
        return None
    engine, build_type = requirement
    prefix = artifact_root / "prefix"
    manifest_path = artifact_root / "manifest.json"
    expected = dependency_manifest(preset, host, prefix)
    if manifest_path.is_file() and prefix.is_dir():
        try:
            if json.loads(manifest_path.read_text()) == expected:
                print(f"Reusing prebuilt {engine} dependency bundle: {artifact_root.relative_to(ROOT)}")
                return prefix
        except json.JSONDecodeError:
            pass

    with artifact_lock(artifact_root):
        if manifest_path.is_file() and prefix.is_dir():
            try:
                if json.loads(manifest_path.read_text()) == expected:
                    print(f"Reusing prebuilt {engine} dependency bundle: {artifact_root.relative_to(ROOT)}")
                    return prefix
            except json.JSONDecodeError:
                pass

        use_host = host or preset in HOST_ONLY_PRESETS
        stage = None if use_host else PRESET_STAGES[preset]
        container = stage is not None
        environment = compiler_cache_environment(container)
        target_system, target_arch = cache_target(preset, host)
        linkage = bundle_linkage(preset)
        build_dir_host = (ROOT / "out" / "build" / "prebuilt" / target_system / target_arch /
                          engine / build_type.lower() / linkage)
        build_dir = (f"/workspace/{build_dir_host.relative_to(ROOT)}"
                     if container else str(build_dir_host))
        configure_command = [
            "cmake", "--preset", artifact_preset(preset), "-B", build_dir,
            f"-DSPENCH_ENGINE_BUNDLE_LINKAGE={linkage}",
            "-DSPENCH_PREPARE_DEPENDENCY_BUNDLE=ON",
        ]
        if container and stage in {"native", "web"}:
            configure_command.extend(("-DCMAKE_AR=/usr/bin/llvm-ar",
                                      "-DCMAKE_RANLIB=/usr/bin/llvm-ranlib"))
        print(f"Preparing {engine} {build_type} dependency bundle for {target_system}/{target_arch}.")
        run(configure_command, container_stage=stage, environment=environment)
        targets = (["libonnxruntime_webassembly.a"]
                   if preset.startswith("wasm-") else ["spench_dependency_bundle"])
        build_command = ["cmake", "--build", build_dir, "--target", *targets, "--parallel"]
        build_command.extend(("--config", build_type))
        run(build_command, container_stage=stage, environment=environment)
        if preset.startswith("wasm-") and engine == "onnx":
            write_wasm_onnx_package_config(prefix, build_dir_host)
        elif linkage == "static":
            archives = copy_static_archives(prefix, build_dir_host)
            write_engine_package_config(prefix, engine, None, linkage,
                                        static_archives=archives)
        else:
            library = copy_engine_library(prefix, engine, build_dir_host)
            write_engine_package_config(prefix, engine, library, linkage)
        if engine == "onnx":
            copy_onnx_headers(prefix)
        else:
            copy_tflite_headers(prefix, build_dir_host)

        write_dependency_bundle(artifact_root, expected)
        print(f"Prepared dependency bundle: {artifact_root.relative_to(ROOT)}")
        return prefix


def find_prebuilt_dependency(preset: str, host: bool) -> Path | None:
    """Return a local compatible bundle, or leave the build to CPM."""
    artifact_root = dependency_artifact_root(preset, host)
    if artifact_root is None:
        return None
    prefix = artifact_root / "prefix"
    manifest_path = artifact_root / "manifest.json"
    if not manifest_path.is_file() or not prefix.is_dir():
        return None
    try:
        if json.loads(manifest_path.read_text()) == dependency_manifest(preset, host, prefix):
            print(f"Using prebuilt dependency bundle: {artifact_root.relative_to(ROOT)}")
            return prefix
    except json.JSONDecodeError:
        pass
    print(f"Ignoring incompatible dependency bundle: {artifact_root.relative_to(ROOT)}")
    return None


def prebuilt_cmake_arguments(preset: str, host: bool, prefix: Path | None,
                             container: bool) -> list[str]:
    if prefix is None:
        return []
    requirement = engine_requirement(preset)
    if requirement is None:
        return []
    root = str(prefix)
    if container:
        root = f"/workspace/{prefix.relative_to(ROOT)}"
    variable = ("SPENCH_PREBUILT_ORT_ROOT" if requirement[0] == "onnx"
                else "SPENCH_PREBUILT_TFLITE_ROOT")
    return [f"-D{variable}={root}"]


def prepare_android_tflite(abi: str, build_type: str, host: bool) -> Path:
    """Export one Android ABI/configuration TFLite bundle without Gradle."""
    identity = f"v{DEPENDENCY_BUNDLE_FORMAT}-{dependency_bundle_digest('tflite')}"
    artifact_root = (ROOT / "out" / "artifacts" / "dependencies" / "android" / abi /
                     "tflite" / build_type.lower() / "shared" / identity)
    prefix = artifact_root / "prefix"
    manifest_path = artifact_root / "manifest.json"
    expected: dict[str, object] = {
        "format": DEPENDENCY_BUNDLE_FORMAT,
        "engine": "tflite",
        "build_type": build_type,
        "linkage": "shared",
        "preset": "android-tflite",
        "target_system": "android",
        "target_architecture": abi,
        "dependency_digest": dependency_bundle_digest("tflite"),
        "toolchain_identity": os.environ.get("SPENCH_TOOLCHAIN_IDENTITY", "local-host"),
        "prefix": str(prefix.relative_to(ROOT)),
    }
    if manifest_path.is_file() and prefix.is_dir():
        try:
            if json.loads(manifest_path.read_text()) == expected:
                print(f"Reusing prebuilt tflite dependency bundle: {artifact_root.relative_to(ROOT)}")
                return prefix
        except json.JSONDecodeError:
            pass

    if host:
        android_ndk = os.environ.get("ANDROID_NDK_HOME")
        if not android_ndk:
            raise SystemExit("ANDROID_NDK_HOME is required for --host Android dependency preparation.")
        source_dir = str(ROOT)
        build_dir = str(ROOT / "out" / "build" / "prebuilt" / "android" / abi /
                        "tflite" / build_type.lower())
        stage = None
    else:
        android_ndk = f"/opt/android-sdk/ndk/{ANDROID_NDK_VERSION}"
        source_dir = "/workspace"
        build_dir = f"/workspace/out/build/prebuilt/android/{abi}/tflite/{build_type.lower()}"
        stage = "android"
    environment = compiler_cache_environment(stage is not None)
    with artifact_lock(artifact_root):
        if manifest_path.is_file() and prefix.is_dir():
            try:
                if json.loads(manifest_path.read_text()) == expected:
                    print(f"Reusing prebuilt tflite dependency bundle: {artifact_root.relative_to(ROOT)}")
                    return prefix
            except json.JSONDecodeError:
                pass
        print(f"Preparing tflite {build_type} dependency bundle for android/{abi}.")
        configure_command = [
            "cmake", "-S", source_dir, "-B", build_dir,
            f"-DCMAKE_TOOLCHAIN_FILE={android_ndk}/build/cmake/android.toolchain.cmake",
            f"-DANDROID_ABI={abi}",
            "-DANDROID_PLATFORM=android-26",
            f"-DCMAKE_BUILD_TYPE={build_type}",
            "-DSPENCH_PROFILE=android-tflite",
            "-DBUILD_TFLITE_ENGINE=ON",
            "-DBUILD_ONNX_ENGINE=OFF",
            "-DBUILD_SHARED_LIBS=OFF",
            "-DBUILD_CLI=OFF",
            "-DBUILD_TESTING=OFF",
            "-DSPENCH_ENGINE_BUNDLE_LINKAGE=shared",
        ]
        run(configure_command, container_stage=stage, environment=environment)
        run(["cmake", "--build", build_dir, "--target", "tensorflow-lite", "--parallel", "2"],
            container_stage=stage, environment=environment)
        build_dir_host = (ROOT / "out" / "build" / "prebuilt" / "android" / abi /
                          "tflite" / build_type.lower())
        library = copy_engine_library(prefix, "tflite", build_dir_host)
        write_engine_package_config(prefix, "tflite", library, "shared")
        copy_tflite_headers(prefix, build_dir_host)
        write_dependency_bundle(artifact_root, expected)
        print(f"Prepared dependency bundle: {artifact_root.relative_to(ROOT)}")
        return prefix
