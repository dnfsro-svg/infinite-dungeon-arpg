"""Regression tests for authored lightning animation-board validation."""

from __future__ import annotations

import importlib.util
from pathlib import Path
import unittest

from PIL import Image, ImageDraw


ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location(
    "build_lightning_material_slice",
    ROOT / "tools" / "build_lightning_material_slice.py")
assert SPEC is not None and SPEC.loader is not None
BUILDER = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(BUILDER)


def rectangle_frame(left: int, top: int, right: int, bottom: int,
                    color=(40, 180, 220, 255)) -> Image.Image:
    frame = Image.new("RGBA", (96, 96))
    ImageDraw.Draw(frame).rectangle((left, top, right, bottom), fill=color)
    return frame


class LightningAssetPipelineTests(unittest.TestCase):
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

    def test_pose_gate_rejects_exact_three_frame_linear_interpolation(self) -> None:
        interpolated = [rectangle_frame(14 + index * 4, 24, 58 + index * 4,
                                        78, color)
                        for index, color in enumerate(((20, 80, 140, 255),
                                                       (80, 130, 180, 255),
                                                       (140, 180, 220, 255)))]
        with self.assertRaisesRegex(RuntimeError, "linear interpolation"):
            BUILDER.validate_frames(interpolated, "synthetic/interpolation")

    def test_final_atlas_states_keep_one_pixel_foot_baseline(self) -> None:
        for role in ("shooter", "dasher"):
            atlas = Image.open(
                ROOT / "assets" / "stage12" / f"lightning_{role}.png").convert("RGBA")
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
