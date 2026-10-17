"""Build the deterministic Stage 12 v3 material-pack handoff archive.

The archive is intentionally a review/handoff package, not a release bundle.
It contains the selected tracked art, the static CJK font used by the current
text fix, integration references, and formal 1920 x 1080 evidence.

Safety properties:

* only Git-tracked files below ``assets`` and ``art_source/stage12`` are
  discovered automatically;
* the old variable font is excluded and the reviewed static Medium font is
  added explicitly;
* every source and the output directory reject symbolic links/reparse points;
* the full formal-evidence tree is validated by the real PowerShell validator;
* a same-directory exclusive temporary archive is verified before atomically
  publishing the final name without overwrite;
* all ZIP timestamps, permissions, ordering, JSON formatting, and compression
  settings are fixed for reproducible output;
* every member except ``MANIFEST.sha256`` is covered by that manifest (a
  manifest cannot contain its own SHA-256 without a circular definition).

Run from any directory with Python 3.10 or newer::

    python tools/package_material_pack_v2.py
"""

from __future__ import annotations

from dataclasses import dataclass
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import shutil
import stat
import struct
import subprocess
import sys
import tempfile
from typing import Any, BinaryIO, Callable, Iterable
import zipfile


PACKAGE_NAME = "arpg-material-pack-v3-native-backgrounds.zip"
PACKAGE_VERSION = "3.0.0-native-backgrounds"
PACKAGE_DATE = "2026-07-23"
ZIP_TIMESTAMP = (2026, 7, 23, 0, 0, 0)
BUFFER_SIZE = 1024 * 1024
FULL_PACK_BYTES = 329430304
RESIDENT_PEAK_BYTES = 226800928
TRANSITION_PEAK_BYTES = 261010720
RESIDENT_HARD_CAP_BYTES = 268435456

LEGACY_PACKAGE_SHA256 = {
    "deliverables/arpg-material-pack-v1.zip":
        "b61c54be14e99fc083d4ea5d665ec24d899e565c835e077926ce46cd86d0f2a4",
    "deliverables/arpg-material-pack-v2-highres-text.zip":
        "3c1c47de0e20fc3c4e97806cd2719ff44357563abf3d8c5d9b4bebfe301e6d1c",
}

VARIABLE_FONT = "assets/fonts/NotoSansSC[wght].ttf"
STATIC_FONT = "assets/fonts/NotoSansCJKsc-Medium.otf"
BACKGROUND_CANDIDATE_PREFIX = "art_source/stage12/backgrounds/candidates/"

PLAN_FILES = (
    "docs/superpowers/plans/2026-07-21-full-material-pack-rebuild.md",
    "docs/superpowers/plans/2026-07-19-stage12-comic-material-pack.md",
    "docs/superpowers/plans/2026-07-23-native-room-backgrounds.md",
    "docs/validation/native-room-backgrounds.md",
    "docs/superpowers/specs/2026-07-25-material-runtime-integration-repair-design.md",
    "docs/superpowers/plans/2026-07-25-material-runtime-integration-repair.md",
    "docs/validation/material-runtime-integration-repair.md",
)

INTEGRATION_FILES = (
    "src/app/CMakeLists.txt",
    "src/platform/raylib/CMakeLists.txt",
    "src/platform/raylib/ui_text_renderer.hpp",
    "src/platform/raylib/ui_text_renderer.cpp",
    "src/platform/raylib/ui_text_bounds_audit.hpp",
    "src/platform/raylib/ui_text_bounds_audit.cpp",
    "src/platform/raylib/ui_text_contrast.hpp",
    "src/platform/raylib/ui_typography.hpp",
    "src/platform/raylib/death_overlay_font.cpp",
    "src/platform/raylib/death_overlay_font.hpp",
    "src/platform/raylib/death_overlay_renderer.cpp",
    "src/platform/raylib/hud_layout.cpp",
    "src/platform/raylib/hud_layout.hpp",
    "src/platform/raylib/hud_renderer.cpp",
    "src/platform/raylib/hud_renderer.hpp",
    "src/platform/raylib/hud_view_model.cpp",
    "src/platform/raylib/hud_view_model.hpp",
    "src/platform/raylib/inventory_renderer.cpp",
    "src/platform/raylib/inventory_view_math.cpp",
    "src/platform/raylib/inventory_view_math.hpp",
    "src/platform/raylib/material_bag_renderer.cpp",
    "src/platform/raylib/material_bag_renderer.hpp",
    "src/platform/raylib/pause_menu_renderer.cpp",
    "src/platform/raylib/pause_menu_view.cpp",
    "src/platform/raylib/pause_menu_view.hpp",
    "src/platform/raylib/active_skill_renderer.cpp",
    "src/platform/raylib/active_skill_view.cpp",
    "src/platform/raylib/active_skill_view.hpp",
    "src/platform/raylib/actor_renderer.cpp",
    "src/platform/raylib/combat_renderer.hpp",
    "src/platform/raylib/combat_renderer.cpp",
    "src/platform/raylib/combat_view_math.cpp",
    "src/platform/raylib/combat_view_math.hpp",
    "src/platform/raylib/room_renderer.cpp",
    "src/platform/raylib/room_background_render_plan.hpp",
    "src/platform/raylib/room_background_render_plan.cpp",
    "src/platform/raylib/material_asset_validation.cpp",
    "src/platform/raylib/material_manifest.hpp",
    "src/platform/raylib/raylib_host.cpp",
    "src/platform/raylib/raylib_host.hpp",
    "src/app/arpg_game.manifest",
    "tests/platform/CMakeLists.txt",
    "tests/platform/stage12_material_formal_game_validation.cpp",
    "tests/platform/stage12_material_validator.ps1",
    "tests/platform/ui_dpi_manifest_test.cmake",
    "tests/platform/ui_material_asset_pipeline_tests.py",
    "tests/platform/native_room_background_asset_pipeline_tests.py",
    "tools/build_native_room_backgrounds.py",
    "src/platform/raylib/material_residency.hpp",
    "src/platform/raylib/material_residency.cpp",
    "src/platform/raylib/material_pack.hpp",
    "src/platform/raylib/material_pack.cpp",
    "src/platform/raylib/material_asset_types.hpp",
    "src/platform/raylib/material_animation.hpp",
    "src/platform/raylib/material_animation.cpp",
    "src/platform/raylib/monster_material_presenter.hpp",
    "src/platform/raylib/monster_material_presenter.cpp",
    "src/platform/raylib/environment_prop_layout.hpp",
    "src/platform/raylib/environment_prop_layout.cpp",
    "src/platform/raylib/active_skill_assets.hpp",
    "src/platform/raylib/active_skill_assets.cpp",
    "src/platform/raylib/host_input.cpp",
    "src/platform/raylib/raylib_input.hpp",
    "src/platform/raylib/raylib_input.cpp",
    "tools/build_environment_props.py",
    "tools/build_active_skill_material_maps.py",
    "tests/platform/environment_prop_asset_pipeline_tests.py",
    "tests/platform/active_skill_material_asset_pipeline_tests.py",
    "tests/platform/stage17_skill_stones_game_validation.cpp",
    "tests/platform/stage17_skill_stones_validator.ps1",
    "tests/platform/package_material_pack_v2_tests.py",
    "tools/package_material_pack_v2.py",
)

MATERIAL_RUNTIME_REQUIRED_FILES = (
    "assets/stage12/element_doors.png",
    "assets/stage12/element_doors_material.png",
    "assets/skills/draw_slash_atlas_material.png",
    "assets/skills/storm_swords_atlas_material.png",
    "assets/stage12/environment-props-build.json",
)

