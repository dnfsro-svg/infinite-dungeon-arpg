#include "test_framework.hpp"

#include "allocation_probe.hpp"
#include "../combat/monster_affix_test_support.hpp"
#include "combat/monster_catalog.hpp"
#include "combat/monster_affix_catalog.hpp"
#include "combat/monster_affix_generation.hpp"
#include "combat/room_bounds.hpp"
#include "dungeon/dungeon_checkpoint.hpp"
#include "dungeon/encounter_director.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace {

using arpg::combat::EncounterWave;
using arpg::combat::MonsterAffixCatalog;
using arpg::combat::MonsterAffixId;
using arpg::combat::MonsterAffixTier;
using arpg::combat::MonsterId;
using arpg::combat::MonsterTag;
using arpg::combat::kEncounterWaveCapacity;
using arpg::dungeon::DungeonFault;
using arpg::dungeon::EncounterDirectorConfig;
using arpg::dungeon::RoomEncounterPlan;
using arpg::dungeon::build_encounter_plan;
using arpg::dungeon::build_abyss_encounter_plan;
using arpg::dungeon::encounter_budget;
using arpg::dungeon::encounter_plan_legal;
using arpg::dungeon::validate_encounter_director_config;
namespace checkpoint = arpg::dungeon::checkpoint;
using checkpoint::DungeonElement;

constexpr bool test_has_tag(
    const arpg::combat::MonsterDefinition& definition,
    MonsterTag value) noexcept {
    return (definition.tags & static_cast<std::uint16_t>(value)) != 0U;
}

bool same_encounter_plan(
    const RoomEncounterPlan& lhs,
    const RoomEncounterPlan& rhs) noexcept {
    if (lhs.wave_count != rhs.wave_count
            || lhs.total_budget != rhs.total_budget) {
        return false;
    }
    for (std::size_t wave_index = 0; wave_index < lhs.wave_count;
         ++wave_index) {
        const EncounterWave& left = lhs.waves[wave_index];
        const EncounterWave& right = rhs.waves[wave_index];
        if (left.spawn_count != right.spawn_count
                || left.spent_budget != right.spent_budget) {
            return false;
        }
        for (std::size_t spawn_index = 0; spawn_index < left.spawn_count;
             ++spawn_index) {
            const auto& a = left.spawns[spawn_index];
            const auto& b = right.spawns[spawn_index];
            if (a.id != b.id || a.position.x != b.position.x
                    || a.position.y != b.position.y
                    || a.position.z != b.position.z
                    || !(a.affixes == b.affixes)
                    || a.spawn_ordinal != b.spawn_ordinal) {
                return false;
            }
        }
    }
    return true;
}

bool test_encounter_plan_legal(
    const RoomEncounterPlan& plan,
    const EncounterDirectorConfig& config) noexcept {
    if (plan.wave_count == 0U || plan.wave_count > kEncounterWaveCapacity
            || plan.total_budget == 0U
            || plan.total_budget > config.max_budget) {
        return false;
    }
    const std::uint8_t expected_wave_count =
        plan.total_budget > config.two_wave_threshold ? 2U : 1U;
    if (plan.wave_count != expected_wave_count) {
        return false;
    }
    std::uint16_t total_spent = 0U;
    for (std::size_t wave_index = 0; wave_index < plan.wave_count;
         ++wave_index) {
        const EncounterWave& wave = plan.waves[wave_index];
        if (wave.spawn_count == 0U || wave.spawn_count
                > arpg::combat::kEncounterSpawnCapacity) {
            return false;
        }
        std::uint16_t spent = 0U;
        std::uint8_t direct_count = 0U;
        std::uint8_t high_priority_count = 0U;
        std::uint8_t ranged_count = 0U;
        std::uint8_t support_count = 0U;
        std::uint8_t hazard_count = 0U;
        for (std::size_t spawn_index = 0; spawn_index < wave.spawn_count;
             ++spawn_index) {
            const auto& spawn = wave.spawns[spawn_index];
            const auto* definition = arpg::combat::monster_definition(spawn.id);
            if (definition == nullptr) {
                return false;
            }
            spent = static_cast<std::uint16_t>(spent + definition->threat_cost);
            direct_count += test_has_tag(*definition, MonsterTag::direct_target);
            high_priority_count += test_has_tag(*definition, MonsterTag::high_priority);
            ranged_count += test_has_tag(*definition, MonsterTag::ranged);
            support_count += test_has_tag(*definition, MonsterTag::support);
            hazard_count += test_has_tag(*definition, MonsterTag::ground_hazard);
            if (spawn.position.x < arpg::combat::room_bounds::min_x
                    || spawn.position.x > arpg::combat::room_bounds::max_x
                    || spawn.position.y < arpg::combat::room_bounds::min_y
                    || spawn.position.y > arpg::combat::room_bounds::max_y
                    || spawn.position.z != 0.0F) {
                return false;
            }
        }
        if (direct_count == 0U || support_count > config.support_limit
                || ranged_count > config.ranged_limit
                || hazard_count > config.ground_hazard_limit) {
            return false;
        }
        const std::uint8_t priority_limit =
            plan.total_budget > config.two_wave_threshold
                ? config.high_budget_priority_limit
                : config.normal_high_priority_limit;
        if (high_priority_count > priority_limit
                || spent != wave.spent_budget) {
            return false;
        }
        total_spent = static_cast<std::uint16_t>(total_spent + spent);
    }
    return total_spent <= plan.total_budget;
}

