"""Codex Cloud migration readiness contract."""

import json
from pathlib import Path
import unittest


REPOSITORY_ROOT = Path(__file__).resolve().parents[2]
LINUX_CORE_PRESET = "linux-gcc-core-debug"


class CloudReadinessContractTest(unittest.TestCase):
    def _load_presets(self):
        preset_path = REPOSITORY_ROOT / "CMakePresets.json"
        self.assertTrue(preset_path.is_file(), f"Missing CMake presets: {preset_path}")
        return json.loads(preset_path.read_text(encoding="utf-8"))

    def _preset_by_name(self, presets, section, name):
        for preset in presets.get(section, []):
            if preset.get("name") == name:
                return preset
        self.fail(f"Missing {section} preset: {name}")

    def _read_required_file(self, relative_path):
        path = REPOSITORY_ROOT / relative_path
        self.assertTrue(path.is_file(), f"Missing required cloud file: {relative_path}")
        return path.read_text(encoding="utf-8").lower()

    def test_linux_core_debug_configure_preset_uses_required_core_settings(self):
        presets = self._load_presets()
        configure = self._preset_by_name(
            presets, "configurePresets", LINUX_CORE_PRESET
        )

        self.assertEqual("Ninja", configure.get("generator"))
        cache_variables = configure.get("cacheVariables", {})
        self.assertEqual("ON", cache_variables.get("BUILD_TESTING"))
        self.assertEqual("OFF", cache_variables.get("ARPG_BUILD_GRAPHICS"))
        self.assertEqual("Debug", cache_variables.get("CMAKE_BUILD_TYPE"))

    def test_linux_core_debug_has_matching_build_and_test_presets(self):
        presets = self._load_presets()
        build = self._preset_by_name(presets, "buildPresets", LINUX_CORE_PRESET)
        test = self._preset_by_name(presets, "testPresets", LINUX_CORE_PRESET)

        self.assertEqual(LINUX_CORE_PRESET, build.get("configurePreset"))
        self.assertEqual(LINUX_CORE_PRESET, test.get("configurePreset"))

    def test_cloud_setup_and_maintenance_run_configure_build_and_ctest(self):
        required_commands = (
            f"cmake --preset {LINUX_CORE_PRESET}",
            f"cmake --build --preset {LINUX_CORE_PRESET}",
            f"ctest --preset {LINUX_CORE_PRESET}",
        )

        for script in ("scripts/cloud/setup.sh", "scripts/cloud/maintenance.sh"):
            contents = self._read_required_file(script)
            for command in required_commands:
                self.assertIn(command, contents, f"{script} must run: {command}")

    def test_ci_workflow_runs_ubuntu_core_tests_and_windows_full_build(self):
        workflow = self._read_required_file(".github/workflows/build-and-test.yml")

        self.assertIn("ubuntu-latest", workflow)
        self.assertIn(LINUX_CORE_PRESET, workflow)
        self.assertIn("ctest --preset", workflow)
        self.assertIn("windows-latest", workflow)
        self.assertIn("windows-msvc-debug", workflow)


if __name__ == "__main__":
    unittest.main()
