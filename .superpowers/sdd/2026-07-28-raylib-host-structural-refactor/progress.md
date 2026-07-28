# SDD ledger — plan: docs/superpowers/plans/2026-07-28-raylib-host-structural-refactor.md

Branch: codex/whole-game-refactor-2026-07
Plan commit: 05c3834
Plan correction: 3b3ac0a (fixed-step production-input guard anchor)
Merge baseline: f132f276e5ac8339acb9573146a260193961eeb6
Task 0: pending
Task 0: review findings (round 1 required) — Stage11C model/hash scan narrowed; alias RHS not proven; fixed-step branch structure not proven
Task 0: fix round 1/5 (3 addressed, 0 open — complete-surface model/hash scan; exact alias RHS; fixed-step branch structure; commits 6c173ee..0197327)
Task 0: complete (commits 3b3ac0a..0197327, review clean)
Task 1: review findings (round 1 required) — Stage11C input-source forbidden scan narrowed; final Head not rebuilt/full-selector rerun
Task 1: fix round 1/5 (2 addressed, 0 open — complete input forbidden scan; final Head build/selector evidence; commits dfbef4d..cdecc8b)
Task 1: deferred baseline — architecture.persistence_checkpoint_only fails on unchanged room_progress_codec.cpp -> dungeon/health_potion_loot.hpp dependency
Task 1: deferred build noise — Ninja reported recoverable premature end of file warning; both targets linked, exit 0
Task 1: complete (commits 0197327..cdecc8b, review clean)
Task 2: review findings (round 1 required) — focused CTest selector matched no stage10/stage11 tests and capture-order mutations failed for the wrong prerequisite; shared Stage10/11 source token scans allowed cross-function substitution
Task 2: fix round 1/5 (2 addressed, 0 open — corrected selector/mutation fixtures; per-function Session-route guards; scoped re-review required; FIX_BASE 67b1574)
Task 2: scoped re-review findings (round 2 required) — Stage10 capture mutations still fail additional non-order gates; brace parser counts comment braces and can again span into Stage11
Task 2: fix round 2/5 (2 addressed, 0 open — single-target capture mutations; shared comment/string-safe function isolation with adversarial mutations; scoped re-review required; FIX_BASE 0ef2305)
Task 2: scoped re-review finding (round 3 required) — C++ backslash-newline splicing can continue or form line comments, letting commented route decoys satisfy the scanner
Task 2: fix round 3/5 (2 addressed, 0 open — splice-aware comment scanning and Stage10/11 continuation-decoy mutations; scoped re-review required; FIX_BASE 894f3e6)
Task 2: deferred full-selector evidence — 22 selected; the last full execution was 16 pass/6 non-guard failures, with no claim that those six failures are baseline-proven
Task 2: complete (commits cdecc8b..7defd1c, review clean after 3 fix rounds)
Task 3: Stage11B state/injection/completion/hash/summary extracted into
host_validation_stage11b.hpp/.cpp. Formal 11B evidence remains red at the
same pause-resume predicate: BASE comparable runs observed 1 -> 4 and 1 -> 5,
while HEAD observed 1 -> 5. Record this as same failing predicate with
variable observed tick count; dependent validator not run, and fingerprints
were noncausal after the short circuit.
Task 3: review findings (round 1 required) — formal Step 4 failed and report reversed the short-circuit cause; Stage/source and host/sequence guards permit comment or cross-scope decoys; Stage11B private header missing from dependency scan; unused host include remains
Task 3: fix round 1/5 (all review findings addressed) — corrected formal
causality with comparable BASE evidence; Stage function isolation and
splice-safe decoys; host loop/function brace-depth scope checks including
lambda/out-of-loop full-call decoys; Stage11B header dependency mutation;
removed unused include. Serial build passed; focused selector lists exactly
six and passed 6/6 (581.18s). Commit pending final diff check.
