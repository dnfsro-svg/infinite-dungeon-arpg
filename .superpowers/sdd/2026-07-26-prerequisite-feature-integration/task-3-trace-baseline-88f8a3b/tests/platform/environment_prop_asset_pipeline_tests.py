"""Contract tests for isolated Stage 12 ecology props and public doors."""

from __future__ import annotations

import hashlib
import json
import os
import shutil
import subprocess
import sys
import tempfile
import unittest
from collections import deque
from pathlib import Path

from PIL import Image


ROOT = Path(__file__).resolve().parents[2]
BUILDER = ROOT / "tools/build_environment_props.py"
REPORT = ROOT / "assets/stage12/environment-props-build.json"
ECOLOGIES = ("water", "lightning", "chaos")
LAYOUT = {
    "wall": (512, 0),
    "surface_prop": (512, 256),
    "hole": (0, 512),
    "light": (256, 512),
    "solid_prop": (512, 512),
}


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def alpha_components(image: Image.Image, threshold: int = 8) -> list[int]:
    alpha = image.getchannel("A")
    remaining = {
        (x, y)
        for y in range(image.height)
        for x in range(image.width)
        if alpha.getpixel((x, y)) > threshold
    }
    sizes: list[int] = []
    while remaining:
        start = remaining.pop()
        queue = deque((start,))
        size = 1
        while queue:
            x, y = queue.popleft()
            for dx, dy in ((-1, 0), (1, 0), (0, -1), (0, 1),
                           (-1, -1), (-1, 1), (1, -1), (1, 1)):
                neighbor = (x + dx, y + dy)
                if neighbor in remaining:
                    remaining.remove(neighbor)
                    queue.append(neighbor)
                    size += 1
        sizes.append(size)
    return sorted(sizes, reverse=True)


