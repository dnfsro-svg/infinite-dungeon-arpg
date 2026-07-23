"""Contract tests for native, non-upscaled Stage 12 room backgrounds."""

from __future__ import annotations

import hashlib
import json
import subprocess
import sys
import tarfile
import tempfile
import unittest
from pathlib import Path

from PIL import Image, ImageStat


ROOT = Path(__file__).resolve().parents[2]
ECOLOGY = "fire"
SOURCES = (
    ROOT / "art_source/stage12/backgrounds/fire/fire-floor-tile-v1.png",
    ROOT / "art_source/stage12/backgrounds/fire/fire-wall-tile-v1.png",
)
SOURCE_MANIFEST = ROOT / "art_source/stage12/backgrounds/background-sources.json"
BUILDER = ROOT / "tools/build_native_room_backgrounds.py"
MASTER = ROOT / "art_source/stage12/backgrounds/fire/fire-room-background-master.png"
RUNTIME = ROOT / "assets/stage12/fire_room_background.png"
MATERIAL = ROOT / "assets/stage12/fire_room_background_material.png"
REPORT = ROOT / "assets/stage12/room-background-build.json"


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def luminance_stddev(image: Image.Image) -> float:
    gray = image.convert("L")
    return ImageStat.Stat(gray).stddev[0]


def declared_sources(root: Path = ROOT) -> list[dict[str, str]]:
    manifest = json.loads((root / SOURCE_MANIFEST).read_text(encoding="utf-8"))
    return manifest["ecologies"][ECOLOGY]["sources"]


