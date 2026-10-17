# Stage 19 Task 2 report: deterministic room-density affixes

## Scope and changes

- Added `src/dungeon/room_affix.hpp` and `.cpp` with the fixed crowded/dense/horde catalog, deterministic domain-separated affix and count streams, and abyss 1.5x rounded-up monster counts.
- Registered the dungeon source and `tests/dungeon/room_affix_tests.cpp`.
- Registered the room-affix suite in `dungeon_test_main.cpp`; total dungeon case count is now 291.
- Did not modify session, encounter director, HUD, camera, persistence, or unrelated files.

## TDD evidence

### RED

After registering `room_affix_tests.cpp`, ran under the MSVC developer environment:

```powershell
cmake --build --preset windows-msvc-debug --target arpg_dungeon_tests
```

Result: failed as expected with `fatal error C1083: cannot open include file: 'dungeon/room_affix.hpp': No such file or directory` from `tests/dungeon/room_affix_tests.cpp`.

### GREEN

Implemented the minimum fixed catalog and rolling API, then ran:

```powershell
cmake --build --preset windows-msvc-debug --target arpg_dungeon_tests
```

Result: passed; `arpg_dungeon_tests.exe` linked successfully.

## Test coverage and final suite

The new tests verify:

- required catalog values;
- normal/abyss identity of affix and base count plus abyss rounding/cap across 10,000 seeds;
- all three weighted proportions over 100,000 stable seeds, each within one percentage point, and every inclusive count endpoint 12 through 30;
- two identical 100,000-seed replays and zero allocations for a separate 100,000-roll probe.

Final command, started once and allowed to finish without restart or termination:

```powershell
ctest --test-dir out/build/windows-msvc-debug -R dungeon.units --output-on-failure
```

Final output: `1/1 Test #5: dungeon.units Passed 366.87 sec`; `100% tests passed, 0 tests failed`; total real time `366.88 sec`.

## Self-review

- `git diff --check` passed.
- Affix and base-count RNG streams use distinct fixed 64-bit domains.
- The API accepts only room seed and abyss state; no configuration or depth input was introduced.
- The implementation returns no dynamic storage and the allocation probe passed.

## Commit

`f0c3cbc feat: add deterministic room density affixes`

## Concerns

None. The task intentionally provides only the dungeon-layer roll API; integration is deferred to a later task.

## Review follow-up: exact dense and horde ranges

Code review found that the original catalog test asserted only the dense/horde weights. The endpoint-union test could not prevent an incorrect redistribution of the dense and horde boundaries. The test now explicitly asserts `dense` is 17 through 22 and `horde` is 23 through 30. No production behavior was changed.

### Follow-up RED

Because the committed implementation was already correct, the new assertions could not fail against the real catalog. After adding the assertions, the uncommitted working tree temporarily changed only the production test fixture catalog entry from `{horde, 15, 23, 30}` to `{horde, 15, 23, 29}`. It was restored immediately after the run.

Build command (with `VsDevCmd.bat` loaded) completed successfully, followed by:

```powershell
ctest --test-dir out/build/windows-msvc-debug -R dungeon.units --output-on-failure
```

Expected result: failed after `367.38 sec`, with the new assertion:

```text
[FAIL] room_affix.catalog has required weights and ranges: horde->maximum == 30U
(E:\game\.worktrees\stage19-large-density-rooms\tests\dungeon\room_affix_tests.cpp:27)
```

The intentionally reduced maximum also caused the existing endpoint-coverage assertion to fail, as expected.

### Follow-up GREEN

Restored `horde.maximum` to 30 without changing any production logic. Rebuilt under `VsDevCmd.bat`:

```powershell
cmake --build --preset windows-msvc-debug --target arpg_dungeon_tests
```

Result: successful link of `arpg_dungeon_tests.exe`.

Then ran one complete final suite:

```powershell
ctest --test-dir out/build/windows-msvc-debug -R dungeon.units --output-on-failure
```

Final output: `1/1 Test #5: dungeon.units Passed 370.76 sec`; `100% tests passed, 0 tests failed`; total real time `370.77 sec`.

### Follow-up commit

`13793d1 test: cover room density affix ranges`
