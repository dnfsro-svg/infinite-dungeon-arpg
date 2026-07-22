"""Codex Cloud migration readiness contract."""

import json
from pathlib import Path
import re
import shlex
import unittest


REPOSITORY_ROOT = Path(__file__).resolve().parents[2]
LINUX_CORE_PRESET = "linux-gcc-core-debug"
CHECKOUT_SHA = "11bd71901bbe5b1630ceea73d27597364c9af683"
UPLOAD_ARTIFACT_SHA = "ea165f8d65b6e75b540449e92b4886f43607fa02"


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

    def _permissions_entries(self, workflow):
        """Read a simple top-level permissions mapping; reject unsupported YAML."""
        lines = workflow.splitlines()
        headers = [index for index, line in enumerate(lines) if line == "permissions:"]
        self.assertEqual(1, len(headers), "Workflow needs one top-level permissions block")

        entries = []
        for line in lines[headers[0] + 1 :]:
            stripped = line.strip()
            if not stripped or stripped.startswith("#"):
                continue
            if not line[0].isspace():
                break
            self.assertTrue(
                line.startswith("  ") and not line.startswith("   "),
                "Permissions entries must use a two-space mapping indentation",
            )
            self.assertRegex(
                stripped,
                r"^[A-Za-z][A-Za-z0-9_-]*:\s*[^#\s]+$",
                "Unsupported permissions entry syntax",
            )
            entries.append(stripped)
        self.assertTrue(entries, "Permissions block must not be empty")
        return tuple(entries)

    def _workflow_uses(self, workflow):
        """Extract only real YAML uses nodes; comments and other syntax are ignored."""
        uses = []
        for line in workflow.splitlines():
            match = re.match(
                r"^\s*(?:-\s+)?uses:\s*([^\s#]+)\s*(?:#.*)?$", line
            )
            if match:
                uses.append(match.group(1))
        return tuple(uses)

    def _assert_pinned_actions(self, workflow):
        uses = self._workflow_uses(workflow)
        self.assertTrue(uses, "Workflow must contain real uses nodes")
        for action in uses:
            self.assertNotIn("@v4", action, "Movable @v4 action refs are forbidden")
            self.assertRegex(
                action,
                r"^actions/[A-Za-z0-9_.-]+@[0-9a-f]{40}$",
                f"Action ref must use a 40-character SHA: {action}",
            )

        checkout = [action for action in uses if action.startswith("actions/checkout@")]
        upload = [
            action for action in uses if action.startswith("actions/upload-artifact@")
        ]
        self.assertEqual(
            (f"actions/checkout@{CHECKOUT_SHA}",) * 2,
            tuple(checkout),
            "Each checkout node must use the approved SHA",
        )
        self.assertEqual(
            (f"actions/upload-artifact@{UPLOAD_ARTIFACT_SHA}",),
            tuple(upload),
            "The upload-artifact node must use the approved SHA",
        )

    def _without_powershell_line_comments(self, script):
        """Strip # comments outside quoted strings; reject unterminated quoted strings."""
        cleaned = []
        for line in script.splitlines():
            quote = None
            output = []
            for character in line:
                if character in "'\"":
                    if quote is None:
                        quote = character
                    elif quote == character:
                        quote = None
                    output.append(character)
                elif character == "#" and quote is None:
                    break
                else:
                    output.append(character)
            self.assertIsNone(quote, "Unsupported unterminated PowerShell string")
            cleaned.append("".join(output).rstrip())
        return tuple(cleaned)

    def _vswhere_args_tokens(self, script):
        """Parse the unique string-only vswhere array; unsupported PowerShell fails closed."""
        lines = self._without_powershell_line_comments(script)
        starts = [
            index
            for index, line in enumerate(lines)
            if re.fullmatch(r"\s*\$vswhereArgs\s*=\s*@\(\s*", line)
        ]
        self.assertEqual(1, len(starts), "Expected one $vswhereArgs array")

        values = []
        end = None
        for index in range(starts[0] + 1, len(lines)):
            line = lines[index].strip()
            if line == ")":
                end = index
                break
            self.assertIsNotNone(
                re.fullmatch(r"(?:'[^']*'\s*,?\s*)+", line),
                "vswhere args must be a string-only array with a standalone closing parenthesis",
            )
            values.extend(re.findall(r"'([^']*)'", line))
        self.assertIsNotNone(end, "Missing closing parenthesis for $vswhereArgs")
        return tuple(values), lines

    def _assert_vswhere_contract(self, script):
        values, lines = self._vswhere_args_tokens(script)
        self.assertNotIn(
            "Microsoft.VisualStudio.Product.BuildTools",
            values,
            "vswhere must not be restricted to Build Tools",
        )
        self.assertEqual("*", values[values.index("-products") + 1])
        self.assertEqual("[17.0,18.0)", values[values.index("-version") + 1])
        requires = values[values.index("-requires") + 1 : values.index("-property")]
        self.assertIn("Microsoft.VisualStudio.Component.VC.Tools.x86.x64", requires)
        self.assertIn("Microsoft.VisualStudio.Component.Windows11SDK.26100", requires)

        install_commands = [
            line
            for line in lines
            if re.fullmatch(
                r"\s*\$installPath\s*=\s*&\s+\$vswhere\s+@vswhereArgs\s*",
                line,
            )
        ]
        self.assertEqual(
            1,
            len(install_commands),
            "$installPath must be assigned by exactly one & $vswhere @vswhereArgs command",
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
        windows_job = self._job_using_runner(jobs, "windows-2022")
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

    def test_ci_workflow_pins_permissions_and_official_actions(self):
        workflow = self._read_required_file(".github/workflows/build-and-test.yml")

        self.assertEqual(("contents: read",), self._permissions_entries(workflow))
        self._assert_pinned_actions(workflow)

        comment_decoy = workflow.replace(
            "permissions:\n", "# permissions:\n", 1
        )
        with self.assertRaises(AssertionError):
            self._permissions_entries(comment_decoy)
        action_rollback = workflow.replace(CHECKOUT_SHA, "v4", 1)
        action_rollback += f"\n# - uses: actions/checkout@{CHECKOUT_SHA}\n"
        with self.assertRaises(AssertionError):
            self._assert_pinned_actions(action_rollback)

    def test_configure_accepts_all_vs_2022_products_with_required_components(self):
        script = self._read_required_file("scripts/Configure.ps1")

        self._assert_vswhere_contract(script)

        products_rollback = script.replace(
            "'-products', '*'",
            "'-products', 'Microsoft.VisualStudio.Product.BuildTools'",
            1,
        )
        products_rollback += "\n# '-products', '*'\n"
        with self.assertRaises(AssertionError):
            self._assert_vswhere_contract(products_rollback)


if __name__ == "__main__":
    unittest.main()
