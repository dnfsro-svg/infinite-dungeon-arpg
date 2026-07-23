"""Contract tests for native, non-upscaled Stage 12 room backgrounds."""

from __future__ import annotations

import hashlib
import json
import subprocess
import sys
import tarfile
import tempfile
import unittest
from collections import Counter
from pathlib import Path

from PIL import Image, ImageStat


ROOT = Path(__file__).resolve().parents[2]
ECOLOGIES = ("fire", "water", "lightning", "chaos")
SOURCE_MANIFEST = ROOT / "art_source/stage12/backgrounds/background-sources.json"
BUILDER = ROOT / "tools/build_native_room_backgrounds.py"
REPORT = ROOT / "assets/stage12/room-background-build.json"
CONTINUOUS_ROOM_ROI = (864, 486, 2976, 1674)


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def luminance_stddev(image: Image.Image) -> float:
    return ImageStat.Stat(image.convert("L")).stddev[0]


def declared_sources(ecology: str, root: Path = ROOT) -> list[dict[str, str]]:
    manifest = json.loads((root / SOURCE_MANIFEST).read_text(encoding="utf-8"))
    return manifest["ecologies"][ecology]["sources"]


def rectangles_cover_roi(rectangles: list[list[int]], roi: tuple[int, int, int, int]) -> bool:
    left, top, right, bottom = roi
    xs = {left, right}
    for rect_left, _, rect_right, _ in rectangles:
        xs.update((max(left, min(right, rect_left)), max(left, min(right, rect_right))))
    for x0, x1 in zip(sorted(xs), sorted(xs)[1:]):
        if x0 == x1:
            continue
        cursor = top
        for y0, y1 in sorted(
            (max(top, b), min(bottom, d))
            for a, b, c, d in rectangles
            if a <= x0 and c >= x1
        ):
            if y0 > cursor:
                return False
            cursor = max(cursor, y1)
        if cursor < bottom:
            return False
    return True


def ecology_color_share(image: Image.Image, ecology: str) -> float:
    pixels = list(image.convert("RGB").getdata())
    if ecology == "water":
        hit = lambda r, g, b: b > r * 1.12 and g > r * 1.08 and b > 55
    elif ecology == "lightning":
        hit = lambda r, g, b: b > r * 1.18 and b > g * 1.04 and b > 70
    elif ecology == "chaos":
        hit = lambda r, g, b: r > g * 1.18 and b > g * 1.16 and max(r, b) > 55
    else:
        hit = lambda r, g, b: r > g * 1.18 and r > b * 1.25 and r > 55
    return sum(hit(*pixel) for pixel in pixels) / len(pixels)


def seam_jump(image: Image.Image, axis: str, coordinate: int, start: int, end: int) -> float:
    """Mean RGB discontinuity across an explicit placement boundary."""
    rgb = image.convert("RGB")
    if axis == "x":
        pairs = (abs(a - b) for y in range(start, end) for a, b in zip(rgb.getpixel((coordinate - 1, y)), rgb.getpixel((coordinate, y))))
    else:
        pairs = (abs(a - b) for x in range(start, end) for a, b in zip(rgb.getpixel((x, coordinate - 1)), rgb.getpixel((x, coordinate))))
    values = list(pairs)
    return sum(values) / len(values)


def edge_band_metrics(image: Image.Image) -> dict[str, tuple[float, float]]:
    """Return luma variance and dominant-colour share for the four outer bands."""
    width, height = image.size
    bands = {
        "top": (0, 0, width, 180),
        "bottom": (0, height - 180, width, height),
        "left": (0, 180, 220, height - 180),
        "right": (width - 220, 180, width, height - 180),
    }
    metrics: dict[str, tuple[float, float]] = {}
    for name, box in bands.items():
        pixels = list(image.convert("RGB").crop(box).getdata())
        dominant_share = max(Counter(pixels).values()) / len(pixels)
        metrics[name] = (ImageStat.Stat(image.convert("L").crop(box)).stddev[0], dominant_share)
    return metrics


