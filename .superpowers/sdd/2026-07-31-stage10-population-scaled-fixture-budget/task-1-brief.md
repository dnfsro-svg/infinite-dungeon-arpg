# Task 1 brief — Stage 10 population-scaled fixture budget

Implement Task 1 from
`docs/superpowers/plans/2026-07-31-stage10-population-scaled-fixture-budget.md`
exactly.

The inherited fixture diff is intentional and must remain the base. Modify only
the fixture, its single CTest timeout, the tracked plan, and this SDD workspace's
report/progress files. Do not touch production code or superseded untracked
plans.

Use the recorded RED without rerunning. Build serially. Run the native fixture
first; run the two exact CTest selectors only if native exits 0. On any failure,
stop without staging or committing and report the exact evidence.
