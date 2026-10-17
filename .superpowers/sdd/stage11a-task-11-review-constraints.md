# Stage 11-A Task 11 Review Constraints

Review the implementation against Task 11 and the design stress requirements.

- The stress suite must really run 1000 complete production-path death/continue cycles, not 1000 shallow helper iterations.
- Ordinary/deep/abyss cases, V6 round trips, pending-death restarts, post-continue restarts, saturation, golden streams, and two batching rhythms must all be observable in assertions.
- No private state injection, fabricated death snapshot, direct stable-state mutation, test-access loophole, or disabled validation.
- Allocation probes must measure the named hot paths and assert exactly zero; setup/codec/file I/O may be outside probes where the design permits.
- Hash/checkpoint/target comparisons must be strong enough to detect rerolls and sequence drift.
- Architecture guard must scan all planned boundaries and production test-access names without false-negative path gaps.
- CTest name, environment, labels, and 300-second timeout must match the plan.
- No Task 12 scope creep.

Report Critical, Important, and Minor findings with evidence, or explicitly Approve.
