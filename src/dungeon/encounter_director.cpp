#include "dungeon/encounter_director.hpp"

#include "combat/monster_catalog.hpp"
#include "combat/monster_affix_catalog.hpp"
#include "combat/monster_affix_generation.hpp"
#include "combat/room_bounds.hpp"
#include "core/deterministic_rng.hpp"
#include "dungeon/room_combat_template.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace arpg::dungeon::detail {

[[nodiscard]] bool checked_accumulate_threat_cost(
    std::uint8_t& total,
    std::uint8_t threat_cost) noexcept;
[[nodiscard]] bool encounter_plan_legal_with_affix_catalog(
    const RoomEncounterPlan& plan,
    const EncounterBuildRequest& request,
    const EncounterDirectorConfig& config,
    const combat::MonsterAffixCatalog& catalog) noexcept;

}  // namespace arpg::dungeon::detail

namespace arpg::combat::detail {

[[nodiscard]] bool monster_affix_catalog_valid(
    const MonsterAffixCatalog& catalog) noexcept;
[[nodiscard]] std::uint16_t monster_affix_danger_score_with_catalog(
    const MonsterAffixSet& set,
    const MonsterAffixCatalog& catalog) noexcept;

}  // namespace arpg::combat::detail

namespace arpg::dungeon {
namespace {

constexpr std::uint64_t kEncounterDirectorDomain =
    0x454E434F554E5434ULL;
constexpr std::uint64_t kEncounterPositionDomain =
    0x454E43504F534954ULL;
constexpr float kRoomMinX = combat::room_bounds::min_x;
constexpr float kRoomMaxX = combat::room_bounds::max_x;
constexpr float kRoomMinY = combat::room_bounds::min_y;
constexpr float kRoomMaxY = combat::room_bounds::max_y;
constexpr float kPlayerExclusionRadius = 4.0F;
constexpr float kNavigationExclusionRadius = 3.0F;
constexpr std::uint64_t kPositionAttempts = 32U;
constexpr std::uint64_t kPositionXMillisteps = 48001U;
constexpr std::uint64_t kPositionYMillisteps = 22001U;
constexpr std::uint64_t kFallbackGridWidth = 49U;
constexpr std::uint64_t kFallbackGridHeight = 23U;
constexpr std::uint64_t kFallbackGridSize =
    kFallbackGridWidth * kFallbackGridHeight;
constexpr combat::Vec3 kHoleCenter{0.0F, 3.5F, 0.0F};
constexpr std::array<combat::Vec3, 4> kDoorCenters{{
    {kRoomMinX, 0.0F, 0.0F},
    {kRoomMaxX, 0.0F, 0.0F},
    {0.0F, kRoomMinY, 0.0F},
    {0.0F, kRoomMaxY, 0.0F},
}};

struct TagCounts final {
    std::uint16_t high_priority{};
    std::uint16_t ranged{};
    std::uint16_t support{};
    std::uint16_t ground_hazard{};
};

struct Candidate final {
    combat::MonsterId id{combat::MonsterId::chaos_chaser};
    std::uint64_t weight{};
};

[[nodiscard]] bool valid_ecology(checkpoint::DungeonElement ecology) noexcept {
    return static_cast<std::uint8_t>(ecology) <= 3U;
}

[[nodiscard]] bool valid_entry(checkpoint::EntrySide entry) noexcept {
    return static_cast<std::uint8_t>(entry)
        <= static_cast<std::uint8_t>(checkpoint::EntrySide::right);
}

[[nodiscard]] bool fits_tag_limits(
    const combat::MonsterDefinition& definition,
    const TagCounts& counts,
    const EncounterDirectorConfig& config) noexcept {
    if (combat::has_tag(definition, combat::MonsterTag::high_priority)
            && counts.high_priority >= config.high_priority_limit) {
        return false;
    }
    if (combat::has_tag(definition, combat::MonsterTag::ranged)
            && counts.ranged >= config.ranged_limit) {
        return false;
    }
    if (combat::has_tag(definition, combat::MonsterTag::support)
            && counts.support >= config.support_limit) {
        return false;
    }
    if (combat::has_tag(definition, combat::MonsterTag::ground_hazard)
            && counts.ground_hazard >= config.ground_hazard_limit) {
        return false;
    }
    return true;
}

void add_tag_counts(
    const combat::MonsterDefinition& definition,
    TagCounts& counts) noexcept {
    counts.high_priority += combat::has_tag(definition, combat::MonsterTag::high_priority);
    counts.ranged += combat::has_tag(definition, combat::MonsterTag::ranged);
    counts.support += combat::has_tag(definition, combat::MonsterTag::support);
    counts.ground_hazard += combat::has_tag(definition, combat::MonsterTag::ground_hazard);
}

[[nodiscard]] float squared_distance(
    combat::Vec3 left,
    combat::Vec3 right) noexcept {
    const float x = left.x - right.x;
    const float y = left.y - right.y;
    return x * x + y * y;
}

[[nodiscard]] bool legal_spawn_position(
    combat::Vec3 position,
    combat::Vec3 player_spawn,
    bool has_hole) noexcept {
    if (position.x < kRoomMinX || position.x > kRoomMaxX
            || position.y < kRoomMinY || position.y > kRoomMaxY
            || position.z != 0.0F
            || squared_distance(position, player_spawn)
                <= kPlayerExclusionRadius * kPlayerExclusionRadius) {
        return false;
    }
    for (const combat::Vec3 door : kDoorCenters) {
        if (squared_distance(position, door)
                <= kNavigationExclusionRadius * kNavigationExclusionRadius) {
            return false;
        }
    }
    return !has_hole || squared_distance(position, kHoleCenter)
        > kNavigationExclusionRadius * kNavigationExclusionRadius;
}

[[nodiscard]] bool next_spawn_position(
    core::DeterministicRng& rng,
    combat::Vec3 player_spawn,
    bool has_hole,
    combat::Vec3& position) noexcept {
    for (std::uint64_t attempt = 0U; attempt < kPositionAttempts; ++attempt) {
        const auto raw_x = rng.next_bounded(kPositionXMillisteps);
        const auto raw_y = rng.next_bounded(kPositionYMillisteps);
        if (!raw_x.has_value() || !raw_y.has_value()) return false;
        const combat::Vec3 candidate{
            kRoomMinX + static_cast<float>(*raw_x) / 1000.0F,
            kRoomMinY + static_cast<float>(*raw_y) / 1000.0F,
            0.0F,
        };
        if (legal_spawn_position(candidate, player_spawn, has_hole)) {
            position = candidate;
            return true;
        }
    }

    const auto offset = rng.next_bounded(kFallbackGridSize);
    if (!offset.has_value()) return false;
    for (std::uint64_t scan = 0U; scan < kFallbackGridSize; ++scan) {
        const std::uint64_t index = (*offset + scan) % kFallbackGridSize;
        const combat::Vec3 candidate{
            kRoomMinX + static_cast<float>(index % kFallbackGridWidth),
            kRoomMinY + static_cast<float>(index / kFallbackGridWidth),
            0.0F,
        };
        if (legal_spawn_position(candidate, player_spawn, has_hole)) {
            position = candidate;
            return true;
        }
    }
    return false;
}

[[nodiscard]] bool append_spawn(
    combat::EncounterWave& wave,
    const combat::MonsterDefinition& definition,
    combat::Vec3 player_spawn,
    bool has_hole,
    core::DeterministicRng& position_rng) noexcept {
    if (wave.spawn_count >= wave.spawns.size()) return false;
    combat::Vec3 position{};
    if (!next_spawn_position(
            position_rng, player_spawn, has_hole, position)
            || !detail::checked_accumulate_threat_cost(
                wave.spent_budget, definition.threat_cost)) {
        return false;
    }
    wave.spawns[wave.spawn_count++] = {definition.id, position};
    return true;
}

[[nodiscard]] bool fill_wave(
    combat::EncounterWave& wave,
    std::uint8_t target_count,
    checkpoint::DungeonElement ecology,
    combat::Vec3 player_spawn,
    bool has_hole,
    const EncounterDirectorConfig& config,
    core::DeterministicRng& selection_rng,
    core::DeterministicRng& position_rng) noexcept {
    TagCounts counts{};
    const auto* fallback = combat::monster_definition(
        combat::MonsterId::chaos_chaser);
    if (fallback == nullptr
            || !combat::has_tag(*fallback, combat::MonsterTag::direct_target)
            || !fits_tag_limits(*fallback, counts, config)
            || !append_spawn(
                wave, *fallback, player_spawn, has_hole, position_rng)) {
        return false;
    }
    add_tag_counts(*fallback, counts);

    while (wave.spawn_count < target_count) {
        std::array<Candidate, static_cast<std::size_t>(combat::MonsterId::count)>
            candidates{};
        std::size_t candidate_count = 0U;
        std::uint64_t total_weight = 0U;
        for (std::uint8_t raw = 0U;
             raw < static_cast<std::uint8_t>(combat::MonsterId::count); ++raw) {
            const auto id = static_cast<combat::MonsterId>(raw);
            const auto* definition = combat::monster_definition(id);
            if (definition == nullptr
                    || !fits_tag_limits(*definition, counts, config)) {
                continue;
            }
            const std::uint64_t weight = definition->preferred_ecology
                    == static_cast<std::uint8_t>(ecology)
                ? config.matching_ecology_weight
                : config.off_ecology_weight;
            if (weight == 0U || total_weight >
                    (std::numeric_limits<std::uint64_t>::max)() - weight) {
                continue;
            }
            candidates[candidate_count++] = {id, weight};
            total_weight += weight;
        }
        if (candidate_count == 0U || total_weight == 0U) return false;
        const auto roll = selection_rng.next_bounded(total_weight);
        if (!roll.has_value()) return false;
        std::uint64_t cursor = *roll;
        std::size_t selected = 0U;
        for (; selected < candidate_count; ++selected) {
            if (cursor < candidates[selected].weight) break;
            cursor -= candidates[selected].weight;
        }
        if (selected >= candidate_count) return false;
        const auto* definition = combat::monster_definition(
            candidates[selected].id);
        if (definition == nullptr || !append_spawn(
                wave, *definition, player_spawn, has_hole, position_rng)) {
            return false;
        }
        add_tag_counts(*definition, counts);
    }
    return wave.spawn_count == target_count;
}

[[nodiscard]] bool affix_set_legal_for_monster(
    const combat::MonsterAffixSet& set,
    const combat::MonsterDefinition& monster,
    const combat::MonsterAffixCatalog& catalog) noexcept {
    if (set.count > set.values.size()) return false;
    for (std::size_t index = 0U; index < set.count; ++index) {
        const combat::MonsterAffixInstance& instance = set.values[index];
        const std::size_t id = static_cast<std::size_t>(instance.id);
        if (id >= catalog.size()
                || static_cast<std::uint8_t>(instance.tier)
                    >= static_cast<std::uint8_t>(combat::MonsterAffixTier::count)
                || (monster.tags & catalog[id].required_tags)
                    != catalog[id].required_tags
                || (monster.tags & catalog[id].forbidden_tags) != 0U) {
            return false;
        }
        for (std::size_t previous = 0U; previous < index; ++previous) {
            if (set.values[previous].id == instance.id) return false;
            const std::size_t previous_id = static_cast<std::size_t>(
                set.values[previous].id);
            if (previous_id >= catalog.size()) return false;
            const std::uint16_t current_bit = static_cast<std::uint16_t>(
                std::uint16_t{1} << id);
            const std::uint16_t previous_bit = static_cast<std::uint16_t>(
                std::uint16_t{1} << previous_id);
            if ((catalog[id].conflict_mask & previous_bit) != 0U
                    || (catalog[previous_id].conflict_mask & current_bit) != 0U) {
                return false;
            }
        }
    }
    return combat::detail::monster_affix_danger_score_with_catalog(set, catalog)
        <= 27U;
}

[[nodiscard]] bool apply_generated_affixes(
    RoomEncounterPlan& plan,
    std::uint64_t room_seed,
    std::uint64_t depth) noexcept {
    for (std::size_t wave_index = 0U; wave_index < plan.wave_count;
         ++wave_index) {
        auto& wave = plan.waves[wave_index];
        for (std::size_t spawn_index = 0U; spawn_index < wave.spawn_count;
             ++spawn_index) {
            auto& spawn = wave.spawns[spawn_index];
            const auto* definition = combat::monster_definition(spawn.id);
            if (definition == nullptr) return false;
            spawn.spawn_ordinal = static_cast<std::uint16_t>(wave_index
                * combat::kEncounterSpawnCapacity + spawn_index);
            const auto generated = combat::generate_monster_affixes(room_seed,
                depth, static_cast<std::uint8_t>(wave_index),
                static_cast<std::uint8_t>(spawn_index), *definition);
            if (!generated.has_value()) return false;
            spawn.affixes = *generated;
        }
    }
    return true;
}

[[nodiscard]] bool supplement_abyss_plan_affixes(
    RoomEncounterPlan& plan,
    std::uint64_t room_seed,
    std::uint64_t depth) noexcept {
    for (std::size_t wave_index = 0U; wave_index < plan.wave_count;
         ++wave_index) {
        auto& wave = plan.waves[wave_index];
        for (std::size_t spawn_index = 0U; spawn_index < wave.spawn_count;
             ++spawn_index) {
            auto& spawn = wave.spawns[spawn_index];
            const auto* definition = combat::monster_definition(spawn.id);
            if (definition == nullptr) return false;
            const auto supplemented = combat::supplement_abyss_affixes(
                room_seed, depth, static_cast<std::uint8_t>(wave_index),
                static_cast<std::uint8_t>(spawn_index), *definition,
                spawn.affixes);
            if (!supplemented.has_value()) return false;
            spawn.affixes = *supplemented;
        }
    }
    return true;
}

[[nodiscard]] bool valid_request(
    const EncounterBuildRequest& request) noexcept {
    return request.depth != 0U && valid_ecology(request.ecology)
        && valid_entry(request.entry)
        && request.target_monster_count >= 12U
        && request.target_monster_count <= 45U;
}

[[nodiscard]] EncounterPlanResult build_encounter_plan_impl(
    const EncounterBuildRequest& request,
    const EncounterDirectorConfig& config) noexcept {
    EncounterPlanResult result{};
    const auto combat_config = make_combat_lab_config(request.entry, 1U);
    if (validate_encounter_director_config(config) != DungeonFault::none
            || !valid_request(request) || !combat_config.has_value()) {
        result.fault = DungeonFault::invalid_rules;
        return result;
    }
    result.plan.wave_count = 1U;
    result.plan.initial_monster_count = request.target_monster_count;
    auto selection_rng = core::DeterministicRng::derive_stream(
        request.room_seed, kEncounterDirectorDomain);
    auto position_rng = core::DeterministicRng::derive_stream(
        request.room_seed, kEncounterPositionDomain);
    if (!fill_wave(result.plan.waves[0], request.target_monster_count,
            request.ecology, combat_config->player_spawn, request.has_hole,
            config, selection_rng, position_rng)) {
        result.fault = DungeonFault::invalid_rules;
        result.plan = {};
        return result;
    }
    result.plan.total_budget = result.plan.waves[0].spent_budget;
    if (!apply_generated_affixes(
            result.plan, request.room_seed, request.depth)
            || !detail::encounter_plan_legal_with_affix_catalog(result.plan,
                request, config, combat::monster_affix_catalog())) {
        result.fault = DungeonFault::invalid_rules;
        result.plan = {};
    }
    return result;
}

}  // namespace

bool detail::encounter_plan_legal_with_affix_catalog(
    const RoomEncounterPlan& plan,
    const EncounterBuildRequest& request,
    const EncounterDirectorConfig& config,
    const combat::MonsterAffixCatalog& catalog) noexcept {
    const auto combat_config = make_combat_lab_config(request.entry, 1U);
    if (!combat::detail::monster_affix_catalog_valid(catalog)
            || validate_encounter_director_config(config) != DungeonFault::none
            || !valid_request(request) || !combat_config.has_value()
            || plan.wave_count != 1U
            || plan.initial_monster_count < 12U
            || plan.initial_monster_count > 45U
            || plan.initial_monster_count != request.target_monster_count
            || plan.initial_monster_count != plan.waves[0].spawn_count
            || plan.waves[1].spawn_count != 0U
            || plan.waves[1].spent_budget != 0U
            || plan.total_budget == 0U || plan.total_budget > 180U) {
        return false;
    }
    std::uint16_t total_spent = 0U;
    for (std::size_t wave_index = 0U; wave_index < plan.wave_count;
         ++wave_index) {
        const auto& wave = plan.waves[wave_index];
        if (wave.spawn_count == 0U || wave.spawn_count > wave.spawns.size()) {
            return false;
        }
        TagCounts counts{};
        std::uint16_t spent = 0U;
        bool has_direct_target = false;
        for (std::size_t spawn_index = 0U; spawn_index < wave.spawn_count;
             ++spawn_index) {
            const auto& spawn = wave.spawns[spawn_index];
            const auto* definition = combat::monster_definition(spawn.id);
            if (definition == nullptr
                    || spawn.position.x < kRoomMinX
                    || spawn.position.x > kRoomMaxX
                    || spawn.position.y < kRoomMinY
                    || spawn.position.y > kRoomMaxY
                    || spawn.position.z != 0.0F
                    || !legal_spawn_position(spawn.position,
                        combat_config->player_spawn, request.has_hole)) {
                return false;
            }
            spent = static_cast<std::uint16_t>(
                spent + definition->threat_cost);
            has_direct_target = has_direct_target
                || combat::has_tag(*definition, combat::MonsterTag::direct_target);
            if (spawn.spawn_ordinal != static_cast<std::uint16_t>(wave_index
                    * combat::kEncounterSpawnCapacity + spawn_index)
                    || !affix_set_legal_for_monster(spawn.affixes,
                        *definition, catalog)) {
                return false;
            }
            add_tag_counts(*definition, counts);
        }
        if (!has_direct_target || spent != wave.spent_budget
                || counts.high_priority > config.high_priority_limit
                || counts.ranged > config.ranged_limit
                || counts.support > config.support_limit
                || counts.ground_hazard > config.ground_hazard_limit) {
            return false;
        }
        total_spent = static_cast<std::uint16_t>(total_spent + spent);
    }
    return total_spent == plan.total_budget;
}

bool encounter_plan_legal(
    const RoomEncounterPlan& plan,
    const EncounterBuildRequest& request,
    const EncounterDirectorConfig& config) noexcept {
    return detail::encounter_plan_legal_with_affix_catalog(
        plan, request, config, combat::monster_affix_catalog());
}

EncounterPlanResult build_encounter_plan(
    const EncounterBuildRequest& request,
    const EncounterDirectorConfig& config) noexcept {
    return build_encounter_plan_impl(request, config);
}

EncounterPlanResult build_abyss_encounter_plan(
    const EncounterBuildRequest& request,
    const EncounterDirectorConfig& config) noexcept {
    EncounterPlanResult result = build_encounter_plan_impl(request, config);
    if (result.fault != DungeonFault::none) return result;
    if (!supplement_abyss_plan_affixes(
            result.plan, request.room_seed, request.depth)
            || !detail::encounter_plan_legal_with_affix_catalog(result.plan,
                request, config, combat::monster_affix_catalog())) {
        return {DungeonFault::invalid_rules, {}};
    }
    return result;
}

}  // namespace arpg::dungeon

namespace arpg::dungeon::test_support {

bool encounter_plan_legal_with_affix_catalog(
    const RoomEncounterPlan& plan,
    const EncounterBuildRequest& request,
    const EncounterDirectorConfig& config,
    const combat::MonsterAffixCatalog& catalog) noexcept {
    return detail::encounter_plan_legal_with_affix_catalog(
        plan, request, config, catalog);
}

}  // namespace arpg::dungeon::test_support
