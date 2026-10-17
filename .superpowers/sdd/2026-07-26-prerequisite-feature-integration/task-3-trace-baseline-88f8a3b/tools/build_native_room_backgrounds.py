"""Deterministically build native Stage 12 room backgrounds without upscaling."""

from __future__ import annotations

import argparse
import hashlib
import json
from dataclasses import asdict, dataclass
from pathlib import Path
from typing import Any

from PIL import Image, ImageChops, ImageDraw, ImageEnhance, ImageFilter


MASTER_SIZE = (3840, 2160)
RUNTIME_SIZE = (2560, 1440)
SOURCE_MANIFEST = Path("art_source/stage12/backgrounds/background-sources.json")


@dataclass(frozen=True)
class BuildReport:
    ecology: str
    master_size: tuple[int, int]
    runtime_size: tuple[int, int]
    source_sha256: dict[str, str]
    placements: list[dict[str, Any]]
    continuous_room_rect: tuple[int, int, int, int]
    runtime_from_master: dict[str, Any]
    output_sha256: dict[str, str]

    def as_json(self) -> dict[str, Any]:
        return asdict(self)


def _sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def _relative(path: Path, root: Path) -> str:
    return path.relative_to(root).as_posix()


def _read_sources(ecology: str, root: Path) -> dict[str, Path]:
    manifest_path = root / SOURCE_MANIFEST
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    try:
        entries = manifest["ecologies"][ecology]["sources"]
    except KeyError as error:
        raise ValueError(f"no declared sources for ecology {ecology!r}") from error

    permitted_directory = (root / "art_source/stage12/backgrounds" / ecology).resolve()
    sources: dict[str, Path] = {}
    for entry in entries:
        role = entry["role"]
        candidate = (root / entry["path"]).resolve()
        try:
            candidate.relative_to(permitted_directory)
        except ValueError as error:
            raise ValueError(f"source outside {permitted_directory}: {candidate}") from error
        if candidate.parent != permitted_directory or not candidate.is_file():
            raise ValueError(f"invalid declared source: {candidate}")
        sources[role] = candidate
    required_roles = (
        {"floor_legacy", "floor_near", "floor_mid", "wall_legacy", "wall_far", "room_layout", "room_right_extension", "room_near_extension"}
        if ecology == "fire" else {"wall", "floor", "open_room", "near_wide", "periphery_wide", "right_extension"}
    )
    if not required_roles.issubset(sources):
        raise ValueError(f"{ecology} build requires its declared native source roles")
    return sources


