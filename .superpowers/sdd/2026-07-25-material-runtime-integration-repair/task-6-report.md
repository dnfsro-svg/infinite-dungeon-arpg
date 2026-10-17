# Task 6 Report: Complete active-skill material timelines

## Outcome

- Added exact integer visual-frame mapping for all 36 draw-slash frames and all
  24 storm-swords frames without changing combat clip timing, damage, or events.
- Added an allocation-free `ActiveSkillEffectPlan` with explicit material,
  procedural fallback, and no-skill modes.
- Separated player material placement from locked damage-effect placement.
- Built the active-skill plan once per rendered combat snapshot after residency
  synchronization and shared it with the actor and skill render passes.
- Suppressed the base player only when the currently active skill atlas is
  available; material failure leaves the player visible and enables the
  procedural main visual.
- Registered and rendered distinct draw-slash and storm-swords HUD icons.
- Registered exactly four new C++ cases and changed the platform count from
  458 to 462.

## RED evidence

- The first environment-neutral build attempt stopped before compiling the new
  cases because the shell lacked the MSVC standard-library include environment.
- Re-running under `VsDevCmd.bat -arch=x64` produced the intended RED: missing
  `active_skill_visual_frame_index`, the three-argument plan builder, visual
  mode/position/suppression fields, and icon API.

## Frame and allocation evidence

- Draw slash maps ticks 0..89 through integer `tick * 36 / 90`, clamped to 35.
- Storm swords uses the specified four integer intervals and clamps to 23,
  reaching frames 16 and 20..23 as well as every other frame.
- The plan is composed only of scalar values, `Vec3`, and fixed-size arrays.
- A 10,000-iteration alternating material/fallback probe reports zero heap
  allocations.

## Render-path audit

- Readiness is computed from only the currently active skill ID and its atlas;
  an equipped but inactive skill cannot mask a failure.
- Material mode draws exactly one atlas frame at `player_position`, suppresses
  the base player, and skips the procedural fan, triangle swords, and finisher
  ellipse.
- Procedural fallback uses `effect_center`, never suppresses the player, and
  keeps a nonzero main-visual count.
- No combat damage values, logical frame counts, or event timelines changed.

## HUD icon audit

- Draw slash uses atlas frame 18 at `{0,627,209,209}`.
- Storm swords uses atlas frame 20 at `{0,1280,256,256}`.
- Occupied slots draw the dedicated icon into inset bounds before cooldown and
  text overlays; empty slots retain `MaterialSpriteId::missing`.

## Final verification

- `cmake --build --preset windows-msvc-debug --target arpg_platform_tests --parallel 1`:
  exit 0 under the Visual Studio developer environment.
- `ctest --test-dir out/build/windows-msvc-debug -R '^platform\.units$' --output-on-failure -j 1`:
  1/1 passed.
- Direct `arpg_platform_tests.exe`: `462 cases, 0 failures`.
- Python `__pycache__` removed after verification.

## Review round 1

- Strengthened the existing material/fallback plan case without registering a
  fifth case. The RED failed because healthy material storm plans discarded the
  24 spawned secondary sword placements.
- Storm secondary placements are now built independently of material readiness
  and consumed once in both render modes. Material mode still reports zero
  procedural main visuals and keeps the central finisher sword/ellipse disabled;
  fallback still renders its sword plan only once.
- Fresh serial build and `platform.units` verification passed after the fix.
