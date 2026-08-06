#!/usr/bin/env bash
set -euo pipefail

DESTINATION_PATH="$1"
shift

mkdir -p "$DESTINATION_PATH"

for bundle_name in "$@"; do
  SOURCE_PATH="${BUILT_PRODUCTS_DIR}/${bundle_name}.bundle"
  TARGET_PATH="${DESTINATION_PATH}/${bundle_name}.bundle"

  rm -rf "$TARGET_PATH"
  if [ -d "$SOURCE_PATH" ]; then
    cp -R "$SOURCE_PATH" "$TARGET_PATH"
  fi
done
