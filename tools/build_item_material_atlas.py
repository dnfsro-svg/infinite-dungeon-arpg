"""Build the original dark-steel/ancient-gold item color/material atlas pair."""

from __future__ import annotations

import hashlib
from pathlib import Path
import statistics

from PIL import Image, ImageChops, ImageFilter, ImageStat


ROOT = Path(__file__).resolve().parents[1]
SOURCE = (ROOT / "art_source" / "stage12" / "items"
          / "dark-steel-ancient-gold-item-board-v1.png")
OUTPUT = ROOT / "assets" / "stage12"
ATLAS_SIZE = 1024
CELL = 128
SOURCE_COLUMNS = 6
SOURCE_ROWS = 5
SAFE_ICON_SIZE = 116

ICON_NAMES = (
    "equipment_weapon", "equipment_helmet", "equipment_chest",
    "equipment_gloves", "equipment_boots", "equipment_accessory",
    "rarity_normal", "rarity_magic", "rarity_rare", "rarity_abyss",
    "skill_active", "skill_support",
    "material_transmute", "material_augment", "material_regal",
    "material_chaos", "material_exalt", "material_annul",
    "material_divine", "material_scour", "material_directed",
    "material_reinforcement", "material_coupon_6", "material_coupon_9",
    "material_coupon_12", "material_coupon_15",
    "frame_corner_nw", "frame_corner_ne", "frame_corner_sw",
    "frame_corner_se",
)
ICON_CELLS = {name: index for index, name in enumerate(ICON_NAMES)}


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
    key_candidates = [pixel for pixel in border
                      if pixel[0] >= 180 and pixel[2] >= 180
                      and pixel[1] <= 80 and abs(pixel[0] - pixel[2]) <= 55]
    if not key_candidates:
        key_candidates = border
    return tuple(round(statistics.median(pixel[channel]
                                         for pixel in key_candidates))
                 for channel in range(3))


def _connected_background_mask(source: Image.Image) -> bytearray:
    rgba = source.convert("RGBA")
    pixels = list(rgba.get_flattened_data())
    key = _key_color(rgba)
    broad = bytearray(len(pixels))
    strong = bytearray(len(pixels))
    for index, (red, green, blue, _) in enumerate(pixels):
        distance = max(abs(red - key[0]), abs(green - key[1]),
                       abs(blue - key[2]))
        key_hue = (red >= 150 and blue >= 150 and green <= 110
                   and abs(red - blue) <= 62)
        broad[index] = key_hue and distance <= 72
        strong[index] = key_hue and distance <= 32

    width = rgba.width
    height = rgba.height
    remaining = bytearray(broad)
    background = bytearray(len(pixels))
    for start, present in enumerate(remaining):
        if not present:
            continue
        stack = [start]
        remaining[start] = 0
        component = []
        strong_count = 0
        touches_edge = False
        while stack:
            point = stack.pop()
            component.append(point)
            strong_count += bool(strong[point])
            x = point % width
            y = point // width
            touches_edge = touches_edge or x == 0 or y == 0 \
                or x == width - 1 or y == height - 1
            if x > 0 and remaining[point - 1]:
                remaining[point - 1] = 0
                stack.append(point - 1)
            if x + 1 < width and remaining[point + 1]:
                remaining[point + 1] = 0
                stack.append(point + 1)
            if y > 0 and remaining[point - width]:
                remaining[point - width] = 0
                stack.append(point - width)
            if y + 1 < height and remaining[point + width]:
                remaining[point + width] = 0
                stack.append(point + width)
        enclosed_key = len(component) >= 32 and strong_count * 5 >= len(component) * 4
        if touches_edge or enclosed_key:
            for point in component:
                background[point] = 1
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
                   and min(red, blue) >= green * 1.35
                   and abs(red - blue) <= 100)
        if alpha != 0 and edge_pixels[x, y] != 0 and key_mix:
            distance = max(abs(red - key[0]), abs(green - key[1]),
                           abs(blue - key[2]))
            coverage = min(1.0, max(0.0, (distance - 55.0) / 125.0))
            if coverage < 1.0:
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


def _subject_components(alpha: Image.Image, threshold: int = 24) \
        -> list[tuple[int, tuple[int, int, int, int], tuple[tuple[int, int], ...]]]:
    pixels = alpha.load()
    remaining = {(x, y) for y in range(alpha.height) for x in range(alpha.width)
                 if pixels[x, y] >= threshold}
    components = []
    while remaining:
        start = remaining.pop()
        stack = [start]
        count = 0
        min_x = max_x = start[0]
        min_y = max_y = start[1]
        points = []
        while stack:
            x, y = stack.pop()
            points.append((x, y))
            count += 1
            min_x = min(min_x, x)
            max_x = max(max_x, x)
            min_y = min(min_y, y)
            max_y = max(max_y, y)
            for neighbor in ((x - 1, y), (x + 1, y),
                             (x, y - 1), (x, y + 1)):
                if neighbor in remaining:
                    remaining.remove(neighbor)
                    stack.append(neighbor)
        components.append((count, (min_x, min_y, max_x + 1, max_y + 1),
                           tuple(points)))
    return sorted(components, key=lambda component: component[0], reverse=True)


def _box_distance(left: tuple[int, int, int, int],
                  right: tuple[int, int, int, int]) -> int:
    horizontal = max(left[0] - right[2], right[0] - left[2], 0)
    vertical = max(left[1] - right[3], right[1] - left[3], 0)
    return max(horizontal, vertical)


