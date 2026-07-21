#include "test_framework.hpp"

#include "allocation_probe.hpp"
#include "../combat/monster_affix_test_support.hpp"
#include "combat/monster_catalog.hpp"
#include "combat/monster_affix_catalog.hpp"
#include "combat/monster_affix_generation.hpp"
#include "combat/room_bounds.hpp"
#include "dungeon/dungeon_checkpoint.hpp"
#include "dungeon/encounter_director.hpp"
#include "dungeon/room_combat_template.hpp"

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
using arpg::dungeon::EncounterBuildRequest;
using arpg::dungeon::EncounterDirectorConfig;
using arpg::dungeon::RoomEncounterPlan;
using arpg::dungeon::build_encounter_plan;
using arpg::dungeon::build_abyss_encounter_plan;

arpg::test::Failure exact_target_count_is_built_in_one_batch() noexcept {
    const EncounterBuildRequest request{0xA11CEULL, 40U,
        arpg::dungeon::checkpoint::DungeonElement::lightning,
        arpg::dungeon::checkpoint::EntrySide::left, true, 30U};
    const auto result = build_encounter_plan(request, {});
    ARPG_REQUIRE(result.fault == DungeonFault::none);
    ARPG_REQUIRE(result.plan.wave_count == 1U);
    ARPG_REQUIRE(result.plan.initial_monster_count == 30U);
    ARPG_REQUIRE(result.plan.waves[0].spawn_count == 30U);
    ARPG_REQUIRE(result.plan.waves[1].spawn_count == 0U);
    return {};
}
using arpg::dungeon::encounter_plan_legal;
using arpg::dungeon::validate_encounter_director_config;
namespace checkpoint = arpg::dungeon::checkpoint;
using checkpoint::DungeonElement;
using checkpoint::EntrySide;

constexpr arpg::combat::Vec3 kHoleCenter{0.0F, 3.5F, 0.0F};

bool outside_circle(arpg::combat::Vec3 point,
    arpg::combat::Vec3 center, float radius) noexcept {
    const float x = point.x - center.x;
    const float y = point.y - center.y;
    return x * x + y * y > radius * radius;
}

bool safe_position(arpg::combat::Vec3 position,
    const EncounterBuildRequest& request) noexcept {
    const auto player = arpg::dungeon::make_combat_lab_config(
        request.entry, 1U);
    if (!player.has_value()
            || position.x < arpg::combat::room_bounds::min_x
            || position.x > arpg::combat::room_bounds::max_x
            || position.y < arpg::combat::room_bounds::min_y
            || position.y > arpg::combat::room_bounds::max_y
            || position.z != 0.0F
            || !outside_circle(position, player->player_spawn, 4.0F)) {
        return false;
    }
    constexpr std::array<arpg::combat::Vec3, 4> kDoorCenters{{
        {arpg::combat::room_bounds::min_x, 0.0F, 0.0F},
        {arpg::combat::room_bounds::max_x, 0.0F, 0.0F},
        {0.0F, arpg::combat::room_bounds::min_y, 0.0F},
        {0.0F, arpg::combat::room_bounds::max_y, 0.0F},
    }};
    for (const auto& door : kDoorCenters) {
        if (!outside_circle(position, door, 3.0F)) return false;
    }
    return !request.has_hole || outside_circle(position, kHoleCenter, 3.0F);
}

constexpr bool test_has_tag(
    const arpg::combat::MonsterDefinition& definition,
    MonsterTag value) noexcept {
    return (definition.tags & static_cast<std::uint16_t>(value)) != 0U;
}

