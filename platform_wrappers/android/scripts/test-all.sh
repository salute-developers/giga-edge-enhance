#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"

cmake \
    -S "$ROOT_DIR/spench-sdk/src/main/cpp/tests" \
    -B "$ROOT_DIR/build/native-tests"

cmake --build "$ROOT_DIR/build/native-tests"

"$ROOT_DIR/build/native-tests/spench-native-tests"

"$ROOT_DIR/gradlew" \
    clean \
    :spench-sdk:testDebugUnitTest \
    :spench-sdk:assembleDebug \
    :spench-model-full24k:assembleDebug \
    :spench-model-full48k:assembleDebug \
    :demo-app:assembleDebug \
    :spench-sdk:connectedDebugAndroidTest \
    :demo-app:connectedDebugAndroidTest
