"""Repository paths and supported build targets."""

from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
BAKE_FILE = ROOT / "docker-bake.hcl"
BASE_DOCKERFILE = ROOT / "dockerfiles/Dockerfile.base"
TOOLCHAIN_DOCKERFILES = {
    "python": ROOT / "dockerfiles/Dockerfile.python",
    "native": ROOT / "dockerfiles/Dockerfile.native",
    "web": ROOT / "dockerfiles/Dockerfile.web",
    "android": ROOT / "dockerfiles/Dockerfile.android",
    "android-ui": ROOT / "dockerfiles/Dockerfile.android-ui",
}
WEB_PACKAGE_LOCK = ROOT / "web/package-lock.json"
WEB_IMAGE_INPUTS = (
    ROOT / "web/.nvmrc",
    ROOT / "web/apps/site/mos_install.sh",
    ROOT / "web/apps/site/requirements-mos.txt",
)
PRESET_STAGES = {
    "python-release-onnx": "python",
    "python-release-tflite": "python",
    "native-debug-onnx": "native",
    "native-release-onnx": "native",
    "native-debug-tflite": "native",
    "native-release-tflite": "native",
    "native-release-static-onnx": "native",
    "native-release-static-tflite": "native",
    "native-asan-onnx": "native",
    "native-model-packer": "native",
    "wasm-release": "web",
    "wasm-debug": "web",
    "wasm-dsp-release": "web",
    "wasm-dsp-debug": "web",
}
HOST_ONLY_PRESETS = (
    "macos-x86_64-debug-onnx",
    "macos-x86_64-release-onnx",
    "macos-x86_64-release-tflite",
    "macos-x86_64-debug-tflite",
    "macos-x86_64-asan-onnx",
    "macos-x86_64-release-static-onnx",
    "macos-arm64-debug-onnx",
    "macos-arm64-release-onnx",
    "macos-arm64-release-tflite",
    "macos-arm64-debug-tflite",
    "macos-arm64-asan-onnx",
    "macos-arm64-release-static-onnx",
    "windows-x64-debug-onnx",
    "windows-x64-release-onnx",
    "windows-x64-release-tflite",
    "windows-x64-debug-tflite",
    "windows-x64-asan-onnx",
    "windows-x64-release-static-tflite",
    "ios-device-release",
    "ios-device-debug",
    "ios-simulator-release",
    "ios-simulator-debug",
)
CACHE_INPUTS = (
    ROOT / "package-lock.cmake",
    ROOT / "cmake/CPM.cmake",
    ROOT / "cmake/Dependencies.cmake",
)
DEPENDENCY_RECIPE_INPUTS = (
    ROOT / "package-lock.cmake",
    ROOT / "cmake/Dependencies.cmake",
    ROOT / "cmake/Platform.cmake",
)
DEPENDENCY_BUNDLE_FORMAT = 5
ENGINE_PACKAGE = {"onnx": "onnxruntime", "tflite": "tflite"}
ENGINE_PATCH = {
    "onnx": ROOT / "cmake/patches/onnxruntime.patch",
    "tflite": ROOT / "cmake/patches/tflite.patch",
}
ANDROID_ABIS = ("arm64-v8a", "armeabi-v7a", "x86_64", "x86")
ANDROID_DEPENDENCY_TARGETS = {
    f"android-release-tflite-{abi}": abi for abi in ANDROID_ABIS
}
ANDROID_NDK_VERSION = "29.0.14206865"
NATIVE_PRESETS = {
    "python-release-onnx",
    "python-release-tflite",
    "native-debug-onnx",
    "native-release-onnx",
    "native-debug-tflite",
    "native-release-tflite",
    "native-release-static-onnx",
    "native-release-static-tflite",
    "native-asan-onnx",
} | set(HOST_ONLY_PRESETS)
