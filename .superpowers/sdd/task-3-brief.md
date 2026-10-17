### Task 3: Generate an exact single-batch encounter

**Files:**
- Modify: `src/dungeon/encounter_director.hpp`
- Modify: `src/dungeon/encounter_director.cpp`
- Modify: `src/dungeon/dungeon_rules.hpp`
- Modify: `src/dungeon/dungeon_rules.cpp`
- Modify: `src/dungeon/encounter_budget.cpp`
- Modify: `tests/dungeon/encounter_director_tests.cpp`
- Modify: `tests/dungeon/dungeon_rules_tests.cpp`
- Modify: `tests/dungeon/stage9_validation_fixture.cpp`
- Modify: `tests/dungeon/stage10_formal_game_validation.cpp`
- Modify: `tests/dungeon/stage16_loot_reinforcement_simulation.cpp`
- Modify: `tests/dungeon/dungeon_stress_tests.cpp`
- Modify: `tests/dungeon/dungeon_lifecycle_tests.cpp`
- Modify: `tests/dungeon/dungeon_abyss_stress_tests.cpp`

**Interfaces:**
- Produces:

```cpp
struct EncounterBuildRequest final {
    std::uint64_t room_seed{};
    std::uint64_t depth{1U};
    checkpoint::DungeonElement ecology{checkpoint::DungeonElement::fire};
    checkpoint::EntrySide entry{checkpoint::EntrySide::initial};
    bool has_hole{};
    std::uint8_t target_monster_count{};
};

EncounterPlanResult build_encounter_plan(
    const EncounterBuildRequest&, const EncounterDirectorConfig&) noexcept;
EncounterPlanResult build_abyss_encounter_plan(
    const EncounterBuildRequest&, const EncounterDirectorConfig&) noexcept;
```

- Produces: `RoomEncounterPlan::initial_monster_count` and exactly one populated wave.
- Consumes: `make_combat_lab_config(request.entry, 1U)` for the player safe-zone center.

- [ ] **Step 1: Replace budget-oriented tests with exact-count RED tests**

```cpp
EncounterBuildRequest request{0xA11CEULL, 40U,
    DungeonElement::lightning, EntrySide::left, true, 30U};
const EncounterPlanResult result = build_encounter_plan(request, {});
ARPG_REQUIRE(result.fault == DungeonFault::none);
ARPG_REQUIRE(result.plan.wave_count == 1U);
ARPG_REQUIRE(result.plan.initial_monster_count == 30U);
ARPG_REQUIRE(result.plan.waves[0].spawn_count == 30U);
ARPG_REQUIRE(result.plan.waves[1].spawn_count == 0U);
```

For each spawn, require room containment and squared distance outside the player, four door, and active-hole exclusion circles. Across 4096 seeds require at least one valid position in each newly exposed band: X below -12, X above 12, Y below -5.5, Y above 5.5.

- [ ] **Step 2: Run the encounter suite to verify RED**

```powershell
cmake --build --preset windows-msvc-debug --target arpg_dungeon_tests
ctest --test-dir out/build/windows-msvc-debug -R dungeon.units --output-on-failure
```

Expected: compile FAIL on the missing request type/new signature, then encounter assertions FAIL until one batch and safe positions are implemented.

- [ ] **Step 3: Simplify director configuration to count-era safety fields**

```cpp
struct EncounterDirectorConfig final {
    std::uint8_t matching_ecology_weight{4};
    std::uint8_t off_ecology_weight{1};
    std::uint8_t high_priority_limit{2};
    std::uint8_t ranged_limit{4};
    std::uint8_t support_limit{2};
    std::uint8_t ground_hazard_limit{3};
};
```

Remove the obsolete depth budget fields, `encounter_budget`, and `abyss_encounter_legality_config`. Repurpose the private helper in `encounter_budget.cpp` as checked threat-cost accumulation used by the director. Keep `RoomEncounterPlan::total_budget` and `EncounterWave::spent_budget` as the summed threat-cost diagnostic; maximum is 180, which fits `std::uint8_t`.

- [ ] **Step 4: Implement count-first selection**

Set `wave_count=1` and `initial_monster_count=request.target_monster_count`. Append one direct-target fallback first, then repeatedly use the existing ecology-weighted candidate selection while enforcing safety tag limits until exact count is reached. The unrestricted `chaos_chaser` guarantees completion. Apply existing monster affixes after composition; abyss generation then supplements existing affixes exactly as before.

- [ ] **Step 5: Implement bounded safe-position sampling**

Draw X uniformly from 48,001 millisteps and Y from 22,001 millisteps. Reject excluded points for at most 32 attempts per spawn. If all attempts fail, scan a deterministic 1-unit grid from a seed-derived offset and take the first legal point. Return `invalid_rules` if no legal point exists; never truncate or reroll the room.

- [ ] **Step 6: Update legality checks**

Require `wave_count==1`, `initial_monster_count==waves[0].spawn_count`, count in `[12,45]`, exact threat sums, sequential ordinals, valid affixes, valid safe positions, at least one direct target, and all safety tag limits. Remove all budget-split and two-wave legality branches.

- [ ] **Step 7: Run focused and full dungeon suites**

Run Step 2, then:

```powershell
ctest --test-dir out/build/windows-msvc-debug -R dungeon.units --output-on-failure
```

Expected: PASS; the same input is byte-equivalent and 12, 16, 17, 22, 23, 30, and 45 counts are legal.

- [ ] **Step 8: Commit**

```powershell
git add src/dungeon tests/dungeon
git commit -m "feat: build exact single-batch encounters"
```

---

