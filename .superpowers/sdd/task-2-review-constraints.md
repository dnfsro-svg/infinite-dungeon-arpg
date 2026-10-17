# Task 2 Binding Constraints

- C++17, 60 Hz fixed logic step, no Dungeon/Persistence/raylib dependency in Combat.
- The fixed history has exactly 300 logical-tick buckets in physical, fire, water, lightning, chaos order.
- `resolve_player_damage_packet()` already returns post-defense values whose sum preserves the legacy total damage rounding semantics.
- CombatWorld must resource-cap actual damage against nonnegative current barrier + HP before recording history. Overkill is excluded.
- Actual per-type values must be deterministic, sum exactly to barrier loss + HP loss, and use physical→fire→water→lightning→chaos as the stable remainder order.
- Evaded, invulnerable, invalid, and zero-actual hits do not enter the history.
- The first hit that lowers HP to zero freezes exactly one `CombatDeathSnapshot`; later same-tick or later-tick damage cannot overwrite it.
- A frozen death stops player, monsters, projectiles, hazards, status timers, effects, action/input processing, and damage-history advancement until CombatWorld destruction/replacement.
- Stable source kinds are monster attack, projectile, ground hazard, monster affix, abyss environment, unknown. Unreliable attribution uses unknown rather than a default monster.
- The death snapshot contains raw positive input total, actual per-type primary type, barrier/HP loss, actual final total, the 300-tick history including the lethal hit, and the post-hit defense snapshot.
- Combat update, damage recording, lethal snapshot creation, and frozen ticks perform zero heap allocations.
- Player death wins over same-tick room-clear interpretation; Combat exposes the frozen snapshot read-only and emits `player_defeated` only once.
- This task must not implement Dungeon, stable checkpoint, persistence, raylib UI, or input gating.
- Tests must demonstrate RED before production code and use production damage paths without private test-only mutation hooks.
