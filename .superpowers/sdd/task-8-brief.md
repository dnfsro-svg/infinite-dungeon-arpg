# Task 8 Brief — Golden updates, clean builds, and depth-3 live validation

## Scope

Final implementation/validation task in `E:\game\.worktrees\balance-hp-potions` after Task 7 review.

Allowed tracked changes:

- `tests/dungeon/dungeon_abyss_stress_tests.cpp` only for the verified depth-1–3 affix-free golden hash.
- `tests/dungeon/stage16_loot_reinforcement_simulation.cpp` only for the independently verified encounter fingerprint change caused by depth-1–3 affix suppression.
- `tests/dungeon/dungeon_stress_tests.cpp` only to include new potion snapshot/diagnostic fields in existing frozen comparisons.
- `src/platform/raylib/raylib_host.cpp` may repair the Stage 10 ordinary-room driver with the only combat route proven to complete all seven captures: keep movement aimed at the nearest living target; while the player is ready, request slot 1 only when that nearest target is inside the production storm circle, then request slot 0 only when the storm is out of range or returns exactly `cooling_down` and the same target is inside the production draw-slash triangle. Accepted skills return the already-computed movement; all other outcomes fall through to the original light driver. Skills are allowed in ordinary rooms and, after the existing last-target-to-hole routing branch has had first refusal, only in the `abyss_hole_descent` abyss scenario. Every other Stage 10 abyss scenario requests no skills.
- The same source may add a strictly scoped Stage 10 transaction-path resource mode for exactly `player_death`, `room_reset`, `leave_started`, `restarted_failed`, and `abyss_hole_descent`. It must require an automated save-backed run with nonzero validation steps/frame and exit frames; no capture file, periodic capture, screenshot request/directory; no Stage 11/11B/11C/11D/17 mode; and every Stage 12 visual/status field disabled. Only this predicate may skip CombatRenderer/PauseMenu/GameAudio resource initialization and the full `renderer.draw()`. It must still preserve CPU observe/event state, fixed ticks, `BeginDrawing`, `ClearBackground`, a real `present_frame_and_maybe_capture(nullptr)`, presented-frame counting, reached predicates, and save commits. The seven visual Stage 10 scenarios always use complete resources and drawing.
- `src/platform/raylib/raylib_host.hpp`, `tests/platform/pause_host_gate_tests.cpp`, and the platform test-count expectation may change only to expose and matrix-test that strict transaction-path predicate: all five transaction scenarios accepted under the exact automation guard, all seven visual scenarios and every conflicting flag rejected.
- `src/platform/raylib/raylib_host.cpp` may also repair the Stage 17 validation-only `approach_storm` step by reusing the existing production-geometry skill-lane movement with an independent retreat direction before requesting slot 2. This must remain inside Stage 17 validation mode and must not change gameplay, skill values, monster values, the seed, frame budget, or timeout. Keep the existing five-hit golden unchanged for the first focused rerun so the resulting summary proves the balance-sensitive actual value before any golden decision. The focused RED then proved `draw=2 / storm=3 / finisher=0`, and two independent reviews proved five hits mathematically impossible with the approved `135/120/110` HP table, 220 draw damage, and 42 storm damage. Therefore `tests/platform/stage17_skill_stones_game_validation.cpp` and `tests/platform/stage17_skill_stones_validator.ps1` may change only the three exact frozen `storm_strike_hit_count` values from `5` to `3`; no other Stage 17 expectation or validator behavior may change.
- Do not change gameplay outside validation mode, capture/reached predicates, frame requirements, timeout, seed search, or screenshot validators. `tests/dungeon/stage10_formal_game_validation.cpp` must remain unchanged after failed fixture experiments are removed.
- Create `docs/validation/extreme-monster-nerf-health-potions.md`.
- Create screenshots under `docs/validation/evidence/`.

If a stress file needs no change, do not touch it. Report: `.superpowers/sdd/balance-hp-task-8-report.md` (never modify tracked historical `task-8-report.md`).

## 1. Core Debug and justified golden update

Run from a fixed VS environment:

```powershell
. .\scripts\Configure.ps1 -Preset windows-msvc-core-debug -Fresh
cmake --build --preset windows-msvc-core-debug --clean-first -- -j1
ctest --preset windows-msvc-core-debug --output-on-failure
```

Before changing any golden, capture and compare:

