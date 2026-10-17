# Stage 19 Task 1 Report: Shared Room Bounds and Entry Spawns

## Scope and implementation

- Commit: `fb7f0f3 feat: expand combat room to four times area`
- Updated `combat::room_bounds` to `min_x=-24`, `max_x=24`,
  `min_y=-11`, `max_y=11`, with derived `width=48` and `depth=22`.
- Kept `make_combat_lab_config(EntrySide, rules_version)` unchanged and
  preserved the initial `{0,0,0}` spawn and existing facing behavior.
- Updated entry spawns to left/right `-22.5/22.5` and top/bottom
  `-10.25/10.25`.
- Added compile-time geometry assertions and exact navigation/entry-spawn
  assertions.
- Replaced non-subject old room-edge literals in combat, dungeon, abyss and
  validation tests with `combat::room_bounds` constants.
- Updated room-width-dependent test robot budgets (256/512 to 640 ticks),
  the deterministic encounter position trace, and the derived abyss stress
  hash. No encounter-director, affix, wave, or budget production algorithm
  was changed.

## TDD evidence

### RED

After adding the geometry/navigation/entry-spawn assertions and before
changing production code, I ran:

```powershell
cmd /c 'call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 >nul && cmake --preset windows-msvc-debug && cmake --build --preset windows-msvc-debug --target arpg_combat_tests arpg_dungeon_tests && ctest --test-dir out/build/windows-msvc-debug -R "combat.units|dungeon.units" --output-on-failure'
```

The focused build failed as intended before test execution:

```text
tests/combat/movement_jump_tests.cpp(15): error C2607: static assertion failed
...
tests/combat/movement_jump_tests.cpp(20): error C2607: static assertion failed
```

Those six assertions covered the old `-12/12/-5.5/5.5/24/11` geometry.
The first shell invocation used PowerShell-incompatible quoting for the
parenthesized Visual Studio path and did not enter CMake; it was retried with
the `cmd /c` form above. One navigation test namespace qualification error was
also corrected before the final RED rerun, leaving only the expected static
assertion failure.

### GREEN

Focused combat verification:

```powershell
cmd /c 'call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 >nul && cmake --build --preset windows-msvc-debug --target arpg_combat_tests && ctest --test-dir out/build/windows-msvc-debug -R "combat.units" --output-on-failure'
```

Result: `combat.units` passed, 230 cases, 0 failures, 0.81 seconds.

Focused dungeon verification:

```powershell
cmd /c 'call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 >nul && cmake --build --preset windows-msvc-debug --target arpg_dungeon_tests && ctest --test-dir out/build/windows-msvc-debug -R "dungeon.units" --output-on-failure'
```

Result: `dungeon.units` passed, 287 cases, 0 failures, 371.52 seconds.

Final required combined verification:

```powershell
cmd /c 'call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 >nul && cmake --preset windows-msvc-debug && cmake --build --preset windows-msvc-debug --target arpg_combat_tests arpg_dungeon_tests && ctest --test-dir out/build/windows-msvc-debug -R "combat.units|dungeon.units" --output-on-failure'
```

Result:

```text
Test #4: combat.units  Passed  0.86 sec
Test #5: dungeon.units Passed 369.05 sec
100% tests passed, 0 tests failed out of 2
Total Test time (real) = 369.92 sec
```

## Self-review

- `git diff --check` passed before staging.
- Staged scope was limited to the two production files and relevant combat/
  dungeon tests. The extra test files beyond the brief's initial list were
  required to remove old room-edge assumptions surfaced by the specified
  suites.
- Exact numeric checks remain only where geometry/spawn values are the test
  subject; fixtures, stress tests and exit helpers use the shared constants.
- The deterministic abyss hash changed only because the doubled shared bounds
  change generated encounter positions; the trace's trial/hit counts remained
  `4000` and `9/11/9/7`.

## Concerns

- `dungeon.units` is intentionally long-running (about 6 minutes) because it
  includes 1000-room stress coverage. CPU sampling confirmed its process kept
  progressing during each run.
- CMake emits expected capability-probe "Failed" messages and Ninja emits a
  `premature end of file; recovering` warning in this Windows environment;
  the final configure, build, and test command exited with code 0.