def isolate_subject(source: Image.Image) -> Image.Image:
    components = _subject_components(source.getchannel("A"))
    if not components:
        return source
    largest_size, largest_box, _ = components[0]
    minimum_size = max(16, largest_size // 250)
    mask = Image.new("L", source.size)
    mask_pixels = mask.load()
    for size, bbox, points in components:
        if size < minimum_size or _box_distance(bbox, largest_box) > 6:
            continue
        for x, y in points:
            mask_pixels[x, y] = 255
    mask = mask.filter(ImageFilter.MaxFilter(5))
    result = source.copy()
    result.putalpha(ImageChops.multiply(source.getchannel("A"), mask))
    return result


def subject_bbox(source: Image.Image) -> tuple[int, int, int, int] | None:
    components = _subject_components(source.getchannel("A"))
    if not components:
        return None
    boxes = [bbox for _, bbox, _ in components]
    return (min(box[0] for box in boxes), min(box[1] for box in boxes),
            max(box[2] for box in boxes), max(box[3] for box in boxes))


def contain_icon(source: Image.Image) -> Image.Image:
    source = isolate_subject(source)
    bbox = subject_bbox(source)
    if bbox is None:
        raise RuntimeError("item source cell contains no authored icon")
    icon = source.crop(bbox)
    icon.thumbnail((SAFE_ICON_SIZE, SAFE_ICON_SIZE), Image.Resampling.LANCZOS)
    canvas = Image.new("RGBA", (CELL, CELL))
    canvas.alpha_composite(icon, ((CELL - icon.width) // 2,
                                  (CELL - icon.height) // 2))
    return canvas


def crop_cell(atlas: Image.Image, cell: int) -> Image.Image:
    column = cell % (ATLAS_SIZE // CELL)
    row = cell // (ATLAS_SIZE // CELL)
    return atlas.crop((column * CELL, row * CELL,
                       (column + 1) * CELL, (row + 1) * CELL))


def icon_signature(icon: Image.Image) -> bytes:
    normalized = icon.convert("RGBA").resize((32, 32), Image.Resampling.LANCZOS)
    return hashlib.sha256(normalized.tobytes()).digest()


def outline_contrast_score(icon: Image.Image) -> float:
    rgba = icon.convert("RGBA")
    alpha = rgba.getchannel("A")
    expanded = alpha.filter(ImageFilter.MaxFilter(3))
    boundary = ImageChops.subtract(expanded, alpha)
    # Inspect visible pixels immediately inside the alpha edge, not transparent matte.
    inner_boundary = ImageChops.subtract(alpha, alpha.filter(ImageFilter.MinFilter(3)))
    pixels = rgba.load()
    mask = inner_boundary.load()
    dark = 0
    total = 0
    for y in range(CELL):
        for x in range(CELL):
            if mask[x, y] < 12:
                continue
            red, green, blue, _ = pixels[x, y]
            luminance = (red * 54 + green * 183 + blue * 19) // 256
            dark += luminance < 118
            total += 1
    if total == 0 or ImageStat.Stat(boundary).mean[0] <= 0.0:
        return 0.0
    return dark / total


def build_color_atlas() -> Image.Image:
    board = Image.open(SOURCE).convert("RGBA")
    atlas = Image.new("RGBA", (ATLAS_SIZE, ATLAS_SIZE))
    for index, name in enumerate(ICON_NAMES):
        icon = contain_icon(remove_magenta_key(source_cell(board, index)))
        if outline_contrast_score(icon) < 0.18:
            raise RuntimeError(f"{name}: authored outline contrast is too low")
        column = index % (ATLAS_SIZE // CELL)
        row = index // (ATLAS_SIZE // CELL)
        atlas.alpha_composite(icon, (column * CELL, row * CELL))
    return atlas


def material_map(color: Image.Image) -> Image.Image:
    rgba = color.convert("RGBA")
    red, green, blue, alpha = rgba.split()
    rgb = Image.merge("RGB", (red, green, blue))
    luminance = rgb.convert("L")
    saturation = rgb.convert("HSV").split()[1]
    roughness = ImageChops.invert(luminance).point(
        lambda value: 54 + value * 154 // 255)
    blue_energy = ImageChops.subtract(blue, red).point(
        lambda value: min(255, value * 3))
    violet_energy = ImageChops.subtract(
        ImageChops.lighter(red, blue), green).point(
            lambda value: min(255, value * 2))
    gold_energy = ImageChops.subtract(red, blue).point(
        lambda value: min(255, value * 2))
    emissive = ImageChops.lighter(
        blue_energy, ImageChops.lighter(violet_energy, gold_energy))
    metalness = ImageChops.invert(saturation).point(
        lambda value: 62 + value * 176 // 255)
    return Image.merge("RGBA", (roughness, emissive, metalness, alpha))


def main() -> None:
    if not SOURCE.is_file():
        raise RuntimeError(f"missing authored item board: {SOURCE}")
    OUTPUT.mkdir(parents=True, exist_ok=True)
    color = build_color_atlas()
    color.save(OUTPUT / "items_ui.png")
    material_map(color).save(OUTPUT / "items_ui_material.png")
    print(f"built {len(ICON_NAMES)} item icons from {SOURCE.name}")


if __name__ == "__main__":
    main()
