### Task 4: Integrate density with session, abyss, snapshot, and reload

**Files:**
- Modify: `src/dungeon/dungeon_types.hpp`
- Modify: `src/dungeon/dungeon_session.hpp`
- Modify: `src/dungeon/dungeon_session.cpp`
- Modify: `src/dungeon/dungeon_snapshot.cpp`
- Modify: `tests/dungeon/dungeon_wave_tests.cpp`
- Modify: `tests/dungeon/dungeon_lifecycle_tests.cpp`
- Modify: `tests/dungeon/dungeon_abyss_stress_tests.cpp`
- Modify: `tests/dungeon/dungeon_death_lifecycle_tests.cpp`
- Modify: `tests/dungeon/dungeon_transaction_tests.cpp`

**Interfaces:**
- Produces in `DungeonSnapshot`: `RoomDensityAffix density_affix`, `std::uint8_t base_monster_count`, `std::uint8_t initial_monster_count`.
- Produces in `DungeonEncounterDiagnostics`: exact initial count plus summed threat budget.
- Consumes: `roll_room_density(current_room.seed, current_room.is_abyss)`.

- [ ] **Step 1: Write RED lifecycle and reload tests**

```cpp
const DungeonSnapshot entered = session.snapshot();
ARPG_REQUIRE(entered.wave_count == 1U);
ARPG_REQUIRE(entered.initial_monster_count >= 12U);
ARPG_REQUIRE(entered.initial_monster_count <= 30U);
ARPG_REQUIRE(entered.combat->monster_count == entered.initial_monster_count);

DungeonSession reloaded{rules, stable_state};
const DungeonSnapshot replay = reloaded.snapshot();
ARPG_REQUIRE(replay.density_affix == entered.density_affix);
ARPG_REQUIRE(replay.base_monster_count == entered.base_monster_count);
ARPG_REQUIRE(replay.initial_monster_count == entered.initial_monster_count);
```

For an abyss seed, require `initial == (base*3+1)/2`, `initial<=45`, and all monsters present before the first combat tick. After defeating the last target, require direct transition toward room clear and no `wave_delay` phase.

- [ ] **Step 2: Run dungeon tests to verify RED**

Run `ctest --test-dir out/build/windows-msvc-debug -R dungeon.units --output-on-failure`. Expected: compile FAIL on snapshot fields/new director calls.

- [ ] **Step 3: Store only transient derived density state**

Add `RoomDensityRoll room_density_{};` to `DungeonSession`. Recompute it in normal construction, abyss construction, death retreat, and reload from stable room seed/is-abyss. Do not add it to `checkpoint::RoomDescriptor`, `DungeonRunState`, or persistence serialization.

- [ ] **Step 4: Build normal and abyss encounter requests**

```cpp
const EncounterBuildRequest request{
    room.seed, room.depth, room.ecology, room.entry,
    room.has_hole, room_density_.monster_count};
const EncounterPlanResult plan = room.is_abyss
    ? build_abyss_encounter_plan(request, rules_.encounter)
    : build_encounter_plan(request, rules_.encounter);
```

Load `plan.waves[0]` once. Keep legacy phase enum values for source compatibility, but ensure production plans never enter `wave_delay` and never call `start_next_wave`.

- [ ] **Step 5: Publish immutable snapshot values**

Populate `density_affix`, `base_monster_count`, and `initial_monster_count` from `room_density_`/plan. Set `wave_index=0`, `wave_count=1`, and `wave_delay_ticks=0` for active Stage 19 rooms. Preserve `remaining_targets()` as the allocation-free live monster count.

- [ ] **Step 6: Verify stable save bytes**

Run existing persistence golden/round-trip tests before and after constructing density-enabled rooms. Expected: serialized checkpoint version and byte layout remain unchanged; reloaded derived density equals the original.

- [ ] **Step 7: Run dungeon and persistence suites**

```powershell
cmake --build --preset windows-msvc-debug --target arpg_dungeon_tests arpg_persistence_tests
ctest --test-dir out/build/windows-msvc-debug -R "dungeon.units|persistence.units" --output-on-failure
```

Expected: PASS.

- [ ] **Step 8: Commit**

```powershell
git add src/dungeon tests/dungeon tests/persistence
git commit -m "feat: integrate density encounters with dungeon lifecycle"
```

---

