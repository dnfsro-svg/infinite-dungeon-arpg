# Stage 14 Task 3 report — deterministic combat audio routing

## Status

Completed and committed as an isolated Task 3 change. The platform unit target
reports exactly `324 cases, 0 failures`.

## TDD evidence

1. Added and registered exactly eight `audio_routing` cases before production
   code. The expected platform case count was changed from 316 to 324.
2. RED was observed in the formal MSVC environment with:

   ```powershell
   . .\scripts\Configure.ps1 -Preset windows-msvc-release -Fresh
   cmake --build --preset windows-msvc-release --target arpg_platform_tests
   ```

   The build stopped with `fatal C1083` because `audio_routing.hpp` was absent
   from `combat_feedback_tests.cpp`, `monster_view_tests.cpp`, and the new
   `audio_routing_tests.cpp`.
3. Implemented the minimum pure routing, selection, and budget state needed to
   satisfy those tests. One migrated legacy test initially omitted its prior
   implicit J1 attack identity; it was corrected to set `AttackId::j1`, then
   the complete suite was rerun.

## Implemented behavior

- `CombatEvent -> AudioPlan` routes J1/J2/air J, J3, launcher, impact/heavy
  impact, player hurt, landing, defeat, and the three affix warning events.
- `AudioSelectionState` round-robins the two light-swing and three impact
  assets deterministically and restarts both rotations on `reset()`.
- `AudioPlaybackBudget` enforces the specified per-tick group limits, keeps
  warnings independently rate-limited for 12 ticks, accepts a wrapped tick as
  a new warning epoch for compatibility with the former throttle behavior,
  and performs no heap allocation in the 100,000-iteration test.
- `CombatAudio::consume_event(reset)` and `CombatAudio::stop_all()` both reset
  routing selection and budget state. The pure module has no dungeon include
  or dungeon event dependency; existing room-destroyed/room-reset host paths
  continue to use `stop_all()`.
- The legacy `AudioCue`, route function, and warning throttle were removed
  from `combat_audio.*`; legacy tests now include `audio_routing.hpp`.

## Final verification

```powershell
cmake --build E:\game\.worktrees\stage14-audio-material-pack\out\build\windows-msvc-release --target arpg_platform_tests
ctest --test-dir E:\game\.worktrees\stage14-audio-material-pack\out\build\windows-msvc-release -R platform.units --output-on-failure
```

Result: build succeeded; `platform.units` passed; the direct test executable
reported `324 cases, 0 failures`.

## Follow-up concern

This task deliberately keeps the existing procedural playback objects as a
temporary facade mapping. Task 4 is responsible for replacing that mapping
with `AudioPack` loading and the per-asset fallback waves.
