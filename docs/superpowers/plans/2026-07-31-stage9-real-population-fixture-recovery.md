# Stage 9 Real-Population Fixture Recovery Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development to implement this plan task-by-task.

**Goal:** Restore the Stage 9 standalone deep-40 reward/reload fixture against the current `RoomMonsterPlan` and streamed `RoomMonsterField` contract while preserving a real combat defeat, production drop roll, transactional pickup, and durable reload evidence chain.

**Architecture:** Replace the obsolete `RoomEncounterPlan` read with the sealed production monster plan. Select one deterministic, affixed, drop-eligible ordinal below the ground-drop capacity, make only its home cell resident, and use a narrowly scoped test access helper to call the real private `CombatWorld::defeat_monster`. The helper must return the real relayed event; the fixture then validates the real ground item, commits a precise pickup, and verifies the rebuilt plan plus durable ownership/claim state after codec reload.

**Tech Stack:** C++17, raylib 6.0 project, CMake 3.25+, Ninja, MSVC 19.44, CTest.

## Global Constraints

- Work only in `E:/game/.worktrees/task9-gate-recovery` on `codex/task9-gate-recovery`.
- Keep `CMAKE_BUILD_PARALLEL_LEVEL=1` and every build/test serial with `-j1`.
- Modify only `tests/dungeon/stage9_validation_fixture.cpp`, `tests/dungeon/dungeon_test_support.hpp`, and this plan.
- Do not change production source, gameplay rules, drop chance, capacity, golden values, CTest timeouts, fixture assertions outside this evidence adaptation, or B-G scope.
- Never call `relay_defeated`, append to the defeat ledger directly, push/synthesize a defeated `CombatEvent`, inject a ground item, or directly mutate ownership/claim state.
- Do not copy a `RoomMonsterPlan`; hold a session-owned pointer and save only a compact target trace.
- Do not delete files, reset Git, or touch any other worktree.
- Use the existing failing selector as RED and record real exit codes and elapsed times.
- Do not run the full CTest suite.

---

### Task 1: Reconnect the Stage 9 fixture to the real monster/drop chain

**Files:**
- Modify: `tests/dungeon/dungeon_test_support.hpp`
- Modify: `tests/dungeon/stage9_validation_fixture.cpp`
- Test: existing `stage9.validation_fixture.real_v4_reward_reload`

**Interfaces:**
- Consumes: `arpg::test::room_monster_plan(session)`, `RoomMonsterField` residency, private real `CombatWorld::defeat_monster`, production drop/pickup/save/codec paths, and `room_monster_plan_equal_fields()`.
- Produces: one test-only by-ordinal real-defeat helper returning the actual relayed event and one deterministic deep-40 evidence trace.

- [x] **Step 1: Preserve RED evidence**

Use the already recorded post-stack-fix run at `ab9f888`:

```text
stage9.validation_fixture.real_v4_reward_reload: 0/1
direct exit: 2 (0x00000002), about 47.6 seconds, no stdout
```

Source tracing proves every constructed session clears legacy
`encounter_plan_`; the fixture's legacy `has_affix(evidence.plan)` check is
therefore false for every root before any combat action. Do not repeat the
47-second RED unless a reviewer requests it.

- [x] **Step 2: Add one real by-ordinal defeat seam**

In `DungeonSessionTestAccess`, next to `defeat_next_live_monster`, add:

```cpp
static std::optional<combat::CombatEvent> defeat_room_monster_by_ordinal(
    dungeon::DungeonSession& session,
    combat::MonsterOrdinal ordinal) noexcept;
```

Add the same inline namespace wrapper. Include `<optional>` explicitly if the
header does not already include it.

The helper must:

1. Reject unless the session is in combat with a nonfaulted world/field and
   `ordinal < field->plan().monster_count`.
2. Read the blueprint at that ordinal and require
   `blueprint.spawn_ordinal == ordinal` plus a valid `home_cell`.
3. Convert `home_cell` to its row/column using the production
   `room_spatial::columns`, then call `synchronize_active_region()` for exactly
   that one cell.
4. Resolve the target only through `resident_handle(ordinal)` and
   `active_runtime(ordinal)`; verify the runtime ordinal/id/spawn/affixes match
   the blueprint and that it is active, alive, and not already defeated.
5. Record field/session defeated counts, set only that real runtime's HP to
   zero (the same private test seam already used by existing real-defeat
   helpers), and call
   `world.defeat_monster(handle->index, combat::AttackId::j1, true)`.
6. Call `session.relay_combat_events()` so production defeat-ledger handling,
   drop rolling, and event forwarding execute.
7. Drain the session combat-event queue, ignore non-defeated warnings, require
   exactly one defeated event for this ordinal, and return that actual event.
8. Before returning, require no world/session fault, both defeated counts rose
   exactly once, the room bit is set, and event ordinal/id/spawn/affix score and
   reward eligibility match the real blueprint.

