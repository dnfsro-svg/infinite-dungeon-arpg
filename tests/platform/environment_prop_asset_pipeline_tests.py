"""Contract tests for isolated Stage 12 ecology props and public doors."""

from __future__ import annotations

import hashlib
import json
import subprocess
import sys
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
