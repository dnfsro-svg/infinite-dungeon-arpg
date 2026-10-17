# Stage 11-C Task 8 Report

## Scope

Only Task 8 test and architecture-gate files were changed. No production HUD,
formal raylib evidence, Task 9 screenshot path, or Task 10 documentation gate
was modified.

## TDD RED

The new stress target and the three required CTest names were registered before
their source/guard files existed. Running the repository build under the
required MSVC 19.44 / Windows SDK 10.0.26100.0 environment failed with the
expected missing implementation:

```text
CMake Error at tests/platform/CMakeLists.txt:40 (add_executable):
  Cannot find source file:
    hud_stress_tests.cpp

CMake Error at tests/platform/CMakeLists.txt:40 (add_executable):
  No SOURCES given to target: arpg_stage11c_hud_stress
```

This was a missing Task 8 source RED, not a syntax or unrelated test failure.

## Stress Coverage

`arpg_stage11c_hud_stress` visits exactly 100,000 iterations across five fixed
production-shaped snapshots, 20,000 visits each:

1. unchanged previous/current snapshot;
2. low-health snapshot at 1024x576;
3. slow/corrosion/invulnerable status-heavy snapshot with debug layout;
4. uint64/uint32 maximum counter snapshot at 1920x1080;
5. priority-overflow snapshot that drops exactly one bounded notice per fresh
   iteration.

Every measured iteration covers `build_hud_view_model`, notice
`observe/update/view`, notice attachment, responsive layout, player/monster/
objective/navigation/context/text pure draw plans, and the pure font-selection
plan. No raylib window, GPU draw, font load, or screenshot call is included.

The allocation probe and immutable-input evidence were:

```text
[stage11c-hud-stress] iterations=100000 scenarios=5 allocations=0 input_hash=0x97d051f59c2f0cef output=0xa59ea490a113aea3 result=pass
```

Running the executable without its CTest-owned environment failed explicitly:

```text
[stage11c-hud-stress-fail] requires ARPG_STAGE11C_HUD_STRESS=1
```

CTest owns `ARPG_STAGE11C_HUD_STRESS=1` and a 300-second timeout.

## Mutation Matrix

The self-test creates a fresh directory per mutation, copies every real
production `hud_*` source/header into it, changes one copied production file,
invokes the main guard, and verifies the named rejection reason.

| Mutation | Production copy | Required named rejection |
| --- | --- | --- |
| `GetKeyPressed()` | `hud_renderer.cpp` | physical input sampling |
| `DungeonSession*` | `hud_view_model.cpp` | DungeonSession access |
| `runtime.fixed_tick({})` | `hud_notice_state.cpp` | fixed tick access |
| `SettingsStore*` | `hud_view_model.cpp` | SettingsStore access |
| `std::string` | `hud_renderer.cpp` | dynamic std::string |
| `std::vector<int>` | `hud_notice_state.cpp` | dynamic std::vector |
| normal `"Budget"` | `hud_renderer.cpp` | legacy Budget text |
| fourth `append_status_tag` | `hud_view_model.cpp` | fourth visible status tag |

The bad-source file is only the checked mutation manifest; acceptance/rejection
is determined by the main guard against the independently copied production
source trees, not by matching the fixture alone.

## GREEN Verification

```powershell
. .\scripts\Configure.ps1 -Preset windows-msvc-debug
cmake --build --preset windows-msvc-debug --target arpg_stage11c_hud_stress arpg_platform_tests
```

Result: exit 0, `ninja: no work to do` after the successful build.

```powershell
ctest --test-dir out/build/windows-msvc-debug -R "^stage11c\.(hud_stress|architecture)" --output-on-failure
```

Result: 3/3 passed; stress 0.55s, main guard 0.02s, mutation self-test 0.18s.

```powershell
ctest --test-dir out/build/windows-msvc-debug -R "^(platform.units|combat.units|dungeon.units)$" --output-on-failure
```

Result: 3/3 passed; combat 0.89s, dungeon 185.88s, platform 3.13s.

```powershell
git diff --check
```

Result: exit 0; only the existing Git LF-to-CRLF advisory was printed.

## Self-review and Concerns

- All changed/tracked files are under `tests/platform`; production sources are
  only read and copied by the self-test.
- The main guard intentionally excludes `debug_overlay_renderer.cpp`, so the
  F1-only production `Budget` diagnostic remains allowed while all normal
  `hud_*` sources reject it.
- The stress hash covers the complete object representation of both snapshots,
  render status, committed hints and monster input before and after the loop.
