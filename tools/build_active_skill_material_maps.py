from __future__ import annotations

import argparse
import tempfile
from pathlib import Path

from PIL import Image


SKILLS = ("draw_slash", "storm_swords")


def clamp(value: int, minimum: int, maximum: int) -> int:
    return max(minimum, min(maximum, value))


def material_pixel(pixel: tuple[int, int, int, int]) -> tuple[int, int, int, int]:
    red, green, blue, alpha = pixel
    if alpha == 0:
        return 0, 0, 0, 0
    luminance = (54 * red + 183 * green + 19 * blue) // 256
    energy = max(blue - red, green - red, 0)
    ancient_gold = red > green > blue and red - blue >= 24
    dark_metal = luminance < 118 and max(red, green, blue) - min(red, green, blue) < 42
    roughness = clamp(220 - luminance // 2 - 70 * dark_metal, 40, 235)
    emissive = clamp(energy * 3 + (48 if luminance > 210 else 0), 0, 255)
    metalness = clamp(32 + 180 * dark_metal + 145 * ancient_gold, 0, 255)
    return roughness, emissive, metalness, alpha


def build_map(source: Path, destination: Path) -> None:
    with Image.open(source) as opened:
        color = opened.convert("RGBA")
    material = Image.new("RGBA", color.size)
    material.putdata(
        [material_pixel(pixel) for pixel in color.get_flattened_data()]
    )

    destination.parent.mkdir(parents=True, exist_ok=True)
    temporary_path: Path | None = None
    try:
        with tempfile.NamedTemporaryFile(
            dir=destination.parent,
            prefix=f".{destination.stem}-",
            suffix=".png",
            delete=False,
        ) as temporary:
            temporary_path = Path(temporary.name)
        material.save(temporary_path, format="PNG", optimize=False, compress_level=9)
        with Image.open(temporary_path) as verified:
            verified.load()
            if verified.size != color.size:
                raise RuntimeError(f"dimension mismatch for {destination.name}")
            if verified.convert("RGBA").getchannel("A").tobytes() != color.getchannel("A").tobytes():
                raise RuntimeError(f"alpha mismatch for {destination.name}")
        temporary_path.replace(destination)
        temporary_path = None
    finally:
        if temporary_path is not None:
            temporary_path.unlink(missing_ok=True)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, default=Path.cwd())
    parser.add_argument("--output-dir", type=Path)
    arguments = parser.parse_args()
    root = arguments.root.resolve()
    output_dir = (arguments.output_dir or root / "assets" / "skills").resolve()
    for skill in SKILLS:
        build_map(
            root / "assets" / "skills" / f"{skill}_atlas.png",
            output_dir / f"{skill}_atlas_material.png",
        )


if __name__ == "__main__":
    main()
