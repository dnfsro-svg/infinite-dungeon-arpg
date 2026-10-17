# Task 6 Brief — Atomic distance-free potion auto-use and room-clear folding

## Scope

Implement only Task 6 in `E:\game\.worktrees\balance-hp-potions` after Task 5 review.

Allowed files:

- `tests/dungeon/dungeon_health_potion_tests.cpp`
- `tests/dungeon/dungeon_test_support.hpp`
- `tests/dungeon/dungeon_test_main.cpp`
- `src/dungeon/dungeon_session.hpp`
- `src/dungeon/dungeon_session.cpp`
- `src/dungeon/dungeon_transition.cpp`
- `src/dungeon/dungeon_snapshot.cpp`

Report: `.superpowers/sdd/balance-hp-task-6-report.md` (never modify tracked `.superpowers/sdd/task-6-report.md`).

## Authoritative semantics

- Auto-use is distance-free and scans active potions in stable ascending spawn order.
- Player must be alive, have a combat instance, be in an item-request-eligible phase, have no other pending save, and have HP `<= 75%` by exact integer comparison.
- HP `751/1000` stays grounded; `750/1000` and below may request. A potion 100 units away behaves exactly like a nearby one.
- Each potion heals 25% of actual max HP, ceil-rounded, capped by missing HP, only after an exact persisted commit.
- A single pickup marks its secondary claim bit in a new `health_potion_pickup` pending transaction. No runtime HP, ground object, or success receipt changes before exact commit.
- `not_committed`: restore phase, no side effects, deterministic retry. `indeterminate`: fault, no side effects. Cache/generation/kind/replaced-ground mismatches fault before side effects.
- A durable commit that crashes before runtime resolve must not replay healing/receipt after reload; the persisted claim bit prevents rediscovery.
- On a final kill at low HP, eligible potion claims are folded into the same room-clear/abyss-clear transaction, together with common-material claims. Do not create a second save.
- Clear batch selects ascending spawns, up to 4, stopping once projected HP becomes strictly greater than 75%. Example from 1/1000 with installed 9,2,6,3 selects exactly `{2,3,6}` and leaves 9.
- High-HP final kill leaves potion grounded through room-clear; successful room transition later clears it.
- Abyss clear order: clear abyss rule preserving resource ratio first, then apply potion using post-clear actual max HP, then publish room clear.

## Private APIs

Add:

```cpp
[[nodiscard]] RequestResult request_health_potion_pickup(
    std::uint16_t spawn_ordinal) noexcept;
[[nodiscard]] bool pending_health_potion_cache_consistent() const noexcept;
void apply_committed_health_potions(
    const PendingHealthPotionClaim& claim, bool room_clear) noexcept;
[[nodiscard]] bool has_claimable_health_potion() const noexcept;
[[nodiscard]] bool append_clear_health_potion_claims(
    PendingSave& pending) noexcept;
```

## Single-potion request contract

Use this order and shape; never read potion position:

```cpp
RequestResult DungeonSession::request_health_potion_pickup(
    std::uint16_t spawn_ordinal) noexcept {
    if (!pending_item_cache_consistent()
            || !pending_material_cache_consistent()
            || !pending_health_potion_cache_consistent()) {
        enter_fault(DungeonFault::save_receipt_mismatch);
        return RequestResult::faulted;
    }
    if (!item_request_phase(phase_) || pending_save_.has_value()
            || !combat_.has_value()
            || spawn_ordinal >= ground_health_potions_.size()) {
        return RequestResult::rejected;
    }
    const GroundHealthPotion& ground = ground_health_potions_[spawn_ordinal];
    const combat::CombatSnapshot snapshot = combat_->snapshot();
    if (!ground.active || ground.spawn_ordinal != spawn_ordinal
            || ground.claim_ordinal != health_potion_claim_ordinal(spawn_ordinal)
            || combat_->death_snapshot().has_value()
            || !health_potion_auto_use_eligible(
                snapshot.player.hp, snapshot.player.max_hp)) {
        return RequestResult::rejected;
    }
    if (bit_is_set(stable_state_.item_ownership.material_claimed_drop_bits,
            ground.claim_ordinal)) {
        enter_fault(DungeonFault::invalid_item_state);
        return RequestResult::faulted;
    }
    if (stable_state_.commit_generation
            == (std::numeric_limits<std::uint64_t>::max)()) {
        enter_fault(DungeonFault::commit_generation_overflow);
        return RequestResult::faulted;
    }
    PendingSave& pending = pending_save_.prepare();
    if (!copy_run_state_reusing_items(pending.next_state, stable_state_)) {
        pending_save_.reset();
        return RequestResult::rejected;
    }
    ++pending.next_state.commit_generation;
    set_bit(pending.next_state.item_ownership.material_claimed_drop_bits,
        ground.claim_ordinal);
    pending.kind = PendingSaveKind::health_potion_pickup;
    pending.expected_generation = pending.next_state.commit_generation;
    pending.transition = TransitionKind::none;
    pending.direction = ExitDirection::none;
    pending.resume_phase = phase_;
    pending.pickup_ordinal = spawn_ordinal;
    pending.death_snapshot.reset();
    pending.reinforcement_receipt.reset();
    pending.health_potion_claim = PendingHealthPotionClaim{
        {{spawn_ordinal, 0U, 0U, 0U}}, 1U,
        snapshot.player.hp, snapshot.player.max_hp};
    phase_ = RoomPhase::committing;
    return RequestResult::accepted;
}
```

