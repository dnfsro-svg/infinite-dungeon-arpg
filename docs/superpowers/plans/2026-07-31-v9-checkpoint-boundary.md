# V9 Checkpoint Boundary Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use
> `superpowers:subagent-driven-development` (recommended) or
> `superpowers:executing-plans` to implement this plan task-by-task. Steps use
> checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make Persistence source- and link-independent from Dungeon and Combat
without changing one V9 byte, validation rule, capacity, or gameplay behavior.

**Architecture:** Add an `arpg_checkpoint` lower-layer library that owns the
stable DTOs and wire-structural validation. Combat and Dungeon use field-wise
adapters; Persistence serializes and owns only neutral checkpoint values.
Existing headers remain as compatibility facades, never as transitive runtime
dependency tunnels.

**Tech Stack:** C++17, CMake 3.25+, Ninja, MSVC 19.44, CTest, raylib 6.0
project (no raylib dependency in this subsystem).

**Design:**
`docs/superpowers/specs/2026-07-31-v9-checkpoint-boundary-design.md`

## Global Constraints

- Work only in `E:/game/.worktrees/whole-game-refactor-2026-07` on
  `codex/whole-game-refactor-2026-07`.
- Preserve the controller's unstaged Task9 progress/baseline evidence and all
  content-identical EOL/stat dirt; stage only each task's reviewed paths.
- Keep `CMAKE_BUILD_PARALLEL_LEVEL=1`; configure/build/test serially with
  `--parallel 1` / `-j1`.
- Do not delete files, reset Git, change gameplay, change V9/V8 bytes, update
  goldens, change capacities, change test timeouts, or weaken a guard.
- Large room/save DTOs remain non-copyable/non-movable and heap-owned at
  runtime/test call sites.
- `src/checkpoint` may depend on existing lower libraries but must not include
  or link Combat, Dungeon runtime, Persistence, Platform, raylib, or App.
- Persistence must link without `arpg_combat` or `arpg_dungeon`.
- Each task ends in independent specification and quality review before its
  exact commit. Full suites wait until Task 4.

---

### Task 0: Freeze the pre-refactor V9 wire manifest

**Files:**

- Create: `tests/persistence/checkpoint_v9_wire_baseline.cpp`
- Create: `tests/persistence/checkpoint_v9_wire_baseline_test.cmake`
- Modify: `tests/persistence/CMakeLists.txt`

**Interfaces:**

- Produces a deterministic V9 fixture executable that selects a named scenario
  and writes its encoded bytes to a requested build-directory path.
- Produces test `persistence.v9_wire_baseline`.
- Freezes exact encoded byte counts and whole-stream SHA-256 values for five
  scenarios before any production checkpoint type changes.

- [ ] **Step 1: Build the characterization probe**

Use fixed literal values for five named scenarios: `canonical_none`,
`active_normal`, `started_abyss`, `death_pending`, and
`cleared_abyss_rewards`. Across the set, cover nondefault run/room/combat state,
attack/history/effects, a nondefault CombatDeathSnapshot, abyss state,
equipment, material, and potion records. Heap-own every large slot and byte
buffer. The probe writes only under its supplied build-directory output path
and does not modify source.

- [ ] **Step 2: Freeze and verify the current output**

The CMake test runs all five scenarios, checks each exact byte count, and uses
`file(SHA256 ...)` to compare each entire stream with its literal expected
digest. Capture those manifest rows once from the current pre-refactor encoder.
Then run:

```powershell
cmake --preset windows-msvc-debug
cmake --build --preset windows-msvc-debug `
  --target arpg_checkpoint_v9_wire_baseline --parallel 1
ctest --test-dir out/build/windows-msvc-debug `
  -R '^persistence\.v9_wire_baseline$' `
  --output-on-failure -j1 --no-tests=error
