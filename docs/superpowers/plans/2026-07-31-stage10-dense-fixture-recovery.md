# Stage 10 Dense-Room Fixture Recovery Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development to implement this plan task-by-task.

**Goal:** Restore the Stage 10 standalone validation fixture after the hundredfold-room population and spatial-streaming changes by equipping its deterministic test run with a valid endgame build and making its legal input driver traverse the whole streamed room, without changing gameplay or the fixture's time budget.

**Architecture:** Keep the production dense-room contract unchanged. Change only fixture-owned equipment and legal player input selection: deterministic five-lane scouting reveals nonresident cells, then the two already-equipped production active skills are attempted before the existing light-attack fallback. The fixture must still clear the real depth-99 abyss room through production movement, skills, collision, damage, and defeat handling, then exercise the existing reward, abandon, and environment assertions.

**Tech Stack:** C++17, raylib 6.0 project, CMake 3.25+, Ninja, MSVC 19.44, CTest.

## Global Constraints

- Work only in `E:/game/.worktrees/task9-gate-recovery` on `codex/task9-gate-recovery`.
- Keep `CMAKE_BUILD_PARALLEL_LEVEL=1` and every build/test serial with `-j1`.
- Modify only `tests/dungeon/stage10_validation_fixture.cpp` plus this plan.
- Do not change production source, gameplay rules, monster density, room bounds, golden values, iteration budgets, CTest timeouts, fixture assertions, or B-G scope.
- Do not import the unrelated diagnostics, persistence-flow edits, exit navigation changes, or environment-probe changes from historical commit `d57c2e8`.
- Never inject a defeat, mutate monster HP/remaining targets directly, call a test defeat helper, or synthesize a combat event.
- Do not delete files, reset Git, or touch any other worktree.
- Use the existing failing selector as RED and record real exit codes and elapsed times.
- Do not run the full CTest suite.

---

### Task 1: Drive the Stage 10 fixture through dense streamed rooms

**Files:**
- Modify: `tests/dungeon/stage10_validation_fixture.cpp`
- Test: existing `stage10.validation_fixture.real_abyss_transactions`

**Interfaces:**
- Consumes: `arpg::items::validate_item()` from `items/item_catalog.hpp`.
- Preserves: six deterministic equipped item IDs, the real production combat loop, its 12,000-tick budget, and every downstream Stage 10 assertion.
- Produces: six valid level-95, reinforcement-15, six-affix fixture items plus deterministic legal player movement/action input that visits all streamed regions.

- [x] **Step 1: Preserve RED evidence**

Use the already recorded post-stack-fix run at commit `ab9f888`:

```text
stage10.validation_fixture.real_abyss_transactions: 0/1
direct exit: 8 (0x00000008)
diagnostic: clear failed phase=1 abyss=1 rule=8 lifecycle_pending= room=1 hp=507 targets=560
```

This is the correct RED because it reaches the real combat loop and fails only after the old fixture build cannot clear the dense room.

- [x] **Step 2: Add only the deterministic validation build**

Add:

```cpp
#include "items/item_catalog.hpp"
```

At the start of `install_validation_build`, define these exact tables:

```cpp
constexpr std::array<std::uint8_t, 6> base_ids{{
    8U, 10U, 12U, 14U, 16U, 18U,
}};
constexpr std::array<std::array<arpg::items::AffixRoll, 6>, 6> affixes{{
    {{{1U, 1U, 0xFFU}, {2U, 1U, 0xFFU}, {3U, 1U, 0xFFU},
      {101U, 1U, 0xFFU}, {105U, 1U, 0xFFU}, {106U, 1U, 0xFFU}}},
    {{{7U, 1U, 0xFFU}, {11U, 1U, 0xFFU}, {12U, 1U, 0xFFU},
      {103U, 1U, 0xFFU}, {104U, 1U, 0xFFU}, {111U, 1U, 0xFFU}}},
    {{{7U, 1U, 0xFFU}, {11U, 1U, 0xFFU}, {12U, 1U, 0xFFU},
      {103U, 1U, 0xFFU}, {104U, 1U, 0xFFU}, {111U, 1U, 0xFFU}}},
    {{{3U, 1U, 0xFFU}, {11U, 1U, 0xFFU}, {12U, 1U, 0xFFU},
      {101U, 1U, 0xFFU}, {103U, 1U, 0xFFU}, {104U, 1U, 0xFFU}}},
    {{{7U, 1U, 0xFFU}, {11U, 1U, 0xFFU}, {12U, 1U, 0xFFU},
      {102U, 1U, 0xFFU}, {103U, 1U, 0xFFU}, {104U, 1U, 0xFFU}}},
    {{{3U, 1U, 0xFFU}, {11U, 1U, 0xFFU}, {12U, 1U, 0xFFU},
      {103U, 1U, 0xFFU}, {110U, 1U, 0xFFU}, {111U, 1U, 0xFFU}}},
}};
```

Make the generated item mutable, then before insertion set exactly:

```cpp
item->base_id = base_ids[index];
item->affixes = {};
std::copy(affixes[index].begin(), affixes[index].end(),
    item->affixes.begin());
item->affix_count = 6U;
item->required_level = 95U;
item->reinforcement = 15U;
if (!arpg::items::validate_item(*item)) return false;
```

Do not copy any other hunk from `d57c2e8`.

- [ ] **Step 2B: Remove the no-resident streaming deadlock with legal input**

