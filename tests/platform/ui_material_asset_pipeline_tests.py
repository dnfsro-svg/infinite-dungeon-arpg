from __future__ import annotations

import importlib.util
from pathlib import Path
import statistics
import unittest

from PIL import Image, ImageChops, ImageFilter, ImageStat


ROOT = Path(__file__).resolve().parents[2]
BUILDER_PATH = ROOT / "tools" / "build_ui_material_atlas.py"
SPEC = importlib.util.spec_from_file_location("ui_material_builder", BUILDER_PATH)
if SPEC is None or SPEC.loader is None:
    raise RuntimeError("cannot load UI material atlas builder")
BUILDER = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(BUILDER)


def connected_components(mask: Image.Image) -> list[int]:
    pixels = mask.load()
    remaining = {(x, y) for y in range(mask.height) for x in range(mask.width)
                 if pixels[x, y] >= 48}
    sizes: list[int] = []
    while remaining:
        start = remaining.pop()
        stack = [start]
        size = 0
        while stack:
            x, y = stack.pop()
            size += 1
            for point in ((x - 1, y), (x + 1, y),
                          (x, y - 1), (x, y + 1)):
                if point in remaining:
                    remaining.remove(point)
                    stack.append(point)
        sizes.append(size)
    return sorted(sizes, reverse=True)