arpg::test::Failure director_budget_is_bounded_and_depth_driven() noexcept {
    EncounterDirectorConfig config{};
    ARPG_REQUIRE(encounter_budget(1U, config) == 8U);
    ARPG_REQUIRE(encounter_budget(5U, config) == 8U);
    ARPG_REQUIRE(encounter_budget(6U, config) == 9U);
    ARPG_REQUIRE(encounter_budget(10000U, config) == 24U);
    return {};
}

arpg::test::Failure abyss_budget_is_ceil_three_halves_and_scaled_max_is_legal() noexcept {
    const EncounterDirectorConfig config{};
    const auto budget_8 = build_abyss_encounter_plan(1U, 1U,
        DungeonElement::chaos, config);
    const auto budget_9 = build_abyss_encounter_plan(1U, 6U,
        DungeonElement::chaos, config);
    const auto budget_24 = build_abyss_encounter_plan(1U, 10000U,
        DungeonElement::chaos, config);
    ARPG_REQUIRE(budget_8.fault == DungeonFault::none);
    ARPG_REQUIRE(budget_9.fault == DungeonFault::none);
    ARPG_REQUIRE(budget_24.fault == DungeonFault::none);
    ARPG_REQUIRE(budget_8.plan.total_budget == 12U);
    ARPG_REQUIRE(budget_9.plan.total_budget == 14U);
    ARPG_REQUIRE(budget_24.plan.total_budget == 36U);
    return {};
}

arpg::test::Failure abyss_plan_keeps_normal_affix_prefix_and_is_allocation_free() noexcept {
    const std::uint64_t before = arpg::test::allocation_count();
    for (std::uint64_t seed = 0U; seed < 1024U; ++seed) {
        const auto result = build_abyss_encounter_plan(seed, 40U,
            DungeonElement::lightning, EncounterDirectorConfig{});
        ARPG_REQUIRE(result.fault == DungeonFault::none);
        ARPG_REQUIRE(result.plan.total_budget == 23U);
        for (std::size_t wave_index = 0U;
             wave_index < result.plan.wave_count; ++wave_index) {
            const auto& wave = result.plan.waves[wave_index];
            ARPG_REQUIRE(wave.spawn_count <= wave.spawns.size());
            for (std::size_t spawn_index = 0U;
                 spawn_index < wave.spawn_count; ++spawn_index) {
                const auto& spawn = wave.spawns[spawn_index];
                const auto* monster = arpg::combat::monster_definition(spawn.id);
                ARPG_REQUIRE(monster != nullptr);
                const auto normal = arpg::combat::generate_monster_affixes(
                    seed, 40U, static_cast<std::uint8_t>(wave_index),
                    static_cast<std::uint8_t>(spawn_index), *monster);
                ARPG_REQUIRE(normal.has_value());
                ARPG_REQUIRE(spawn.affixes.count == 3U);
                for (std::size_t index = 0U; index < normal->count; ++index) {
                    ARPG_REQUIRE(spawn.affixes.values[index]
                        == normal->values[index]);
                }
            }
        }
    }
    ARPG_REQUIRE(arpg::test::allocation_count() == before);
    return {};
}

