import hashlib
import io
import json
from pathlib import Path
import tarfile
import tempfile
import unittest
from unittest import mock

from infra import dependencies


class DependenciesTest(unittest.TestCase):
    def test_file_sha256_and_checksum_manifest(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "nested").mkdir()
            (root / "b.txt").write_text("second")
            (root / "nested/a.txt").write_text("first")
            dependencies.write_artifact_checksums(root)
            lines = (root / "checksums.sha256").read_text().splitlines()
            expected_a = hashlib.sha256(b"first").hexdigest()
            expected_b = hashlib.sha256(b"second").hexdigest()
            self.assertEqual(lines, [
                f"{expected_b}  b.txt",
                f"{expected_a}  nested/a.txt",
            ])

    def test_artifact_lock_rejects_concurrent_writer_and_cleans_up(self):
        with tempfile.TemporaryDirectory(dir=dependencies.ROOT) as directory:
            root = Path(directory) / "artifact"
            with dependencies.artifact_lock(root):
                with self.assertRaisesRegex(SystemExit, "already running"):
                    with dependencies.artifact_lock(root):
                        pass
            self.assertFalse(root.with_name("artifact.lock").exists())

    def test_restore_rejects_path_traversal_before_extracting(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            archive = root / "dependency.tar.gz"
            with tarfile.open(archive, "w:gz") as bundle:
                member = tarfile.TarInfo("../escaped")
                member.size = 4
                bundle.addfile(member, io.BytesIO(b"data"))
            Path(f"{archive}.sha256").write_text(
                f"{dependencies.file_sha256(archive)}  {archive.name}\n")
            with mock.patch.object(dependencies, "dependency_artifact_for_target",
                                   return_value=root / "artifact"):
                with self.assertRaisesRegex(SystemExit, "Unsafe path"):
                    dependencies.restore_dependency_bundle(
                        "native-release-onnx", False, archive)
            self.assertFalse((root / "escaped").exists())

    def test_write_shared_package_config_is_relocatable(self):
        with tempfile.TemporaryDirectory() as directory:
            prefix = Path(directory)
            library = prefix / "lib/libonnxruntime.so"
            library.parent.mkdir()
            library.touch()
            dependencies.write_engine_package_config(prefix, "onnx", library, "shared")
            config = (prefix / "lib/cmake/onnxruntime/onnxruntimeConfig.cmake").read_text()
            self.assertIn("onnxruntime::onnxruntime", config)
            self.assertIn("${_ges_ort_prefix}/lib/libonnxruntime.so", config)
            self.assertNotIn(str(prefix), config)

    def test_prebuilt_arguments_use_container_path(self):
        prefix = dependencies.ROOT / "out/artifacts/dependencies/test/prefix"
        with mock.patch.object(dependencies, "engine_requirement",
                               return_value=("tflite", "Release")):
            arguments = dependencies.prebuilt_cmake_arguments(
                "native-release-tflite", False, prefix, True)
        self.assertEqual(arguments, [
            "-DGES_PREBUILT_TFLITE_ROOT=/workspace/out/artifacts/dependencies/test/prefix"
        ])

    def test_prepare_dependency_builds_renamed_bundle_target(self):
        with tempfile.TemporaryDirectory(dir=dependencies.ROOT) as directory:
            artifact_root = Path(directory) / "artifact"
            with (
                mock.patch.object(dependencies, "engine_requirement",
                                  return_value=("onnx", "Release")),
                mock.patch.object(dependencies, "dependency_artifact_root",
                                  return_value=artifact_root),
                mock.patch.object(dependencies, "dependency_manifest", return_value={}),
                mock.patch.object(dependencies, "compiler_cache_environment", return_value={}),
                mock.patch.object(dependencies, "cache_target", return_value=("linux", "x86_64")),
                mock.patch.object(dependencies, "bundle_linkage", return_value="shared"),
                mock.patch.object(dependencies, "artifact_preset",
                                  return_value="native-release-onnx"),
                mock.patch.object(dependencies, "copy_engine_library",
                                  return_value=Path("libonnxruntime.so")),
                mock.patch.object(dependencies, "write_engine_package_config"),
                mock.patch.object(dependencies, "copy_onnx_headers"),
                mock.patch.object(dependencies, "write_dependency_bundle"),
                mock.patch.object(dependencies, "run") as run_mock,
            ):
                dependencies.prepare_dependency("native-release-onnx", False)

            build_command = run_mock.call_args_list[1].args[0]
            self.assertIn("ges_dependency_bundle", build_command)
            self.assertNotIn("spench_dependency_bundle", build_command)


if __name__ == "__main__":
    unittest.main()
