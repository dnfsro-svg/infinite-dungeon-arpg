### Task 6: Dummy Hitstun, Knockback, Launch, Knockdown, and Respawn

**Files:** `target_simulation.cpp`, `dummy_reaction_tests.cpp`, world/hit/CMake/test-main updates.

**Interface:** orthogonal ReactionState/ArmorState; deterministic target motion and local timers.

- [ ] **Step 1: Add five RED reaction cases**

Prove normal J1 hitstun 10 versus light 13, J2 X velocities 2.2 versus 2.75, U Z velocity 9.5 and gravity landing, J3/L Knockdown 45→Rising 30→Idle, and Defeated exactly 90 active ticks then one respawn event. Change count 20→25.

- [ ] **Step 2: Observe RED**

Run core-debug; targets currently lose HP but stay static.

- [ ] **Step 3: Implement separate reaction and armor runtime**

```cpp
struct DummyRuntime final {
 DummyKind kind{}; Vec3 spawn{}; Vec3 position{}; Vec3 velocity{};
 ReactionState reaction{}; ArmorState armor{};
 std::uint16_t reaction_ticks{};
 std::uint16_t break_window_ticks{};
 std::uint16_t hit_stop_ticks{};
 int hp{}; int max_hp{}; int break_value{}; int max_break{};
};
constexpr std::uint16_t kLightHitstunTicks=10;
constexpr std::uint16_t kMediumHitstunTicks=16;
constexpr std::uint16_t kHeavyHitstunTicks=22;
constexpr std::uint16_t kKnockdownTicks=45;
constexpr std::uint16_t kRisingTicks=30;
constexpr std::uint16_t kRespawnTicks=90;
```

Light multiplies reaction tick durations and impulses by 1.25; tick values use ceiling. Normal uses 1.0. Target Hit Stop decrements first and pauses local motion/timers. Airborne integrates position then gravity. Any launch landing and knockdown impact follows Knockdown→Rising. HP zero immediately enters Defeated, ignores hits, and respawns at config spawn with full stats.

Stats: HP `{300,450,700}`, break `{0,0,120}`, armor `{none,none,armored}`.

- [ ] **Step 4: Run GREEN and commit**

```powershell
.\scripts\Test.ps1 -Preset windows-msvc-core-debug
.\out\build\windows-msvc-core-debug\bin\arpg_combat_tests.exe
git add src/combat tests/combat
git commit -m "feat: add combat target reactions"
```

Expected combat `25 cases, 0 failures`.

---

