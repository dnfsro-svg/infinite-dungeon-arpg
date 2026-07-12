#include "test_framework.hpp"

#include "combat/combat_world.hpp"
#include "combat/monster_pool.hpp"

#include <array>
#include <cstddef>

namespace {

using namespace arpg::combat;

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
        ARPG_REQUIRE(runtime->active);
        ARPG_REQUIRE(runtime->id == before[index].id);
        ARPG_REQUIRE(runtime->position.x == before[index].position.x);
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

constexpr arpg::test::TestCase kCases[] = {
    {"first-free deterministic allocation", &pool_allocates_first_free_slot_deterministically},
    {"stale handle rejection", &pool_rejects_stale_handle_after_destroy_and_reuse},
    {"full capacity rejection", &pool_rejects_97th_spawn_without_mutating_full_slots},
    {"clear reset and reuse", &pool_clear_deactivates_all_slots_and_reuses_slot_zero},
    {"wave snapshot active count", &encounter_wave_load_populates_snapshot_active_count},
};

}  // namespace

arpg::test::TestSuite monster_pool_suite() noexcept {
    return arpg::test::make_suite("monster_pool", kCases);
}