def edge_band_profile_jump(image: Image.Image, band: str) -> float:
    """Detect a hard horizontal or vertical panel edge in one outer band."""
    width, height = image.size
    box = {
        "top": (0, 0, width, 180),
        "bottom": (0, height - 180, width, height),
        "left": (0, 180, 220, height - 180),
        "right": (width - 220, 180, width, height - 180),
    }[band]
    crop = image.convert("L").crop(box)
    if band in {"top", "bottom"}:
        profile = [ImageStat.Stat(crop.crop((0, y, crop.width, y + 1))).mean[0] for y in range(crop.height)]
    else:
        profile = [ImageStat.Stat(crop.crop((x, 0, x + 1, crop.height))).mean[0] for x in range(crop.width)]
    return max(abs(current - previous) for previous, current in zip(profile, profile[1:]))


class NativeRoomBackgroundAssetPipelineTests(unittest.TestCase):
    def test_manifest_declares_four_isolated_ecologies_with_native_sources(self) -> None:
        manifest = json.loads(SOURCE_MANIFEST.read_text(encoding="utf-8"))
        self.assertEqual(set(manifest["ecologies"]), set(ECOLOGIES))
        for ecology in ECOLOGIES:
            entries = declared_sources(ecology)
            self.assertGreaterEqual(len(entries), 3)
            self.assertEqual(len({entry["path"] for entry in entries}), len(entries))
            for entry in entries:
                source = ROOT / entry["path"]
                self.assertIn(f"/backgrounds/{ecology}/", entry["path"])
                self.assertTrue(source.is_file(), source)
                with Image.open(source) as image:
                    self.assertGreaterEqual(image.width, 720)
                    self.assertGreaterEqual(image.height, 720)

    def test_indexed_clean_tree_contains_every_source_and_rebuilds_all_ecologies(self) -> None:
        inputs = [entry["path"] for ecology in ECOLOGIES for entry in declared_sources(ecology)]
        tracked = subprocess.run(["git", "ls-files", "--error-unmatch", "--", *inputs], cwd=ROOT, text=True, capture_output=True)
        self.assertEqual(tracked.returncode, 0, tracked.stderr)
        tree = subprocess.check_output(["git", "write-tree"], cwd=ROOT, text=True).strip()
        with tempfile.TemporaryDirectory() as temporary:
            archive = Path(temporary) / "candidate.tar"
            with archive.open("wb") as stream:
                subprocess.run(["git", "archive", "--format=tar", tree], cwd=ROOT, stdout=stream, check=True)
            with tarfile.open(archive) as bundle:
                bundle.extractall(temporary, filter="data")
            clean_root = Path(temporary)
            for path in inputs:
                self.assertTrue((clean_root / path).is_file(), f"archive missing manifest input: {path}")
            for ecology in ECOLOGIES:
                rebuilt = subprocess.run([sys.executable, str(clean_root / "tools/build_native_room_backgrounds.py"), "--ecology", ecology], cwd=clean_root, text=True, capture_output=True)
                self.assertEqual(rebuilt.returncode, 0, rebuilt.stderr or rebuilt.stdout)

    def test_all_ecology_builds_are_native_distinct_and_combat_readable(self) -> None:
        self.assertTrue(BUILDER.is_file())
        for ecology in ECOLOGIES:
            result = subprocess.run([sys.executable, str(BUILDER), "--ecology", ecology], cwd=ROOT, text=True, capture_output=True)
            self.assertEqual(result.returncode, 0, result.stderr or result.stdout)
        report = json.loads(REPORT.read_text(encoding="utf-8"))
        runtime_hashes: set[str] = set()
        for ecology in ECOLOGIES:
            with self.subTest(ecology=ecology):
                entries = declared_sources(ecology)
                data = report["ecologies"][ecology]
                master = ROOT / f"art_source/stage12/backgrounds/{ecology}/{ecology}-room-background-master.png"
                runtime = ROOT / f"assets/stage12/{ecology}_room_background.png"
                material = ROOT / f"assets/stage12/{ecology}_room_background_material.png"
                self.assertEqual(data["master_size"], [3840, 2160])
                self.assertEqual(data["runtime_size"], [2560, 1440])
                self.assertEqual(data["runtime_from_master"], {"resampling": "LANCZOS", "passes": 1})
                self.assertEqual(data["source_sha256"], {entry["path"]: sha256(ROOT / entry["path"]) for entry in entries})
                self.assertEqual(data["output_sha256"], {"master": sha256(master), "runtime": sha256(runtime), "material": sha256(material)})
                self.assertTrue(all(f"/backgrounds/{ecology}/" in source for source in data["source_sha256"]))
                self.assertEqual(
                    {entry["path"] for entry in entries},
                    {placement["source"] for placement in data["placements"]},
                    "every declared source must be consumed by an actual placement",
                )
                continuous = [placement["target_rect"] for placement in data["placements"] if placement.get("continuous_room")]
                self.assertTrue(rectangles_cover_roi(continuous, CONTINUOUS_ROOM_ROI))
                continuous_details = [placement for placement in data["placements"] if placement.get("continuous_room")]
                consumed_crops = {(placement["source"], tuple(placement["source_rect"])) for placement in continuous_details}
                self.assertEqual(len(consumed_crops), len(continuous_details), "continuous room repeats an identical source crop")
                for placement in data["placements"]:
                    sx0, sy0, sx1, sy1 = placement["source_rect"]
                    tx0, ty0, tx1, ty1 = placement["target_rect"]
                    self.assertLessEqual(tx1 - tx0, sx1 - sx0)
                    self.assertLessEqual(ty1 - ty0, sy1 - sy0)
                    self.assertAlmostEqual(placement["scale_x"], (tx1 - tx0) / (sx1 - sx0))
                    self.assertAlmostEqual(placement["scale_y"], (ty1 - ty0) / (sy1 - sy0))
                with Image.open(master) as master_image, Image.open(runtime) as runtime_image, Image.open(material) as material_image:
                    self.assertEqual(master_image.size, (3840, 2160))
                    self.assertEqual(runtime_image.size, (2560, 1440))
                    self.assertEqual(material_image.size, (2560, 1440))
                    self.assertEqual(runtime_image.mode, "RGBA")
                    self.assertEqual(material_image.mode, "RGBA")
                    self.assertEqual(runtime_image.tobytes(), master_image.convert("RGBA").resize((2560, 1440), Image.Resampling.LANCZOS).tobytes())
                    center = master_image.crop((1120, 1030, 2720, 1870))
                    edge = master_image.crop((864, 486, 1084, 1674))
                    self.assertGreater(len(set(center.convert("RGB").getdata())), 1)
                    # A combat floor must be visibly textured but controlled;
                    # the new continuous native art intentionally has a softly
                    # vignetted outer context, so edge variance is not a proxy
                    # for room readability.
                    self.assertGreater(luminance_stddev(center), 5.0)
                    self.assertLess(luminance_stddev(center), 42.0)
                    internal_x = sorted({rect[0] for rect in continuous if CONTINUOUS_ROOM_ROI[0] + 100 < rect[0] < CONTINUOUS_ROOM_ROI[2] - 100})
                    internal_y = sorted({rect[1] for rect in continuous if CONTINUOUS_ROOM_ROI[1] + 100 < rect[1] < CONTINUOUS_ROOM_ROI[3] - 100})
                    for x in internal_x:
                        self.assertLess(seam_jump(master_image, "x", x, CONTINUOUS_ROOM_ROI[1] + 36, CONTINUOUS_ROOM_ROI[3] - 36), 46.0, f"hard vertical seam at x={x}")
                    for y in internal_y:
                        self.assertLess(seam_jump(master_image, "y", y, CONTINUOUS_ROOM_ROI[0] + 36, CONTINUOUS_ROOM_ROI[2] - 36), 46.0, f"hard horizontal seam at y={y}")
                    share = ecology_color_share(runtime_image, ecology)
                    self.assertGreater(share, 0.008, f"{ecology} accent share too small: {share}")
                    self.assertLess(share, 0.45, f"{ecology} accent share too dominant: {share}")
                    for band, (variance, dominant_share) in edge_band_metrics(runtime_image).items():
                        self.assertGreater(variance, 1.0, f"{ecology} {band} edge lacks native texture")
                        self.assertLess(dominant_share, 0.45, f"{ecology} {band} edge is too close to a blank fill")
                        self.assertLess(edge_band_profile_jump(runtime_image, band), 3.0, f"{ecology} {band} has a hard panel edge")
                runtime_hashes.add(sha256(runtime))
        self.assertEqual(len(runtime_hashes), len(ECOLOGIES))


if __name__ == "__main__":
    unittest.main()
