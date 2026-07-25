from __future__ import annotations

from pathlib import Path

from PIL import Image


ROOT = Path(__file__).resolve().parents[1]
SOURCE_PATH = ROOT / "assets" / "stage12" / "ui_material.png"
OUTPUT_PATH = ROOT / "assets" / "launcher" / "infinite_dungeon.ico"
CELL_BOX = (128, 128, 256, 256)
ICON_SIZES = ((16, 16), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256))


def build_icon() -> None:
    with Image.open(SOURCE_PATH) as source:
        cell = source.convert("RGBA").crop(CELL_BOX)

    canvas = Image.new("RGBA", (256, 256), (0, 0, 0, 0))
    canvas.alpha_composite(
        cell,
        ((canvas.width - cell.width) // 2, (canvas.height - cell.height) // 2),
    )
    frames = [
        canvas.resize(size, Image.Resampling.LANCZOS)
        for size in ICON_SIZES
    ]

    OUTPUT_PATH.parent.mkdir(parents=True, exist_ok=True)
    frames[-1].save(
        OUTPUT_PATH,
        format="ICO",
        sizes=ICON_SIZES,
        append_images=frames[:-1],
        bitmap_format="png",
    )


if __name__ == "__main__":
    build_icon()
