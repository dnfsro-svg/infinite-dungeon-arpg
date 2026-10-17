# Stage 11-D Task 8 Implementation Report

Start commit: `1004940`

Scope: architecture guards and deterministic dungeon/settings regression stress only. No production source, formal raylib evidence, Task 9, Task 10, or Stage 12 behavior was changed.

## TDD RED evidence

The guard was registered in `tests/platform/CMakeLists.txt` before its script existed.

```powershell
ctest --test-dir out/build/windows-msvc-debug -R "^stage11d\.architecture\.loot_boundaries$" --output-on-failure
```

Result: `0/1` passed. CTest failed because `tests/platform/stage11d_loot_architecture_guard_test.cmake` did not exist.

The long-trace assertions were then added before their counters/behavior were implemented.

```powershell
$env:ARPG_TEST_FILTER='stage11b.settings_stress'
out/build/windows-msvc-debug/bin/arpg_settings_stress_tests.exe
```

Result: `3 cases, 1 failures`, at `filter_mode_saves[0] > 0U`.

```powershell
out/build/windows-msvc-debug/bin/arpg_dungeon_tests.exe
```

Result: `254 cases, 1 failures`, at `filtered_items_retained > 0U`.

## Implemented boundaries

- Dungeon include paths reject settings and raylib dependencies, including normalized relative paths.
- Settings include paths reject dungeon dependencies, including normalized relative paths.
- Ground-loot view and pickup feedback reject the reviewed dynamic string/container families (including PMR aliases and container adaptors), direct physical input sampling, `.`/`->` Session calls, `DungeonSession`, `SettingsStore`, `SaveStore`, and store load/save/commit/recover calls. Fixed `std::array` remains allowed.
- Ordinary presentation checks use the repository `cpp_source_lexer.cmake`, so phase-2 line splices, raw/ordinary strings, characters, and comments are sanitized consistently. The real `room_renderer.cpp` rejects direct save/store access.
- The extracted `DungeonSession::request_pickup` body rejects rarity/filter policy checks, preserving explicit-pickup semantics.
- Function bodies are extracted from sanitized code by matched definition and balanced braces; declarations ending in `;` are skipped, and bare calls cannot shorten the checked body.
- The extracted `auto_pickup_eligible` body derives both parameter names and must match the complete anchored top-level canonical body: active guard, abyss true bypass, then monster-source + rarity + minimum-rarity return. Extra wrappers and tail decoys are rejected.
- Dungeon/settings include lines are comment-cleaned, every operand must be a literal, and forbidden absolute or relative path segments are rejected.
- All required files, manifest tokens, mutation targets, and substitutions fail closed.

After final review hardening, the mutation self-test rejects 27 named bad variants, including phase-2 line-splice bypasses, forward-declaration extraction traps, and an `if(false)` canonical-policy wrapper. Eight semantics-preserving variants cover reversed comparisons, parameter renaming/function reformatting, a valid forward declaration plus unrelated function, raw strings/comments, harmless non-session/non-store member calls, and commented include decoys.

## Deterministic stress coverage

- The 1,000-room equipment trace rotates `normal`, `magic`, and `rare` minimum-rarity policies on both same-seed sessions.
- Filtered ground items are verified byte-for-byte and position-for-position unchanged, then picked after relaxing to the default show-all policy.
- A successful pickup verifies the exact pending kind and ordinal, then verifies the target slot is inactive, its item ID has left every ground slot, ownership contains it exactly once, and its claimed bit is set.
- All three policies record actual attempts; normal retains none while both restrictive policies retain items, and relaxed pickup count equals the total retained count.
- Left/right sessions compare ground source and abyss reward ordinal in addition to content/position after every defeat, filtered attempt, committed pickup, equipment/recipe transaction, transition, and restart.
- A 100,000-operation `auto_pickup_eligible` probe rotates rarity, policy, monster source, and abyss source and asserts zero heap allocations.
- The 1,000-save settings trace records the exact deterministic filter distribution `{333, 334, 333}` across both reruns.
- The settings/input/control-hint hot-path probe now runs 100,000 iterations while rotating all three filter modes and asserts zero heap allocations.

## GREEN evidence

Guard and mutation self-test:

```powershell
ctest --test-dir out/build/windows-msvc-debug -R "^stage11d\.architecture\.loot_boundaries(_self_test)?$" --output-on-failure
```

Result: `2/2` passed.

Direct stress executables:

```powershell
out/build/windows-msvc-debug/bin/arpg_dungeon_tests.exe
$env:ARPG_TEST_FILTER='stage11b.settings_stress'
out/build/windows-msvc-debug/bin/arpg_settings_stress_tests.exe
```

Results: dungeon `254 cases, 0 failures`; settings `3 cases, 0 failures`.

Relevant architecture/stress CTests:

```powershell
ctest --test-dir out/build/windows-msvc-debug -R "^(stage11b\.architecture\.settings_boundaries|stage11c\.architecture\.hud_boundaries(_self_test)?|stage11d\.(architecture\.loot_boundaries(_self_test)?|renderer_integration_guard(_self_test)?)|architecture\.(core_no_raylib|dungeon_no_raylib)|stage11b\.settings_stress\.atomic_reload_zero_alloc)$" --output-on-failure
```

Result: `10/10` passed.

`git diff --check` passed; Git emitted only the repository's existing LF-to-CRLF normalization warnings.

## Review-fix TDD evidence

The review hardening was also test-first. Before the guard update, the expanded self-test failed because the old guard accepted `std::pmr::vector`. Before exact pickup/count implementation, the full dungeon executable failed at `policy_attempts[0] > 0U` with `254 cases, 1 failures`.

After the final fixes, production guard and self-test passed with 27 bad and eight good variants, dungeon passed `254/254`, settings passed `3/3`, and the same relevant architecture/stress selection passed `10/10`.
