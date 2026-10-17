# Task 9 follow-up: Stage 9 real-combat evidence

Base commit: `e416340`

## Problem

`stage9.evidence.no_injected_defeat_trace` is RED because the 1000-room
Stage 9 trace calls `arpg::test::relay_defeated`, which directly mutates the
defeat field/ledger and relays a synthetic defeated event. The authoritative
Stage 9 evidence contract in `docs/validation/stage9-monster-affixes.md` and
the approved recovery plan explicitly accepts the narrow test seam that sets
the selected real runtime HP to zero, calls the real private
`CombatWorld::defeat_monster`, and lets `DungeonSession::relay_combat_events`
perform production ledger/drop/event handling. Do not replace this 1000-room
stress with hundreds of thousands of public-input combat ticks.

## Required result

- Replace only the trace's synthetic `relay_defeated` path with the existing
  narrowly scoped `defeat_room_monster_by_ordinal` real-defeat seam.
- Generalize that helper only enough to accept both affixed and zero-affix
  blueprints while preserving all blueprint/runtime/count/bit/event checks.
- Observe the actual relayed defeated event returned by the helper; drops must
  arise from the production event relay.
- The 1000-room trace must defeat every generated monster, including monsters
  with zero affix score, and preserve deterministic trace comparison.
- The deep-40 fixture must remain unchanged and continue selecting its
  deterministic affixed target, validating its real defeat payload, drop,
  pickup, V4 save/reload, and summary.
- Keep the existing Stage 9 evidence guard unchanged and GREEN once the trace
  no longer contains `relay_defeated(`. Do not weaken existing assertions.
- Preserve `stage10.validation_fixture.real_abyss_transactions` at the
  approved 75-second worst-case budget. The current design records a legal
  49.1-second upper path even though fresh successful runs are faster.

## Scope

Allowed files only:

- `tests/dungeon/dungeon_affix_stress_tests.cpp`
- `tests/dungeon/dungeon_test_support.hpp`

No production-code changes, no fixture changes, no guard changes, no
golden/evidence-image changes, no synthetic event/ledger/defeat-bit injection,
no timeout relaxation, and no unrelated test rewrites.

## TDD and verification contract

1. Record the existing exact RED guard output before implementation.
2. Implement the smallest adaptation of the approved real-defeat helper and
   replace the trace's synthetic relay call.
3. Build serially (`--parallel 1`) in the existing Debug preset.
4. Run serially and report exact outcomes/timings for:
   - `stage9.evidence.no_injected_defeat_trace`
   - `stage9.validation_fixture.real_v4_reward_reload`
   - `stage10.validation_fixture.real_abyss_transactions`
   - direct `arpg_dungeon_tests` with
     `ARPG_STAGE9_AFFIX_STRESS_ONLY=1`
5. Do not commit. Report changed files, commands, timings, and residual risks.
