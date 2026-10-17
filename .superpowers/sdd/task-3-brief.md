### Task 3: Dungeon-Owned Automatic Pickup Policy

**Files:**
- Modify: `src/dungeon/dungeon_types.hpp`
- Modify: `src/dungeon/dungeon_session.hpp`
- Modify: `src/dungeon/dungeon_session.cpp`
- Modify: `src/dungeon/dungeon_transition.cpp`
- Modify: `tests/dungeon/dungeon_loot_drop_tests.cpp`
- Modify: `tests/dungeon/dungeon_abyss_reward_tests.cpp`
- Modify: `tests/dungeon/dungeon_stress_tests.cpp`

**Interfaces:**
- Produces: `struct AutoPickupPolicy { items::ItemRarity minimum_rarity{items::ItemRarity::normal}; }`.
- Produces: `bool auto_pickup_eligible(const GroundItem&, AutoPickupPolicy) noexcept`.
- Changes: `DungeonSession::tick(combat::MovementInput, AutoPickupPolicy = {}) noexcept`.
- Changes: `request_nearby_pickups(combat::Vec3, AutoPickupPolicy = {}) noexcept`.
- Preserves: `request_pickup(std::uint16_t)` ignores policy.

- [ ] **Step 1: Write failing policy and transaction tests**

Inject normal, magic, and rare ordinary ground items inside the pickup radius. Require `magic_or_better` to leave normal active and pick magic first; require `rare_only` to pick rare; require an ordinary normal plus normal abyss reward to pick the abyss reward. Require an explicit normal `request_pickup` to remain accepted.

```cpp
session.request_nearby_pickups(player,
    {items::ItemRarity::magic});
ARPG_REQUIRE(arpg::test::ground_items(session)[normal_ordinal].active);
ARPG_REQUIRE(session.pending_save_view()->pickup_ordinal == magic_ordinal);
```

- [ ] **Step 2: Build dungeon tests and verify RED**

Run `cmake --build out/build/windows-msvc-debug --target arpg_dungeon_tests`; expected compilation failure for the missing policy overload.

- [ ] **Step 3: Implement minimal eligibility and thread policy through tick**

```cpp
bool auto_pickup_eligible(
    const GroundItem& ground, AutoPickupPolicy policy) noexcept {
    if (!ground.active) return false;
    if (ground.source == GroundItemSource::abyss_chest) return true;
    return ground.source == GroundItemSource::monster_drop
        && static_cast<std::uint8_t>(ground.item.rarity)
            >= static_cast<std::uint8_t>(policy.minimum_rarity);
}
```

Call the policy-aware nearby scan at the same point before exit attempts. Do not add policy checks to explicit pickup transactions.

- [ ] **Step 4: Verify focused and deterministic tests GREEN**

Run filtered `dungeon.loot`, `dungeon.abyss_reward`, and `dungeon.stress` suites. Expected: ordering, persistence transactions, room exit behavior, and deterministic traces pass.

- [ ] **Step 5: Commit**

```powershell
git add src/dungeon tests/dungeon
git commit -m "feat: filter automatic loot pickup"
```

