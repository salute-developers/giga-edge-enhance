#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
REPO_DIR="$(cd "$ROOT_DIR/.." && pwd)"
ARTIFACT_DIR="$REPO_DIR/out/artifacts/ios"
CORE_STAMP="$ARTIFACT_DIR/.core-build.stamp"
CORE_INPUTS=(
  "$REPO_DIR/CMakeLists.txt"
  "$REPO_DIR/CMakePresets.json"
  "$REPO_DIR/cmake"
  "$REPO_DIR/core"
  "$ROOT_DIR/cmake/ios.toolchain.cmake"
)

cd "$REPO_DIR"

archive_contains_onnxruntime() {
  xcrun nm -gjU "$1" | grep -Fx '_OrtGetApiBase' >/dev/null
}

core_is_current() {
  [[ -f "$ARTIFACT_DIR/device/libgigaedge-spench.a" ]] || return 1
  [[ -f "$ARTIFACT_DIR/simulator/libgigaedge-spench.a" ]] || return 1
  [[ -d "$REPO_DIR/out/build/ios-device-release" ]] || return 1
  [[ -d "$REPO_DIR/out/build/ios-simulator-release" ]] || return 1
  archive_contains_onnxruntime "$ARTIFACT_DIR/device/libgigaedge-spench.a" || return 1
  archive_contains_onnxruntime "$ARTIFACT_DIR/simulator/libgigaedge-spench.a" || return 1

  local reference="$CORE_STAMP"
  if [[ ! -f "$reference" ]]; then
    reference="$ARTIFACT_DIR/device/libgigaedge-spench.a"
  fi

  [[ -z "$(find "${CORE_INPUTS[@]}" -newer "$reference" -print -quit)" ]]
}

build_target() {
  local preset="$1"
  local prebuilt_root=""
  case "$preset" in
    ios-device-*) prebuilt_root="${GES_PREBUILT_ORT_DEVICE_ROOT:-}" ;;
    ios-simulator-*) prebuilt_root="${GES_PREBUILT_ORT_SIMULATOR_ROOT:-}" ;;
  esac
  if [[ -n "$prebuilt_root" ]]; then
    cmake --preset "$preset" "-DGES_PREBUILT_ORT_ROOT=$prebuilt_root"
  else
    cmake --preset "$preset"
  fi
  cmake --build --preset "$preset"
}

merge_archives() {
  local build_dir="$1"
  local output_path="$2"
  local prebuilt_root="$3"

  local archives=()
  local archive_roots=("$build_dir")
  if [[ -n "$prebuilt_root" && -d "$prebuilt_root/lib/archives" ]]; then
    archive_roots+=("$prebuilt_root/lib/archives")
  fi
  while IFS= read -r archive; do
    archives+=("$archive")
  done < <(find "${archive_roots[@]}" -type f -name '*.a' -print | LC_ALL=C sort)

  if [[ "${#archives[@]}" -eq 0 ]]; then
    echo "error: no static archives were produced in $build_dir" >&2
    exit 1
  fi

  mkdir -p "$(dirname "$output_path")"
  rm -f "$output_path"
  xcrun libtool -static -o "$output_path" "${archives[@]}"
  if ! archive_contains_onnxruntime "$output_path"; then
    echo "error: merged iOS core archive does not contain ONNX Runtime" >&2
    exit 1
  fi
}

if core_is_current; then
  touch "$CORE_STAMP"
  echo "iOS core is current; reusing $ARTIFACT_DIR"
  exit 0
fi

build_target ios-device-release
build_target ios-simulator-release

merge_archives \
  "$REPO_DIR/out/build/ios-device-release" \
  "$ARTIFACT_DIR/device/libgigaedge-spench.a" \
  "${GES_PREBUILT_ORT_DEVICE_ROOT:-}"
merge_archives \
  "$REPO_DIR/out/build/ios-simulator-release" \
  "$ARTIFACT_DIR/simulator/libgigaedge-spench.a" \
  "${GES_PREBUILT_ORT_SIMULATOR_ROOT:-}"
touch "$CORE_STAMP"

echo "Built device static library: $ARTIFACT_DIR/device/libgigaedge-spench.a"
echo "Built simulator static library: $ARTIFACT_DIR/simulator/libgigaedge-spench.a"
lipo -info "$ARTIFACT_DIR/device/libgigaedge-spench.a"
lipo -info "$ARTIFACT_DIR/simulator/libgigaedge-spench.a"