arpg::test::Failure abyss_budget_capacity_boundary_is_legal_and_overflow_fails() noexcept {
    EncounterDirectorConfig boundary{};
    boundary.base_budget = 170U;
    boundary.max_budget = 170U;
    boundary.two_wave_threshold = 255U;
    boundary.matching_ecology_weight = 255U;
    boundary.normal_high_priority_limit = 0U;
    boundary.high_budget_priority_limit = 0U;
    boundary.ranged_limit = 0U;
    boundary.support_limit = 0U;
    boundary.ground_hazard_limit = 0U;
    const auto legal = build_abyss_encounter_plan(0x96ULL, 1U,
        DungeonElement::chaos, boundary);
    ARPG_REQUIRE(legal.fault == DungeonFault::none);
    ARPG_REQUIRE(legal.plan.total_budget == 255U);
    ARPG_REQUIRE(legal.plan.wave_count == 1U);
    ARPG_REQUIRE(legal.plan.waves[0].spawn_count
        == arpg::combat::kEncounterSpawnCapacity);

    EncounterDirectorConfig overflow = boundary;
    overflow.base_budget = 171U;
    overflow.max_budget = 171U;
    overflow.two_wave_threshold = 255U;
    const auto rejected = build_abyss_encounter_plan(0x96ULL, 1U,
        DungeonElement::chaos, overflow);
    ARPG_REQUIRE(rejected.fault == DungeonFault::invalid_rules);
    ARPG_REQUIRE(rejected.plan.wave_count == 0U);
    return {};
}

arpg::test::Failure encounter_plan_is_deterministic_and_legal() noexcept {
    const auto a = build_encounter_plan(0xA11CEULL, 26U,
        DungeonElement::lightning, EncounterDirectorConfig{});
    const auto b = build_encounter_plan(0xA11CEULL, 26U,
        DungeonElement::lightning, EncounterDirectorConfig{});
    ARPG_REQUIRE(a.fault == DungeonFault::none);
    ARPG_REQUIRE(same_encounter_plan(a.plan, b.plan));
    ARPG_REQUIRE(test_encounter_plan_legal(a.plan, EncounterDirectorConfig{}));
    ARPG_REQUIRE(a.plan.wave_count == 2U);
    struct ExpectedBaseSpawn final {
        MonsterId id{};
        float x{};
        float y{};
    };
    constexpr std::array<ExpectedBaseSpawn, 4> kStage8BaseTrace{{
        {MonsterId::chaos_chaser, -1.898F, -3.159F},
        {MonsterId::fire_charger, 5.914F, 3.418F},
        {MonsterId::chaos_chaser, -1.935F, -3.361F},
        {MonsterId::lightning_dasher, -2.939F, 0.850F},
    }};
    for (std::size_t wave_index = 0U; wave_index < a.plan.wave_count;
         ++wave_index) {
        const auto& wave = a.plan.waves[wave_index];
        ARPG_REQUIRE(wave.spawn_count == 2U);
        for (std::size_t spawn_index = 0U; spawn_index < wave.spawn_count;
             ++spawn_index) {
            const auto& actual = wave.spawns[spawn_index];
            const auto& expected = kStage8BaseTrace[wave_index * 2U + spawn_index];
            ARPG_REQUIRE(actual.id == expected.id);
            const float legacy_x = actual.position.x * 12.0F
                / arpg::combat::room_bounds::half_extent;
            const float legacy_y = actual.position.y * 5.5F
                / arpg::combat::room_bounds::half_extent;
            ARPG_REQUIRE(arpg::test::near(legacy_x, expected.x, 0.002));
            ARPG_REQUIRE(arpg::test::near(legacy_y, expected.y, 0.002));
            ARPG_REQUIRE(actual.position.z == 0.0F);
        }
    }
    return {};
}