The helper must not accept caller-supplied id, position, affix score, or reward
payload, and must not construct/push an event or call `relay_defeated`.

- [x] **Step 3: Select a real deterministic affix target**

In `stage9_validation_fixture.cpp`:

- Add the current deterministic RNG/drop-chance support and
  `dungeon/room_monster_plan_builder.hpp`.
- Remove the obsolete `RoomEncounterPlan` evidence value,
  `has_affix(RoomEncounterPlan)`, legacy plan comparison, and legacy
  `build_encounter_plan()` eligibility check.
- Keep `deep40_state` responsible only for constructing the existing valid
  deep-40 state.
- After constructing `DungeonSession`, obtain a session-owned pointer with
  `arpg::test::room_monster_plan(session)` and never copy it.
- Scan only ordinals below
  `min(plan->monster_count, dungeon::kGroundDropCapacity)` and choose the first
  blueprint whose `spawn_ordinal` equals its ordinal, affix count and production
  danger score are positive, and the production-equivalent deterministic
  affix-drop roll is below `affix_drop_chance_bp(score)`.
- Use the exact chance-domain/RNG derivation already shared by production
  `DungeonSession::roll_ground_drop` and
  `tests/dungeon/dungeon_affix_reward_tests.cpp`; it selects a candidate but
  does not replace validation of the actual generated ground item.
- Save only a compact trace: plan count/threat/generator/hash plus target
  ordinal, id, initial position, home cell, roaming leash, full affix set, and
  danger score.

If a root has no qualifying real target, continue to the next root. Never fall
back to an unaffixed or ordinal-at/above-capacity target.

- [x] **Step 4: Drive the real defeat, drop, pickup, and reload path**

For the selected target:

1. Tick once to enter combat and drain startup events.
2. Call only the new by-ordinal helper and compare its returned real event to
   the compact trace.
3. Read the actual ground item at `target.spawn_ordinal`; require active,
   `monster_drop`, matching ordinal, and a valid item. Do not inject/recreate it.
4. Move the player to the actual drop position with the existing test seam,
   call public `session.request_pickup(target.spawn_ordinal)`, require accepted,
   and commit the real pending save with the existing fixture commit helper.
5. Require the target claim bit, exactly one new owned item, and matching item
   id/level/rarity; save the committed durable state.
6. Repeat the fixture and require identical target/event/drop traces.
7. Encode/decode the committed state, construct original/reloaded sessions,
   and compare their rebuilt non-null plans using
   `room_monster_plan_equal_fields()` plus target trace fields.
8. Verify reloaded ownership and all claim words; do not assert transient
   resident, defeated-runtime, or ground-item state survives v4 reload.

Remove the obsolete 4096-tick `defeat_next_live_monster()` loop. Do not trigger
room unlock/clear; this fixture defeats exactly one target.

- [x] **Step 5: Rebuild and run the focused GREEN**

From a VS2022 x64 Developer PowerShell:

```powershell
$env:CMAKE_BUILD_PARALLEL_LEVEL = '1'
cmake --preset windows-msvc-debug
cmake --build --preset windows-msvc-debug `
  --target arpg_stage9_validation_fixture -- -j1
ctest --test-dir out/build/windows-msvc-debug `
  -R '^stage9\.validation_fixture\.real_v4_reward_reload$' `
  --output-on-failure -j1
```

Expected: build exits 0; selector runs exactly one test and passes 1/1. Record
elapsed time and pristine output.

- [x] **Step 6: Audit the evidence chain and scope**

```powershell
rg -n "relay_defeated\s*\(|events_\.try_push\s*\(" `
  tests/dungeon/stage9_validation_fixture.cpp
git diff --check
git diff -- tests/dungeon/stage9_validation_fixture.cpp `
  tests/dungeon/dungeon_test_support.hpp
git status --short
```

Expected: no injection match in the fixture, no whitespace error, and only the
two test files plus this plan changed. The separate
`stage9.evidence.no_injected_defeat_trace` remains a known independent RED at
`dungeon_affix_stress_tests.cpp`; do not weaken it or fold it into this card.

- [x] **Step 7: Commit the independently testable repair**

```powershell
git add docs/superpowers/plans/2026-07-31-stage9-real-population-fixture-recovery.md `
  tests/dungeon/stage9_validation_fixture.cpp `
  tests/dungeon/dungeon_test_support.hpp
git commit -m "test: restore stage9 real population fixture"
```

## Self-Review

- The selected blueprint, real defeated event, actual ground item, claim bit,
  owned item, and reload trace all refer to one ordinal below capacity.
- No old encounter cache or active-slot ordering is treated as authoritative.
- The only private test seam invokes the real defeat function and returns its
  real relayed event; it cannot synthesize caller-chosen evidence.
- The large fixed-capacity plan is never copied onto the stack.
