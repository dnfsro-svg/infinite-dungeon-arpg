"""Build the original dark-steel/ancient-gold common UI atlas pair."""

from __future__ import annotations

import hashlib
from pathlib import Path
import statistics

from PIL import Image, ImageChops, ImageFilter


ROOT = Path(__file__).resolve().parents[1]
SOURCE = (ROOT / "art_source/stage12/ui"
          / "dark-steel-ancient-gold-ui-board-v1.png")
OUTPUT = ROOT / "assets/stage12"
ATLAS_SIZE = 1024
CELL = 128
SOURCE_COLUMNS = 8
SOURCE_ROWS = 5
SAFE_SIZE = 120

UI_NAMES = (
    "hud_panel", "hud_health_track", "hud_health_fill",
    "hud_barrier_track", "hud_barrier_fill", "hud_resource_track",
    "hud_resource_fill", "hud_status_slow",
    "hud_status_corrosion", "hud_status_invulnerable",
    "hud_objective_panel", "hud_navigation_panel", "hud_notice",
    "hud_notice_abyss", "hud_skill_empty", "hud_skill_ready",
    "hud_skill_cooldown", "inventory_panel_equipment",
    "inventory_panel_grid", "inventory_panel_detail", "inventory_tab_idle",
    "inventory_tab_active", "inventory_slot_idle", "inventory_slot_selected",
    "inventory_button_idle", "inventory_button_active",
    "inventory_button_disabled", "skill_panel", "skill_slot_empty",
    "skill_slot_ready", "skill_slot_selected", "skill_slot_support",
    "pause_panel", "pause_row_idle", "pause_row_selected", "pause_footer",
    "warning_modal", "label_plate", "reinforcement_confirm",
    "reinforcement_cancel",
)
UI_CELLS = {name: index for index, name in enumerate(UI_NAMES)}


def source_cell(board: Image.Image, index: int) -> Image.Image:
    column = index % SOURCE_COLUMNS
    row = index // SOURCE_COLUMNS
    return board.crop((
        round(column * board.width / SOURCE_COLUMNS),
        round(row * board.height / SOURCE_ROWS),
        round((column + 1) * board.width / SOURCE_COLUMNS),
        round((row + 1) * board.height / SOURCE_ROWS),
    ))


def _key_color(source: Image.Image) -> tuple[int, int, int]:
    rgb = source.convert("RGB")
    border = []
    for x in range(rgb.width):
        border.extend((rgb.getpixel((x, 0)), rgb.getpixel((x, rgb.height - 1))))
    for y in range(rgb.height):
        border.extend((rgb.getpixel((0, y)), rgb.getpixel((rgb.width - 1, y))))
    candidates = [pixel for pixel in border
                  if pixel[0] >= 150 and pixel[2] >= 150
                  and pixel[1] <= 120 and abs(pixel[0] - pixel[2]) <= 75]
    if not candidates:
        candidates = border
    return tuple(round(statistics.median(pixel[channel]
                                         for pixel in candidates))
                 for channel in range(3))


def _connected_background_mask(source: Image.Image) -> bytearray:
    rgba = source.convert("RGBA")
    pixels = list(rgba.get_flattened_data())
    key = _key_color(rgba)
    broad = bytearray(len(pixels))
    for index, (red, green, blue, _) in enumerate(pixels):
        distance = max(abs(red - key[0]), abs(green - key[1]),
                       abs(blue - key[2]))
        key_hue = (red >= 120 and blue >= 120 and green <= 145
                   and min(red, blue) >= green * 1.22
                   and abs(red - blue) <= 105)
        broad[index] = key_hue and distance <= 115
    width = rgba.width
    height = rgba.height
    remaining = bytearray(broad)
    background = bytearray(len(pixels))
    starts = list(range(width)) + list(range((height - 1) * width, height * width))
    starts += [row * width for row in range(height)]
    starts += [row * width + width - 1 for row in range(height)]
    stack = []
    for point in starts:
        if remaining[point]:
            remaining[point] = 0
            stack.append(point)
    while stack:
        point = stack.pop()
        background[point] = 1
        x = point % width
        y = point // width
        for neighbor in ((point - 1) if x > 0 else -1,
                         (point + 1) if x + 1 < width else -1,
                         (point - width) if y > 0 else -1,
                         (point + width) if y + 1 < height else -1):
            if neighbor >= 0 and remaining[neighbor]:
                remaining[neighbor] = 0
                stack.append(neighbor)
    return background


def remove_magenta_key(source: Image.Image) -> Image.Image:
    rgba = source.convert("RGBA")
    background = _connected_background_mask(rgba)
    background_image = Image.new("L", rgba.size)
    background_image.putdata([255 if value else 0 for value in background])
    edge_zone = background_image.filter(ImageFilter.MaxFilter(5))
    edge_pixels = edge_zone.load()
    key = _key_color(rgba)
    pixels = []
    for index, (red, green, blue, source_alpha) in enumerate(
            rgba.get_flattened_data()):
        x = index % rgba.width
        y = index // rgba.width
        alpha = 0 if background[index] else source_alpha
        key_mix = (red >= 70 and blue >= 70
                   and min(red, blue) >= green * 1.28
                   and abs(red - blue) <= 110)
        if alpha and edge_pixels[x, y] and key_mix:
            distance = max(abs(red - key[0]), abs(green - key[1]),
                           abs(blue - key[2]))
            coverage = min(1.0, max(0.0, (distance - 42.0) / 145.0))
            alpha = round(source_alpha * coverage)
            if coverage > 0.05:
                key_share = 1.0 - coverage
                red = max(0, min(255, round(
                    (red - key_share * key[0]) / coverage)))
                green = max(0, min(255, round(
                    (green - key_share * key[1]) / coverage)))
                blue = max(0, min(255, round(
                    (blue - key_share * key[2]) / coverage)))
            else:
                alpha = 0
        if alpha == 0:
            red = green = blue = 0
        pixels.append((red, green, blue, alpha))
    result = Image.new("RGBA", rgba.size)
    result.putdata(pixels)
    return result


