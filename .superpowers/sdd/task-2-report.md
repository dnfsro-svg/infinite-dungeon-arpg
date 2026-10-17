# Task 2 Report — Protect depths 1–3 from monster affixes

- Status: DONE
- Commit: `54a4f9f87233867d62ac7c65fa68585a4be4b995`

## RED evidence

After adding the focused test before production changes, running the configured
core Debug build and `ctest --preset windows-msvc-core-debug -R '^combat\.units$'
produced `243 cases, 2 failures`:

- `first three depths are affix free in normal and abyss rooms` failed at
  `abyss->count == 0U`.
- `frozen depth bands` failed because depth 3 still used `{80,20,0,0}` rather
  than `{100,0,0,0}`.

The first build attempt ran without the project configuration environment and
failed to find the MSVC standard header `cmath`; its following CTest invocation
used a stale executable and was not treated as RED evidence. The configured
rerun above is the valid RED result.

## GREEN evidence

After the minimal production changes, ran:

```powershell
. .\scripts\Configure.ps1 -Preset windows-msvc-core-debug
cmake --build --preset windows-msvc-core-debug --target arpg_combat_tests -- -j1
ctest --preset windows-msvc-core-debug -R '^combat\.units$' --output-on-failure
```

Build exited 0 and `combat.units` passed. Direct execution of
`out/build/windows-msvc-core-debug/bin/arpg_combat_tests.exe` reported:

```text
243 cases, 0 failures
```

`git diff --check` exited 0 before commit.

## Files changed

- `src/combat/monster_affix_generation.cpp`
- `tests/combat/monster_affix_generation_tests.cpp`
- `tests/combat/combat_test_main.cpp`

## Concerns

None. The configure script prints expected compile-probe failures for poison
headers; configuration, build, and the required combat test all succeeded.
