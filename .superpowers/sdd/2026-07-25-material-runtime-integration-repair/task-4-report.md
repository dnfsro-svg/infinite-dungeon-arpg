# Task 4 Report

## Outcome

- Added phase-aware monster animation states: idle, move, telegraph, active,
  recovery, cooldown, hurt, and death.
- Added fixed-duration explicit fire-atlas frames using the committed 256x256
  cells and foot anchors.
- Registered all 12 bomber cells and only the 11 opaque charger cells; charger
  cell 7 is never referenced.
- Removed legacy generic monster clips and enabled fire monsters in the fixed
  capacity presenter.
- Kept the six 96x96 ecology monster atlases on their existing authored ranges,
  while giving each AI phase an independent animation state/clock.
- Material-frame plans now bypass the legacy static-sprite path in the renderer.

## TDD Evidence

RED command:

```powershell
cmake --build --preset windows-msvc-debug --target arpg_platform_tests --parallel 1
ctest --test-dir out/build/windows-msvc-debug -R '^platform\.units$' --output-on-failure -j 1
```

RED result: build succeeded; 455 cases ran with 3 expected failures. The first
failure was `material_animation.exposes every drawable phase clip for every
monster` at `clip != nullptr`, caused by absent fire clips.

GREEN result after implementation: build exited 0; `platform.units` passed
455/455 cases with 0 failures in 9.97 seconds.

## Scope Check

Exactly three C++ cases were added and the platform expected case count changed
from 452 to 455. No AI, gameplay, atlas dimensions, or asset files were changed,
and the game was not launched.

## Review Round 1

Two Important findings were repaired with regression-first coverage in the
existing Task 4 cases; no fourth case was registered.

- Sparse-residency RED: a fire-room snapshot with `monster_count=1`, inactive
  slot 0, and an active chaos chaser in the highest fixed slot failed at
  `residency.contains(MaterialAtlasId::chaos_chaser)`. The residency collector
  now scans every fixed slot and requests only active monsters.
- Scale/renderer RED: the strengthened actor-render case initially failed to
  compile because the atlas-aware scale and renderer decision seam did not
  exist. Fire bomber/charger now use `78/176` times projection scale, while all
  six 96x96 ecology atlases retain `0.92` times projection scale.
- The renderer calls the tested allocation-free decision seam. A successful
  material draw selects `material`; an unavailable material frame with a
  material plan selects `silhouette`. Legacy material sprites remain available
  only for plans that do not request an animation frame.

Round 1 GREEN: final serial build exited 0 and `platform.units` passed 455/455
cases with 0 failures in 9.71 seconds. No game process was launched.