class NativeRoomBackgroundAssetPipelineTests(unittest.TestCase):
    def test_fire_sources_are_native_tiles_at_least_1024_pixels(self) -> None:
        for entry in declared_sources():
            source = ROOT / entry["path"]
            with self.subTest(source=source.name), Image.open(source) as image:
                if entry["role"] == "room_layout":
                    self.assertGreaterEqual(image.width, 1024)
                    self.assertGreaterEqual(image.height, 720)
                else:
                    self.assertGreaterEqual(image.width, 1024)
                    self.assertGreaterEqual(image.height, 1024)

    def test_fire_manifest_has_multiple_independent_native_floor_samples(self) -> None:
        floor_sources = [entry for entry in declared_sources() if entry["role"].startswith("floor")]
        self.assertGreaterEqual(len(floor_sources), 2)
        floor_hashes = {sha256(ROOT / entry["path"]) for entry in floor_sources}
        self.assertEqual(len(floor_hashes), len(floor_sources))

    def test_indexed_clean_tree_contains_all_manifest_inputs_and_rebuilds(self) -> None:
        inputs = [entry["path"] for entry in declared_sources()]
        tracked = subprocess.run(
            ["git", "ls-files", "--error-unmatch", "--", *inputs],
            cwd=ROOT,
            text=True,
            capture_output=True,
            check=False,
        )
        self.assertEqual(tracked.returncode, 0, tracked.stderr)

        tree = subprocess.check_output(["git", "write-tree"], cwd=ROOT, text=True).strip()
        with tempfile.TemporaryDirectory() as temporary:
            archive = Path(temporary) / "candidate.tar"
            with archive.open("wb") as stream:
                subprocess.run(["git", "archive", "--format=tar", tree], cwd=ROOT, stdout=stream, check=True)
            with tarfile.open(archive) as bundle:
                bundle.extractall(temporary, filter="data")
            clean_root = Path(temporary)
            for path in inputs:
                self.assertTrue((clean_root / path).is_file(), f"archive missing manifest input: {path}")
            rebuilt = subprocess.run(
                [sys.executable, str(clean_root / "tools/build_native_room_backgrounds.py"), "--ecology", ECOLOGY],
                cwd=clean_root,
                text=True,
                capture_output=True,
                check=False,
            )
            self.assertEqual(rebuilt.returncode, 0, rebuilt.stderr or rebuilt.stdout)
            self.assertTrue((clean_root / "assets/stage12/room-background-build.json").is_file())

    def test_fire_build_is_traceable_downsampled_and_never_upscaled(self) -> None:
        self.assertTrue(BUILDER.is_file(), "native room background builder is missing")
        result = subprocess.run(
            [sys.executable, str(BUILDER), "--ecology", ECOLOGY],
            cwd=ROOT,
            text=True,
            capture_output=True,
            check=False,
        )
        self.assertEqual(result.returncode, 0, result.stderr or result.stdout)

        self.assertTrue(SOURCE_MANIFEST.is_file(), "source provenance manifest is missing")
        self.assertTrue(REPORT.is_file(), "build report is missing")
        for output in (MASTER, RUNTIME, MATERIAL):
            self.assertTrue(output.is_file(), f"required output is missing: {output.name}")

        fire_inputs = declared_sources()
        self.assertEqual({entry["path"] for entry in fire_inputs}, {
            "art_source/stage12/backgrounds/fire/fire-floor-tile-v1.png",
            "art_source/stage12/backgrounds/fire/fire-wall-tile-v1.png",
            "art_source/stage12/backgrounds/fire/fire-floor-tile-v2-a.png",
            "art_source/stage12/backgrounds/fire/fire-floor-tile-v2-b.png",
            "art_source/stage12/backgrounds/fire/fire-wall-tile-v2.png",
            "art_source/stage12/backgrounds/fire/fire-room-layout-v2.png",
        })
        manifest_text = SOURCE_MANIFEST.read_text(encoding="utf-8")
        self.assertNotIn("assets/stage12/fire_environment.png", manifest_text)

        report = json.loads(REPORT.read_text(encoding="utf-8"))
        fire_report = report["ecologies"][ECOLOGY]
        self.assertEqual(fire_report["master_size"], [3840, 2160])
        self.assertEqual(fire_report["runtime_size"], [2560, 1440])
        self.assertEqual(fire_report["runtime_from_master"], {
            "resampling": "LANCZOS",
            "passes": 1,
        })
        self.assertEqual(
            fire_report["source_sha256"],
            {entry["path"]: sha256(ROOT / entry["path"]) for entry in fire_inputs},
        )
        self.assertEqual(fire_report["output_sha256"], {
            "master": sha256(MASTER),
            "runtime": sha256(RUNTIME),
            "material": sha256(MATERIAL),
        })
        self.assertTrue(fire_report["placements"], "no native placements were recorded")
        for placement in fire_report["placements"]:
            source_left, source_top, source_right, source_bottom = placement["source_rect"]
            target_left, target_top, target_right, target_bottom = placement["target_rect"]
            source_width = source_right - source_left
            source_height = source_bottom - source_top
            target_width = target_right - target_left
            target_height = target_bottom - target_top
            self.assertGreater(source_width, 0)
            self.assertGreater(source_height, 0)
            self.assertLessEqual(target_width, source_width)
            self.assertLessEqual(target_height, source_height)
            self.assertAlmostEqual(placement["scale_x"], target_width / source_width)
            self.assertAlmostEqual(placement["scale_y"], target_height / source_height)
            self.assertLessEqual(placement["scale_x"], 1.0)
            self.assertLessEqual(placement["scale_y"], 1.0)
            self.assertIn("source_rect", placement)
            self.assertIn("target_rect", placement)

        with Image.open(MASTER) as master, Image.open(RUNTIME) as runtime, Image.open(MATERIAL) as material:
            self.assertEqual(master.size, (3840, 2160))
            self.assertEqual(runtime.size, (2560, 1440))
            self.assertEqual(material.size, (2560, 1440))
            self.assertEqual(runtime.mode, "RGBA")
            self.assertEqual(material.mode, "RGBA")
            expected_runtime = master.convert("RGBA").resize((2560, 1440), Image.Resampling.LANCZOS)
            self.assertEqual(runtime.tobytes(), expected_runtime.tobytes())

            center = master.crop((1120, 1030, 2720, 1870))
            room_edge = master.crop((1084, 536, 1300, 1477))
            self.assertGreater(len(set(center.convert("RGB").get_flattened_data())), 1)
            self.assertLess(luminance_stddev(center), luminance_stddev(room_edge))


if __name__ == "__main__":
    unittest.main()
