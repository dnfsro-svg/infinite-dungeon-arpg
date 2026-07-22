"""Codex Cloud migration readiness contract."""

import json
from pathlib import Path
import re
import shlex
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
        return path.read_text(encoding="utf-8")

    def _shell_commands(self, script):
        """Parse one-command-per-line Bash scripts with shlex comments enabled."""
        commands = []
        for line_number, line in enumerate(
            self._read_required_file(script).splitlines(), start=1
        ):
            try:
                tokens = shlex.split(line, comments=True, posix=True)
            except ValueError as error:
                self.fail(f"Invalid Bash syntax in {script}:{line_number}: {error}")
            if tokens:
                commands.append(tuple(tokens))
        return tuple(commands)

    def _workflow_jobs(self, workflow):
        """Parse the supported YAML subset: jobs and two-space job headers."""
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

        return {name: tuple(lines) for name, lines in jobs.items()}

    def _job_using_runner(self, jobs, runner):
        """Find a job by its supported four-space `runs-on:` YAML key."""
        for job in jobs.values():
            for line in job:
                if not line.startswith("    runs-on:"):
                    continue
                runner_value = line.split(":", maxsplit=1)[1].strip()
                if runner_value.lower() == runner.lower():
                    return job
        self.fail(f"Missing jobs block with runs-on: {runner}")

    def _workflow_run_commands(self, job):
        """Extract inline and |/> block `run:` commands from the supported YAML."""
        commands = []
        line_index = 0
        while line_index < len(job):
            line = job[line_index]
            indentation = len(line) - len(line.lstrip())
            stripped = line.strip()
            if indentation < 4 or not stripped.startswith("run:"):
                line_index += 1
                continue

            run_value = stripped[len("run:"):].strip()
            if run_value not in ("|", ">", "|-", "|+", ">-", ">+"):
                if run_value:
                    commands.append(run_value)
                line_index += 1
                continue

            line_index += 1
            while line_index < len(job):
                block_line = job[line_index]
                block_indentation = len(block_line) - len(block_line.lstrip())
                if block_line.strip() and block_indentation <= indentation:
                    break
                if block_line.strip() and not block_line.lstrip().startswith("#"):
                    commands.append(block_line.strip())
                line_index += 1

        return tuple(commands)

    def _has_exact_tokens(self, commands, expected_tokens):
        expected = tuple(token.lower() for token in expected_tokens)
        return any(
            tuple(token.lower() for token in command) == expected
            for command in commands
        )

    def _has_run_command(self, commands, expected_command):
        expected = " ".join(expected_command.split()).lower().replace("\\", "/")
        return any(
            " ".join(command.split()).lower().replace("\\", "/") == expected
            for command in commands
        )

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

    def test_non_windows_gate_requires_gnu_or_clang_for_c_and_cxx(self):
        cmake = self._read_required_file("CMakeLists.txt")

        self.assertRegex(
            cmake,
            re.compile(
                r'if\(\s*NOT\s+CMAKE_C_COMPILER_ID\s+MATCHES\s+'
                r'"\^\(GNU\|Clang\)\$"'
                r'\s+OR\s+NOT\s+CMAKE_CXX_COMPILER_ID\s+MATCHES\s+'
                r'"\^\(GNU\|Clang\)\$"\s*\)'
            ),
            "Non-Windows gate must require GNU or Clang for both C and CXX",
        )
        self.assertIn(
            "Non-Windows core builds require GNU or Clang C and CXX compilers",
            cmake,
        )

    def test_linux_core_debug_excludes_only_windows_powershell_audio_tests(self):
        presets = self._load_presets()
        test = self._preset_by_name(presets, "testPresets", LINUX_CORE_PRESET)

        self.assertEqual(
            r"^(stage14\.audio_sources|stage15\.audio_sources|"
            r"stage15\.audio_sources_self_test)$",
            test.get("filter", {}).get("exclude", {}).get("name"),
        )

    def test_cloud_setup_and_maintenance_run_configure_build_and_ctest(self):
        setup_commands = self._shell_commands("scripts/cloud/setup.sh")
        maintenance_commands = self._shell_commands("scripts/cloud/maintenance.sh")
        expected_setup_commands = (
            ("cmake", "--preset", LINUX_CORE_PRESET, "--fresh"),
            ("cmake", "--build", "--preset", LINUX_CORE_PRESET),
            ("ctest", "--preset", LINUX_CORE_PRESET, "--no-tests=error"),
        )
        expected_maintenance_commands = (
            ("cmake", "--preset", LINUX_CORE_PRESET),
            ("cmake", "--build", "--preset", LINUX_CORE_PRESET),
            ("ctest", "--preset", LINUX_CORE_PRESET, "--no-tests=error"),
        )

        for command in expected_setup_commands:
            self.assertTrue(
                self._has_exact_tokens(setup_commands, command),
                f"scripts/cloud/setup.sh must run: {' '.join(command)}",
            )
        for command in expected_maintenance_commands:
            self.assertTrue(
                self._has_exact_tokens(maintenance_commands, command),
                f"scripts/cloud/maintenance.sh must run: {' '.join(command)}",
            )

    def test_ci_workflow_runs_ubuntu_core_tests_and_windows_full_build(self):
        workflow = self._read_required_file(".github/workflows/build-and-test.yml")
        jobs = self._workflow_jobs(workflow)
        ubuntu_job = self._job_using_runner(jobs, "ubuntu-latest")
        windows_job = self._job_using_runner(jobs, "windows-latest")
        ubuntu_commands = self._workflow_run_commands(ubuntu_job)
        windows_commands = self._workflow_run_commands(windows_job)

        self.assertTrue(
            self._has_run_command(ubuntu_commands, "bash scripts/cloud/setup.sh"),
            "Ubuntu job must run: bash scripts/cloud/setup.sh",
        )
        self.assertTrue(
            self._has_run_command(
                windows_commands,
                ".\\scripts\\Test.ps1 -Preset windows-msvc-debug",
            ),
            "Windows job must run: .\\scripts\\Test.ps1 -Preset windows-msvc-debug",
        )
        self.assertTrue(
            self._has_run_command(
                windows_commands,
                ".\\scripts\\Build.ps1 -Preset windows-msvc-release",
            ),
            "Windows job must run: .\\scripts\\Build.ps1 -Preset windows-msvc-release",
        )


if __name__ == "__main__":
    unittest.main()
