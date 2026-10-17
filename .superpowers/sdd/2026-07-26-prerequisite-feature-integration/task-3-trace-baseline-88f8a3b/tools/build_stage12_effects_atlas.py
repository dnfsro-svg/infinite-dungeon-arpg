"""Build the deterministic Stage 12 1024px RGBA effects and loot atlas."""

from pathlib import Path

from PIL import Image, ImageDraw, ImageEnhance


ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "art_source" / "stage12" / "effects-ui-concept-v1.png"
OUTPUT = ROOT / "assets" / "stage12" / "effects_ui.png"
SIZE = 1024
CELL = 128


def key_green(image: Image.Image) -> Image.Image:
    rgba = image.convert("RGBA")
    pixels = rgba.load()
    for y in range(rgba.height):
        for x in range(rgba.width):
            red, green, blue, alpha = pixels[x, y]
            # Remove chroma pixels including dark antialiased spill. Cyan water
            # highlights stay intact because blue is not materially lower.
            if green > 20 and green >= red + 8 and green >= blue + 8:
                pixels[x, y] = (red, green, blue, 0)
    return rgba


def source_cell(image: Image.Image, index: int) -> Image.Image:
    columns, rows = 4, 4
    width, height = image.width // columns, image.height // rows
    x, y = (index % columns) * width, (index // columns) * height
    return image.crop((x, y, x + width, y + height))


def fit(image: Image.Image) -> Image.Image:
    image.thumbnail((116, 116), Image.Resampling.LANCZOS)
    result = Image.new("RGBA", (CELL, CELL))
    result.alpha_composite(image, ((CELL - image.width) // 2,
        (CELL - image.height) // 2))
    return result


def icon(color: tuple[int, int, int], shape: str) -> Image.Image:
    result = Image.new("RGBA", (CELL, CELL))
    draw = ImageDraw.Draw(result)
    if shape == "diamond":
        draw.polygon(((64, 14), (112, 64), (64, 114), (16, 64)),
            fill=(*color, 230), outline=(240, 245, 255, 255), width=5)
    elif shape == "star":
        draw.regular_polygon((64, 64, 48), 4, rotation=45,
            fill=(*color, 230), outline=(255, 245, 210, 255), width=5)
    else:
        draw.ellipse((18, 18, 110, 110), fill=(*color, 210),
            outline=(245, 235, 255, 255), width=5)
    return result


def main() -> None:
    source = key_green(Image.open(SOURCE))
    atlas = Image.new("RGBA", (SIZE, SIZE))
    # First two rows: source concept effects, arranged into stable manifest slots.
    for index in range(8):
        cell = fit(ImageEnhance.Contrast(source_cell(source, index)).enhance(1.08))
        atlas.alpha_composite(cell, ((index % 4) * CELL, (index // 4) * CELL))
    # Third row: white, blue, gold, and abyss loot markers.
    loot_icons = (((220, 228, 236), "diamond"),
        ((70, 139, 255), "diamond"), ((255, 193, 52), "star"),
        ((218, 74, 205), "orb"))
    for index, (color, shape) in enumerate(loot_icons):
        atlas.alpha_composite(icon(color, shape), (index * CELL, 2 * CELL))
    # Resampling transparent chroma-key edges can reintroduce green spill;
    # clean the final composited atlas once more before writing it.
    atlas = key_green(atlas)
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    atlas.save(OUTPUT)
    print(f"Wrote {OUTPUT} {atlas.width}x{atlas.height} RGBA")


if __name__ == "__main__":
    main()
