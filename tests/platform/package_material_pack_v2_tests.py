from __future__ import annotations

import hashlib
import importlib.util
import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest
from unittest import mock


ROOT = Path(__file__).resolve().parents[2]
PACKAGER_PATH = ROOT / "tools" / "package_material_pack_v2.py"
SPEC = importlib.util.spec_from_file_location(
    "package_material_pack_v2", PACKAGER_PATH)
if SPEC is None or SPEC.loader is None:
    raise RuntimeError("cannot load material-pack v2 packager")
PACKAGER = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = PACKAGER
SPEC.loader.exec_module(PACKAGER)


def fake_png(width: int = 1920, height: int = 1080) -> bytes:
    ihdr = struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)
    return b"\x89PNG\r\n\x1a\n" + struct.pack(">I", 13) + b"IHDR" + ihdr + b"\0\0\0\0"


class PackageMaterialPackV2Tests(unittest.TestCase):
    def make_formal_fixture(self, root: Path) -> Path:
        validator = root / "tests/platform/stage12_material_validator.ps1"
        validator.parent.mkdir(parents=True)
        validator.write_text("Write-Output 'fixture validator'\n", encoding="utf-8")

        evidence = root.joinpath(*Path(PACKAGER.EVIDENCE_DIRECTORY).parts)
        evidence.mkdir(parents=True)
        report_lines = [
            "result=pass",
            "bundled_font_source_base_size=96",
            "ui_text_solid_fill=pass",
            "ui_text_physical_scale=pass",
            "hud_real_font_bounds_1920=pass",
            "inventory_real_font_bounds_1920=pass",
            "skill_real_font_bounds_1920=pass",
            "pause_real_font_bounds_1920=pass",
        ]
        for field, filename in PACKAGER.FORMAL_PREVIEW_FIELDS:
            report_lines.append(f"{field}={filename}")
            (evidence / filename).write_bytes(fake_png())
        (evidence / PACKAGER.EVIDENCE_FILE).write_text(
            "\n".join(report_lines) + "\n", encoding="utf-8")
        (evidence / "nested").mkdir()
        (evidence / "nested/validator-detail.txt").write_text(
            "full formal detail\n", encoding="utf-8")
        return evidence

    def test_formal_validation_runs_powershell_and_binds_preview_hashes(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            evidence = self.make_formal_fixture(root)
            completed = subprocess.CompletedProcess([], 0, "validated\n", "")
            with mock.patch.object(
                    PACKAGER.subprocess, "run", return_value=completed) as run:
                snapshot = PACKAGER.validate_formal_evidence(root)

            command = run.call_args.args[0]
            self.assertEqual(command[0].lower(), "powershell.exe")
            self.assertIn(str(root / "tests/platform/stage12_material_validator.ps1"), command)
            self.assertIn(str(evidence), command)
            self.assertTrue(run.call_args.kwargs["check"])
            self.assertEqual(run.call_args.kwargs["cwd"], root)

            bindings = {item.evidence_field: item for item in snapshot.previews}
            self.assertEqual(set(bindings), {
                field for field, _ in PACKAGER.FORMAL_PREVIEW_FIELDS
            })
            for field, filename in PACKAGER.FORMAL_PREVIEW_FIELDS:
                binding = bindings[field]
                self.assertEqual(binding.filename, filename)
                self.assertEqual(
                    binding.sha256,
                    hashlib.sha256((evidence / filename).read_bytes()).hexdigest())

    def test_formal_validation_rejects_wrong_report_preview_binding(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            evidence = self.make_formal_fixture(root)
            report = evidence / PACKAGER.EVIDENCE_FILE
            text = report.read_text(encoding="utf-8").replace(
                "hud_ui_screenshot_1920=ui-hud-1920x1080.png",
                "hud_ui_screenshot_1920=unrelated.png")
            report.write_text(text, encoding="utf-8")
            with mock.patch.object(PACKAGER.subprocess, "run") as run:
                with self.assertRaisesRegex(RuntimeError, "preview binding"):
                    PACKAGER.validate_formal_evidence(root)
            run.assert_not_called()

    def test_formal_validation_propagates_validator_failure(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            self.make_formal_fixture(root)
            failure = subprocess.CalledProcessError(7, ["powershell.exe"])
            with mock.patch.object(
                    PACKAGER.subprocess, "run", side_effect=failure):
                with self.assertRaises(subprocess.CalledProcessError):
                    PACKAGER.validate_formal_evidence(root)

    def test_full_formal_evidence_is_mapped_once_and_hashed(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            self.make_formal_fixture(root)
            with mock.patch.object(
                    PACKAGER.subprocess, "run",
                    return_value=subprocess.CompletedProcess([], 0, "", "")):
                snapshot = PACKAGER.validate_formal_evidence(root)
            records = PACKAGER.formal_evidence_metadata(snapshot)

            archive_paths = [item["archive_path"] for item in records["files"]]
            self.assertEqual(len(archive_paths), len(set(archive_paths)))
            self.assertIn(
                "previews/formal-evidence/nested/validator-detail.txt",
                archive_paths)
            self.assertEqual(records["file_count"], 6)
            for item in records["files"]:
                self.assertRegex(item["sha256"], r"^[0-9a-f]{64}$")

            entries: dict[str, object] = {}
            PACKAGER.add_formal_evidence_entries(entries, root, snapshot)
            self.assertEqual(set(entries), set(archive_paths))

    def test_formal_snapshot_rejects_preview_changed_after_validation(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            evidence = self.make_formal_fixture(root)
            with mock.patch.object(
                    PACKAGER.subprocess, "run",
                    return_value=subprocess.CompletedProcess([], 0, "", "")):
                snapshot = PACKAGER.validate_formal_evidence(root)
            entries: dict[str, object] = {}
            PACKAGER.add_formal_evidence_entries(entries, root, snapshot)
            (evidence / PACKAGER.PREVIEW_FILES[0]).write_bytes(
                fake_png() + b"changed")
            with self.assertRaisesRegex(RuntimeError, "changed after validation"):
                PACKAGER.validate_formal_inputs(entries, snapshot)

    def test_atomic_candidate_cleanup_and_no_overwrite(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            output = directory / "package.zip"

            def rejected(candidate: object) -> str:
                candidate.write(b"candidate")
                raise RuntimeError("verification failed")

            with self.assertRaisesRegex(RuntimeError, "verification failed"):
                PACKAGER.atomic_publish_verified(output, rejected)
            self.assertFalse(output.exists())
            self.assertEqual(list(directory.glob(".package.zip.*.tmp")), [])

            output.write_bytes(b"existing")
            with self.assertRaises(FileExistsError):
                PACKAGER.atomic_publish_verified(
                    output, lambda candidate: hashlib.sha256(b"new").hexdigest())
            self.assertEqual(output.read_bytes(), b"existing")
            self.assertEqual(list(directory.glob(".package.zip.*.tmp")), [])

            output.unlink()

            def raced(candidate: object) -> str:
                candidate.write(b"candidate")
                output.write_bytes(b"racer")
                return hashlib.sha256(b"candidate").hexdigest()

            with self.assertRaises(FileExistsError):
                PACKAGER.atomic_publish_verified(output, raced)
            self.assertEqual(output.read_bytes(), b"racer")
            self.assertEqual(list(directory.glob(".package.zip.*.tmp")), [])

    def test_atomic_publish_uses_verified_temporary_file(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            output = directory / "package.zip"

            expected = hashlib.sha256(b"verified").hexdigest()

            def verified(candidate: object) -> str:
                candidate.write(b"verified")
                self.assertFalse(output.exists())
                return expected

            self.assertEqual(
                PACKAGER.atomic_publish_verified(output, verified), expected)
            self.assertEqual(output.read_bytes(), b"verified")
            self.assertEqual(list(directory.glob(".package.zip.*.tmp")), [])

    def test_atomic_publish_rejects_replaced_temporary_candidate(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            output = directory / "package.zip"

            def replaced(candidate: object) -> str:
                candidate.write(b"verified")
                candidate.flush()
                temporary_path = next(directory.glob(".package.zip.*.tmp"))
                temporary_path.unlink()
                temporary_path.write_bytes(b"attacker")
                return hashlib.sha256(b"verified").hexdigest()

            # Windows denies deletion while the exclusive candidate handle is
            # open; POSIX permits it and the identity check rejects replacement.
            with self.assertRaises((OSError, RuntimeError)):
                PACKAGER.atomic_publish_verified(output, replaced)
            self.assertFalse(output.exists())
            self.assertEqual(list(directory.glob(".package.zip.*.tmp")), [])

    def test_transient_temp_unlink_failure_does_not_report_false_failure(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            output = directory / "package.zip"
            expected = hashlib.sha256(b"verified").hexdigest()
            real_unlink = Path.unlink
            failed_once = False

            def transient_unlink(path: Path, *args: object, **kwargs: object) -> None:
                nonlocal failed_once
                if path.name.startswith(".package.zip.") and not failed_once:
                    failed_once = True
                    raise PermissionError("transient cleanup failure")
                real_unlink(path, *args, **kwargs)

            def verified(candidate: object) -> str:
                candidate.write(b"verified")
                return expected

            with mock.patch.object(Path, "unlink", new=transient_unlink):
                self.assertEqual(
                    PACKAGER.atomic_publish_verified(output, verified), expected)
            self.assertTrue(failed_once)
            self.assertEqual(output.read_bytes(), b"verified")
            self.assertEqual(list(directory.glob(".package.zip.*.tmp")), [])

    def test_source_and_output_symlinks_are_rejected_when_supported(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            real_source = root / "real.txt"
            real_source.write_text("data", encoding="utf-8")
            source_link = root / "source-link.txt"
            real_output = root / "real-output"
            real_output.mkdir()
            output_link = root / "output-link"
            try:
                os.symlink(real_source, source_link)
                os.symlink(real_output, output_link, target_is_directory=True)
            except (OSError, NotImplementedError) as error:
                self.skipTest(f"symbolic links are unavailable: {error}")

            with self.assertRaisesRegex(RuntimeError, "reparse|symbolic"):
                PACKAGER.require_source(root, "source-link.txt")
            with self.assertRaisesRegex(RuntimeError, "reparse|symbolic"):
                PACKAGER.validate_output_destination(output_link / "package.zip")

    def test_nested_evidence_symlink_is_rejected_when_supported(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            evidence = self.make_formal_fixture(root)
            target = root / "outside.txt"
            target.write_text("outside", encoding="utf-8")
            try:
                os.symlink(target, evidence / "nested/evidence-link.txt")
            except (OSError, NotImplementedError) as error:
                self.skipTest(f"symbolic links are unavailable: {error}")
            with self.assertRaisesRegex(RuntimeError, "reparse|symbolic"):
                PACKAGER.validate_formal_evidence(root)

    def test_lstat_errors_are_not_treated_as_safe(self) -> None:
        path = mock.Mock()
        path.is_symlink.return_value = False
        path.is_junction.return_value = False
        path.lstat.side_effect = PermissionError("denied")
        with self.assertRaises(PermissionError):
            PACKAGER.path_is_reparse_point(path)


if __name__ == "__main__":
    unittest.main()
