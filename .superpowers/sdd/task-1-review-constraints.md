# Task 1 Binding Constraints

- C++17, raylib 6.0 project, 60 Hz fixed logic step.
- Combat owns damage resolution and the fixed 300-tick history; it must not read Dungeon, Persistence, room, depth, or raylib state.
- The history stores five types in stable physical, fire, water, lightning, chaos order.
- A bucket is advanced and cleared by logical tick, not render frame; same-tick records accumulate.
- Resource-capped actual damage is a Task 2 integration concern. Task 1 resolves per-type post-defense damage and provides the standalone fixed history abstraction.
- All accumulation saturates; unsigned values never wrap.
- Construction, tick advance, record, and total queries perform no heap allocation.
- Existing total-only `resolve_player_damage()` behavior remains compatible.
- This task must not implement death sources, death snapshots, Dungeon state, persistence, UI, or other Stage 11 features.
- Tests must show an actual RED failure before production code and must exercise real code rather than test-only production hooks.
