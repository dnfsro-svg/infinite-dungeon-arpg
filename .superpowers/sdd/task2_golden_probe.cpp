#include "combat/combat_world.hpp"

#include <cstdio>

int main() {
    using namespace arpg::combat;
    CombatEncounterConfig config{};
    config.wave.spawn_count = 5U;
    config.wave.spawns[0] = {MonsterId::water_bulwark, {3.0F, 0.0F, 0.0F}};
    config.wave.spawns[1] = {MonsterId::chaos_chaser, {1.5F, 1.0F, 0.0F}};
    config.wave.spawns[2] = {MonsterId::lightning_shooter, {4.5F, 0.0F, 0.0F}};
    config.wave.spawns[3] = {MonsterId::water_support, {4.0F, 2.0F, 0.0F}};
    config.wave.spawns[4] = {MonsterId::chaos_hazard, {4.0F, -2.0F, 0.0F}};
    CombatWorld world{config};
    for (std::uint32_t tick = 0; tick < 240U; ++tick) {
        if (tick % 90U == 0U) {
            std::printf("queue %u %d\n", tick,
                world.queue_action(Action::light) ? 1 : 0);
        }
        world.tick({static_cast<std::int8_t>(tick % 120U < 60U ? 1 : -1), 0});
        while (const auto event = world.try_pop_event()) {
            std::printf("event %u %u %u %d\n", tick,
                static_cast<unsigned>(event->kind), event->target_index,
                event->value);
        }
        if (tick == 0U || tick == 30U || tick == 45U || tick == 60U
            || tick == 90U || tick == 120U || tick == 180U || tick == 239U) {
            const CombatSnapshot snapshot = world.snapshot();
            std::printf("snap %u hp=%d proj=%zu haz=%zu effects=%zu/%zu diag=%u,%u,%u\n",
                tick, snapshot.player.hp, snapshot.projectile_count,
                snapshot.hazard_count, snapshot.diagnostics.effect_owner_count,
                snapshot.diagnostics.active_effect_count,
                snapshot.diagnostics.effect_overflow_count,
                snapshot.diagnostics.effect_command_overflow_count,
                snapshot.diagnostics.event_overflow_count);
            for (std::size_t slot = 0; slot < 5U; ++slot) {
                const MonsterSnapshot& monster = snapshot.monsters[slot];
                std::printf("monster %zu active=%d id=%u hp=%d phase=%u ticks=%u shield=%d shieldticks=%u x=%.6f y=%.6f\n",
                    slot, monster.active ? 1 : 0, static_cast<unsigned>(monster.id),
                    monster.hp, static_cast<unsigned>(monster.ai_phase),
                    monster.hit_stop_ticks, monster.shield, monster.shield_ticks,
                    monster.position.x, monster.position.y);
            }
        }
    }
}
