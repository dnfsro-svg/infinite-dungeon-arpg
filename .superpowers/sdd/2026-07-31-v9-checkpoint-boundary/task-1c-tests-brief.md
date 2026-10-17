# Task 1C: Neutral schema unit tests

Worktree `E:/game/.worktrees/whole-game-refactor-2026-07`, base `d6854ac`.
Read the complete V9 design and Task 1 plan before editing.

Allowed files only:

- Create `tests/checkpoint/checkpoint_schema_tests.cpp`
- Create `tests/checkpoint/checkpoint_test_main.cpp`

Do not edit production code, CMake, existing tests, or plans. Do not commit or
build; DTO/target files are being created in parallel.

Use the existing tiny test framework conventions. Heap-allocate
`arpg::checkpoint::SaveCheckpointSlot` using `unique_ptr + nothrow`, check before
use, reserve item storage, record its data/capacity, mutate representative
fields, and call all three clear functions. Assert canonical default state,
including commit_generation/depth/floor/default skill loadout, right/none/count/
0xFFFF sentinels, air_attack_available=true, canonical unused counts/tails, and
retained item vector backing/capacity.

Cover noncopyable/nonmovable traits, exact capacities, required_kills edge
values, ordinary/abyss material mappings and inverse behavior, potion ordinals,
canonical equality ignoring inactive array tails, and representative structural
rejections for invalid enum/count/RNG/room identity. Do not weaken the tests to
match an implementation. Provide suite function `checkpoint_schema_suite()`
and a standalone main following current repository style.

Return the test case list and expected initial compile RED. Do not build.
