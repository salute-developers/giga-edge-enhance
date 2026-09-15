#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
AVD_NAME="giga-denoiser-ci-api35"
export ANDROID_AVD_HOME="${ANDROID_AVD_HOME:-$HOME/.android/avd}"
AVD_DIR="$ANDROID_AVD_HOME"
SYSTEM_IMAGE="${GDN_ANDROID_SYSTEM_IMAGE:?Android emulator system image is not configured}"
EMULATOR_LOG="$ROOT_DIR/../out/artifacts/android-emulator.log"

mkdir -p "$AVD_DIR" "$(dirname "$EMULATOR_LOG")"
if [[ ! -d "$AVD_DIR/$AVD_NAME.avd" ]]; then
  printf 'no\n' | avdmanager create avd \
    --force \
    --name "$AVD_NAME" \
    --package "$SYSTEM_IMAGE" \
    --device pixel_2
fi

emulator \
  -avd "$AVD_NAME" \
  -no-window \
  -no-audio \
  -no-boot-anim \
  -no-metrics \
  -no-snapshot \
  -partition-size 2048 \
  -gpu swiftshader_indirect \
  -wipe-data >"$EMULATOR_LOG" 2>&1 &
emulator_pid=$!

cleanup() {
  adb emu kill >/dev/null 2>&1 || true
  if kill -0 "$emulator_pid" >/dev/null 2>&1; then
    kill "$emulator_pid" >/dev/null 2>&1 || true
  fi
  wait "$emulator_pid" 2>/dev/null || true
}
trap cleanup EXIT

fail_with_log() {
  echo "error: $1" >&2
  cat "$EMULATOR_LOG" >&2
  exit 1
}

adb_deadline=$((SECONDS + 120))
until [[ "$(adb get-state 2>/dev/null)" == "device" ]]; do
  if ! kill -0 "$emulator_pid" >/dev/null 2>&1; then
    fail_with_log "Android emulator exited before ADB connected"
  fi
  if ((SECONDS >= adb_deadline)); then
    fail_with_log "Android emulator did not connect to ADB within 120 seconds"
  fi
  sleep 2
done

boot_deadline=$((SECONDS + 300))
until [[ "$(adb shell getprop sys.boot_completed 2>/dev/null | tr -d '\r')" == "1" ]]; do
  if ! kill -0 "$emulator_pid" >/dev/null 2>&1; then
    fail_with_log "Android emulator exited before completing boot"
  fi
  if ((SECONDS >= boot_deadline)); then
    fail_with_log "Android emulator did not finish booting within 300 seconds"
  fi
  sleep 2
done

adb shell settings put global window_animation_scale 0
adb shell settings put global transition_animation_scale 0
adb shell settings put global animator_duration_scale 0

"$ROOT_DIR/gradlew" \
  -p "$ROOT_DIR" \
  -PgigaDenoiserAndroidTestBuildType=release \
  :giga-denoiser-sdk:connectedReleaseAndroidTest \
  :demo-app:connectedReleaseAndroidTest
