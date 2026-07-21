# Stage 11-D Task 8 Review Fix Report

Base implementation commit: `661ff76afc8e8cb8b537aa0e9ca0ccfc82798ee1`

Scope remained Task 8 tests and architecture guards. No production source, Task 9 evidence, Task 10 final gates, or Stage 12 behavior changed.

## Test-first RED

Expanded mutation self-test before guard changes:

```powershell
ctest --test-dir out/build/windows-msvc-debug -R "^stage11d\.architecture\.loot_boundaries_self_test$" --output-on-failure
```

Result: `0/1`; the old guard incorrectly accepted the new `pmr_vector` mutation.

Expanded stress assertions before helper/count changes:

```powershell
out/build/windows-msvc-debug/bin/arpg_dungeon_tests.exe
```

Result: `254 cases, 1 failures`; the old trace failed at `policy_attempts[0] > 0U`.

## Guard hardening

- Dynamic families now cover `basic_string`, all named string aliases, sequential/associative containers, `forward_list`, stack/queue adaptors, and `std::pmr` aliases while allowing `std::array`.
- Presentation checks reject `DungeonSession`; member-call checks target identifiers containing `session` or `store` through `.`/`->`, avoiding unrelated `animation.tick()`, `frame.snapshot()`, and `cache.load()` calls.
- The final guard reuses repository `cpp_source_lexer.cmake` for phase-2 line splices, raw/ordinary strings, characters, comments, and preprocessor include/define handling.
- Include checks remove comments, require every dungeon/settings include operand to be a literal, normalize separators, and reject forbidden relative/absolute path segments.
- Explicit pickup is extracted by sanitized function-definition plus balanced braces; declarations are skipped, so an injected call or forward declaration cannot redirect extraction.
- Automatic policy captures renamed parameters and validates the entire anchored canonical top-level body, rejecting unreachable wrappers and tail decoys.
- Final self-test proves 27 bad mutations fail for their named reason and eight equivalent variants pass.

## Stress hardening

- Exact pickup caches item ID and verifies pending `loot_pickup` kind plus target ordinal.
- After commit it requires the target slot inactive, the ID absent from all ground slots, exactly one owned item with that ID, and the matching claimed bit set.
- The trace records attempts/retained counts separately for normal, magic, and rare thresholds; both restrictive policies demonstrably filter, normal does not, and every retained item is picked after relaxation.
- Session comparison now includes ground source and abyss reward ordinal.
- Existing dungeon and settings 100,000-operation allocation probes remain asserted at zero allocations.

## Final GREEN

```powershell
ctest --test-dir out/build/windows-msvc-debug -R "^stage11d\.architecture\.loot_boundaries(_self_test)?$" --output-on-failure
```

Result: `2/2`; guard `4.36s`, mutation self-test `55.96s` in the focused run.

```powershell
out/build/windows-msvc-debug/bin/arpg_dungeon_tests.exe
$env:ARPG_TEST_FILTER='stage11b.settings_stress'
out/build/windows-msvc-debug/bin/arpg_settings_stress_tests.exe
```

Results: dungeon `254 cases, 0 failures`; settings `3 cases, 0 failures`.

```powershell
ctest --test-dir out/build/windows-msvc-debug -R "^(stage11b\.architecture\.settings_boundaries|stage11c\.architecture\.hud_boundaries(_self_test)?|stage11d\.(architecture\.loot_boundaries(_self_test)?|renderer_integration_guard(_self_test)?)|architecture\.(core_no_raylib|dungeon_no_raylib)|stage11b\.settings_stress\.atomic_reload_zero_alloc)$" --output-on-failure
```

Result: `10/10`, total `74.07s`.

`git diff --check` passed with only existing LF-to-CRLF normalization warnings.
