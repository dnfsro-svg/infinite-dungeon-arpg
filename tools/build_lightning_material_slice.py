"""Build and verify original lightning-ecology material atlases."""

from __future__ import annotations

import argparse
import math
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
FOOT_BASELINE = 92
SOURCE_SAFE_MARGIN_RATIO = 0.025
SEQUENCES = {
    "shooter": (
        ("idle", "lightning-shooter-idle-12-alpha-v4.png", 4, 3, 12),
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
    from build_environment_props import _atomic_image, build_ecology_environment
    atlas, material, _ = build_ecology_environment("lightning", ROOT)
    _atomic_image(OUTPUT / "lightning_environment.png", atlas)
    _atomic_image(OUTPUT / "lightning_environment_material.png", material)


def alpha_bbox(image: Image.Image) -> tuple[int, int, int, int]:
    bbox = image.getchannel("A").getbbox()
    if bbox is None:
        raise RuntimeError("source character has no visible pixels")
    return bbox


def alpha_components(frame: Image.Image,
                     threshold: int = 96) -> list[set[tuple[int, int]]]:
    alpha = frame.getchannel("A")
    remaining = {(x, y) for y in range(frame.height) for x in range(frame.width)
                 if alpha.getpixel((x, y)) > threshold}
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
    components.sort(key=len, reverse=True)
    return components


def component_bbox(component: set[tuple[int, int]]) -> tuple[int, int, int, int]:
    return (min(x for x, _ in component), min(y for _, y in component),
            max(x for x, _ in component) + 1,
            max(y for _, y in component) + 1)


def validate_source_tile(tile: Image.Image, label: str) -> None:
    components = alpha_components(tile)
    if not components:
        raise RuntimeError(f"{label}: source subject is empty")
    visible = sum(len(component) for component in components)
    largest = len(components[0])
    second = len(components[1]) if len(components) > 1 else 0
    if largest * 100 < visible * 94 or second * 100 > visible * 2:
        raise RuntimeError(
            f"{label}: source subject integrity failed "
            f"visible={visible} largest={largest} second={second}")
    if largest < max(64, tile.width * tile.height // 200):
        raise RuntimeError(f"{label}: source main silhouette is incomplete")
    left, top, right, bottom = component_bbox(components[0])
    margin = max(8, math.ceil(min(tile.width, tile.height)
                              * SOURCE_SAFE_MARGIN_RATIO))
    margins = (left, top, tile.width - right, tile.height - bottom)
    if min(margins) < margin:
        raise RuntimeError(
            f"{label}: source main silhouette violates {margin}px safe margin "
            f"left/top/right/bottom={margins}")


def split_board(board: Image.Image, columns: int, rows: int,
                frame_count: int, label: str = "source board") -> list[Image.Image]:
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
        validate_source_tile(tile, f"{label} frame {frame}")
        frames.append(tile.crop(alpha_bbox(tile)))
    return frames


def detect_source_frames(board: Image.Image, columns: int, rows: int,
                         frame_count: int, label: str) -> list[Image.Image]:
    """Directly crop complete source subjects; never rewrite source pixels."""
    components = alpha_components(board)
    if len(components) < frame_count:
        raise RuntimeError(
            f"{label}: detected only {len(components)} of {frame_count} subjects")
    subjects = components[:frame_count]
    if len(components) > frame_count:
        noise = len(components[frame_count])
        if noise * 100 > len(subjects[-1]) * 2:
            raise RuntimeError(
                f"{label}: unassigned component is too large to be chroma noise "
                f"noise={noise} smallest_subject={len(subjects[-1])}")

    slots: dict[int, tuple[set[tuple[int, int]], tuple[int, int, int, int]]] = {}
    for component in subjects:
        bounds = component_bbox(component)
        center_x = (bounds[0] + bounds[2]) * 0.5
        center_y = (bounds[1] + bounds[3]) * 0.5
        column = min(columns - 1, int(center_x * columns / board.width))
        row = min(rows - 1, int(center_y * rows / board.height))
        slot = row * columns + column
        if slot >= frame_count or slot in slots:
            raise RuntimeError(
                f"{label}: subjects do not map one-to-one to target frames "
                f"slot={slot}")
        slots[slot] = (component, bounds)
    if len(slots) != frame_count or set(slots) != set(range(frame_count)):
        raise RuntimeError(f"{label}: target-frame mapping has omissions")

    padding = 8
    crop_bounds: list[tuple[int, int, int, int]] = []
    for slot in range(frame_count):
        _, bounds = slots[slot]
        crop = (bounds[0] - padding, bounds[1] - padding,
                bounds[2] + padding, bounds[3] + padding)
        if (crop[0] < 0 or crop[1] < 0 or crop[2] > board.width
                or crop[3] > board.height):
            raise RuntimeError(
                f"{label} frame {slot}: complete subject lacks outer safe margin")
        crop_bounds.append(crop)
    for first in range(frame_count):
        for second in range(first + 1, frame_count):
            lhs = crop_bounds[first]
            rhs = crop_bounds[second]
            overlaps = (max(lhs[0], rhs[0]) < min(lhs[2], rhs[2])
                        and max(lhs[1], rhs[1]) < min(lhs[3], rhs[3]))
            if overlaps:
                raise RuntimeError(
                    f"{label}: direct subject crops overlap frames "
                    f"{first} and {second}")

    frames: list[Image.Image] = []
    mapped_pixels = 0
    for slot, crop in enumerate(crop_bounds):
        frame = board.crop(crop)
        validate_source_tile(frame, f"{label} frame {slot}")
        mapped_pixels += len(slots[slot][0])
        frames.append(frame)
    if mapped_pixels != sum(len(component) for component in subjects):
        raise RuntimeError(f"{label}: source subject pixels were omitted")
    return frames


def connected_components(frame: Image.Image) -> tuple[int, int, int]:
    components = alpha_components(frame)
    visible = sum(len(component) for component in components)
    return visible, len(components[0]) if components else 0, (
        len(components[1]) if len(components) > 1 else 0)


def remove_chroma_islands(frame: Image.Image) -> Image.Image:
    """Drop only tiny detached alpha specks left by chroma-key antialiasing."""
    result = frame.copy()
    alpha = result.getchannel("A")
    components = alpha_components(result)
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


def align_to_main_component(frame: Image.Image) -> Image.Image:
    """Align after cleanup, using the surviving main component as the anchor."""
    components = alpha_components(frame)
    if not components:
        raise RuntimeError("cannot align an empty frame")
    left, _, right, bottom = component_bbox(components[0])
    dx = CELL // 2 - ((left + right - 1) // 2)
    dy = FOOT_BASELINE - (bottom - 1)
    aligned = Image.new("RGBA", frame.size)
    aligned.alpha_composite(frame, (dx, dy))
    return aligned


def normalized_frames(board_path: Path, columns: int, rows: int,
                      frame_count: int) -> list[Image.Image]:
    board = Image.open(board_path).convert("RGBA")
    poses = detect_source_frames(
        board, columns, rows, frame_count, board_path.stem)
    main_bounds = [component_bbox(alpha_components(pose)[0]) for pose in poses]
    scale = min(184.0 / max(bounds[3] - bounds[1] for bounds in main_bounds),
                188.0 / max(bounds[2] - bounds[0] for bounds in main_bounds))
    frames: list[Image.Image] = []
    for pose in poses:
        pose = pose.resize((round(pose.width * scale), round(pose.height * scale)),
                           Image.Resampling.LANCZOS)
        pose = pose.crop(alpha_bbox(pose))
        canvas = Image.new("RGBA", (CELL * 2, CELL * 2))
        canvas.alpha_composite(pose,
            ((canvas.width - pose.width) // 2, 186 - pose.height))
        cleaned = remove_chroma_islands(
            canvas.resize((CELL, CELL), Image.Resampling.LANCZOS))
        frames.append(align_to_main_component(cleaned))
    return frames


def visible_points(frame: Image.Image) -> set[tuple[int, int]]:
    components = alpha_components(frame)
    return set().union(*components) if components else set()


def registered_pose_similarity(lhs: Image.Image,
                               rhs: Image.Image) -> tuple[float, tuple[int, int]]:
    lhs_points = visible_points(lhs)
    rhs_points = visible_points(rhs)
    if not lhs_points or not rhs_points:
        return 0.0, (0, 0)
    lhs_x = sum(x for x, _ in lhs_points) / len(lhs_points)
    lhs_y = sum(y for _, y in lhs_points) / len(lhs_points)
    rhs_x = sum(x for x, _ in rhs_points) / len(rhs_points)
    rhs_y = sum(y for _, y in rhs_points) / len(rhs_points)
    base_dx = round(rhs_x - lhs_x)
    base_dy = round(rhs_y - lhs_y)
    best = (0.0, (0, 0))
    for dy in range(base_dy - 2, base_dy + 3):
        for dx in range(base_dx - 2, base_dx + 3):
            translated = {(x + dx, y + dy) for x, y in lhs_points
                          if 0 <= x + dx < lhs.width
                          and 0 <= y + dy < lhs.height}
            union = translated | rhs_points
            similarity = len(translated & rhs_points) / len(union) if union else 0.0
            if similarity > best[0]:
                best = (similarity, (dx, dy))
    return best


def _cosine_similarity(lhs: list[float], rhs: list[float]) -> float:
    denominator = math.sqrt(sum(value * value for value in lhs)
                            * sum(value * value for value in rhs))
    if denominator <= 1.0e-9:
        return 0.0
    return sum(first * second for first, second in zip(lhs, rhs)) / denominator


def _trajectory_follows(lhs: list[float], total: list[float]) -> bool:
    total_energy = sum(value * value for value in total)
    lhs_energy = sum(value * value for value in lhs)
    if total_energy <= 1.0e-12:
        return lhs_energy <= 1.0e-12
    return _cosine_similarity(lhs, total) >= 0.96


def _linear_interpolation_evidence(before: Image.Image, middle: Image.Image,
                                   after: Image.Image,
                                   before_shift: tuple[int, int],
                                   after_shift: tuple[int, int]) -> bool:
    samples: list[tuple[int, int, int]] = []
    alpha_moments = [[0.0] * 6 for _ in range(3)]
    transparent = (0, 0, 0, 0)
    for y in range(middle.height):
        for x in range(middle.width):
            before_x = x - before_shift[0]
            before_y = y - before_shift[1]
            after_x = x - after_shift[0]
            after_y = y - after_shift[1]
            first = (before.getpixel((before_x, before_y))
                     if 0 <= before_x < before.width
                     and 0 <= before_y < before.height else transparent)
            second = middle.getpixel((x, y))
            third = (after.getpixel((after_x, after_y))
                     if 0 <= after_x < after.width
                     and 0 <= after_y < after.height else transparent)
            if first[3] <= 24 and second[3] <= 24 and third[3] <= 24:
                continue
            for channel in range(4):
                samples.append((first[channel], second[channel], third[channel]))
            for moment, pixel in zip(alpha_moments, (first, second, third)):
                alpha = pixel[3] / 255.0
                moment[0] += alpha
                moment[1] += alpha * x
                moment[2] += alpha * y
                moment[3] += alpha * x * x
                moment[4] += alpha * y * y
                moment[5] += alpha * x * y
    if not samples:
        return False
    total_difference = [third - first for first, _, third in samples]
    total_energy = sum(value * value for value in total_difference)
    if total_energy <= 1.0:
        return False
    early_difference = [second - first for first, second, _ in samples]
    late_difference = [third - second for _, second, third in samples]
    amount = sum(total * early for total, early
                 in zip(total_difference, early_difference)) / total_energy
    if not 0.12 <= amount <= 0.88:
        return False
    residuals = [second - (first + amount * (third - first))
                 for first, second, third in samples]
    residual_ratio = math.sqrt(
        sum(value * value for value in residuals) / total_energy)
    approximate = {
        threshold: sum(abs(value) <= threshold for value in residuals)
        / len(residuals)
        for threshold in (4, 8, 16, 32)
    }

    # Alpha-weighted contour moments act as stable silhouette keypoints.  A
    # blended/resampled tween keeps their trajectory collinear even when edge
    # pixels have been antialiased; a genuinely redrawn pose does not.
    scales = (middle.width * middle.height,
              middle.width * middle.width * middle.height,
              middle.height * middle.width * middle.height,
              middle.width ** 3 * middle.height,
              middle.height ** 3 * middle.width,
              middle.width ** 2 * middle.height ** 2)
    normalized = [[value / scale for value, scale in zip(moment, scales)]
                  for moment in alpha_moments]
    moment_total = [third - first for first, third
                    in zip(normalized[0], normalized[2])]
    moment_early = [second - first for first, second
                    in zip(normalized[0], normalized[1])]
    moment_late = [third - second for second, third
                   in zip(normalized[1], normalized[2])]
    return (
        residual_ratio <= 0.29
        and _cosine_similarity(early_difference, total_difference) >= 0.84
        and _cosine_similarity(late_difference, total_difference) >= 0.84
        and approximate[16] >= 0.85
        and approximate[32] >= 0.89
        and _trajectory_follows(moment_early, moment_total)
        and _trajectory_follows(moment_late, moment_total))


def is_linear_interpolation(before: Image.Image, middle: Image.Image,
                            after: Image.Image) -> bool:
    _, before_shift = registered_pose_similarity(before, middle)
    _, after_shift = registered_pose_similarity(after, middle)
    # Evaluate the pose-registered triplet and an anchor-preserving candidate.
    # The latter matters for articulated poses whose limbs move the alpha
    # centroid while their feet/body remain intentionally fixed.
    candidates = {(before_shift, after_shift), ((0, 0), (0, 0))}
    return any(_linear_interpolation_evidence(
        before, middle, after, candidate_before, candidate_after)
        for candidate_before, candidate_after in candidates)


def validate_frames(frames: list[Image.Image], label: str) -> None:
    if len({frame.tobytes() for frame in frames}) != len(frames):
        raise RuntimeError(f"{label}: duplicate whole frames")
    for index in range(1, len(frames) - 1):
        if is_linear_interpolation(frames[index - 1], frames[index],
                                   frames[index + 1]):
            raise RuntimeError(
                f"{label} frames {index - 1}/{index}/{index + 1}: "
                "three-frame linear interpolation detected")
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
        similarity, shift = registered_pose_similarity(current, following)
        if similarity >= 0.97:
            reason = ("whole-frame translation" if shift != (0, 0)
                      else "registered pose similarity lacks authored change")
            raise RuntimeError(
                f"{label} frame {index}->{index + 1}: {reason} "
                f"similarity={similarity:.3f} shift={shift}")


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
