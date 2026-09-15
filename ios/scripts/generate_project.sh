#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

if ! command -v xcodegen >/dev/null 2>&1; then
  echo "error: xcodegen is not installed. Install with: brew install xcodegen"
  exit 1
fi

cd "$ROOT_DIR"
xcodegen generate
echo "Generated Xcode project at $ROOT_DIR/GigaDenoiserSDK.xcodeproj"