git diff --check
```

Expected: 1/1 pass with five validated artifacts. Tasks 1-4 may not modify the
fixture source, expected sizes, or digests.

- [ ] **Step 3: Review and commit**

After two independent reviews commit only Task 0 paths:

```text
test: freeze v9 checkpoint wire manifest
```

---

### Task 1: Add the neutral schema and its own boundary

**Files:**

- Create: `src/checkpoint/CMakeLists.txt`
- Create: `src/checkpoint/dungeon_run_state.hpp`
- Create: `src/checkpoint/room_checkpoint_schema.hpp`
- Create: `src/checkpoint/room_combat_checkpoint.hpp`
- Create: `src/checkpoint/room_progress_checkpoint.hpp`
- Create: `src/checkpoint/room_checkpoint_validation.hpp`
- Create: `src/checkpoint/room_checkpoint_validation.cpp`
- Create: `cmake/AssertTargetDependencyBoundary.cmake`
- Modify: `CMakeLists.txt`
- Create: `tests/checkpoint/CMakeLists.txt`
- Create: `tests/checkpoint/checkpoint_schema_tests.cpp`
- Create: `tests/checkpoint/checkpoint_test_main.cpp`
- Create: `tests/checkpoint/checkpoint_boundary_test.cmake`

**Interfaces:**

- Produces namespace `arpg::checkpoint`.
- Produces non-copyable/non-movable `RoomCombatCheckpoint`,
  `RoomProgressCheckpoint`, and `SaveCheckpointSlot`.
- Produces:

```cpp
void clear_room_combat_checkpoint(RoomCombatCheckpoint&) noexcept;
void clear_room_progress_checkpoint(RoomProgressCheckpoint&) noexcept;
void clear_save_checkpoint_slot(SaveCheckpointSlot&) noexcept;

[[nodiscard]] bool valid_room_combat_checkpoint_structural(
    const RoomCombatCheckpoint&, std::uint32_t generated_monsters) noexcept;
[[nodiscard]] bool valid_room_progress_checkpoint_structural(
    const RoomProgressCheckpoint&, const DungeonRunState&) noexcept;
[[nodiscard]] bool same_room_combat_checkpoint(
    const RoomCombatCheckpoint&, const RoomCombatCheckpoint&) noexcept;
[[nodiscard]] bool same_room_progress_checkpoint(
    const RoomProgressCheckpoint&, const RoomProgressCheckpoint&) noexcept;
```

- Produces schema constants and mappings:

```cpp
inline constexpr std::size_t kRoomEquipmentGroundCapacity =
    limits::kRoomMonsterCapacity;
inline constexpr std::size_t kRoomSecondaryGroundCapacity =
    limits::kRoomMonsterCapacity * 2U + 16U;
inline constexpr std::uint16_t kOrdinarySecondaryOrdinalEnd = 768U;
inline constexpr std::uint16_t kAbyssSecondaryOrdinalBegin = 2304U;
inline constexpr std::size_t kHealthPotionGroundCapacity = 192U;

[[nodiscard]] constexpr std::uint16_t checkpoint_material_ordinal(
    std::uint16_t material_ordinal) noexcept;
[[nodiscard]] constexpr std::uint16_t material_ordinal_from_checkpoint(
    std::uint16_t checkpoint_ordinal) noexcept;
[[nodiscard]] constexpr std::uint16_t health_potion_claim_ordinal(
    std::uint16_t spawn_ordinal) noexcept;
[[nodiscard]] constexpr std::uint32_t required_kills(
    std::uint32_t generated_monsters) noexcept;
```

- Consumes only `core`, `abyss`, `skills`, `modifiers`, `items`,
  `progression`, and `passives` value contracts.

- [ ] **Step 1: Add a compile RED for the missing neutral API**

Create `checkpoint_schema_tests.cpp` with a test that heap-allocates a
`SaveCheckpointSlot`, calls the three clear functions, and checks canonical
default state, fixed capacities, all three ordinal mappings, the required-kills
formula, and noncopyable type traits. The canonical default assertions must
preserve nonzero defaults/sentinels such as initial depth, default skill
loadout, right/none/count/`0xFFFF`, and `air_attack_available=true`; they must
also prove that clear reuses the item vector backing/capacity. Register
`arpg_checkpoint_tests` before adding the implementation.

Run:

```powershell
cmake --preset windows-msvc-debug
cmake --build --preset windows-msvc-debug `
  --target arpg_checkpoint_tests --parallel 1
```

Expected: build fails because the neutral headers/functions do not exist.

- [ ] **Step 2: Define the stable value model**

Copy the declarations currently owned by `dungeon/dungeon_checkpoint.hpp`
into `checkpoint/dungeon_run_state.hpp`, changing only the namespace to
`arpg::checkpoint` and assigning every enum value explicitly. Do not edit or
alias the legacy production header in this task; the cutover occurs in Task 3.

