### Task 6: Ranged, support, projectile and shield behavior

**Files:**
- Modify: `src/combat/combat_types.hpp`
- Modify: `src/combat/monster_pool.hpp`
- Modify: `src/combat/monster_pool.cpp`
- Modify: `src/combat/monster_ai.cpp`
- Modify: `src/combat/combat_world.cpp`
- Create: `tests/combat/monster_ranged_tests.cpp`
- Modify: `tests/combat/CMakeLists.txt`
- Modify: `tests/combat/combat_test_main.cpp`

**Interfaces:**
- Adds fixed `ProjectileRuntime[384]`, `ProjectileSnapshot[384]`, active count, owner handle, position/velocity, lifetime, damage, and collision radius.
- Adds bounded shield points/duration to monster runtime/snapshot.

- [ ] **Step 1: Write RED tests**

Cover shooter range maintenance, telegraph-before-projectile, deterministic projectile trajectory, projectile removal on hit/bounds/lifetime, pool saturation diagnostics, support selecting the lowest-index legal ally, shield non-stacking refresh, and support never being the only direct target.

- [ ] **Step 2: Verify RED**

Expected: missing projectile and shield behavior failures.

- [ ] **Step 3: Implement shooter and support**

The shooter moves toward/away from its preferred range, freezes during telegraph, and creates one projectile in active. The support chooses an active non-support ally without shield; if none exists it repositions and enters a short cooldown. Shield is a separate bounded integer absorbed before HP and never stacks above the definition cap.

- [ ] **Step 4: Run core-only tests**

Expected: combat and architecture tests pass with no capacity overflow under legal fixtures.

- [ ] **Step 5: Commit**

```powershell
git add src/combat tests/combat
git commit -m "feat: add ranged and support monsters"
```

---