def contain_tile(source: Image.Image) -> Image.Image:
    alpha = source.getchannel("A")
    threshold = alpha.point(lambda value: 255 if value >= 48 else 0)
    bbox = threshold.getbbox()
    if bbox is None:
        raise RuntimeError("UI source cell contains no authored subject")
    tile = source.crop(bbox)
    tile.thumbnail((SAFE_SIZE, SAFE_SIZE), Image.Resampling.LANCZOS)
    canvas = Image.new("RGBA", (CELL, CELL))
    canvas.alpha_composite(tile, ((CELL - tile.width) // 2,
                                  (CELL - tile.height) // 2))
    return canvas


def authored_tiles(board: Image.Image) -> list[Image.Image]:
    keyed = remove_magenta_key(board)
    alpha = keyed.getchannel("A")
    pixels = alpha.load()
    remaining = {(x, y) for y in range(alpha.height) for x in range(alpha.width)
                 if pixels[x, y] >= 48}
    components: list[tuple[int, tuple[int, int, int, int]]] = []
    while remaining:
        start = remaining.pop()
        stack = [start]
        size = 0
        min_x = max_x = start[0]
        min_y = max_y = start[1]
        while stack:
            x, y = stack.pop()
            size += 1
            min_x = min(min_x, x)
            max_x = max(max_x, x)
            min_y = min(min_y, y)
            max_y = max(max_y, y)
            for point in ((x - 1, y), (x + 1, y),
                          (x, y - 1), (x, y + 1)):
                if point in remaining:
                    remaining.remove(point)
                    stack.append(point)
        if size > 100:
            components.append((size, (min_x, min_y, max_x + 1, max_y + 1)))
    if len(components) != len(UI_NAMES):
        raise RuntimeError(
            f"expected {len(UI_NAMES)} authored UI subjects, got {len(components)}")
    ordered: list[tuple[int, tuple[int, int, int, int]]] = []
    by_y = sorted(components, key=lambda value: (value[1][1] + value[1][3]) / 2)
    for row in range(SOURCE_ROWS):
        row_components = by_y[row * SOURCE_COLUMNS:(row + 1) * SOURCE_COLUMNS]
        ordered.extend(sorted(row_components,
            key=lambda value: (value[1][0] + value[1][2]) / 2))
    return [keyed.crop(bbox) for _, bbox in ordered]


def crop_cell(atlas: Image.Image, cell: int) -> Image.Image:
    column = cell % (ATLAS_SIZE // CELL)
    row = cell // (ATLAS_SIZE // CELL)
    return atlas.crop((column * CELL, row * CELL,
                       (column + 1) * CELL, (row + 1) * CELL))


def tile_signature(tile: Image.Image) -> bytes:
    normalized = tile.convert("RGBA").resize((32, 32), Image.Resampling.LANCZOS)
    return hashlib.sha256(normalized.tobytes()).digest()


def build_color_atlas() -> Image.Image:
    board = Image.open(SOURCE).convert("RGBA")
    atlas = Image.new("RGBA", (ATLAS_SIZE, ATLAS_SIZE))
    signatures: set[bytes] = set()
    tiles = authored_tiles(board)
    for index, name in enumerate(UI_NAMES):
        tile = contain_tile(tiles[index])
        signature = tile_signature(tile)
        if signature in signatures:
            raise RuntimeError(f"{name}: duplicate authored tile")
        signatures.add(signature)
        x = (index % 8) * CELL
        y = (index // 8) * CELL
        atlas.alpha_composite(tile, (x, y))
    return atlas


def material_map(color: Image.Image) -> Image.Image:
    rgba = color.convert("RGBA")
    red, green, blue, alpha = rgba.split()
    rgb = Image.merge("RGB", (red, green, blue))
    luminance = rgb.convert("L")
    saturation = rgb.convert("HSV").split()[1]
    roughness = ImageChops.invert(luminance).point(
        lambda value: 48 + value * 150 // 255)
    blue_energy = ImageChops.subtract(blue, red).point(
        lambda value: min(255, value * 4))
    gold_energy = ImageChops.subtract(red, blue).point(
        lambda value: min(255, value * 2))
    emissive = ImageChops.lighter(blue_energy, gold_energy)
    emissive = emissive.filter(ImageFilter.GaussianBlur(0.45))
    metalness = ImageChops.invert(saturation).point(
        lambda value: 74 + value * 168 // 255)
    return Image.merge("RGBA", (roughness, emissive, metalness, alpha))


def main() -> None:
    if not SOURCE.is_file():
        raise RuntimeError(f"missing authored UI board: {SOURCE}")
    OUTPUT.mkdir(parents=True, exist_ok=True)
    color = build_color_atlas()
    material = material_map(color)
    color.save(OUTPUT / "ui_material.png")
    material.save(OUTPUT / "ui_material_material.png")
    print(f"built {len(UI_NAMES)} UI materials from {SOURCE.name}")


if __name__ == "__main__":
    main()
