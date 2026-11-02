"""Prepare an iPhone simulator compatible with the selected Xcode SDK."""

import json
import os
import subprocess
import sys


COMMAND_TIMEOUT = 180


def run(*command, capture_output=False):
    print("+ " + " ".join(command), file=sys.stderr, flush=True)
    try:
        result = subprocess.run(
            command, check=True, text=True, timeout=COMMAND_TIMEOUT,
            stdout=subprocess.PIPE if capture_output else sys.stderr,
            stderr=sys.stderr)
    except subprocess.TimeoutExpired:
        raise SystemExit(
            f"error: simulator command timed out after {COMMAND_TIMEOUT}s: "
            + " ".join(command))
    except subprocess.CalledProcessError as error:
        raise SystemExit(error.returncode)
    return result.stdout


def select_device(devices, sdk_version, device_name=""):
    # `available` also includes newer runtimes that the selected Xcode cannot boot.
    runtime = "com.apple.CoreSimulator.SimRuntime.iOS-" + sdk_version.replace(".", "-")
    available = [device for device in devices.get(runtime, []) if device["isAvailable"]]
    for device in available:
        if device_name:
            matches = device["name"] == device_name
        else:
            matches = device["name"].startswith("iPhone")
        if matches:
            return device
    names = ", ".join(device["name"] for device in available) or "none"
    raise SystemExit(
        f"error: no available {device_name or 'iPhone'} simulator for iOS SDK "
        f"{sdk_version}. Available devices in this runtime: {names}. "
        "Install a matching simulator runtime or select another Xcode.")


def main():
    sdk_version = run(
        "xcrun", "--sdk", "iphonesimulator", "--show-sdk-version",
        capture_output=True).strip()
    devices = json.loads(run(
        "xcrun", "simctl", "list", "devices", "available", "--json",
        capture_output=True))["devices"]
    device = select_device(devices, sdk_version, os.environ.get("TEST_DEVICE_NAME", ""))
    udid = device["udid"]
    print(
        f"Preparing simulator: {device['name']} ({udid}), iOS {sdk_version}, "
        f"state {device['state']}", file=sys.stderr, flush=True)

    # Only reset the selected device; do not disturb other local simulators.
    if device["state"] != "Shutdown":
        run("xcrun", "simctl", "shutdown", udid)
    if os.environ.get("ERASE_SIMULATOR", "1") == "1":
        run("xcrun", "simctl", "erase", udid)
    run("xcrun", "simctl", "boot", udid)
    run("xcrun", "simctl", "bootstatus", udid, "-b")

    # stdout is consumed by test.sh; diagnostics and boot progress go to stderr.
    print(f"id={udid}")


if __name__ == "__main__":
    main()
