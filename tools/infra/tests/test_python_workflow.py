import os
from pathlib import Path
import tempfile
import unittest
from unittest import mock

from infra import cli, execution, identity, python_workflow


class PythonWorkflowTest(unittest.TestCase):
    def test_distribution_actions_dispatch_through_canonical_tools(self):
        for target in ("python-wheels", "python-sdist"):
            for action in (cli.build_main, cli.test_main, cli.package_main):
                with (
                    self.subTest(target=target, action=action.__name__),
                    mock.patch.object(cli, "python_workflow") as workflow,
                ):
                    action([target])
                    workflow.assert_called_once_with(
                        action.__name__.removesuffix("_main"), target, False
                    )

    def test_manylinux_and_ubuntu_bundles_never_share_default_identity(self):
        with mock.patch.dict(os.environ, {}, clear=True):
            self.assertNotEqual(
                identity.dependency_tag("python-release-onnx", False),
                identity.dependency_tag("native-release-onnx", False),
            )
            self.assertEqual(
                identity.dependency_toolchain_identity("python-release-onnx"),
                identity.toolchain_recipe_digest("python"),
            )

    def test_container_can_expose_same_workspace_at_cibuildwheel_source_path(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            with (
                mock.patch.object(execution, "ROOT", root),
                mock.patch.object(execution, "ensure_container_image", return_value="python-image"),
                mock.patch.object(execution.subprocess, "run") as run,
            ):
                execution.run(
                    ["python", "test.py"],
                    container_stage="python",
                    workdir=root,
                    additional_mounts=((root, "/project"),),
                )
            command = run.call_args.args[0]
            self.assertIn(f"{root}:/project", command)
            self.assertIn(f"{root}:/workspace", command)

    def test_all_actions_dispatch_python_targets(self):
        for action in (cli.build_main, cli.test_main, cli.package_main):
            with (
                self.subTest(action=action.__name__),
                mock.patch.object(cli, "python_workflow") as workflow,
            ):
                action(["python"])
                workflow.assert_called_once_with(
                    action.__name__.removesuffix("_main"), "python", False
                )

    def test_default_python_build_requires_container(self):
        with (
            mock.patch.object(python_workflow, "compiler_cache_environment", return_value={}),
            mock.patch.object(python_workflow, "find_prebuilt_dependency", return_value=None),
            mock.patch.object(python_workflow, "run") as run,
            mock.patch.dict(os.environ, {"CMAKE_BUILD_PARALLEL_LEVEL": "2"}),
        ):
            python_workflow.python_workflow("build", "python", False)
        self.assertEqual(
            run.call_args.args[0][:3], ["python3", "python/scripts/workflow.py", "build"]
        )
        self.assertEqual(run.call_args.kwargs["container_stage"], "native")
        self.assertEqual(run.call_args.kwargs["environment"]["CMAKE_BUILD_PARALLEL_LEVEL"], "2")

    def test_macos_target_rejects_other_hosts(self):
        with (
            mock.patch.object(python_workflow.platform, "system", return_value="Linux"),
            self.assertRaises(SystemExit),
        ):
            python_workflow.python_workflow("build", "python-macos", False)
