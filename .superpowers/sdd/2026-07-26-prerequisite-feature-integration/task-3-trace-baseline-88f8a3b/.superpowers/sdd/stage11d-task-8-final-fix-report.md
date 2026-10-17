# Stage 11-D Task 8 Final Guard Fix Report

Previous review-fix commit: `dd685632602f7d7d1e3dab5187b547232bf995db`

This final review round changed only Task 8 guard scripts, mutation fixtures, and reports. Dungeon/settings production and C++ stress behavior were unchanged.

## Test-first RED

Before replacing the private sanitizer, the new phase-2 mutation was run directly:

```powershell
ctest --test-dir out/build/windows-msvc-debug -R "^stage11d\.architecture\.loot_boundaries_self_test$" --output-on-failure
```

Result: `0/1`, `5.56s`; the old guard incorrectly accepted `std::vec\` followed by a spliced newline and `tor`.

## Final guard changes

- Reused `tests/platform/cpp_source_lexer.cmake`; removed the Stage11-D private lexer.
- All presentation, include, and function checks now consume shared sanitized source, covering phase-2 `\` line splices, raw/ordinary strings, characters, and comments.
- Mutation-mode runs inspect only the changed boundary while the separate production guard still scans the complete production dungeon/settings trees.
- Function extraction skips matching declarations whose first significant terminator is `;`, then extracts the actual definition by balanced braces.
- Automatic pickup must match a complete anchored canonical top-level body: active guard, abyss bypass, monster rarity return, and nothing else.
- Session/store member checks require the object identifier to contain `session`/`store`; class-name bans remain. Unrelated animation/frame/cache calls are accepted.
- Self-test now contains 27 named bad mutations and eight semantics-preserving good variants, including line splices, forward declaration traps, `if(false)` wrapping, raw strings, comments, and harmless member calls.

## Final GREEN

Focused production guard:

```powershell
ctest --test-dir out/build/windows-msvc-debug -R "^stage11d\.architecture\.loot_boundaries$" --output-on-failure
```

Result: `1/1`, `28.87s`.

Focused mutation self-test:

```powershell
ctest --test-dir out/build/windows-msvc-debug -R "^stage11d\.architecture\.loot_boundaries_self_test$" --output-on-failure
```

Result: `1/1`, `102.51s`.

Direct stress executables:

```powershell
out/build/windows-msvc-debug/bin/arpg_dungeon_tests.exe
$env:ARPG_TEST_FILTER='stage11b.settings_stress'
out/build/windows-msvc-debug/bin/arpg_settings_stress_tests.exe
```

Results: dungeon `254 cases, 0 failures`; settings `3 cases, 0 failures`. Existing dungeon/settings 100,000-operation zero-allocation assertions passed inside these executables.

Relevant architecture/stress CTests:

```powershell
ctest --test-dir out/build/windows-msvc-debug -R "^(stage11b\.architecture\.settings_boundaries|stage11c\.architecture\.hud_boundaries(_self_test)?|stage11d\.(architecture\.loot_boundaries(_self_test)?|renderer_integration_guard(_self_test)?)|architecture\.(core_no_raylib|dungeon_no_raylib)|stage11b\.settings_stress\.atomic_reload_zero_alloc)$" --output-on-failure
```

Result: `10/10`, total `151.09s`; Stage11-D production guard `27.78s`, mutation self-test `105.61s`.

`git diff --check` passed with only existing LF-to-CRLF normalization warnings.
