import os
from pathlib import Path
import tempfile
import unittest
from unittest import mock

from infra import execution


class ExecutionTest(unittest.TestCase):
    def test_host_run_passes_workdir_and_environment(self):
        with tempfile.TemporaryDirectory() as directory, \
             mock.patch.object(execution.subprocess, "run") as subprocess_run, \
             mock.patch.dict(os.environ, {"BASE": "value"}, clear=True):
            execution.run(["command", "argument"], workdir=Path(directory),
                          environment={"EXTRA": "added"})
        subprocess_run.assert_called_once()
        args, kwargs = subprocess_run.call_args
        self.assertEqual(args[0], ["command", "argument"])
        self.assertEqual(kwargs["cwd"], Path(directory))
        self.assertTrue(kwargs["check"])
        self.assertEqual(kwargs["env"]["BASE"], "value")
        self.assertEqual(kwargs["env"]["EXTRA"], "added")

    def test_interactive_run_requires_terminal(self):
        with mock.patch.object(execution.sys.stdin, "isatty", return_value=False):
            with self.assertRaisesRegex(SystemExit, "requires an interactive terminal"):
                execution.run(["bash"], interactive=True)

    def test_container_run_builds_isolated_docker_command(self):
        completed = mock.Mock(returncode=0)
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            with mock.patch.object(execution, "ROOT", root), \
                 mock.patch.object(execution, "ensure_container_image", return_value="gigaedge-enhance:test"), \
                 mock.patch.object(execution.subprocess, "run", return_value=completed) as subprocess_run, \
                 mock.patch.object(execution.os, "getuid", return_value=1000), \
                 mock.patch.object(execution.os, "getgid", return_value=1000):
                execution.run(["cmake", "--version"], container_stage="native",
                              workdir=root, environment={"KEY": "value"})
        command = subprocess_run.call_args.args[0]
        self.assertEqual(command[:3], ["docker", "run", "--rm"])
        self.assertIn("gigaedge-enhance:test", command)
        self.assertIn("KEY=value", command)
        self.assertIn("GIT_CONFIG_COUNT=1", command)
        self.assertIn("GIT_CONFIG_KEY_0=safe.directory", command)
        self.assertIn("GIT_CONFIG_VALUE_0=*", command)
        self.assertEqual(command[-2:], ["cmake", "--version"])

    def test_container_run_preserves_build_resource_limits(self):
        with tempfile.TemporaryDirectory() as directory, \
             mock.patch.object(execution, "ROOT", Path(directory)), \
             mock.patch.object(execution, "ensure_container_image", return_value="test"), \
             mock.patch.object(execution.subprocess, "run") as subprocess_run, \
             mock.patch.dict(os.environ, {"CMAKE_BUILD_PARALLEL_LEVEL": "2", "EMCC_CORES": "1"}):
            execution.run(["cmake", "--build", "."], container_stage="web",
                          workdir=Path(directory))
        command = subprocess_run.call_args.args[0]
        self.assertIn("CMAKE_BUILD_PARALLEL_LEVEL=2", command)
        self.assertIn("EMCC_CORES=1", command)

    def test_compiler_cache_sizes_honor_user_limits(self):
        with tempfile.TemporaryDirectory() as directory, \
             mock.patch.object(execution, "ROOT", Path(directory)), \
             mock.patch.dict(os.environ, {"CCACHE_MAXSIZE": "256M", "SCCACHE_CACHE_SIZE": "128M"}):
            environment = execution.compiler_cache_environment(True)
        self.assertEqual(environment["CCACHE_MAXSIZE"], "256M")
        self.assertEqual(environment["SCCACHE_CACHE_SIZE"], "128M")


if __name__ == "__main__":
    unittest.main()
