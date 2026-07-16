# Task 6 report: active monster affix triggers

Baseline: `1296fac6034cb2ab8949a0da1bdf0c2c6df50c92`

## Implemented

- Fixed-array multishot fan (`std::array<Vec3, 4>`), M1/M2/M3 counts 2/3/4 and per-projectile 75/60/50% damage.
- Burning-ground periodic fire hazards with frozen interval, radius, lifetime, 30-tick damage cadence, kind projection, and fixed pool allocation.
- Chain-lightning warning hazards for successful direct melee hits and for each chain-marked projectile end; projectile state carries the owner affix snapshot and chain hazards cannot recursively trigger.
- Blink-assault cooldown/warning state, AI pause, bounded teleport, next-direct-hit empowerment, and multiplicative Frenzy interaction.
- `HazardKind`, hazard persistence projection, warning event kinds, and safe full-pool downgrade. Warning events are emitted only after the corresponding warning state/hazard is created.

## Tests and verification

1. Wrote four active-trigger tests first and ran the requested RED cycle: all four failed for missing multishot, burning, chain, and blink behavior.
2. Added tiered M1/M2/M3 value checks and full 384-projectile / 96-hazard pool downgrade checks.
3. Ran with the Visual Studio developer environment and `WindowsSDKVersion=10.0.26100.0\\`:

   ```text
   cmake --build build-release --target arpg_combat_tests
   ctest --test-dir build-release -R combat.units --output-on-failure
   125 cases, 0 failures
   git diff --check
   ```

The normal PowerShell session did not export the required MSVC/SDK environment; the developer-command prompt supplied it for each verified build.
