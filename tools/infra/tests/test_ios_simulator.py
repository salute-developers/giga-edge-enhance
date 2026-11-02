import importlib.util
import io
import json
import os
from pathlib import Path
import subprocess
import unittest
from unittest import mock


ROOT = Path(__file__).resolve().parents[3]
SPEC = importlib.util.spec_from_file_location(
    "ios_simulator", ROOT / "ios/scripts/prepare_simulator.py")
simulator = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(simulator)


class IosSimulatorTest(unittest.TestCase):
    def setUp(self):
        self.device = {
            "name": "iPhone 16", "udid": "compatible-udid",
            "state": "Shutdown", "isAvailable": True,
        }
        self.devices = {
            "com.apple.CoreSimulator.SimRuntime.iOS-26-0": [
                {**self.device, "name": "iPhone 17", "udid": "incompatible-udid"}],
            "com.apple.CoreSimulator.SimRuntime.iOS-18-5": [self.device],
        }

    def test_default_device_matches_selected_xcode_sdk(self):
        self.assertEqual(
            simulator.select_device(self.devices, "18.5")["udid"], "compatible-udid")

    def test_explicit_device_cannot_select_a_newer_runtime(self):
        with self.assertRaisesRegex(SystemExit, "no available iPhone 17.*18.5"):
            simulator.select_device(self.devices, "18.5", "iPhone 17")

    def test_explicit_device_name_is_respected(self):
        other_device = {**self.device, "name": "iPhone 16 Pro", "udid": "requested-udid"}
        self.devices["com.apple.CoreSimulator.SimRuntime.iOS-18-5"].append(other_device)
        self.assertEqual(
            simulator.select_device(self.devices, "18.5", "iPhone 16 Pro")["udid"],
            "requested-udid")

    def test_unavailable_device_fails_before_boot(self):
        self.device["isAvailable"] = False
        with self.assertRaisesRegex(SystemExit, "Available devices in this runtime: none"):
            simulator.select_device(self.devices, "18.5")

    def test_missing_runtime_has_actionable_error(self):
        with self.assertRaisesRegex(SystemExit, "Install a matching simulator runtime"):
            simulator.select_device(self.devices, "18.4")

    def test_preparation_keeps_stdout_as_destination_only(self):
        output = io.StringIO()
        with mock.patch.object(simulator, "run", side_effect=[
                "18.5\n", json.dumps({"devices": self.devices}), None, None, None]) as run, \
             mock.patch.dict(os.environ, {}, clear=True), \
             mock.patch("sys.stdout", output), mock.patch("sys.stderr", io.StringIO()):
            simulator.main()
        self.assertEqual(output.getvalue(), "id=compatible-udid\n")
        self.assertEqual([call.args for call in run.call_args_list[2:]], [
            ("xcrun", "simctl", "erase", "compatible-udid"),
            ("xcrun", "simctl", "boot", "compatible-udid"),
            ("xcrun", "simctl", "bootstatus", "compatible-udid", "-b"),
        ])

    def test_booted_device_is_shutdown_and_erase_can_be_disabled(self):
        self.device["state"] = "Booted"
        with mock.patch.object(simulator, "run", side_effect=[
                "18.5\n", json.dumps({"devices": self.devices}), None, None, None]) as run, \
             mock.patch.dict(os.environ, {"ERASE_SIMULATOR": "0"}, clear=True), \
             mock.patch("sys.stdout", io.StringIO()), mock.patch("sys.stderr", io.StringIO()):
            simulator.main()
        self.assertEqual([call.args for call in run.call_args_list[2:]], [
            ("xcrun", "simctl", "shutdown", "compatible-udid"),
            ("xcrun", "simctl", "boot", "compatible-udid"),
            ("xcrun", "simctl", "bootstatus", "compatible-udid", "-b"),
        ])

    def test_simulator_timeout_is_bounded_and_reported(self):
        with mock.patch.object(simulator.subprocess, "run", side_effect=
                subprocess.TimeoutExpired(["xcrun", "simctl", "bootstatus"], 180)) as run, \
             mock.patch("sys.stderr", io.StringIO()), \
             self.assertRaisesRegex(SystemExit, "timed out after 180s"):
            simulator.run("xcrun", "simctl", "bootstatus", "compatible-udid", "-b")
        self.assertEqual(run.call_args.kwargs["timeout"], 180)

    def test_simulator_errors_are_not_ignored(self):
        with mock.patch.object(simulator.subprocess, "run", side_effect=
                subprocess.CalledProcessError(42, ["xcrun", "simctl", "boot"])), \
             mock.patch("sys.stderr", io.StringIO()), self.assertRaises(SystemExit) as error:
            simulator.run("xcrun", "simctl", "boot", "compatible-udid")
        self.assertEqual(error.exception.code, 42)


if __name__ == "__main__":
    unittest.main()
