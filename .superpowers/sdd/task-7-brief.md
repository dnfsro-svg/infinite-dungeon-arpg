### Task 7: Capacity, determinism, full-build, and fullscreen validation

**Files:**
- Create: `docs/validation/stage19-large-room-density-affix.md`
- Modify: `tests/dungeon/dungeon_abyss_stress_tests.cpp`
- Modify: `tests/dungeon/dungeon_stress_tests.cpp`
- Modify: `tests/platform/hud_stress_tests.cpp`

**Interfaces:**
- Consumes: all prior Stage 19 interfaces.
- Produces: reproducible validation record with commands, suite counts, representative seeds, and visual findings.

- [ ] **Step 1: Add maximum-density stress coverage**

Construct a deterministic abyss horde whose base count is 30 and final count is 45. Require `CombatSnapshot::monster_count==45`, `remaining_targets==45`, no pool saturation diagnostics, and no allocation during 10,000 fixed ticks. Repeat after reload and compare density, spawn IDs, ordinals, positions, and affixes.

- [ ] **Step 2: Add long-run room coverage**

Generate at least 10,000 normal/abyss rooms. Require every normal count in 12–30 according to its affix, every abyss count in 18–45 with the exact ceil formula, one wave only, legal safe positions, and no deterministic-stream drift between two runs.

- [ ] **Step 3: Run full Debug verification**

```powershell
cmake --preset windows-msvc-debug
cmake --build --preset windows-msvc-debug
ctest --preset windows-msvc-debug
```

Expected: all configured Debug tests PASS.

- [ ] **Step 4: Run full Release verification**

```powershell
cmake --preset windows-msvc-release
cmake --build --preset windows-msvc-release
ctest --preset windows-msvc-release
```

Expected: all configured Release tests PASS and `out/build/windows-msvc-release/bin/arpg_game.exe` exists.

- [ ] **Step 5: Perform fullscreen visual validation**

Launch the Release executable. Verify at 16:9 fullscreen: center movement, all four camera clamps, character size unchanged, floor motion visible, doors/hole aligned, normal 12+ encounter, horde 23+ encounter, and an abyss encounter up to 45. Confirm all enemies exist immediately on entry and no second wave appears after clearing any subset.

- [ ] **Step 6: Record exact evidence**

Write the commit hash, Debug/Release test totals, selected room seeds, density/count pairs, maximum live monster count, allocation/saturation result, executable path, resolution, and visual pass/fail observations to `docs/validation/stage19-large-room-density-affix.md`. Do not claim any check that was not actually run.

- [ ] **Step 7: Commit validation and final fixes**

```powershell
git add tests/dungeon/dungeon_abyss_stress_tests.cpp tests/dungeon/dungeon_stress_tests.cpp tests/platform/hud_stress_tests.cpp docs/validation/stage19-large-room-density-affix.md
git commit -m "test: verify large density rooms end to end"
```

---

## Completion Gate

- Every task commit exists and contains only its scoped files.
- Normal rooms always produce 12–30 monsters from the displayed affix.
- Abyss rooms use the displayed base affix and exact 1.5× ceil count, capped at 45.
- One batch is loaded before combat and no production path enters wave delay.
- Same stable room data reproduces affix, count, composition, positions, and monster affixes without save-format changes.
- A 16:9 fullscreen camera shows 24×11 world units, tracks without added delay, and clamps at all four edges.
- Debug and Release CTest are fully green.
- Fullscreen raylib visual validation is recorded with truthful evidence.
