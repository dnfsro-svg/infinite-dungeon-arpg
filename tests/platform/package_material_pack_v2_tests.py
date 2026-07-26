from __future__ import annotations

import hashlib
import importlib.util
import json
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


ECOLOGIES = ("fire", "water", "lightning", "chaos")
EXPECTED_NATIVE_PREVIEW_FIELDS = tuple(
    (
        f"{ecology}_{view}_screenshot_{short_resolution}",
        f"{ecology}-{view.replace('_', '-')}-{full_resolution}.png",
    )
    for ecology in ECOLOGIES
    for view in ("background_only", "gameplay")
    for short_resolution, full_resolution in (
        ("1280", "1280x720"),
        ("1920", "1920x1080"),
    )
)
EXPECTED_TASK10_PLAN_FILES = {
    "docs/superpowers/specs/2026-07-25-material-runtime-integration-repair-design.md",
    "docs/superpowers/plans/2026-07-25-material-runtime-integration-repair.md",
    "docs/validation/material-runtime-integration-repair.md",
}
EXPECTED_TASK10_INTEGRATION_FILES = {
    "src/platform/raylib/material_residency.hpp",
    "src/platform/raylib/material_residency.cpp",
    "src/platform/raylib/material_pack.hpp",
    "src/platform/raylib/material_pack.cpp",
    "src/platform/raylib/material_asset_types.hpp",
    "src/platform/raylib/material_animation.hpp",
    "src/platform/raylib/material_animation.cpp",
    "src/platform/raylib/monster_material_presenter.hpp",
    "src/platform/raylib/monster_material_presenter.cpp",
    "src/platform/raylib/environment_prop_layout.hpp",
    "src/platform/raylib/environment_prop_layout.cpp",
    "src/platform/raylib/active_skill_assets.hpp",
    "src/platform/raylib/active_skill_assets.cpp",
    "src/platform/raylib/host_input.cpp",
    "src/platform/raylib/raylib_input.hpp",
    "src/platform/raylib/raylib_input.cpp",
    "tools/build_environment_props.py",
    "tools/build_active_skill_material_maps.py",
    "tests/platform/environment_prop_asset_pipeline_tests.py",
    "tests/platform/active_skill_material_asset_pipeline_tests.py",
    "tests/platform/stage17_skill_stones_game_validation.cpp",
    "tests/platform/stage17_skill_stones_validator.ps1",
    "tests/platform/package_material_pack_v2_tests.py",
    "tools/package_material_pack_v2.py",
}
EXPECTED_TASK10_REQUIRED_ASSETS = {
    "assets/stage12/element_doors.png",
    "assets/stage12/element_doors_material.png",
    "assets/skills/draw_slash_atlas_material.png",
    "assets/skills/storm_swords_atlas_material.png",
    "assets/stage12/environment-props-build.json",
}


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
            "full_pack_bytes=329430304",
            "resident_peak_bytes=226800928",
            "transition_peak_bytes=261010720",
            "bundled_font_source_base_size=96",
            "ui_text_solid_fill=pass",
            "ui_text_physical_scale=pass",
            "hud_real_font_bounds_1920=pass",
            "inventory_real_font_bounds_1920=pass",
            "skill_real_font_bounds_1920=pass",
            "pause_real_font_bounds_1920=pass",
        ]
        provenance: dict[str, object] = {"schema_version": 1, "ecologies": {}}
        for ecology in ECOLOGIES:
            master_path = root / (
                f"art_source/stage12/backgrounds/{ecology}/"
                f"{ecology}-room-background-master.png")
            runtime_path = root / f"assets/stage12/{ecology}_room_background.png"
            material_path = root / (
                f"assets/stage12/{ecology}_room_background_material.png")
            outputs = {
                "master": (master_path, fake_png(3840, 2160) + ecology.encode()),
                "runtime": (runtime_path, fake_png(2560, 1440) + ecology.encode()),
                "material": (material_path, fake_png(2560, 1440) + ecology.encode() + b"m"),
            }
            output_hashes: dict[str, str] = {}
            for kind, (path, data) in outputs.items():
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(data)
                output_hashes[kind] = hashlib.sha256(data).hexdigest()
            provenance["ecologies"][ecology] = {
                "ecology": ecology,
                "master_size": [3840, 2160],
                "runtime_size": [2560, 1440],
                "output_sha256": output_hashes,
            }
            report_lines.extend((
                f"{ecology}_background_atlas=assets/stage12/{ecology}_room_background.png",
                f"{ecology}_background_material_atlas=assets/stage12/{ecology}_room_background_material.png",
                f"{ecology}_background_master=art_source/stage12/backgrounds/{ecology}/{ecology}-room-background-master.png",
                f"{ecology}_background_provenance=assets/stage12/room-background-build.json",
                f"{ecology}_background_source=2560x1440",
                f"{ecology}_background_scale_1280=1/2",
                f"{ecology}_background_scale_1920=3/4",
                f"{ecology}_background_runtime_sha256={output_hashes['runtime']}",
                f"{ecology}_background_material_sha256={output_hashes['material']}",
                f"{ecology}_background_master_sha256={output_hashes['master']}",
            ))
        provenance_path = root / PACKAGER.BACKGROUND_PROVENANCE
        provenance_path.parent.mkdir(parents=True, exist_ok=True)
        provenance_path.write_text(
            json.dumps(provenance, sort_keys=True), encoding="utf-8")
        for field, filename in PACKAGER.FORMAL_PREVIEW_FIELDS:
            report_lines.append(f"{field}={filename}")
            dimensions = (1280, 720) if "1280x720" in filename else (1920, 1080)
            (evidence / filename).write_bytes(fake_png(*dimensions))
        (evidence / PACKAGER.EVIDENCE_FILE).write_text(
            "\n".join(report_lines) + "\n", encoding="utf-8")
        (evidence / "material-runtime-integration-evidence.txt").write_text(
            "\n".join((
                "schema=material-runtime-integration-v1",
                "fixture_path=production-room-renderer",
                "showcase_capture_count=0",
                "graphics_context=pass",
                "renderer_initialized=pass",
                "full_pack_bytes=329430304",
                "resident_peak_bytes=226800928",
                "transition_peak_bytes=261010720",
                "observed_resident_peak_bytes=226800928",
                "cross_ecology=pass",
                "door_sprite_unique=pass",
                "environment=pass",
                "death=pass",
                "draw_slash_frame_union=0..35",
                "storm_swords_frame_union=0..23",
                "healthy_skill_suppression=pass",
                "missing_map_fallback=pass",
                "stress_warmup_frames=300",
                "stress_measured_frames=1800",
                "screenshot_count=70",
                "capture_prime_count=70",
                "screenshot_pixel_guard=pass",
                "screenshot_nonblack_failures=0",
                "scene_sentinel_failures=0",
                "result=pass",
            )) + "\n", encoding="utf-8")
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

    def test_native_background_previews_cover_every_ecology_view_and_resolution(
            self) -> None:
        self.assertEqual(
            PACKAGER.NATIVE_BACKGROUND_PREVIEW_FIELDS,
            EXPECTED_NATIVE_PREVIEW_FIELDS)
        self.assertEqual(
            PACKAGER.FORMAL_PREVIEW_FIELDS[-len(EXPECTED_NATIVE_PREVIEW_FIELDS):],
            EXPECTED_NATIVE_PREVIEW_FIELDS)

        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            evidence = self.make_formal_fixture(root)
            completed = subprocess.CompletedProcess([], 0, "validated\n", "")
            with mock.patch.object(
                    PACKAGER.subprocess, "run", return_value=completed):
                snapshot = PACKAGER.validate_formal_evidence(root)

        bindings = {item.evidence_field: item for item in snapshot.previews}
        for field, filename in EXPECTED_NATIVE_PREVIEW_FIELDS:
            self.assertEqual(bindings[field].filename, filename)
            dimensions = (1280, 720) if "1280x720" in filename else (1920, 1080)
            self.assertEqual(
                bindings[field].sha256,
                hashlib.sha256(fake_png(*dimensions)).hexdigest())

    def test_formal_preview_dimensions_accept_native_720p_and_1080p(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            evidence = self.make_formal_fixture(root)
            completed = subprocess.CompletedProcess([], 0, "validated\n", "")
            with mock.patch.object(
                    PACKAGER.subprocess, "run", return_value=completed):
                snapshot = PACKAGER.validate_formal_evidence(root)
            entries: dict[str, object] = {}
            PACKAGER.add_formal_evidence_entries(entries, root, snapshot)
            PACKAGER.validate_formal_inputs(entries, snapshot)

            wrong = evidence / "fire-background-only-1280x720.png"
            wrong.write_bytes(fake_png(1920, 1080))
            with self.assertRaisesRegex(RuntimeError, "dimensions|changed"):
                PACKAGER.validate_formal_inputs(entries, snapshot)

    def test_v3_name_and_legacy_archives_are_immutable(self) -> None:
        self.assertEqual(
            PACKAGER.PACKAGE_NAME,
            "arpg-material-pack-v3-native-backgrounds.zip")
        self.assertEqual(PACKAGER.PACKAGE_VERSION, "3.0.0-native-backgrounds")
        self.assertEqual(PACKAGER.LEGACY_PACKAGE_SHA256, {
            "deliverables/arpg-material-pack-v1.zip":
                "b61c54be14e99fc083d4ea5d665ec24d899e565c835e077926ce46cd86d0f2a4",
            "deliverables/arpg-material-pack-v2-highres-text.zip":
                "3c1c47de0e20fc3c4e97806cd2719ff44357563abf3d8c5d9b4bebfe301e6d1c",
        })

    def test_v3_integration_references_include_renderer_and_formal_stack_guard(
            self) -> None:
        required_sources = {
            "src/platform/raylib/combat_renderer.hpp",
            "src/platform/raylib/combat_renderer.cpp",
            "tests/platform/CMakeLists.txt",
        }
        self.assertTrue(required_sources.issubset(PACKAGER.INTEGRATION_FILES))

        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            for relative in (
                    PACKAGER.STATIC_FONT,
                    *PACKAGER.PLAN_FILES,
                    *PACKAGER.INTEGRATION_FILES,
                    *EXPECTED_TASK10_REQUIRED_ASSETS):
                source = root / relative
                source.parent.mkdir(parents=True, exist_ok=True)
                source.write_bytes(b"integration fixture\n")
            snapshot = PACKAGER.FormalEvidenceSnapshot(
                directory=root,
                report_sha256="0" * 64,
                files=(),
                previews=(),
            )
            with mock.patch.object(
                    PACKAGER, "git_tracked_material_paths", return_value=[]):
                entries = PACKAGER.build_source_entries(root, snapshot)

        expected_archive_paths = {
            f"integration_reference/{source}" for source in required_sources
        }
        self.assertTrue(expected_archive_paths.issubset(entries))
        for source in required_sources:
            entry = entries[f"integration_reference/{source}"]
            self.assertEqual(entry.archive_path, f"integration_reference/{source}")
            self.assertEqual(entry.origin, "explicit-integration-reference")
            self.assertEqual(entry.category, "integration-reference")

    def test_task10_contract_is_packaged_with_exact_memory_and_source_hashes(
            self) -> None:
        self.assertEqual(PACKAGER.FULL_PACK_BYTES, 329430304)
        self.assertEqual(PACKAGER.RESIDENT_PEAK_BYTES, 226800928)
        self.assertEqual(PACKAGER.TRANSITION_PEAK_BYTES, 261010720)
        self.assertTrue(EXPECTED_TASK10_PLAN_FILES.issubset(PACKAGER.PLAN_FILES))
        self.assertTrue(
            EXPECTED_TASK10_INTEGRATION_FILES.issubset(PACKAGER.INTEGRATION_FILES))
        self.assertEqual(
            set(PACKAGER.MATERIAL_RUNTIME_REQUIRED_FILES),
            EXPECTED_TASK10_REQUIRED_ASSETS)

        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            for relative in (
                    PACKAGER.STATIC_FONT,
                    *PACKAGER.PLAN_FILES,
                    *PACKAGER.INTEGRATION_FILES,
                    *EXPECTED_TASK10_REQUIRED_ASSETS):
                source = root / relative
                source.parent.mkdir(parents=True, exist_ok=True)
                source.write_bytes((relative + "\n").encode("utf-8"))
            snapshot = PACKAGER.FormalEvidenceSnapshot(
                directory=root, report_sha256="0" * 64,
                files=(), previews=())
            with mock.patch.object(
                    PACKAGER, "git_tracked_material_paths", return_value=[]):
                entries = PACKAGER.build_source_entries(root, snapshot)
            for relative in EXPECTED_TASK10_REQUIRED_ASSETS:
                entry = entries[relative]
                self.assertEqual(entry.source_path, root / relative)
                self.assertEqual(
                    PACKAGER.digest_entry(entry),
                    hashlib.sha256((relative + "\n").encode("utf-8")).hexdigest())

    def test_selection_policy_reports_every_automatic_exclusion(self) -> None:
        self.assertEqual(PACKAGER.material_selection_policy(), {
            "automatic": (
                "Git-tracked files below assets/ and art_source/stage12/ only"
            ),
            "automatic_exclusions": [
                "assets/fonts/NotoSansSC[wght].ttf",
                "art_source/stage12/backgrounds/candidates/",
            ],
            "explicit_inclusion": "assets/fonts/NotoSansCJKsc-Medium.otf",
            "untracked_candidate_art_included": False,
        })

    def test_git_material_discovery_excludes_archival_background_candidates(
            self) -> None:
        completed = subprocess.CompletedProcess(
            [], 0,
            b"assets/stage12/fire_room_background.png\0"
            b"art_source/stage12/backgrounds/fire/fire-room-background-master.png\0"
            b"art_source/stage12/backgrounds/candidates/candidate-manifest.json\0"
            b"art_source/stage12/backgrounds/candidates/fire/rejected.png\0",
            b"")
        with mock.patch.object(
                PACKAGER.subprocess, "run", return_value=completed):
            paths = PACKAGER.git_tracked_material_paths(Path("fixture-root"))
        self.assertEqual(paths, [
            "art_source/stage12/backgrounds/fire/fire-room-background-master.png",
            "assets/stage12/fire_room_background.png",
        ])

    def make_resolution_records(self) -> dict[str, dict[str, object]]:
        records: dict[str, dict[str, object]] = {}
        digest_prefix = {
            "fire": "a", "water": "b", "lightning": "c", "chaos": "d"}
        for ecology in ECOLOGIES:
            legacy_size = 1024 if ecology == "fire" else 768
            legacy_path = f"assets/stage12/{ecology}_environment.png"
            records[legacy_path] = {
                "path": legacy_path,
                "sha256": ecology[0] * 64,
                "png": {"width": legacy_size, "height": legacy_size},
            }
            master_path = (
                f"art_source/stage12/backgrounds/{ecology}/"
                f"{ecology}-room-background-master.png")
            runtime_path = f"assets/stage12/{ecology}_room_background.png"
            material_path = (
                f"assets/stage12/{ecology}_room_background_material.png")
            records[master_path] = {
                "path": master_path,
                "sha256": (digest_prefix[ecology] + "1") * 32,
                "png": {"width": 3840, "height": 2160},
            }
            records[runtime_path] = {
                "path": runtime_path,
                "sha256": (digest_prefix[ecology] + "2") * 32,
                "png": {"width": 2560, "height": 1440},
            }
            records[material_path] = {
                "path": material_path,
                "sha256": (digest_prefix[ecology] + "3") * 32,
                "png": {"width": 2560, "height": 1440},
            }
            for view in ("background-only", "gameplay"):
                for resolution in ("1280x720", "1920x1080"):
                    preview_path = (
                        f"previews/{ecology}-{view}-{resolution}.png")
                    records[preview_path] = {
                        "path": preview_path,
                        "sha256": hashlib.sha256(
                            preview_path.encode("utf-8")).hexdigest(),
                        "png": {
                            "width": int(resolution.split("x")[0]),
                            "height": int(resolution.split("x")[1]),
                        },
                    }
        records[PACKAGER.BACKGROUND_PROVENANCE] = {
            "path": PACKAGER.BACKGROUND_PROVENANCE,
            "sha256": "ab" * 32,
        }
        return records

    def test_resolution_audit_retains_history_and_binds_native_outputs(
            self) -> None:
        records = self.make_resolution_records()

        audit = PACKAGER.resolution_audit(records)
        self.assertEqual(audit["schema_version"], 2)
        self.assertEqual(
            [entry["status"] for entry in audit["legacy_room_backgrounds"]],
            ["native-redraw-required"] * 4)
        self.assertEqual(audit["runtime_memory"], {
            "full_pack_bytes": 329430304,
            "resident_peak_bytes": 226800928,
            "transition_peak_bytes": 261010720,
            "resident_hard_cap_bytes": 268435456,
        })
        self.assertEqual(len(audit["room_backgrounds"]), 4)
        for entry in audit["room_backgrounds"]:
            self.assertEqual(entry["status"], "native-background-verified")
            self.assertEqual(
                entry["runtime_background_source_rect"],
                {"x": 0, "y": 0, "width": 2560, "height": 1440})
            self.assertEqual(entry["source_to_screen_scale"], {
                "1280x720": "1/2",
                "1920x1080": "3/4",
                "upscale_allowed": False,
            })
            self.assertRegex(entry["master"]["sha256"], r"^[0-9a-f]{64}$")
            self.assertRegex(entry["runtime"]["sha256"], r"^[0-9a-f]{64}$")
            self.assertRegex(entry["material"]["sha256"], r"^[0-9a-f]{64}$")
            self.assertEqual(
                entry["provenance"]["path"], PACKAGER.BACKGROUND_PROVENANCE)
            self.assertEqual(
                set(entry["evidence"]),
                {"background_only_1280", "background_only_1920",
                 "gameplay_1280", "gameplay_1920"})

    def test_resolution_audit_rejects_duplicate_material_backgrounds(
            self) -> None:
        records = self.make_resolution_records()
        records["assets/stage12/water_room_background_material.png"][
            "sha256"] = records[
                "assets/stage12/fire_room_background_material.png"]["sha256"]
        with self.assertRaisesRegex(
                RuntimeError, "cross-ecology native background hashes"):
            PACKAGER.resolution_audit(records)

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

    def test_formal_validation_rejects_wrong_integration_transition_peak(
            self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            evidence = self.make_formal_fixture(root)
            integration = evidence / "material-runtime-integration-evidence.txt"
            integration.write_text(
                integration.read_text(encoding="utf-8").replace(
                    "transition_peak_bytes=261010720",
                    "transition_peak_bytes=1"),
                encoding="utf-8")
            with mock.patch.object(PACKAGER.subprocess, "run") as run:
                with self.assertRaisesRegex(RuntimeError, "integration|transition"):
                    PACKAGER.validate_formal_evidence(root)
            run.assert_not_called()

    def test_formal_validation_rejects_forged_native_background_hash(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            evidence = self.make_formal_fixture(root)
            report = evidence / PACKAGER.EVIDENCE_FILE
            text = report.read_text(encoding="utf-8")
            prefix = "fire_background_runtime_sha256="
            text = "\n".join(
                prefix + "0" * 64 if line.startswith(prefix) else line
                for line in text.splitlines()) + "\n"
            report.write_text(text, encoding="utf-8")
            with mock.patch.object(PACKAGER.subprocess, "run") as run:
                with self.assertRaisesRegex(
                        RuntimeError, "SHA-256|provenance"):
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
            self.assertEqual(
                records["file_count"], len(PACKAGER.FORMAL_PREVIEW_FIELDS) + 3)
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
