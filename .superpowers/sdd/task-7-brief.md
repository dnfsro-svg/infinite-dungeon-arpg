### Task 7: High-priority bomber, charger, dasher and hazard behavior

**Files:**
- Modify: `src/combat/combat_types.hpp`
- Modify: `src/combat/monster_pool.hpp`
- Modify: `src/combat/monster_pool.cpp`
- Modify: `src/combat/monster_ai.cpp`
- Modify: `src/combat/combat_world.cpp`
- Create: `tests/combat/monster_special_tests.cpp`
- Modify: `tests/combat/CMakeLists.txt`
- Modify: `tests/combat/combat_test_main.cpp`

**Interfaces:**
- Adds fixed `HazardRuntime[96]`/snapshot with owner, center, radius, telegraph/active/lifetime ticks, damage interval, and per-player attack latch.
- Adds monster phase payload for captured target position and attack vector.

- [ ] **Step 1: Write RED tests for all four roles**

Bomber: long warning, no early damage, one explosion, self defeated, no repeated damage. Charger: line capture at telegraph start, no retarget during active, boundary clamp. Dasher: shorter warning, burst movement, recovery gap. Hazard caster: warning marker then bounded active area, periodic damage protected by interval/invulnerability, owner defeat removes non-death hazards.

- [ ] **Step 2: Verify RED**

Expected: required phases/hazards do not exist.

- [ ] **Step 3: Implement high-priority attacks using shared phase transitions**

All target capture happens once at telegraph entry. All spawned hazards/projectiles include a valid owner handle. On monster defeat, scan bounded transient arrays once and remove non-death-owned objects. Saturation increments diagnostics and does not allocate or overwrite active slots.

- [ ] **Step 4: Run combat Debug and Release direct executables**

Expected: every role test and legacy combat case has zero failures in both configurations.

- [ ] **Step 5: Commit**

```powershell
git add src/combat tests/combat
git commit -m "feat: add high priority monster attacks"
```

---

