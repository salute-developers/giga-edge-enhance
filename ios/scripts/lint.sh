#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MODE="${1:-lint}"

run_swiftlint() {
  if ! command -v swiftlint >/dev/null 2>&1; then
    echo "error: swiftlint is not installed. Install with: brew install swiftlint"
    exit 1
  fi

  swiftlint version
  swiftlint --config "$ROOT_DIR/.swiftlint.yml"
}

run_clang_format() {
  if ! command -v clang-format >/dev/null 2>&1; then
    echo "warning: clang-format not installed; skipping ObjC++ format check."
    echo "Install via: brew install clang-format"
    return 0
  fi

  local cmd="$1"
  local files
  files=$(find "$ROOT_DIR/Sources/GigaEdgeSpenchAudioKit/ObjC" \
                "$ROOT_DIR/Sources/GigaEdgeSpenchAudioKit/include" \
                -type f \( -name "*.mm" -o -name "*.h" \) 2>/dev/null || true)

  if [[ -z "$files" ]]; then
    return 0
  fi

  clang-format --version

  case "$cmd" in
    check)
      # Exit non-zero if any file would be reformatted.
      echo "$files" | xargs clang-format --dry-run --Werror
      ;;
    format)
      echo "$files" | xargs clang-format -i
      ;;
  esac
}

case "$MODE" in
  lint)
    run_swiftlint
    run_clang_format check
    ;;
  format)
    run_clang_format format
    ;;
  *)
    echo "Usage: $0 [lint|format]"
    exit 1
    ;;
esac
