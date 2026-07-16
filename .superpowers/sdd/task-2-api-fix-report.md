# Stage 9 Task 2 API Fix Report

## Scope

- Restored Task 2's production-facing API to fixed-catalog generation,
  danger scoring, and normal encounter-plan validation.
- Removed production-header declarations for replacement catalogs, external
  catalog validation, and affix-context/random-domain inspection.
- Kept test-only catalog and random-domain probes in
  `tests/combat/monster_affix_test_support.hpp`; production headers do not
  include or declare those hooks.

## Regression coverage

- Invalid catalogs fail before a zero-affix result can be accepted.
- Candidate exhaustion still returns `nullopt`.
- Conflicts are rejected in both selection orders by encounter legality.
- The historical depth/spawn bit-overlap collision is covered.
- Count, selection, and tier random subdomains are independently derived from
  one context and asserted distinct without inspecting implementation strings.

## TDD evidence

- RED: after the tests were changed to request the test-only support header,
  the combat and dungeon test targets failed because that header did not yet
  exist.
- GREEN: adding the local test support and internal production implementations
  built both requested test targets successfully.

## Verification

- `cmake --build build-release --target arpg_combat_tests arpg_dungeon_tests --parallel 4`
- `ctest --test-dir build-release -R "combat.units|dungeon.units|architecture.combat" --output-on-failure`
- `git diff --check`

All five selected tests passed: `combat.units`, `dungeon.units`, and the
three combat architecture-isolation checks. The full selected run completed in
160.27 seconds.
