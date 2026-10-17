# SDD ledger — Stage 10 close-facing liveness

Plan: `docs/superpowers/plans/2026-07-31-stage10-close-facing-liveness.md`

- Baseline committed HEAD: `feadac9`.
- Preserved inherited fixture+CMake diff hash:
  `0e0846cdaf04a0eccb540b6396376a08aec0556c`.
- RED: 48,914 ticks, 486/600 defeated, exit 8, 23.911s.
- Root cause: a live close lock can enter a permanent zero-input facing
  deadband at `0.2 < |dx| <= 0.45` while the target remains in light range.
- Approved design: at most one action-priority-safe public facing input per
  lock, then existing exclusion/light-lane recovery.
- Task 1 implementation: close-facing correction applied; final serial target
  build exit 0 without warnings.
- Task 1 direct gate: exit 8 after the single allowed run; 562/600 defeated at
  tick 48,914, HP 760, 29 facing corrections, no live lock, and sweep active.
- Old close-facing fixed-point terminal signature: absent.
- Exact CTests: not run because the direct gate was RED.
- Task 1: BLOCKED — unchanged budget exhausted with 38 targets remaining;
  stopped without staging, commit, budget, timeout, CMake, or production edits.
