# Stage 18 Performance Foundation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Remove full combat/dungeon snapshot construction from fixed-tick scalar queries and provide a repeatable Release benchmark for atomic save commits.

**Architecture:** Add narrow, allocation-free query methods at the owners of combat and dungeon state, then route existing high-frequency consumers through them while preserving the public render snapshot. Add an opt-in, non-CTest performance executable that measures the production dual-slot `SaveStore` transaction without enforcing a machine-dependent threshold.

**Tech Stack:** C++17, raylib 6.0, CMake 3.25+, Ninja, MSVC 19.44, Windows SDK 10.0.26100.0, CTest.

## Global Constraints

- Do not change gameplay, input timing, V8 persistence bytes, dual-slot recovery semantics, or public snapshot field layout.
- New runtime queries must be `noexcept` and allocation-free.
- Do not add snapshot caching, background save threads, codec migrations, or broad file splits.
- Write a failing test before each production API or behavior change.
- Preserve all unrelated tracked and untracked workspace changes.

---

### Task 1: Combat scalar queries

**Files:**
- Create: `tests/combat/combat_query_tests.cpp`
- Modify: `tests/combat/combat_test_main.cpp`
- Modify: `tests/combat/CMakeLists.txt`
- Modify: `src/combat/combat_world.hpp`
- Modify: `src/combat/combat_world.cpp`

**Interfaces:**
- Produces: `combat::Vec3 CombatWorld::player_position() const noexcept`
- Produces: `std::size_t CombatWorld::living_monster_count() const noexcept`

- [ ] **Step 1: Write the failing query tests**

Create a two-case suite. The first checks that `player_position()` matches `snapshot().player.position` before and after a movement tick. The second constructs a non-respawning legacy world, defeats the first target with the existing light-attack helper, and checks that `living_monster_count()` equals the number of `active && hp > 0` entries in the snapshot.

Register `combat_query_suite()` and raise the complete combat case count from `228` to `230`.

- [ ] **Step 2: Run the combat build to verify RED**

Run:

```powershell
cmake --build --preset windows-msvc-debug --target arpg_combat_tests
```

Expected: compilation fails because `CombatWorld` has no `player_position` or `living_monster_count` member.

- [ ] **Step 3: Add the minimal query implementation**

Declare both methods beside the existing active-count methods. Implement:

```cpp
Vec3 CombatWorld::player_position() const noexcept {
    return player_.position;
}

std::size_t CombatWorld::living_monster_count() const noexcept {
    std::size_t count = 0U;
    for (const MonsterRuntime& monster : monsters_.slots()) {
        if (monster.active && monster.hp > 0) ++count;
    }
    return count;
}
```

- [ ] **Step 4: Run the focused test to verify GREEN**

Run:

```powershell
cmake --build --preset windows-msvc-debug --target arpg_combat_tests
ctest --test-dir out/build/windows-msvc-debug -R '^combat\.units$' --output-on-failure
```

Expected: `combat.units` passes with `230 cases, 0 failures`.

- [ ] **Step 5: Commit**

```powershell
git add src/combat/combat_world.hpp src/combat/combat_world.cpp tests/combat/combat_query_tests.cpp tests/combat/combat_test_main.cpp tests/combat/CMakeLists.txt
git commit -m "perf: add allocation-free combat queries"
```

### Task 2: Route dungeon and runtime hot paths through scalar queries

**Files:**
- Create: `tests/dungeon/dungeon_query_tests.cpp`
- Modify: `tests/dungeon/dungeon_test_main.cpp`
- Modify: `tests/dungeon/CMakeLists.txt`
- Modify: `src/dungeon/dungeon_session.hpp`
- Modify: `src/dungeon/dungeon_session.cpp`
- Modify: `src/dungeon/dungeon_transition.cpp`
- Modify: `src/platform/raylib/dungeon_runtime.cpp`

**Interfaces:**
- Consumes: `CombatWorld::player_position()` and `CombatWorld::living_monster_count()` from Task 1.
- Produces: `dungeon::RoomPhase DungeonSession::phase() const noexcept`.

- [ ] **Step 1: Write the failing phase query tests**

Create a two-case suite using the existing initial-run-state helpers. Verify a new session reports `RoomPhase::locked`, reports `RoomPhase::combat` after one tick, and reports `RoomPhase::faulted` after an existing deterministic fault trigger used by dungeon lifecycle tests. Register the suite and raise the complete dungeon case count from `285` to `287`.

- [ ] **Step 2: Run the dungeon build to verify RED**

Run:

```powershell
cmake --build --preset windows-msvc-debug --target arpg_dungeon_tests
```

Expected: compilation fails because `DungeonSession` has no `phase()` member.

- [ ] **Step 3: Implement the phase query and hot-path replacements**

Implement:

```cpp
RoomPhase DungeonSession::phase() const noexcept {
    return phase_;
}
```

Then make these exact substitutions:

- In `DungeonSession::tick()`, read `const combat::Vec3 player_position = combat_->player_position();` once and use it for exit confirmation, exit selection, and nearby pickups.
- In both explicit pickup methods, replace `combat_->snapshot().player.position` with `combat_->player_position()`.
- In `remaining_targets()`, clamp `combat_->living_monster_count()` to the `std::uint8_t` return range.
- In `DungeonRuntime::state()`, replace `session_->snapshot().phase` with `session_->phase()`.

