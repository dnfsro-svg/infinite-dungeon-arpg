# SDD ledger — plan: docs/superpowers/plans/2026-07-31-stage10-dense-fixture-recovery.md

Task 1: RED confirmed at ab9f888 — exact CTest 0/1 and direct exit 8 after 12,000 real-combat ticks, with 560 targets remaining and no stack fault.
Task 1: root cause confirmed — the pre-hundredfold fixture build is underpowered for the current 450-1125-monster abyss contract; the minimum repair is fixture-owned equipment data only.
Task 1: implementation attempt 1 — exact historical fixture equipment applied; target build passed, but direct and exact CTest remain RED with exit 8 and 577 targets after 12,000 ticks. No commit created.
Task 1: BLOCKED pending deeper diagnosis — stronger fixture equipment raised HP from 507 to 729 but did not improve dense-room clear completion; do not expand the tick budget or commit a red repair.
Task 1: fix round 1/5 — five-lane scouting plus Storm/Draw/light implemented; direct run reached a silent service-pending failure, still RED, no commit.
Task 1: fix round 2/5 — diagnostic proved death_retreat save was rejected as invalid_checkpoint; pending-kind omission was not the cause, no commit.
Task 1: fix round 3/5 — diagnostic proved death at tick 10710 with only 3/600 defeated, no-resident count 0, one Storm, zero Draw, six light inputs, and a consumed 192-HP potion. Driver geometry/target lock remains open; no commit.
Task 1: fix round 4/5 — target lock and interior ranged stance implemented; direct raw exit remained 8. Review found dynamic stance drift and a 0.45 X dead zone larger than the 0.25 arrival threshold; no commit.
Task 1: fix round 5/5 — fresh maximum-capability implementer dispatched to freeze the locked stance, use 0.25 movement, and scan all living residents for legal action geometry.
Task 1: fix round 5/5 result — direct exit 8 at tick 12000 with 55/600 defeated, 545 remaining, full 768 HP, 92 barrier, 5 Storm, 11 Draw, and 11 light inputs; no CTest and no commit.
Task 1: BLOCKED — the plan-mandated lock survives while the resident target remains active even after it leaves every attack geometry, so navigation never re-locks or resumes scouting. This is a real load-bearing plan defect after the five-round breaker; preserve the red diff and do not continue this task without a separately redesigned lock-expiry contract.
