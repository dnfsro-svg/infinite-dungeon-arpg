### Task 2: Deterministic budget encounter director

**Files:**
- Create: `src/dungeon/encounter_director.hpp`
- Create: `src/dungeon/encounter_director.cpp`
- Modify: `src/dungeon/dungeon_rules.hpp`
- Modify: `src/dungeon/dungeon_rules.cpp`
- Modify: `src/dungeon/CMakeLists.txt`
- Create: `tests/dungeon/encounter_director_tests.cpp`
- Modify: `tests/dungeon/CMakeLists.txt`
- Modify: `tests/dungeon/dungeon_test_main.cpp`

**Interfaces:**
- Consumes: Task 1 `MonsterId`, `MonsterDefinition`, catalog lookup, deterministic RNG, and `checkpoint::DungeonElement`.
- Produces:

```cpp
struct EncounterDirectorConfig final {
    std::uint8_t base_budget{8};
    std::uint8_t depth_step{5};
    std::uint8_t budget_per_step{1};
    std::uint8_t max_budget{24};
    std::uint8_t two_wave_threshold{12};
    std::uint8_t matching_ecology_weight{4};
    std::uint8_t off_ecology_weight{1};
    std::uint8_t normal_high_priority_limit{1};
    std::uint8_t high_budget_priority_limit{2};
    std::uint8_t ranged_limit{4};
    std::uint8_t support_limit{2};
    std::uint8_t ground_hazard_limit{3};
};

struct RoomEncounterPlan final {
    std::array<combat::EncounterWave, combat::kEncounterWaveCapacity> waves{};
    std::uint8_t wave_count{};
    std::uint8_t total_budget{};
};

EncounterPlanResult build_encounter_plan(std::uint64_t room_seed,
    std::uint64_t depth, checkpoint::DungeonElement ecology,
    const EncounterDirectorConfig&) noexcept;
```

`EncounterDirectorConfig` is declared in `dungeon_rules.hpp`; `encounter_director.hpp` includes that header and returns the existing `DungeonFault` type. `dungeon_rules.hpp` must not include `encounter_director.hpp`, preventing an include cycle.

- [ ] **Step 1: Write RED tests for budget math, determinism, ecology and constraints**

```cpp
arpg::test::Failure director_budget_is_bounded_and_depth_driven() noexcept {
    EncounterDirectorConfig config{};
    ARPG_REQUIRE(encounter_budget(1U, config) == 8U);
    ARPG_REQUIRE(encounter_budget(5U, config) == 8U);
    ARPG_REQUIRE(encounter_budget(6U, config) == 9U);
    ARPG_REQUIRE(encounter_budget(10000U, config) == 24U);
    return {};
}

arpg::test::Failure encounter_plan_is_deterministic_and_legal() noexcept {
    const auto a = build_encounter_plan(0xA11CEULL, 26U,
        DungeonElement::lightning, EncounterDirectorConfig{});
    const auto b = build_encounter_plan(0xA11CEULL, 26U,
        DungeonElement::lightning, EncounterDirectorConfig{});
    ARPG_REQUIRE(a.fault == DungeonFault::none);
    ARPG_REQUIRE(same_encounter_plan(a.plan, b.plan));
    ARPG_REQUIRE(encounter_plan_legal(a.plan, EncounterDirectorConfig{}));
    ARPG_REQUIRE(a.plan.wave_count == 2U);
    return {};
}
```

Add a 4096-seed ecology A/B test: run the same seeds once with `matching_ecology_weight=1` and once with the approved default `4`, then assert the mean number of preferred-ecology spawns increases by a positive margin. Do not assert raw matching count exceeds off-element count, because role costs and support/direct-target safety constraints legitimately alter candidate counts.

- [ ] **Step 2: Verify RED**

Run:

```powershell
.\scripts\Build.ps1 -Preset windows-msvc-core-debug
```

Expected: compilation fails on the missing director interfaces.

- [ ] **Step 3: Implement budget calculation and constrained deterministic selection**

Use a dedicated domain constant:

```cpp
constexpr std::uint64_t kEncounterDirectorDomain = 0x454E434F554E5434ULL;
```

Algorithm:

1. Validate all config fields are nonzero where required and limits do not exceed fixed capacities.
2. Calculate the bounded budget with checked integer operations.
3. Set wave count to 2 only when budget is greater than 12; split budget deterministically with the first wave receiving the extra point.
4. For each wave, start with the cheapest `direct_target` candidate that fits.
5. Build a fixed candidate array from catalog entries that fit remaining budget and tag limits; weight matching ecology by 4 and off ecology by 1.
6. Select with `DeterministicRng::next_bounded(total_weight)`; stop when no legal candidate fits.
7. If no legal direct target was produced, replace the wave with a deterministic `chaos_chaser` safe fallback.
8. Generate spawn positions from a separate encounter-position substream and clamp them to the existing room bounds.

- [ ] **Step 4: Run dungeon and full core-only tests**

Run:

```powershell
.\scripts\Test.ps1 -Preset windows-msvc-core-debug
.\out\build\windows-msvc-core-debug\bin\arpg_dungeon_tests.exe
```

Expected: director cases pass, dungeon deterministic tests pass, all 12 core-only CTest entries pass.

- [ ] **Step 5: Commit**

```powershell
git add src/dungeon tests/dungeon
git commit -m "feat: add deterministic encounter director"
```

---

