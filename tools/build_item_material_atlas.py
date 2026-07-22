"""Build the original dark-steel/ancient-gold item color/material atlas pair."""

from __future__ import annotations

import hashlib
from pathlib import Path

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


def remove_magenta_key(source: Image.Image) -> Image.Image:
    rgba = source.convert("RGBA")
    pixels = []
    for red, green, blue, source_alpha in rgba.get_flattened_data():
        distance = max(abs(255 - red), green, abs(255 - blue))
        if distance <= 10:
            alpha = 0
        elif distance >= 72:
            alpha = source_alpha
        else:
            alpha = round(source_alpha * (distance - 10) / 62)
        if alpha < 255:
            key_fraction = (255 - alpha) / 255.0
            red = max(0, round(red - 180 * key_fraction))
            blue = max(0, round(blue - 180 * key_fraction))
        pixels.append((red, green, blue, alpha))
    result = Image.new("RGBA", rgba.size)
    result.putdata(pixels)
    return result


def contain_icon(source: Image.Image) -> Image.Image:
    alpha = source.getchannel("A")
    bbox = alpha.point(lambda value: 255 if value >= 16 else 0).getbbox()
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