Add explicit includes for the production skill geometry and room grid:

```cpp
#include "combat/active_skill_runtime.hpp"
#include "combat/room_spatial_grid.hpp"
#include "skills/active_skill_types.hpp"
```

Add a fixture-local driver state containing the sweep waypoint, an optional
locked navigation ordinal, and its fixed legal stance/facing, plus a
deterministic waypoint function. Use
exactly the zero-based row centers `{2U, 6U, 10U, 14U, 18U}`. Build ten
boustrophedon waypoints from the center of the first/last grid columns:

```text
(minX,row2) -> (maxX,row2) -> (maxX,row6) -> (minX,row6)
-> (minX,row10) -> (maxX,row10) -> (maxX,row14) -> (minX,row14)
-> (minX,row18) -> (maxX,row18) -> repeat
```

Compute centers from `room_bounds::min_x/min_y` and
`room_spatial::cell_width/cell_depth`; do not hard-code world coordinates.
Advance the waypoint only when both axes are within `0.25F`, and use the
existing `movement_toward()` for the legal movement input. These five rows are
four cells apart, so their five-row streamed footprints overlap.

Inside `drive_clear()`, keep one driver state. When
`nearest_monster(*state.combat)` is null while `remaining_targets != 0`, pass
the sweep movement to `session.tick()` instead of the current zero movement.
When a resident navigation target exists, lock its `monster_ordinal` until that
target is dead or absent from the active snapshot so moving monsters cannot
make the driver switch targets every tick. Compute and store the stance/facing
once when the lock is acquired; do not recompute the waypoint from the moving
monster every tick.

Do not move toward the target center. Compute a legal ranged-skill stance:

- choose facing right for a target at/after room center and facing left for a
  target before room center, so the stance remains on the room-interior side;
- desired X is `target.x - facing * kDrawSlashRange`;
- desired Y is the target Y clamped to
  `[room_bounds::min_y + kDrawSlashHalfWidthAtEnd,
    room_bounds::max_y - kDrawSlashHalfWidthAtEnd]`;
- move toward that stance with a `0.25F` dead zone on both axes; do not reuse
  the generic helper's `0.45F` X dead zone because it can stop outside the
  exact `kDrawSlashRange` stance. Once both axes are within `0.25F`, send one
  legal horizontal movement tick if needed to establish the intended facing.

This keeps boundary targets inside the Draw Slash cone instead of pinning the
player against an unreachable target center. Navigation locking and action
selection are separate: each tick scan all active, living resident monsters
for legal attack geometry rather than requiring the navigation target itself
to be attackable. With that movement, use this exact legal action priority
while the player has HP, no hurt/hit-stop, no active basic attack, no active
skill, and an empty input queue:

1. If any living resident lies inside the real Storm Swords center/radius geometry,
   call `session.request_active_skill_slot(1U)`.
2. If Storm was not accepted and any living resident lies inside the real Draw Slash
   forward cone, call `session.request_active_skill_slot(0U)`.
3. If neither skill was accepted, queue the existing `Action::light` fallback
   only when at least one living resident satisfies the existing
   attack-lane/idle checks.

Use only `kStormCenterForward`, `kStormStrikeRadius`, `kDrawSlashRange`, and
`kDrawSlashHalfWidthAtEnd` from `combat/active_skill_runtime.hpp`; do not add
fixture-owned damage, range, cooldown, or movement constants. Always call
`session.tick(movement)` exactly once per loop iteration. Do not call
`relay_defeated`, any test defeat helper, or mutate monsters/room counters.

- [ ] **Step 3: Reconfigure and rebuild only the fixture**

From a VS2022 x64 Developer PowerShell:

```powershell
$env:CMAKE_BUILD_PARALLEL_LEVEL = '1'
cmake --preset windows-msvc-debug
cmake --build --preset windows-msvc-debug `
  --target arpg_stage10_validation_fixture -- -j1
```

Expected: configure and target build exit 0.

- [ ] **Step 4: Verify GREEN directly and through CTest**

```powershell
& 'out/build/windows-msvc-debug/bin/arpg_stage10_validation_fixture.exe'
ctest --test-dir out/build/windows-msvc-debug `
  -R '^stage10\.validation_fixture\.real_abyss_transactions$' `
  --output-on-failure -j1
```

Expected: direct executable exits 0; selector runs exactly one test and passes 1/1 within its unchanged timeout. Confirm the output contains no warnings or fixture diagnostics.

- [ ] **Step 5: Scope and whitespace audit**

```powershell
git diff --check
git diff -- tests/dungeon/stage10_validation_fixture.cpp
git status --short
```

Expected: only this plan and the Stage 10 fixture changed; no unrelated historical hunk or whitespace error.

- [ ] **Step 6: Commit the independently testable repair**

```powershell
git add docs/superpowers/plans/2026-07-31-stage10-dense-fixture-recovery.md `
  tests/dungeon/stage10_validation_fixture.cpp
git commit -m "test: restore stage10 dense-room fixture build"
```

## Self-Review

- The fixture still clears real monsters with the existing production combat loop.
- The test budget and all Stage 10 assertions remain unchanged.
- Only deterministic fixture equipment and legal player input selection are strengthened.
- Empty resident regions are traversed rather than treated as an empty whole room.
- Storm Swords and Draw Slash are requested through the production skill API; light remains the fallback.
- No unrelated hunk from `d57c2e8` is imported.
