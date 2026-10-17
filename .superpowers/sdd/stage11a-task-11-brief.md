# Stage 11-A Task 11 Implementation Brief

Implement Task 11 exactly as specified in `docs/superpowers/plans/2026-07-16-stage11a-death-continue.md`.

## Scope

- Add the dedicated `ARPG_STAGE11_DEATH_STRESS_ONLY=1` suite and CTest registration.
- Run 1000 production-path deaths mixing depth-1 clamp, deep ordinary death, and formal abyss death.
- Use real Combat damage, Session pending transactions, codec V6 encode/decode, Session reconstruction, and continue receipts. Do not inject private death state or weaken validation.
- Compare identical roots under different batching rhythms, including the specified 17/31/43 restart cadence, checkpoint/target descriptors, and cumulative hashes.
- Exercise a saturated production ground pool and prove no ground state leaks into the death checkpoint while saturation diagnostics remain stable.
- Allocation probes must cover the Combat ledger/death tick and Dungeon death prepare/commit hot paths with exactly zero allocations. Working set is diagnostic only.
- Preserve existing door/descent/abyss/drop golden streams before and after the loop.
- Add the architecture guard with every planned dependency/private-access rule.

## Process

1. Write stress and guard tests first.
2. Record a valid RED from missing registration/failing behavior.
3. Fix only genuine production defects. Do not lower 1000 iterations, remove restart cadences, relax zero allocation, or increase the planned 300-second timeout to hide a regression.
4. Run the exact Task 11 GREEN commands and relevant existing architecture/Dungeon checks.
5. Commit only Task 11 work as `test: stress death continue determinism` and write `.superpowers/sdd/stage11a-task-11-report.md` with RED/GREEN evidence and commit hash.

Do not start Task 12 or add real-window validation hooks.
