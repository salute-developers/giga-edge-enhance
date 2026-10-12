#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PROJECT="$ROOT_DIR/GigaEdgeSpenchSDK.xcodeproj"
TEST_BUILD_DIR="$ROOT_DIR/build/xcframework/test-derived-data"
DEVICE_NAME="${TEST_DEVICE_NAME:-iPhone 17}"
GES_RETRIES="${GES_TEST_RETRIES:-2}"
APP_RETRIES="${APP_TEST_RETRIES:-3}"
ERASE_SIMULATOR="${ERASE_SIMULATOR:-1}"

COMMON_XCODEBUILD_ARGS=(
  -project "$PROJECT"
  -configuration Release
  -derivedDataPath "$TEST_BUILD_DIR"
  -parallel-testing-enabled NO
  -maximum-concurrent-test-simulator-destinations 1
)

SIMULATOR_FLAKE_PATTERN='Mach error -308|server died|Application failed preflight checks|FBSOpenApplicationServiceErrorDomain|SBMainWorkspace.*Busy|Failed to launch app with identifier|error -10868'

destination_for_tests() {
  local udid
  udid="$(xcrun simctl list devices available | grep -F "$DEVICE_NAME (" | sed -E 's/.*\(([0-9A-F-]+)\).*/\1/' | head -n1 || true)"

  if [[ -n "$udid" ]]; then
    printf 'id=%s' "$udid"
  else
    printf 'platform=iOS Simulator,name=%s' "$DEVICE_NAME"
  fi
}

prepare_simulator() {
  local destination
  local udid

  destination="$(destination_for_tests)"
  udid="${destination#id=}"

  echo "Preparing simulator state" >&2
  xcrun simctl shutdown all || true

  if [[ "$ERASE_SIMULATOR" == "1" ]]; then
    if [[ "$destination" == id=* ]]; then
      xcrun simctl erase "$udid" >/dev/null 2>&1 || true
    else
      xcrun simctl erase all >/dev/null 2>&1 || true
    fi
  fi

  if [[ "$destination" == id=* ]]; then
    xcrun simctl boot "$udid" >/dev/null 2>&1 || true
    xcrun simctl bootstatus "$udid" -b >/dev/null 2>&1 || true
  fi

  DESTINATION="$destination"
}

is_simulator_flake() {
  local log_file="$1"
  grep -Eq "$SIMULATOR_FLAKE_PATTERN" "$log_file"
}

run_tests_with_retry() {
  local scheme="$1"
  local retries="$2"
  local destination="$3"
  local attempt=1

  while [[ "$attempt" -le "$retries" ]]; do
    local log_file
    log_file="$(mktemp -t "${scheme}.xcodebuild.XXXXXX.log")"

    echo "Running $scheme tests (attempt $attempt/$retries)"
    set +e
    xcodebuild \
      "${COMMON_XCODEBUILD_ARGS[@]}" \
      -scheme "$scheme" \
      -destination "$destination" \
      test-without-building 2>&1 | tee "$log_file"
    local status=${PIPESTATUS[0]}
    set -e

    if [[ "$status" -eq 0 ]]; then
      rm -f "$log_file"
      return 0
    fi

    if [[ "$attempt" -lt "$retries" ]] && is_simulator_flake "$log_file"; then
      echo "Detected simulator launch flake, retrying..."
      sleep $((attempt * 2))
      attempt=$((attempt + 1))
      continue
    fi

    echo "Test run failed; preserving log at $log_file"
    return "$status"
  done
}

if [[ ! -d "$PROJECT" ]] || ! find "$TEST_BUILD_DIR" -name '*.xctestrun' -print -quit | grep -q .; then
  echo "error: iOS test products are missing; run tools/build ios first" >&2
  exit 1
fi

DESTINATION=""
prepare_simulator

run_tests_with_retry "GigaEdgeSpenchAudioKit" "$GES_RETRIES" "$DESTINATION"
run_tests_with_retry "AudioDemoApp" "$APP_RETRIES" "$DESTINATION"
