### Task 9: Raylib presentation, warnings, HP and director HUD

**Files:**
- Modify: `src/platform/raylib/combat_renderer.hpp`
- Modify: `src/platform/raylib/combat_renderer.cpp`
- Modify: `src/platform/raylib/combat_feedback.cpp`
- Modify: `src/platform/raylib/combat_audio.cpp`
- Modify: `src/platform/raylib/combat_view_math.hpp`
- Modify: `src/platform/raylib/combat_view_math.cpp`
- Create: `tests/platform/monster_view_tests.cpp`
- Modify: `tests/platform/CMakeLists.txt`
- Modify: `tests/platform/platform_test_main.cpp`

**Interfaces:**
- Pure `monster_visual(MonsterId, MonsterAiPhase, ecology)` returns body/accent/warning colors, shape ID, icon label, and warning mode.
- Pure projectile/hazard projection functions reuse existing 2.5D projection.

- [ ] **Step 1: Write RED pure-view tests**

Assert all eight IDs produce distinct role labels, matching ecology accent colors, high-priority phases expose a warning, inactive slots are hidden, player HP ratio clamps to `[0,1]`, and hazard warning/active modes differ.

- [ ] **Step 2: Verify RED**

Expected: missing visual functions fail compilation.

- [ ] **Step 3: Implement visuals and feedback**

Render active monsters in depth order using simple low-cost silhouettes: bomber round/explosive, charger long weapon, bulwark wide body/shield, support cross/shield ring, shooter ranged barrel, dasher pointed shape, chaser claw, hazard caster staff/circle. Draw warnings before active effects. Add player HP bar and HUD lines for `Wave current/total`, budget, active monster/projectile/hazard counts, and saturation diagnostics. Reuse aggregated shake/audio rules; one monster attack emits one low-frequency layer regardless of targets.

- [ ] **Step 4: Run platform tests and launch Debug**

Run:

```powershell
.\scripts\Test.ps1 -Preset windows-msvc-debug
.\out\build\windows-msvc-debug\bin\arpg_platform_tests.exe
```

Expected: all platform tests pass and the executable opens with readable silhouettes/HUD.

- [ ] **Step 5: Commit**

```powershell
git add src/platform/raylib tests/platform
git commit -m "feat: render stage 4 monster encounters"
```

---

