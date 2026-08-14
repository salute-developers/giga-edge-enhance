#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"

"$ROOT_DIR/gradlew" \
    clean \
    :spench-sdk:assemble \
    :spench-model-full24k:assemble \
    :spench-model-full48k:assemble \
    :demo-app:assemble
