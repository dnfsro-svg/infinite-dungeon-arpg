# Task 1A: Neutral checkpoint DTOs

Worktree `E:/game/.worktrees/whole-game-refactor-2026-07`, base `d6854ac`.
Read the complete V9 design and Task 1 plan before editing.

Allowed files only:

- Create `src/checkpoint/dungeon_run_state.hpp`
- Create `src/checkpoint/room_checkpoint_schema.hpp`
- Create `src/checkpoint/room_combat_checkpoint.hpp`
- Create `src/checkpoint/room_progress_checkpoint.hpp`

Do not edit existing Combat/Dungeon/Persistence files, CMake, tests, or plans.
Do not commit.

Mirror every V9-serialized declaration currently in
`dungeon/dungeon_checkpoint.hpp`, `combat/room_combat_checkpoint.hpp` and their
included Combat value DTOs into namespace `arpg::checkpoint`. Do not include a
Combat or Dungeon header. Use a schema-owned `CheckpointVec3`; define explicit
fixed-width checkpoint enums and aggregates for every serialized Combat value.
Facing is `std::int8_t` with exact `-1/1` values. Preserve field order, scalar
widths, capacities, nonzero defaults/sentinels, and the noncopyable/nonmovable
policy for RoomCombatCheckpoint, RoomProgressCheckpoint, SaveCheckpointSlot.

The schema header owns the exact capacities and pure mappings specified by the
plan, including required kills, material/potion ordinals, 400 environment
cells, 2 fire crates, room bounds, attack active-tick and burning/blink timing
tables needed by neutral validation. Use only lower headers from core, abyss,
skills, modifiers, items, progression, passives.

The existing production declarations remain untouched in this task. Run a
static forbidden-include search over the four new files and report an exact
symbol inventory. Do not build because the Task 1 files are integrated in
parallel.
