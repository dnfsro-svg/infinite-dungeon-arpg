### Task 1: Combat Target, Stable Types, Attack Catalog, and Architecture Gate

**Worktree:** `E:\game\.worktrees\m01-combat-core`

**Files:**
- Create: `src/combat/CMakeLists.txt`
- Create: `src/combat/combat_types.hpp`
- Create: `src/combat/attack_catalog.hpp`
- Create: `src/combat/attack_catalog.cpp`
- Create: `tests/combat/CMakeLists.txt`
- Create: `tests/combat/combat_test_main.cpp`
- Create: `tests/combat/attack_catalog_tests.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/platform/CMakeLists.txt`
- Modify: `tests/platform/core_boundary_test.cmake`

**Interfaces:**
- Consumes: `arpg_core` and `tests/core/test_framework.hpp`.
- Produces: `Vec3`、`Aabb`、六个 AttackId、帧段/反馈枚举、`AttackDefinition`、`find_attack_definition`、`attack_phase_at`、`validate_attack_catalog` and `arpg_combat`.

- [ ] **Step 1: Write the first RED catalog suite**

Create three cases that include the missing `combat/attack_catalog.hpp` and assert:

```cpp
using namespace arpg::combat;
ARPG_REQUIRE(kAttackCount == 6);
ARPG_REQUIRE(find_attack_definition(AttackId::j1) != nullptr);
ARPG_REQUIRE(find_attack_definition(AttackId::air_j) != nullptr);
ARPG_REQUIRE(find_attack_definition(AttackId::none) == nullptr);
```

```cpp
const AttackDefinition& j1 = *find_attack_definition(AttackId::j1);
ARPG_REQUIRE(j1.startup_ticks == 5);
ARPG_REQUIRE(j1.active_ticks == 3);
ARPG_REQUIRE(j1.recovery_ticks == 9);
ARPG_REQUIRE(j1.damage == 28);
ARPG_REQUIRE(j1.break_damage == 10);
```

```cpp
ARPG_REQUIRE(attack_phase_at(j1, 4) == AttackPhase::startup);
ARPG_REQUIRE(attack_phase_at(j1, 5) == AttackPhase::active);
ARPG_REQUIRE(attack_phase_at(j1, 8) == AttackPhase::recovery);
ARPG_REQUIRE(attack_phase_at(j1, 17) == AttackPhase::finished);
ARPG_REQUIRE(validate_attack_catalog());
```

Register only this suite with exact case count 3. Add the combat test subdirectory so the missing header produces a compiler RED.

- [ ] **Step 2: Observe RED**

```powershell
.\scripts\Test.ps1 -Preset windows-msvc-core-debug -Fresh
```

Expected: missing combat target/header; existing 22 Core cases remain untouched.

- [ ] **Step 3: Implement the stable value types and six-row table**

Use these public contracts:

```cpp
struct Vec3 final { float x{}; float y{}; float z{}; };
struct Aabb final { Vec3 minimum{}; Vec3 maximum{}; };
enum class AttackId : std::uint8_t {
    j1 = 0, j2, j3, heavy, launcher, air_j, none = 0xFF
};
enum class AttackPhase : std::uint8_t {
    startup, active, recovery, finished
};
enum class FeedbackLevel : std::uint8_t { light, medium, heavy };
enum class ImpactKind : std::uint8_t {
    light_hitstun, medium_hitstun, knockdown, launch
};
struct AttackDefinition final {
    AttackId id{AttackId::none};
    std::uint16_t startup_ticks{};
    std::uint16_t active_ticks{};
    std::uint16_t recovery_ticks{};
    int damage{};
    int break_damage{};
    ImpactKind impact{ImpactKind::light_hitstun};
    Aabb local_hitbox{};
    float lunge_distance{};
    float knockback_speed{};
    float launch_speed{};
    FeedbackLevel feedback{FeedbackLevel::light};
};
```

The private `constexpr std::array` rows are:

```cpp
{AttackId::j1,       5, 3,  9, 28, 10, ImpactKind::light_hitstun,
 {{0.20F,-0.65F, 0.10F},{1.50F,0.65F,1.50F}},0.10F,0.0F,0.0F,FeedbackLevel::light},
{AttackId::j2,       6, 3, 10, 34, 12, ImpactKind::medium_hitstun,
 {{0.20F,-0.65F, 0.10F},{1.65F,0.65F,1.55F}},0.14F,2.2F,0.0F,FeedbackLevel::medium},
{AttackId::j3,       8, 4, 16, 52, 20, ImpactKind::knockdown,
 {{0.15F,-0.70F, 0.05F},{1.90F,0.70F,1.65F}},0.20F,5.0F,0.0F,FeedbackLevel::heavy},
{AttackId::heavy,   14, 5, 22, 90, 40, ImpactKind::knockdown,
 {{0.10F,-0.75F, 0.00F},{2.20F,0.75F,1.75F}},0.24F,7.0F,0.0F,FeedbackLevel::heavy},
{AttackId::launcher, 7, 4, 17, 38, 18, ImpactKind::launch,
 {{0.10F,-0.65F, 0.00F},{1.40F,0.65F,1.90F}},0.12F,1.2F,9.5F,FeedbackLevel::medium},
{AttackId::air_j,    4, 5, 12, 42, 15, ImpactKind::medium_hitstun,
 {{0.10F,-0.65F,-0.40F},{1.60F,0.65F,1.20F}},0.08F,2.5F,0.0F,FeedbackLevel::medium},
```

Use half-open phase intervals. Validation rejects duplicate IDs, non-positive phases/damage, negative break, inverted AABBs or invalid feedback.

- [ ] **Step 4: Wire targets and two architecture tests**

`arpg_combat` is STATIC, `PUBLIC` includes `${PROJECT_SOURCE_DIR}/src`, `PRIVATE` links `arpg_core`, uses C++17 and project warnings. Root CMake adds it immediately after Core and also adds `tests/combat` under BUILD_TESTING. CMake will preserve the static link-only dependency for final executables without exposing Core as part of Combat's public API.

Generalize the source scanner from `CORE_DIR` to `SOURCE_DIR`/`SOURCE_LABEL` without changing its lexer. At configure time call:

```cmake
arpg_assert_target_dependency_boundary(arpg_core)
arpg_assert_target_dependency_boundary(arpg_combat)
```

Register `architecture.core_no_raylib` for `src/core` and `architecture.combat_no_raylib` for `src/combat`; both use label `headless;architecture`.

- [ ] **Step 5: Run GREEN and commit**

```powershell
.\scripts\Test.ps1 -Preset windows-msvc-core-debug -Fresh
ctest --test-dir .\out\build\windows-msvc-core-debug -N
.\out\build\windows-msvc-core-debug\bin\arpg_combat_tests.exe
git diff --check
git add CMakeLists.txt src/combat tests/combat tests/platform
git commit -m "feat: establish stage 1 combat module"
```

Expected: four CTests; combat `3 cases, 0 failures`.

---