- actual vs expected hash;
- 4000 door trials and exact direction hit counts;
- same trace with and without reloads;
- room depths and depth 1–3 affix counts are zero;
- depth 4+ affix rules remain active;
- reward/resolution counts are unchanged.

Only if the sole failure is the already-observed abyss trace hash caused by approved depth 1–3 zero-affix behavior, update exactly that hash constant. Never delete/relax assertions or regenerate unrelated goldens. Re-run core Debug until all core CTest pass.

If the Stage 16 frozen room/equipment/abyss stream fails only at its fingerprint while material probabilities and baseline-vs-interleaved equality still pass, accept only the independently reproduced Task 2 transition `15972885342817959559ULL -> 10882612280735857555ULL`. Change that one constant and preserve every existing assertion.

## 2. Extend stress equality/frozen evidence

In existing snapshot equality/summary logic, add exact comparisons for:

```cpp
lhs.ground_health_potion_count == rhs.ground_health_potion_count
lhs.health_potion_pickup_receipt.valid
    == rhs.health_potion_pickup_receipt.valid
lhs.health_potion_pickup_receipt.commit_generation
    == rhs.health_potion_pickup_receipt.commit_generation
lhs.health_potion_pickup_receipt.restored_hp
    == rhs.health_potion_pickup_receipt.restored_hp
lhs.diagnostics.health_potion_ground_saturation_count
    == rhs.diagnostics.health_potion_ground_saturation_count
```

Also compare every active potion's spawn ordinal, claim ordinal, and Vec3 position. Preserve all existing equipment/common/coupon/exit/room hashes so the new random domain is proven not to disturb them. Treat this as test-hardening; if these comparisons already exist equivalently, do not duplicate.

## 3. Clean Debug graphical build and full CTest

```powershell
. .\scripts\Configure.ps1 -Preset windows-msvc-debug -Fresh
cmake --build --preset windows-msvc-debug --clean-first -- -j1
ctest --preset windows-msvc-debug --output-on-failure
```

Require all Debug CTest pass, including `combat.units`, `dungeon.units`, `platform.units`, and `stage12.item_material_asset_pipeline`. Record exact passed/total/time.

If `stage10.formal_game.capture_after_present` returns code 13 because the
fourth chaos scenario dies in the ordinary dense room before entering the
abyss, first preserve the failed save evidence and compare it with the main
baseline. A validated `root 2068 / room 0 / death_retreat / chaos_chaser`
failure may be repaired only by having the existing Stage 10 validation input
request default skill slots through the public production API while
`!snapshot.is_abyss`, after selecting the nearest target and computing movement.
Keep movement aimed at the nearest target. Request slot 1 only when that target
is inside the production storm circle; when the storm is out of range or
returns exactly `cooling_down`, request slot 0 only when the same target is
inside the production draw-slash triangle. Return movement after an accepted
skill; otherwise preserve the original light driver. Every abyss scenario keeps
the existing light-only driver except `abyss_hole_descent`: after its existing
last-target routing branch has returned early while the target is outside the
hole radius, the same public range-gated skills may run to prevent the verified
hunting-flames environment death and complete the descent. Do not add a
`remaining_targets > 1` restriction. Warning/death/reset/restart scenarios must
remain zero-skill in abyss rooms.
If that seven-capture route succeeds but the unchanged 180-second formal gate
times out in the later transaction paths, use only the strict five-scenario
resource predicate described above. Skip visual resource initialization and
full scene drawing only there, while retaining a real blank presented frame and
all fixed-tick/reached/save semantics. Add the predicate matrix test before the
implementation and preserve the observed failing test as the TDD red state.
Re-run the focused formal test, all Stage 10 evidence guards, the transaction
fixture, the validation game, the content validator, and abyss stress before
resuming the full suite.

## 4. Clean Release build and full CTest

```powershell
. .\scripts\Configure.ps1 -Preset windows-msvc-release -Fresh
cmake --build --preset windows-msvc-release --clean-first -- -j1
ctest --preset windows-msvc-release --output-on-failure
```

Require all Release CTest pass. Verify:

- `out/build/windows-msvc-release/bin/arpg_game.exe` exists;
- both regenerated `items_ui` atlases are copied into the runtime resource tree;
- executable links the approved raylib 6.0 build path.

Record exact passed/total/time.

## 5. Persistence/material boundary audit

```powershell
git diff 44f2e590413b9e17ddbf915a09b6acd9cac511fb -- src/persistence src/items/item_types.hpp src/items/material_catalog.hpp
rg -n "health_potion|生命药" src/persistence src/items
```

