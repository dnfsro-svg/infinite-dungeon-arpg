"""Deterministically build native Stage 12 room backgrounds without upscaling."""

from __future__ import annotations

import argparse
import hashlib
import json
from dataclasses import asdict, dataclass
from pathlib import Path
from typing import Any

from PIL import Image, ImageChops, ImageDraw, ImageFilter


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
    required_roles = {"floor_legacy", "floor_near", "floor_mid", "wall_legacy", "wall_far", "room_layout"}
    if set(sources) != required_roles:
        raise ValueError("fire build requires declared floor, wall, and native room layout sources")
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


def build_ecology(ecology: str, root: Path) -> BuildReport:
    """Build one ecology strictly from its declared native source directory."""
    root = root.resolve()
    if ecology != "fire":
        raise ValueError(f"Task 1 only supports fire, not {ecology!r}")
    sources = _read_sources(ecology, root)
    with (
        Image.open(sources["floor_legacy"]) as floor_legacy_input,
        Image.open(sources["floor_near"]) as floor_near_input,
        Image.open(sources["floor_mid"]) as floor_mid_input,
        Image.open(sources["wall_legacy"]) as wall_legacy_input,
        Image.open(sources["wall_far"]) as wall_far_input,
        Image.open(sources["room_layout"]) as room_layout_input,
    ):
        loaded = {"floor_legacy": floor_legacy_input.convert("RGBA"), "floor_near": floor_near_input.convert("RGBA"), "floor_mid": floor_mid_input.convert("RGBA"), "wall_legacy": wall_legacy_input.convert("RGBA"), "wall_far": wall_far_input.convert("RGBA"), "room_layout": room_layout_input.convert("RGBA")}
        if any(min(image.size) < 1024 for role, image in loaded.items() if role != "room_layout"):
            raise ValueError("native source tiles must each be at least 1024 by 1024")
        if loaded["room_layout"].width < 1024 or loaded["room_layout"].height < 720:
            raise ValueError("native room layout source is unexpectedly small")
        master = Image.new("RGBA", MASTER_SIZE, (20, 21, 22, 255))
        placements: list[dict[str, Any]] = []
        wall_samples = [(sources["wall_far"], loaded["wall_far"]), (sources["wall_legacy"], loaded["wall_legacy"])]
        _cover_perspective_band(master, wall_samples, root, (0, 0, 3840, 650), 0.78, 17, placements, (760, 540), 75, 150)
        _cover_perspective_band(master, [(sources["floor_mid"], loaded["floor_mid"]), (sources["floor_legacy"], loaded["floor_legacy"])], root, (0, 1450, 3840, 2160), 0.80, 29, placements, (760, 560), 85, 140)
        _cover_perspective_band(master, [(sources["wall_far"], loaded["wall_far"]), (sources["floor_mid"], loaded["floor_mid"])], root, (0, 420, 1050, 1710), 0.74, 43, placements, (680, 600), 75, 130)
        _cover_perspective_band(master, [(sources["wall_legacy"], loaded["wall_legacy"]), (sources["floor_mid"], loaded["floor_mid"])], root, (2790, 420, 3840, 1710), 0.74, 59, placements, (680, 600), 75, 130)
        layout = loaded["room_layout"]
        _place_native(master, layout, sources["room_layout"], root, (0, 0, layout.width, layout.height), (1084, 536, 1084 + layout.width, 536 + layout.height), placements, feather_px=36)
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
