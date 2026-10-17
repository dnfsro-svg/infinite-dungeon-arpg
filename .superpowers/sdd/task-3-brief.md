### Task 3: Fixed runtime pools and snapshot migration

**Files:**
- Create: `src/combat/monster_pool.hpp`
- Create: `src/combat/monster_pool.cpp`
- Modify: `src/combat/combat_types.hpp`
- Modify: `src/combat/combat_world.hpp`
- Modify: `src/combat/combat_world.cpp`
- Modify: `src/combat/CMakeLists.txt`
- Create: `tests/combat/monster_pool_tests.cpp`
- Modify: `tests/combat/attack_state_tests.cpp`
- Modify: `tests/combat/break_stress_tests.cpp`
- Modify: `tests/combat/combat_config_tests.cpp`
- Modify: `tests/combat/dummy_reaction_tests.cpp`
- Modify: `tests/combat/hit_resolution_tests.cpp`
- Modify: `tests/combat/movement_jump_tests.cpp`
- Modify: `tests/combat/combat_test_support.hpp`
- Modify: `tests/combat/CMakeLists.txt`
- Modify: `tests/combat/combat_test_main.cpp`

**Interfaces:**
- Consumes: Tasks 1-2 `MonsterId`, `MonsterSpawnSpec`, `EncounterWave`.
- Produces fixed runtime slot APIs:

```cpp
struct MonsterHandle final { std::uint16_t index{0xFFFF}; std::uint16_t generation{}; };

class MonsterPool final {
public:
    void clear() noexcept;
    [[nodiscard]] std::optional<MonsterHandle> spawn(
        MonsterId id, Vec3 position) noexcept;
    [[nodiscard]] bool destroy(MonsterHandle) noexcept;
    [[nodiscard]] std::size_t active_count() const noexcept;
    [[nodiscard]] MonsterRuntime* get(MonsterHandle) noexcept;
    [[nodiscard]] const std::array<MonsterRuntime, kMonsterCapacity>& slots() const noexcept;
};
```

`CombatSnapshot` changes from a fixed `dummies[3]` presentation to `monsters[96]` plus `monster_count`. All existing combat tests are migrated in this task to use active monster slots; no duplicate legacy snapshot is retained.

- [ ] **Step 1: Write RED pool tests**

Test first-free deterministic allocation, stale handle rejection after destroy/reuse, full-capacity rejection, clear reset, and snapshot active count. Fill all 96 slots and assert allocation 97 returns `nullopt` without modifying existing slots.

- [ ] **Step 2: Verify RED**

Run:

```powershell
.\scripts\Build.ps1 -Preset windows-msvc-core-debug
```

Expected: missing `MonsterPool` and snapshot fields fail compilation.

- [ ] **Step 3: Implement pool and migrate CombatWorld initialization**

`MonsterRuntime` must contain only bounded state: active/generation, definition ID, spawn/position/velocity, facing, reaction/armor, AI phase/ticks, HP/break/shield, hit-stop, and owner-linked transient counters. No owning pointers or vectors.

Add:

```cpp
struct CombatEncounterConfig final {
    Vec3 player_spawn{};
    Facing initial_facing{Facing::right};
    EncounterWave wave{};
    bool reset_player_health{true};
};

explicit CombatWorld(CombatEncounterConfig config) noexcept;
[[nodiscard]] bool load_wave(const EncounterWave&) noexcept;
[[nodiscard]] std::size_t active_monster_count() const noexcept;
```

Keep the old `CombatLabConfig` constructor temporarily as a test compatibility adapter until Task 8 removes its dungeon use.

- [ ] **Step 4: Run combat tests**

Run:

```powershell
.\scripts\Test.ps1 -Preset windows-msvc-core-debug
.\out\build\windows-msvc-core-debug\bin\arpg_combat_tests.exe
```

Expected: all pool tests and all legacy combat tests pass; no persistent allocation regression.

- [ ] **Step 5: Commit**

```powershell
git add src/combat tests/combat
git commit -m "feat: add fixed monster runtime pool"
```

---

