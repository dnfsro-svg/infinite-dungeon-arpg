### Task 4: Attack State Machine, Whiff Timing, and Air Attack Limit

**Files:** `src/combat/player_simulation.cpp`, `tests/combat/attack_state_tests.cpp`, world/CMake/test-main updates.

**Interface:** six attack timelines; J1/J2 whiff continuation; J3 terminal; one air J per airtime. Hit-confirm early windows are enabled by real hits in Task 5.

- [ ] **Step 1: Add four RED timeline cases**

Prove J1 phases 0–4/5–7/8–16, L/U catalog totals, J1 whiff opens at elapsed 13, J2 at 15, J3 never wraps, attacks reject illegal K/L/U/J cancels, and air permits only one J. Change count 11→15.

- [ ] **Step 2: Observe RED**

Run core-debug; expected queued attack remains unconsumed.

- [ ] **Step 3: Implement attack runtime and deterministic action priority**

```cpp
struct AttackRuntime final {
 AttackId id{AttackId::none};
 std::uint16_t elapsed_ticks{};
 std::uint64_t serial{};
 bool connected{};
 bool impact_event_emitted{};
 std::array<bool,3> hit_targets{};
};
constexpr std::uint16_t kJ1WhiffCancelTick = 13;
constexpr std::uint16_t kJ2WhiffCancelTick = 15;
```

On a non-frozen tick consume K→L→U→J from Idle/Move; airborne consumes only J when quota remains. Starting an attack increments a nonzero serial, clears hit flags, applies one lunge and records combo stage. Whiff J1/J2 consumes buffered J only at the constants above. J3/L/U return Idle. Air J returns rise/fall from Z velocity. Input aging occurs after consumption and pauses during player Hit Stop.

- [ ] **Step 4: Run GREEN and commit**

```powershell
.\scripts\Test.ps1 -Preset windows-msvc-core-debug
.\out\build\windows-msvc-core-debug\bin\arpg_combat_tests.exe
git add src/combat tests/combat
git commit -m "feat: add combat attack state machine"
```

Expected combat `15 cases, 0 failures`.

---