arpg::test::Failure generated_spawns_have_stable_ordinals_and_legal_affixes() noexcept {
    const auto result = build_encounter_plan(0xD19E5EEDULL, 40U,
        DungeonElement::lightning, EncounterDirectorConfig{});
    ARPG_REQUIRE(result.fault == DungeonFault::none);
    for (std::size_t wave_index = 0U; wave_index < result.plan.wave_count;
         ++wave_index) {
        const auto& wave = result.plan.waves[wave_index];
        for (std::size_t spawn_index = 0U; spawn_index < wave.spawn_count;
             ++spawn_index) {
            const auto& spawn = wave.spawns[spawn_index];
            ARPG_REQUIRE(spawn.spawn_ordinal == static_cast<std::uint16_t>(
                wave_index * arpg::combat::kEncounterSpawnCapacity + spawn_index));
            ARPG_REQUIRE(spawn.affixes.count <= spawn.affixes.values.size());
            const auto* definition = arpg::combat::monster_definition(spawn.id);
            ARPG_REQUIRE(definition != nullptr);
            for (std::size_t affix_index = 0U;
                 affix_index < spawn.affixes.count; ++affix_index) {
                const auto* affix = arpg::combat::monster_affix_definition(
                    spawn.affixes.values[affix_index].id);
                ARPG_REQUIRE(affix != nullptr);
                ARPG_REQUIRE((definition->tags & affix->required_tags)
                    == affix->required_tags);
                ARPG_REQUIRE((definition->tags & affix->forbidden_tags) == 0U);
            }
        }
    }
    return {};
}

arpg::test::Failure ecology_weighting_prefers_matching_elements() noexcept {
    std::uint32_t low_weight_matching = 0U;
    std::uint32_t high_weight_matching = 0U;
    EncounterDirectorConfig low_weight{};
    low_weight.matching_ecology_weight = 1U;
    for (std::uint64_t seed = 0U; seed < 4096U; ++seed) {
        const auto low = build_encounter_plan(seed, 1U,
            DungeonElement::fire, low_weight);
        const auto high = build_encounter_plan(seed, 1U,
            DungeonElement::fire, EncounterDirectorConfig{});
        ARPG_REQUIRE(low.fault == DungeonFault::none);
        ARPG_REQUIRE(high.fault == DungeonFault::none);
        for (std::size_t wave_index = 0U; wave_index < low.plan.wave_count;
             ++wave_index) {
            const auto& wave = low.plan.waves[wave_index];
            for (std::size_t spawn_index = 0U; spawn_index < wave.spawn_count;
                 ++spawn_index) {
                const auto* definition = arpg::combat::monster_definition(
                    wave.spawns[spawn_index].id);
                ARPG_REQUIRE(definition != nullptr);
                if (definition->preferred_ecology
                        == static_cast<std::uint8_t>(DungeonElement::fire)) {
                    ++low_weight_matching;
                }
            }
        }
        for (std::size_t wave_index = 0U; wave_index < high.plan.wave_count;
             ++wave_index) {
            const auto& wave = high.plan.waves[wave_index];
            for (std::size_t spawn_index = 0U; spawn_index < wave.spawn_count;
                 ++spawn_index) {
                const auto* definition = arpg::combat::monster_definition(
                    wave.spawns[spawn_index].id);
                ARPG_REQUIRE(definition != nullptr);
                if (definition->preferred_ecology
                        == static_cast<std::uint8_t>(DungeonElement::fire)) {
                    ++high_weight_matching;
                }
            }
        }
    }
    ARPG_REQUIRE(high_weight_matching > low_weight_matching);
    return {};
}

arpg::test::Failure invalid_director_config_is_rejected() noexcept {
    EncounterDirectorConfig config{};
    config.ranged_limit = static_cast<std::uint8_t>(
        arpg::combat::kEncounterSpawnCapacity + 1U);
    const auto result = build_encounter_plan(11U, 1U,
        DungeonElement::chaos, config);
    ARPG_REQUIRE(result.fault == DungeonFault::invalid_rules);
    return {};
}

