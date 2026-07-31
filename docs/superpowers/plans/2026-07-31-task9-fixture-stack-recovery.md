# Task 9 Fixture Stack Recovery Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Restore the Stage 9 and Stage 10 standalone validation fixtures by giving their intentional fixed-capacity Debug frames the same bounded Windows stack reserve already used by equivalent project test executables.

**Architecture:** This is a test-executable link contract only. Keep the fixture code and all production gameplay unchanged; on MSVC, reserve 4 MiB for the two standalone executables, matching `arpg_dungeon_tests`, `arpg_combat_tests`, `arpg_persistence_tests`, and `arpg_platform_tests`. The existing CTest fixtures are the behavioral regression tests, and PE header readback proves the linker contract was applied.

**Tech Stack:** C++17, raylib 6.0 project, CMake 3.25+, Ninja, MSVC 19.44, CTest.

## Global Constraints

- Work only in `E:/game/.worktrees/task9-gate-recovery` on `codex/task9-gate-recovery`.
- Keep `CMAKE_BUILD_PARALLEL_LEVEL=1` and every build/test serial with `-j1`.
- Do not modify production source, gameplay rules, golden values, CTest timeouts, fixture assertions, or B-G scope.
- Do not add stack reserve to Stage16 or any unrelated executable in this card.
- Do not delete files, reset Git, or touch any other worktree.
- Before rerunning a timed-out command, inspect and stop only its exact orphan PID.
- Use RED -> GREEN and record real exit codes and elapsed times.

---

### Task 1: Restore the two standalone validation fixtures

**Files:**
- Modify: `tests/dungeon/CMakeLists.txt`
- Test: existing `stage9.validation_fixture.real_v4_reward_reload`
- Test: existing `stage10.validation_fixture.real_abyss_transactions`

**Interfaces:**
- Consumes: the existing MSVC-only `/STACK:4194304` convention used by `arpg_dungeon_tests` in the same CMake file.
- Produces: `arpg_stage9_validation_fixture.exe` and `arpg_stage10_validation_fixture.exe` with PE stack reserve `0x400000`; non-MSVC builds remain unchanged.

- [ ] **Step 1: Preserve the current RED evidence**

Run the already-built Debug fixtures from their CTest working directory:

```powershell
$bin = 'E:/game/.worktrees/task9-gate-recovery/out/build/windows-msvc-debug/bin'
& "$bin/arpg_stage9_validation_fixture.exe"
& "$bin/arpg_stage10_validation_fixture.exe"
```

Expected before the fix: both return signed exit `-1073741571`, hexadecimal `0xC00000FD`. Windows Application Error event 1000 reports exception `0xc00000fd` for each executable.

- [ ] **Step 2: Add the minimal target-local linker contract**

Immediately after each validation fixture target is defined, add the matching
MSVC-only link option next to that target:

```cmake
if(MSVC)
    target_link_options(arpg_stage9_validation_fixture
        PRIVATE /STACK:4194304)
endif()

# ... after arpg_stage10_validation_fixture is defined ...
if(MSVC)
    target_link_options(arpg_stage10_validation_fixture
        PRIVATE /STACK:4194304)
endif()
```

Use one concise explanatory comment with each option. Do not change the target
sources, test commands, labels, or timeout.

- [ ] **Step 3: Reconfigure and rebuild only the two fixtures**

Run from a VS2022 x64 Developer PowerShell:

```powershell
$env:CMAKE_BUILD_PARALLEL_LEVEL = '1'
cmake --preset windows-msvc-debug
cmake --build --preset windows-msvc-debug `
  --target arpg_stage9_validation_fixture arpg_stage10_validation_fixture -- -j1
```

Expected: configure and both links exit 0; no unrelated executable is rebuilt except CMake-required dependencies.

- [ ] **Step 4: Read back the PE stack reserve**

```powershell
dumpbin /headers out/build/windows-msvc-debug/bin/arpg_stage9_validation_fixture.exe |
  Select-String 'size of stack reserve'
dumpbin /headers out/build/windows-msvc-debug/bin/arpg_stage10_validation_fixture.exe |
  Select-String 'size of stack reserve'
```

Expected: both report `400000 size of stack reserve`.

- [ ] **Step 5: Run the two exact CTest regressions serially**

```powershell
ctest --test-dir out/build/windows-msvc-debug `
  -R '^stage9\.validation_fixture\.real_v4_reward_reload$' `
  --output-on-failure -j1
ctest --test-dir out/build/windows-msvc-debug `
  -R '^stage10\.validation_fixture\.real_abyss_transactions$' `
  --output-on-failure -j1
```

Expected: each selector runs exactly one test and passes 1/1. Confirm no Application Error event 1000 was created for either executable during the GREEN run.

- [ ] **Step 6: Scope and whitespace audit**

```powershell
git diff --check
git diff -- tests/dungeon/CMakeLists.txt
git status --short
```

Expected: only this plan and `tests/dungeon/CMakeLists.txt` are changed; build output remains ignored; no whitespace errors.

- [ ] **Step 7: Commit the independently testable fix**

```powershell
git add docs/superpowers/plans/2026-07-31-task9-fixture-stack-recovery.md `
  tests/dungeon/CMakeLists.txt
git commit -m "test: restore stage9 stage10 fixture stack budget"
```

Expected: one commit containing only the plan and CMake target-local stack contract.

## Self-Review

- Spec coverage: covers only the two confirmed `0xC00000FD` fixtures and their Windows linker contract.
- Placeholder scan: no TBD/TODO or unspecified implementation step remains.
- Type/interface consistency: no C++ interface changes; both targets use the existing project-wide 4 MiB test-stack convention.
- Scope exclusions: Stage9 evidence trace, Stage10 raylib capture, Stage11/Stage16, persistence, performance, and full Debug/Release gates remain separate cards.
