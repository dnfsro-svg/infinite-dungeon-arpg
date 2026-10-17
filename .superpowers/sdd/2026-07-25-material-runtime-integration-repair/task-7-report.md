# Task 7 Report: Lay out isolated room props safely

## Outcome

- Added a pure fixed-capacity `EnvironmentPropLayout` with at most nine slots;
  water, lightning, and chaos each produce exactly five decorative placements.
- Used the shared normalized foot positions and clamped them against projected
  alpha bounds with an 8-pixel safety inset and a bottom HUD reservation of
  `max(72, 0.12 * height)`.
- Kept gameplay hole sprites out of decorative layouts.
- Copied generated alpha bounds and foot anchors into constexpr metadata owned
  by the three ecology slices. Runtime sources do not read JSON.
- Replaced the three ecology-specific prop draw loops with one shared loop.
- Split background, prop, and gameplay-hole fallback behavior. A missing prop
  draws only a low-alpha dark-steel outline over its projected alpha bounds.
- Registered exactly three resolution-layout cases and changed the platform
  expected case count from 462 to 465.

## RED evidence

- The first shell attempt stopped before the intended API failure because the
  MSVC standard-library include environment was not loaded (`cmath` missing).
- Re-running under `VsDevCmd.bat -arch=x64` produced the intended RED:
  `ecology_material_coverage_tests.cpp(4): fatal error C1083`, missing
  `environment_prop_layout.hpp`.

## Layout evidence

- Each ecology case iterates 800x450, 1280x720, and 1920x1080.
- Every projected alpha bbox remains inside the 8-pixel viewport inset and
  above the reserved HUD band.
- Every pair overlaps by no more than 15 percent of the smaller bbox.
- Metadata provenance assertions match runtime constexpr alpha bounds and
  anchors to all 15 generated JSON records, including the isolated holes.

## Corrections during GREEN

- The first platform run reported one water top-edge clamp failure caused by
  exact-boundary floating-point rounding, plus two obsolete atomic-gate source
  assertions.
- Clamp limits now use direct alpha-bound offsets with a small internal margin,
  and the source contract asserts independent background, prop, and hole paths.

## Final verification

- `cmake --build --preset windows-msvc-debug --target arpg_platform_tests --parallel 1`:
  exit 0 under the Visual Studio developer environment.
- `ctest --test-dir out/build/windows-msvc-debug -R '^platform\.units$' --output-on-failure -j 1`:
  1/1 passed; platform suite reports 465 cases.
- `ctest --test-dir out/build/windows-msvc-debug -R '^stage12\.environment_prop_asset_pipeline$' --output-on-failure -j 1`:
  1/1 passed.
- The game executable was not launched.

## Review round 1

- Strengthened the same three ecology cases, without adding a fourth case, to
  project the real gameplay-hole bbox from shared `kHoleCenter`,
  `project_render_world`, ecology hole metadata, and the production 0.77 scale.
- RED confirmed 3/3 colliding resolutions for each ecology: nine collisions
  across the nine ecology/resolution combinations.
- Added a pure gameplay-hole exclusion pass that considers only the fifth
  center placement. It sorts the four axis-aligned separation translations by
  distance and accepts the first candidate that stays inside viewport/HUD,
  has zero hole intersection, and preserves the 15 percent decor-overlap cap.
- The roughly 130 added layout-source lines implement that fixed-capacity
  candidate set plus shared bbox, viewport/HUD, and 15 percent overlap checks;
  the path uses only stack values and fixed arrays, with no dynamic allocation.
- Centralized the unchanged gameplay-hole scale in the layout header so the
  renderer, layout, and tests cannot drift; hole world position, collision,
  scale value, and gameplay behavior were not changed.
- Fresh serial build, `platform.units`, and environment-prop asset-pipeline
  verification all passed after the review fix.