arpg::test::Failure high_budget_priority_limit_applies_to_each_wave() noexcept {
    RoomEncounterPlan plan{};
    plan.wave_count = 2U;
    plan.total_budget = 13U;
    plan.waves[0].spawns[0].id = MonsterId::fire_bomber;
    plan.waves[0].spawns[0].spawn_ordinal = 0U;
    plan.waves[0].spawns[1].id = MonsterId::fire_charger;
    plan.waves[0].spawns[1].spawn_ordinal = 1U;
    plan.waves[0].spawn_count = 2U;
    plan.waves[0].spent_budget = 7U;
    plan.waves[1].spawns[0].id = MonsterId::chaos_chaser;
    plan.waves[1].spawns[0].spawn_ordinal = 96U;
    plan.waves[1].spawn_count = 1U;
    plan.waves[1].spent_budget = 2U;
    ARPG_REQUIRE(encounter_plan_legal(plan, EncounterDirectorConfig{}));
    return {};
}

arpg::test::Failure illegal_wave_budget_or_cost_is_rejected() noexcept {
    RoomEncounterPlan plan{};
    plan.wave_count = 2U;
    plan.total_budget = 13U;
    plan.waves[0].spawns[0].id = MonsterId::fire_charger;
    plan.waves[0].spawns[0].spawn_ordinal = 0U;
    plan.waves[0].spawns[1].id = MonsterId::water_bulwark;
    plan.waves[0].spawns[1].spawn_ordinal = 1U;
    plan.waves[0].spawn_count = 2U;
    plan.waves[0].spent_budget = 8U;
    plan.waves[1].spawns[0].id = MonsterId::chaos_chaser;
    plan.waves[1].spawns[0].spawn_ordinal = 96U;
    plan.waves[1].spawn_count = 1U;
    plan.waves[1].spent_budget = 2U;
    ARPG_REQUIRE(!encounter_plan_legal(plan, EncounterDirectorConfig{}));
    plan.waves[0].spent_budget = 7U;
    ARPG_REQUIRE(!encounter_plan_legal(plan, EncounterDirectorConfig{}));
    plan.wave_count = 0U;
    ARPG_REQUIRE(!encounter_plan_legal(plan, EncounterDirectorConfig{}));
    plan.wave_count = 3U;
    ARPG_REQUIRE(!encounter_plan_legal(plan, EncounterDirectorConfig{}));
    return {};
}

arpg::test::Failure invalid_spawn_ordinal_is_rejected() noexcept {
    RoomEncounterPlan plan{};
    plan.wave_count = 1U;
    plan.total_budget = 8U;
    plan.waves[0].spawns[0].id = MonsterId::chaos_chaser;
    plan.waves[0].spawns[0].spawn_ordinal = 1U;
    plan.waves[0].spawn_count = 1U;
    plan.waves[0].spent_budget = 2U;
    ARPG_REQUIRE(!encounter_plan_legal(plan, EncounterDirectorConfig{}));
    return {};
}

arpg::test::Failure mutually_conflicting_affixes_are_rejected_by_plan_legality() noexcept {
    RoomEncounterPlan plan{};
    plan.wave_count = 1U;
    plan.total_budget = 8U;
    auto& spawn = plan.waves[0].spawns[0];
    spawn.id = MonsterId::chaos_chaser;
    spawn.spawn_ordinal = 0U;
    spawn.affixes.values[0] = {MonsterAffixId::mighty, MonsterAffixTier::m1};
    spawn.affixes.values[1] = {MonsterAffixId::frenzy, MonsterAffixTier::m1};
    spawn.affixes.count = 2U;
    plan.waves[0].spawn_count = 1U;
    plan.waves[0].spent_budget = 2U;

    MonsterAffixCatalog catalog = arpg::combat::monster_affix_catalog();
    catalog[static_cast<std::size_t>(MonsterAffixId::mighty)].conflict_mask =
        static_cast<std::uint16_t>(1U << static_cast<std::uint8_t>(
            MonsterAffixId::frenzy));
    ARPG_REQUIRE(arpg::combat::test_support::monster_affix_catalog_valid(catalog));
    ARPG_REQUIRE(!arpg::dungeon::test_support::encounter_plan_legal_with_affix_catalog(plan,
        EncounterDirectorConfig{}, catalog));
    const auto first = plan.waves[0].spawns[0].affixes.values[0];
    plan.waves[0].spawns[0].affixes.values[0] =
        plan.waves[0].spawns[0].affixes.values[1];
    plan.waves[0].spawns[0].affixes.values[1] = first;
    ARPG_REQUIRE(!arpg::dungeon::test_support::encounter_plan_legal_with_affix_catalog(plan,
        EncounterDirectorConfig{}, catalog));
    return {};
}

