# Task 9 implementer report

Status: DONE

## Commits

- Base: `891826d8d0b099952cd5b8ffcd38b2b4df1ddde0`
- Head: `d55585eca1f6815445b363df28c9d6277b2a8824`
- Initial commit: `236b5ce fix: bind active skills to numpad keys`
- Fix round 1: `d55585e fix: consume both pressed input sources`

## Scope and implementation

- `sample_physical_keys` now samples active-skill edges from an explicit fixed `std::array` containing `KEY_KP_1` through `KEY_KP_5`; no raylib-enum continuity is assumed.
- Added pure `win32_virtual_key_for_raylib(int)` and explicit `KEY_KP_1..5` to `0x61..0x65` mappings. Both `platform_key_pressed` and `platform_key_down` call that mapper while retaining every previous supported-key mapping.
- `map_host_frame_input` and `submit_frame_actions` signatures and semantics are unchanged.
- Added exactly four platform cases, taking the expected count from 469 to 473: exact NumPad slots, main-number rejection/pressed-edge behavior, pure Win32 mapping, and unchanged submit ordering.
- The controller authorized the minimal scope extension to `tests/platform/host_input_tests.cpp`: old main-number query counts were changed from two to one and each explicit NumPad query is required exactly once. No other old input behavior changed.

## TDD evidence

The first direct PowerShell build lacked the MSVC standard-library environment and failed on `<cmath>`; that infrastructure failure was rejected as RED evidence. All recorded build evidence below used Visual Studio `VsDevCmd.bat`, remained serial, and checked native exit codes.

### RED 1: sampling behavior

Commands:

```powershell
cmake --build --preset windows-msvc-debug --target arpg_platform_tests --parallel 1
ctest --test-dir out/build/windows-msvc-debug -R '^platform\.units$' --output-on-failure -j 1
```

Result: build succeeded; `platform.units` reported `472 cases, 4 failures`. Failures were the expected old-path behavior: main numbers were still queried as skills and NumPad pressed edges were absent. The new submit-semantics case passed.

### RED 2: pure Win32 mapper

Command:

```powershell
cmake --build --preset windows-msvc-debug --target arpg_platform_tests --parallel 1
```

Result: expected compile failure because `arpg::platform::win32_virtual_key_for_raylib` did not yet exist.

## GREEN verification

Commands were run serially in this order:

```powershell
cmake --build --preset windows-msvc-debug --target arpg_platform_tests --parallel 1
ctest --test-dir out/build/windows-msvc-debug -R '^platform\.units$' --output-on-failure -j 1
ctest --test-dir out/build/windows-msvc-debug -R '^(platform\.input_latency_source|platform\.host_input_source)$' --output-on-failure -j 1
```

Results:

- Build: passed.
- `platform.units`: passed, `473 cases, 0 failures`, 11.07 seconds.
- `platform.input_latency_source`: passed, 62.06 seconds.
- `platform.host_input_source`: passed, 201.77 seconds.
- Combined source-guard CPU time: 263.83 seconds. The existing source lexer was single-core and slow but showed continuous CPU progress, about 16 MB working set, no process storm, and exited normally. The controller independently confirmed the final `LastTest.log`; the guards were not redundantly rerun.

## Self-review and cleanup

- `git diff --cached --check`: clean before commit.
- Net platform case delta is exactly +4 and the expected count is 473.
- Main keyboard `KEY_ONE..KEY_FIVE` does not set active skill slots; held NumPad state without a pressed edge does not cast repeatedly.
- No gameplay, cooldown, save, skill-slot, or launcher behavior was changed.
- No game executable was launched; build and tests were never run concurrently.
- No `__pycache__`/`.pyc` artifacts or build/test/game processes remained.
- Final repository status after the commit is clean.

## Concerns

None.

## Fix round 1

Review finding: the Windows `platform_key_pressed` expression used short-circuit `||`; when raylib returned true it skipped `GetAsyncKeyState`, allowing the Win32 low-bit edge to survive into the following frame and duplicate a pressed edge.

Resolution:

- Strengthened the existing `raylib_input` Task 9 case without registering a new case; the platform total remains 473.
- Because raylib and `GetAsyncKeyState` cannot be safely injected in the existing headless executable without adding a test-only production seam, the existing case now guards the real function body: it requires separate `raylib_pressed` and `asynchronous_state` local samples, requires their post-sampling merge, and rejects the direct short-circuit call form.
- Windows `platform_key_pressed` now samples `IsKeyPressed` and the mapped asynchronous state unconditionally before combining their values.
- `platform_key_down`, NumPad mapping, J/K/L/WASD handling, and all host input signatures remain unchanged.

TDD evidence:

```powershell
cmake --build --preset windows-msvc-debug --target arpg_platform_tests --parallel 1
ctest --test-dir out/build/windows-msvc-debug -R '^platform\.units$' --output-on-failure -j 1
```

- RED: build passed; `473 cases, 1 failures`, specifically the missing unconditional `raylib_pressed` local sample.
- GREEN: build passed; `platform.units` passed, 473 cases, 0 failures, 13.32 seconds.
- Per controller direction, the unchanged 263.83-second input architecture guards were not rerun. Their prior final log remained recorded as passing; this fix touched only the local pressed-source merge and its existing unit case.
- `git diff --cached --check` passed before commit; no cache files, task processes, or game process remained.
