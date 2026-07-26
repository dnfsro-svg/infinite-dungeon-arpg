from __future__ import annotations

import hashlib
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

from PIL import Image


ROOT = Path(__file__).resolve().parents[2]
CASES = {
    "draw_slash": ((1254, 1254), 36),
    "storm_swords": ((1024, 1536), 24),
}


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


class ActiveSkillMaterialAssetPipelineTests(unittest.TestCase):
    def test_checked_in_material_maps_match_dimensions_and_alpha(self) -> None:
        for name, (expected_size, _frame_count) in CASES.items():
            with self.subTest(name=name):
                with Image.open(ROOT / "assets" / "skills" / f"{name}_atlas.png") as color:
                    with Image.open(
                        ROOT / "assets" / "skills" / f"{name}_atlas_material.png"
                    ) as material:
                        self.assertEqual(color.size, expected_size)
                        self.assertEqual(material.size, expected_size)
                        self.assertEqual(
                            color.getchannel("A").tobytes(),
                            material.getchannel("A").tobytes(),
                        )

    def test_generator_is_deterministic(self) -> None:
        hashes: list[dict[str, str]] = []
        with tempfile.TemporaryDirectory() as first, tempfile.TemporaryDirectory() as second:
            for output in (Path(first), Path(second)):
                subprocess.run(
                    [
                        sys.executable,
                        str(ROOT / "tools" / "build_active_skill_material_maps.py"),
                        "--root",
                        str(ROOT),
                        "--output-dir",
                        str(output),
                    ],
                    check=True,
                )
                hashes.append(
                    {
                        name: sha256(output / f"{name}_atlas_material.png")
                        for name in CASES
                    }
                )
        self.assertEqual(hashes[0], hashes[1])


if __name__ == "__main__":
    unittest.main()
