"""Deterministic quality gates for the authored item/material atlas pair."""

from __future__ import annotations

import importlib.util
from pathlib import Path
import unittest

from PIL import Image, ImageChops, ImageStat


ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location(
    "build_item_material_atlas", ROOT / "tools" / "build_item_material_atlas.py")
assert SPEC is not None and SPEC.loader is not None
BUILDER = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(BUILDER)


class ItemMaterialAssetPipelineTests(unittest.TestCase):
    def test_color_and_material_atlases_exist_and_match(self) -> None:
        color = Image.open(ROOT / "assets" / "stage12" / "items_ui.png").convert("RGBA")
        material = Image.open(
            ROOT / "assets" / "stage12" / "items_ui_material.png").convert("RGBA")
        self.assertEqual(color.size, (1024, 1024))
        self.assertEqual(material.size, color.size)
        self.assertEqual(color.getchannel("A").tobytes(),
                         material.getchannel("A").tobytes())
        self.assertNotEqual(color.convert("RGB").tobytes(),
                            material.convert("RGB").tobytes())

    def test_every_declared_icon_is_nonflat_unique_and_safely_anchored(self) -> None:
        atlas = Image.open(ROOT / "assets" / "stage12" / "items_ui.png").convert("RGBA")
        seen: set[bytes] = set()
        for name, cell in BUILDER.ICON_CELLS.items():
            icon = BUILDER.crop_cell(atlas, cell)
            alpha = icon.getchannel("A")
            bbox = alpha.point(lambda value: 255 if value >= 48 else 0).getbbox()
            self.assertIsNotNone(bbox, name)
            assert bbox is not None
            self.assertGreaterEqual(bbox[0], 4, name)
            self.assertGreaterEqual(bbox[1], 4, name)
            self.assertLessEqual(bbox[2], BUILDER.CELL - 4, name)
            self.assertLessEqual(bbox[3], BUILDER.CELL - 4, name)
            self.assertGreater(ImageStat.Stat(alpha).mean[0], 12.0, name)
            visible_colors = {pixel[:3] for pixel in icon.get_flattened_data()
                              if pixel[3] >= 96}
            self.assertGreater(len(visible_colors), 24, name)
            signature = BUILDER.icon_signature(icon)
            self.assertNotIn(signature, seen, name)
            seen.add(signature)

    def test_icons_keep_a_high_contrast_authored_outline(self) -> None:
        atlas = Image.open(ROOT / "assets" / "stage12" / "items_ui.png").convert("RGBA")
        for name, cell in BUILDER.ICON_CELLS.items():
            icon = BUILDER.crop_cell(atlas, cell)
            self.assertGreaterEqual(BUILDER.outline_contrast_score(icon), 0.18,
                                    name)

    def test_rarity_silhouettes_are_structurally_distinct(self) -> None:
        atlas = Image.open(ROOT / "assets" / "stage12" / "items_ui.png").convert("RGBA")
        masks = []
        for name in ("rarity_normal", "rarity_magic", "rarity_rare", "rarity_abyss"):
            icon = BUILDER.crop_cell(atlas, BUILDER.ICON_CELLS[name])
            masks.append(icon.getchannel("A").point(
                lambda value: 255 if value >= 64 else 0))
        for index, mask in enumerate(masks):
            for previous in masks[:index]:
                changed = ImageStat.Stat(ImageChops.difference(mask, previous)).mean[0]
                self.assertGreater(changed, 5.0)


if __name__ == "__main__":
    unittest.main()