class UiMaterialAssetPipelineTests(unittest.TestCase):
    def test_bundled_cjk_font_and_runtime_archive_are_required(self) -> None:
        font = ROOT / "assets/fonts/NotoSansSC[wght].ttf"
        license_file = ROOT / "assets/fonts/OFL.txt"
        self.assertTrue(font.is_file())
        self.assertGreater(font.stat().st_size, 10_000_000)
        self.assertIn("SIL OPEN FONT LICENSE Version 1.1",
                      license_file.read_text(encoding="utf-8"))

        font_source = (ROOT / "src/platform/raylib/death_overlay_font.cpp").read_text(
            encoding="utf-8")
        app_cmake = (ROOT / "src/app/CMakeLists.txt").read_text(encoding="utf-8")
        formal_cmake = (ROOT / "tests/platform/CMakeLists.txt").read_text(
            encoding="utf-8")
        self.assertIn('assets/fonts/NotoSansSC[wght].ttf', font_source)
        self.assertIn('${PROJECT_SOURCE_DIR}/assets/fonts', app_cmake)
        self.assertIn('${PROJECT_SOURCE_DIR}/assets/fonts', formal_cmake)

        contract = (ROOT / "src/platform/raylib/death_overlay_font.hpp").read_text(
            encoding="utf-8")
        self.assertIn("kUiFontSourceBaseSize = 64", contract)
        self.assertIn("kUiFontMaximumDisplaySize = 26", contract)
        self.assertIn("kUiFontAtlasByteBudget", contract)
        for filename in ("hud_renderer.cpp", "pause_menu_renderer.cpp",
                         "death_overlay_renderer.cpp"):
            renderer = (ROOT / "src/platform/raylib" / filename).read_text(
                encoding="utf-8")
            self.assertIn("LoadFontEx(path, kUiFontSourceBaseSize", renderer)
            self.assertIn("TEXTURE_FILTER_BILINEAR", renderer)

    def test_decorated_ui_controls_use_non_stretching_draw_paths(self) -> None:
        hud = (ROOT / "src/platform/raylib/hud_renderer.cpp").read_text(
            encoding="utf-8")
        inventory = (ROOT / "src/platform/raylib/inventory_renderer.cpp").read_text(
            encoding="utf-8")
        pause = (ROOT / "src/platform/raylib/pause_menu_renderer.cpp").read_text(
            encoding="utf-8")
        for element in ("hud_panel", "hud_objective_panel",
                        "hud_navigation_panel"):
            self.assertRegex(
                hud,
                rf"draw_nine_slice\(\s*ui_material_sprite\(\s*UiMaterialElement::{element}\)")
        self.assertIn("draw_horizontal_slice", inventory)
        self.assertIn("draw_horizontal_slice", pause)
        self.assertNotRegex(
            pause,
            r"draw_to\(\s*ui_material_sprite\(op\.selected")

    def test_all_critical_ui_text_uses_the_contrast_contract(self) -> None:
        critical_renderers = (
            "hud_renderer.cpp",
            "inventory_renderer.cpp",
            "material_bag_renderer.cpp",
            "active_skill_renderer.cpp",
            "pause_menu_renderer.cpp",
            "death_overlay_renderer.cpp",
        )
        for filename in critical_renderers:
            source = (ROOT / "src/platform/raylib" / filename).read_text(
                encoding="utf-8")
            self.assertIn('ui_text_contrast.hpp', source, filename)
            self.assertIn('ui_text_contrast_style()', source, filename)
        contract = (ROOT / "src/platform/raylib/ui_text_contrast.hpp").read_text(
            encoding="utf-8")
        self.assertIn("Color primary{248, 246, 238, 255}", contract)
        self.assertIn("Color secondary{194, 229, 255, 255}", contract)
        self.assertIn("Color interaction{194, 229, 255, 255}", contract)
        self.assertIn("Color muted{184, 204, 220, 255}", contract)
        self.assertIn("int outline_pixels{1}", contract)
        self.assertIn("int shadow_pixels{2}", contract)
        self.assertIn("ui_luma_contrast_ratio", contract)
        for filename in critical_renderers:
            source = (ROOT / "src/platform/raylib" / filename).read_text(
                encoding="utf-8")
            self.assertNotIn("DrawTextGradient", source, filename)
            self.assertNotRegex(source, r"DrawTextEx\([^;]*Fade\(", filename)

    def test_paired_atlases_exist_with_matching_alpha(self) -> None:
        color = Image.open(ROOT / "assets/stage12/ui_material.png").convert("RGBA")
        material = Image.open(
            ROOT / "assets/stage12/ui_material_material.png").convert("RGBA")
        self.assertEqual(color.size, (1024, 1024))
        self.assertEqual(material.size, color.size)
        self.assertEqual(color.getchannel("A").tobytes(),
                         material.getchannel("A").tobytes())
        self.assertNotEqual(color.convert("RGB").tobytes(),
                            material.convert("RGB").tobytes())

    def test_every_declared_element_is_unique_detailed_and_safely_anchored(self) -> None:
        atlas = Image.open(ROOT / "assets/stage12/ui_material.png").convert("RGBA")
        signatures: set[bytes] = set()
        for name, cell in BUILDER.UI_CELLS.items():
            tile = BUILDER.crop_cell(atlas, cell)
            alpha = tile.getchannel("A")
            bbox = alpha.point(lambda value: 255 if value >= 48 else 0).getbbox()
            self.assertIsNotNone(bbox, name)
            assert bbox is not None
            self.assertGreaterEqual(bbox[0], 3, name)
            self.assertGreaterEqual(bbox[1], 3, name)
            self.assertLessEqual(bbox[2], BUILDER.CELL - 3, name)
            self.assertLessEqual(bbox[3], BUILDER.CELL - 3, name)
            visible = [pixel for pixel in tile.get_flattened_data()
                       if pixel[3] >= 96]
            self.assertGreater(len({pixel[:3] for pixel in visible}), 32, name)
            signature = BUILDER.tile_signature(tile)
            self.assertNotIn(signature, signatures, name)
            signatures.add(signature)

    def test_tiles_have_authored_contours_and_dark_steel_gold_energy_palette(self) -> None:
        atlas = Image.open(ROOT / "assets/stage12/ui_material.png").convert("RGBA")
        palette_hits = {"dark": 0, "gold": 0, "energy": 0}
        for name, cell in BUILDER.UI_CELLS.items():
            tile = BUILDER.crop_cell(atlas, cell)
            alpha = tile.getchannel("A")
            components = connected_components(alpha)
            self.assertTrue(components, name)
            self.assertGreaterEqual(components[0], 180, name)
            inner = ImageChops.subtract(alpha, alpha.filter(ImageFilter.MinFilter(3)))
            edge_pixels = [pixel for pixel, edge in zip(tile.get_flattened_data(),
                inner.get_flattened_data()) if edge >= 12]
            self.assertGreater(len(edge_pixels), 40, name)
            edge_luminance = [(p[0] * 54 + p[1] * 183 + p[2] * 19) / 256
                              for p in edge_pixels]
            self.assertGreater(statistics.pstdev(edge_luminance), 10.0, name)
            for red, green, blue, visible in tile.get_flattened_data():
                if visible < 96:
                    continue
                palette_hits["dark"] += red < 82 and green < 96 and blue < 116
                palette_hits["gold"] += red > 128 and green > 88 and red > blue * 1.25
                palette_hits["energy"] += blue > 135 and green > 110 and blue > red * 1.20
        for value in palette_hits.values():
            self.assertGreater(value, 500)

    def test_status_and_state_pairs_are_not_flat_color_repaints(self) -> None:
        atlas = Image.open(ROOT / "assets/stage12/ui_material.png").convert("RGBA")
        groups = (
            ("hud_health_track", "hud_health_fill"),
            ("hud_barrier_track", "hud_barrier_fill"),
            ("hud_resource_track", "hud_resource_fill"),
            ("hud_status_slow", "hud_status_corrosion", "hud_status_invulnerable"),
            ("hud_skill_empty", "hud_skill_ready", "hud_skill_cooldown"),
            ("inventory_slot_idle", "inventory_slot_selected"),
            ("skill_slot_empty", "skill_slot_ready", "skill_slot_selected"),
            ("pause_row_idle", "pause_row_selected"),
        )
        for names in groups:
            masks = [BUILDER.crop_cell(atlas, BUILDER.UI_CELLS[name])
                     .getchannel("A").point(lambda v: 255 if v >= 48 else 0)
                     for name in names]
            for index, mask in enumerate(masks):
                for previous in masks[:index]:
                    difference = ImageStat.Stat(
                        ImageChops.difference(mask, previous)).mean[0]
                    self.assertGreater(difference, 1.5, names)


if __name__ == "__main__":
    unittest.main()