- The measured loop is CPU-only and reports zero allocations.
- No unresolved functional concern. The source guard is intentionally lexical;
  it is a fast boundary regression gate, not a C++ parser.

## Commit

`c5bc56e` — `test: stress stage 11c hud boundaries`

## Review Fix — Expanded Pure Paths and HUD Boundary Ownership

### Review RED

Before expanding the main guard, the self-test was changed to copy the real
`combat_renderer.cpp` and inject `GetKeyPressed()` there. The existing guard
failed for the intended reason:

```text
Stage11C HUD guard accepted get_key_pressed production-source mutation
```

This proved that the old `hud_*`-only scan did not cover the reviewed boundary.

### Stress Corrections

- Calls the real `make_debug_overlay_diagnostics_plan` on every cold and steady
  pass. The status-heavy/F1 scenario owns a real combat event plus budget,
  monster/projectile/hazard, saturation, invalid-owner, ground-saturation,
  room-index-overflow, binding, CJK and HUD diagnostic values. These fields are
  individually validated and folded into the output checksum.
- Keeps one `HudNoticeState` per scenario across the complete run. The unchanged
  state receives its baseline observation before the steady counter; each of
  its 20,000 same-revision refreshes proves the primary/secondary view and drop
  count were not requeued or reset before time advances.
- The persistent overflow state alternates two prebuilt room/commit/XP/level
  event snapshots. Each visit changes the event identity and must increment the
  real bounded-queue drop count exactly once. Final drops equal the cold pass
  plus all 20,000 steady overflow visits.
- Takes a cold allocation baseline before any HUD pure function, runs all five
  scenarios through every pure path, and requires zero allocations. A separate
  steady baseline requires zero allocations across the complete 100,000 loop.

Fresh direct evidence:

```text
[stage11c-hud-stress] iterations=100000 scenarios=5 cold_allocations=0 steady_allocations=0 input_hash=0x7268e6897c851557 output=0x89baf09448bc5420 result=pass
```

The no-environment negative path still exits nonzero with:

```text
[stage11c-hud-stress-fail] requires ARPG_STAGE11C_HUD_STRESS=1
```

### Expanded Guard

The six general prohibitions now scan all normal `hud_*` files plus
`combat_renderer.hpp/.cpp` and `debug_overlay_renderer.hpp/.cpp`. `Budget` is
separate: it scans normal `hud_*` and `combat_renderer` only, intentionally
excluding the F1 debug overlay.

The host is not scanned as a whole. The guard extracts the exact recovery HUD
observation statement and the normal/death observation-to-renderer-presentation
block, validates their ownership tokens, and applies the six general
prohibitions only to those seams.

The eight self mutations now copy all real production files and are distributed
across combat renderer, debug overlay, both host seams and HUD core. Every
mutation invokes the same main guard and checks its exact named reason.

Focused review-fix verification:

```text
stage11c.hud_stress.zero_alloc_100k               Passed 0.53 sec
stage11c.architecture.hud_boundaries              Passed 0.02 sec
stage11c.architecture.hud_boundaries_self_test    Passed 0.19 sec
3/3 passed
```

Required regressions after the review fix:

```text
combat.units      Passed   0.86 sec
dungeon.units     Passed 179.26 sec
platform.units    Passed   3.17 sec
3/3 passed
```

Review-fix commit: `6b1268a` —
`test: harden stage 11c hud stress guards`.

## Final Review Fix — Host Seam Budget Boundary

The remaining Important finding was reproduced before changing the main guard.
The existing Budget mutation was moved from `combat_renderer.cpp` into the real
normal host HUD seam immediately before the production `renderer.draw` call.
The old guard failed with:

```text
Stage11C HUD guard accepted legacy_budget production-source mutation
```

The main guard now applies the same named `Normal HUD rejects legacy Budget
text` rule to both extracted host HUD seams. It still does not apply that rule
to `debug_overlay_renderer.hpp/.cpp`, so the production F1-only Budget line is
explicitly legal and the main guard passes against the real source tree.

Fresh verification:

```text
stage11c.hud_stress.zero_alloc_100k               Passed 0.53 sec
stage11c.architecture.hud_boundaries              Passed 0.03 sec
stage11c.architecture.hud_boundaries_self_test    Passed 0.20 sec
3/3 focused passed

platform.units                 Passed 3.13 sec
platform.input_latency_source  Passed 6.03 sec
platform.host_input_source     Passed 0.03 sec
3/3 related regressions passed
```

Final review-fix commit: `22d2c1b` —
`test: guard host hud budget text`.
