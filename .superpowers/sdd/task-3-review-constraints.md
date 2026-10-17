# Task 3 Binding Constraints

- Stable death enums and `DeathCheckpoint` live in `dungeon_checkpoint.hpp`; Persistence must be able to use structural validation without linking `arpg_dungeon`.
- `valid_death_checkpoint_structural()` is header-only, `constexpr`, `noexcept`, catalog/rules independent, and rejects noncanonical `none` payloads plus malformed pending payload relationships.
- Canonical `none` uses lifecycle none, data version 0, semantic default enums, zero numeric/boolean fields, and target room index/seed/depth/floor all 0 despite `RoomDescriptor` normal defaults being 1.
- Pending death uses a nonzero data version, positive death depth, valid source/damage enums, actual final damage equal to barrier loss + health loss without overflow, valid defense ranges, and a structurally valid non-abyss initial target.
- `DungeonRunState` adds monotonic `death_sequence` plus `DeathCheckpoint death`; initial state is sequence 0/canonical none and `same_run_state()` compares every added field.
- `make_death_retreat_target(current,next_death_sequence,rules)` uses a dedicated death_retreat domain and inputs current room seed, current pre-commit generation, next sequence, and target depth.
- Target depth is max(1,current.depth-1), global index is current.index+1, floor index 0, entry initial, zero biases for ecology generation, `is_abyss=false`; ecology and ordinary hole use existing independent room-generation streams.
- A zero derived seed maps once to fixed nonzero `0xD34D5EEDULL`; it is not re-rolled.
- Max current room index, max current commit generation, max current death sequence, zero/incorrect next sequence, and invalid rules return explicit faults before output publication; values never wrap or clamp.
- Identical inputs produce identical full descriptors. Existing door, descent, abyss, ecology/hole and drop named streams/golden values must not change.
- Combat-to-stable conversion may live in Dungeon code, but Combat remains unaware of Dungeon. This task does not implement Session transactions, persistence V6, UI, or input gating.
- C++17; deterministic functions are noexcept and allocate nothing.
- Tests must demonstrate actual RED before implementation and must not mutate private Session state.
