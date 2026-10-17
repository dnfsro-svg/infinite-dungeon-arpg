# Balance/HP Task 3 Report — Fixed-percent player health restoration

- Status: DONE
- Commit: `90c4348567a57ce614f5eaa427197cd35e04ee59`

## Scope

- Added only `CombatWorld::restore_player_health_percent(std::uint16_t)`.
- The method calculates from actual player `max_hp` with ceil basis-point scaling, clamps to missing health, returns the amount restored, and rejects zero basis points, invalid max HP, full health, and defeated players.
- It does not call or change `restore_player_resources()`, so it bypasses the abyss `player_resource_restore_bp` multiplier.
- No potion drop, pool, persistence, auto-use, UI, or asset work was included.

## RED evidence

After adding and registering both required tests, the VS 2022 x64 build failed with MSVC C2039 because `CombatWorld::restore_player_health_percent` did not exist. An earlier ordinary-PowerShell failure to locate `cmath` was discarded and was not used as RED evidence.

## GREEN evidence

```powershell
cmake --build --preset windows-msvc-core-debug --target arpg_combat_tests -- -j1
ctest --preset windows-msvc-core-debug -R '^combat\.units$' --output-on-failure
```

The build and CTest passed. Direct execution reported `245 cases, 0 failures`.

## Changed files

- `src/combat/combat_world.hpp`
- `src/combat/combat_world.cpp`
- `tests/combat/player_health_tests.cpp`
- `tests/combat/combat_test_main.cpp`

`git diff --check` passed; no functional concerns remain.
