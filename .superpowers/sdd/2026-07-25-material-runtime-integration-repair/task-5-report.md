# Task 5 Report: Material-backed active skill atlases

## Outcome

- Added deterministic offline material-map generation for draw slash and storm swords.
- Registered paired color/material atlases as `MaterialAtlasId::skill_draw_slash`
  and `MaterialAtlasId::skill_storm_swords`.
- Replaced runtime-owned active-skill textures with metadata-only frame lookup.
- Routed active-skill atlas drawing through `MaterialPack::draw_frame`.
- Made active-skill residency follow equipped non-`none` loadout slots.
- Registered exactly three new C++ cases and changed the platform count from
  455 to 458.

## RED evidence

- `python -m unittest tests.platform.active_skill_material_asset_pipeline_tests -v`
  failed because the generator and both material PNGs did not exist.
- The first serial C++ build failed on the missing skill material atlas IDs and
  metadata API, after entering the Visual Studio developer environment.

## Deterministic asset evidence

- Python pipeline: 2/2 passed.
- Draw slash SHA-256:
  `1D11E064A3A2C3781632688C1870A25596EDCCB4B1A6101C4897442D13035ED4`
- Storm swords SHA-256:
  `E43E5DD0EC98AD7F2EFF84BD74FB5C1F42A6C5830842FF541BC13C7D37F28690`
- Generator output is written to a same-directory temporary PNG, reopened to
  validate dimensions and alpha parity, then atomically replaced.
- Pillow and pixel parsing exist only in the offline tool/test path, never in
  runtime C++.

## Residency and ecology self-review

Both skill atlases use `MaterialEcology::skill`, not
`MaterialEcology::common`. `MaterialPack::load(fire|water|lightning|chaos)`
selects only common atlases plus the requested room ecology, so it does not
load skill atlases. The production draw path synchronizes
`make_material_residency_request(current)`, which adds a skill atlas only when
the corresponding loadout slot contains a non-`none` active skill. Tests prove
draw-slash-only selectivity and both-atlas residency under 256 MiB.

## Runtime ownership audit

The active-skill asset and renderer files contain no class-owned `Texture2D`,
`LoadTexture`, `UnloadTexture`, or `DrawTexturePro`. Combat renderer no longer
initializes, shuts down, or warns about a separate active-skill texture pack.
`active_skill_assets_ready()` now validates manifest and frame metadata only.

## Scope note

The brief's Files list omitted
`tests/platform/room_background_render_plan_tests.cpp`. Adding two atlas IDs
necessarily changes `MaterialAtlasId::count` from 27 to 29, so the authorized
scope extension changes only that historical static assertion. It also omitted
`src/platform/raylib/material_asset_validation.cpp`; the authorized two-line
extension adds both equipped skill atlases to the legal worst residency request,
raising the verified peak from 201,637,888 to 226,800,928 bytes without changing
the 256 MiB limit or other validation logic.

## Final verification

- `python tools/build_active_skill_material_maps.py --root .`: exit 0.
- `python -m unittest tests.platform.active_skill_material_asset_pipeline_tests -v`:
  2/2 passed.
- `cmake --build --preset windows-msvc-debug --target arpg_platform_tests --parallel 1`:
  exit 0.
- `ctest --test-dir out/build/windows-msvc-debug -R '^platform\.units$' --output-on-failure -j 1`:
  1/1 passed.
- Direct `arpg_platform_tests.exe`: `458 cases, 0 failures`.
- `ctest --test-dir out/build/windows-msvc-debug -R '^stage17\.active_skill_material_asset_pipeline$' --output-on-failure -j 1`:
  1/1 passed.
- Python `__pycache__` removed after verification.

## Review

The read-only review initially found that the legal worst residency peak did
not include both equipped skills. After the authorized validation change and
expanded worst-case test, re-review reported no Critical, Important, or Minor
issues and marked the task Ready.
