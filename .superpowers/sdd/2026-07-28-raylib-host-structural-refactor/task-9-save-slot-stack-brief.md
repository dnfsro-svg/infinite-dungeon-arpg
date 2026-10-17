# Task 9 follow-up: heap-own exit-unlock save slots

Base commit: `e416340`

Preserved controller diff before this task:

- `tests/dungeon/dungeon_affix_stress_tests.cpp`
- `tests/dungeon/dungeon_test_support.hpp`
- Task 9 progress/baseline evidence dirt and content-identical EOL/stat dirt

## RED

The exact current guard fails with:

```text
Large persistence object may be automatic in
tests/dungeon/dungeon_exit_unlock_tests.cpp:
arpg::dungeon::checkpoint::SaveCheckpointSlot pending_checkpoint{};
```

Read-only audit found three automatic `SaveCheckpointSlot` declarations in
that file (near the former lines 191, 301, and 407).

## Required result

- Replace exactly those three large automatic values with explicit nothrow
  heap ownership, checking allocation failure before dereference.
- Preserve every existing checkpoint field, mutation, encode/decode call,
  assertion, and test meaning.
- Do not add a stack-guard exemption and do not enlarge any process stack.
- Do not touch production code or unrelated tests.

## Scope

Allowed tracked file only:

- `tests/dungeon/dungeon_exit_unlock_tests.cpp`

Do not modify, stage, or overwrite any preserved controller/user diff. Do not
commit.

## Verification

1. Preserve the exact RED above.
2. Implement the smallest three-site heap conversion.
3. Run `git diff --check` on the allowed file.
4. Run the exact stack guard and require exit 0.
5. In VS2022 x64 Developer PowerShell, build `arpg_dungeon_tests` serially
   with `CMAKE_BUILD_PARALLEL_LEVEL=1` and `--parallel 1`; require exit 0.
6. Report changed lines, commands, timings, warnings, and residual risks.