bool same_encounter_plan(
    const RoomEncounterPlan& lhs,
    const RoomEncounterPlan& rhs) noexcept {
    if (lhs.wave_count != rhs.wave_count
            || lhs.initial_monster_count != rhs.initial_monster_count
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
    const EncounterBuildRequest& request,
    const EncounterDirectorConfig& config) noexcept {
    if (plan.wave_count != 1U || plan.wave_count > kEncounterWaveCapacity
            || plan.initial_monster_count < 12U
            || plan.initial_monster_count > 45U
            || plan.initial_monster_count != request.target_monster_count
            || plan.initial_monster_count != plan.waves[0].spawn_count
            || plan.waves[1].spawn_count != 0U
            || plan.waves[1].spent_budget != 0U
            || plan.total_budget == 0U) {
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
                    || spawn.position.z != 0.0F
                    || !safe_position(spawn.position, request)
                    || spawn.spawn_ordinal != spawn_index) {
                return false;
            }
        }
        if (direct_count == 0U || support_count > config.support_limit
                || ranged_count > config.ranged_limit
                || hazard_count > config.ground_hazard_limit) {
            return false;
        }
        if (high_priority_count > config.high_priority_limit
                || spent != wave.spent_budget) {
            return false;
        }
        total_spent = static_cast<std::uint16_t>(total_spent + spent);
    }
    return total_spent == plan.total_budget;
}

arpg::test::Failure required_target_counts_are_exact_and_legal() noexcept {
    constexpr std::array<std::uint8_t, 7> kCounts{{
        12U, 16U, 17U, 22U, 23U, 30U, 45U,
    }};
    for (const std::uint8_t count : kCounts) {
        const EncounterBuildRequest request{0xC0010000ULL + count, 40U,
            DungeonElement::lightning, EntrySide::bottom, true, count};
        const auto result = build_encounter_plan(request, {});
        ARPG_REQUIRE(result.fault == DungeonFault::none);
        ARPG_REQUIRE(result.plan.wave_count == 1U);
        ARPG_REQUIRE(result.plan.initial_monster_count == count);
        ARPG_REQUIRE(result.plan.waves[0].spawn_count == count);
        ARPG_REQUIRE(result.plan.waves[1].spawn_count == 0U);
        ARPG_REQUIRE(encounter_plan_legal(result.plan, request, {}));
    }
    return {};
}

arpg::test::Failure abyss_builder_consumes_exact_target_count() noexcept {
    const EncounterBuildRequest request{1U, 10000U, DungeonElement::chaos,
        EntrySide::right, true, 45U};
    const auto result = build_abyss_encounter_plan(request, {});
    ARPG_REQUIRE(result.fault == DungeonFault::none);
    ARPG_REQUIRE(result.plan.wave_count == 1U);
    ARPG_REQUIRE(result.plan.initial_monster_count == 45U);
    ARPG_REQUIRE(result.plan.waves[0].spawn_count == 45U);
    ARPG_REQUIRE(encounter_plan_legal(result.plan, request, {}));
    return {};
}

