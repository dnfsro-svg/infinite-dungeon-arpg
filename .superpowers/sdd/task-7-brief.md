### Task 7: Heavy Armor, Break Window, Reset, Determinism, and Ten-Minute Stress Gate

**Files:** `break_stress_tests.cpp`, world/hit/target/CMake/test-main updates.

**Interface:** 120 break, immediate breaking-blow control, 180 local-tick broken window, complete reset, deterministic replay and zero-allocation pressure.

- [ ] **Step 1: Add five RED cases**

Prove armored suppression before zero; breaking blow immediately sets Broken and applies its control; Broken lasts exactly 180 non-frozen target ticks then restores 120; Defeated overrides recovery; reset clears all transients and leaves one reset event; two-world replay matches; 36,000 ticks allocate/overflow zero. Change count 25→30.

- [ ] **Step 2: Observe RED**

Run core-debug; expected heavy reacts incorrectly or lacks lifecycle/reset evidence.

- [ ] **Step 3: Implement break/defeat priority**

On hit: subtract HP first; HP zero enters Defeated. Otherwise armored heavy subtracts break. If break reaches zero, set Broken 180, emit `break_started`, and apply the current breaking hit's reaction immediately. Already Broken uses normal reaction. Broken countdown pauses only during target Hit Stop, continues through other reactions, then restores 120/armored without overwriting current ReactionState.

- [ ] **Step 4: Implement complete reset and two stress scripts**

Reset drains events, reconstructs all runtime fields/config spawns, clears input/diagnostics/hit flags/Hit Stop/tick, then emits exactly one reset event at tick zero.

The 2,400-tick replay compares every snapshot field and each event field. The 36,000-tick allocation loop constructs world/schedules first, records `allocation_count`, then runs:

```cpp
if (tick % 37 == 0)  { world.queue_action(Action::light); }
if (tick % 181 == 0) { world.queue_action(Action::jump); }
if (tick % 251 == 0) { world.queue_action(Action::heavy); }
if (tick % 307 == 0) { world.queue_action(Action::launcher); }
world.tick({
 static_cast<std::int8_t>((tick/120)%2==0 ? 1 : -1),
 static_cast<std::int8_t>((tick/180)%2==0 ? 1 : -1)});
while (world.try_pop_event().has_value()) {}
if (tick > 0 && tick % 3600 == 0) {
 world.reset();
 while (world.try_pop_event().has_value()) {}
}
```

Assert allocation delta, input overflow and event overflow are zero.

- [ ] **Step 5: Run Debug/Release Core gates and reviews**

```powershell
.\scripts\Test.ps1 -Preset windows-msvc-core-debug -Fresh
.\scripts\Test.ps1 -Preset windows-msvc-release -Fresh
.\out\build\windows-msvc-core-debug\bin\arpg_combat_tests.exe
.\out\build\windows-msvc-release\bin\arpg_combat_tests.exe
rg -n "raylib|raymath|rlgl|std::vector|std::deque|unordered|new |malloc" src/combat
git diff --check
```

Expected combat `30 cases, 0 failures`, no forbidden/deferred allocation path. Complete specification and quality reviews.

- [ ] **Step 6: Commit and merge reviewed Combat**

```powershell
git add src/combat tests/combat
git commit -m "feat: complete deterministic combat lab core"
```

From integration worktree:

```powershell
git merge --no-ff task/m01-combat-core -m "merge: integrate stage 1 combat core"
.\scripts\Test.ps1 -Preset windows-msvc-core-debug -Fresh
git worktree add ..\m01-raylib-combat-host -b task/m01-raylib-combat-host milestone/m01-combat-lab
```

Expected four Core-only CTests pass; Host starts from reviewed merged API.

---

