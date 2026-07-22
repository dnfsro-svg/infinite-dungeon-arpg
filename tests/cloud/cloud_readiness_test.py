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
        try:
            return json.loads(preset_path.read_text(encoding="utf-8"))
        except json.JSONDecodeError as error:
            self.fail(
                "Invalid JSON in CMakePresets.json: "
                f"{error.msg} (line {error.lineno}, column {error.colno})"
            )

    def _preset_by_name(self, presets, section, name):
        for preset in presets.get(section, []):
            if preset.get("name") == name:
                return preset
        self.fail(f"Missing {section} preset: {name}")

    def _read_required_file(self, relative_path):
        path = REPOSITORY_ROOT / relative_path
        self.assertTrue(path.is_file(), f"Missing required cloud file: {relative_path}")
        return path.read_text(encoding="utf-8").lower()

    def _executable_shell_lines(self, script):
        return tuple(
            line.strip()
            for line in self._read_required_file(script).splitlines()
            if line.strip() and not line.lstrip().startswith("#")
        )

    def _workflow_jobs(self, workflow):
        jobs = {}
        current_job = None
        in_jobs = False

        for line in workflow.splitlines():
            if not in_jobs:
                if line == "jobs:":
                    in_jobs = True
                continue
            if line and not line[0].isspace():
                break
            is_job_header = (
                line.startswith("  ")
                and not line.startswith("   ")
                and line.strip().endswith(":")
            )
            if is_job_header:
                current_job = line.strip()[:-1]
                jobs[current_job] = []
            elif current_job is not None:
                jobs[current_job].append(line)

        return {name: "\n".join(lines) for name, lines in jobs.items()}

    def _job_using_runner(self, jobs, runner):
        runner_marker = f"runs-on: {runner}"
        for job in jobs.values():
            if runner_marker in job:
                return job
        self.fail(f"Missing jobs block with {runner_marker}")

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
            executable_lines = self._executable_shell_lines(script)
            for command in required_commands:
                self.assertTrue(
                    any(command in line for line in executable_lines),
                    f"{script} must run: {command}",
                )

    def test_ci_workflow_runs_ubuntu_core_tests_and_windows_full_build(self):
        workflow = self._read_required_file(".github/workflows/build-and-test.yml")
        jobs = self._workflow_jobs(workflow)
        ubuntu_job = self._job_using_runner(jobs, "ubuntu-latest")
        windows_job = self._job_using_runner(jobs, "windows-latest")

        self.assertIn(LINUX_CORE_PRESET, ubuntu_job)
        self.assertIn(f"ctest --preset {LINUX_CORE_PRESET}", ubuntu_job)
        self.assertIn("scripts/test.ps1", windows_job)
        self.assertIn("windows-msvc-debug", windows_job)
        has_release_build_script = (
            "scripts/build.ps1" in windows_job
            and "windows-msvc-release" in windows_job
        )
        has_release_cmake_build = (
            "cmake --build --preset windows-msvc-release" in windows_job
        )
        self.assertTrue(
            has_release_build_script or has_release_cmake_build,
            "Windows job must include a Windows release full-build command",
        )


if __name__ == "__main__":
    unittest.main()
