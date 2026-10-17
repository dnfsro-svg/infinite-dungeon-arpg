### Task 2: Add deterministic room-density affixes

**Files:**
- Create: `src/dungeon/room_affix.hpp`
- Create: `src/dungeon/room_affix.cpp`
- Create: `tests/dungeon/room_affix_tests.cpp`
- Modify: `src/dungeon/CMakeLists.txt`
- Modify: `tests/dungeon/CMakeLists.txt`
- Modify: `tests/dungeon/dungeon_test_main.cpp`

**Interfaces:**
- Produces: `enum class RoomDensityAffix : std::uint8_t { crowded, dense, horde, count };`
- Produces: `struct RoomDensityRoll { RoomDensityAffix affix; std::uint8_t base_count; std::uint8_t monster_count; };`
- Produces: `RoomDensityRoll roll_room_density(std::uint64_t room_seed, bool is_abyss) noexcept;`
- Produces: `const RoomDensityDefinition* room_density_definition(RoomDensityAffix) noexcept;`

- [ ] **Step 1: Register the new test file and write RED tests**

```cpp
ARPG_REQUIRE(room_density_definition(RoomDensityAffix::crowded)->weight == 50U);
ARPG_REQUIRE(room_density_definition(RoomDensityAffix::crowded)->minimum == 12U);
ARPG_REQUIRE(room_density_definition(RoomDensityAffix::crowded)->maximum == 16U);
ARPG_REQUIRE(room_density_definition(RoomDensityAffix::dense)->weight == 35U);
ARPG_REQUIRE(room_density_definition(RoomDensityAffix::horde)->weight == 15U);

for (std::uint64_t seed = 0; seed < 10000U; ++seed) {
    const RoomDensityRoll normal = roll_room_density(seed, false);
    const RoomDensityRoll abyss = roll_room_density(seed, true);
    ARPG_REQUIRE(abyss.affix == normal.affix);
    ARPG_REQUIRE(abyss.base_count == normal.base_count);
    ARPG_REQUIRE(abyss.monster_count
        == static_cast<std::uint8_t>((normal.base_count * 3U + 1U) / 2U));
    ARPG_REQUIRE(abyss.monster_count <= 45U);
}
```

Also count all three affixes over 100,000 stable seeds and require absolute deviation below 1 percentage point; record whether every inclusive interval endpoint appears.

- [ ] **Step 2: Run the dungeon target to verify RED**

```powershell
cmake --build --preset windows-msvc-debug --target arpg_dungeon_tests
ctest --test-dir out/build/windows-msvc-debug -R dungeon.units --output-on-failure
```

Expected: compile FAIL because `room_affix.hpp` and its APIs do not exist.

- [ ] **Step 3: Implement the fixed catalog and domain-separated rolls**

```cpp
struct RoomDensityDefinition final {
    RoomDensityAffix id{RoomDensityAffix::crowded};
    std::uint8_t weight{};
    std::uint8_t minimum{};
    std::uint8_t maximum{};
};

constexpr std::array<RoomDensityDefinition, 3> kDefinitions{{
    {RoomDensityAffix::crowded, 50U, 12U, 16U},
    {RoomDensityAffix::dense, 35U, 17U, 22U},
    {RoomDensityAffix::horde, 15U, 23U, 30U},
}};
```

Use separate fixed 64-bit domains for affix selection and count selection. Select with `next_bounded(100U)`, select the inclusive count with `next_bounded(maximum-minimum+1)`, and compute abyss count as `(base_count * 3U + 1U) / 2U`. Do not accept configuration or depth as input.

- [ ] **Step 4: Prove deterministic output and no allocation**

Repeat the same seeds twice and compare `RoomDensityRoll` fields. Wrap 100,000 rolls with the existing allocation probe and require zero allocations.

- [ ] **Step 5: Run the dungeon suite to verify GREEN**

Run Step 2. Expected: PASS with the dungeon suite count updated in `dungeon_test_main.cpp`.

- [ ] **Step 6: Commit**

```powershell
git add src/dungeon/room_affix.* src/dungeon/CMakeLists.txt tests/dungeon/room_affix_tests.cpp tests/dungeon/CMakeLists.txt tests/dungeon/dungeon_test_main.cpp
git commit -m "feat: add deterministic room density affixes"
```

---

