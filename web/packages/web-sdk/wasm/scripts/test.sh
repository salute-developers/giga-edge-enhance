#!/usr/bin/env bash

set -euo pipefail
ROOT_DIR="$(git -C "$(dirname "${BASH_SOURCE[0]}")" rev-parse --show-toplevel)"
PROFILE="${1:-wasm-dsp-release}"
case "$PROFILE" in
  wasm-release)
    MODULE_NAME="gigaedge-enhance"
    MAX_SIZE_KB=8192
    ;;
  wasm-dsp-release)
    MODULE_NAME="gigaedge-enhance.dsp"
    MAX_SIZE_KB=4250
    ;;
  *)
    echo "error: unsupported WASM profile '$PROFILE'" >&2
    exit 2
    ;;
esac

BUILD_DIR="${GEE_BUILD_DIR:-$ROOT_DIR/out/build/$PROFILE}"
FILE_PATH="$BUILD_DIR/web/packages/web-sdk/wasm/$MODULE_NAME.wasm"
# Check if file exists
if [ ! -f "$FILE_PATH" ]; then
    echo "Error: File $FILE_PATH not found!"
    exit 1
fi

if [ "$PROFILE" = "wasm-release" ]; then
    exit 0
fi

MAX_SIZE=$(($MAX_SIZE_KB * 1024))  # MAX_SIZE_KB KB in bytes

# Get file size in bytes
FILE_SIZE=$(wc -c < "$FILE_PATH")

# Compare sizes
if [ "$FILE_SIZE" -gt "$MAX_SIZE" ]; then
    echo "Error: WASM file size exceeds $MAX_SIZE_KB KB limit! Current size: $((FILE_SIZE / 1024)) KB"
    exit 1
fi

echo "WASM file size is within limits: $((FILE_SIZE / 1024)) KB"
exit 0
