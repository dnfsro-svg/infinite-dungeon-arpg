"""Deterministic quality gates for the authored item/material atlas pair."""

from __future__ import annotations

import importlib.util
from pathlib import Path
import unittest

from PIL import Image, ImageChops, ImageFilter, ImageStat


ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location(
    "build_item_material_atlas", ROOT / "tools" / "build_item_material_atlas.py")
assert SPEC is not None and SPEC.loader is not None
BUILDER = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(BUILDER)


def connected_component_sizes(alpha: Image.Image, threshold: int = 8) -> list[int]:
    pixels = alpha.load()
    remaining = {(x, y) for y in range(alpha.height) for x in range(alpha.width)
                 if pixels[x, y] >= threshold}
    sizes: list[int] = []
    while remaining:
        stack = [remaining.pop()]
        size = 0
        while stack:
            x, y = stack.pop()
            size += 1
            for neighbor in ((x - 1, y), (x + 1, y),
                             (x, y - 1), (x, y + 1)):
                if neighbor in remaining:
                    remaining.remove(neighbor)
                    stack.append(neighbor)
        sizes.append(size)
    return sorted(sizes, reverse=True)


def outer_magenta_ratio(icon: Image.Image) -> float:
    alpha = icon.getchannel("A")
    inner = alpha.filter(ImageFilter.MinFilter(3))
    edge = ImageChops.subtract(alpha, inner)
    edge_pixels = edge.load()
    pixels = icon.load()
    total = 0
    magenta = 0
    for y in range(icon.height):
        for x in range(icon.width):
            red, green, blue, visible = pixels[x, y]
            if edge_pixels[x, y] < 12 or visible < 12:
                continue
            total += 1
            if (red >= 70 and blue >= 70 and min(red, blue) >= green * 1.35
                    and abs(red - blue) <= 100):
                magenta += 1
    return magenta / total if total else 0.0


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

    def test_cells_have_clean_transparent_backgrounds_and_one_main_subject(self) -> None:
        atlas = Image.open(ROOT / "assets" / "stage12" / "items_ui.png").convert("RGBA")
        transparent_ratios = []
        for name, cell in BUILDER.ICON_CELLS.items():
            alpha = BUILDER.crop_cell(atlas, cell).getchannel("A")
            values = list(alpha.get_flattened_data())
            transparent_ratio = sum(value == 0 for value in values) / len(values)
            transparent_ratios.append(transparent_ratio)
            self.assertGreaterEqual(transparent_ratio, 0.30, name)
            components = connected_component_sizes(alpha)
            self.assertTrue(components, name)
            self.assertGreaterEqual(components[0] / sum(components), 0.88, name)
            border = list(alpha.crop((0, 0, BUILDER.CELL, 5)).get_flattened_data())
            border += list(alpha.crop(
                (0, BUILDER.CELL - 5, BUILDER.CELL, BUILDER.CELL)).get_flattened_data())
            border += list(alpha.crop((0, 5, 5, BUILDER.CELL - 5)).get_flattened_data())
            border += list(alpha.crop(
                (BUILDER.CELL - 5, 5, BUILDER.CELL, BUILDER.CELL - 5)).get_flattened_data())
            self.assertEqual(max(border), 0, name)
        self.assertGreaterEqual(sum(transparent_ratios) / len(transparent_ratios),
                                0.57)

    def test_background_matte_does_not_move_or_erase_a_purple_subject(self) -> None:
        source = Image.new("RGBA", (180, 180), (255, 0, 255, 255))
        pixels = source.load()
        for y in range(38):
            for x in range(44):
                pixels[x, y] = (235 + (x % 3) * 5, y % 4, 238, 255)
        for y in range(68, 148):
            for x in range(105, 151):
                pixels[x, y] = (150, 20, 210, 255)
        for y in range(52, 54):
            for x in range(8, 10):
                pixels[x, y] = (118, 12, 166, 255)
        icon = BUILDER.contain_icon(BUILDER.remove_magenta_key(source))
        alpha = icon.getchannel("A")
        self.assertEqual(alpha.getpixel((6, 6)), 0)
        bbox = alpha.point(lambda value: 255 if value >= 96 else 0).getbbox()
        self.assertIsNotNone(bbox)
        assert bbox is not None
        self.assertLessEqual(abs((bbox[0] + bbox[2]) / 2 - BUILDER.CELL / 2), 1.0)
        self.assertLessEqual(abs((bbox[1] + bbox[3]) / 2 - BUILDER.CELL / 2), 1.0)
        center = icon.getpixel((BUILDER.CELL // 2, BUILDER.CELL // 2))
        self.assertGreaterEqual(center[0], 130)
        self.assertGreaterEqual(center[2], 190)
        self.assertEqual(center[3], 255)

    def test_non_purple_resources_have_no_magenta_key_halo(self) -> None:
        atlas = Image.open(ROOT / "assets" / "stage12" / "items_ui.png").convert("RGBA")
        purple_cells = {
            BUILDER.ICON_CELLS["rarity_abyss"],
            BUILDER.ICON_CELLS["material_augment"],
            BUILDER.ICON_CELLS["material_chaos"],
            BUILDER.ICON_CELLS["material_coupon_9"],
        }
        for name, cell in BUILDER.ICON_CELLS.items():
            if cell in purple_cells:
                continue
            ratio = outer_magenta_ratio(BUILDER.crop_cell(atlas, cell))
            self.assertLessEqual(ratio, 0.12, f"{name}: {ratio:.3f}")

    def test_key_blended_outer_edge_is_despilled_but_inner_purple_survives(self) -> None:
        source = Image.new("RGBA", (48, 48), (245, 3, 243, 255))
        pixels = source.load()
        for y in range(11, 37):
            for x in range(11, 37):
                pixels[x, y] = (140, 25, 160, 255)
        for y in range(14, 34):
            for x in range(14, 34):
                pixels[x, y] = (108, 28, 174, 255)
        cleaned = BUILDER.remove_magenta_key(source)
        edge = cleaned.getpixel((11, 24))
        interior = cleaned.getpixel((24, 24))
        self.assertLessEqual(edge[3], 112)
        self.assertLess(min(edge[0], edge[2]), 90)
        self.assertEqual(interior, (108, 28, 174, 255))

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
