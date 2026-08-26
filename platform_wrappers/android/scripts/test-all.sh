#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
REPO_DIR="$(cd "$ROOT_DIR/../.." && pwd)"
NATIVE_BUILD_DIR="${SPENCH_ANDROID_NATIVE_BUILD_DIR:-$REPO_DIR/out/build/android-native-tests}"

cmake \
    -S "$ROOT_DIR/spench-sdk/src/main/cpp/tests" \
    -B "$NATIVE_BUILD_DIR" \
    -DCPM_SOURCE_CACHE="$REPO_DIR/out/cache/cpm"

cmake --build "$NATIVE_BUILD_DIR"

"$NATIVE_BUILD_DIR/spench-native-tests"

"$ROOT_DIR/gradlew" \
    -p "$ROOT_DIR" \
    :spench-sdk:testDebugUnitTest \
    :spench-sdk:assembleDebug \
    :spench-model-full24k:assembleDebug \
    :spench-model-full48k:assembleDebug \
    :demo-app:assembleDebug

if [[ "${RUN_ANDROID_INSTRUMENTED_TESTS:-0}" == "1" ]]; then
  "$ROOT_DIR/gradlew" \
      -p "$ROOT_DIR" \
      :spench-sdk:connectedDebugAndroidTest \
      :demo-app:connectedDebugAndroidTest
else
  echo "Skipping Android instrumented tests; set RUN_ANDROID_INSTRUMENTED_TESTS=1 with an emulator/device."
fi
