"""Regression tests for authored chaos animation-board validation."""

from __future__ import annotations

import importlib.util
from pathlib import Path
import unittest

from PIL import Image, ImageDraw


ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location(
    "build_chaos_material_slice",
    ROOT / "tools" / "build_chaos_material_slice.py")
assert SPEC is not None and SPEC.loader is not None
BUILDER = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(BUILDER)


def rectangle_frame(left: int, top: int, right: int, bottom: int,
                    color=(40, 180, 220, 255)) -> Image.Image:
    frame = Image.new("RGBA", (96, 96))
    ImageDraw.Draw(frame).rectangle((left, top, right, bottom), fill=color)
    return frame


def rewrite_transparent_rgb(frame: Image.Image,
                            color: tuple[int, int, int]) -> Image.Image:
    """Change hidden RGB without changing any visible source pixel."""
    rewritten = frame.copy()
    rewritten.putdata([
        (*color, 0) if pixel[3] <= 24 else pixel
        for pixel in rewritten.get_flattened_data()
    ])
    return rewritten


def authored_endpoint(direction: int, color: tuple[int, int, int, int]) -> Image.Image:
    """Connected synthetic pose with a changing arm and trailing coat."""
    frame = Image.new("RGBA", (96, 96))
    draw = ImageDraw.Draw(frame)
    draw.ellipse((37, 15, 57, 35), fill=color)
    draw.polygon(((31, 35), (63, 35), (68, 77), (27, 77)), fill=color)
    if direction < 0:
        draw.polygon(((34, 41), (8, 27), (5, 35), (31, 54)), fill=color)
        draw.polygon(((32, 62), (14, 76), (29, 79), (43, 67)), fill=color)
    else:
        draw.polygon(((60, 41), (88, 23), (91, 32), (63, 54)), fill=color)
        draw.polygon(((61, 62), (82, 75), (66, 80), (49, 68)), fill=color)
    return frame


def mix_frames(before: Image.Image, after: Image.Image, amount: float) -> Image.Image:
    pixels = []
    for first, third in zip(before.get_flattened_data(),
                            after.get_flattened_data()):
        pixels.append(tuple(round(first[channel] * (1.0 - amount)
                                  + third[channel] * amount)
                            for channel in range(4)))
    result = Image.new("RGBA", before.size)
    result.putdata(pixels)
    return result


