"""Build water-ecology atlases from complete authored animation poses."""

from __future__ import annotations

from pathlib import Path

from PIL import Image, ImageChops, ImageEnhance


ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "art_source" / "stage12"
OUTPUT = ROOT / "assets" / "stage12"
ENV_SIZE = 768
MONSTER_SIZE = 864
CELL = 96
COLUMNS = 9
SEQUENCES = {
    "bulwark": (
        ("idle", "water-bulwark-idle-12-alpha-v3.png", 4, 3, 12),
        ("move", "water-bulwark-move-16-alpha-v5.png", 4, 4, 16),
        ("special", "water-bulwark-special-20-alpha-v3.png", 5, 4, 20),
        ("hurt", "water-bulwark-hurt-8-alpha-v3.png", 4, 2, 8),
        ("death", "water-bulwark-death-16-alpha-v4.png", 4, 4, 16),
    ),
    "support": (
        ("idle", "water-support-idle-12-alpha-v4.png", 4, 3, 12),
        ("move", "water-support-move-16-alpha-v3.png", 4, 4, 16),
        ("special", "water-support-special-20-alpha-v5.png", 5, 4, 20),
        ("hurt", "water-support-hurt-8-alpha-v4.png", 4, 2, 8),
        ("death", "water-support-death-16-alpha-v3.png", 4, 4, 16),
    ),
}


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
    from build_environment_props import _atomic_image, build_ecology_environment
    atlas, material, _ = build_ecology_environment("water", ROOT)
    _atomic_image(OUTPUT / "water_environment.png", atlas)
    _atomic_image(OUTPUT / "water_environment_material.png", material)


def alpha_bbox(image: Image.Image) -> tuple[int, int, int, int]:
    bbox = image.getchannel("A").getbbox()
    if bbox is None:
        raise RuntimeError("source character has no visible pixels")
    return bbox


def sequence_frames(role: str) -> list[Image.Image]:
    frames: list[Image.Image] = []
    for state, filename, columns, rows, frame_count in SEQUENCES[role]:
        board = Image.open(SOURCE / "water_sequences" / filename).convert("RGBA")
        poses: list[Image.Image] = []
        for frame in range(frame_count):
            column = frame % columns
            row = frame // columns
            pose = board.crop((
                round(column * board.width / columns),
                round(row * board.height / rows),
                round((column + 1) * board.width / columns),
                round((row + 1) * board.height / rows)))
            alpha = pose.getchannel("A").point(
                lambda value: 0 if value < 64 else (
                    255 if value > 160 else (value - 64) * 255 // 96))
            pose.putalpha(alpha)
            poses.append(pose.crop(alpha_bbox(pose)))
        scale = min(184.0 / max(pose.height for pose in poses),
                    188.0 / max(pose.width for pose in poses))
        for pose in poses:
            pose = pose.resize((round(pose.width * scale),
                                round(pose.height * scale)),
                               Image.Resampling.LANCZOS)
            pose = pose.crop(alpha_bbox(pose))
            canvas = Image.new("RGBA", (CELL * 2, CELL * 2))
            canvas.alpha_composite(pose,
                ((canvas.width - pose.width) // 2, 186 - pose.height))
            frames.append(canvas.resize(
                (CELL, CELL), Image.Resampling.LANCZOS))
        if len(poses) != frame_count:
            raise RuntimeError(f"{role}/{state}: incomplete pose sheet")
    return frames


def validate_monster_frames(atlas: Image.Image, role: str) -> None:
    frame_offset = 0
    for state, frame_count in (
            ("idle", 12), ("move", 16), ("special", 20),
            ("hurt", 8), ("death", 16)):
        frames = []
        for local_frame in range(frame_count):
            cell = frame_offset + local_frame
            x = cell % COLUMNS * CELL
            y = cell // COLUMNS * CELL
            frames.append(atlas.crop((x, y, x + CELL, y + CELL)))
        if len({frame.tobytes() for frame in frames}) != frame_count:
            raise RuntimeError(f"{role}/{state}: duplicate whole frames")
        for local_frame, (current, following) in enumerate(
                zip(frames, frames[1:])):
            visible_union = 0
            changed = 0
            for lhs, rhs in zip(current.get_flattened_data(),
                                following.get_flattened_data()):
                if lhs[3] > 24 or rhs[3] > 24:
                    visible_union += 1
                if sum(abs(lhs[channel] - rhs[channel])
                       for channel in range(4)) >= 48:
                    changed += 1
            percent = changed * 100.0 / visible_union
            if percent < 3.0:
                raise RuntimeError(
                    f"{role}/{state} frame {local_frame}->{local_frame + 1}: "
                    f"only {percent:.2f}% pixels changed")
        for local_frame, frame in enumerate(frames):
            visible = {
                (x, y) for y in range(CELL) for x in range(CELL)
                if frame.getpixel((x, y))[3] > 96}
            remaining = set(visible)
            components: list[int] = []
            while remaining:
                stack = [remaining.pop()]
                size = 0
                while stack:
                    x, y = stack.pop()
                    size += 1
                    for dy in (-1, 0, 1):
                        for dx in (-1, 0, 1):
                            neighbor = (x + dx, y + dy)
                            if neighbor in remaining:
                                remaining.remove(neighbor)
                                stack.append(neighbor)
                components.append(size)
            components.sort(reverse=True)
            second = components[1] if len(components) > 1 else 0
            if (not components or components[0] * 100 < len(visible) * 94
                    or second * 100 > len(visible) * 2):
                raise RuntimeError(
                    f"{role}/{state} frame {local_frame}: disconnected silhouette")
        frame_offset += frame_count


def build_monster(role: str) -> None:
    atlas = Image.new("RGBA", (MONSTER_SIZE, MONSTER_SIZE))
    for frame_index, frame in enumerate(sequence_frames(role)):
        atlas.alpha_composite(frame,
            ((frame_index % COLUMNS) * CELL,
             (frame_index // COLUMNS) * CELL))
    validate_monster_frames(atlas, role)
    atlas.save(OUTPUT / f"water_{role}.png")
    material_map(atlas).save(OUTPUT / f"water_{role}_material.png")


def build_common_material_maps() -> None:
    for name in ("environment", "actors", "effects_ui"):
        color = Image.open(OUTPUT / f"{name}.png").convert("RGBA")
        material_map(color).save(OUTPUT / f"{name}_material.png")


def main() -> None:
    OUTPUT.mkdir(parents=True, exist_ok=True)
    build_environment()
    build_monster("bulwark")
    build_monster("support")
    build_common_material_maps()
    for name in ("water_environment", "water_bulwark", "water_support"):
        image = Image.open(OUTPUT / f"{name}.png")
        print(f"wrote {name}: {image.width}x{image.height} RGBA")


if __name__ == "__main__":
    main()
