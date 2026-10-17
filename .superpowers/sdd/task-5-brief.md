# Task 5 Brief — Kill drops, fixed ground pool, and snapshots

## Scope

Implement only Task 5 in `E:\game\.worktrees\balance-hp-potions` after Task 4 is reviewed.

Allowed files:

- `tests/dungeon/dungeon_health_potion_tests.cpp`
- `tests/dungeon/dungeon_test_support.hpp`
- `tests/dungeon/dungeon_test_main.cpp`
- `src/dungeon/dungeon_session.hpp`
- `src/dungeon/dungeon_session.cpp`
- `src/dungeon/dungeon_snapshot.cpp`
- `src/dungeon/dungeon_transition.cpp`

Report: `.superpowers/sdd/balance-hp-task-5-report.md` (do not touch tracked `.superpowers/sdd/task-5-report.md`).

## Required behavior

- Reward-eligible defeated monsters use the existing ordinary-material roll unchanged.
- The existing secondary claim bit/ordinal is evaluated once. Coupon has first priority; only when coupon misses may the independent 30% health-potion roll place a potion.
- A common material may coexist with a potion because its roll/domain is independent.
- Adding potions must not change any existing material or coupon roll outcome.
- Ground potion storage is a fixed 192-element array indexed directly by `spawn_ordinal` (0..191); no allocation and no overwrite.
- Repeated placement into an already active slot returns false, leaves the old object byte-for-byte unchanged, and saturating-increments `health_potion_ground_saturation_count`.
- Snapshot packing scans array indices ascending, therefore active potions are sorted by spawn ordinal; copy the pickup receipt.
- Ground potions survive ordinary/abyss room-clear success until consumed or the player exits/abandons/transitions. They are cleared on room construction, transient room reset, successful transition, and abyss abandon/exit paths.
- Do not wire pending pickup claims/consumption/healing/saves yet; Task 6 owns that.

## Required production shape

Add to `DungeonSession`:

```cpp
[[nodiscard]] bool place_ground_health_potion(
    std::uint16_t spawn_ordinal, combat::Vec3 position) noexcept;

std::array<GroundHealthPotion, kGroundHealthPotionCapacity>
    ground_health_potions_{};
HealthPotionPickupReceipt health_potion_pickup_receipt_{};
```

Placement implementation:

```cpp
bool DungeonSession::place_ground_health_potion(
    std::uint16_t spawn_ordinal, combat::Vec3 position) noexcept {
    if (spawn_ordinal >= ground_health_potions_.size()) return false;
    GroundHealthPotion& slot = ground_health_potions_[spawn_ordinal];
    if (slot.active) {
        saturating_increment(
            diagnostics_.health_potion_ground_saturation_count);
        return false;
    }
    slot = {true, spawn_ordinal,
        health_potion_claim_ordinal(spawn_ordinal), position};
    return true;
}
```

In `roll_ground_materials()`, preserve the ordinary-material branch and change only the secondary branch to one claim-bit decision, coupon first, potion second:

```cpp
if (claim_material_roll(coupon_ordinal)) {
    const bool abyss_monster = stable_state_.current_room.is_abyss
        && stable_state_.abyss.lifecycle == abyss::AbyssLifecycle::started;
    const auto coupon = roll_coupon_drop(stable_state_.current_room.seed,
        spawn_ordinal, stable_state_.current_room.depth,
        event.affix_score, abyss_monster);
    if (coupon.has_value()) {
        static_cast<void>(place_ground_material(coupon_ordinal,
            GroundMaterialSource::monster_coupon, position, *coupon));
    } else if (roll_health_potion_drop(
            stable_state_.current_room.seed, spawn_ordinal)) {
        static_cast<void>(place_ground_health_potion(spawn_ordinal, position));
    }
}
```

In `build_dungeon_snapshot()`, scan the fixed potion array in ascending index order, pack active entries, and copy `health_potion_pickup_receipt_`. Do not expose pending claim ordinal until Task 6.

## Tests to write first

Extend the existing health-potion suite by exactly four focused cases:

