# Stage 11-A Task 10 Implementation Brief

Implement Task 10 from `docs/superpowers/plans/2026-07-16-stage11a-death-continue.md`: a headless-testable death recap view model and final-pass raylib overlay.

## Required behavior

- Add the four planned platform files and wire them through `CombatRenderer::draw()` after the existing world/HUD rendering.
- The view builder accepts only `const dungeon::DungeonSnapshot&`; the renderer owns no gameplay state and does not access Session, Runtime, SaveStore, stable state, pending saves, or test access.
- Use fixed-capacity/no-throw-friendly view data; no raylib dependency in the view-model header or source.
- Show all planned recap data: room/depth/biome/abyss, lethal source kind and catalog display name with explicit unknown fallback, final-hit raw/barrier/hp/final values, five recent damage totals, defense snapshot, and depth transition.
- State prompts must be exact Chinese text: `正在记录死亡`, `E 继续`, and `保存失败，请重试`.
- Layout must remain inside 1280x720 and the repository's minimum supported window, with panel/text bounds not overlapping the bottom prompt.
- Preserve the existing HUD and input behavior.

## Important interface gap to solve explicitly

The current `DungeonSnapshot::death` exposes only `saving` and `can_continue`. A `death_continue` `not_committed` result resumes `death_pending` with `can_continue=true`, so Task 10 cannot reliably show `保存失败，请重试` from those two flags. Do not infer it from the cumulative diagnostics counter or Runtime save status. Add the narrowest transient Session/Snapshot state needed (for example a `continue_failed` flag), set only for a failed `death_continue`, clear it when a retry begins/succeeds or a new room is published, and test it. Keep persistence unchanged; after process restart the normal `E 继续` prompt is acceptable because no failed attempt exists in that process.

## TDD and validation

1. Add headless RED tests first, including the retry-failed state.
2. Build once and record the expected RED failure.
3. Implement minimally.
4. Run `cmake --build --preset windows-msvc-debug --target arpg_platform_tests arpg_game` and `ctest --preset windows-msvc-debug -R '^platform\.units$' --output-on-failure`.
5. Also run focused death tests and architecture guards impacted by any snapshot/interface change.
6. Commit only Task 10 changes as `feat: render death recap overlay` and write `.superpowers/sdd/stage11a-task-10-report.md` with RED/GREEN evidence and the commit hash.

Do not start Task 11 or real-window Stage 11 validation.
