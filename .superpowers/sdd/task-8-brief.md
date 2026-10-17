### Task 8: Dungeon wave lifecycle integration

**Files:**
- Modify: `src/dungeon/dungeon_rules.hpp`
- Modify: `src/dungeon/dungeon_rules.cpp`
- Modify: `src/dungeon/dungeon_types.hpp`
- Modify: `src/dungeon/dungeon_session.hpp`
- Modify: `src/dungeon/dungeon_session.cpp`
- Modify: `src/dungeon/room_combat_template.hpp`
- Modify: `src/dungeon/room_combat_template.cpp`
- Create: `tests/dungeon/dungeon_wave_tests.cpp`
- Modify: `tests/dungeon/CMakeLists.txt`
- Modify: `tests/dungeon/dungeon_test_main.cpp`
- Modify: existing dungeon lifecycle/transaction/stress helpers that assume exactly three dummies.

**Interfaces:**
- `DungeonRules` contains `EncounterDirectorConfig encounter{}` and validation delegates to director validation.
- `DungeonSnapshot` adds `wave_index`, `wave_count`, `wave_delay_ticks`, `remaining_targets`, and encounter diagnostics.
- `RoomPhase` adds `wave_delay` between combat waves.

- [ ] **Step 1: Write RED wave tests**

```cpp
arpg::test::Failure two_wave_room_keeps_exits_closed_until_last_wave() noexcept {
    DungeonRules rules = rules_with_budget(16U);
    DungeonSession session{rules, state_for_seed(two_wave_seed())};
    clear_current_wave(session);
    ARPG_REQUIRE(session.snapshot().phase == RoomPhase::wave_delay);
    ARPG_REQUIRE(!any_exit_open(session.snapshot()));
    tick_wave_delay(session);
    ARPG_REQUIRE(session.snapshot().wave_index == 1U);
    clear_current_wave(session);
    session.tick({});
    ARPG_REQUIRE(session.snapshot().phase == RoomPhase::awaiting_exit);
    ARPG_REQUIRE(all_exits_open(session.snapshot()));
    return {};
}
```

Add tests that a hole stays sealed through wave delay, reset reproduces the same plan, reload reproduces the same plan, committed room entry restores player HP, and abyss uses the normal director config.

- [ ] **Step 2: Verify RED**

Expected: session clears after wave one and missing snapshot fields fail.

- [ ] **Step 3: Integrate director and waves**

`construct_current_room()` builds the plan from `stable_state_.current_room.seed/depth/ecology`, constructs CombatWorld from wave 0, and records plan/wave index. When active monsters reach zero, either start fixed `kWaveDelayTicks=45` or enter `cleared`. During delay, no combat simulation or exit requests are accepted. Loading wave 1 does not reset player HP; committed new-room construction does.

- [ ] **Step 4: Update dungeon test support and run core-only CTest**

Replace three-dummy helpers with nearest-living-monster helpers over active snapshot slots. Expected: all dungeon progression, transaction, navigation, lifecycle, and wave cases pass.

- [ ] **Step 5: Commit**

```powershell
git add src/dungeon tests/dungeon
git commit -m "feat: integrate deterministic monster waves"
```

---

