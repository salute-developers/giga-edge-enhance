import io
import unittest
from unittest import mock

from infra import identity


class IdentityTest(unittest.TestCase):
    def test_cache_target_maps_cross_platform_presets(self):
        cases = {
            "android-release-tflite-arm64-v8a": ("android", "arm64-v8a"),
            "macos-x86_64-release-onnx": ("macos", "x86_64"),
            "macos-arm64-release-tflite": ("macos", "arm64"),
            "windows-x64-release-onnx": ("windows", "x86_64"),
            "wasm-release": ("emscripten", "wasm32"),
            "ios-device-release": ("ios", "arm64"),
            "ios-simulator-release": ("ios", "arm64-x86_64"),
        }
        for preset, expected in cases.items():
            with self.subTest(preset=preset):
                self.assertEqual(identity.cache_target(preset, False), expected)

    def test_cache_target_normalizes_host_architecture(self):
        with mock.patch.object(identity.platform, "system", return_value="Linux"), \
             mock.patch.object(identity.platform, "machine", return_value="aarch64"):
            self.assertEqual(identity.cache_target("native-release-onnx", True),
                             ("linux", "arm64"))

    def test_engine_requirements_and_linkage(self):
        self.assertEqual(identity.engine_requirement("native-asan-onnx"), ("onnx", "Debug"))
        self.assertEqual(identity.engine_requirement("native-release-tflite"),
                         ("tflite", "Release"))
        self.assertIsNone(identity.engine_requirement("native-model-packer"))
        self.assertEqual(identity.bundle_linkage("ios-device-release"), "static")
        self.assertEqual(identity.bundle_linkage("native-release-static-onnx"), "static")
        self.assertEqual(identity.bundle_linkage("native-release-onnx"), "shared")

    def test_dependency_tag_contains_concrete_target_dimensions(self):
        with mock.patch.object(identity, "dependency_bundle_digest", return_value="0123456789abcdef"):
            tag = identity.dependency_tag("windows-x64-release-tflite", False)
        self.assertEqual(tag, "tflite-windows-x86-64-release-shared-0123456789abcdef")

    def test_dependency_tag_rejects_target_without_engine(self):
        with self.assertRaisesRegex(SystemExit, "does not use a prebuilt"):
            identity.dependency_tag("native-model-packer", False)

    def test_cache_key_output_has_stable_contract(self):
        output = io.StringIO()
        with mock.patch.object(identity, "dependency_cache_digest", return_value="abc123"), \
             mock.patch.object(identity, "cache_target", return_value=("linux", "x86_64")), \
             mock.patch.object(identity.platform, "system", return_value="Linux"), \
             mock.patch("sys.stdout", output):
            identity.print_cache_keys("native-release-onnx", False)
        self.assertEqual(output.getvalue().splitlines(), [
            "source_cache_path=out/cache/cpm",
            "source_cache_key=ges-sources-v2-linux-abc123",
            "source_cache_restore_key=ges-sources-v2-linux-",
            "compiler_cache_path=out/cache/compiler",
            "compiler_cache_key_prefix=ges-compiler-v1-linux-x86_64-native-release-onnx-abc123",
            "compiler_cache_restore_key=ges-compiler-v1-linux-x86_64-native-release-onnx-",
        ])

    def test_source_cache_key_normalizes_macos(self):
        output = io.StringIO()
        with mock.patch.object(identity, "dependency_cache_digest", return_value="abc123"), \
             mock.patch.object(identity, "cache_target", return_value=("ios", "arm64")), \
             mock.patch.object(identity.platform, "system", return_value="Darwin"), \
             mock.patch("sys.stdout", output):
            identity.print_cache_keys("ios-device-release", False)
        self.assertIn("source_cache_key=ges-sources-v2-macos-abc123",
                      output.getvalue().splitlines())

    def test_invalid_engine_and_container_stage_fail_clearly(self):
        with self.assertRaisesRegex(ValueError, "Unsupported dependency bundle engine"):
            identity.engine_git_tag("missing")
        with self.assertRaisesRegex(ValueError, "Unsupported container stage"):
            identity.container_image_digest("missing")


if __name__ == "__main__":
    unittest.main()
