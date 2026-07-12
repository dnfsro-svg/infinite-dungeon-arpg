#include "test_framework.hpp"

#include "combat/combat_world.hpp"
#include "combat/monster_pool.hpp"

#include <array>
#include <cstddef>
#include <type_traits>
#include <utility>

namespace {

using namespace arpg::combat;

static_assert(std::is_same_v<
    decltype(std::declval<MonsterPool&>().slots()),
    const std::array<MonsterRuntime, kMonsterCapacity>&>);

arpg::test::Failure pool_allocates_first_free_slot_deterministically() noexcept {
    MonsterPool pool;
    const auto first = pool.spawn(MonsterId::fire_bomber, Vec3{1.0F, 2.0F, 0.0F});
    const auto second = pool.spawn(MonsterId::water_bulwark, Vec3{3.0F, 4.0F, 0.0F});
    ARPG_REQUIRE(first.has_value());
    ARPG_REQUIRE(second.has_value());
    ARPG_REQUIRE(first->index == 0U);
    ARPG_REQUIRE(second->index == 1U);
    ARPG_REQUIRE(pool.active_count() == 2U);
    ARPG_REQUIRE(pool.get(*first) != nullptr);
    ARPG_REQUIRE(pool.get(*second)->id == MonsterId::water_bulwark);
    ARPG_REQUIRE(pool.get(*first)->position.x == 1.0F);
    return {};
}

arpg::test::Failure pool_rejects_stale_handle_after_destroy_and_reuse() noexcept {
    MonsterPool pool;
    const auto original = pool.spawn(MonsterId::fire_charger, Vec3{});
    ARPG_REQUIRE(original.has_value());
    ARPG_REQUIRE(pool.destroy(*original));
    ARPG_REQUIRE(pool.active_count() == 0U);
    ARPG_REQUIRE(pool.get(*original) == nullptr);

    const auto replacement = pool.spawn(MonsterId::chaos_chaser, Vec3{5.0F, 0.0F, 0.0F});
    ARPG_REQUIRE(replacement.has_value());
    ARPG_REQUIRE(replacement->index == original->index);
    ARPG_REQUIRE(replacement->generation != original->generation);
    ARPG_REQUIRE(pool.get(*original) == nullptr);
    ARPG_REQUIRE(pool.get(*replacement) != nullptr);
    ARPG_REQUIRE(!pool.destroy(*original));
    ARPG_REQUIRE(pool.active_count() == 1U);
    return {};
}

arpg::test::Failure pool_rejects_97th_spawn_without_mutating_full_slots() noexcept {
    MonsterPool pool;
    std::array<MonsterHandle, kMonsterCapacity> handles{};
    for (std::size_t index = 0; index < kMonsterCapacity; ++index) {
        const auto handle = pool.spawn(
            MonsterId::lightning_shooter,
            Vec3{static_cast<float>(index), 0.0F, 0.0F});
        ARPG_REQUIRE(handle.has_value());
        ARPG_REQUIRE(handle->index == index);
        handles[index] = *handle;
    }
    const auto before = pool.slots();
    ARPG_REQUIRE(!pool.spawn(MonsterId::chaos_hazard, Vec3{99.0F, 0.0F, 0.0F}));
    ARPG_REQUIRE(pool.active_count() == kMonsterCapacity);
    for (std::size_t index = 0; index < kMonsterCapacity; ++index) {
        const MonsterRuntime* runtime = pool.get(handles[index]);
        ARPG_REQUIRE(runtime != nullptr);
        ARPG_REQUIRE(runtime->active == before[index].active);
        ARPG_REQUIRE(runtime->generation == before[index].generation);
        ARPG_REQUIRE(runtime->id == before[index].id);
        ARPG_REQUIRE(runtime->kind == before[index].kind);
        ARPG_REQUIRE(runtime->position.x == before[index].position.x);
        ARPG_REQUIRE(runtime->position.y == before[index].position.y);
        ARPG_REQUIRE(runtime->position.z == before[index].position.z);
        ARPG_REQUIRE(runtime->velocity.x == before[index].velocity.x);
        ARPG_REQUIRE(runtime->velocity.y == before[index].velocity.y);
        ARPG_REQUIRE(runtime->velocity.z == before[index].velocity.z);
        ARPG_REQUIRE(runtime->reaction == before[index].reaction);
        ARPG_REQUIRE(runtime->armor == before[index].armor);
        ARPG_REQUIRE(runtime->ai_phase == before[index].ai_phase);
        ARPG_REQUIRE(runtime->ai_ticks == before[index].ai_ticks);
        ARPG_REQUIRE(runtime->hp == before[index].hp);
        ARPG_REQUIRE(runtime->max_hp == before[index].max_hp);
        ARPG_REQUIRE(runtime->break_value == before[index].break_value);
        ARPG_REQUIRE(runtime->max_break == before[index].max_break);
        ARPG_REQUIRE(runtime->break_window_ticks == before[index].break_window_ticks);
        ARPG_REQUIRE(runtime->hit_stop_ticks == before[index].hit_stop_ticks);
    }
    return {};
}

arpg::test::Failure pool_clear_deactivates_all_slots_and_reuses_slot_zero() noexcept {
    MonsterPool pool;
    const auto old = pool.spawn(MonsterId::fire_bomber, Vec3{});
    ARPG_REQUIRE(old.has_value());
    pool.clear();
    ARPG_REQUIRE(pool.active_count() == 0U);
    ARPG_REQUIRE(pool.get(*old) == nullptr);
    const auto fresh = pool.spawn(MonsterId::water_support, Vec3{});
    ARPG_REQUIRE(fresh.has_value());
    ARPG_REQUIRE(fresh->index == 0U);
    ARPG_REQUIRE(fresh->generation != old->generation);
    return {};
}

arpg::test::Failure encounter_wave_load_populates_snapshot_active_count() noexcept {
    EncounterWave wave{};
    wave.spawn_count = 2U;
    wave.spawns[0] = MonsterSpawnSpec{MonsterId::fire_bomber, Vec3{2.0F, 0.0F, 0.0F}};
    wave.spawns[1] = MonsterSpawnSpec{MonsterId::chaos_hazard, Vec3{4.0F, 1.0F, 0.0F}};
    CombatEncounterConfig config{};
    config.wave = wave;
    CombatWorld world{config};
    const CombatSnapshot snapshot = world.snapshot();
    ARPG_REQUIRE(snapshot.monster_count == 2U);
    ARPG_REQUIRE(world.active_monster_count() == 2U);
    ARPG_REQUIRE(snapshot.monsters[0].active);
    ARPG_REQUIRE(snapshot.monsters[0].id == MonsterId::fire_bomber);
    ARPG_REQUIRE(snapshot.monsters[1].id == MonsterId::chaos_hazard);
    ARPG_REQUIRE(snapshot.monsters[1].position.x == 4.0F);
    return {};
}

arpg::test::Failure compatibility_projection_packs_active_slots_after_destroy() noexcept {
    EncounterWave wave{};
    wave.spawn_count = 3U;
    wave.spawns[0] = MonsterSpawnSpec{MonsterId::fire_bomber, Vec3{2.0F, 0.0F, 0.0F}};
    wave.spawns[1] = MonsterSpawnSpec{MonsterId::water_support, Vec3{3.0F, 0.0F, 0.0F}};
    wave.spawns[2] = MonsterSpawnSpec{MonsterId::chaos_hazard, Vec3{4.0F, 0.0F, 0.0F}};
    CombatEncounterConfig config{};
    config.wave = wave;
    CombatWorld world{config};
    CombatSnapshot before = world.snapshot();
    ARPG_REQUIRE(world.destroy_monster(MonsterHandle{
        0U, before.monsters[0].generation}));
    CombatSnapshot after = world.snapshot();
    ARPG_REQUIRE(after.monster_count == 2U);
    ARPG_REQUIRE(after.dummies[0].active);
    ARPG_REQUIRE(after.dummies[0].id == MonsterId::water_support);
    ARPG_REQUIRE(after.dummies[1].active);
    ARPG_REQUIRE(after.dummies[1].id == MonsterId::chaos_hazard);
    ARPG_REQUIRE(!after.dummies[2].active);
    return {};
}

arpg::test::Failure load_wave_clears_attack_input_and_old_events() noexcept {
    CombatLabConfig legacy{};
    legacy.dummy_spawns = {{{2.13F, 0.0F, 0.0F},
                            {6.0F, 3.0F, 0.0F},
                            {7.0F, -3.0F, 0.0F}}};
    CombatWorld world{legacy};
    ARPG_REQUIRE(world.queue_action(Action::light));
    for (int tick = 0; tick < 10; ++tick) {
        world.tick(MovementInput{});
    }

    EncounterWave wave{};
    wave.spawn_count = 1U;
    wave.spawns[0] = MonsterSpawnSpec{
        MonsterId::chaos_chaser, Vec3{2.13F, 0.0F, 0.0F}};
    ARPG_REQUIRE(world.load_wave(wave));
    ARPG_REQUIRE(world.snapshot().player.active_attack == AttackId::none);
    ARPG_REQUIRE(world.snapshot().diagnostics.input_size == 0U);
    ARPG_REQUIRE(!world.try_pop_event().has_value());

    ARPG_REQUIRE(world.queue_action(Action::light));
    for (int tick = 0; tick < 20; ++tick) {
        world.tick(MovementInput{});
    }
    ARPG_REQUIRE(world.snapshot().monsters[0].hp <
        world.snapshot().monsters[0].max_hp);
    return {};
}

arpg::test::Failure wave_with_four_monsters_resolves_all_active_slots() noexcept {
    EncounterWave wave{};
    wave.spawn_count = 4U;
    for (std::size_t index = 0; index < wave.spawn_count; ++index) {
        wave.spawns[index] = MonsterSpawnSpec{
            MonsterId::chaos_chaser, Vec3{2.13F, 0.0F, 0.0F}};
    }
    CombatEncounterConfig config{};
    config.wave = wave;
    CombatWorld world{config};
    ARPG_REQUIRE(world.queue_action(Action::light));
    for (int tick = 0; tick < 20; ++tick) {
        world.tick(MovementInput{});
    }
    const CombatSnapshot snapshot = world.snapshot();
    ARPG_REQUIRE(snapshot.monster_count == 4U);
    for (std::size_t index = 0; index < 4U; ++index) {
        ARPG_REQUIRE(snapshot.monsters[index].hp <
            snapshot.monsters[index].max_hp);
    }
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"first-free deterministic allocation", &pool_allocates_first_free_slot_deterministically},
    {"stale handle rejection", &pool_rejects_stale_handle_after_destroy_and_reuse},
    {"full capacity rejection", &pool_rejects_97th_spawn_without_mutating_full_slots},
    {"clear reset and reuse", &pool_clear_deactivates_all_slots_and_reuses_slot_zero},
    {"wave snapshot active count", &encounter_wave_load_populates_snapshot_active_count},
    {"active-slot projection after destroy", &compatibility_projection_packs_active_slots_after_destroy},
    {"wave load clears transient combat state", &load_wave_clears_attack_input_and_old_events},
    {"four-monster hit resolution", &wave_with_four_monsters_resolves_all_active_slots},
};

}  // namespace

arpg::test::TestSuite monster_pool_suite() noexcept {
    return arpg::test::make_suite("monster_pool", kCases);
}
