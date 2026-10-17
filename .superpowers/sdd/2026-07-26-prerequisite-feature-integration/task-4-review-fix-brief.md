# Task 4 external-review fix brief

## Scope

Fix only the deterministic comparison coverage finding in
`tests/dungeon/dungeon_stress_tests.cpp`. Do not change production code, test
registry totals, source gameplay, goldens, or Task 5 files.

## Required behavior

1. `same_snapshot()` must compare
   `pending_health_potion_spawn_ordinal`.
2. Add a narrow comparator for optional `PendingHealthPotionClaim` values. It
   must compare presence, the complete fixed `spawn_ordinals` array, `count`,
   `expected_hp`, and `expected_max_hp`.
3. `commit_equal()` must reject sessions whose pending health-potion claims
   differ by any of those fields before resolving either save.
4. Add negative regression assertions inside an existing registered stress
   test so the dungeon case count remains exactly 306:
   - snapshots differing only in pending potion ordinal are not equal;
   - optional claims differing in presence, count, any spawn ordinal,
     expected HP, or expected max HP are not equal.
5. Follow TDD: add the negative assertions first and record the focused RED
   (runtime failure or compile failure), then implement the two comparator
   changes and rerun GREEN.

Prefer heap allocation for large `DungeonSnapshot` fixtures if needed. Do not
add mutable production hooks or use `const_cast`.

## Verification and commit

- Run the existing potion-focused 19 cases.
- Run the full direct dungeon executable and require exactly
  `306 cases, 0 failures`; this is the authoritative broad check because the
  helper participates in long deterministic stress.
- Run `git diff --check` and inspect the exact one-file diff.
- Commit only `tests/dungeon/dungeon_stress_tests.cpp` as
  `test: harden potion pending-state determinism`.
- Do not push.
- Append RED/GREEN, SHA, clean status, and self-review to `task-4-report.md`
  and `progress.md`; these ignored SDD files must not be staged.

