# Task 10 review fix: bind formal skill evidence to actual rendering

Base commit: `db1271d`

## Confirmed review defects

1. `present_integration_frame` reconstructs an `ActiveSkillEffectPlan` after
   `CombatRenderer::draw` and writes `skill_drawn`, `suppress_base_player`, and
   `procedural_main_visual_count` from that prediction.  A disconnected or
   failed `ActiveSkillRenderer::draw_world` can therefore pass Stage 12.
2. In the healthy material branch, `ActiveSkillRenderer::draw_world` still
   calls `draw_storm_swords`.  The plan reports zero procedural main visuals,
   but visible procedural triangle swords can actually be drawn over the atlas.
3. Stage 17 writes healthy/missing-material fields from pure functions in the
   parent fixture.  Its production raylib host and screenshots are not bound to
   those fields, so they cannot serve as real renderer evidence.

Review severity: item 1+2 are Critical; item 3 is Important.

## Authorized production changes

- `src/platform/raylib/active_skill_renderer.hpp/.cpp`
- `src/platform/raylib/combat_renderer.hpp/.cpp`
- `src/platform/raylib/actor_renderer.cpp`
- `src/platform/raylib/raylib_host.cpp` only to observe and report the new
  read-only renderer status during the existing Stage 17 validation scenario.
- Existing active-skill/platform unit tests only as needed, without changing
  gameplay semantics.
- `tests/platform/stage12_material_formal_game_validation.cpp`
- `tests/platform/stage12_material_validator.ps1` only if its column checks
  must be tightened to consume the corrected runtime evidence.
- `tests/platform/stage17_skill_stones_game_validation.cpp`
- `tests/platform/stage17_skill_stones_validator.ps1`
- `tools/package_material_pack_v2.py` only to update the exact formal
  screenshot/capture-prime count contract from 64 to 70.
- `tests/platform/package_material_pack_v2_tests.py` only to update the same
  exact expected count in the existing 23-case suite; do not add a case or
  relax any file/hash/evidence requirement.
- `tests/dungeon/stage10_evidence_guard_test.cmake` only to update its exact
  positive source contract for the capture helper from `void`/discarded
  `ExportImage` to the real boolean export result and completion gating.  Keep
  every negative guard and ordering constraint unchanged.
- `docs/validation/material-runtime-integration-repair.md`

No other production or gameplay file is authorized.  Do not change timing,
damage, AI, collision, input, assets, residency policy, packaging membership,
or visual art.

## Required implementation

1. Introduce a small read-only `ActiveSkillDrawRuntimeStatus` (or an equally
   direct equivalent) produced by the actual render pass.  It must record:
   active mode/atlas/frame, whether `MaterialPack::draw_frame` actually
   returned true, whether the base player was actually drawn, and the number
   of procedural main-visual routines actually invoked.
2. Make `draw_material_active_skill` return the real `draw_frame` result.
   Make `ActiveSkillRenderer::draw_world` return its actual status.  Count a
   procedural main visual only when its visible procedural draw routine is
   actually invoked.
3. The material branch must not invoke the draw-slash fan, triangle-sword
   field, or ellipse finisher.  Keep only non-main supplementary effects that
   are explicitly allowed by the existing design (the screen flash may stay).
   Prefer also making the material plan's procedural visibility false so its
   state cannot contradict execution.
4. Make the actor pass report whether it actually entered the base-player draw
   path.  `CombatRenderer` stores one status for the most recent `draw` and
   exposes a const accessor.  Reset it at the start of every draw so stale
   evidence cannot pass.
5. Stage 12 must consume only that stored runtime status after `renderer.draw`.
   It may use the pure plan only for deterministic atlas-frame coverage, never
   for `skill_drawn`, base-player suppression, or procedural draw evidence.
   Healthy frames require: material frame actually drawn, base player not
   drawn, actual procedural count zero.  Missing-map frames require the exact
   inverse: material frame not drawn, base player drawn, actual procedural
   count nonzero.
6. Add a skill-off baseline for the same room/player/monster snapshot at each
   formal resolution and export a player/skill ROI derived from the production
   projection and actual atlas-frame geometry.  The validator must require a
   meaningful connected pixel difference between the baseline and captured
   material skill frame.  Do not use whole-frame color percentages or a fixed
   stale rectangle.  This pixel gate complements, rather than replaces, the
   actual runtime status.
7. Stage 17 production validation must sample the stored renderer status after
   the existing real host draw.  Its summary/pass gate must prove that both
   skills had a material frame actually drawn, no base-player draw, and zero
   actual procedural main visuals.  The parent fixture may retain frame-union
   mapping as explicitly named pure timeline evidence, but must not generate
   healthy or missing-map renderer claims from `make_active_skill_effect_plan`.
Missing-map runtime proof belongs to Stage 12; remove or explicitly relabel
   Stage 17's synthetic missing-map fields so the validator cannot treat them
   as host evidence.
8. Add/extend tests proving the material-ready storm plan does not expose
   procedural swords and that runtime evidence defaults/reset behavior cannot
   self-satisfy.  Keep the platform case total at 473 unless a new case is
   genuinely unavoidable; extending existing cases is preferred.
9. Packaging must fail closed unless both `screenshot_count` and
   `capture_prime_count` are exactly 70, accounting for the six new skill-off
   baseline captures.  No other package membership or numeric contract changes.
10. A validation capture is complete only when the requested image was valid
    and `ExportImage` returned true.  Stage 17 and the existing Stage 10/11
    capture-completion flags must not advance on export failure.

## Verification

Run serially and report exact results:

```powershell
cmake --build --preset windows-msvc-debug --target arpg_platform_tests arpg_stage12_material_formal arpg_stage17_skill_stones_game_validation --parallel 1
ctest --test-dir out/build/windows-msvc-debug -R '^(platform\.units|stage12\.(material_formal|material_evidence_validator)|stage17\.skill_stones\.(real_raylib|evidence_validator))$' --output-on-failure -j 1
cmake --build --preset windows-msvc-release --target arpg_stage12_material_formal arpg_stage17_skill_stones_game_validation --parallel 1
ctest --test-dir out/build/windows-msvc-release -R '^(stage12\.(material_formal|material_evidence_validator)|stage17\.skill_stones\.(real_raylib|evidence_validator))$' --output-on-failure -j 1
```

Inspect the corrected skill CSV rows and baseline/skill ROI pairs at original
size for all three resolutions and both healthy skills plus the missing-map
path. Re-run the 30-monster stress gate and confirm stable texture counts,
average FPS >= 60, and 1% low >= 45. Confirm no build/game/test process remains,
`git diff --check` passes, the worktree contains only authorized changes, then
commit as `fix: bind skill validation to actual rendering`.