arpg::test::Failure indivisible_small_two_wave_config_is_rejected() noexcept {
    EncounterDirectorConfig config{};
    config.base_budget = 2U;
    config.max_budget = 3U;
    config.depth_step = 1U;
    config.budget_per_step = 1U;
    config.two_wave_threshold = 2U;
    ARPG_REQUIRE(validate_encounter_director_config(config)
        == DungeonFault::invalid_rules);
    ARPG_REQUIRE(build_encounter_plan(77U, 2U, DungeonElement::water, config)
        .fault == DungeonFault::invalid_rules);
    return {};
}

arpg::test::Failure large_base_with_low_threshold_is_accepted() noexcept {
    EncounterDirectorConfig config{};
    config.base_budget = 8U;
    config.max_budget = 24U;
    config.two_wave_threshold = 2U;
    ARPG_REQUIRE(validate_encounter_director_config(config)
        == DungeonFault::none);
    const auto result = build_encounter_plan(
        77U, 1U, DungeonElement::water, config);
    ARPG_REQUIRE(result.fault == DungeonFault::none);
    ARPG_REQUIRE(result.plan.wave_count == 2U);
    return {};
}

arpg::test::Failure fallback_config_keeps_a_direct_target() noexcept {
    EncounterDirectorConfig config{};
    config.base_budget = 2U;
    config.max_budget = 2U;
    config.depth_step = 1U;
    config.budget_per_step = 1U;
    config.two_wave_threshold = 2U;
    const auto result = build_encounter_plan(77U, 100U,
        DungeonElement::water, config);
    ARPG_REQUIRE(result.fault == DungeonFault::none);
    ARPG_REQUIRE(result.plan.wave_count == 1U);
    ARPG_REQUIRE(result.plan.waves[0].spawn_count > 0U);
    ARPG_REQUIRE(result.plan.waves[0].spawns[0].id == MonsterId::chaos_chaser);
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"abyss budget scales and max is legal",
        &abyss_budget_is_ceil_three_halves_and_scaled_max_is_legal},
    {"abyss plan keeps normal affix prefix and allocates nothing",
        &abyss_plan_keeps_normal_affix_prefix_and_is_allocation_free},
    {"abyss budget capacity boundary and overflow",
        &abyss_budget_capacity_boundary_is_legal_and_overflow_fails},
    {"budget is bounded and depth driven", &director_budget_is_bounded_and_depth_driven},
    {"plan is deterministic and legal", &encounter_plan_is_deterministic_and_legal},
    {"generated spawn ordinals and affixes",
        &generated_spawns_have_stable_ordinals_and_legal_affixes},
    {"ecology weighting prefers matching elements", &ecology_weighting_prefers_matching_elements},
    {"invalid director config is rejected", &invalid_director_config_is_rejected},
    {"high budget limit applies to each wave", &high_budget_priority_limit_applies_to_each_wave},
    {"illegal wave budget or cost is rejected", &illegal_wave_budget_or_cost_is_rejected},
    {"invalid spawn ordinal is rejected", &invalid_spawn_ordinal_is_rejected},
    {"mutually conflicting affixes are rejected",
        &mutually_conflicting_affixes_are_rejected_by_plan_legality},
    {"indivisible small two-wave config is rejected", &indivisible_small_two_wave_config_is_rejected},
    {"large base with low threshold is accepted", &large_base_with_low_threshold_is_accepted},
    {"fallback keeps a direct target", &fallback_config_keeps_a_direct_target},
};

}  // namespace

arpg::test::TestSuite encounter_director_suite() noexcept {
    return arpg::test::make_suite("encounter_director", kCases);
}
