# Task 3 Report: Directional Element Doors

## RED

- Added exactly three cases: directional sprite/mode decisions, directional arrow geometry, and independent `element_doors` atlas/background gating.
- Updated the platform expected-case count from 449 to 452.
- Ran the required serial target build after loading the Visual Studio developer environment. The first relevant failure was the intended missing API: `DoorRenderDecision::sprite`, `body_tint`, and `draw_lock_marker`, plus `door_arrow_geometry`.
- The initial shell did not have the MSVC developer environment loaded and failed on the pre-existing `<cmath>` include. Re-running the same build command after `VsDevCmd.bat` isolated the expected RED failure above.

## GREEN

- Implemented directional sprite decisions, closed/open tint and lock state, pure three-segment arrow geometry, and per-door material fallback rendering.
- Removed door atlas readiness from the material room-background gate.
- Added the narrowly authorized atlas routing correction in `material_pack.cpp`: all four `environment_door_*` sprites now resolve to `MaterialAtlasId::element_doors`.
- Serial build passed:

  `cmake --build --preset windows-msvc-debug --target arpg_platform_tests --parallel 1`

- Serial test passed:

  `ctest --test-dir out/build/windows-msvc-debug -R '^platform\.units$' --output-on-failure -j 1`

  Result: 1/1 test target passed, 452 cases.

## Commit

- `9b35f07 fix: render directional element doors`

## Concerns

- The Task 3 Files list omitted `src/platform/raylib/material_pack.cpp`, although its existing atlas routing left the four door sprites tied to the old ecology atlases. Parent authorization added only the four required routing cases; no other routing or loading behavior changed.
- The report is intentionally ignored by the repository's `.superpowers/` exclude rule.
