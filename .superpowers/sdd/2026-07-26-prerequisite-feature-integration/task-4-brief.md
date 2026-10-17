# Task 4 brief: deterministic atomic HP-potion gameplay

## Preconditions

- Worktree: `E:/game/.worktrees/integration-large-square-rooms`
- Branch: `codex/integration-large-square-rooms`
- Start only after Task 3 independent approval.
- Serial builds only. Never overlap build/test processes.
- Preserve all Task 1-3 launcher, recovery, balance, material, and golden gates.

## Binding gameplay contract

- Healing API: `restore_player_health_percent(2500U)` restores exactly 25% of
  max HP, clamps at max, returns the exact amount restored, and allocates
  nothing.
- Drop chance: exactly 3000 basis points in an independent deterministic RNG
  domain keyed by `(room_seed, spawn_ordinal)`.
- Auto-use eligibility: HP at or below 75% of maximum; HP above 75% leaves the
  potion on the ground. Use integer-safe comparisons at the exact boundary.
- Potion state uses fixed-capacity records and deterministic ordered batch
  choice; no dynamic allocation in the fixed gameplay path.
- A potion cannot heal, disappear, or publish a receipt until the exact save
  commit succeeds. `not_committed` retries the same claim unchanged;
  `indeterminate` faults and cannot duplicate healing.
- Potion claim ordinal shares the existing per-monster secondary material
  claim ordinal exactly as the approved source series does.
- Room-clear and abyss-clear must atomically compose material vacuum, potion
  claim, abyss cleanup, max-HP result, heal result, and one coherent next state.
- Transition removes prior-room potion presentation state.

## Execution

Follow Task 4 in the plan exactly:

1. Port/freeze tests first and record the expected RED compile failures.
2. Replay `90c4348`, `82379ae`, `2eabc1b` in order, resolving only current
   registry totals and running the focused executable after each dependency.
3. Apply `37a0848 a83531f 78da4bf 6ffa3cf` with `cherry-pick -n` as one
   reviewed transaction unit.
4. Manually transfer only missing potion comparisons into stress tests.
   Trace every changed golden; never copy dirty Stage 10/11 evidence, guards,
   screenshots, renderer/font/host/Stage12/validation files, or any unrelated
   state from `E:/game/.worktrees/balance-hp-potions`.
5. Final serial build and exact combat+dungeon CTest gate must pass; direct
   executables must report the current combined counts, including all 306
   approved dungeon HP-potion cases.
6. Commit only the reviewed gameplay/test unit as
   `feat: add atomic health potion drops`; do not push.

## Review and report

Before commit, inspect the complete diff against the Task 3 HEAD and prove
there are no renderer/material/host/evidence/room/performance changes. Write
`task-4-report.md` with RED evidence, replay/conflict decisions, API and
boundary readbacks, exact transaction invariants, deterministic/golden traces,
test totals, changed files, commit SHA, clean status, and concerns. Update the
ignored progress ledger.

