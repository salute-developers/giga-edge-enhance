#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"

"$ROOT_DIR/gradlew" \
    -p "$ROOT_DIR" \
    :giga-denoiser-sdk:testReleaseUnitTest

if [[ "${RUN_ANDROID_INSTRUMENTED_TESTS:-0}" == "1" ]]; then
  "$ROOT_DIR/gradlew" \
      -p "$ROOT_DIR" \
      :giga-denoiser-sdk:connectedReleaseAndroidTest \
      :demo-app:connectedReleaseAndroidTest
else
  echo "Skipping Android instrumented tests; set RUN_ANDROID_INSTRUMENTED_TESTS=1 with an emulator/device."
fi
