#include "dungeon/encounter_director.hpp"

#include "abyss/abyss_rules.hpp"
#include "combat/monster_catalog.hpp"
#include "combat/monster_affix_catalog.hpp"
#include "combat/monster_affix_generation.hpp"
#include "combat/room_bounds.hpp"
#include "core/deterministic_rng.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>

namespace arpg::dungeon::detail {

[[nodiscard]] std::uint8_t compute_encounter_budget(
    std::uint64_t depth,
    const EncounterDirectorConfig& config) noexcept;
[[nodiscard]] bool encounter_plan_legal_with_affix_catalog(
    const RoomEncounterPlan& plan,
    const EncounterDirectorConfig& config,
    const combat::MonsterAffixCatalog& catalog) noexcept;
void fill_encounter_wave(combat::EncounterWave& wave,
    std::uint8_t wave_budget, std::uint8_t encounter_budget_value,
    checkpoint::DungeonElement ecology,
    const EncounterDirectorConfig& config,
    core::DeterministicRng& selection_rng,
    core::DeterministicRng& position_rng) noexcept;
[[nodiscard]] bool encounter_wave_tags_legal(const combat::EncounterWave& wave,
    std::uint8_t encounter_budget_value,
    const EncounterDirectorConfig& config) noexcept;

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

[[nodiscard]] bool valid_ecology(checkpoint::DungeonElement ecology) noexcept {
    return static_cast<std::uint8_t>(ecology) <= 3U;
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

[[nodiscard]] bool apply_affixes(
    RoomEncounterPlan& plan,
    std::uint64_t room_seed,
    std::uint64_t depth,
    bool supplement_abyss) noexcept {
    for (std::size_t wave_index = 0U; wave_index < plan.wave_count;
         ++wave_index) {
        auto& wave = plan.waves[wave_index];
        for (std::size_t spawn_index = 0U; spawn_index < wave.spawn_count;
             ++spawn_index) {
            auto& spawn = wave.spawns[spawn_index];
            const auto* definition = combat::monster_definition(spawn.id);
            if (definition == nullptr) return false;
            if (!supplement_abyss) {
                spawn.spawn_ordinal = static_cast<std::uint16_t>(wave_index
                    * combat::kEncounterSpawnCapacity + spawn_index);
            }
            const auto generated = supplement_abyss
                ? combat::supplement_abyss_affixes(room_seed, depth,
                    static_cast<std::uint8_t>(wave_index),
                    static_cast<std::uint8_t>(spawn_index), *definition,
                    spawn.affixes)
                : combat::generate_monster_affixes(room_seed, depth,
                    static_cast<std::uint8_t>(wave_index),
                    static_cast<std::uint8_t>(spawn_index), *definition);
            if (!generated.has_value()) return false;
            spawn.affixes = *generated;
        }
    }
    return true;
}

[[nodiscard]] EncounterPlanResult build_encounter_plan_with_budget(
    std::uint64_t room_seed,
    std::uint64_t depth,
    checkpoint::DungeonElement ecology,
    const EncounterDirectorConfig& director_config,
    const EncounterDirectorConfig& legality_config,
    std::uint8_t budget) noexcept {
    EncounterPlanResult result{};
    if (budget == 0U) {
        result.fault = DungeonFault::invalid_rules;
        return result;
    }
    result.plan.total_budget = budget;
    result.plan.wave_count = budget > director_config.two_wave_threshold
        ? 2U : 1U;
    const std::uint8_t first_wave_budget =
        result.plan.wave_count == 2U
            ? static_cast<std::uint8_t>((budget + 1U) / 2U)
            : budget;
    const std::uint8_t second_wave_budget =
        static_cast<std::uint8_t>(budget - first_wave_budget);
    auto selection_rng = core::DeterministicRng::derive_stream(
        room_seed, kEncounterDirectorDomain);
    auto position_rng = core::DeterministicRng::derive_stream(
        room_seed, kEncounterPositionDomain);
    detail::fill_encounter_wave(result.plan.waves[0], first_wave_budget,
        budget, ecology, director_config, selection_rng, position_rng);
    if (result.plan.wave_count == 2U) {
        detail::fill_encounter_wave(result.plan.waves[1], second_wave_budget,
            budget, ecology, director_config, selection_rng, position_rng);
    }
    if (!apply_affixes(result.plan, room_seed, depth, false)
            || !detail::encounter_plan_legal_with_affix_catalog(result.plan,
                legality_config, combat::monster_affix_catalog())) {
        result.fault = DungeonFault::invalid_rules;
        result.plan = {};
    }
    return result;
}

}  // namespace

std::uint8_t encounter_budget(
    std::uint64_t depth,
    const EncounterDirectorConfig& config) noexcept {
    return detail::compute_encounter_budget(depth, config);
}

bool detail::encounter_plan_legal_with_affix_catalog(
    const RoomEncounterPlan& plan,
    const EncounterDirectorConfig& config,
    const combat::MonsterAffixCatalog& catalog) noexcept {
    if (!combat::detail::monster_affix_catalog_valid(catalog)
            || validate_encounter_director_config(config) != DungeonFault::none
            || plan.wave_count == 0U
            || plan.wave_count > plan.waves.size()
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
    for (std::size_t wave_index = 0U; wave_index < plan.wave_count;
         ++wave_index) {
        const auto& wave = plan.waves[wave_index];
        if (wave.spawn_count == 0U || wave.spawn_count > wave.spawns.size()) {
            return false;
        }
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
                    || spawn.position.z != 0.0F) {
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
        }
        const std::uint8_t first_wave_budget =
            plan.wave_count == 2U
                ? static_cast<std::uint8_t>((plan.total_budget + 1U) / 2U)
                : plan.total_budget;
        const std::uint8_t wave_budget = wave_index == 0U
            ? first_wave_budget
            : static_cast<std::uint8_t>(
                plan.total_budget - first_wave_budget);
        if (!has_direct_target
                || !detail::encounter_wave_tags_legal(
                    wave, plan.total_budget, config)
                || spent != wave.spent_budget
                || spent > wave_budget) {
            return false;
        }
        total_spent = static_cast<std::uint16_t>(total_spent + spent);
    }
    return total_spent <= plan.total_budget;
}

bool encounter_plan_legal(
    const RoomEncounterPlan& plan,
    const EncounterDirectorConfig& config) noexcept {
    return detail::encounter_plan_legal_with_affix_catalog(plan, config,
        combat::monster_affix_catalog());
}

EncounterPlanResult build_encounter_plan(
    std::uint64_t room_seed,
    std::uint64_t depth,
    checkpoint::DungeonElement ecology,
    const EncounterDirectorConfig& config) noexcept {
    if (validate_encounter_director_config(config) != DungeonFault::none
            || !valid_ecology(ecology)) {
        return {DungeonFault::invalid_rules, {}};
    }
    return build_encounter_plan_with_budget(room_seed, depth, ecology, config,
        config, encounter_budget(depth, config));
}

std::optional<EncounterDirectorConfig> abyss_encounter_legality_config(
    const EncounterDirectorConfig& config) noexcept {
    if (validate_encounter_director_config(config) != DungeonFault::none) {
        return std::nullopt;
    }
    const std::uint16_t abyss_max = abyss::abyss_encounter_budget_wide(
        config.max_budget);
    if (abyss_max > (std::numeric_limits<std::uint8_t>::max)()) {
        return std::nullopt;
    }
    EncounterDirectorConfig legality = config;
    legality.max_budget = static_cast<std::uint8_t>(abyss_max);
    if (validate_encounter_director_config(legality) != DungeonFault::none) {
        return std::nullopt;
    }
    return legality;
}

EncounterPlanResult build_abyss_encounter_plan(
    std::uint64_t room_seed,
    std::uint64_t depth,
    checkpoint::DungeonElement ecology,
    const EncounterDirectorConfig& config) noexcept {
    if (validate_encounter_director_config(config) != DungeonFault::none
            || !valid_ecology(ecology)) {
        return {DungeonFault::invalid_rules, {}};
    }
    const std::uint16_t abyss_budget = abyss::abyss_encounter_budget_wide(
        encounter_budget(depth, config));
    const auto legality_config = abyss_encounter_legality_config(config);
    if (abyss_budget == 0U
            || abyss_budget > (std::numeric_limits<std::uint8_t>::max)()
            || !legality_config.has_value()) {
        return {DungeonFault::invalid_rules, {}};
    }
    EncounterPlanResult result = build_encounter_plan_with_budget(room_seed,
        depth, ecology, config, *legality_config,
        static_cast<std::uint8_t>(abyss_budget));
    if (result.fault != DungeonFault::none) return result;
    if (!apply_affixes(result.plan, room_seed, depth, true)
            || !detail::encounter_plan_legal_with_affix_catalog(result.plan,
                *legality_config, combat::monster_affix_catalog())) {
        return {DungeonFault::invalid_rules, {}};
    }
    return result;
}

}  // namespace arpg::dungeon

namespace arpg::dungeon::test_support {

bool encounter_plan_legal_with_affix_catalog(
    const RoomEncounterPlan& plan,
    const EncounterDirectorConfig& config,
    const combat::MonsterAffixCatalog& catalog) noexcept {
    return detail::encounter_plan_legal_with_affix_catalog(plan, config, catalog);
}

}  // namespace arpg::dungeon::test_support
