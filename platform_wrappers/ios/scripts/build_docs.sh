#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PROJECT="$ROOT_DIR/SpenchSDK.xcodeproj"
BUILD_DIR="$ROOT_DIR/build/docs"
SCHEME="SpenchAudioKit"
HOSTING_BASE_PATH="${DOCC_HOSTING_BASE_PATH:-}"
STATIC_OUTPUT="$BUILD_DIR/static-site"

"$ROOT_DIR/scripts/generate_project.sh"

rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"

xcodebuild docbuild \
  -project "$PROJECT" \
  -scheme "$SCHEME" \
  -destination "generic/platform=iOS Simulator" \
  -derivedDataPath "$BUILD_DIR/derived-data"

ARCHIVE_PATH="$(/usr/bin/find "$BUILD_DIR/derived-data" -name "${SCHEME}.doccarchive" -print -quit)"

if [[ -z "$ARCHIVE_PATH" ]]; then
  echo "error: failed to locate ${SCHEME}.doccarchive"
  exit 1
fi

echo "Built DocC archive at $ARCHIVE_PATH"

# Transform the archive into a static site that can be served from any
# static host (GitHub Pages, S3, internal CDN, ...). Pass a non-empty
# DOCC_HOSTING_BASE_PATH (e.g. "spench-ios") to mount the site under a
# subpath; otherwise it is hosted at the document root.
mkdir -p "$STATIC_OUTPUT"

DOCC_ARGS=(process-archive transform-for-static-hosting "$ARCHIVE_PATH" --output-path "$STATIC_OUTPUT")
if [[ -n "$HOSTING_BASE_PATH" ]]; then
  DOCC_ARGS+=(--hosting-base-path "$HOSTING_BASE_PATH")
fi

xcrun docc "${DOCC_ARGS[@]}"

echo "Built static documentation site at $STATIC_OUTPUT"
echo "Serve locally with: python3 -m http.server --directory \"$STATIC_OUTPUT\" 8000"
