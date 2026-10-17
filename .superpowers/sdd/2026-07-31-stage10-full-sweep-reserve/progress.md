# SDD ledger — Stage 10 full-sweep reserve

Plan: `docs/superpowers/plans/2026-07-31-stage10-full-sweep-reserve.md`

- Baseline committed HEAD: `898755d`.
- Preserved inherited fixture+CMake diff hash:
  `811392e3c7d0e5857f68feb43fbba45e44cbfc97`.
- RED: 69,600 ticks, 599/600 defeated, exit 8, 29.638s; no live lock and
  deterministic sweep active at waypoint 4.
- Approved design: add one 12,000-tick fixed sweep cycle; keep 96N and timeout
  75 seconds.
- Task 1 status: ready for implementation.
