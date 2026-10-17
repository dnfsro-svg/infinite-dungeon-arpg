# Task 3 brief: replay approved early-floor monster balance

## Preconditions

- Worktree: `E:/game/.worktrees/integration-large-square-rooms`
- Branch: `codex/integration-large-square-rooms`
- Start only after Task 2 review fix is committed and independently approved.
- Serial builds only (`-j1` / `CMAKE_BUILD_PARALLEL_LEVEL=1`).
- No active build, launcher, game, or test process before starting.

## Scope and invariants

Implement exactly Task 3 in
`docs/superpowers/plans/2026-07-26-prerequisite-feature-integration.md`.

1. Replay `cd31982` and `c738053` without committing first. Review the exact
   combined diff; import only the approved catalog and repaired combat fixtures.
2. Require catalog-order HP/damage pairs:
   `(100,36)`, `(190,27)`, `(315,21)`, `(135,0)`, `(110,12)`, `(125,18)`,
   `(120,14)`, `(145,11)`.
3. Preserve movement, timing, threat cost, tags, shields, break, projectile,
   feedback, and all unrelated gameplay/material state.
4. Build and run `combat.units`; commit the catalog/fixture slice as
   `balance: sharply reduce monster base stats`.
5. Replay only `54a4f9f`. Require zero normal and abyss affixes at depths 1-3,
   and byte-for-byte-equivalent depth-4+ weights/behavior except any registry
   count reconciliation required by the current combined suite.
6. Build and run the exact combined combat/dungeon/stage16 gates from the plan.
   Update a frozen hash only when a trace proves the sole semantic difference
   is the approved shallow zero-affix rule.
7. Commit the affix slice as
   `balance: suppress monster affixes through depth three`.

Do not cherry-pick or import `742ce70`, `b57930e`, or `2422d13`. Do not modify
launcher, potion, material, room-size, camera, framerate, threading, or GPU
code. Do not push.

## Required report

Write `task-3-report.md` with:

- start and final HEADs;
- exact changed-file list per commit;
- exact catalog pairs and shallow-affix rule readback;
- command lines and pass/fail totals;
- any conflict and why the chosen side preserves the current integration;
- deterministic golden trace, if any golden changes;
- `git diff --check`, clean status, self-review, and concerns.

