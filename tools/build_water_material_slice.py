"""Build deterministic water-ecology runtime atlases from original source art."""

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


def pose_board_rows(role: str) -> list[list[Image.Image]]:
    board = Image.open(
        SOURCE / f"water-{role}-pose-board-alpha-v2.png").convert("RGBA")
    reference = board.crop((0, 0, round(board.width / 4),
                            round(board.height / 5)))
    reference_alpha = reference.getchannel("A").point(
        lambda value: 0 if value < 64 else 255)
    reference_bbox = reference_alpha.getbbox()
    if reference_bbox is None:
        raise RuntimeError(f"{role} reference pose has no visible pixels")
    cell_scale = 172.0 / (reference_bbox[3] - reference_bbox[1])
    rows: list[list[Image.Image]] = []
    for row in range(5):
        poses: list[Image.Image] = []
        top = round(row * board.height / 5)
        bottom = round((row + 1) * board.height / 5)
        for column in range(4):
            left = round(column * board.width / 4)
            right = round((column + 1) * board.width / 4)
            pose = board.crop((left, top, right, bottom))
            alpha = pose.getchannel("A").point(
                lambda value: 0 if value < 64 else (
                    255 if value > 160 else (value - 64) * 255 // 96))
            pose.putalpha(alpha)
            pose = pose.resize((round(pose.width * cell_scale),
                                round(pose.height * cell_scale)),
                               Image.Resampling.LANCZOS)
            pose = pose.crop(alpha_bbox(pose))
            canvas = Image.new("RGBA", (CELL * 2, CELL * 2))
            canvas.alpha_composite(pose,
                ((canvas.width - pose.width) // 2, 186 - pose.height))
            poses.append(canvas)
        rows.append(poses)
    return rows


def discrete_pose(poses: list[Image.Image], frame: int,
                  frame_count: int, loop: bool) -> Image.Image:
    if loop:
        pose_index = frame * len(poses) // frame_count
    else:
        pose_index = round(frame * (len(poses) - 1) / (frame_count - 1))
    return poses[pose_index].resize((CELL, CELL), Image.Resampling.LANCZOS)


def build_monster(role: str) -> None:
    rows = pose_board_rows(role)
    atlas = Image.new("RGBA", (MONSTER_SIZE, MONSTER_SIZE))
    frame_index = 0
    for row, frame_count, loop in (
            (0, 12, True), (1, 16, True), (2, 20, False),
            (3, 8, False), (4, 16, False)):
        for local_frame in range(frame_count):
            frame = discrete_pose(
                rows[row], local_frame, frame_count, loop)
            atlas.alpha_composite(frame,
                ((frame_index % COLUMNS) * CELL,
                 (frame_index // COLUMNS) * CELL))
            frame_index += 1
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