class ChaosAssetPipelineTests(unittest.TestCase):
    def test_detected_frames_directly_crop_each_complete_source_subject(self) -> None:
        detect = getattr(BUILDER, "detect_source_frames", None)
        self.assertIsNotNone(detect, "missing complete-subject crop detection")
        board = Image.new("RGBA", (192, 192))
        draw = ImageDraw.Draw(board)
        draw.rectangle((60, 30, 110, 82), fill=(30, 160, 220, 255))
        draw.rectangle((130, 25, 175, 75), fill=(220, 150, 30, 255))
        draw.rectangle((18, 115, 70, 165), fill=(40, 190, 230, 255))
        draw.rectangle((112, 108, 170, 170), fill=(210, 145, 25, 255))
        source_pixels = sorted(pixel for pixel in board.get_flattened_data()
                               if pixel[3] > 96)
        frames = detect(board, 2, 2, 4, "synthetic/detected")
        self.assertEqual(len(frames), 4)
        cropped_pixels = sorted(pixel for frame in frames
                                for pixel in frame.get_flattened_data()
                                if pixel[3] > 96)
        self.assertEqual(cropped_pixels, source_pixels,
                         "direct crops must preserve every source subject pixel")

    def test_detected_frames_reject_many_unassigned_tiny_components(self) -> None:
        board = Image.new("RGBA", (240, 240))
        draw = ImageDraw.Draw(board)
        for bounds in ((30, 30, 80, 80), (150, 30, 200, 80),
                       (30, 150, 80, 200), (150, 150, 200, 200)):
            draw.rectangle(bounds, fill=(40, 180, 220, 255))
        for y in range(92, 140, 2):
            for x in range(92, 140, 2):
                draw.point((x, y), fill=(220, 80, 190, 255))
        with self.assertRaisesRegex(RuntimeError, "chroma noise budget"):
            BUILDER.detect_source_frames(board, 2, 2, 4,
                                         "synthetic/many-islands")

    def test_source_grid_rejects_main_silhouette_touching_cell_edge(self) -> None:
        board = Image.new("RGBA", (96, 96))
        ImageDraw.Draw(board).rectangle((0, 18, 60, 90),
                                        fill=(30, 160, 220, 255))
        with self.assertRaisesRegex(RuntimeError, "safe margin"):
            BUILDER.split_board(board, 1, 1, 1)

    def test_pose_gate_rejects_whole_frame_translation(self) -> None:
        translated = [rectangle_frame(18 + shift, 24, 58 + shift, 78)
                      for shift in (0, 4)]
        with self.assertRaisesRegex(RuntimeError, "whole-frame translation"):
            BUILDER.validate_frames(translated, "synthetic/translation")

    def test_pose_gate_treats_hidden_rgb_variants_as_duplicate_frames(self) -> None:
        visible = rectangle_frame(18, 24, 58, 78)
        hidden_variant = rewrite_transparent_rgb(visible, (231, 17, 149))
        with self.assertRaisesRegex(RuntimeError, "duplicate whole frames"):
            BUILDER.validate_frames([visible, hidden_variant],
                                    "synthetic/hidden-rgb-duplicate")

    def test_adjacent_change_counts_only_visible_union_pixels(self) -> None:
        visible = rectangle_frame(18, 24, 58, 78)
        hidden_variant = rewrite_transparent_rgb(visible, (231, 17, 149))
        hidden_variant.putpixel((30, 40), (220, 30, 80, 255))
        with self.assertRaisesRegex(RuntimeError, r"only 0\.\d+% changed"):
            BUILDER.validate_frames([visible, hidden_variant],
                                    "synthetic/hidden-rgb-adjacent")

    def test_pose_gate_rejects_exact_three_frame_linear_interpolation(self) -> None:
        interpolated = [rectangle_frame(14 + index * 4, 24, 58 + index * 4,
                                        78, color)
                        for index, color in enumerate(((20, 80, 140, 255),
                                                       (80, 130, 180, 255),
                                                       (140, 180, 220, 255)))]
        with self.assertRaisesRegex(RuntimeError, "linear interpolation"):
            BUILDER.validate_frames(interpolated, "synthetic/interpolation")

    def test_pose_gate_rejects_bilinear_resampled_interpolation(self) -> None:
        before = authored_endpoint(-1, (24, 92, 148, 255))
        after = authored_endpoint(1, (148, 184, 224, 255))
        middle = mix_frames(before, after, 0.5)
        middle = middle.resize((89, 89), Image.Resampling.BILINEAR).resize(
            before.size, Image.Resampling.BILINEAR)
        with self.assertRaisesRegex(RuntimeError, "linear interpolation"):
            BUILDER.validate_frames([before, middle, after],
                                    "synthetic/bilinear-interpolation")

    def test_pose_gate_rejects_eased_interpolation(self) -> None:
        before = authored_endpoint(-1, (24, 92, 148, 255))
        after = authored_endpoint(1, (148, 184, 224, 255))
        middle = mix_frames(before, after, 0.32)
        with self.assertRaisesRegex(RuntimeError, "linear interpolation"):
            BUILDER.validate_frames([before, middle, after],
                                    "synthetic/eased-interpolation")

    def test_pose_gate_rejects_color_perturbed_compressed_interpolation(self) -> None:
        before = authored_endpoint(-1, (24, 92, 148, 255))
        after = authored_endpoint(1, (148, 184, 224, 255))
        middle = mix_frames(before, after, 0.5)
        perturbed = []
        for index, pixel in enumerate(middle.get_flattened_data()):
            delta = (-5, 3, 6, -2)[index % 4]
            perturbed.append(tuple(
                max(0, min(255, ((value + delta) // 6) * 6))
                for value in pixel))
        middle.putdata(perturbed)
        with self.assertRaisesRegex(RuntimeError, "linear interpolation"):
            BUILDER.validate_frames([before, middle, after],
                                    "synthetic/perturbed-interpolation")

    def test_pose_gate_accepts_nonlinear_authored_pose_progression(self) -> None:
        before = authored_endpoint(-1, (30, 120, 180, 255))
        middle = Image.new("RGBA", (96, 96))
        draw = ImageDraw.Draw(middle)
        draw.ellipse((36, 12, 58, 34), fill=(55, 155, 205, 255))
        draw.polygon(((27, 36), (67, 36), (61, 79), (33, 79)),
                     fill=(55, 155, 205, 255))
        draw.polygon(((31, 42), (19, 18), (27, 15), (43, 43)),
                     fill=(55, 155, 205, 255))
        draw.polygon(((63, 42), (78, 62), (70, 67), (52, 48)),
                     fill=(55, 155, 205, 255))
        after = authored_endpoint(1, (80, 190, 225, 255))
        self.assertFalse(BUILDER.is_linear_interpolation(before, middle, after),
            "a genuinely redrawn intermediate pose must remain accepted")

    def test_final_atlas_states_keep_one_pixel_foot_baseline(self) -> None:
        for role in ("chaser", "hazard"):
            atlas = Image.open(
                ROOT / "assets" / "stage12" / f"chaos_{role}.png").convert("RGBA")
            offset = 0
            for state, _, _, _, frame_count in BUILDER.SEQUENCES[role]:
                frames = [atlas.crop((
                    ((offset + index) % BUILDER.COLUMNS) * BUILDER.CELL,
                    ((offset + index) // BUILDER.COLUMNS) * BUILDER.CELL,
                    ((offset + index) % BUILDER.COLUMNS + 1) * BUILDER.CELL,
                    ((offset + index) // BUILDER.COLUMNS + 1) * BUILDER.CELL))
                    for index in range(frame_count)]
                bottoms = []
                for frame in frames:
                    bbox = frame.getchannel("A").point(
                        lambda value: 255 if value > 96 else 0).getbbox()
                    self.assertIsNotNone(bbox)
                    bottoms.append(bbox[3] - 1)
                self.assertLessEqual(max(bottoms) - min(bottoms), 1,
                    f"{role}/{state} foot baseline drifted: {bottoms}")
                offset += frame_count


if __name__ == "__main__":
    unittest.main()