def _place_native(
    canvas: Image.Image,
    source: Image.Image,
    source_path: Path,
    root: Path,
    source_rect: tuple[int, int, int, int],
    target_rect: tuple[int, int, int, int],
    placements: list[dict[str, Any]],
    feather_px: int = 0,
    opacity: int = 255,
    continuous_room: bool = False,
) -> None:
    left, top, right, bottom = source_rect
    source_width, source_height = right - left, bottom - top
    target_left, target_top, target_right, target_bottom = target_rect
    target_width, target_height = target_right - target_left, target_bottom - target_top
    if source_width <= 0 or source_height <= 0 or right > source.width or bottom > source.height:
        raise ValueError(f"invalid source crop {source_rect} for {source_path}")
    if target_width <= 0 or target_height <= 0:
        raise ValueError(f"invalid target rectangle: {target_rect}")
    if target_right > canvas.width or target_bottom > canvas.height:
        raise ValueError(f"target rectangle outside master: {target_rect}")
    if target_width > source_width or target_height > source_height:
        raise ValueError(f"native source upscale is forbidden: {source_rect} -> {target_rect}")
    crop = source.crop(source_rect)
    if crop.size != (target_width, target_height):
        crop = crop.resize((target_width, target_height), Image.Resampling.LANCZOS)
    if feather_px or opacity < 255:
        feather = min(feather_px, target_width // 3, target_height // 3)
        mask = Image.new("L", crop.size, opacity)
        if feather:
            mask = Image.new("L", crop.size, 0)
            ImageDraw.Draw(mask).rectangle(
                (feather, feather, target_width - feather - 1, target_height - feather - 1),
                fill=opacity,
            )
            mask = mask.filter(ImageFilter.GaussianBlur(max(1, feather // 2)))
        crop.putalpha(ImageChops.multiply(crop.getchannel("A"), mask))
    canvas.alpha_composite(crop, (target_left, target_top))
    placements.append(
        {
            "source": _relative(source_path, root),
            "source_rect": list(source_rect),
            "target_rect": list(target_rect),
            "scale_x": target_width / source_width,
            "scale_y": target_height / source_height,
            "alpha_feather_px": feather_px,
            "continuous_room": continuous_room,
        }
    )


def _cover_perspective_band(
    canvas: Image.Image,
    samples: list[tuple[Path, Image.Image]],
    root: Path,
    area: tuple[int, int, int, int],
    sample_scale: float,
    seed: int,
    placements: list[dict[str, Any]],
    cell_target: tuple[int, int],
    feather_px: int = 0,
    opacity: int = 255,
) -> None:
    left, top, right, bottom = area
    y = top
    row = 0
    while y < bottom:
        source_path, source = samples[(seed + row * 2) % len(samples)]
        target_height = min(max(1, int(cell_target[1] * sample_scale)), bottom - y)
        source_height = min(source.height, max(target_height, round(target_height / sample_scale)))
        source_y = (seed * 53 + row * 193) % (source.height - source_height + 1)
        x = left
        column = 0
        while x < right:
            source_path, source = samples[(seed + row * 3 + column) % len(samples)]
            target_width = min(max(1, int(cell_target[0] * sample_scale)), right - x)
            source_width = min(source.width, max(target_width, round(target_width / sample_scale)))
            source_x = (seed * 71 + row * 107 + column * 251) % (source.width - source_width + 1)
            source_height = min(source.height, max(target_height, round(target_height / sample_scale)))
            source_y = (seed * 53 + row * 193 + column * 89) % (source.height - source_height + 1)
            target_width = min(target_width, round(source_width * sample_scale), right - x)
            target_height_actual = min(target_height, round(source_height * sample_scale), bottom - y)
            _place_native(
                canvas,
                source,
                source_path,
                root,
                (source_x, source_y, source_x + source_width, source_y + source_height),
                (x, y, x + target_width, y + target_height_actual),
                placements,
                feather_px=feather_px,
                opacity=opacity,
            )
            x += target_width
            column += 1
        y += target_height
        row += 1


def _apply_room_structure(master: Image.Image) -> None:
    """Add deterministic frame lines and restrained edge fire without room props."""
    glow = Image.new("RGBA", master.size, (0, 0, 0, 0))
    glow_draw = ImageDraw.Draw(glow)
    orange = (238, 80, 18, 105)
    for offset in (0, 34, 68):
        glow_draw.line([(110 + offset, 650), (640 + offset, 430), (1920, 365), (3230 - offset, 430), (3730 - offset, 650)], fill=orange, width=7)
    glow = glow.filter(ImageFilter.GaussianBlur(18))
    master.alpha_composite(glow)

    draw = ImageDraw.Draw(master)
    dark_steel = (20, 23, 25, 238)
    gold = (137, 103, 46, 220)
    ember = (223, 76, 20, 195)
    horizon = [(120, 690), (690, 505), (1920, 445), (3150, 505), (3720, 690)]
    draw.line(horizon, fill=dark_steel, width=44, joint="curve")
    draw.line(horizon, fill=gold, width=6, joint="curve")
    draw.line(horizon, fill=ember, width=2, joint="curve")
    draw.line([(92, 130), (92, 1740), (530, 2110)], fill=dark_steel, width=34)
    draw.line([(3748, 130), (3748, 1740), (3310, 2110)], fill=dark_steel, width=34)
    draw.line([(92, 130), (92, 1740), (530, 2110)], fill=gold, width=4)
    draw.line([(3748, 130), (3748, 1740), (3310, 2110)], fill=gold, width=4)
    for y, left, right in ((835, 470, 3370), (1250, 350, 3490)):
        draw.line([(left, y), (right, y)], fill=(12, 14, 16, 115), width=18)
        draw.line([(left + 24, y), (right - 24, y)], fill=(114, 76, 35, 95), width=2)


def _apply_quiet_outer_vignette(master: Image.Image) -> None:
    overlay = Image.new("RGBA", master.size, (0, 0, 0, 0))
    draw = ImageDraw.Draw(overlay)
    for inset, alpha in ((0, 55), (110, 32), (230, 14)):
        draw.rectangle((inset, inset, master.width - inset - 1, master.height - inset - 1), outline=(5, 6, 7, alpha), width=110)
    master.alpha_composite(overlay)


def _make_material(runtime: Image.Image) -> Image.Image:
    rgba = runtime.convert("RGBA")
    red, green, blue, _ = rgba.split()
    coverage = rgba.convert("L").point(lambda value: min(255, value + 28))
    fire_mask = ImageChops.subtract(red, green).point(lambda value: min(255, value * 3))
    gradient = Image.linear_gradient("L").resize(RUNTIME_SIZE)
    blue_channel = ImageChops.add(gradient.point(lambda value: value // 3), fire_mask.point(lambda value: value // 4))
    return Image.merge("RGBA", (coverage, fire_mask, blue_channel, Image.new("L", RUNTIME_SIZE, 255)))


def _save_png(image: Image.Image, path: Path) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    image.save(path, format="PNG")


_ECOLOGY_STYLE = {
    "water": ((13, 25, 29, 255), (49, 161, 182, 155), (142, 112, 61, 190)),
    "lightning": ((17, 21, 27, 255), (112, 178, 238, 155), (154, 100, 50, 190)),
    "chaos": ((19, 15, 23, 255), (176, 46, 139, 150), (132, 106, 54, 185)),
}


def _cover_native_area(
    canvas: Image.Image,
    source: Image.Image,
    source_path: Path,
    root: Path,
    area: tuple[int, int, int, int],
    placements: list[dict[str, Any]],
    seed: int,
    *,
    continuous_room: bool = False,
) -> None:
    """Cover an area with shifted native crops, never enlarging an input crop."""
    left, top, right, bottom = area
    cell_width, cell_height = 960, 540
    row = 0
    for y in range(top, bottom, cell_height):
        column = 0
        for x in range(left, right, cell_width):
            width, height = min(cell_width, right - x), min(cell_height, bottom - y)
            source_x = (seed * 71 + row * 137 + column * 229) % (source.width - width + 1)
            source_y = (seed * 43 + row * 113 + column * 67) % (source.height - height + 1)
            _place_native(
                canvas, source, source_path, root,
                (source_x, source_y, source_x + width, source_y + height),
                (x, y, x + width, y + height), placements,
                continuous_room=continuous_room,
            )
            column += 1
        row += 1


def _apply_ecology_frame(master: Image.Image, ecology: str) -> None:
    """Add only low-key edge architecture; central gameplay floor remains clear."""
    _, accent, gold = _ECOLOGY_STYLE[ecology]
    draw = ImageDraw.Draw(master)
    horizon = [(86, 660), (650, 500), (1920, 452), (3190, 500), (3754, 660)]
    draw.line(horizon, fill=(9, 11, 14, 205), width=34, joint="curve")
    draw.line(horizon, fill=gold, width=4, joint="curve")
    draw.line(horizon, fill=accent, width=2, joint="curve")
    for x in (80, 3760):
        draw.line([(x, 120), (x, 1760), (1920 + (x - 1920) * 0.82, 2120)], fill=(8, 10, 13, 185), width=28)
        draw.line([(x, 120), (x, 1760), (1920 + (x - 1920) * 0.82, 2120)], fill=gold, width=3)


def _make_ecology_material(runtime: Image.Image, ecology: str) -> Image.Image:
    rgba = runtime.convert("RGBA")
    red, green, blue, _ = rgba.split()
    coverage = rgba.convert("L").point(lambda value: min(255, value + 24))
    if ecology == "water":
        accent = ImageChops.subtract(blue, red)
    elif ecology == "lightning":
        accent = ImageChops.subtract(blue, green)
    else:
        accent = ImageChops.subtract(red, green)
    gradient = Image.linear_gradient("L").resize(RUNTIME_SIZE)
    return Image.merge("RGBA", (coverage, accent, gradient.point(lambda value: value // 3), Image.new("L", RUNTIME_SIZE, 255)))


def _apply_outer_edge_shade(master: Image.Image) -> None:
    """Fade only the outer frame; the central 55% is intentionally untouched."""
    low_width, low_height = 320, 180
    central_left, central_top = 72, 40
    central_right, central_bottom = 248, 140
    mask = Image.new("L", (low_width, low_height), 0)
    pixels = mask.load()
    for y in range(low_height):
        for x in range(low_width):
            distance = max(central_left - x, x - central_right, central_top - y, y - central_bottom, 0)
            pixels[x, y] = min(74, distance * 2)
    shade = Image.new("RGBA", MASTER_SIZE, (4, 5, 7, 0))
    shade.putalpha(mask.resize(MASTER_SIZE, Image.Resampling.LANCZOS))
    master.alpha_composite(shade)


def _build_generic_ecology(ecology: str, root: Path) -> BuildReport:
    if ecology not in _ECOLOGY_STYLE:
        raise ValueError(f"unsupported ecology: {ecology!r}")
    sources = _read_sources(ecology, root)
    with Image.open(sources["wall"]) as wall_input, Image.open(sources["floor"]) as floor_input, Image.open(sources["open_room"]) as room_input, Image.open(sources["near_wide"]) as near_input, Image.open(sources["right_extension"]) as right_input, Image.open(sources["periphery_wide"]) as periphery_input:
        wall, floor, room_wide, near_wide, right_extension, periphery = (
            wall_input.convert("RGBA"), floor_input.convert("RGBA"),
            room_input.convert("RGBA"), near_input.convert("RGBA"),
            right_input.convert("RGBA"), periphery_input.convert("RGBA"),
        )
        if any(image.width < 720 or image.height < 720 for image in (wall, floor, room_wide, near_wide, right_extension, periphery)):
            raise ValueError("native ecology source images must each be at least 720 by 720")
        master = Image.new("RGBA", MASTER_SIZE, _ECOLOGY_STYLE[ecology][0])
        outer = Image.new("RGBA", MASTER_SIZE, (0, 0, 0, 0))
        placements: list[dict[str, Any]] = []
        # Eight broad, overlapping crops make the outer context continuous;
        # no regular full-screen tile grid is permitted.
        wide_width = min(1536, periphery.width)
        wide_x = (periphery.width - wide_width) // 2
        top_height = min(760, periphery.height)
        side_width, side_height = min(1152, periphery.width), min(1024, periphery.height)
        left_x = 0
        right_x = periphery.width - side_width
        _place_native(outer, periphery, sources["periphery_wide"], root, (wide_x, 0, wide_x + wide_width, top_height), (1152, 0, 1152 + wide_width, top_height), placements, feather_px=170)
        _place_native(outer, periphery, sources["periphery_wide"], root, (left_x, 0, left_x + side_width, side_height), (0, 0, side_width, side_height), placements, feather_px=170)
        _place_native(outer, periphery, sources["periphery_wide"], root, (right_x, 0, right_x + side_width, side_height), (3840 - side_width, 0, 3840, side_height), placements, feather_px=170)
        bottom_height = 680
        bottom_y = periphery.height - bottom_height
        _place_native(outer, periphery, sources["periphery_wide"], root, (wide_x, bottom_y, wide_x + wide_width, periphery.height), (1152, 1480, 1152 + wide_width, 2160), placements, feather_px=170)
        wall_height = min(860, wall.height)
        _place_native(outer, wall, sources["wall"], root, (0, 0, 950, wall_height), (0, 720, 950, 720 + wall_height), placements, feather_px=160)
        _place_native(outer, wall, sources["wall"], root, (wall.width - 950, wall.height - wall_height, wall.width, wall.height), (2890, 720, 3840, 720 + wall_height), placements, feather_px=160)
        floor_height = min(760, floor.height)
        _place_native(outer, floor, sources["floor"], root, (0, floor.height - floor_height, 1250, floor.height), (0, 1400, 1250, 2160), placements, feather_px=160)
        _place_native(outer, floor, sources["floor"], root, (floor.width - 1250, floor.height - floor_height, floor.width, floor.height), (2590, 1400, 3840, 2160), placements, feather_px=160)
        outer = ImageEnhance.Contrast(outer.filter(ImageFilter.GaussianBlur(6))).enhance(0.78)
        outer = ImageEnhance.Brightness(outer).enhance(1.40 if ecology == "chaos" else 1.10)
        outer.putalpha(outer.getchannel("A").point(lambda value: value * 100 // 255))
        master.alpha_composite(outer)
        _apply_outer_edge_shade(master)
        continuous_room_rect = (864, 486, 2976, 1674)
        room_left = 560 if ecology == "chaos" else 700
        near_left = 760 if ecology == "chaos" else 900
        wide_width, wide_height = room_wide.size
        _place_native(master, room_wide, sources["open_room"], root, (0, 0, wide_width, wide_height), (room_left, 486, room_left + wide_width, 486 + wide_height), placements, feather_px=85, continuous_room=True)
        right_start = max(room_left + wide_width - 180, 3150 - right_extension.width)
        right_width = 3150 - right_start
        _place_native(master, right_extension, sources["right_extension"], root, (0, 0, right_width, 1188), (right_start, 486, 3150, 1674), placements, feather_px=75 if ecology == "chaos" else 90, continuous_room=True)
        near_width, near_height = near_wide.size
        near_target_height = min(near_height, MASTER_SIZE[1] - 1259)
        near_source_top = near_height - near_target_height
        _place_native(master, near_wide, sources["near_wide"], root, (0, near_source_top, near_width, near_height), (near_left, 1259, near_left + near_width, 1259 + near_target_height), placements, feather_px=90, continuous_room=True)
        _place_native(master, near_wide, sources["near_wide"], root, (0, near_source_top, 200, near_height), (room_left, 1200, near_left, 1200 + near_target_height), placements, feather_px=75, continuous_room=True)

    master_path = root / f"art_source/stage12/backgrounds/{ecology}/{ecology}-room-background-master.png"
    runtime_path = root / f"assets/stage12/{ecology}_room_background.png"
    material_path = root / f"assets/stage12/{ecology}_room_background_material.png"
    if ecology == "chaos":
        # Keep the dark obsidian value range while making the restrained
        # magenta/acid-green ecology legible after the runtime downsample.
        master = ImageEnhance.Color(master).enhance(1.07)
    _save_png(master, master_path)
    runtime = master.resize(RUNTIME_SIZE, Image.Resampling.LANCZOS)
    _save_png(runtime, runtime_path)
    _save_png(_make_ecology_material(runtime, ecology), material_path)
    report = BuildReport(
        ecology=ecology, master_size=MASTER_SIZE, runtime_size=RUNTIME_SIZE,
        source_sha256={_relative(path, root): _sha256(path) for path in sources.values()},
        placements=placements, continuous_room_rect=continuous_room_rect,
        runtime_from_master={"resampling": "LANCZOS", "passes": 1},
        output_sha256={"master": _sha256(master_path), "runtime": _sha256(runtime_path), "material": _sha256(material_path)},
    )
    report_path = root / "assets/stage12/room-background-build.json"
    existing: dict[str, Any] = {"schema_version": 1, "ecologies": {}}
    if report_path.exists():
        existing = json.loads(report_path.read_text(encoding="utf-8"))
    existing.setdefault("schema_version", 1)
    existing.setdefault("ecologies", {})[ecology] = report.as_json()
    report_path.write_text(json.dumps(existing, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    return report


def build_ecology(ecology: str, root: Path) -> BuildReport:
    """Build one ecology strictly from its declared native source directory."""
    root = root.resolve()
    if ecology != "fire":
        return _build_generic_ecology(ecology, root)
    sources = _read_sources(ecology, root)
    with (
        Image.open(sources["floor_legacy"]) as floor_legacy_input,
        Image.open(sources["floor_near"]) as floor_near_input,
        Image.open(sources["floor_mid"]) as floor_mid_input,
        Image.open(sources["wall_legacy"]) as wall_legacy_input,
        Image.open(sources["wall_far"]) as wall_far_input,
        Image.open(sources["room_layout"]) as room_layout_input,
        Image.open(sources["room_right_extension"]) as room_right_extension_input,
        Image.open(sources["room_near_extension"]) as room_near_extension_input,
    ):
        loaded = {"floor_legacy": floor_legacy_input.convert("RGBA"), "floor_near": floor_near_input.convert("RGBA"), "floor_mid": floor_mid_input.convert("RGBA"), "wall_legacy": wall_legacy_input.convert("RGBA"), "wall_far": wall_far_input.convert("RGBA"), "room_layout": room_layout_input.convert("RGBA"), "room_right_extension": room_right_extension_input.convert("RGBA"), "room_near_extension": room_near_extension_input.convert("RGBA")}
        room_roles = {"room_layout", "room_right_extension", "room_near_extension"}
        if any(min(image.size) < 1024 for role, image in loaded.items() if role not in room_roles):
            raise ValueError("native source tiles must each be at least 1024 by 1024")
        if any(loaded[role].width < 1024 or loaded[role].height < 720 for role in room_roles):
            raise ValueError("native room extension source is unexpectedly small")
        master = Image.new("RGBA", MASTER_SIZE, (20, 21, 22, 255))
        placements: list[dict[str, Any]] = []
        wall_samples = [(sources["wall_far"], loaded["wall_far"]), (sources["wall_legacy"], loaded["wall_legacy"])]
        _cover_perspective_band(master, wall_samples, root, (0, 0, 3840, 650), 0.78, 17, placements, (760, 540), 75, 150)
        _cover_perspective_band(master, [(sources["floor_mid"], loaded["floor_mid"]), (sources["floor_legacy"], loaded["floor_legacy"])], root, (0, 1450, 3840, 2160), 0.80, 29, placements, (760, 560), 85, 140)
        _cover_perspective_band(master, [(sources["wall_far"], loaded["wall_far"]), (sources["floor_mid"], loaded["floor_mid"])], root, (0, 420, 1050, 1710), 0.74, 43, placements, (680, 600), 75, 130)
        _cover_perspective_band(master, [(sources["wall_legacy"], loaded["wall_legacy"]), (sources["floor_mid"], loaded["floor_mid"])], root, (2790, 420, 3840, 1710), 0.74, 59, placements, (680, 600), 75, 130)
        continuous_room_rect = (864, 486, 2976, 1674)
        layout = loaded["room_layout"]
        right_extension = loaded["room_right_extension"]
        near_extension = loaded["room_near_extension"]
        _place_native(master, layout, sources["room_layout"], root, (0, 0, layout.width, layout.height), (864, 486, 864 + layout.width, 486 + layout.height), placements, feather_px=36, continuous_room=True)
        _place_native(master, right_extension, sources["room_right_extension"], root, (0, 0, 676, 941), (2300, 486, 2976, 1427), placements, feather_px=56, continuous_room=True)
        _place_native(master, near_extension, sources["room_near_extension"], root, (0, near_extension.height - 324, 1672, near_extension.height), (864, 1350, 2536, 1674), placements, feather_px=50, continuous_room=True)
        _place_native(master, right_extension, sources["room_right_extension"], root, (0, right_extension.height - 324, 676, right_extension.height), (2300, 1350, 2976, 1674), placements, feather_px=50, continuous_room=True)
        floor_near = loaded["floor_near"]
        _place_native(master, floor_near, sources["floor_near"], root, (0, 520, floor_near.width, 1006), (1293, 1674, 2547, 2160), placements, feather_px=96, opacity=125)
        _apply_quiet_outer_vignette(master)

    master_path = root / "art_source/stage12/backgrounds/fire/fire-room-background-master.png"
    runtime_path = root / "assets/stage12/fire_room_background.png"
    material_path = root / "assets/stage12/fire_room_background_material.png"
    _save_png(master, master_path)
    runtime = master.resize(RUNTIME_SIZE, Image.Resampling.LANCZOS)
    _save_png(runtime, runtime_path)
    _save_png(_make_material(runtime), material_path)

    report = BuildReport(
        ecology=ecology,
        master_size=MASTER_SIZE,
        runtime_size=RUNTIME_SIZE,
        source_sha256={_relative(path, root): _sha256(path) for path in sources.values()},
        placements=placements,
        continuous_room_rect=continuous_room_rect,
        runtime_from_master={"resampling": "LANCZOS", "passes": 1},
        output_sha256={
            "master": _sha256(master_path),
            "runtime": _sha256(runtime_path),
            "material": _sha256(material_path),
        },
    )
    report_path = root / "assets/stage12/room-background-build.json"
    existing: dict[str, Any] = {"schema_version": 1, "ecologies": {}}
    if report_path.exists():
        existing = json.loads(report_path.read_text(encoding="utf-8"))
    existing.setdefault("schema_version", 1)
    existing.setdefault("ecologies", {})[ecology] = report.as_json()
    report_path.parent.mkdir(parents=True, exist_ok=True)
    report_path.write_text(json.dumps(existing, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    return report


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ecology", required=True)
    args = parser.parse_args()
    report = build_ecology(args.ecology, Path.cwd())
    print(json.dumps(report.as_json(), sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
