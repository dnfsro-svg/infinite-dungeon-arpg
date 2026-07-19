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
- Ground-loot view and pickup feedback reject dynamic strings/containers, direct physical input sampling, Session calls, and `SettingsStore` access.
- The real `room_renderer.cpp` rejects direct `SettingsStore` access.
- The extracted `DungeonSession::request_pickup` body rejects rarity/filter policy checks, preserving explicit-pickup semantics.
- The extracted `auto_pickup_eligible` body derives its parameter name, normalizes formatting, removes line comments, and requires an adjacent abyss-source comparison plus `return true` before the monster rarity rule.
- All required files, manifest tokens, mutation targets, and substitutions fail closed.

The mutation self-test rejects the seven required bad variants (`std::string`, `IsKeyDown`, `session.tick`, `SettingsStore`, dungeon dependency from settings, rarity policy in explicit pickup, and missing abyss bypass), plus direct/relative include variants for both dependency directions. It also accepts two semantics-preserving reversed-comparison variants.

## Deterministic stress coverage

- The 1,000-room equipment trace rotates `normal`, `magic`, and `rare` minimum-rarity policies on both same-seed sessions.
- Filtered ground items are verified byte-for-byte and position-for-position unchanged, then picked after relaxing to the default show-all policy.
- Left/right sessions are compared after every defeat, filtered attempt, committed pickup, equipment/recipe transaction, transition, and restart.
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
