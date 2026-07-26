from __future__ import annotations

from pathlib import Path
import unittest

from PIL import Image


ROOT = Path(__file__).resolve().parents[2]
ICON_PATH = ROOT / "assets" / "launcher" / "infinite_dungeon.ico"
EXPECTED_SIZES = {(16, 16), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)}


class LauncherIconTests(unittest.TestCase):
    def test_icon_contains_the_required_rgba_frames(self) -> None:
        with Image.open(ICON_PATH) as icon:
            self.assertEqual(set(icon.ico.sizes()), EXPECTED_SIZES)

            frames = {}
            for size in EXPECTED_SIZES:
                icon.size = size
                frames[size] = icon.convert("RGBA").copy()

        alpha_extrema = frames[(256, 256)].getchannel("A").getextrema()
        self.assertEqual(alpha_extrema, (0, 255))

        pixels = list(frames[(256, 256)].getdata())
        self.assertTrue(any(alpha > 0 for _, _, _, alpha in pixels))
        self.assertTrue(any(red or green or blue for red, green, blue, alpha in pixels if alpha > 0))


if __name__ == "__main__":
    unittest.main()
