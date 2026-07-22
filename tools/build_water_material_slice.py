"""Build deterministic water-ecology runtime atlases from original source art."""

from __future__ import annotations

from math import pi, sin
from pathlib import Path

from PIL import Image, ImageChops, ImageDraw, ImageEnhance, ImageFilter


ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "art_source" / "stage12"
OUTPUT = ROOT / "assets" / "stage12"
ENV_SIZE = 768
MONSTER_SIZE = 864
CELL = 96
COLUMNS = 9


def cover(image: Image.Image, size: tuple[int, int]) -> Image.Image:
    scale = max(size[0] / image.width, size[1] / image.height)
    resized = image.resize((round(image.width * scale), round(image.height * scale)),
                           Image.Resampling.LANCZOS)
    left = (resized.width - size[0]) // 2
    top = (resized.height - size[1]) // 2
    return resized.crop((left, top, left + size[0], top + size[1]))


def contain(image: Image.Image, size: tuple[int, int]) -> Image.Image:
    result = image.copy()
    result.thumbnail(size, Image.Resampling.LANCZOS)
    canvas = Image.new("RGBA", size)
    canvas.alpha_composite(result,
        ((size[0] - result.width) // 2, (size[1] - result.height) // 2))
    return canvas


def material_map(color: Image.Image) -> Image.Image:
    rgba = color.convert("RGBA")
    r, g, b, a = rgba.split()
    luminance = Image.merge("RGB", (r, g, b)).convert("L")
    roughness = ImageChops.invert(luminance).point(lambda value: 70 + value * 150 // 255)
    cyan = ImageChops.subtract(b, r).point(lambda value: min(255, value * 3))
    metal = luminance.point(lambda value: 65 + value * 145 // 255)
    return Image.merge("RGBA", (roughness, cyan, metal, a))


def build_environment() -> None:
    source = Image.open(SOURCE / "water-environment-concept-v1.png").convert("RGBA")
    atlas = Image.new("RGBA", (ENV_SIZE, ENV_SIZE))
    room = ImageEnhance.Contrast(cover(source, (512, 512))).enhance(1.08)
    atlas.alpha_composite(room, (0, 0))
    crops = {
        (512, 0): (360, 0, 900, 350),       # sealed door
        (512, 256): (690, 900, 1240, 1254), # bronze drainage grate
        (0, 512): (20, 880, 690, 1254),     # flooded abyss opening
        (256, 512): (965, 420, 1254, 840),  # cyan glass lantern
        (512, 512): (1000, 295, 1254, 620), # mineral coral
    }
    for position, box in crops.items():
        prop = contain(source.crop(box), (256, 256))
        atlas.alpha_composite(prop, position)
    atlas.save(OUTPUT / "water_environment.png")
    material_map(atlas).save(OUTPUT / "water_environment_material.png")


def alpha_bbox(image: Image.Image) -> tuple[int, int, int, int]:
    bbox = image.getchannel("A").getbbox()
    if bbox is None:
        raise RuntimeError("source character has no visible pixels")
    return bbox


def transformed_actor(actor: Image.Image, index: int, role: str) -> Image.Image:
    scale = 2
    canvas = Image.new("RGBA", (CELL * scale, CELL * scale))
    if index < 12:
        local, count, state = index, 12, "idle"
    elif index < 28:
        local, count, state = index - 12, 16, "move"
    elif index < 48:
        local, count, state = index - 28, 20, "special"
    elif index < 56:
        local, count, state = index - 48, 8, "hurt"
    else:
        local, count, state = index - 56, 16, "death"
    phase = 2.0 * pi * local / count
    x_shift = 0
    y_shift = round(sin(phase) * 3)
    angle = 0.0
    opacity = 255
    width = 168 if role == "bulwark" else 158
    height = 178 if role == "bulwark" else 184
    if state == "move":
        x_shift = round(sin(phase) * 7)
        y_shift = round(abs(sin(phase)) * -7)
        angle = sin(phase) * 2.2
    elif state == "special":
        x_shift = round(sin(phase) * 4)
        y_shift = round(sin(phase * 2.0) * 3)
        angle = sin(phase) * 3.0
    elif state == "hurt":
        x_shift = round((1.0 - local / count) * -10)
        angle = -7.0 + local * 0.75
    elif state == "death":
        progress = local / (count - 1)
        angle = -70.0 * progress
        x_shift = round(-16 * progress)
        y_shift = round(50 * progress)
        opacity = round(255 - 95 * progress)
    sprite = actor.resize((width, height), Image.Resampling.LANCZOS)
    if state == "idle":
        breathe = round(2.0 * sin(phase))
        sprite = sprite.resize((width + breathe, height - breathe),
            Image.Resampling.LANCZOS)
    if state == "hurt":
        white = Image.new("RGBA", sprite.size, (190, 236, 255, 0))
        white.putalpha(sprite.getchannel("A").point(lambda alpha: alpha // 3))
        sprite = Image.alpha_composite(sprite, white)
    if opacity != 255:
        sprite.putalpha(sprite.getchannel("A").point(lambda alpha: alpha * opacity // 255))
    sprite = sprite.rotate(angle, Image.Resampling.BICUBIC, expand=True)
    position = ((canvas.width - sprite.width) // 2 + x_shift,
                canvas.height - sprite.height - 4 + y_shift)
    canvas.alpha_composite(sprite, position)

    draw = ImageDraw.Draw(canvas, "RGBA")
    if state == "special":
        pulse = local / (count - 1)
        radius = round(22 + 38 * sin(pi * pulse))
        center = (140 if role == "support" else 133, 83)
        for band in range(3):
            inset = band * 8
            draw.ellipse((center[0] - radius - inset, center[1] - radius - inset,
                          center[0] + radius + inset, center[1] + radius + inset),
                         outline=(72, 221, 255, 185 - band * 42), width=3)
        if role == "bulwark":
            draw.arc((94, 25, 184, 178), 252, 108,
                     fill=(156, 240, 255, 220), width=6)
    elif state == "move":
        for drop in range(3):
            dx = 55 - drop * 13 - (local * 4) % 18
            dy = 156 + drop * 7
            draw.ellipse((dx, dy, dx + 4, dy + 8), fill=(75, 205, 238, 145))
    return canvas.resize((CELL, CELL), Image.Resampling.LANCZOS)


def build_monster(role: str) -> None:
    source = Image.open(SOURCE / f"water-{role}-alpha-v1.png").convert("RGBA")
    actor = source.crop(alpha_bbox(source))
    atlas = Image.new("RGBA", (MONSTER_SIZE, MONSTER_SIZE))
    for index in range(72):
        frame = transformed_actor(actor, index, role)
        atlas.alpha_composite(frame,
            ((index % COLUMNS) * CELL, (index // COLUMNS) * CELL))
    atlas.save(OUTPUT / f"water_{role}.png")
    material_map(atlas).save(OUTPUT / f"water_{role}_material.png")


def main() -> None:
    OUTPUT.mkdir(parents=True, exist_ok=True)
    build_environment()
    build_monster("bulwark")
    build_monster("support")
    for name in ("water_environment", "water_bulwark", "water_support"):
        image = Image.open(OUTPUT / f"{name}.png")
        print(f"wrote {name}: {image.width}x{image.height} RGBA")


if __name__ == "__main__":
    main()