- [ ] **Step 4: Run focused dungeon and platform tests**

Run:

```powershell
cmake --build --preset windows-msvc-debug --target arpg_dungeon_tests arpg_platform_tests
ctest --test-dir out/build/windows-msvc-debug -R '^(dungeon\.units|platform\.units)$' --output-on-failure
```

Expected: both CTest entries pass; dungeon output reports `287 cases, 0 failures`.

- [ ] **Step 5: Audit the removed hot-path snapshot calls**

Run:

```powershell
rg -n "combat_->snapshot\(\)\.player\.position|const combat::CombatSnapshot state = combat_->snapshot\(\)|session_->snapshot\(\)\.phase" src/dungeon src/platform/raylib/dungeon_runtime.cpp
```

Expected: no matches. `DungeonSession::snapshot()` and render/receipt snapshot consumers remain.

- [ ] **Step 6: Commit**

```powershell
git add src/dungeon/dungeon_session.hpp src/dungeon/dungeon_session.cpp src/dungeon/dungeon_transition.cpp src/platform/raylib/dungeon_runtime.cpp tests/dungeon/dungeon_query_tests.cpp tests/dungeon/dungeon_test_main.cpp tests/dungeon/CMakeLists.txt
git commit -m "perf: remove snapshots from dungeon scalar queries"
```

### Task 3: Production save commit benchmark and final verification

**Files:**
- Create: `tools/performance/CMakeLists.txt`
- Create: `tools/performance/save_commit_probe.cpp`
- Modify: `CMakeLists.txt`
- Create: `docs/validation/stage18-performance-foundation.md`

**Interfaces:**
- Produces: opt-in CMake option `ARPG_BUILD_PERFORMANCE_PROBES` (default `OFF`).
- Produces: executable `arpg_save_commit_probe` with optional sample-count argument and textual min/median/P95/max/average milliseconds.

- [ ] **Step 1: Add the opt-in target before its source exists**

Add this top-level option and conditional subdirectory:

```cmake
option(ARPG_BUILD_PERFORMANCE_PROBES "Build local performance probes" OFF)
if(ARPG_BUILD_PERFORMANCE_PROBES)
    add_subdirectory(tools/performance)
endif()
```

Create `tools/performance/CMakeLists.txt` referring to `save_commit_probe.cpp` and linking `arpg_persistence` plus `arpg_dungeon`.

- [ ] **Step 2: Configure/build to verify RED**

Run:

```powershell
.\scripts\Build.ps1 -Preset windows-msvc-release
cmake -S . -B out/build/windows-msvc-release -DARPG_BUILD_PERFORMANCE_PROBES=ON
cmake --build out/build/windows-msvc-release --target arpg_save_commit_probe
```

Expected: the probe target fails because `save_commit_probe.cpp` does not exist.

- [ ] **Step 3: Implement the probe**

The executable must:

- Parse an optional integer sample count, accepting `1..10000` and defaulting to `101`.
- Build a valid initial run using `dungeon::make_initial_run_state(0x53544147453138ULL, dungeon::DungeonRules{})`.
- Use a dedicated directory below the executable build tree, never the user's default save directory.
- Perform one warm-up commit, then increment `commit_generation` for every measured commit.
- Time only `SaveStore::commit()` with `std::chrono::steady_clock`.
- Fail immediately if any commit is not `committed` or verifies the wrong generation.
- Sort samples and print count, min, median, P95, max, and average in milliseconds.

- [ ] **Step 4: Build and run the Release benchmark**

Run:

```powershell
cmake --build out/build/windows-msvc-release --target arpg_save_commit_probe
.\out\build\windows-msvc-release\bin\arpg_save_commit_probe.exe 101
```

Expected: exit `0`, 101 valid samples, and all six statistics printed. Record the exact result and compare P95 with `16.67 ms`; do not infer asynchronous-save need from Debug data.

- [ ] **Step 5: Run final verification**

Run:

```powershell
.\scripts\Build.ps1 -Preset windows-msvc-debug
ctest --test-dir out/build/windows-msvc-debug -R '^(combat\.units|dungeon\.units|platform\.units|persistence\.units|stage16\.loot_reinforcement\.real_raylib|stage17\.skill_stones\.)$' --output-on-failure
.\scripts\Build.ps1 -Preset windows-msvc-release
ctest --test-dir out/build/windows-msvc-release -R '^(combat\.units|dungeon\.units|platform\.units|persistence\.units|stage16\.loot_reinforcement\.real_raylib|stage17\.skill_stones\.)$' --output-on-failure
git diff --check
```

Expected: both builds succeed, all selected Debug/Release tests pass, and `git diff --check` is clean.

- [ ] **Step 6: Document and commit**

Record baseline, changed call sites, benchmark statistics, async-save decision, build/test commands, and results in the validation document.

```powershell
git add CMakeLists.txt tools/performance docs/validation/stage18-performance-foundation.md docs/superpowers/plans/2026-07-21-stage18-performance-foundation.md
git commit -m "perf: add save commit benchmark"
```