Define checkpoint-specific combat values in
`checkpoint/room_combat_checkpoint.hpp` in the exact order written/read by
`room_progress_codec.cpp`. Use this wire vector instead of `combat::Vec3`:

```cpp
struct CheckpointVec3 final { float x{}; float y{}; float z{}; };
```

Define checkpoint-specific fixed-width enums for facing, player state, attack,
monster identity/kind/reaction/AI/armor/affix warning, damage source/type, and
every other serialized Combat enum. `Facing` remains `std::int8_t` with exact
values `-1` and `1`. Reuse only already-lower-layer value DTOs
such as `modifiers::EffectSetCheckpoint` and `items::ItemInstance`.

Keep the current field order and array capacities. Preserve constructors:

```cpp
RoomCombatCheckpoint() noexcept = default;
RoomCombatCheckpoint(const RoomCombatCheckpoint&) = delete;
RoomCombatCheckpoint& operator=(const RoomCombatCheckpoint&) = delete;
RoomCombatCheckpoint(RoomCombatCheckpoint&&) = delete;
RoomCombatCheckpoint& operator=(RoomCombatCheckpoint&&) = delete;
```

Apply the same policy to room progress and save slot.

- [ ] **Step 3: Mirror pure schema rules and validation**

Implement ordinal mappings with the exact existing formulas:

```cpp
return material_ordinal >= 384U
    ? static_cast<std::uint16_t>(2304U + material_ordinal - 384U)
    : static_cast<std::uint16_t>(material_ordinal * 2U);
```

Port canonical clear/equality and every pre-publish structural rejection from
the existing Combat/Dungeon checkpoint validators. Neutral rules explicitly
own room bounds, `(generated + 3) / 4` required kills, damage-history
structure/totals, attack active-tick values, and affix interval/duration values.
Use `arpg_modifiers` for effect checkpoint validation and `arpg_items` for item
validation. Do not call `room_bounds`, `PlayerDamageHistory`,
`find_attack_definition`, `monster_affix_definition`, or any other live
Combat/Dungeon symbol. Preserve the existing
item/effect/history/death/ground/abyss rejection matrix.

Implement clear field-by-field; assigning an entire large checkpoint is
ill-formed because the DTOs are noncopyable/nonmovable. Equality compares only
records below each active count and deliberately ignores unused array tails,
matching the existing equivalence contract.

- [ ] **Step 4: Add the neutral-layer source guard**

`checkpoint_boundary_test.cmake` recursively scans `src/checkpoint` and fails
on direct includes whose first component is any of:

```text
combat dungeon persistence platform app raylib
```

It also scans `src/checkpoint/CMakeLists.txt` for those forbidden targets.
Add a reusable configure-time target-closure assertion that recursively walks
`LINK_LIBRARIES`/`INTERFACE_LINK_LIBRARIES`; assert that `arpg_checkpoint`
cannot reach any forbidden runtime target.

Add a self-contained mutation call in the test script: write one temporary
header containing `#include "combat/combat_types.hpp"`, run the same scanner,
require the intended forbidden-runtime diagnostic, and configure a tiny
build-directory-only target graph containing a forbidden link edge to require
the intended dependency-closure diagnostic. Then remove only those build
directory fixtures.

- [ ] **Step 5: Register and verify Task 1**

Register all lower targets first, then `src/checkpoint`, then
Combat/Dungeon/Persistence. Add `tests/checkpoint` in the root `BUILD_TESTING`
block. Link `arpg_checkpoint` only to the lower targets listed above.

Run serially:

```powershell
cmake --preset windows-msvc-debug
cmake --build --preset windows-msvc-debug `
  --target arpg_checkpoint_tests --parallel 1
ctest --test-dir out/build/windows-msvc-debug `
  -R '^(checkpoint\.units|architecture\.checkpoint_no_runtime_headers)$' `
  --output-on-failure -j1 --no-tests=error