arpg::test::Failure abyss_plan_keeps_normal_affix_prefix_and_is_allocation_free() noexcept {
    const std::uint64_t before = arpg::test::allocation_count();
    for (std::uint64_t seed = 0U; seed < 1024U; ++seed) {
        const EncounterBuildRequest request{seed, 40U,
            DungeonElement::lightning, EntrySide::left, true, 45U};
        const auto result = build_abyss_encounter_plan(request, {});
        ARPG_REQUIRE(result.fault == DungeonFault::none);
        ARPG_REQUIRE(result.plan.initial_monster_count == 45U);
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

arpg::test::Failure target_count_bounds_fail_closed_without_truncation() noexcept {
    EncounterBuildRequest request{0x96ULL, 1U, DungeonElement::chaos,
        EntrySide::initial, false, 11U};
    const auto low = build_encounter_plan(request, {});
    ARPG_REQUIRE(low.fault == DungeonFault::invalid_rules);
    ARPG_REQUIRE(low.plan.wave_count == 0U);
    request.target_monster_count = 46U;
    const auto high = build_encounter_plan(request, {});
    ARPG_REQUIRE(high.fault == DungeonFault::invalid_rules);
    ARPG_REQUIRE(high.plan.wave_count == 0U);
    return {};
}

arpg::test::Failure encounter_plan_is_deterministic_and_legal() noexcept {
    const EncounterBuildRequest request{0xA11CEULL, 26U,
        DungeonElement::lightning, EntrySide::left, true, 30U};
    const auto a = build_encounter_plan(request, {});
    const auto b = build_encounter_plan(request, {});
    ARPG_REQUIRE(a.fault == DungeonFault::none);
    ARPG_REQUIRE(same_encounter_plan(a.plan, b.plan));
    ARPG_REQUIRE(test_encounter_plan_legal(a.plan, request, {}));
    ARPG_REQUIRE(encounter_plan_legal(a.plan, request, {}));
    ARPG_REQUIRE(a.plan.wave_count == 1U);
    ARPG_REQUIRE(a.plan.waves[0].spawn_count == 30U);
    return {};
}

arpg::test::Failure positions_avoid_exclusions_and_cover_expanded_bands() noexcept {
    bool saw_left_band = false;
    bool saw_right_band = false;
    bool saw_top_band = false;
    bool saw_bottom_band = false;
    for (std::uint64_t seed = 0U; seed < 4096U; ++seed) {
        const EncounterBuildRequest request{seed, 40U,
            DungeonElement::lightning,
            static_cast<EntrySide>(seed % 5U), true, 12U};
        const auto result = build_encounter_plan(request, {});
        ARPG_REQUIRE(result.fault == DungeonFault::none);
        for (std::size_t index = 0U;
             index < result.plan.waves[0].spawn_count; ++index) {
            const auto position = result.plan.waves[0].spawns[index].position;
            ARPG_REQUIRE(safe_position(position, request));
            saw_left_band = saw_left_band || position.x < -12.0F;
            saw_right_band = saw_right_band || position.x > 12.0F;
            saw_top_band = saw_top_band || position.y < -5.5F;
            saw_bottom_band = saw_bottom_band || position.y > 5.5F;
        }
    }
    ARPG_REQUIRE(saw_left_band);
    ARPG_REQUIRE(saw_right_band);
    ARPG_REQUIRE(saw_top_band);
    ARPG_REQUIRE(saw_bottom_band);
    return {};
}

arpg::test::Failure generated_spawns_have_stable_ordinals_and_legal_affixes() noexcept {
    const EncounterBuildRequest request{0xD19E5EEDULL, 40U,
        DungeonElement::lightning, EntrySide::top, true, 30U};
    const auto result = build_encounter_plan(request, {});
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
        const EncounterBuildRequest request{seed, 1U,
            DungeonElement::fire, EntrySide::initial, false, 12U};
        const auto low = build_encounter_plan(request, low_weight);
        const auto high = build_encounter_plan(request, {});
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
    const EncounterBuildRequest request{11U, 1U, DungeonElement::chaos,
        EntrySide::initial, false, 12U};
    const auto result = build_encounter_plan(request, config);
    ARPG_REQUIRE(result.fault == DungeonFault::invalid_rules);
    return {};
}

arpg::test::Failure safety_tag_limits_apply_to_the_single_batch() noexcept {
    const EncounterDirectorConfig config{};
    for (std::uint64_t seed = 0U; seed < 4096U; ++seed) {
        const EncounterBuildRequest request{seed, 40U, DungeonElement::fire,
            EntrySide::initial, true, 45U};
        const auto result = build_encounter_plan(request, config);
        ARPG_REQUIRE(result.fault == DungeonFault::none);
        ARPG_REQUIRE(encounter_plan_legal(result.plan, request, config));
        std::uint8_t high_priority{};
        std::uint8_t ranged{};
        std::uint8_t support{};
        std::uint8_t ground_hazard{};
        for (std::size_t index = 0U;
             index < result.plan.waves[0].spawn_count; ++index) {
            const auto* definition = arpg::combat::monster_definition(
                result.plan.waves[0].spawns[index].id);
            ARPG_REQUIRE(definition != nullptr);
            high_priority += test_has_tag(*definition, MonsterTag::high_priority);
            ranged += test_has_tag(*definition, MonsterTag::ranged);
            support += test_has_tag(*definition, MonsterTag::support);
            ground_hazard += test_has_tag(*definition, MonsterTag::ground_hazard);
        }
        ARPG_REQUIRE(high_priority <= config.high_priority_limit);
        ARPG_REQUIRE(ranged <= config.ranged_limit);
        ARPG_REQUIRE(support <= config.support_limit);
        ARPG_REQUIRE(ground_hazard <= config.ground_hazard_limit);
    }
    return {};
}

arpg::test::Failure illegal_count_wave_or_threat_diagnostics_are_rejected() noexcept {
    const EncounterBuildRequest request{0xBADC0DEULL, 40U,
        DungeonElement::water, EntrySide::right, true, 12U};
    const auto result = build_encounter_plan(request, {});
    ARPG_REQUIRE(result.fault == DungeonFault::none);
    RoomEncounterPlan plan = result.plan;
    ++plan.total_budget;
    ARPG_REQUIRE(!encounter_plan_legal(plan, request, {}));
    plan = result.plan;
    ++plan.waves[0].spent_budget;
    ARPG_REQUIRE(!encounter_plan_legal(plan, request, {}));
    plan = result.plan;
    plan.wave_count = 2U;
    ARPG_REQUIRE(!encounter_plan_legal(plan, request, {}));
    plan = result.plan;
    --plan.initial_monster_count;
    ARPG_REQUIRE(!encounter_plan_legal(plan, request, {}));
    plan = result.plan;
    plan.waves[1].spent_budget = 1U;
    ARPG_REQUIRE(!encounter_plan_legal(plan, request, {}));
    return {};
}

arpg::test::Failure invalid_spawn_ordinal_is_rejected() noexcept {
    const EncounterBuildRequest request{17U, 1U, DungeonElement::chaos,
        EntrySide::initial, false, 12U};
    const auto result = build_encounter_plan(request, {});
    ARPG_REQUIRE(result.fault == DungeonFault::none);
    RoomEncounterPlan plan = result.plan;
    plan.waves[0].spawns[0].spawn_ordinal = 1U;
    ARPG_REQUIRE(!encounter_plan_legal(plan, request, {}));
    plan = result.plan;
    plan.waves[0].spawns[0].position = {0.0F, 0.0F, 0.0F};
    ARPG_REQUIRE(!encounter_plan_legal(plan, request, {}));
    return {};
}

arpg::test::Failure mutually_conflicting_affixes_are_rejected_by_plan_legality() noexcept {
    const EncounterBuildRequest request{0xAFF1C7ULL, 40U,
        DungeonElement::chaos, EntrySide::left, false, 12U};
    const auto result = build_encounter_plan(request, {});
    ARPG_REQUIRE(result.fault == DungeonFault::none);
    RoomEncounterPlan plan = result.plan;
    auto& spawn = plan.waves[0].spawns[0];
    spawn.affixes.values[0] = {MonsterAffixId::mighty, MonsterAffixTier::m1};
    spawn.affixes.values[1] = {MonsterAffixId::frenzy, MonsterAffixTier::m1};
    spawn.affixes.count = 2U;

    MonsterAffixCatalog catalog = arpg::combat::monster_affix_catalog();
    catalog[static_cast<std::size_t>(MonsterAffixId::mighty)].conflict_mask =
        static_cast<std::uint16_t>(1U << static_cast<std::uint8_t>(
            MonsterAffixId::frenzy));
    ARPG_REQUIRE(arpg::combat::test_support::monster_affix_catalog_valid(catalog));
    ARPG_REQUIRE(!arpg::dungeon::test_support::encounter_plan_legal_with_affix_catalog(plan,
        request, EncounterDirectorConfig{}, catalog));
    const auto first = plan.waves[0].spawns[0].affixes.values[0];
    plan.waves[0].spawns[0].affixes.values[0] =
        plan.waves[0].spawns[0].affixes.values[1];
    plan.waves[0].spawns[0].affixes.values[1] = first;
    ARPG_REQUIRE(!arpg::dungeon::test_support::encounter_plan_legal_with_affix_catalog(plan,
        request, EncounterDirectorConfig{}, catalog));
    return {};
}

arpg::test::Failure invalid_request_fields_are_rejected() noexcept {
    EncounterDirectorConfig config{};
    EncounterBuildRequest request{77U, 2U, DungeonElement::water,
        EntrySide::left, true, 12U};
    request.ecology = static_cast<DungeonElement>(4U);
    ARPG_REQUIRE(build_encounter_plan(request, config).fault
        == DungeonFault::invalid_rules);
    request.ecology = DungeonElement::water;
    request.entry = static_cast<EntrySide>(5U);
    ARPG_REQUIRE(build_encounter_plan(request, config).fault
        == DungeonFault::invalid_rules);
    request.entry = EntrySide::left;
    request.depth = 0U;
    ARPG_REQUIRE(build_encounter_plan(request, config).fault
        == DungeonFault::invalid_rules);
    return {};
}

arpg::test::Failure zero_safety_limits_still_reach_exact_count() noexcept {
    EncounterDirectorConfig config{};
    config.high_priority_limit = 0U;
    config.ranged_limit = 0U;
    config.support_limit = 0U;
    config.ground_hazard_limit = 0U;
    ARPG_REQUIRE(validate_encounter_director_config(config)
        == DungeonFault::none);
    const EncounterBuildRequest request{77U, 1U, DungeonElement::water,
        EntrySide::bottom, true, 45U};
    const auto result = build_encounter_plan(request, config);
    ARPG_REQUIRE(result.fault == DungeonFault::none);
    ARPG_REQUIRE(result.plan.waves[0].spawn_count == 45U);
    ARPG_REQUIRE(encounter_plan_legal(result.plan, request, config));
    return {};
}

arpg::test::Failure fallback_config_keeps_a_direct_target() noexcept {
    EncounterDirectorConfig config{};
    const EncounterBuildRequest request{77U, 100U, DungeonElement::water,
        EntrySide::top, true, 30U};
    const auto result = build_encounter_plan(request, config);
    ARPG_REQUIRE(result.fault == DungeonFault::none);
    ARPG_REQUIRE(result.plan.wave_count == 1U);
    ARPG_REQUIRE(result.plan.waves[0].spawn_count == 30U);
    ARPG_REQUIRE(result.plan.waves[0].spawns[0].id == MonsterId::chaos_chaser);
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"exact target count is built in one batch",
        &exact_target_count_is_built_in_one_batch},
    {"required target counts are exact and legal",
        &required_target_counts_are_exact_and_legal},
    {"abyss builder consumes exact target count",
        &abyss_builder_consumes_exact_target_count},
    {"abyss plan keeps normal affix prefix and allocates nothing",
        &abyss_plan_keeps_normal_affix_prefix_and_is_allocation_free},
    {"target count bounds fail closed",
        &target_count_bounds_fail_closed_without_truncation},
    {"plan is deterministic and legal", &encounter_plan_is_deterministic_and_legal},
    {"positions avoid exclusions and cover expanded bands",
        &positions_avoid_exclusions_and_cover_expanded_bands},
    {"generated spawn ordinals and affixes",
        &generated_spawns_have_stable_ordinals_and_legal_affixes},
    {"ecology weighting prefers matching elements", &ecology_weighting_prefers_matching_elements},
    {"invalid director config is rejected", &invalid_director_config_is_rejected},
    {"safety tag limits apply to single batch",
        &safety_tag_limits_apply_to_the_single_batch},
    {"illegal count wave or threat diagnostics are rejected",
        &illegal_count_wave_or_threat_diagnostics_are_rejected},
    {"invalid spawn ordinal is rejected", &invalid_spawn_ordinal_is_rejected},
    {"mutually conflicting affixes are rejected",
        &mutually_conflicting_affixes_are_rejected_by_plan_legality},
    {"invalid request fields are rejected", &invalid_request_fields_are_rejected},
    {"zero safety limits still reach exact count",
        &zero_safety_limits_still_reach_exact_count},
    {"fallback keeps a direct target", &fallback_config_keeps_a_direct_target},
};

}  // namespace

arpg::test::TestSuite encounter_director_suite() noexcept {
    return arpg::test::make_suite("encounter_director", kCases);
}
