#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"

"$ROOT_DIR/gradlew" \
    -p "$ROOT_DIR" \
    :gigaedge-enhance-sdk:testDebugUnitTest \
    :gigaedge-enhance-sdk:connectedDebugAndroidTest
