import io
from pathlib import Path
import tempfile
import unittest
from unittest import mock

from infra import cli, workflows


class CliTest(unittest.TestCase):
    def test_metadata_action_prints_value(self):
        output = io.StringIO()
        with mock.patch.object(cli, "container_image_digest", return_value="digest"), \
             mock.patch("sys.stdout", output):
            cli.main(["image-key", "native"])
        self.assertEqual(output.getvalue(), "digest\n")

    def test_restore_requires_archive(self):
        with self.assertRaisesRegex(SystemExit, "requires an archive"):
            cli.main(["restore-deps", "native-release-onnx"])

    def test_platform_action_dispatches(self):
        with mock.patch.object(cli, "android_workflow") as android:
            cli.main(["build", "android", "--host"])
        android.assert_called_once_with("build", True)

    def test_invalid_platform_action_fails_without_dispatch(self):
        with self.assertRaisesRegex(SystemExit, "Android supports build and test"):
            cli.main(["package", "android"])


class WorkflowsTest(unittest.TestCase):
    def test_container_cmake_build_uses_expected_commands(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            with mock.patch.object(workflows, "ROOT", root), \
                 mock.patch.object(workflows, "find_prebuilt_dependency", return_value=None), \
                 mock.patch.object(workflows, "compiler_cache_environment", return_value={}), \
                 mock.patch.object(workflows, "print_compiler_cache_stats"), \
                 mock.patch.object(workflows, "run") as run:
                workflows.cmake_workflow("build", "native-release-onnx", False)
        configure = run.call_args_list[0]
        build = run.call_args_list[1]
        self.assertEqual(configure.args[0][:3], ["cmake", "--preset", "native-release-onnx"])
        self.assertIn("-DCMAKE_AR=/usr/bin/llvm-ar", configure.args[0])
        self.assertEqual(configure.kwargs["container_stage"], "native")
        self.assertEqual(build.args[0][:2], ["cmake", "--build"])

    def test_container_install_verifies_external_consumer(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            build_dir = (root / "out" / "build" / "container-clang-llvm-tools" /
                         "native-release-onnx")
            build_dir.mkdir(parents=True)
            with mock.patch.object(workflows, "ROOT", root), \
                 mock.patch.object(workflows, "find_prebuilt_dependency", return_value=None), \
                 mock.patch.object(workflows, "compiler_cache_environment", return_value={}), \
                 mock.patch.object(workflows, "write_artifact_checksums"), \
                 mock.patch.object(workflows, "run") as run:
                workflows.cmake_workflow("install", "native-release-onnx", False)

        self.assertEqual(run.call_count, 4)
        self.assertEqual(run.call_args_list[0].args[0][:2], ["cmake", "--install"])
        self.assertEqual(run.call_args_list[1].args[0][0:2], ["cmake", "-S"])
        self.assertEqual(run.call_args_list[2].args[0][0:2], ["cmake", "--build"])
        self.assertIn("LD_LIBRARY_PATH", run.call_args_list[3].kwargs["environment"])

    def test_native_package_keeps_only_the_final_archive(self):
        for preset in ("native-release-onnx", "native-release-tflite"):
            with self.subTest(preset=preset), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                (root / "out/build" / preset).mkdir(parents=True)
                package_dir = root / "out/artifacts" / preset
                package_dir.mkdir(parents=True)
                (package_dir / "stale-file").touch()

                def create_package(command, **_kwargs):
                    output_dir = Path(command[command.index("-B") + 1])
                    (output_dir / "_CPack_Packages").mkdir(parents=True)
                    (output_dir / "spench-0.0.1-Linux.tar.gz").touch()

                with mock.patch.object(workflows, "ROOT", root), \
                     mock.patch.object(workflows, "compiler_cache_environment", return_value={}), \
                     mock.patch.object(workflows, "run", side_effect=create_package):
                    workflows.cmake_workflow("package", preset, True)

                self.assertEqual(
                    [path.name for path in package_dir.iterdir()],
                    ["spench-0.0.1-Linux.tar.gz"])
                self.assertFalse((root / "out/build/package" / preset).exists())

    def test_macos_x86_install_verifies_matching_consumer_architecture(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            build_dir = root / "out" / "build" / "macos-x86_64-release-onnx"
            build_dir.mkdir(parents=True)
            with mock.patch.object(workflows, "ROOT", root), \
                 mock.patch.object(workflows, "find_prebuilt_dependency", return_value=None), \
                 mock.patch.object(workflows, "compiler_cache_environment", return_value={}), \
                 mock.patch.object(workflows, "write_artifact_checksums"), \
                 mock.patch.object(workflows.platform, "system", return_value="Darwin"), \
                 mock.patch.object(workflows, "run") as run:
                workflows.cmake_workflow("install", "macos-x86_64-release-onnx", True)

        self.assertIn("-DCMAKE_OSX_ARCHITECTURES=x86_64", run.call_args_list[1].args[0])
        self.assertIn("DYLD_LIBRARY_PATH", run.call_args_list[3].kwargs["environment"])

    def test_windows_install_uses_visual_studio_consumer(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            build_dir = root / "out" / "build" / "windows-x64-release-onnx"
            build_dir.mkdir(parents=True)
            with mock.patch.object(workflows, "ROOT", root), \
                 mock.patch.object(workflows, "find_prebuilt_dependency", return_value=None), \
                 mock.patch.object(workflows, "compiler_cache_environment", return_value={}), \
                 mock.patch.object(workflows, "write_artifact_checksums"), \
                 mock.patch.object(workflows.os, "name", "nt"), \
                 mock.patch.object(workflows, "run") as run:
                workflows.cmake_workflow("install", "windows-x64-release-onnx", True)

        self.assertIn("Visual Studio 17 2022", run.call_args_list[1].args[0])
        self.assertEqual(run.call_args_list[2].args[0][-2:], ["--config", "Release"])
        self.assertTrue(run.call_args_list[3].args[0][0].endswith("/Release/spench_package_consumer.exe"))

    def test_android_workflow_sets_dependency_root(self):
        with mock.patch.object(workflows, "compiler_cache_environment", return_value={}), \
             mock.patch.object(workflows, "run") as run:
            workflows.android_workflow("test", False)
        self.assertEqual(run.call_args.args[0], ["platform_wrappers/android/scripts/test-all.sh"])
        self.assertEqual(run.call_args.kwargs["container_stage"], "android")
        self.assertEqual(
            run.call_args.kwargs["environment"]["SPENCH_PREBUILT_DEPENDENCIES_ROOT"],
            "/workspace/out/artifacts/dependencies")

    def test_prepare_android_target_dispatches_single_abi(self):
        with mock.patch.object(workflows, "prepare_android_tflite") as prepare:
            workflows.prepare_platform_dependencies(
                "android-release-tflite-x86_64", False)
        prepare.assert_called_once_with("x86_64", "Release", False)

    def test_container_playwright_runs_all_browsers_with_audio(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            installed_sdk = (root / "platform_wrappers/web/apps/spench-site/node_modules" /
                             "@gigaedge/spench-web-sdk/package.json")
            installed_sdk.parent.mkdir(parents=True)
            installed_sdk.touch()
            with mock.patch.object(workflows, "ROOT", root), \
                 mock.patch.object(workflows, "run_web_node") as run_web_node:
                workflows.web_workflow("playwright", False)

        install_command = run_web_node.call_args_list[0].args[0]
        test_command = run_web_node.call_args_list[2].args[0]
        self.assertEqual(install_command[-3:], ["chromium", "firefox", "webkit"])
        self.assertEqual(test_command[:3], ["bash", "-c", 'pulseaudio --start --exit-idle-time=-1 && exec "$@"'])
        self.assertEqual(test_command[-3:], ["--project=chromium", "--project=firefox", "--project=webkit"])
        self.assertEqual(run_web_node.call_args_list[2].kwargs["container_stage"], "web")


if __name__ == "__main__":
    unittest.main()