`pending_health_potion_cache_consistent()` must verify:

- kind and optional presence agree;
- count is 1..4;
- spawns strictly increase;
- every referenced ground object is active and structurally correct;
- each stable claim bit is 0 and each next-state bit is 1;
- expected HP/max HP equal the current combat snapshot;
- for a single pickup, ownership is unchanged except generation and the corresponding claim bit.

Call this checker beside all existing pending-cache checks in `commit_pending_save()`.

At the beginning of `request_nearby_pickups()`, before equipment/material distance scans:

```cpp
for (std::uint16_t spawn = 0U;
     spawn < ground_health_potions_.size(); ++spawn) {
    if (!ground_health_potions_[spawn].active) continue;
    const RequestResult result = request_health_potion_pickup(spawn);
    if (result != RequestResult::rejected) return;
}
```

Preserve all existing equipment/material distance behavior.

After exact commit validation and `publish_run_state_reusing_items()`, copy the pending potion claim before reset and apply it only after stable publish:

```cpp
void DungeonSession::apply_committed_health_potions(
    const PendingHealthPotionClaim& claim, bool room_clear) noexcept {
    HealthPotionPickupReceipt receipt{};
    receipt.valid = true;
    receipt.room_clear = room_clear;
    receipt.commit_generation = stable_state_.commit_generation;
    for (std::uint8_t index = 0U; index < claim.count; ++index) {
        const std::uint16_t spawn = claim.spawn_ordinals[index];
        receipt.restored_hp += combat_->restore_player_health_percent(
            kHealthPotionRestoreBp);
        ground_health_potions_[spawn] = {};
        ++receipt.consumed_count;
    }
    health_potion_pickup_receipt_ = receipt;
}
```

Single-pickup success resumes `resume_phase`. Snapshot exposes `pending_health_potion_spawn_ordinal` only for `health_potion_pickup`.

## Room-clear folding

`append_clear_health_potion_claims()` obtains the current combat snapshot. If dead or above threshold, return true with no optional. Otherwise scan ascending active spawns, select at most four, and project each heal using:

```cpp
const int restore = combat::scale_basis_points(max_hp,
    kHealthPotionRestoreBp, combat::BasisPointRounding::ceil);
projected_hp = (std::min)(max_hp, projected_hp + restore);
```

Set each selected secondary claim bit in `pending.next_state`; stop once projected HP is strictly above 75%.

Change direct room-clear publish condition to:

```cpp
if (!started_abyss && !has_ground_materials()
        && !has_claimable_health_potion()) {
    settle_room_experience();
    publish_room_clear();
    return;
}
```

When constructing clear pending, run both `vacuum_room_materials()` and `append_clear_health_potion_claims()`. Merge potion ordinals into the expected claimed-bit set inside `pending_material_cache_consistent()`; potion claims must not alter material count/discovery bits.

For exact abyss-clear commit:

```cpp
if (abyss_clear_commit) {
    combat_->clear_abyss_rule_preserving_resources();
}
if (committed_health_claim.has_value()) {
    apply_committed_health_potions(*committed_health_claim, true);
}
publish_room_clear();
```

Remove only selected potion slots, never the entire pool.

## Tests and phased TDD

Read and follow:
`E:\codex1\.codex\plugins\cache\openai-curated-remote\superpowers\6.1.1\skills\test-driven-development\SKILL.md`