git diff --check
```

Expected: 2/2 pass. Existing production consumers remain unchanged and
`architecture.persistence_checkpoint_only` remains the preserved independent
RED until Task 3.

- [ ] **Step 6: Review and commit**

After two independent reviews stage only the Task 1 files and commit:

```text
refactor: add neutral checkpoint schema
```

---

### Task 2: Adapt Combat capture and restore

**Files:**

- Modify: `src/combat/room_combat_checkpoint.hpp`
- Modify: `src/combat/room_combat_checkpoint.cpp`
- Modify: `src/combat/combat_world.hpp`
- Modify: `src/combat/combat_world.cpp`
- Modify: `src/combat/player_damage_history.hpp/.cpp`
- Modify: `src/combat/CMakeLists.txt`
- Modify: `tests/combat/room_combat_checkpoint_tests.cpp`

**Interfaces:**

- The existing `combat::RoomCombatCheckpoint` and its production overload stay
  unchanged in this task.
- `CombatWorld` gains parallel capture/restore overloads for
  `checkpoint::RoomCombatCheckpoint`; no call allocates or copies a second full
  room checkpoint.
- Produces field-wise conversion helpers only inside the Combat implementation.

- [ ] **Step 1: Add enum and capture/restore RED coverage**

Extend the Combat checkpoint suite to compare every runtime enum numeric value
to its checkpoint counterpart; compare every `AttackId` active-tick value and
every monster-affix tier interval/duration against the live catalogs; then
capture → restore → recapture a maximum resident room, effects, active attack,
environment, obstacles, history, and death snapshot through the new neutral
overload. Build/run the Combat target before adapters.

Expected: compile failure because the neutral overload does not exist.

- [ ] **Step 2: Add explicit value conversions**

Use checked conversion functions shaped as:

```cpp
[[nodiscard]] bool to_runtime(checkpoint::Facing source,
    Facing& destination) noexcept;
[[nodiscard]] checkpoint::Facing to_checkpoint(Facing source) noexcept;
```

Provide one pair for every serialized Combat enum/aggregate. Reject values at
or beyond each `count`/sentinel. Copy arrays field-wise; never `memcpy` structs
and never allocate/copy a second full room checkpoint. Keep the legacy overload
as the unchanged production path until Task 3.

- [ ] **Step 3: Bind schema and production rules**

Add `static_assert`s for every frozen enum value, capacity, effect size,
history length, skill cooldown count, obstacle count, and monster capacity.
Because live attack/affix tables are `.cpp` data, enforce their full parity with
the runtime matrix test from Step 1, not a cross-target `static_assert`. Keep
live-plan, obstacle-layout, and runtime configuration validation in Combat
restore before mutation.

- [ ] **Step 4: Verify Combat**

Run serially:

```powershell
cmake --build --preset windows-msvc-debug `
  --target arpg_combat_tests arpg_checkpoint_tests arpg_dungeon_tests `
           arpg_persistence_tests arpg_game --parallel 1
ctest --test-dir out/build/windows-msvc-debug `
  -R '^(combat\.units|checkpoint\.units|persistence\.v9_wire_baseline|architecture\.checkpoint_no_runtime_headers)$' `
  --output-on-failure -j1 --no-tests=error
