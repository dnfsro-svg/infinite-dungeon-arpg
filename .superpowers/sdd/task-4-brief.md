### Task 4: Production Snapshot Metadata and Runtime Mapping

**Files:**
- Modify: `src/dungeon/dungeon_types.hpp`
- Modify: `src/dungeon/dungeon_snapshot.cpp`
- Modify: `src/platform/raylib/dungeon_runtime.hpp`
- Modify: `src/platform/raylib/dungeon_runtime.cpp`
- Modify: `src/platform/raylib/raylib_host.cpp`
- Modify: `tests/dungeon/dungeon_loot_drop_tests.cpp`
- Modify: `tests/platform/dungeon_runtime_tests.cpp`

**Interfaces:**
- Adds to `GroundItemSnapshot`: `std::uint8_t base_id`, `std::uint8_t item_level`.
- Adds to `DungeonSnapshot`: `std::optional<std::uint16_t> pending_pickup_ordinal`.
- Changes: `DungeonRuntime::fixed_tick(combat::MovementInput, dungeon::AutoPickupPolicy = {}) noexcept`.
- Produces: `dungeon::AutoPickupPolicy loot_pickup_policy(settings::LootFilterMode) noexcept` in the host-facing platform boundary.

- [ ] **Step 1: Write failing snapshot/runtime tests**

Require snapshots to match production ground item `base_id`, `item_level`, ordinal, source, rarity, and pending pickup ordinal. Require runtime with rare-only policy to leave a normal drop active while default runtime still picks it.

- [ ] **Step 2: Verify RED**

Build dungeon and platform test targets; expected compile failures for missing fields and runtime overload.

- [ ] **Step 3: Implement snapshot packing and runtime forwarding**

Pack only existing validated production fields. Set `pending_pickup_ordinal` only for `loot_pickup` and `abyss_reward_claim`; otherwise reset it. Forward the host's committed mode on every non-paused fixed tick.

- [ ] **Step 4: Verify GREEN and default compatibility**

Run dungeon snapshot/stress and platform runtime tests. Existing callers omitting policy must retain show-all behavior.

- [ ] **Step 5: Commit**

```powershell
git add src/dungeon src/platform/raylib/dungeon_runtime.* src/platform/raylib/raylib_host.cpp tests/dungeon tests/platform/dungeon_runtime_tests.cpp
git commit -m "feat: expose filtered ground loot snapshots"
```

