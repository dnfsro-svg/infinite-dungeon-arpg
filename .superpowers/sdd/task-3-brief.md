### Task 3: Deterministic Ground Movement and Jump Arc

**Files:** `src/combat/combat_world.hpp/.cpp`, `tests/combat/movement_jump_tests.cpp`, type/CMake/test-main updates.

**Interface:** `CombatWorld::{queue_action,tick,reset,snapshot}`; no delta-time parameter.

To keep a fixed object size without a heap-backed pimpl, `PlayerRuntime`, `DummyRuntime` and `AttackRuntime` are private nested structs fully defined in `combat_world.hpp`. `player_simulation.cpp`, `target_simulation.cpp` and `hit_resolution.cpp` implement private `CombatWorld` member functions declared in the same class; they do not duplicate runtime structs or expose them through snapshots.

- [ ] **Step 1: Add four RED cases**

Prove 60 right ticks = 5.4 units, diagonal normalization, room clamps, deterministic jump/apex/landing, 70% air control and one landing transition. Change count 7→11.

- [ ] **Step 2: Observe missing-world RED**

Run core-debug; expected missing `combat/combat_world.hpp`.

- [ ] **Step 3: Add public states, config and snapshots**

```cpp
enum class Facing : std::int8_t { left = -1, right = 1 };
enum class PlayerState : std::uint8_t {
 idle, move, attack_startup, attack_active, attack_recovery,
 jump_rise, jump_fall, landing
};
enum class DummyKind : std::uint8_t { light, normal, heavy };
enum class ReactionState : std::uint8_t {
 idle, hitstun, airborne, knockdown, rising, defeated, respawning
};
enum class ArmorState : std::uint8_t { none, armored, broken };
struct MovementInput final { std::int8_t x{}; std::int8_t y{}; };
struct CombatLabConfig final {
 Vec3 player_spawn{0,0,0};
 std::array<Vec3,3> dummy_spawns{{
   {2.30F,-0.35F,0},{2.80F,0,0},{3.30F,0.35F,0}}};
};
```

Snapshots include every field required by the design: position/velocity, player/attack phase/combo/hit-stop/air quota, and per-dummy kind/reaction/armor/HP/break/break-window/hit-stop plus diagnostics.

- [ ] **Step 4: Implement exact movement and jump constants**

```cpp
constexpr float kTickSeconds = 1.0F/60.0F;
constexpr float kGroundSpeed = 5.4F;
constexpr float kAirRatio = 0.70F;
constexpr float kJumpSpeed = 8.5F;
constexpr float kGravity = 24.0F;
constexpr float kDiagonal = 0.7071067811865475F;
```

Clamp X `[-8,8]`, Y `[-3.5,3.5]`. Preserve facing without horizontal input. Integrate Z position then gravity; clamp landing Z/velocity to zero. Reset restores config and diagnostics.

- [ ] **Step 5: Run GREEN and commit**

```powershell
.\scripts\Test.ps1 -Preset windows-msvc-core-debug
.\out\build\windows-msvc-core-debug\bin\arpg_combat_tests.exe
git add src/combat tests/combat
git commit -m "feat: add deterministic combat movement and jump"
```

Expected combat `11 cases, 0 failures`.

---