git diff --check
```

Require 4/4 pass; this proves the parallel API did not break any legacy
production consumer.

- [ ] **Step 5: Review and commit**

After two independent reviews commit only Task 2 paths:

```text
refactor: adapt combat to neutral checkpoints
```

---

### Task 3: Atomic production cutover — Part A, Combat and Dungeon

**Files:**

- Modify: `src/dungeon/dungeon_checkpoint.hpp`
- Modify: `src/dungeon/room_progress_checkpoint.hpp/.cpp`
- Modify: `src/dungeon/dungeon_session.hpp/.cpp`
- Modify: `src/dungeon/material_loot.hpp/.cpp`
- Modify: `src/dungeon/health_potion_loot.hpp/.cpp`
- Modify: `src/dungeon/CMakeLists.txt`
- Modify: `src/combat/room_combat_checkpoint.hpp/.cpp`
- Modify: `src/combat/combat_world.hpp/.cpp`
- Modify: `src/combat/player_damage_history.hpp/.cpp`
- Modify: `src/combat/CMakeLists.txt`
- Modify: direct Combat checkpoint tests under `tests/combat`
- Modify: direct Dungeon checkpoint tests under `tests/dungeon`

**Interfaces:**

- Existing `arpg::dungeon::checkpoint` type names remain compatibility aliases
  to `arpg::checkpoint`.
- `combat::RoomCombatCheckpoint` becomes the final compatibility alias and the
  temporary legacy Combat overload from Task 2 is removed.
- `DungeonSession::capture_save_checkpoint` and
  `restore_room_progress_checkpoint` consume neutral `SaveCheckpointSlot`.
- Dungeon loot uses schema-owned ordinal constants/mappings.
- Part A and Part B are one atomic task: do not build, review, or commit the
  production cutover between them because `arpg_dungeon_tests` links
  `arpg_persistence`.

- [ ] **Step 1: Add mapping and maximum-room RED coverage**

Extend Dungeon tests to assert the Dungeon material/potion APIs return the
same ordinals as `arpg::checkpoint`, then capture and restore normal, abyss,
death-pending, partial-unlock, full-clear, equipment, material, and potion
states through the neutral slot.

Expected before the complete Part A+B cutover: compile/type mismatch or
duplicate mapping owner. This RED is not committed.

- [ ] **Step 2: Turn old headers into explicit adapters**

`dungeon_checkpoint.hpp` includes the neutral run-state header and declares
`using` aliases only. `room_progress_checkpoint.hpp` includes the neutral room
schema and declares Dungeon semantic adapter functions only; it must not own a
second DTO definition.

- [ ] **Step 3: Switch capture/restore field access**

Update Dungeon capture/restore and room-progress validation to use neutral
vectors/enums/records. Call the Combat field-wise adapter for the embedded room
checkpoint. Preserve validation-before-mutation and all existing fault codes.

- [ ] **Step 4: Make schema the sole ordinal owner**

Keep the public Dungeon helper names as constexpr wrappers if needed, but make
their bodies call `arpg::checkpoint` functions and bind constants with
`static_assert`. Remove no runtime loot API.

- [ ] **Step 5: Continue directly to Part B**

Do not run the Dungeon-linked test target or commit yet. Continue in the same
working tree to Part B so Persistence and all downstream callers switch in the
same reviewed commit.

---

### Task 3 continued: Part B, Persistence and boundary closure

**Files:**

- Modify: `src/persistence/room_progress_codec.hpp/.cpp`
- Modify: `src/persistence/save_commit_worker.hpp/.cpp`
- Modify: other Persistence files that spell the old checkpoint namespace
- Modify: `src/persistence/CMakeLists.txt`
- Modify: `CMakeLists.txt`
- Modify: `tests/persistence/CMakeLists.txt`
- Modify: `tests/persistence/checkpoint_v9_tests.cpp`
- Modify: `tests/persistence/save_commit_worker_tests.cpp`
- Modify: `tests/platform/persistence_boundary_test.cmake`
- Create: `tests/persistence/persistence_link_isolation_test.cpp`

**Interfaces:**

- Public Persistence V9 APIs take `arpg::checkpoint::SaveCheckpointSlot`.
- `arpg_persistence` links `arpg_checkpoint`; it does not link Combat/Dungeon.
- `arpg_persistence_link_isolation_tests` links only `arpg_persistence`.
- The configure-time dependency-closure assertion proves neither
  `arpg_persistence` nor `arpg_checkpoint` can reach Combat/Dungeon.

- [ ] **Step 1: Add the link-isolation RED**

Create a tiny executable that heap-allocates a neutral save slot, calls
`clear_save_checkpoint_slot`, encodes V9, decodes V9, and compares the result.
Link only `arpg_persistence`.

Run:

```powershell
cmake --build --preset windows-msvc-debug `
  --target arpg_persistence_link_isolation_tests --parallel 1
```

Expected: current Persistence headers/types or unresolved Dungeon/Combat
symbols make the target fail before the switch.

- [ ] **Step 2: Switch codec and worker ownership**

Replace forbidden includes with neutral checkpoint headers. Delete no codec
field and preserve read/write order. Replace calls to Dungeon/Combat clear,
same, capacities, and ordinal helpers with `arpg::checkpoint` calls. Keep the
heap scratch decode/publish pattern.

- [ ] **Step 3: Link the real lower dependency**

Add `arpg_checkpoint` to `arpg_persistence`. Do not add `arpg_dungeon` or
`arpg_combat` to the library or isolation executable. Apply the recursive
target-closure assertion to `arpg_persistence`.

Remove the `dungeon/dungeon_checkpoint.hpp` exception from
`tests/platform/persistence_boundary_test.cmake`; after this task every Dungeon
or Combat include under `src/persistence` is forbidden.

- [ ] **Step 4: Configure and build the complete atomic cutover**