```cpp
arpg::test::Failure coupon_has_priority_and_common_material_can_coexist_with_potion() noexcept;
arpg::test::Failure adding_potions_does_not_change_existing_material_or_coupon_rolls() noexcept;
arpg::test::Failure ground_potion_pool_accepts_spawn_zero_and_191_without_overwrite() noexcept;
arpg::test::Failure potion_snapshot_is_sorted_by_spawn_ordinal_and_clears_on_room_exit() noexcept;
```

Use the existing test-access seam to push reward-eligible `CombatEventKind::defeated` events. Find deterministic seeds rather than hard-coding unverified guesses. Freeze these assertions:

```cpp
const dungeon::GroundMaterialSnapshot* find_ground_material_snapshot(
    const dungeon::DungeonSnapshot& snapshot,
    std::uint16_t ordinal) noexcept {
    for (std::uint16_t index = 0U;
         index < snapshot.ground_material_count; ++index) {
        if (snapshot.ground_materials[index].ordinal == ordinal) {
            return &snapshot.ground_materials[index];
        }
    }
    return nullptr;
}
```

For each relevant event:

```cpp
if (coupon.has_value()) {
    ARPG_REQUIRE(snapshot.ground_health_potion_count == 0U);
    const auto* secondary = find_ground_material_snapshot(
        snapshot, health_potion_claim_ordinal(spawn));
    ARPG_REQUIRE(secondary != nullptr);
    ARPG_REQUIRE(secondary->source == GroundMaterialSource::monster_coupon);
} else if (roll_health_potion_drop(seed, spawn)) {
    ARPG_REQUIRE(snapshot.ground_health_potion_count == 1U);
    ARPG_REQUIRE(snapshot.ground_health_potions[0].claim_ordinal
        == health_potion_claim_ordinal(spawn));
}
```

Test requirements:

- Locate/freeze deterministic examples of coupon hit, potion hit, and common-material plus potion coexistence.
- Before integration, store a batch of `roll_material_drop()` and `roll_coupon_drop()` results; after pushing kills, compare every stored result to prove no RNG disturbance.
- Place spawn 0 and 191, then fill all 192 slots. Verify no allocation while filling, snapshot packing, or stable scanning (use `allocation_probe.hpp`).
- Reinsert an occupied slot; old object bytes/fields remain unchanged and saturation diagnostic increments exactly once.
- Install in non-sorted order and verify snapshot order by spawn ordinal.
- Trigger real room exit/transition/abandon seam and verify potions clear; do not clear merely on room-clear success.

Register four cases. Update focused total `3 -> 7` and full dungeon total `290 -> 294` (or exactly +4 from current reviewed baseline).

## TDD and verification

Read and follow:
`E:\codex1\.codex\plugins\cache\openai-curated-remote\superpowers\6.1.1\skills\test-driven-development\SKILL.md`

1. Add all four tests first; run focused suite and capture a valid RED (no potion placement/snapshot yet).
2. Implement the smallest fixed-pool/drop/snapshot/clear integration.
3. Run:

```powershell
cmake --build --preset windows-msvc-core-debug --target arpg_dungeon_tests -- -j1
$env:ARPG_HEALTH_POTION_ONLY='1'
& .\out\build\windows-msvc-core-debug\bin\arpg_dungeon_tests.exe
Remove-Item Env:ARPG_HEALTH_POTION_ONLY
ctest --preset windows-msvc-core-debug -R '^dungeon\.units$' --output-on-failure
```

Expected: focused `7 cases, 0 failures`; full `294 cases, 0 failures`.

## Hard boundaries

- Coupon wins over potion. Potion is evaluated only after coupon miss, using the same single secondary claim ordinal/bit.
- Ordinary materials remain independent and unchanged.
- Do not consume/pick up/heal/save potions yet.
- No dynamic allocation, no vector, no persistence codec changes, no `MaterialId` mapping.
- Do not alter room sizes, monsters, player rules, or earlier tasks.
- Do not touch main worktree `E:\game`.

## Completion

Commit only the seven allowed files with:
`feat: drop and snapshot ground health potions`

Write the unique report with status, commit hash, valid RED/GREEN evidence, changed files, and concerns. Return `DONE`, `DONE_WITH_CONCERNS`, `NEEDS_CONTEXT`, or `BLOCKED`.