Expected: no potion in persistence codec, `ItemOwnershipState` material array, or `MaterialId`; no codec/version change. Any match/change blocks completion unless it is demonstrably unrelated baseline content.

## 6. Live play to depth 3

The main agent will have read the `computer-use:computer-use` skill before dispatch. Use real Windows UI control, not synthetic unit-test claims, and launch:

```powershell
& .\out\build\windows-msvc-release\bin\arpg_game.exe `
  --seed 20260723 `
  --save-dir .\out\validation\hp-potion-depth3-save
```

Use a fresh/empty validation save directory belonging only to this worktree. Play through at least floors/depths 1, 2, and 3. For each floor record:

```text
层数；普通/深渊；怪物词条数量；代表怪物 max_hp；
掉落药瓶序号；掉落时 HP/max_hp；是否立即使用；
提交前 HP；提交后 HP；HUD 文案；高血量留地表现；
退出房间后未用药是否消失；发现的问题。
```

Actively produce all three scenes:

1. HP above 75%: visible red-glass/gold-cap potion, red beam, non-overlapping pure-red `生命药`; it stays grounded.
2. In the same room, lower HP to exactly or below 75%, do not move/press pickup, wait for exact save, then observe HP increase and `生命药 +N HP`.
3. A final monster drops an eligible potion: exits remain closed until save succeeds, then room-clear and heal publish together.

Save at least:

- one screenshot showing a grounded potion/beam/label;
- one screenshot showing committed HUD feedback and healed HP;

under `docs/validation/evidence/`. Inspect each screenshot with `view_image` at original detail. Text must be crisp/high-resolution, pure-color, non-overlapping; icon must be the dedicated red-glass/gold-cap art, not an old material placeholder.

If deterministic seed/path does not naturally produce a required visual scene within a bounded play session, use existing non-production validation/test seams or a fresh seed only if it does not modify gameplay code; document exactly what was used. Never fake screenshots or alter drop rules.

## 7. Validation document

Create `docs/validation/extreme-monster-nerf-health-potions.md` with actual values, not placeholders:

```markdown
# 怪物大幅削弱与生命药验收

- Commit: <git rev-parse HEAD full SHA used for validation>
- Core Debug CTest: <actual passed/total/time>
- Debug CTest: <actual passed/total/time>
- Release CTest: <actual passed/total/time>
- Seed: 20260723
- Save: out/validation/hp-potion-depth3-save
- 深度 1–3 词条：普通/深渊均为 0
- 深度 4 回归：既有词条权重仍启用
- 药瓶事务：提交前无回血；exact commit 后回血；失败/不确定无副作用
- 视觉：独立红玻璃金盖图标、纯色“生命药”、红光柱、HUD 实际恢复量
- 试玩问题：<actual issues, or 未发现阻断问题>
```

Include the per-floor table, the verified golden reasoning, persistence audit, artifact paths, screenshot paths, and any non-blocking observations.

## 8. Final checks and commit

```powershell
git status --short
git diff --check
git diff --stat 44f2e590413b9e17ddbf915a09b6acd9cac511fb
git diff 44f2e590413b9e17ddbf915a09b6acd9cac511fb -- src/persistence src/items
```

No unrelated files, no persistence/material enum expansion, no whitespace errors. Commit only actual allowed changes/evidence with:

`test: validate monster nerf and health potion flow`

Write the unique Task 8 report with all commands/results, final commit hash, screenshot visual observations, changed files, and concerns.

## Completion definition

- Exact 8-monster table; no non-target catalog fields changed.
- Depth 1–3 normal/abyss affixes zero across tests and live play; depth 4+ remains active.
- Independent potion RNG, coupon priority, common coexistence, no save/material expansion.
- 75% boundary, distance-free stable scan, death rejection, fixed capacities, room-destruction semantics frozen.
- Single and room-clear batch heal only after exact commit; failures/indeterminate/mismatch/crash do not replay or side-effect.
- Dedicated high-resolution bottle, pure clear Chinese, red beam, exact heal HUD, no label overlap.
- Core Debug, graphical Debug, Release full suites pass and depth-3 live evidence exists.

If any required live visual conclusion is unreliable, a test/build fails after justified golden update, or permission/external UI blocks real play, return `DONE_WITH_CONCERNS` or `BLOCKED`; never claim full completion.
