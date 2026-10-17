# Task 2 Brief — Protect depths 1–3 from monster affixes

## Scope

Implement only Task 2 from:
`E:\game\.worktrees\balance-hp-potions\docs\superpowers\plans\2026-07-23-extreme-monster-nerf-health-potions.md`

Worktree: `E:\game\.worktrees\balance-hp-potions`

Allowed production/test files:

- `src/combat/monster_affix_generation.cpp`
- `tests/combat/monster_affix_generation_tests.cpp`
- `tests/combat/combat_test_main.cpp`

Report file (ignored artifact):
`E:\game\.worktrees\balance-hp-potions\.superpowers\sdd\task-2-report.md`

## Required behavior

- Depths 1, 2, and 3 must always return a valid, empty `MonsterAffixSet` for both ordinary generation and abyss supplementation.
- `supplement_abyss_affixes(...)` must clear even a valid non-empty input set at depths 1–3.
- Preserve all catalog, monster, and input-set validation; the early empty return belongs after those validations and before `minimum_abyss_affixes()` is read.
- Depth 4 and above retain all existing random behavior and weights.
- Change the depth-band entry ending at depth 3 to normal-count weights `{100,0,0,0}` and tier weights `{100,0,0}`.
- Update the abyss minimum-affix test first case from depth 1 to depth 4.
- Add exactly one focused case that covers every depth 1–3, every monster, seeds 0–255, normal generation, abyss supplementation, and clearing a pre-populated valid set.
- Register the new case; expected combat case count changes from 242 to 243 (or from the current Task-1 total to one more).

## Required test body

Use the plan's `first_three_depths_are_affix_free_in_normal_and_abyss_rooms()` test shape exactly unless a compile-level adjustment is required:

```cpp
arpg::test::Failure first_three_depths_are_affix_free_in_normal_and_abyss_rooms() noexcept {
    MonsterAffixSet nonempty{};
    nonempty.values[0] = {MonsterAffixId::mighty, MonsterAffixTier::m1};
    nonempty.count = 1U;
    for (std::uint64_t depth = 1U; depth <= 3U; ++depth) {
        for (std::uint8_t raw = 0U;
             raw < static_cast<std::uint8_t>(MonsterId::count); ++raw) {
            const MonsterDefinition* monster = monster_definition(static_cast<MonsterId>(raw));
            ARPG_REQUIRE(monster != nullptr);
            for (std::uint64_t seed = 0U; seed < 256U; ++seed) {
                const auto normal = generate_monster_affixes(seed, depth, 1U, raw, *monster);
                ARPG_REQUIRE(normal.has_value());
                ARPG_REQUIRE(normal->count == 0U);
                const auto abyss = supplement_abyss_affixes(seed, depth, 1U, raw,
                    *monster, *normal);
                ARPG_REQUIRE(abyss.has_value());
                ARPG_REQUIRE(abyss->count == 0U);
            }
            const auto cleared = supplement_abyss_affixes(7U, depth, 0U, raw,
                *monster, nonempty);
            ARPG_REQUIRE(cleared.has_value());
            ARPG_REQUIRE(cleared->count == 0U);
        }
    }
    return {};
}
```

Required depth table first entry:

```cpp
{3U, {{100U, 0U, 0U, 0U}}, {{100U, 0U, 0U}}},
```

Required supplement guard after validation:

```cpp
if (depth <= 3U) return MonsterAffixSet{};
```

## TDD and verification

Read and follow:
`E:\codex1\.codex\plugins\cache\openai-curated-remote\superpowers\6.1.1\skills\test-driven-development\SKILL.md`

1. Write/update tests first.
2. Run the focused build/test and capture the expected RED caused by depths 1–3 still receiving affixes.
3. Make the minimal production change.
4. Run:

```powershell
cmake --build --preset windows-msvc-core-debug --target arpg_combat_tests -- -j1
ctest --preset windows-msvc-core-debug -R '^combat\.units$' --output-on-failure
```

Expected final result: `243 cases, 0 failures`.

## Hard boundaries

- Do not change monster base stats from Task 1.
- Do not implement potions, loot pools, persistence, UI, assets, or HUD.
- Do not change player damage/defense, monster counts/speeds, room size/density, or any gameplay rule outside the depth-1–3 affix guard.
- Do not touch the dirty main worktree at `E:\game`.
- Do not amend or squash earlier commits.

## Completion

- Commit only the scoped files with message:
  `balance: suppress monster affixes through depth three`
- Write the report file with: status, commit hash, RED evidence, GREEN evidence, files changed, and any concerns.
- Return `DONE`, `DONE_WITH_CONCERNS`, `NEEDS_CONTEXT`, or `BLOCKED`.
