# SDD ledger — Stage 10 event-lease fixture recovery

Plan: `docs/superpowers/plans/2026-07-31-stage10-event-lease-recovery.md`

- Baseline committed HEAD: `2eec9c2`.
- Preserved round-5 RED: native exit 8 after 12,000 ticks; 55/600 defeated,
  545 remaining; player 768/768 HP with 92 barrier at a fixed position; 5
  Storm, 11 Draw, and 11 light actions.
- Root cause: the navigation ordinal remains locked while its monster is alive
  and resident even after the frozen stance loses every legal attack geometry.
- Protected pre-card fixture diff hash:
  `8bcc00efda5fc9fd4c6dd030800c3875e8853947`.
- Superseded old Stage 10 plan remains untracked and is excluded from staging
  and commits.
- Task 1 status: ready for implementation.
# BLOCKED

Task 1 exhausted the five-direct-run breaker. Direct round 5 exited natively
with code 8 at 119/600 defeated and exposed a deterministic sweep arrival
deadband: `sweep_movement()` requires `|dx| <= 0.25F` to advance waypoint 0,
while its required existing `movement_toward()` emits zero X input once
`|dx| <= 0.45F`. CTest was not run and no implementation commit was created.
See `task-1-report.md` for the full round evidence and next-card direction.
