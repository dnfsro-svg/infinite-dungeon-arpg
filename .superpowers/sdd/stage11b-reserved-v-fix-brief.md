# Stage 11-B cross-task invariant fix: reserve V

## Scope

Fix the Task 1/2 invariant before Task 4: `V` is the global screenshot safety key and must never be accepted as a gameplay binding, while remaining in `StableKey` for platform/global sampling.

## Required TDD behavior

1. Add a failing settings-types test proving `validate_settings` returns `SettingsValidationError::reserved_key` when any binding is `StableKey::v`.
2. Add a failing test proving `assign_or_swap(..., StableKey::v)` returns false and leaves the entire settings value unchanged.
3. Ensure the 1,000-swap stress never selects V as an assignable candidate.
4. Add a codec test proving a correctly re-CRC'd record containing V decodes as `SettingsCodecError::invalid_settings`.
5. Implement the smallest fix: add `reserved_key`, reject V during validation and assignment, preserve the stable vocabulary and file layout.
6. Run focused settings tests plus platform/persistence boundary tests.

## Constraints

- Do not begin Task 4.
- Do not change the 44-byte settings format, defaults, global input behavior, or raylib host.
- Use `apply_patch`; preserve unrelated changes.
- Commit the design/plan clarification, tests, and implementation together.
- Write `.superpowers/sdd/stage11b-reserved-v-fix-report.md` with RED/GREEN evidence, commands, test counts, and commit SHA.