Run serially after all Part A+B source changes:

```powershell
cmake --preset windows-msvc-debug
cmake --build --preset windows-msvc-debug `
  --target arpg_checkpoint_tests arpg_combat_tests arpg_dungeon_tests `
           arpg_persistence_tests arpg_persistence_link_isolation_tests `
           arpg_checkpoint_v9_wire_baseline arpg_game --parallel 1
```

Configuration must also pass the recursive target-closure assertions. Do not
proceed to CTest unless every listed target builds.

- [ ] **Step 5: Make the integrated gates GREEN**

Run:

```powershell
ctest --test-dir out/build/windows-msvc-debug `
  -R '^(architecture\.persistence_checkpoint_only|architecture\.checkpoint_no_runtime_headers|checkpoint\.units|combat\.units|dungeon\.units|persistence\.units|persistence\.link_isolation|persistence\.save_commit_stack_guard|persistence\.v9_wire_baseline)$' `
  --output-on-failure -j1 --no-tests=error
git diff --check
```

Expected: 9/9 pass. Require the frozen V9 wire manifest, CRC/header, malformed
matrix, exact worker verification, recovery selection, maximum capacity,
Combat/Dungeon adapters, game link, and stack guard all pass without editing
Task 0 fixture values, expected byte count, or SHA-256.

- [ ] **Step 6: Review and commit**

After two independent reviews commit all Part A+B Task 3 paths together:

```text
refactor: cut over to neutral checkpoint boundary
```

---

### Task 4: Cross-layer compatibility and Release proof

**Files:**

- Update after verification:
  `.superpowers/sdd/2026-07-28-raylib-host-structural-refactor/progress.md`
- Update checked steps: this plan.

Task 4 makes no production/test source change. Any compile fallout returns to
the owning Task 1-3 implementation and its two reviews before Task 4 resumes.

**Interfaces:**

- Consumes all Task 1-3 neutral/adaptor APIs.
- Produces Debug and Release focused gate evidence for the overall Task9 gate.

- [ ] **Step 1: Static scope and byte audit**

Run:

```powershell
ctest --test-dir out/build/windows-msvc-debug `
  -R '^(architecture\.persistence_checkpoint_only|architecture\.checkpoint_no_runtime_headers|persistence\.link_isolation)$' `
  --output-on-failure -j1 --no-tests=error
git diff --check
```

Require 3/3 pass and no wire-manifest/timeout/gameplay diff.

- [ ] **Step 2: Debug focused build and tests**

Build `arpg_checkpoint_tests`, `arpg_combat_tests`, `arpg_dungeon_tests`,
`arpg_persistence_tests`, `arpg_platform_tests`, and `arpg_game` serially.
Also build `arpg_checkpoint_v9_wire_baseline` and
`arpg_persistence_link_isolation_tests`. Run exact architecture, checkpoint,
combat, dungeon, persistence, wire-manifest, stack, and platform runtime
selectors with `-j1`.

- [ ] **Step 3: Release focused build and tests**

Repeat Step 2 with `windows-msvc-release`. Require identical test counts and
zero failures.

- [ ] **Step 4: Compare V9 artifacts**

Run `persistence.v9_wire_baseline` in Debug and Release. Each of the five probes
writes only under its configuration build directory and must match Task 0's
exact byte count and whole-stream SHA-256. Then compare each Debug/Release pair
byte-for-byte and require matching header/version/CRC. Do not rewrite the
frozen manifest.

- [ ] **Step 5: Final boundary reviews**

Run two independent read-only reviews: one for dependency/spec compliance and
one for wire compatibility, memory/ownership, and code quality. P0-P3 must all
be closed before the overall Debug/Release full gates begin.

- [ ] **Step 6: Record the compatibility closeout**

Record exact commands, counts, elapsed times, and Debug/Release V9 byte hashes
in the Task9 progress ledger. Keep that controller ledger unstaged and do not
create an empty commit.

## Plan self-review

- Every design invariant is assigned to a task.
- No task deletes a file, weakens a guard, rewrites a golden, or changes a
  timeout/gameplay rule.
- Neutral type/function names are consistent across Tasks 0-4.
- Source and link isolation are separate permanent gates.
- Combat and Dungeon adapters preserve validation-before-mutation.
- Debug and Release proofs precede the overall Task9 full suites.
