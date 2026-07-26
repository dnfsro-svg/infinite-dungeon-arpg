"""Deterministically export the shared element-door and ecology prop atlases."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from collections import deque
from pathlib import Path
from tempfile import NamedTemporaryFile

from PIL import Image, ImageChops, ImageEnhance, ImageFilter


ECOLOGIES = ("water", "lightning", "chaos")
CELL = 256
LAYOUT = {
    "wall": (512, 0),
    "surface_prop": (512, 256),
    "hole": (0, 512),
    "light": (256, 512),
    "solid_prop": (512, 512),
}
CONCEPT_CROPS = {
    "water": {
        "surface_prop": (690, 900, 1240, 1254),
        "hole": (20, 880, 690, 1254),
        "light": (965, 420, 1254, 840),
        "solid_prop": (1000, 295, 1254, 620),
    },
    "lightning": {
        "surface_prop": (860, 100, 1254, 610),
        "hole": (0, 300, 470, 790),
        "light": (0, 40, 320, 470),
        "solid_prop": (900, 540, 1254, 1080),
    },
    "chaos": {
        "surface_prop": (860, 100, 1254, 610),
        "hole": (0, 300, 470, 790),
        "light": (0, 40, 320, 470),
        "solid_prop": (900, 540, 1254, 1080),
    },
}


def _components(alpha: Image.Image, threshold: int = 8) -> list[set[tuple[int, int]]]:
    remaining = {(x, y) for y in range(alpha.height) for x in range(alpha.width)
                 if alpha.getpixel((x, y)) > threshold}
    result: list[set[tuple[int, int]]] = []
    while remaining:
        component = {remaining.pop()}
        queue = deque(component)
        while queue:
            x, y = queue.popleft()
            for dx, dy in ((-1, -1), (-1, 0), (-1, 1), (0, -1),
                           (0, 1), (1, -1), (1, 0), (1, 1)):
                point = (x + dx, y + dy)
                if point in remaining:
                    remaining.remove(point)
                    component.add(point)
                    queue.append(point)
        result.append(component)
    return sorted(result, key=len, reverse=True)


def _bounds(points: set[tuple[int, int]]) -> tuple[int, int, int, int]:
    return (min(x for x, _ in points), min(y for _, y in points),
            max(x for x, _ in points) + 1, max(y for _, y in points) + 1)


def _background_to_alpha(image: Image.Image) -> Image.Image:
    """Remove border-connected near-background pixels using RGB distance 28."""
    image = image.convert("RGBA")
    rgb = image.convert("RGB")
    border = []
    for x in range(rgb.width):
        border.extend((rgb.getpixel((x, 0)), rgb.getpixel((x, rgb.height - 1))))
    for y in range(1, rgb.height - 1):
        border.extend((rgb.getpixel((0, y)), rgb.getpixel((rgb.width - 1, y))))
    reference = tuple(sorted(channel[index] for channel in border)[len(border) // 2]
                      for index in range(3))
    def close(pixel: tuple[int, int, int]) -> bool:
        return sum((pixel[index] - reference[index]) ** 2 for index in range(3)) <= 28 ** 2
    transparent: set[tuple[int, int]] = set()
    visited: set[tuple[int, int]] = set()
    queue = deque()
    for x in range(rgb.width):
        queue.extend(((x, 0), (x, rgb.height - 1)))
    for y in range(1, rgb.height - 1):
        queue.extend(((0, y), (rgb.width - 1, y)))
    while queue:
        point = queue.popleft()
        if point in visited:
            continue
        visited.add(point)
        x, y = point
        if not close(rgb.getpixel(point)):
            continue
        transparent.add(point)
        for dx, dy in ((-1, 0), (1, 0), (0, -1), (0, 1)):
            next_point = (x + dx, y + dy)
            if 0 <= next_point[0] < rgb.width and 0 <= next_point[1] < rgb.height:
                queue.append(next_point)
    alpha = image.getchannel("A").copy()
    for point in transparent:
        alpha.putpixel(point, 0)
    image.putalpha(alpha)
    return image


def _retain_subject(image: Image.Image, pad: int, keep_small: bool) -> Image.Image:
    # The exported cell is 256px square; scanning larger concept crops only
    # changes work, not the retained silhouette at its final sampling rate.
    if max(image.size) > CELL:
        scale = CELL / max(image.size)
        image = image.resize((round(image.width * scale), round(image.height * scale)),
                             Image.Resampling.LANCZOS)
    image = _background_to_alpha(image)
    components = _components(image.getchannel("A"))
    if not components:
        raise RuntimeError("source has no foreground component")
    largest = len(components[0])
    keep = [component for component in components
            if component is components[0] or (keep_small and len(component) * 100 >= largest * 2)]
    alpha = Image.new("L", image.size)
    for component in keep:
        for point in component:
            alpha.putpixel(point, image.getchannel("A").getpixel(point))
    alpha = alpha.filter(ImageFilter.GaussianBlur(2))
    image.putalpha(alpha)
    bbox = alpha.getbbox()
    if bbox is None:
        raise RuntimeError("foreground disappeared during alpha cleanup")
    left, top, right, bottom = bbox
    if left < pad or top < pad or right > image.width - pad or bottom > image.height - pad:
        # Cropping can legally touch its source boundary; padding is applied below.
        pass
    return image.crop((max(0, left - pad), max(0, top - pad),
                       min(image.width, right + pad), min(image.height, bottom + pad)))


def _fit_to_cell(subject: Image.Image, margin: int, foot: int) -> Image.Image:
    subject = subject.convert("RGBA")
    # Reserve two extra pixels for antialiased edge expansion after resampling.
    usable = CELL - margin * 2 - 4
    scale = min(usable / subject.width, usable / subject.height, 1.0)
    size = (max(1, round(subject.width * scale)), max(1, round(subject.height * scale)))
    subject = subject.resize(size, Image.Resampling.LANCZOS)
    canvas = Image.new("RGBA", (CELL, CELL))
    canvas.alpha_composite(subject, ((CELL - subject.width) // 2, foot - subject.height))
    bbox = canvas.getchannel("A").getbbox()
    if bbox is None or min(bbox[0], bbox[1], CELL - bbox[2], CELL - bbox[3]) < margin:
        raise RuntimeError("subject violates output safe margin")
    return canvas


def _material_map(color: Image.Image) -> Image.Image:
    red, green, blue, alpha = color.convert("RGBA").split()
    luminance = Image.merge("RGB", (red, green, blue)).convert("L")
    roughness = ImageChops.invert(luminance).point(lambda value: 70 + value * 150 // 255)
    emissive = ImageChops.subtract(blue, red).point(lambda value: min(255, value * 3))
    metalness = luminance.point(lambda value: 65 + value * 145 // 255)
    return Image.merge("RGBA", (roughness, emissive, metalness, alpha))


def _boost_warning_accents(image: Image.Image, ecology: str) -> Image.Image:
    if ecology not in {"lightning", "chaos"}:
        return image
    result = image.copy()
    pixels = result.load()
    for y in range(result.height):
        for x in range(result.width):
            red, green, blue, alpha = pixels[x, y]
            if alpha > 96 and red >= green + 12 and green >= blue + 12 and red > 80:
                pixels[x, y] = (max(red, 194), max(green, 154), min(blue, 88), alpha)
    return result


def _stage_image(path: Path, image: Image.Image) -> Path:
    with NamedTemporaryFile(suffix=".png", dir=path.parent, delete=False) as temporary:
        temp = Path(temporary.name)
    image.save(temp)
    return temp


def _stage_json(path: Path, value: dict) -> Path:
    with NamedTemporaryFile(mode="w", suffix=".json", dir=path.parent,
                            encoding="utf-8", delete=False) as temporary:
        json.dump(value, temporary, indent=2, sort_keys=True)
        temporary.write("\n")
        temp = Path(temporary.name)
    return temp


def _rgba_hash(image: Image.Image) -> str:
    return hashlib.sha256(image.convert("RGBA").tobytes()).hexdigest()


def _canonical_element_doors(root: Path) -> Image.Image | None:
    assets = root / "assets" / "stage12"
    report_path = assets / "environment-props-build.json"
    color_path = assets / "element_doors.png"
    material_path = assets / "element_doors_material.png"
    # Legacy atlas bootstrap is legal only for a root with no published report.
    if not report_path.exists():
        return None
    try:
        if not report_path.is_file() or not color_path.is_file() or not material_path.is_file():
            raise RuntimeError("published report or public door files are missing")
        report = json.loads(report_path.read_text(encoding="utf-8"))
        if (report.get("schema_version") != 1
                or set(report.get("ecologies", {})) != set(ECOLOGIES)):
            raise RuntimeError("published report schema is invalid")
        expected_hash = report["element_doors"]["rgba_sha256"]
        if not isinstance(expected_hash, str) or len(expected_hash) != 64:
            raise RuntimeError("published public door hash is invalid")
        color = Image.open(color_path).convert("RGBA")
        material = Image.open(material_path).convert("RGBA")
    except (OSError, KeyError, TypeError, json.JSONDecodeError, RuntimeError) as error:
        raise RuntimeError(f"published element doors validation failed: {error}") from error
    if (color.size != (1024, 256) or material.size != color.size
            or color.getchannel("A").tobytes() != material.getchannel("A").tobytes()
            or material.tobytes() != _material_map(color).tobytes()
            or _rgba_hash(color) != expected_hash):
        raise RuntimeError("published element doors validation failed")
    return color.copy()


def build_element_doors(root: Path) -> tuple[Image.Image, Image.Image]:
    canonical = _canonical_element_doors(root)
    if canonical is not None:
        return canonical, _material_map(canonical)
    output = Image.new("RGBA", (CELL * 4, CELL))
    sources = (("fire", root / "assets" / "stage12" / "fire_environment.png",
                (768, 0, 1024, 256)),
               ("water", root / "assets" / "stage12" / "water_environment.png",
                (512, 0, 768, 256)),
               ("lightning", root / "assets" / "stage12" / "lightning_environment.png",
                (512, 0, 768, 256)),
               ("chaos", root / "assets" / "stage12" / "chaos_environment.png",
                (512, 0, 768, 256)))
    assets = root / "assets" / "stage12"
    # Capture every legacy source before callers can replace an ecology atlas.
    captured = [(name, Image.open(path).convert("RGBA").crop(box))
                for name, path, box in sources]
    for index, (name, source) in enumerate(captured):
        if max(source.size) > CELL:
            scale = CELL / max(source.size)
            source = source.resize((round(source.width * scale), round(source.height * scale)),
                                   Image.Resampling.LANCZOS)
        components = _components(source.getchannel("A"))
        if not components:
            raise RuntimeError(f"{name}: door source is empty")
        alpha = Image.new("L", source.size)
        for point in components[0]: alpha.putpixel(point, source.getchannel("A").getpixel(point))
        source.putalpha(alpha)
        bbox = alpha.getbbox()
        assert bbox is not None
        source = source.crop((max(0, bbox[0] - 12), max(0, bbox[1] - 12),
                              min(CELL, bbox[2] + 12), min(CELL, bbox[3] + 12)))
        output.alpha_composite(_fit_to_cell(source, 12, 244), (index * CELL, 0))
    return output, _material_map(output)


def build_ecology_environment(ecology: str, root: Path) -> tuple[Image.Image, Image.Image, dict]:
    if ecology not in ECOLOGIES:
        raise ValueError(f"unknown ecology: {ecology}")
    source_root = root / "art_source" / "stage12"
    concept = Image.open(source_root / f"{ecology}-environment-concept-v1.png").convert("RGBA")
    atlas = Image.new("RGBA", (768, 768))
    room = ImageEnhance.Contrast(concept.resize((512, 512), Image.Resampling.LANCZOS)).enhance(1.08)
    atlas.alpha_composite(room, (0, 0))
    objects: dict[str, dict] = {}
    wall = Image.open(source_root / "backgrounds" / ecology / f"{ecology}-wall-tile-v1.png").convert("RGBA")
    object_sources = {"wall": wall}
    object_sources.update({name: concept.crop(box) for name, box in CONCEPT_CROPS[ecology].items()})
    for name, position in LAYOUT.items():
        # A wall tile is authored as a complete opaque background surface;
        # unlike isolated concept props it must not be reduced to one fragment.
        isolated = (object_sources[name] if name == "wall"
                    else _retain_subject(object_sources[name], 8, keep_small=False))
        cell = _fit_to_cell(isolated, 8, 244)
        atlas.alpha_composite(cell, position)
        bbox = cell.getchannel("A").getbbox()
        if bbox is None:
            raise RuntimeError(f"{ecology}/{name}: empty output")
        components = _components(cell.getchannel("A"))
        if len(components) > 1 and len(components[1]) * 100 > len(components[0]) * 2:
            raise RuntimeError(f"{ecology}/{name}: unrelated second component")
        objects[name] = {"source_rect": [position[0], position[1], CELL, CELL],
                         "alpha_bbox": list(bbox),
                         "foot_anchor": [(bbox[0] + bbox[2]) // 2, bbox[3]]}
    atlas = _boost_warning_accents(atlas, ecology)
    return atlas, _material_map(atlas), {"objects": objects}


def _validate_staged_outputs(staged: dict[Path, Path]) -> None:
    def image(path: Path) -> Image.Image:
        return Image.open(staged[path]).convert("RGBA")
    assets = next(iter(staged)).parent
    color = image(assets / "element_doors.png")
    material = image(assets / "element_doors_material.png")
    if color.size != (1024, 256) or color.getchannel("A").tobytes() != material.getchannel("A").tobytes():
        raise RuntimeError("element doors color/material validation failed")
    for ecology in ECOLOGIES:
        color = image(assets / f"{ecology}_environment.png")
        material = image(assets / f"{ecology}_environment_material.png")
        if color.size != (768, 768) or color.getchannel("A").tobytes() != material.getchannel("A").tobytes():
            raise RuntimeError(f"{ecology}: color/material validation failed")
    report = json.loads(staged[assets / "environment-props-build.json"].read_text(encoding="utf-8"))
    if report.get("schema_version") != 1 or set(report.get("ecologies", {})) != set(ECOLOGIES):
        raise RuntimeError("environment props report schema validation failed")
    if report.get("element_doors", {}).get("rgba_sha256") != _rgba_hash(image(assets / "element_doors.png")):
        raise RuntimeError("element doors hash validation failed")
    for ecology in ECOLOGIES:
        color = image(assets / f"{ecology}_environment.png")
        objects = report["ecologies"][ecology].get("objects", {})
        if set(objects) != set(LAYOUT):
            raise RuntimeError(f"{ecology}: report object set validation failed")
        for name, (x, y) in LAYOUT.items():
            record = objects[name]
            if record.get("source_rect") != [x, y, CELL, CELL]:
                raise RuntimeError(f"{ecology}/{name}: report source rectangle validation failed")
            bbox = color.crop((x, y, x + CELL, y + CELL)).getchannel("A").getbbox()
            if bbox is None or record.get("alpha_bbox") != list(bbox):
                raise RuntimeError(f"{ecology}/{name}: report alpha validation failed")
            if record.get("foot_anchor") != [(bbox[0] + bbox[2]) // 2, bbox[3]]:
                raise RuntimeError(f"{ecology}/{name}: report anchor validation failed")
    if os.environ.get("ARPG_ENVIRONMENT_PROPS_FAIL_VALIDATION") == "1":
        raise RuntimeError("forced staged validation failure")


def validate_outputs(root: Path) -> None:
    assets = root / "assets" / "stage12"
    _validate_staged_outputs({path: path for path in (
        assets / "element_doors.png", assets / "element_doors_material.png",
        assets / "environment-props-build.json", *(
            path for ecology in ECOLOGIES for path in (
                assets / f"{ecology}_environment.png",
                assets / f"{ecology}_environment_material.png")))})


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    args = parser.parse_args()
    root = args.root.resolve()
    assets = root / "assets" / "stage12"
    assets.mkdir(parents=True, exist_ok=True)
    door_color, door_material = build_element_doors(root)
    staged: list[tuple[Path, Image.Image]] = [(assets / "element_doors.png", door_color),
                                                (assets / "element_doors_material.png", door_material)]
    report = {"schema_version": 1, "element_doors": {"rgba_sha256": _rgba_hash(door_color)},
              "ecologies": {}}
    for ecology in ECOLOGIES:
        color, material, record = build_ecology_environment(ecology, root)
        staged.extend(((assets / f"{ecology}_environment.png", color),
                       (assets / f"{ecology}_environment_material.png", material)))
        report["ecologies"][ecology] = record
    temporary: dict[Path, Path] = {}
    try:
        for path, image in staged:
            temporary[path] = _stage_image(path, image)
        report_path = assets / "environment-props-build.json"
        temporary[report_path] = _stage_json(report_path, report)
        _validate_staged_outputs(temporary)
        for path, temporary_path in temporary.items():
            os.replace(temporary_path, path)
    finally:
        for temporary_path in temporary.values():
            temporary_path.unlink(missing_ok=True)


if __name__ == "__main__":
    main()
