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
    if set(sources) != {"floor", "wall"}:
        raise ValueError("fire build requires exactly one floor and one wall source")
    return sources


def _place_native(
    canvas: Image.Image,
    source: Image.Image,
    source_path: Path,
    root: Path,
    source_rect: tuple[int, int, int, int],
    target_xy: tuple[int, int],
    placements: list[dict[str, Any]],
) -> None:
    left, top, right, bottom = source_rect
    width, height = right - left, bottom - top
    if width <= 0 or height <= 0 or right > source.width or bottom > source.height:
        raise ValueError(f"invalid source crop {source_rect} for {source_path}")
    target_rect = (target_xy[0], target_xy[1], target_xy[0] + width, target_xy[1] + height)
    if target_rect[2] > canvas.width or target_rect[3] > canvas.height:
        raise ValueError(f"target rectangle outside master: {target_rect}")
    canvas.alpha_composite(source.crop(source_rect), target_xy)
    placements.append(
        {
            "source": _relative(source_path, root),
            "source_rect": list(source_rect),
            "target_rect": list(target_rect),
            "scale_x": 1.0,
            "scale_y": 1.0,
        }
    )


def _cover_native_area(
    canvas: Image.Image,
    source: Image.Image,
    source_path: Path,
    root: Path,
    area: tuple[int, int, int, int],
    phase: tuple[int, int],
    placements: list[dict[str, Any]],
) -> None:
    left, top, right, bottom = area
    y = top
    row = 0
    while y < bottom:
        source_y = (phase[1] + row * 173) % source.height
        height = min(source.height - source_y, bottom - y)
        x = left
        column = 0
        while x < right:
            source_x = (phase[0] + column * 211 + row * 97) % source.width
            width = min(source.width - source_x, right - x)
            _place_native(
                canvas,
                source,
                source_path,
                root,
                (source_x, source_y, source_x + width, source_y + height),
                (x, y),
                placements,
            )
            x += width
            column += 1
        y += height
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
    horizon = [(0, 680), (560, 485), (1920, 415), (3280, 485), (3840, 680)]
    draw.line(horizon, fill=dark_steel, width=44, joint="curve")
    draw.line(horizon, fill=gold, width=6, joint="curve")
    draw.line(horizon, fill=ember, width=2, joint="curve")
    draw.line([(80, 100), (80, 1800), (500, 2110)], fill=dark_steel, width=34)
    draw.line([(3760, 100), (3760, 1800), (3340, 2110)], fill=dark_steel, width=34)
    draw.line([(80, 100), (80, 1800), (500, 2110)], fill=gold, width=4)
    draw.line([(3760, 100), (3760, 1800), (3340, 2110)], fill=gold, width=4)
    for x in (420, 3420):
        draw.rectangle((x - 92, 532, x + 92, 696), outline=dark_steel, width=28)
        draw.rectangle((x - 92, 532, x + 92, 696), outline=gold, width=4)
    draw.rectangle((1570, 58, 2270, 248), outline=dark_steel, width=32)
    draw.rectangle((1570, 58, 2270, 248), outline=gold, width=4)


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
    floor_path, wall_path = sources["floor"], sources["wall"]
    with Image.open(floor_path) as floor_input, Image.open(wall_path) as wall_input:
        floor = floor_input.convert("RGBA")
        wall = wall_input.convert("RGBA")
        if min(floor.size) < 1024 or min(wall.size) < 1024:
            raise ValueError("native source tiles must each be at least 1024 by 1024")
        master = Image.new("RGBA", MASTER_SIZE, (20, 21, 22, 255))
        placements: list[dict[str, Any]] = []
        _cover_native_area(master, wall, wall_path, root, (0, 0, 3840, 690), (83, 37), placements)
        _cover_native_area(master, floor, floor_path, root, (0, 610, 3840, 2160), (171, 259), placements)
        _apply_room_structure(master)

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
        source_sha256={_relative(path, root): _sha256(path) for path in (floor_path, wall_path)},
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