class EnvironmentPropAssetPipelineTests(unittest.TestCase):
    def test_chaos_wall_cell_preserves_complete_authored_tile_coverage(self) -> None:
        color_path = ROOT / "assets/stage12/chaos_environment.png"
        with Image.open(color_path).convert("RGBA") as atlas:
            bbox = atlas.crop((512, 0, 768, 256)).getchannel("A").getbbox()
        self.assertIsNotNone(bbox)
        assert bbox is not None
        self.assertGreaterEqual(bbox[2] - bbox[0], 224)
        self.assertGreaterEqual(bbox[3] - bbox[1], 224)

    def test_bootstrap_doors_read_old_ecology_atlas_cells(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            assets = root / "assets" / "stage12"
            sources = root / "art_source" / "stage12"
            assets.mkdir(parents=True)
            sources.mkdir(parents=True)
            colors = {
                "fire": (201, 31, 29, 255),
                "water": (23, 151, 213, 255),
                "lightning": (240, 205, 31, 255),
                "chaos": (160, 44, 196, 255),
            }
            for ecology, color in colors.items():
                size = (1024, 1024) if ecology == "fire" else (768, 768)
                atlas = Image.new("RGBA", size)
                rect = (768, 0, 1024, 256) if ecology == "fire" else (512, 0, 768, 256)
                atlas.paste(color, rect)
                atlas.save(assets / f"{ecology}_environment.png")
                # Old code re-crops these concept files instead of the atlas.
                Image.new("RGBA", (1254, 1254), (255, 0, 255, 255)).save(
                    sources / f"{ecology}-environment-concept-v1.png")
            source = ROOT / "tools" / "build_environment_props.py"
            result = subprocess.run(
                [sys.executable, "-c", (
                    "import importlib.util, pathlib; "
                    f"spec=importlib.util.spec_from_file_location('builder', r'{source}'); "
                    "module=importlib.util.module_from_spec(spec); spec.loader.exec_module(module); "
                    f"image, _ = module.build_element_doors(pathlib.Path(r'{root}')); "
                    f"image.save(pathlib.Path(r'{root}') / 'doors.png')")],
                text=True, capture_output=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            with Image.open(root / "doors.png").convert("RGBA") as doors:
                for index, ecology in enumerate(("water", "lightning", "chaos"), 1):
                    cell = doors.crop((index * 256, 0, (index + 1) * 256, 256))
                    pixels = [pixel for pixel in cell.get_flattened_data() if pixel[3] > 128]
                    self.assertTrue(pixels)
                    self.assertEqual(max(set(pixels), key=pixels.count), colors[ecology])

    def test_forced_staged_validation_failure_leaves_destinations_unchanged(self) -> None:
        outputs = [
            ROOT / "assets/stage12/element_doors.png",
            ROOT / "assets/stage12/element_doors_material.png",
            REPORT,
        ]
        for ecology in ECOLOGIES:
            outputs.extend((ROOT / f"assets/stage12/{ecology}_environment.png",
                            ROOT / f"assets/stage12/{ecology}_environment_material.png"))
        before = {path: sha256(path) for path in outputs}
        environment = os.environ | {"ARPG_ENVIRONMENT_PROPS_FAIL_VALIDATION": "1"}
        result = subprocess.run([sys.executable, str(BUILDER), "--root", str(ROOT)],
                                cwd=ROOT, text=True, capture_output=True, env=environment)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("forced staged validation failure", result.stderr)
        self.assertEqual({path: sha256(path) for path in outputs}, before)

    def test_invalid_published_door_hash_fails_closed_without_reusing_wall_cells(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            assets = root / "assets" / "stage12"
            assets.mkdir(parents=True)
            outputs = [assets / "element_doors.png", assets / "element_doors_material.png",
                       assets / "environment-props-build.json"]
            for ecology in ECOLOGIES:
                outputs.extend((assets / f"{ecology}_environment.png",
                                assets / f"{ecology}_environment_material.png"))
            for output in outputs:
                shutil.copy2(ROOT / "assets/stage12" / output.name, output)
            shutil.copy2(ROOT / "assets/stage12/fire_environment.png",
                         assets / "fire_environment.png")
            source_root = ROOT / "art_source" / "stage12"
            for ecology in ECOLOGIES:
                target = root / "art_source" / "stage12"
                (target / "backgrounds" / ecology).mkdir(parents=True, exist_ok=True)
                shutil.copy2(source_root / f"{ecology}-environment-concept-v1.png",
                             target / f"{ecology}-environment-concept-v1.png")
                shutil.copy2(source_root / "backgrounds" / ecology / f"{ecology}-wall-tile-v1.png",
                             target / "backgrounds" / ecology / f"{ecology}-wall-tile-v1.png")
                with Image.open(assets / f"{ecology}_environment.png").convert("RGBA") as image:
                    image.paste((17, 19, 23, 255), (512, 0, 768, 256))
                    image.save(assets / f"{ecology}_environment.png")
            report_path = assets / "environment-props-build.json"
            report = json.loads(report_path.read_text(encoding="utf-8"))
            report["element_doors"]["rgba_sha256"] = "0" * 64
            report_path.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n",
                                   encoding="utf-8")
            before = {path: sha256(path) for path in outputs}
            result = subprocess.run([sys.executable, str(BUILDER), "--root", str(root)],
                                    cwd=root, text=True, capture_output=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("published element doors validation failed", result.stderr)
            self.assertEqual({path: sha256(path) for path in outputs}, before)

    def test_builder_repeat_generation_is_byte_for_byte_stable(self) -> None:
        self.assertTrue(BUILDER.is_file(), "shared environment prop builder is missing")
        outputs = [
            ROOT / "assets/stage12/element_doors.png",
            ROOT / "assets/stage12/element_doors_material.png",
            REPORT,
        ]
        for ecology in ECOLOGIES:
            outputs.extend((
                ROOT / f"assets/stage12/{ecology}_environment.png",
                ROOT / f"assets/stage12/{ecology}_environment_material.png",
            ))
        for path in outputs:
            self.assertTrue(path.is_file(), path)
        before = {path: sha256(path) for path in outputs}
        for _ in range(2):
            result = subprocess.run(
                [sys.executable, str(BUILDER), "--root", str(ROOT)],
                cwd=ROOT, text=True, capture_output=True)
            self.assertEqual(result.returncode, 0, result.stderr or result.stdout)
        self.assertEqual({path: sha256(path) for path in outputs}, before)

    def test_public_element_doors_are_isolated_unique_material_cells(self) -> None:
        color_path = ROOT / "assets/stage12/element_doors.png"
        material_path = ROOT / "assets/stage12/element_doors_material.png"
        self.assertTrue(color_path.is_file(), color_path)
        self.assertTrue(material_path.is_file(), material_path)
        with Image.open(color_path).convert("RGBA") as color, \
                Image.open(material_path).convert("RGBA") as material:
            self.assertEqual(color.size, material.size)
            self.assertEqual(color.size, (1024, 256))
            self.assertEqual(color.getchannel("A").tobytes(),
                             material.getchannel("A").tobytes())
            door_cells = [
                color.crop((cell * 256, 0, (cell + 1) * 256, 256))
                for cell in range(4)
            ]
            for tile in door_cells:
                bbox = tile.getchannel("A").getbbox()
                self.assertIsNotNone(bbox)
                assert bbox is not None
                self.assertGreaterEqual(
                    min(bbox[0], bbox[1], 256 - bbox[2], 256 - bbox[3]), 12)
            self.assertEqual(len({cell.tobytes() for cell in door_cells}), 4)

    def test_ecology_prop_manifest_matches_isolated_outputs(self) -> None:
        self.assertTrue(REPORT.is_file(), REPORT)
        manifest = json.loads(REPORT.read_text(encoding="utf-8"))
        self.assertEqual(manifest["schema_version"], 1)
        self.assertEqual(set(manifest["ecologies"]), set(ECOLOGIES))
        for ecology in ECOLOGIES:
            color_path = ROOT / f"assets/stage12/{ecology}_environment.png"
            material_path = ROOT / f"assets/stage12/{ecology}_environment_material.png"
            with self.subTest(ecology=ecology), \
                    Image.open(color_path).convert("RGBA") as color, \
                    Image.open(material_path).convert("RGBA") as material:
                self.assertEqual(color.size, material.size)
                self.assertEqual(color.size, (768, 768))
                self.assertEqual(color.getchannel("A").tobytes(),
                                 material.getchannel("A").tobytes())
                records = manifest["ecologies"][ecology]["objects"]
                self.assertEqual(set(records), set(LAYOUT))
                for name, (x, y) in LAYOUT.items():
                    record = records[name]
                    self.assertEqual(record["source_rect"], [x, y, 256, 256])
                    cell = color.crop((x, y, x + 256, y + 256))
                    material_cell = material.crop((x, y, x + 256, y + 256))
                    bbox = cell.getchannel("A").getbbox()
                    self.assertIsNotNone(bbox, f"{ecology}/{name} is empty")
                    assert bbox is not None
                    self.assertGreaterEqual(
                        min(bbox[0], bbox[1], 256 - bbox[2], 256 - bbox[3]), 8)
                    self.assertEqual(cell.getchannel("A").tobytes(),
                                     material_cell.getchannel("A").tobytes())
                    components = alpha_components(cell)
                    self.assertTrue(components)
                    if len(components) > 1:
                        self.assertLessEqual(components[1] * 100,
                                             components[0] * 2)
                    self.assertGreaterEqual(components[0] * 100,
                                            sum(components) * 95)
                    self.assertEqual(record["alpha_bbox"], list(bbox))
                    expected_anchor = [(bbox[0] + bbox[2]) // 2, bbox[3]]
                    self.assertEqual(record["foot_anchor"], expected_anchor)


if __name__ == "__main__":
    unittest.main()