### Phase A: four request/commit tests

Write first:

```cpp
potion_above_threshold_stays_grounded_but_75_percent_ignores_distance();
potion_auto_use_scans_stable_spawn_order_and_rechecks_after_commit();
dead_player_or_death_snapshot_never_requests_or_consumes_potion();
committed_pickup_heals_caps_removes_and_publishes_exact_receipt();
```

Freeze:

- 751 creates no pending; later setting 750 in the same `awaiting_exit` room creates one next tick.
- 750/749 create pending; 100-unit distance is irrelevant.
- Install 9,6,2 and verify pending spawn is 2.
- Exact commit from 750/1000 produces 1000 HP, receipt `restored_hp=250`, `consumed_count=1`, valid, correct generation, and removes only spawn 2.
- Dead/death snapshot/no combat/ineligible phase leave pending, HP, ground objects, and receipt unchanged.
- Allocation count is unchanged across scan and exact resolve.

Update focused 7->11, full 294->298. Obtain RED before production changes, then GREEN `11 cases, 0 failures`.

### Phase B: three rollback/cache/crash tests

Write first:

```cpp
failed_or_indeterminate_save_never_heals_removes_or_reports_success();
mismatched_pending_or_replaced_ground_faults_before_health_side_effects();
committed_claim_without_runtime_resolve_does_not_replay_after_reload();
```

Freeze:

- `not_committed`: phase restored; HP/ground/stable claim/receipt unchanged; retry produces identical next state.
- `indeterminate`: faulted; HP/ground/stable claim/receipt unchanged.
- wrong generation, wrong kind, or replaced ground -> `save_receipt_mismatch`, no heal.
- Real SaveStore commit without runtime resolve, then reconstruct: same kill does not rediscover potion, bit remains 1, no pickup receipt, no old-room heal replay.

Update focused 11->14, full 298->301. Demonstrate RED for at least a missing cache/replaced-ground check, then GREEN `14/0`.

### Phase C: four room-clear tests

Write first:

```cpp
final_kill_low_health_folds_sorted_potions_into_clear_transaction();
clear_batch_selects_only_until_health_is_strictly_above_75_percent();
final_kill_high_health_keeps_potion_until_room_transition();
abyss_clear_potion_uses_post_clear_actual_max_health();
```

Freeze:

- Low-HP final kill creates one `room_clear` or `abyss_clear` pending containing sorted potion claim(s); HP and ground remain unchanged before commit.
- Test both normal and started abyss. Include a common material and prove material claim plus potion secondary claim coexist in the same next state.
- From 1/1000 with installed 9,2,6,3 select exactly `{2,3,6}` (1->251->501->751), leaving 9.
- Failure restores entire transaction; next fixed frame regenerates the same pending.
- High-HP final kill includes no potion claim; potion remains after room clear and disappears only after successful room transition.
- With `life_sacrifice`, exact abyss-clear commit clears the rule first and heals from the post-clear actual max HP.

Update focused 14->18 and full 301->305. Obtain RED, then GREEN.

## Final verification

```powershell
cmake --build --preset windows-msvc-core-debug --target arpg_dungeon_tests -- -j1
$env:ARPG_HEALTH_POTION_ONLY='1'
& .\out\build\windows-msvc-core-debug\bin\arpg_dungeon_tests.exe
Remove-Item Env:ARPG_HEALTH_POTION_ONLY
ctest --preset windows-msvc-core-debug -R '^dungeon\.units$' --output-on-failure
```

Expected focused `18 cases, 0 failures`; full `305 cases, 0 failures`.

## Hard boundaries

- No distance check for potions; preserve distance checks for other loot.
- Never mutate runtime HP/ground/receipt before exact commit.
- No compensation side effects on `not_committed`/`indeterminate`.
- No replay after durable commit and crash.
- Claim ordinals reuse existing persisted material-claim bits; no codec/version change and no MaterialId/inventory.
- Maximum batch size exactly 4; stable ascending order.
- Do not touch UI/assets (Task 7), monster/room/player balance, or main worktree.

## Completion

Commit only the seven allowed files with:
`feat: atomically auto-consume health potions`

Write unique report with per-phase RED/GREEN evidence, final hash, files, concerns. Return `DONE`, `DONE_WITH_CONCERNS`, `NEEDS_CONTEXT`, or `BLOCKED`.
