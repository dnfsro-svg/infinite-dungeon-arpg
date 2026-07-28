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
