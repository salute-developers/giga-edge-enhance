#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
REPO_DIR="$(cd "$ROOT_DIR/../.." && pwd)"
BUILD_DIR="$ROOT_DIR/build/core"
TOOLCHAIN_FILE="$ROOT_DIR/cmake/ios.toolchain.cmake"

configure_build() {
  local build_dir="$1"
  local platform="$2"

  cmake -S "$REPO_DIR" -B "$build_dir" \
    -G Xcode \
    -DBUILD_SHARED_LIBS=OFF \
    -DBUILD_ONNX_ENGINE=ON \
    -DBUILD_DEMO=OFF \
    -DBUILD_TESTING=OFF \
    -DPLATFORM="$platform" \
    -DCMAKE_DISABLE_FIND_PACKAGE_flatbuffers=ON \
    -DCMAKE_CXX_FLAGS="-Wno-deprecated-literal-operator -Wno-error=deprecated-literal-operator -Wno-dangling-capture" \
    -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN_FILE" \
    -DDEPLOYMENT_TARGET=15.0
}

build_target() {
  local build_dir="$1"
  cmake --build "$build_dir" --config Release --target spench
}

merge_archives() {
  local build_dir="$1"
  local output_path="$2"

  rm -f "$output_path"
  find "$build_dir" -type f -name '*.a' -print0 | xargs -0 libtool -static -o "$output_path"
}

rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR/device" "$BUILD_DIR/simulator"

configure_build "$BUILD_DIR/device" OS64
build_target "$BUILD_DIR/device"

configure_build "$BUILD_DIR/simulator" SIMULATOR64COMBINED
build_target "$BUILD_DIR/simulator"

merge_archives "$BUILD_DIR/device" "$BUILD_DIR/device/libspench.a"
merge_archives "$BUILD_DIR/simulator" "$BUILD_DIR/simulator/libspench.a"

echo "Built device static library: $BUILD_DIR/device/libspench.a"
echo "Built simulator static library: $BUILD_DIR/simulator/libspench.a"
lipo -info "$BUILD_DIR/device/libspench.a"
lipo -info "$BUILD_DIR/simulator/libspench.a"
