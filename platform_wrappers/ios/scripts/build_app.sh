#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PROJECT="$ROOT_DIR/SpenchSDK.xcodeproj"

"$ROOT_DIR/scripts/generate_project.sh"

xcodebuild \
  -project "$PROJECT" \
  -scheme "AudioDemoApp" \
  -destination "generic/platform=iOS Simulator" \
  build
