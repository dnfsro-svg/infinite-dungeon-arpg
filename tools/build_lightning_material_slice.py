"""Build and verify original lightning-ecology material atlases."""

from __future__ import annotations

import argparse
from collections import deque
from pathlib import Path

from PIL import Image, ImageChops, ImageEnhance


ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "art_source" / "stage12"
SEQUENCE_SOURCE = SOURCE / "lightning_sequences"
OUTPUT = ROOT / "assets" / "stage12"
ENV_SIZE = 768
MONSTER_SIZE = 864
CELL = 96
COLUMNS = 9
SEQUENCES = {
    "shooter": (
        ("idle", "lightning-shooter-idle-12-alpha-v1.png", 4, 3, 12),
        ("move", "lightning-shooter-move-16-alpha-v1.png", 4, 4, 16),
        ("special", "lightning-shooter-special-20-alpha-v1.png", 5, 4, 20),
        ("hurt", "lightning-shooter-hurt-8-alpha-v2.png", 4, 2, 8),
        ("death", "lightning-shooter-death-16-alpha-v5.png", 4, 4, 16),
    ),
    "dasher": (
        ("idle", "lightning-dasher-idle-12-alpha-v1.png", 4, 3, 12),
        ("move", "lightning-dasher-move-16-alpha-v3.png", 4, 4, 16),
        ("special", "lightning-dasher-special-20-alpha-v3.png", 5, 4, 20),
        ("hurt", "lightning-dasher-hurt-8-alpha-v1.png", 4, 2, 8),
        ("death", "lightning-dasher-death-16-alpha-v2.png", 4, 4, 16),
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
    red, green, blue, alpha = rgba.split()
    luminance = Image.merge("RGB", (red, green, blue)).convert("L")
    roughness = ImageChops.invert(luminance).point(
        lambda value: 60 + value * 155 // 255)
    electric = ImageChops.add(
        ImageChops.subtract(blue, red),
        ImageChops.subtract(green, red)).point(lambda value: min(255, value * 2))
    metal = luminance.point(lambda value: 72 + value * 150 // 255)
    return Image.merge("RGBA", (roughness, electric, metal, alpha))


def boost_warning_accents(image: Image.Image) -> Image.Image:
    """Lift authored brass and warning lamps without recoloring the storm field."""
    result = image.copy()
    pixels = result.load()
    for y in range(result.height):
        for x in range(result.width):
            red, green, blue, alpha = pixels[x, y]
            if (alpha > 96 and red >= green + 12 and green >= blue + 12
                    and red > 80):
                pixels[x, y] = (
                    max(red, 194), max(green, 154), min(blue, 88), alpha)
    return result


def build_environment() -> None:
    source = Image.open(SOURCE / "lightning-environment-concept-v1.png").convert("RGBA")
    atlas = Image.new("RGBA", (ENV_SIZE, ENV_SIZE))
    room = ImageEnhance.Contrast(cover(source, (512, 512))).enhance(1.10)
    atlas.alpha_composite(room, (0, 0))
    crops = {
        (512, 0): (350, 0, 910, 470),       # sealed storm door
        (512, 256): (860, 100, 1254, 610),  # capacitor bank
        (0, 512): (0, 300, 470, 790),       # energized abyss hole
        (256, 512): (0, 40, 320, 470),      # arc lamp
        (512, 512): (900, 540, 1254, 1080), # grounding rod
    }
    for position, box in crops.items():
        atlas.alpha_composite(contain(source.crop(box), (256, 256)), position)
    atlas = boost_warning_accents(atlas)
    atlas.save(OUTPUT / "lightning_environment.png")
    material_map(atlas).save(OUTPUT / "lightning_environment_material.png")


def alpha_bbox(image: Image.Image) -> tuple[int, int, int, int]:
    bbox = image.getchannel("A").getbbox()
    if bbox is None:
        raise RuntimeError("source character has no visible pixels")
    return bbox


def split_board(board: Image.Image, columns: int, rows: int,
                frame_count: int) -> list[Image.Image]:
    frames: list[Image.Image] = []
    for frame in range(frame_count):
        column = frame % columns
        row = frame // columns
        tile = board.crop((
            round(column * board.width / columns),
            round(row * board.height / rows),
            round((column + 1) * board.width / columns),
            round((row + 1) * board.height / rows)))
        alpha = tile.getchannel("A").point(
            lambda value: 0 if value < 24 else (
                255 if value > 128 else (value - 24) * 255 // 104))
        tile.putalpha(alpha)
        frames.append(tile.crop(alpha_bbox(tile)))
    return frames


def connected_components(frame: Image.Image) -> tuple[int, int, int]:
    alpha = frame.getchannel("A")
    visible = {(x, y) for y in range(frame.height) for x in range(frame.width)
               if alpha.getpixel((x, y)) > 96}
    remaining = set(visible)
    components: list[int] = []
    while remaining:
        queue = deque([remaining.pop()])
        size = 0
        while queue:
            x, y = queue.popleft()
            size += 1
            for dy in (-1, 0, 1):
                for dx in (-1, 0, 1):
                    neighbor = (x + dx, y + dy)
                    if neighbor in remaining:
                        remaining.remove(neighbor)
                        queue.append(neighbor)
        components.append(size)
    components.sort(reverse=True)
    return len(visible), components[0] if components else 0, (
        components[1] if len(components) > 1 else 0)


def remove_chroma_islands(frame: Image.Image) -> Image.Image:
    """Drop only tiny detached alpha specks left by chroma-key antialiasing."""
    result = frame.copy()
    alpha = result.getchannel("A")
    remaining = {(x, y) for y in range(result.height) for x in range(result.width)
                 if alpha.getpixel((x, y)) > 96}
    components: list[set[tuple[int, int]]] = []
    while remaining:
        component = {remaining.pop()}
        queue = deque(component)
        while queue:
            x, y = queue.popleft()
            for dy in (-1, 0, 1):
                for dx in (-1, 0, 1):
                    neighbor = (x + dx, y + dy)
                    if neighbor in remaining:
                        remaining.remove(neighbor)
                        component.add(neighbor)
                        queue.append(neighbor)
        components.append(component)
    if not components:
        return result
    main = max(components, key=len)
    pixels = alpha.load()
    for component in components:
        if component is main or len(component) > 47:
            continue
        for x, y in component:
            pixels[x, y] = 0
    result.putalpha(alpha)
    return result


def normalized_frames(board_path: Path, columns: int, rows: int,
                      frame_count: int) -> list[Image.Image]:
    board = Image.open(board_path).convert("RGBA")
    poses = split_board(board, columns, rows, frame_count)
    scale = min(184.0 / max(pose.height for pose in poses),
                188.0 / max(pose.width for pose in poses))
    frames: list[Image.Image] = []
    for pose in poses:
        pose = pose.resize((round(pose.width * scale), round(pose.height * scale)),
                           Image.Resampling.LANCZOS)
        pose = pose.crop(alpha_bbox(pose))
        canvas = Image.new("RGBA", (CELL * 2, CELL * 2))
        canvas.alpha_composite(pose,
            ((canvas.width - pose.width) // 2, 186 - pose.height))
        frames.append(remove_chroma_islands(
            canvas.resize((CELL, CELL), Image.Resampling.LANCZOS)))
    return frames


def validate_frames(frames: list[Image.Image], label: str) -> None:
    if len({frame.tobytes() for frame in frames}) != len(frames):
        raise RuntimeError(f"{label}: duplicate whole frames")
    for index, frame in enumerate(frames):
        visible, largest, second = connected_components(frame)
        if visible == 0 or largest * 100 < visible * 94 or second * 100 > visible * 2:
            raise RuntimeError(
                f"{label} frame {index}: disconnected silhouette "
                f"visible={visible} largest={largest} second={second}")
    for index, (current, following) in enumerate(zip(frames, frames[1:])):
        visible_union = 0
        changed = 0
        for lhs, rhs in zip(current.get_flattened_data(),
                            following.get_flattened_data()):
            if lhs[3] > 24 or rhs[3] > 24:
                visible_union += 1
            if sum(abs(lhs[channel] - rhs[channel]) for channel in range(4)) >= 48:
                changed += 1
        percent = changed * 100.0 / visible_union
        if percent < 3.0:
            raise RuntimeError(
                f"{label} frame {index}->{index + 1}: only {percent:.2f}% changed")


def validate_source_board(path: Path, columns: int, rows: int,
                          frame_count: int) -> None:
    frames = normalized_frames(path, columns, rows, frame_count)
    validate_frames(frames, path.stem)
    print(f"PASS {path.name}: {frame_count} complete unique connected poses")


def sequence_frames(role: str) -> list[Image.Image]:
    frames: list[Image.Image] = []
    for state, filename, columns, rows, frame_count in SEQUENCES[role]:
        state_frames = normalized_frames(
            SEQUENCE_SOURCE / filename, columns, rows, frame_count)
        validate_frames(state_frames, f"{role}/{state}")
        frames.extend(state_frames)
    return frames


def build_monster(role: str) -> None:
    atlas = Image.new("RGBA", (MONSTER_SIZE, MONSTER_SIZE))
    frames = sequence_frames(role)
    for frame_index, frame in enumerate(frames):
        atlas.alpha_composite(frame,
            ((frame_index % COLUMNS) * CELL,
             (frame_index // COLUMNS) * CELL))
    validate_frames([
        atlas.crop(((index % COLUMNS) * CELL, (index // COLUMNS) * CELL,
                    (index % COLUMNS + 1) * CELL,
                    (index // COLUMNS + 1) * CELL))
        for index in range(len(frames))], role)
    atlas.save(OUTPUT / f"lightning_{role}.png")
    material_map(atlas).save(OUTPUT / f"lightning_{role}_material.png")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--check-source", type=Path)
    parser.add_argument("--columns", type=int)
    parser.add_argument("--rows", type=int)
    parser.add_argument("--frames", type=int)
    args = parser.parse_args()
    if args.check_source is not None:
        if not all((args.columns, args.rows, args.frames)):
            parser.error("--check-source requires --columns --rows --frames")
        validate_source_board(args.check_source, args.columns, args.rows, args.frames)
        return
    OUTPUT.mkdir(parents=True, exist_ok=True)
    build_environment()
    build_monster("shooter")
    build_monster("dasher")
    for name in ("lightning_environment", "lightning_shooter", "lightning_dasher"):
        image = Image.open(OUTPUT / f"{name}.png")
        print(f"wrote {name}: {image.width}x{image.height} RGBA")


if __name__ == "__main__":
    main()