EVIDENCE_DIRECTORY = (
    "out/build/windows-msvc-debug/tests/platform/"
    "stage12 material evidence/stage12-run"
)
EVIDENCE_FILE = "stage12-material-evidence.txt"
INTEGRATION_EVIDENCE_FILE = "material-runtime-integration-evidence.txt"
UI_PREVIEW_FIELDS = (
    ("hud_ui_screenshot_1920", "ui-hud-1920x1080.png"),
    ("inventory_ui_screenshot_1920", "ui-inventory-1920x1080.png"),
    ("skill_ui_screenshot_1920", "ui-skill-stones-1920x1080.png"),
    ("pause_ui_screenshot_1920", "ui-pause-1920x1080.png"),
)
ECOLOGIES = ("fire", "water", "lightning", "chaos")
NATIVE_BACKGROUND_PREVIEW_FIELDS = tuple(
    (
        f"{ecology}_{view}_screenshot_{short_resolution}",
        f"{ecology}-{view.replace('_', '-')}-{full_resolution}.png",
    )
    for ecology in ECOLOGIES
    for view in ("background_only", "gameplay")
    for short_resolution, full_resolution in (
        ("1280", "1280x720"),
        ("1920", "1920x1080"),
    )
)
FORMAL_PREVIEW_FIELDS = UI_PREVIEW_FIELDS + NATIVE_BACKGROUND_PREVIEW_FIELDS
PREVIEW_FILES = tuple(filename for _, filename in FORMAL_PREVIEW_FIELDS)
BACKGROUND_PROVENANCE = "assets/stage12/room-background-build.json"

LEGACY_BACKGROUND_SPECS = (
    ("fire", "assets/stage12/fire_environment.png", 256, 256),
    ("water", "assets/stage12/water_environment.png", 512, 512),
    ("lightning", "assets/stage12/lightning_environment.png", 512, 512),
    ("chaos", "assets/stage12/chaos_environment.png", 512, 512),
)

GENERATED_PATHS = (
    "README_FOR_CHATGPT.md",
    "metadata/package.json",
    "metadata/asset-index.json",
    "metadata/formal-evidence.json",
    "metadata/resolution-audit.json",
    "metadata/text-rendering-root-cause.md",
    "metadata/changelog-v3.md",
)

@dataclass(frozen=True)
class PackageEntry:
    archive_path: str
    origin: str
    category: str
    source_path: Path | None = None
    data: bytes | None = None

    def size(self) -> int:
        if self.data is not None:
            return len(self.data)
        if self.source_path is None:
            raise ValueError(f"entry has no content: {self.archive_path}")
        return self.source_path.stat().st_size


@dataclass(frozen=True)
class FormalEvidenceFile:
    relative_path: str
    archive_path: str
    source_path: Path
    bytes: int
    sha256: str


@dataclass(frozen=True)
class FormalPreviewBinding:
    evidence_field: str
    filename: str
    archive_path: str
    sha256: str


@dataclass(frozen=True)
class FormalEvidenceSnapshot:
    directory: Path
    report_sha256: str
    files: tuple[FormalEvidenceFile, ...]
    previews: tuple[FormalPreviewBinding, ...]


def repo_root() -> Path:
    root = Path(__file__).resolve().parents[1]
    if not (root / ".git").exists():
        # A linked worktree has a .git file, while a normal checkout has a
        # directory. Both satisfy exists().
        raise RuntimeError(f"repository root was not found at {root}")
    return root


def normalized_archive_path(value: str) -> str:
    if "\\" in value:
        raise ValueError(f"archive paths must use forward slashes: {value}")
    path = PurePosixPath(value)
    if path.is_absolute() or not path.parts or any(
            part in ("", ".", "..") for part in path.parts):
        raise ValueError(f"unsafe archive path: {value}")
    return path.as_posix()


def path_is_reparse_point(path: Path) -> bool:
    """Return True for POSIX symlinks and Windows reparse points/junctions."""
    if path.is_symlink():
        return True
    is_junction = getattr(path, "is_junction", None)
    if is_junction is not None and is_junction():
        return True
    try:
        attributes = path.lstat().st_file_attributes
    except (AttributeError, FileNotFoundError):
        return False
    reparse_flag = getattr(stat, "FILE_ATTRIBUTE_REPARSE_POINT", 0x400)
    return bool(attributes & reparse_flag)


def reject_reparse_point(path: Path, label: str) -> None:
    if path_is_reparse_point(path):
        raise RuntimeError(
            f"{label} must not be a symbolic link or reparse point: {path}")


def reject_source_chain(root: Path, relative: str) -> Path:
    """Reject a reparse point at the root or any source path component."""
    source = root
    reject_reparse_point(source, "repository root")
    for part in PurePosixPath(relative).parts:
        source = source / part
        reject_reparse_point(source, "package source path")
    return source


def path_exists_without_following(path: Path) -> bool:
    return os.path.lexists(path)


def validate_output_destination(output: Path) -> None:
    if path_exists_without_following(output):
        raise FileExistsError(
            f"refusing to overwrite existing package: {output}")
    reject_reparse_point(output.parent, "deliverables directory")
    if not output.parent.is_dir():
        raise FileNotFoundError(
            f"deliverables directory does not exist: {output.parent}")


def require_source(root: Path, relative_path: str) -> Path:
    relative = normalized_archive_path(relative_path)
    source = reject_source_chain(root, relative)
    if not source.is_file():
        raise FileNotFoundError(f"required package input is missing: {source}")
    resolved_root = root.resolve()
    resolved_source = source.resolve()
    try:
        resolved_source.relative_to(resolved_root)
    except ValueError as error:
        raise RuntimeError(f"package input escapes repository: {source}") from error
    return source


