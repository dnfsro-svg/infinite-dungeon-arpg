# Task 5 implementation report

## Result

Task 5 is complete on baseline `c6ecd4f090d47fdede97f4a601c9909eb61624cd`.
The implementation writes checkpoint V9, persists and restores large-room
authority, moves codec/file work to the bounded persistent worker, and keeps
the default Windows 1 MiB stack reserve. The foundation, runtime, and worker
reviews all closed with P0 = 0, P1 = 0, P2 = 0 and `APPROVED` verdicts.

## Implemented scope

- Added fixed-capacity room/combat checkpoint models for 1,152 monsters,
  ordered equipment/material/potion ground state, defeat/claim ledgers,
  environment state, death state, damage history, RNG, attacks, effects, and
  skill cooldowns.
- Added explicit little-endian V9 encoding with `ARPGSV9\0`, CRC and bounded
  decode, while retaining V1-V8 loading and V8-to-fresh-room migration.
- Added atomic blueprint-first validation and restore. Fallible validation is
  complete before live authority is published; declared transients alone are
  cleared and physical gameplay input must rearm.
- Added heap-owned preallocated persistence storage and a persistent worker
  with exact/background arbitration, byte-exact mirror verification,
  equal-revision final scans, deterministic start-failure rollback, bounded
  queueing, shutdown join, and recovery through the same V9 path.
- Wired five-second coalesced room-progress saves and exact transaction,
  death, clear, transition, recovery, and clean-shutdown saves into the host
  loop without synchronous render-thread file I/O.
- Replaced large automatic snapshot/checkpoint paths with reusable heap/output
  storage, including `DungeonSession::snapshot(DungeonSnapshot&)`, so the
  executable remains valid with the default 1 MiB PE stack reserve.
- Kept Stage17 timing, material, frame and hit assertions strict. Its formal
  validation is split into deterministic production, storm, and restart
  scenarios. Production completes loadout transactions, closes the inventory,
  advances 150 normal empty-input authority ticks until cooldown zero, and
  then completes the clean-shutdown exact save. Restart verifies the final
  loadout and zero cooldowns from that durable save.

## Verification

Exact serial build command from the brief passed:

```text
cmake --build out/build/windows-msvc-debug --target arpg_core_tests arpg_combat_tests arpg_persistence_tests arpg_dungeon_tests arpg_platform_tests arpg_stage17_skill_stones_game_validation -- -j1
```

The first exact combined CTest run completed all nine requested gates. Eight
passed; `persistence.units` exposed two newly added integration expectations
that incorrectly treated protected abyss `not_committed` receipts as
rollback-to-locked. Production and the existing dungeon authority tests require
those receipts to fail closed. The integration expectations were corrected to
require `faulted` while retaining the `disk_old` atomicity assertion; no
production behavior changed. The focused complete persistence registry then
passed.

| Gate | Result | Evidence |
|---|---:|---|
| `core.units` | PASS | 0.05 s in exact combined run |
| `combat.units` | PASS | 1.35 s in exact combined run |
| `dungeon.units` | PASS | 330 cases, 0 failures; 322.50 s in combined run |
| `persistence.units` | PASS after expectation correction | 100 cases, 0 failures; 82.66 s focused rerun |
| `persistence.save_commit_stack_guard` | PASS | 2.72 s; default 1 MiB stack path |
| `platform.units` | PASS | 495 cases, 0 failures; 14.96 s in combined run |
| `stage17.skill_stones.real_raylib` | PASS | 20.38 s in combined run |
| `stage17.skill_stones.evidence_validator` | PASS | 1.25 s in combined run |
| `stage17.skill_stones.evidence_validator_self_test` | PASS | 1.74 s in combined run |

Additional Stage17 summary evidence records
`production_cooldown_start_ticks=150`,
`production_cooldown_wait_ticks=150`,
`production_cooldowns_zero_before_shutdown=1`,
`restart_persisted=1`, `restart_cooldowns_zero=1`, and
`clean_shutdown_exact_ready=1`.

`platform.host_input_source` also passed its final semantic version. Its
character-by-character CMake source sanitizer is slow on the enlarged host
file (187.59 s in the confirmed passing run). A later unverified performance
experiment was fully reverted, so the committed guard is the already-passing
semantic version. This is a test-runtime concern, not a gameplay or persistence
correctness failure.

No temporary seed scanner source, scanner CMake target, or scanner reference
remains. Build/evidence output stays under the ignored `out/` tree for the
controller's independent post-commit verification.

## Remaining concerns

- The host-input architecture source guard should eventually be moved from the
  generic character-at-a-time CMake lexer to a bounded compiled/static-analysis
  helper; doing that safely is outside Task 5 and was not mixed into this
  persistence commit.
- The controller requested an independent final combined gate after the commit;
  this report therefore records the passing component evidence and the one
  corrected test-only expectation rather than claiming a second combined run.
