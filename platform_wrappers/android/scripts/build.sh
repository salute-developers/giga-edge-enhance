#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"

"$ROOT_DIR/gradlew" \
    -p "$ROOT_DIR" \
    :spench-sdk:assembleRelease \
    :spench-model-full24k:assembleRelease \
    :spench-model-full48k:assembleRelease