def git_tracked_material_paths(root: Path) -> list[str]:
    command = [
        "git", "-C", str(root), "ls-files", "-z", "--",
        "assets", "art_source/stage12",
    ]
    completed = subprocess.run(
        command, check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    paths = [
        item.decode("utf-8")
        for item in completed.stdout.split(b"\0")
        if item
    ]
    allowed_prefixes = ("assets/", "art_source/stage12/")
    excluded_prefixes = (BACKGROUND_CANDIDATE_PREFIX,)
    for path in paths:
        normalized = normalized_archive_path(path)
        if normalized != path or not path.startswith(allowed_prefixes):
            raise RuntimeError(f"unexpected Git material path: {path}")
    return sorted(
        path for path in paths
        if not path.startswith(excluded_prefixes))


def material_selection_policy() -> dict[str, Any]:
    return {
        "automatic": (
            "Git-tracked files below assets/ and art_source/stage12/ only"
        ),
        "automatic_exclusions": [
            VARIABLE_FONT,
            BACKGROUND_CANDIDATE_PREFIX,
        ],
        "explicit_inclusion": STATIC_FONT,
        "untracked_candidate_art_included": False,
    }


def add_entry(entries: dict[str, PackageEntry], entry: PackageEntry) -> None:
    path = normalized_archive_path(entry.archive_path)
    if path in entries:
        raise RuntimeError(f"duplicate archive path: {path}")
    if (entry.source_path is None) == (entry.data is None):
        raise ValueError(
            f"entry must have exactly one content source: {entry.archive_path}")
    entries[path] = PackageEntry(
        archive_path=path,
        origin=entry.origin,
        category=entry.category,
        source_path=entry.source_path,
        data=entry.data,
    )


def add_source_entry(
        entries: dict[str, PackageEntry], root: Path, source_relative: str,
        archive_path: str, origin: str, category: str) -> None:
    add_entry(entries, PackageEntry(
        archive_path=archive_path,
        origin=origin,
        category=category,
        source_path=require_source(root, source_relative),
    ))


def add_generated_entry(
        entries: dict[str, PackageEntry], archive_path: str, text: str,
        category: str = "metadata") -> None:
    add_entry(entries, PackageEntry(
        archive_path=archive_path,
        origin="generated-by-packager",
        category=category,
        data=text.replace("\r\n", "\n").encode("utf-8"),
    ))


def digest_stream(stream: Any) -> str:
    digest = hashlib.sha256()
    while True:
        chunk = stream.read(BUFFER_SIZE)
        if not chunk:
            break
        digest.update(chunk)
    return digest.hexdigest()


def digest_entry(entry: PackageEntry) -> str:
    if entry.data is not None:
        return hashlib.sha256(entry.data).hexdigest()
    if entry.source_path is None:
        raise ValueError(f"entry has no content: {entry.archive_path}")
    with entry.source_path.open("rb") as stream:
        return digest_stream(stream)


def digest_path(path: Path) -> str:
    with path.open("rb") as stream:
        return digest_stream(stream)


def validate_legacy_packages(root: Path) -> None:
    for relative_path, expected_sha256 in LEGACY_PACKAGE_SHA256.items():
        path = require_source(root, relative_path)
        actual_sha256 = digest_path(path)
        if actual_sha256 != expected_sha256:
            raise RuntimeError(
                f"legacy package SHA-256 changed: {relative_path} "
                f"expected={expected_sha256} actual={actual_sha256}")


def formal_archive_path(relative_path: str) -> str:
    relative = normalized_archive_path(relative_path)
    if relative == EVIDENCE_FILE or relative in PREVIEW_FILES:
        return f"previews/{relative}"
    return f"previews/formal-evidence/{relative}"


def collect_formal_evidence(directory: Path) -> tuple[FormalEvidenceFile, ...]:
    reject_reparse_point(directory, "formal evidence directory")
    if not directory.is_dir():
        raise FileNotFoundError(
            f"formal evidence directory does not exist: {directory}")

    files: list[FormalEvidenceFile] = []

    def visit(current: Path) -> None:
        reject_reparse_point(current, "formal evidence directory")
        for child in sorted(current.iterdir(), key=lambda item: item.name):
            reject_reparse_point(child, "formal evidence source")
            relative = normalized_archive_path(
                child.relative_to(directory).as_posix())
            if child.is_dir():
                visit(child)
            elif child.is_file():
                files.append(FormalEvidenceFile(
                    relative_path=relative,
                    archive_path=formal_archive_path(relative),
                    source_path=child,
                    bytes=child.stat().st_size,
                    sha256=digest_path(child),
                ))
            else:
                raise RuntimeError(
                    f"formal evidence contains a non-regular entry: {child}")

    visit(directory)
    archive_paths = [item.archive_path for item in files]
    if len(archive_paths) != len(set(archive_paths)):
        raise RuntimeError("formal evidence maps to duplicate archive paths")
    return tuple(sorted(files, key=lambda item: item.relative_path))


def parse_evidence_report(report_path: Path) -> tuple[str, dict[str, str]]:
    report = report_path.read_text(encoding="utf-8")
    return report, parse_evidence_report_text(report)


def parse_evidence_report_text(report: str) -> dict[str, str]:
    fields: dict[str, str] = {}
    for line in report.splitlines():
        key, separator, value = line.partition("=")
        if not separator:
            continue
        if not key or key in fields:
            raise RuntimeError(f"invalid or duplicate formal evidence field: {key}")
        fields[key] = value
    return fields


def parse_evidence_report_bytes(payload: bytes) -> dict[str, str]:
    try:
        return parse_evidence_report_text(payload.decode("utf-8"))
    except UnicodeDecodeError as error:
        raise RuntimeError("formal evidence is not valid UTF-8") from error


def validate_formal_evidence(root: Path) -> FormalEvidenceSnapshot:
    """Run the real validator and capture the exact evidence files it approved."""
    validator = require_source(
        root, "tests/platform/stage12_material_validator.ps1")
    evidence_directory = root.joinpath(*PurePosixPath(EVIDENCE_DIRECTORY).parts)
    before = collect_formal_evidence(evidence_directory)
    before_by_path = {item.relative_path: item for item in before}
    report_file = before_by_path.get(EVIDENCE_FILE)
    if report_file is None:
        raise FileNotFoundError(
            f"formal evidence report is missing: {evidence_directory / EVIDENCE_FILE}")
    report, report_fields = parse_evidence_report(report_file.source_path)

    required_lines = (
        "result=pass",
        f"full_pack_bytes={FULL_PACK_BYTES}",
        f"resident_peak_bytes={RESIDENT_PEAK_BYTES}",
        f"transition_peak_bytes={TRANSITION_PEAK_BYTES}",
        "bundled_font_source_base_size=96",
        "ui_text_solid_fill=pass",
        "ui_text_physical_scale=pass",
        "hud_real_font_bounds_1920=pass",
        "inventory_real_font_bounds_1920=pass",
        "skill_real_font_bounds_1920=pass",
        "pause_real_font_bounds_1920=pass",
    ) + tuple(
        line
        for ecology in ECOLOGIES
        for line in (
            f"{ecology}_background_atlas=assets/stage12/{ecology}_room_background.png",
            f"{ecology}_background_material_atlas=assets/stage12/{ecology}_room_background_material.png",
            f"{ecology}_background_master=art_source/stage12/backgrounds/{ecology}/{ecology}-room-background-master.png",
            f"{ecology}_background_provenance={BACKGROUND_PROVENANCE}",
            f"{ecology}_background_source=2560x1440",
            f"{ecology}_background_scale_1280=1/2",
            f"{ecology}_background_scale_1920=3/4",
        )
    )
    report_lines = set(report.splitlines())
    missing = [line for line in required_lines if line not in report_lines]
    if missing:
        raise RuntimeError(
            "formal evidence is not the verified high-resolution run; missing: "
            + ", ".join(missing))

    integration_file = before_by_path.get(INTEGRATION_EVIDENCE_FILE)
    if integration_file is None:
        raise FileNotFoundError(
            "material runtime integration evidence is missing: "
            f"{evidence_directory / INTEGRATION_EVIDENCE_FILE}")
    _, integration_fields = parse_evidence_report(
        integration_file.source_path)
    expected_integration_fields = {
        "schema": "material-runtime-integration-v1",
        "fixture_path": "production-room-renderer",
        "showcase_capture_count": "0",
        "graphics_context": "pass",
        "renderer_initialized": "pass",
        "full_pack_bytes": str(FULL_PACK_BYTES),
        "resident_peak_bytes": str(RESIDENT_PEAK_BYTES),
        "transition_peak_bytes": str(TRANSITION_PEAK_BYTES),
        "observed_resident_peak_bytes": str(RESIDENT_PEAK_BYTES),
        "cross_ecology": "pass",
        "door_sprite_unique": "pass",
        "environment": "pass",
        "death": "pass",
        "draw_slash_frame_union": "0..35",
        "storm_swords_frame_union": "0..23",
        "healthy_skill_suppression": "pass",
        "missing_map_fallback": "pass",
        "stress_warmup_frames": "300",
        "stress_measured_frames": "1800",
        "screenshot_count": "70",
        "capture_prime_count": "70",
        "screenshot_pixel_guard": "pass",
        "screenshot_nonblack_failures": "0",
        "scene_sentinel_failures": "0",
        "result": "pass",
    }
    rejected_integration = [
        f"{key}={integration_fields.get(key, '<missing>')}"
        for key, expected in expected_integration_fields.items()
        if integration_fields.get(key) != expected
    ]
    if rejected_integration:
        raise RuntimeError(
            "material runtime integration evidence rejected: "
            + ", ".join(rejected_integration))

    provenance_path = require_source(root, BACKGROUND_PROVENANCE)
    try:
        provenance = json.loads(provenance_path.read_text(encoding="utf-8"))
    except (json.JSONDecodeError, UnicodeDecodeError) as error:
        raise RuntimeError("native background provenance is invalid") from error
    provenance_ecologies = provenance.get("ecologies", {})
    for ecology in ECOLOGIES:
        build = provenance_ecologies.get(ecology, {})
        outputs = build.get("output_sha256", {})
        if build.get("master_size") != [3840, 2160] or (
                build.get("runtime_size") != [2560, 1440]):
            raise RuntimeError(
                f"native background provenance dimensions disagree: {ecology}")
        for kind, suffix, relative_path, expected_dimensions in (
                ("runtime", "background_runtime_sha256",
                 f"assets/stage12/{ecology}_room_background.png", (2560, 1440)),
                ("material", "background_material_sha256",
                 f"assets/stage12/{ecology}_room_background_material.png", (2560, 1440)),
                ("master", "background_master_sha256",
                 f"art_source/stage12/backgrounds/{ecology}/"
                 f"{ecology}-room-background-master.png", (3840, 2160))):
            source = require_source(root, relative_path)
            actual = digest_path(source)
            reported = report_fields.get(f"{ecology}_{suffix}", "")
            if reported != actual or outputs.get(kind) != actual:
                raise RuntimeError(
                    f"native background SHA-256/provenance mismatch: "
                    f"{ecology}_{kind}")
            with source.open("rb") as stream:
                dimensions = png_info_from_bytes(
                    stream.read(33), relative_path)
            if (dimensions["width"], dimensions["height"]) != expected_dimensions:
                raise RuntimeError(
                    f"native background dimensions disagree: {relative_path}")

    for field, filename in FORMAL_PREVIEW_FIELDS:
        if report_fields.get(field) != filename:
            raise RuntimeError(
                f"formal preview binding mismatch: {field} must name {filename}")
        if filename not in before_by_path:
            raise FileNotFoundError(
                f"formal preview named by evidence is missing: {filename}")

    subprocess.run(
        [
            "powershell.exe", "-NoProfile", "-ExecutionPolicy", "Bypass",
            "-File", str(validator),
            "-EvidenceDirectory", str(evidence_directory),
        ],
        check=True,
        cwd=root,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )

    after = collect_formal_evidence(evidence_directory)
    if after != before:
        raise RuntimeError(
            "formal evidence changed while the PowerShell validator was running")
    bindings = tuple(
        FormalPreviewBinding(
            evidence_field=field,
            filename=filename,
            archive_path=before_by_path[filename].archive_path,
            sha256=before_by_path[filename].sha256,
        )
        for field, filename in FORMAL_PREVIEW_FIELDS
    )
    return FormalEvidenceSnapshot(
        directory=evidence_directory,
        report_sha256=report_file.sha256,
        files=before,
        previews=bindings,
    )


def formal_evidence_metadata(
        snapshot: FormalEvidenceSnapshot) -> dict[str, Any]:
    integration = next(
        (item for item in snapshot.files
         if item.relative_path == INTEGRATION_EVIDENCE_FILE), None)
    if integration is None:
        raise RuntimeError("formal snapshot lacks material runtime integration evidence")
    return {
        "schema_version": 1,
        "validator": "tests/platform/stage12_material_validator.ps1",
        "evidence_report": {
            "relative_path": EVIDENCE_FILE,
            "archive_path": f"previews/{EVIDENCE_FILE}",
            "sha256": snapshot.report_sha256,
        },
        "integration_evidence": {
            "relative_path": integration.relative_path,
            "archive_path": integration.archive_path,
            "sha256": integration.sha256,
        },
        "file_count": len(snapshot.files),
        "total_bytes": sum(item.bytes for item in snapshot.files),
        "files": [
            {
                "relative_path": item.relative_path,
                "archive_path": item.archive_path,
                "bytes": item.bytes,
                "sha256": item.sha256,
            }
            for item in snapshot.files
        ],
        "preview_bindings": [
            {
                "evidence_field": item.evidence_field,
                "filename": item.filename,
                "archive_path": item.archive_path,
                "sha256": item.sha256,
            }
            for item in snapshot.previews
        ],
    }


def png_info_from_bytes(header: bytes, label: str) -> dict[str, int]:
    if len(header) < 33 or header[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError(f"not a PNG file: {label}")
    chunk_length = struct.unpack(">I", header[8:12])[0]
    if chunk_length != 13 or header[12:16] != b"IHDR":
        raise ValueError(f"PNG has no leading IHDR chunk: {label}")
    width, height, bit_depth, color_type, compression, filtering, interlace = (
        struct.unpack(">IIBBBBB", header[16:29]))
    if width <= 0 or height <= 0:
        raise ValueError(f"PNG has invalid dimensions: {label}")
    return {
        "width": width,
        "height": height,
        "bit_depth": bit_depth,
        "color_type": color_type,
        "compression_method": compression,
        "filter_method": filtering,
        "interlace_method": interlace,
    }


def png_info(entry: PackageEntry) -> dict[str, int] | None:
    if not entry.archive_path.lower().endswith(".png"):
        return None
    if entry.data is not None:
        return png_info_from_bytes(entry.data[:33], entry.archive_path)
    if entry.source_path is None:
        raise ValueError(f"entry has no content: {entry.archive_path}")
    with entry.source_path.open("rb") as stream:
        return png_info_from_bytes(stream.read(33), entry.archive_path)


def media_type(path: str) -> str:
    suffix = PurePosixPath(path).suffix.lower()
    return {
        ".png": "image/png",
        ".otf": "font/otf",
        ".ttf": "font/ttf",
        ".wav": "audio/wav",
        ".ogg": "audio/ogg",
        ".json": "application/json",
        ".md": "text/markdown; charset=utf-8",
        ".txt": "text/plain; charset=utf-8",
        ".cpp": "text/x-c++src; charset=utf-8",
        ".hpp": "text/x-c++hdr; charset=utf-8",
        ".manifest": "application/xml; charset=utf-8",
    }.get(suffix, "application/octet-stream")


def inventory_records(entries: Iterable[PackageEntry]) -> list[dict[str, Any]]:
    records: list[dict[str, Any]] = []
    for entry in sorted(entries, key=lambda item: item.archive_path):
        record: dict[str, Any] = {
            "path": entry.archive_path,
            "origin": entry.origin,
            "category": entry.category,
            "media_type": media_type(entry.archive_path),
            "bytes": entry.size(),
            "sha256": digest_entry(entry),
        }
        dimensions = png_info(entry)
        if dimensions is not None:
            record["png"] = dimensions
        records.append(record)
    return records


def json_text(value: Any) -> str:
    return json.dumps(
        value, ensure_ascii=False, indent=2, sort_keys=True) + "\n"


def cover_scale(source_width: int, source_height: int) -> float:
    return round(max(1920 / source_width, 1080 / source_height), 4)


def resolution_audit(
        records_by_path: dict[str, dict[str, Any]]) -> dict[str, Any]:
    def required_record(
            path: str, width: int | None = None,
            height: int | None = None) -> dict[str, Any]:
        record = records_by_path.get(path)
        if record is None:
            raise RuntimeError(f"native background package input is missing: {path}")
        digest = str(record.get("sha256", ""))
        if len(digest) != 64 or any(
                character not in "0123456789abcdef" for character in digest):
            raise RuntimeError(f"invalid indexed SHA-256 for native background: {path}")
        if width is not None or height is not None:
            png = record.get("png")
            if not isinstance(png, dict) or png.get("width") != width or (
                    png.get("height") != height):
                raise RuntimeError(
                    f"native background PNG dimensions disagree: {path}")
        return record

    legacy: list[dict[str, Any]] = []
    for ecology, path, source_width, source_height in LEGACY_BACKGROUND_SPECS:
        record = records_by_path.get(path)
        if record is None or "png" not in record:
            raise RuntimeError(f"background was not indexed as PNG: {path}")
        legacy.append({
            "ecology": ecology,
            "atlas_path": path,
            "atlas_dimensions": {
                "width": record["png"]["width"],
                "height": record["png"]["height"],
            },
            "runtime_background_source_rect": {
                "x": 0,
                "y": 0,
                "width": source_width,
                "height": source_height,
            },
            "cover_scale_for_1920x1080": cover_scale(
                source_width, source_height),
            "status": "native-redraw-required",
            "fake_upscale_accepted": False,
        })

    provenance = required_record(BACKGROUND_PROVENANCE)
    audited: list[dict[str, Any]] = []
    for ecology in ECOLOGIES:
        master_path = (
            f"art_source/stage12/backgrounds/{ecology}/"
            f"{ecology}-room-background-master.png")
        runtime_path = f"assets/stage12/{ecology}_room_background.png"
        material_path = (
            f"assets/stage12/{ecology}_room_background_material.png")
        master = required_record(master_path, 3840, 2160)
        runtime = required_record(runtime_path, 2560, 1440)
        material = required_record(material_path, 2560, 1440)
        evidence: dict[str, Any] = {}
        for view_key, view_name in (
                ("background_only", "background-only"),
                ("gameplay", "gameplay")):
            for short_resolution, resolution, width, height in (
                    ("1280", "1280x720", 1280, 720),
                    ("1920", "1920x1080", 1920, 1080)):
                path = f"previews/{ecology}-{view_name}-{resolution}.png"
                record = required_record(path, width, height)
                evidence[f"{view_key}_{short_resolution}"] = {
                    "path": path,
                    "sha256": record["sha256"],
                }
        audited.append({
            "ecology": ecology,
            "status": "native-background-verified",
            "master": {
                "path": master_path,
                "dimensions": {"width": 3840, "height": 2160},
                "sha256": master["sha256"],
            },
            "runtime": {
                "path": runtime_path,
                "dimensions": {"width": 2560, "height": 1440},
                "sha256": runtime["sha256"],
            },
            "material": {
                "path": material_path,
                "dimensions": {"width": 2560, "height": 1440},
                "sha256": material["sha256"],
            },
            "provenance": {
                "path": BACKGROUND_PROVENANCE,
                "sha256": provenance["sha256"],
                "builder": "tools/build_native_room_backgrounds.py",
            },
            "runtime_background_source_rect": {
                "x": 0, "y": 0, "width": 2560, "height": 1440,
            },
            "source_to_screen_scale": {
                "1280x720": "1/2",
                "1920x1080": "3/4",
                "upscale_allowed": False,
            },
            "evidence": evidence,
        })

    master_hashes = {item["master"]["sha256"] for item in audited}
    runtime_hashes = {item["runtime"]["sha256"] for item in audited}
    material_hashes = {item["material"]["sha256"] for item in audited}
    if (len(master_hashes) != len(ECOLOGIES)
            or len(runtime_hashes) != len(ECOLOGIES)
            or len(material_hashes) != len(ECOLOGIES)):
        raise RuntimeError("cross-ecology native background hashes are not unique")

    return {
        "schema_version": 2,
        "audit_date": PACKAGE_DATE,
        "reference_viewport": {"width": 1920, "height": 1080},
        "text": {
            "status": "high-resolution-fix-verified",
            "static_font": STATIC_FONT,
            "font_source_base_pixels": 96,
            "font_display_cap_pixels": 39,
            "ui_scale_1280x720": 1.0,
            "ui_scale_1920x1080": 1.5,
            "verified_minimum_display_sizes": {
                "inventory_1280": 16,
                "inventory_1920": 24,
                "skills_1280": 18,
                "skills_1920": 27,
                "pause_1280": 18,
                "pause_1920": 27,
            },
            "formal_evidence": f"previews/{EVIDENCE_FILE}",
        },
        "legacy_room_backgrounds": legacy,
        "room_backgrounds": audited,
        "runtime_memory": {
            "full_pack_bytes": FULL_PACK_BYTES,
            "resident_peak_bytes": RESIDENT_PEAK_BYTES,
            "transition_peak_bytes": TRANSITION_PEAK_BYTES,
            "resident_hard_cap_bytes": RESIDENT_HARD_CAP_BYTES,
        },
        "room_background_conclusion": (
            "The prior low-resolution crop audit is retained verbatim under "
            "legacy_room_backgrounds. Fire, water, lightning, and chaos now use "
            "independent 3840x2160 masters and 2560x1440 runtime backgrounds, "
            "verified at downscale-only 1280x720 and 1920x1080 viewports."
        ),
        "recommended_background_delivery": {
            "master_per_ecology": "3840x2160-or-larger native artwork",
            "runtime_export_per_ecology": "2560x1440 independent background",
            "keep_gameplay_objects_separate": True,
        },
        "other_materials": (
            "Actor, skill, item, UI, and prop atlases are generally rendered "
            "at native size or downsampled. Do not blanket-upscale them without "
            "a per-sprite runtime footprint audit."
        ),
    }


def root_cause_markdown() -> str:
    return """# 文字渲染根因与当前修复

## 真正根因

1. 旧资源 `NotoSansSC[wght].ttf` 是可变字体；raylib 6.0 的 `LoadFontEx`
   不负责选择可变字体的 weight axis，默认轮廓落在很细的字重上。
2. 旧绘制为了“加粗”叠加了阴影、八方向描边、偏移重画和前景，边缘形成
   灰色/青色毛边，并非源图片分辨率不足。
3. 字形落在小字号、分数坐标并经过双线性缩放，进一步损失锐度。
4. 背包、技能页和暂停页原来按 1280×720 的固定像素布局绘制；在
   1920×1080 下相对尺寸缩小到约三分之二。部分文字还直接压在高亮纹理上。

正式截图不经过 Windows 桌面合成仍能复现旧模糊，因此不能把桌面 DPI 或
DWM 当成唯一根因。

## 已实施并纳入本包的修复

- 使用静态 `NotoSansCJKsc-Medium.otf`，不再依赖可变字重默认值。
- 字形源栅格基准提高到 96 px，显示上限为 39 px；当前正式证据中的单图集
  预算为 16 MiB，实际为 8 MiB。
- 所有关键 UI 文本走统一的 `ui_text_renderer`，坐标对齐到整数像素。
- 主文字使用纯色 RGBA `(248, 246, 238, 255)`；取消多方向描边和伪粗体，
  只保留 2 px 阴影。
- 背包、技能和暂停 UI 随视口缩放：1280×720 为 1.0，1920×1080 为 1.5。
- 技能名、取出按钮等高风险文字增加不透明深色底板，避免和高亮贴图混色。
- Windows 可执行文件嵌入 PerMonitorV2 DPI manifest。

## 证据

`previews/stage12-material-evidence.txt` 中 `result=pass`，并记录：

- `bundled_font_source_base_size=96`
- `ui_text_solid_fill=pass`
- `ui_text_physical_scale=pass`
- 四个主要 UI 的 1920 文本布局和真实字体边界全部为 `pass`

## 背景后续状态

v2 首次记录的 256×256/512×512 放大问题仍保存在
`resolution-audit.json` 的 `legacy_room_backgrounds`。v3 已用四张独立
3840×2160 master 导出 2560×1440 runtime，并在 1280×720 与
1920×1080 真实 raylib 画面中仅做 1/2、3/4 下采样验证。
"""


def changelog_markdown() -> str:
    return """# v3 变更记录

相对已有 v2 交接包，本包：

- 保留已验收的静态 Noto Sans CJK SC Medium 高分辨率文字链路；
- 新增火、水、雷、混沌四张 3840×2160 master、2560×1440 runtime 和 material；
- 新增四生态 background-only/gameplay 的 1280×720 与 1920×1080 共 16 张正式截图；
- 记录 full pack 302170112 bytes、resident peak 163708928 bytes；
- 为每个文件生成 SHA-256，并记录每张 PNG 的 IHDR 原始尺寸；
- 保留 v2 的旧放大倍率历史，并将新背景标记为 `native-background-verified`。

已有 v1/v2 ZIP 在打包前按固定 SHA-256 校验，不会被覆盖或删除。
"""


def readme_markdown(source_count: int) -> str:
    return f"""# ARPG 材质包 v3（原生高分辨率房间背景版）

这是给 ChatGPT/Codex 和美术、程序共同审查的单文件交接包，共收录
{source_count} 个源文件/证据文件，另带机器可读元数据。

## 先读这些文件

1. `metadata/package.json`：包版本、范围、入口和硬性边界。
2. `metadata/asset-index.json`：源文件 SHA-256、字节数、PNG IHDR 尺寸。
3. `metadata/formal-evidence.json`：完整正式证据文件、字段绑定和 SHA-256。
4. `metadata/text-rendering-root-cause.md`：文字长期模糊的真正原因与修复。
5. `metadata/resolution-audit.json`：旧倍率历史与四生态原生背景哈希、scale、证据。
6. `previews/`：UI 主预览、16 张背景正式截图、报告和完整正式证据树。

## 结论

- **文字已完成高分辨率修复并通过正式验收。**
- **火、水、雷、混沌四个房间背景已完成原生高分辨率重制与正式验收。**
  runtime source 固定 2560×1440，1280/1920 画面分别按 1/2、3/4 下采样。
- 其余角色、技能、道具、UI 图集不应盲目整体放大，应按实际绘制尺寸审计。

## 目录

- `assets/`：Git 已跟踪运行时素材，以及显式加入的静态 Medium 字体。
- `art_source/stage12/`：Git 已跟踪、已选用的 Stage 12 原始美术源。
- `docs/`：完整重制与 Stage 12 材质包计划。
- `integration_reference/`：文字与原生背景运行时 C++/manifest 参考实现。
- `previews/`：正式报告、UI 与四生态背景截图及完整证据树。
- `metadata/`：索引、分辨率审计、根因、变更记录。

## 完整性校验

`MANIFEST.sha256` 覆盖 ZIP 内除清单自身外的每个文件。清单不哈希自身，
因为自引用 SHA-256 没有可用的稳定定义。打包脚本完成后还会重新打开 ZIP，
逐项验证路径、固定时间戳和所有哈希。

## 给 ChatGPT 的处理规则

把本文件和 `metadata/*.json` 作为入口；只把 `asset-index.json` 中列出的文件
视为本次正式交付。不要把仓库中未跟踪的候选图当作正式素材。四生态必须继续
保持独立 4K master、2560×1440 runtime 与门、洞口、障碍物等玩法对象分层。
"""


def package_metadata(
        source_records: list[dict[str, Any]], generated_file_count: int,
        formal_snapshot: FormalEvidenceSnapshot) -> dict[str, Any]:
    category_counts: dict[str, int] = {}
    for record in source_records:
        category = str(record["category"])
        category_counts[category] = category_counts.get(category, 0) + 1
    return {
        "schema_version": 1,
        "package_format": "arpg-material-pack-chatgpt-handoff",
        "package_name": PACKAGE_NAME,
        "package_version": PACKAGE_VERSION,
        "package_date": PACKAGE_DATE,
        "language": "zh-CN",
        "source_file_count": len(source_records),
        "generated_file_count_excluding_manifest": generated_file_count,
        "total_zip_member_count_including_manifest": (
            len(source_records) + generated_file_count + 1),
        "source_category_counts": dict(sorted(category_counts.items())),
        "formal_evidence": {
            "metadata": "metadata/formal-evidence.json",
            "file_count": len(formal_snapshot.files),
            "report_sha256": formal_snapshot.report_sha256,
        },
        "scope": {
            "text_rendering": "high-resolution-fix-verified",
            "room_backgrounds": "native-background-verified",
            "room_background_ecologies": ["fire", "water", "lightning", "chaos"],
            "fake_background_upscale_accepted": False,
            "full_pack_bytes": FULL_PACK_BYTES,
            "resident_peak_bytes": RESIDENT_PEAK_BYTES,
            "transition_peak_bytes": TRANSITION_PEAK_BYTES,
            "material_runtime_integration": "formal-raylib-verified",
        },
        "font": {
            "runtime_path": STATIC_FONT,
            "style": "Noto Sans CJK SC Medium static OTF",
            "excluded_legacy_variable_font": VARIABLE_FONT,
        },
        "entrypoints": [
            "README_FOR_CHATGPT.md",
            "metadata/asset-index.json",
            "metadata/formal-evidence.json",
            "metadata/resolution-audit.json",
            "metadata/text-rendering-root-cause.md",
            f"previews/{EVIDENCE_FILE}",
        ],
        "reproducibility": {
            "zip_member_timestamp": "2026-07-23T00:00:00",
            "zip_member_permissions": "0644",
            "zip_compression": "deflate-level-9",
            "member_order": "lexicographic-posix-path",
            "json_encoding": "UTF-8 LF, sorted keys, two-space indent",
        },
        "integrity": {
            "algorithm": "SHA-256",
            "manifest": "MANIFEST.sha256",
            "manifest_excludes_itself": True,
        },
    }


def add_formal_evidence_entries(
        entries: dict[str, PackageEntry], root: Path,
        formal_snapshot: FormalEvidenceSnapshot) -> None:
    for evidence in formal_snapshot.files:
        source_relative = f"{EVIDENCE_DIRECTORY}/{evidence.relative_path}"
        add_source_entry(
            entries, root, source_relative, evidence.archive_path,
            "formal-validation-evidence", "preview-evidence")


def build_source_entries(
        root: Path,
        formal_snapshot: FormalEvidenceSnapshot) -> dict[str, PackageEntry]:
    entries: dict[str, PackageEntry] = {}
    for path in git_tracked_material_paths(root):
        if path in (VARIABLE_FONT, STATIC_FONT) or (
                path in MATERIAL_RUNTIME_REQUIRED_FILES):
            continue
        category = "runtime-assets" if path.startswith("assets/") else "art-source"
        add_source_entry(entries, root, path, path, "git-tracked", category)

    add_source_entry(
        entries, root, STATIC_FONT, STATIC_FONT,
        "explicit-reviewed-static-font", "runtime-assets")

    for path in MATERIAL_RUNTIME_REQUIRED_FILES:
        add_source_entry(
            entries, root, path, path,
            "explicit-material-runtime-contract", "runtime-assets")

    for path in PLAN_FILES:
        add_source_entry(entries, root, path, path, "explicit-plan", "documentation")

    for path in INTEGRATION_FILES:
        add_source_entry(
            entries, root, path, f"integration_reference/{path}",
            "explicit-integration-reference", "integration-reference")

    add_formal_evidence_entries(entries, root, formal_snapshot)

    if VARIABLE_FONT in entries:
        raise RuntimeError("legacy variable font must not enter the v3 archive")
    return entries


def validate_formal_inputs(
        entries: dict[str, PackageEntry],
        snapshot: FormalEvidenceSnapshot) -> None:
    evidence_entry = entries[f"previews/{EVIDENCE_FILE}"]
    if evidence_entry.source_path is None:
        raise RuntimeError("formal evidence must be a source file")
    snapshot_by_archive = {
        item.archive_path: item for item in snapshot.files
    }
    if set(snapshot_by_archive) - set(entries):
        raise RuntimeError("not all validated formal evidence entered the package")
    for archive_path, evidence in snapshot_by_archive.items():
        entry = entries[archive_path]
        if entry.source_path is None or digest_entry(entry) != evidence.sha256:
            raise RuntimeError(
                f"formal evidence changed after validation: {evidence.relative_path}")

    for binding in snapshot.previews:
        entry = entries[binding.archive_path]
        dimensions = png_info(entry)
        if "1280x720" in binding.filename:
            expected_dimensions = (1280, 720)
        elif "1920x1080" in binding.filename:
            expected_dimensions = (1920, 1080)
        else:
            raise RuntimeError(
                f"formal preview has no declared resolution: {binding.filename}")
        if dimensions is None or (
                dimensions["width"], dimensions["height"]) != expected_dimensions:
            raise RuntimeError(
                f"formal preview dimensions mismatch: {binding.filename}")


def manifest_text(entries: dict[str, PackageEntry]) -> str:
    return "".join(
        f"{digest_entry(entries[path])}  {path}\n"
        for path in sorted(entries)
    )


def zip_info(path: str) -> zipfile.ZipInfo:
    info = zipfile.ZipInfo(filename=path, date_time=ZIP_TIMESTAMP)
    info.compress_type = zipfile.ZIP_DEFLATED
    info.create_system = 3
    info.external_attr = (stat.S_IFREG | 0o644) << 16
    info._compresslevel = 9  # zipfile exposes no public per-ZipInfo setter.
    return info


def write_entry(archive: zipfile.ZipFile, entry: PackageEntry) -> None:
    info = zip_info(entry.archive_path)
    if entry.data is not None:
        archive.writestr(
            info, entry.data, compress_type=zipfile.ZIP_DEFLATED,
            compresslevel=9)
        return
    if entry.source_path is None:
        raise ValueError(f"entry has no content: {entry.archive_path}")
    with entry.source_path.open("rb") as source, archive.open(
            info, mode="w", force_zip64=True) as destination:
        shutil.copyfileobj(source, destination, length=BUFFER_SIZE)


def parse_manifest(data: bytes) -> dict[str, str]:
    result: dict[str, str] = {}
    for raw_line in data.decode("utf-8").splitlines():
        if not raw_line:
            continue
        digest, separator, path = raw_line.partition("  ")
        if separator != "  " or len(digest) != 64:
            raise RuntimeError(f"malformed manifest line: {raw_line}")
        if path in result:
            raise RuntimeError(f"duplicate manifest path: {path}")
        result[path] = digest
    return result


def verify_archive(output: Path | BinaryIO, expected_paths: list[str]) -> str:
    if hasattr(output, "seek"):
        output.seek(0)
    with zipfile.ZipFile(output, mode="r") as archive:
        infos = archive.infolist()
        names = [info.filename for info in infos]
        if names != expected_paths:
            raise RuntimeError("ZIP member order or membership is not deterministic")
        if len(names) != len(set(names)):
            raise RuntimeError("ZIP contains duplicate paths")
        for info in infos:
            if "\\" in info.filename or info.date_time != ZIP_TIMESTAMP:
                raise RuntimeError(f"invalid ZIP metadata: {info.filename}")
            if info.is_dir():
                raise RuntimeError(f"unexpected directory entry: {info.filename}")

        manifest = parse_manifest(archive.read("MANIFEST.sha256"))
        expected_manifest_paths = set(names) - {"MANIFEST.sha256"}
        if set(manifest) != expected_manifest_paths:
            raise RuntimeError("manifest coverage does not match ZIP contents")
        for path in sorted(expected_manifest_paths):
            with archive.open(path, mode="r") as stream:
                actual = digest_stream(stream)
            if actual != manifest[path]:
                raise RuntimeError(f"SHA-256 verification failed: {path}")

        metadata = json.loads(archive.read("metadata/package.json"))
        if metadata.get("package_version") != PACKAGE_VERSION:
            raise RuntimeError("package metadata version mismatch")
        scope = metadata.get("scope", {})
        if scope.get("room_backgrounds") != "native-background-verified" or (
                scope.get("full_pack_bytes") != FULL_PACK_BYTES) or (
                scope.get("resident_peak_bytes") != RESIDENT_PEAK_BYTES) or (
                scope.get("transition_peak_bytes") != TRANSITION_PEAK_BYTES) or (
                scope.get("material_runtime_integration")
                    != "formal-raylib-verified"):
            raise RuntimeError("package metadata lacks native background verification")
        required_contract_paths = set(MATERIAL_RUNTIME_REQUIRED_FILES)
        required_contract_paths.update(PLAN_FILES)
        required_contract_paths.update(
            f"integration_reference/{path}" for path in INTEGRATION_FILES)
        if not required_contract_paths.issubset(names):
            raise RuntimeError("material runtime contract files are absent")
        audit = json.loads(archive.read("metadata/resolution-audit.json"))
        legacy = audit.get("legacy_room_backgrounds")
        native = audit.get("room_backgrounds")
        if not isinstance(legacy, list) or len(legacy) != len(ECOLOGIES) or any(
                not isinstance(item, dict)
                or item.get("status") != "native-redraw-required"
                for item in legacy):
            raise RuntimeError("legacy room background audit history was not preserved")
        if not isinstance(native, list) or len(native) != len(ECOLOGIES) or any(
                not isinstance(item, dict)
                or item.get("status") != "native-background-verified"
                or item.get("runtime_background_source_rect") != {
                    "x": 0, "y": 0, "width": 2560, "height": 1440}
                or item.get("source_to_screen_scale") != {
                    "1280x720": "1/2", "1920x1080": "3/4",
                    "upscale_allowed": False}
                for item in native):
            raise RuntimeError("native room background audit is incomplete")
        if audit.get("runtime_memory") != {
                "full_pack_bytes": FULL_PACK_BYTES,
                "resident_peak_bytes": RESIDENT_PEAK_BYTES,
                "transition_peak_bytes": TRANSITION_PEAK_BYTES,
                "resident_hard_cap_bytes": RESIDENT_HARD_CAP_BYTES}:
            raise RuntimeError("native room background memory audit disagrees")

        native_by_ecology = {
            str(item.get("ecology")): item for item in native
        }
        if set(native_by_ecology) != set(ECOLOGIES):
            raise RuntimeError("native room background ecology coverage mismatch")
        master_hashes: set[str] = set()
        runtime_hashes: set[str] = set()
        material_hashes: set[str] = set()
        provenance = json.loads(archive.read(BACKGROUND_PROVENANCE))
        provenance_ecologies = provenance.get("ecologies", {})
        for ecology in ECOLOGIES:
            item = native_by_ecology[ecology]
            build = provenance_ecologies.get(ecology, {})
            outputs = build.get("output_sha256", {})
            for kind, expected_dimensions in (
                    ("master", {"width": 3840, "height": 2160}),
                    ("runtime", {"width": 2560, "height": 1440}),
                    ("material", {"width": 2560, "height": 1440})):
                binding = item.get(kind, {})
                path = normalized_archive_path(str(binding.get("path", "")))
                if path not in names or binding.get("dimensions") != expected_dimensions:
                    raise RuntimeError(
                        f"native background {kind} binding mismatch: {ecology}")
                with archive.open(path, mode="r") as stream:
                    actual_sha256 = digest_stream(stream)
                if actual_sha256 != binding.get("sha256") or (
                        actual_sha256 != outputs.get(kind)):
                    raise RuntimeError(
                        f"native background {kind} SHA-256 mismatch: {ecology}")
                if kind == "master":
                    master_hashes.add(actual_sha256)
                elif kind == "runtime":
                    runtime_hashes.add(actual_sha256)
                elif kind == "material":
                    material_hashes.add(actual_sha256)
            evidence = item.get("evidence", {})
            if not isinstance(evidence, dict) or set(evidence) != {
                    "background_only_1280", "background_only_1920",
                    "gameplay_1280", "gameplay_1920"}:
                raise RuntimeError(
                    f"native background evidence coverage mismatch: {ecology}")
            for binding in evidence.values():
                path = normalized_archive_path(str(binding.get("path", "")))
                if path not in names:
                    raise RuntimeError(
                        f"native background evidence path is absent: {path}")
                with archive.open(path, mode="r") as stream:
                    if digest_stream(stream) != binding.get("sha256"):
                        raise RuntimeError(
                            f"native background evidence SHA-256 mismatch: {path}")
        if (len(master_hashes) != len(ECOLOGIES)
                or len(runtime_hashes) != len(ECOLOGIES)
                or len(material_hashes) != len(ECOLOGIES)):
            raise RuntimeError("cross-ecology native backgrounds are duplicated")

        formal = json.loads(archive.read("metadata/formal-evidence.json"))
        formal_files = formal.get("files")
        if not isinstance(formal_files, list) or (
                formal.get("file_count") != len(formal_files)):
            raise RuntimeError("formal evidence metadata file count mismatch")
        formal_by_archive: dict[str, dict[str, Any]] = {}
        for record in formal_files:
            if not isinstance(record, dict):
                raise RuntimeError("malformed formal evidence metadata record")
            archive_path = normalized_archive_path(str(record.get("archive_path", "")))
            if archive_path in formal_by_archive or archive_path not in names:
                raise RuntimeError(
                    f"formal evidence metadata path mismatch: {archive_path}")
            if archive.getinfo(archive_path).file_size != record.get("bytes"):
                raise RuntimeError(
                    f"formal evidence byte count mismatch: {archive_path}")
            with archive.open(archive_path, mode="r") as stream:
                actual = digest_stream(stream)
            if actual != record.get("sha256"):
                raise RuntimeError(
                    f"formal evidence SHA-256 mismatch: {archive_path}")
            formal_by_archive[archive_path] = record

        expected_formal_paths = {
            name for name in names if name.startswith("previews/")
        }
        if set(formal_by_archive) != expected_formal_paths:
            raise RuntimeError(
                "formal evidence metadata coverage does not match previews")
        if formal.get("total_bytes") != sum(
                int(record["bytes"]) for record in formal_files):
            raise RuntimeError("formal evidence metadata byte total mismatch")

        report_metadata = formal.get("evidence_report")
        report_archive_path = f"previews/{EVIDENCE_FILE}"
        report_record = formal_by_archive.get(report_archive_path)
        if not isinstance(report_metadata, dict) or report_record is None or (
                report_metadata.get("relative_path") != EVIDENCE_FILE
                or report_metadata.get("archive_path") != report_archive_path
                or report_metadata.get("sha256") != report_record.get("sha256")):
            raise RuntimeError("formal evidence report SHA-256 binding mismatch")

        integration_metadata = formal.get("integration_evidence")
        integration_archive_path = (
            f"previews/formal-evidence/{INTEGRATION_EVIDENCE_FILE}")
        integration_record = formal_by_archive.get(integration_archive_path)
        if not isinstance(integration_metadata, dict) or (
                integration_record is None) or (
                integration_metadata.get("relative_path")
                    != INTEGRATION_EVIDENCE_FILE) or (
                integration_metadata.get("archive_path")
                    != integration_archive_path) or (
                integration_metadata.get("sha256")
                    != integration_record.get("sha256")):
            raise RuntimeError(
                "material runtime integration evidence SHA-256 binding mismatch")
        integration_fields = parse_evidence_report_bytes(
            archive.read(integration_archive_path))
        for key, expected in {
                "fixture_path": "production-room-renderer",
                "showcase_capture_count": "0",
                "full_pack_bytes": str(FULL_PACK_BYTES),
                "resident_peak_bytes": str(RESIDENT_PEAK_BYTES),
                "transition_peak_bytes": str(TRANSITION_PEAK_BYTES),
                "screenshot_pixel_guard": "pass",
                "result": "pass",
        }.items():
            if integration_fields.get(key) != expected:
                raise RuntimeError(
                    f"material runtime integration archive field mismatch: {key}")

        bindings = formal.get("preview_bindings")
        if not isinstance(bindings, list) or len(bindings) != len(
                FORMAL_PREVIEW_FIELDS):
            raise RuntimeError("formal preview binding count mismatch")
        bindings_by_field = {
            item.get("evidence_field"): item
            for item in bindings if isinstance(item, dict)
        }
        for field, filename in FORMAL_PREVIEW_FIELDS:
            binding = bindings_by_field.get(field)
            archive_path = f"previews/{filename}"
            if binding is None or (
                    binding.get("filename") != filename
                    or binding.get("archive_path") != archive_path):
                raise RuntimeError(
                    f"formal preview binding mismatch in archive: {field}")
            formal_record = formal_by_archive.get(archive_path)
            if formal_record is None or (
                    binding.get("sha256") != formal_record.get("sha256")):
                raise RuntimeError(
                    f"formal preview SHA-256 binding mismatch: {field}")

    if isinstance(output, Path):
        with output.open("rb") as stream:
            return digest_stream(stream)
    output.flush()
    output.seek(0)
    result = digest_stream(output)
    output.seek(0)
    return result


def file_identity(metadata: os.stat_result) -> tuple[int, int]:
    return metadata.st_dev, metadata.st_ino


def unlink_owned_path(
        path: Path, expected_identity: tuple[int, int],
        attempts: int = 2) -> bool:
    """Delete only the inode created by this run; never unlink a replacement."""
    for attempt in range(attempts):
        if not path_exists_without_following(path):
            return True
        current = path.lstat()
        if file_identity(current) != expected_identity:
            return False
        try:
            path.unlink()
            return True
        except OSError:
            if attempt + 1 == attempts:
                raise
    return False


def digest_binary_stream(stream: BinaryIO) -> str:
    stream.flush()
    stream.seek(0)
    digest = digest_stream(stream)
    stream.seek(0)
    return digest


def atomic_publish_verified(
        output: Path,
        build_and_verify: Callable[[BinaryIO], str]) -> str:
    """Build through one pinned handle, verify, then publish without overwrite."""
    validate_output_destination(output)
    descriptor, temporary_name = tempfile.mkstemp(
        dir=output.parent,
        prefix=f".{output.name}.",
        suffix=".tmp",
    )
    temporary = Path(temporary_name)
    initial_identity = file_identity(os.fstat(descriptor))
    temporary_owned = True
    output_owned = False
    try:
        with os.fdopen(descriptor, "w+b") as stream:
            expected_sha256 = build_and_verify(stream)
            stream.flush()
            os.fsync(stream.fileno())
            pinned = os.fstat(stream.fileno())
            if file_identity(pinned) != initial_identity:
                raise RuntimeError("temporary package handle identity changed")
            reject_reparse_point(temporary, "temporary package")
            if not temporary.is_file():
                raise RuntimeError(
                    f"verified temporary package is not a regular file: {temporary}")
            candidate = temporary.lstat()
            if file_identity(candidate) != initial_identity:
                temporary_owned = False
                raise RuntimeError("temporary package path was replaced")
            if candidate.st_size != pinned.st_size:
                raise RuntimeError("temporary package size changed after verification")
            if not isinstance(expected_sha256, str) or len(expected_sha256) != 64:
                raise RuntimeError("candidate verification returned no SHA-256")
            if digest_binary_stream(stream) != expected_sha256:
                raise RuntimeError("temporary package changed after verification")

            # Recheck immediately before the atomic, no-replace link. os.link()
            # fails when the final name appeared in a race and never overwrites it.
            validate_output_destination(output)
            os.link(temporary, output)
            final_metadata = output.lstat()
            if file_identity(final_metadata) != initial_identity:
                raise RuntimeError("published package identity mismatch")
            output_owned = True
            if digest_path(output) != expected_sha256:
                raise RuntimeError("published package changed after verification")
    except BaseException:
        # The stream is closed here. Roll back only paths that still name the
        # inode created by this run; never delete a concurrent replacement.
        if output_owned:
            unlink_owned_path(output, initial_identity)
        if temporary_owned:
            unlink_owned_path(temporary, initial_identity)
        raise

    # A transient Windows sharing violation during hard-link cleanup must not
    # turn a successful publication into a false failure. Two identity-checked
    # attempts cover the close-to-unlink transition without touching outsiders.
    try:
        cleaned = unlink_owned_path(temporary, initial_identity)
    except OSError:
        if output_owned:
            unlink_owned_path(output, initial_identity)
        raise
    if not cleaned:
        if output_owned:
            unlink_owned_path(output, initial_identity)
        raise RuntimeError("temporary package path was replaced before cleanup")
    return expected_sha256


def build_package() -> Path:
    root = repo_root()
    output = root / "deliverables" / PACKAGE_NAME
    validate_output_destination(output)
    validate_legacy_packages(root)

    formal_snapshot = validate_formal_evidence(root)
    entries = build_source_entries(root, formal_snapshot)
    validate_formal_inputs(entries, formal_snapshot)
    source_records = inventory_records(entries.values())
    records_by_path = {record["path"]: record for record in source_records}

    audit = resolution_audit(records_by_path)
    asset_index = {
        "schema_version": 1,
        "package_version": PACKAGE_VERSION,
        "selection_policy": material_selection_policy(),
        "item_count": len(source_records),
        "items": source_records,
    }

    add_generated_entry(
        entries, "metadata/resolution-audit.json", json_text(audit))
    add_generated_entry(
        entries, "metadata/text-rendering-root-cause.md",
        root_cause_markdown())
    add_generated_entry(
        entries, "metadata/changelog-v3.md", changelog_markdown())
    add_generated_entry(
        entries, "metadata/asset-index.json", json_text(asset_index))
    add_generated_entry(
        entries, "metadata/formal-evidence.json",
        json_text(formal_evidence_metadata(formal_snapshot)))
    add_generated_entry(
        entries, "metadata/package.json",
        json_text(package_metadata(
            source_records, len(GENERATED_PATHS), formal_snapshot)))
    add_generated_entry(
        entries, "README_FOR_CHATGPT.md", readme_markdown(len(source_records)),
        category="readme")

    if set(GENERATED_PATHS) - set(entries):
        missing = sorted(set(GENERATED_PATHS) - set(entries))
        raise RuntimeError(f"generated package files are missing: {missing}")

    add_generated_entry(
        entries, "MANIFEST.sha256", manifest_text(entries),
        category="integrity")
    expected_paths = sorted(entries)

    def write_and_verify(candidate: BinaryIO) -> str:
        # Re-hash the evidence after metadata/manifest construction. If any
        # source changed since the PowerShell validator approved it, abort
        # before publishing a package.
        validate_formal_inputs(entries, formal_snapshot)
        candidate.seek(0)
        candidate.truncate(0)
        with zipfile.ZipFile(
                candidate, mode="w", compression=zipfile.ZIP_DEFLATED,
                compresslevel=9, allowZip64=True) as archive:
            for path in expected_paths:
                write_entry(archive, entries[path])
        candidate.flush()
        return verify_archive(candidate, expected_paths)

    archive_sha256 = atomic_publish_verified(output, write_and_verify)
    print(f"created={output}")
    print(f"members={len(expected_paths)}")
    print(f"sha256={archive_sha256}")
    return output


def main() -> int:
    try:
        build_package()
    except (OSError, RuntimeError, ValueError, subprocess.CalledProcessError,
            zipfile.BadZipFile) as error:
        print(f"package failed: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
