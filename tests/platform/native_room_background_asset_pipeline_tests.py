"""Contract tests for native, non-upscaled Stage 12 room backgrounds."""

from __future__ import annotations

import hashlib
import json
import subprocess
import sys
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


class NativeRoomBackgroundAssetPipelineTests(unittest.TestCase):
    def test_fire_sources_are_native_tiles_at_least_1024_pixels(self) -> None:
        for source in SOURCES:
            with self.subTest(source=source.name), Image.open(source) as image:
                self.assertGreaterEqual(image.width, 1024)
                self.assertGreaterEqual(image.height, 1024)

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

        source_manifest = json.loads(SOURCE_MANIFEST.read_text(encoding="utf-8"))
        fire_inputs = source_manifest["ecologies"][ECOLOGY]["sources"]
        self.assertEqual({entry["path"] for entry in fire_inputs}, {
            "art_source/stage12/backgrounds/fire/fire-floor-tile-v1.png",
            "art_source/stage12/backgrounds/fire/fire-wall-tile-v1.png",
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
            {source.relative_to(ROOT).as_posix(): sha256(source) for source in SOURCES},
        )
        self.assertEqual(fire_report["output_sha256"], {
            "master": sha256(MASTER),
            "runtime": sha256(RUNTIME),
            "material": sha256(MATERIAL),
        })
        self.assertTrue(fire_report["placements"], "no native placements were recorded")
        for placement in fire_report["placements"]:
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
            edge = master.crop((0, 0, 3840, 520))
            self.assertGreater(len(set(center.convert("RGB").get_flattened_data())), 1)
            self.assertLess(luminance_stddev(center), luminance_stddev(edge))


if __name__ == "__main__":
    unittest.main()
