import os
from pathlib import Path
import tempfile
import unittest
from unittest import mock

from infra import execution


class ExecutionTest(unittest.TestCase):
    def test_add_github_git_auth_consumes_secret_header(self):
        environment = {
            "GIT_CONFIG_COUNT": "1",
            "GIT_CONFIG_KEY_0": "safe.directory",
            "SPENCH_GITHUB_AUTH_HEADER": "AUTHORIZATION: basic secret",
        }
        execution.add_github_git_auth(environment)
        self.assertNotIn("SPENCH_GITHUB_AUTH_HEADER", environment)
        self.assertEqual(environment["GIT_CONFIG_COUNT"], "2")
        self.assertEqual(environment["GIT_CONFIG_KEY_1"],
                         "http.https://github.com/.extraheader")
        self.assertEqual(environment["GIT_CONFIG_VALUE_1"], "AUTHORIZATION: basic secret")

    def test_add_github_git_auth_rejects_invalid_count(self):
        environment = {
            "GIT_CONFIG_COUNT": "invalid",
            "SPENCH_GITHUB_AUTH_HEADER": "secret",
        }
        with self.assertRaisesRegex(SystemExit, "must be an integer"):
            execution.add_github_git_auth(environment)

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
                 mock.patch.object(execution, "ensure_container_image", return_value="spench:test"), \
                 mock.patch.object(execution.subprocess, "run", return_value=completed) as subprocess_run, \
                 mock.patch.object(execution.os, "getuid", return_value=1000), \
                 mock.patch.object(execution.os, "getgid", return_value=1000):
                execution.run(["cmake", "--version"], container_stage="native",
                              workdir=root, environment={"KEY": "value"})
        command = subprocess_run.call_args.args[0]
        self.assertEqual(command[:3], ["docker", "run", "--rm"])
        self.assertIn("spench:test", command)
        self.assertIn("KEY=value", command)
        self.assertEqual(command[-2:], ["cmake", "--version"])


if __name__ == "__main__":
    unittest.main()
