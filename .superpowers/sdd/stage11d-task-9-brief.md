# Stage 11-D Task 9 Brief: Formal Raylib Evidence

Work only in `E:\game\.worktrees\stage11d-loot-filter` on branch `codex/stage11d-loot-filter`.

## Objective

Implement Task 9 from `docs/superpowers/plans/2026-07-18-stage11d-ground-loot-filter.md`: six deterministic formal raylib scenarios, semantic evidence validation, mutation guard/self-test, and fresh committed PNG evidence. Reuse the production raylib host, production dungeon/settings/session paths, the existing post-`EndDrawing()` capture helper, and patterns from Stage 11-B/11-C.

## Mandatory scenarios

1. Show all: normal, magic, and rare ground labels visible together.
2. Magic+: normal item remains in the production snapshot but is not rendered; magic and rare remain visible.
3. Rare only: only rare ordinary ground loot visible.
4. Rare only plus abyss: low-rarity abyss reward remains visible and claimable.
5. Settings draft preview changes labels, Cancel restores the committed presentation and policy.
6. Confirmed automatic pickup displays the real HUD receipt notice.

## Required files

- Create `tests/platform/stage11d_loot_formal_game_validation.cpp`
- Create `tests/platform/stage11d_loot_formal_validator.ps1`
- Create `tests/platform/stage11d_loot_evidence_guard_test.cmake`
- Create `tests/platform/stage11d_loot_evidence_guard_self_test.cmake`
- Modify `src/platform/raylib/raylib_host.hpp`
- Modify `src/platform/raylib/raylib_host.cpp`
- Modify `tests/platform/CMakeLists.txt`
- Generate `docs/validation/evidence/stage11d/*.png` and semantic summaries/manifest required by the validator

## Hard constraints

- Add exactly six `Stage11DLootValidationScenario` values plus `none`.
- Drive scenarios through public host configuration/input and real persisted settings/save transactions.
- No public snapshot setter, direct view/model injection, private-member access, fake renderer, direct pickup-completion publication, pre-present screenshot, or fabricated summary values.
- Capture only after `EndDrawing()` through the single production capture helper.
- Formal validator must require fresh 1280x720 PNGs, unique hashes, meaningful non-empty label/HUD regions, exact production item IDs before/after filtering, hidden items absent from inventory, abyss reward present and claimable, preview rollback, and confirmed pickup notice text.
- Evidence guard and self-test must prove rejection of private injection, fake settings publication, direct pickup completion, snapshot mutation, pre-`EndDrawing()` capture, non-production renderer, stale PNGs, duplicate hashes, and existence-only validation.
- Preserve all unrelated behavior and old evidence. Do not overwrite historical `.superpowers/sdd/task-9-report.md` files.

## TDD and verification

1. Register formal tests first and record the intended RED caused by absent harness files/target.
2. Implement the minimum production seam and harness.
3. Run focused builds/tests, then all Stage 11-D formal/guard tests.
4. Run `git diff --check` and inspect status.
5. Write evidence and exact commands/counts to `.superpowers/sdd/stage11d-task-9-report.md`.

Do not commit. Return a concise summary of changes, tests, known risks, and report path.
