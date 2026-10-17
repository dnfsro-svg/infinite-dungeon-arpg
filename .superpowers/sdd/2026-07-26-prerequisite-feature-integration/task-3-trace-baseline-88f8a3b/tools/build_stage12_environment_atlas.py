"""Build the deterministic Stage 12 1024px RGBA environment atlas.

Run the imagegen chroma-key helper first; see assets/stage12/environment-source.md.
"""

from pathlib import Path

from PIL import Image, ImageDraw, ImageEnhance


ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "art_source" / "stage12"
TEMP = ROOT / "out" / "stage12-art-temp"
OUTPUT = ROOT / "assets" / "stage12" / "environment.png"

ATLAS_SIZE = (1024, 1024)
BACKGROUND_BOX = (0, 0, 1024, 704)
DOOR_BOXES = ((0, 704, 112, 112), (112, 704, 112, 112),
              (224, 704, 112, 112), (336, 704, 112, 112))
HOLE_BOX = (448, 704, 192, 160)
ACCENTS = ((238, 86, 67), (67, 160, 234), (242, 204, 68), (210, 73, 205))


def fit(image: Image.Image, size: tuple[int, int]) -> Image.Image:
    """Fit image inside size without distorting its perspective."""
    result = image.copy()
    result.thumbnail(size, Image.Resampling.LANCZOS)
    return result


def centered(image: Image.Image, size: tuple[int, int]) -> Image.Image:
    result = Image.new("RGBA", size)
    result.alpha_composite(image, ((size[0] - image.width) // 2,
        (size[1] - image.height) // 2))
    return result


def main() -> None:
    background = Image.open(SOURCE / "environment-concept-v1.png").convert("RGBA")
    door = Image.open(TEMP / "door-alpha.png").convert("RGBA")
    hole = Image.open(TEMP / "hole-alpha.png").convert("RGBA")

    atlas = Image.new("RGBA", ATLAS_SIZE, (0, 0, 0, 0))
    room = ImageEnhance.Contrast(background).enhance(1.06)
    atlas.alpha_composite(room.resize(BACKGROUND_BOX[2:], Image.Resampling.LANCZOS))

    door_art = fit(door, (104, 108))
    for box, accent in zip(DOOR_BOXES, ACCENTS, strict=True):
        frame = centered(door_art, box[2:])
        tint = Image.new("RGBA", box[2:], (*accent, 80))
        tint.putalpha(frame.getchannel("A").point(lambda alpha: alpha * 80 // 255))
        frame = Image.alpha_composite(frame, tint)
        outline = ImageDraw.Draw(frame)
        outline.rounded_rectangle((2, 2, box[2] - 3, box[3] - 3),
            radius=12, outline=(*accent, 220), width=3)
        atlas.alpha_composite(frame, box[:2])

    hole_art = centered(fit(hole, (188, 150)), HOLE_BOX[2:])
    atlas.alpha_composite(hole_art, HOLE_BOX[:2])

    # The remaining lower strip deliberately stays transparent for future props.
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    atlas.save(OUTPUT)
    print(f"Wrote {OUTPUT} {atlas.width}x{atlas.height} RGBA")


if __name__ == "__main__":
    main()
