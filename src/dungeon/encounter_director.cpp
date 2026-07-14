#include "dungeon/encounter_director.hpp"

#include "combat/monster_catalog.hpp"
#include "combat/room_bounds.hpp"
#include "core/deterministic_rng.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace arpg::dungeon::detail {

[[nodiscard]] std::uint8_t compute_encounter_budget(
    std::uint64_t depth,
    const EncounterDirectorConfig& config) noexcept;

}  // namespace arpg::dungeon::detail

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

constexpr std::uint16_t tag(combat::MonsterTag value) noexcept {
    return static_cast<std::uint16_t>(value);
}

constexpr bool has_tag(
    const combat::MonsterDefinition& definition,
    combat::MonsterTag value) noexcept {
    return (definition.tags & tag(value)) != 0U;
}

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

[[nodiscard]] std::uint8_t priority_limit(
    std::uint8_t encounter_budget_value,
    const EncounterDirectorConfig& config) noexcept {
    return encounter_budget_value > config.two_wave_threshold
        ? config.high_budget_priority_limit
        : config.normal_high_priority_limit;
}

[[nodiscard]] bool fits_tag_limits(
    const combat::MonsterDefinition& definition,
    const TagCounts& counts,
    std::uint8_t encounter_budget_value,
    const EncounterDirectorConfig& config) noexcept {
    if (has_tag(definition, combat::MonsterTag::high_priority)
            && counts.high_priority >= priority_limit(
                encounter_budget_value, config)) {
        return false;
    }
    if (has_tag(definition, combat::MonsterTag::ranged)
            && counts.ranged >= config.ranged_limit) {
        return false;
    }
    if (has_tag(definition, combat::MonsterTag::support)
            && counts.support >= config.support_limit) {
        return false;
    }
    if (has_tag(definition, combat::MonsterTag::ground_hazard)
            && counts.ground_hazard >= config.ground_hazard_limit) {
        return false;
    }
    return true;
}

void add_tag_counts(
    const combat::MonsterDefinition& definition,
    TagCounts& counts) noexcept {
    counts.high_priority += has_tag(definition, combat::MonsterTag::high_priority);
    counts.ranged += has_tag(definition, combat::MonsterTag::ranged);
    counts.support += has_tag(definition, combat::MonsterTag::support);
    counts.ground_hazard += has_tag(definition, combat::MonsterTag::ground_hazard);
}

[[nodiscard]] combat::Vec3 next_spawn_position(
    core::DeterministicRng& rng) noexcept {
    const std::uint64_t raw_x = rng.next_bounded(16001U).value_or(0U);
    const std::uint64_t raw_y = rng.next_bounded(7001U).value_or(0U);
    const float x = kRoomMinX
        + static_cast<float>(raw_x) / 1000.0F
            * combat::room_bounds::width / 16.0F;
    const float y = kRoomMinY
        + static_cast<float>(raw_y) / 1000.0F
            * combat::room_bounds::depth / 7.0F;
    return {
        std::clamp(x, kRoomMinX, kRoomMaxX),
        std::clamp(y, kRoomMinY, kRoomMaxY),
        0.0F,
    };
}

[[nodiscard]] bool append_spawn(
    combat::EncounterWave& wave,
    combat::MonsterId id,
    core::DeterministicRng& position_rng) noexcept {
    if (wave.spawn_count >= wave.spawns.size()) {
        return false;
    }
    wave.spawns[wave.spawn_count++] = {id, next_spawn_position(position_rng)};
    return true;
}

[[nodiscard]] bool append_cheapest_direct_target(
    combat::EncounterWave& wave,
    std::uint8_t wave_budget,
    std::uint8_t encounter_budget_value,
    TagCounts& counts,
    const EncounterDirectorConfig& config,
    core::DeterministicRng& position_rng) noexcept {
    const combat::MonsterDefinition* best = nullptr;
    for (std::uint8_t raw = 0U;
         raw < static_cast<std::uint8_t>(combat::MonsterId::count); ++raw) {
        const auto* definition = combat::monster_definition(
            static_cast<combat::MonsterId>(raw));
        if (definition == nullptr
                || !has_tag(*definition, combat::MonsterTag::direct_target)
                || definition->threat_cost > wave_budget
                || !fits_tag_limits(
                    *definition, counts, encounter_budget_value, config)) {
            continue;
        }
        if (best == nullptr || definition->threat_cost < best->threat_cost
                || (definition->threat_cost == best->threat_cost
                    && static_cast<std::uint8_t>(definition->id)
                        < static_cast<std::uint8_t>(best->id))) {
            best = definition;
        }
    }
    if (best == nullptr || !append_spawn(wave, best->id, position_rng)) {
        return false;
    }
    wave.spent_budget = best->threat_cost;
    add_tag_counts(*best, counts);
    return true;
}

void fill_wave(
    combat::EncounterWave& wave,
    std::uint8_t wave_budget,
    std::uint8_t encounter_budget_value,
    checkpoint::DungeonElement ecology,
    const EncounterDirectorConfig& config,
    core::DeterministicRng& selection_rng,
    core::DeterministicRng& position_rng) noexcept {
    TagCounts counts{};
    if (!append_cheapest_direct_target(
            wave, wave_budget, encounter_budget_value, counts, config,
            position_rng)) {
        const auto* fallback = combat::monster_definition(
            combat::MonsterId::chaos_chaser);
        if (fallback != nullptr && append_spawn(
                wave, fallback->id, position_rng)) {
            wave.spent_budget = fallback->threat_cost;
            add_tag_counts(*fallback, counts);
        }
    }

    while (wave.spent_budget < wave_budget
            && wave.spawn_count < wave.spawns.size()) {
        const std::uint16_t remaining = static_cast<std::uint16_t>(
            wave_budget - wave.spent_budget);
        std::array<Candidate, static_cast<std::size_t>(combat::MonsterId::count)>
            candidates{};
        std::size_t candidate_count = 0U;
        std::uint64_t total_weight = 0U;
        for (std::uint8_t raw = 0U;
             raw < static_cast<std::uint8_t>(combat::MonsterId::count); ++raw) {
            const auto id = static_cast<combat::MonsterId>(raw);
            const auto* definition = combat::monster_definition(id);
            if (definition == nullptr || definition->threat_cost > remaining
                    || !fits_tag_limits(
                        *definition, counts, encounter_budget_value, config)) {
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
        if (candidate_count == 0U || total_weight == 0U) {
            break;
        }
        const std::uint64_t roll = selection_rng.next_bounded(total_weight)
            .value_or(0U);
        std::uint64_t cursor = roll;
        std::size_t selected = 0U;
        for (; selected < candidate_count; ++selected) {
            if (cursor < candidates[selected].weight) {
                break;
            }
            cursor -= candidates[selected].weight;
        }
        if (selected >= candidate_count) {
            selected = candidate_count - 1U;
        }
        const auto* definition = combat::monster_definition(
            candidates[selected].id);
        if (definition == nullptr || !append_spawn(
                wave, definition->id, position_rng)) {
            break;
        }
        wave.spent_budget = static_cast<std::uint8_t>(
            wave.spent_budget + definition->threat_cost);
        add_tag_counts(*definition, counts);
    }
}

}  // namespace

std::uint8_t encounter_budget(
    std::uint64_t depth,
    const EncounterDirectorConfig& config) noexcept {
    return detail::compute_encounter_budget(depth, config);
}

bool encounter_plan_legal(
    const RoomEncounterPlan& plan,
    const EncounterDirectorConfig& config) noexcept {
    if (validate_encounter_director_config(config) != DungeonFault::none
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
                    || spawn.position.z != 0.0F) {
                return false;
            }
            spent = static_cast<std::uint16_t>(
                spent + definition->threat_cost);
            has_direct_target = has_direct_target
                || has_tag(*definition, combat::MonsterTag::direct_target);
            add_tag_counts(*definition, counts);
        }
        const std::uint8_t first_wave_budget =
            plan.wave_count == 2U
                ? static_cast<std::uint8_t>((plan.total_budget + 1U) / 2U)
                : plan.total_budget;
        const std::uint8_t wave_budget = wave_index == 0U
            ? first_wave_budget
            : static_cast<std::uint8_t>(
                plan.total_budget - first_wave_budget);
        if (!has_direct_target || spent != wave.spent_budget
                || spent > wave_budget
                || counts.high_priority > priority_limit(
                    plan.total_budget, config)
                || counts.ranged > config.ranged_limit
                || counts.support > config.support_limit
                || counts.ground_hazard > config.ground_hazard_limit) {
            return false;
        }
        total_spent = static_cast<std::uint16_t>(total_spent + spent);
    }
    return total_spent <= plan.total_budget;
}

EncounterPlanResult build_encounter_plan(
    std::uint64_t room_seed,
    std::uint64_t depth,
    checkpoint::DungeonElement ecology,
    const EncounterDirectorConfig& config) noexcept {
    EncounterPlanResult result{};
    result.fault = validate_encounter_director_config(config);
    if (result.fault != DungeonFault::none || !valid_ecology(ecology)) {
        result.fault = DungeonFault::invalid_rules;
        return result;
    }

    const std::uint8_t budget = encounter_budget(depth, config);
    if (budget == 0U) {
        result.fault = DungeonFault::invalid_rules;
        return result;
    }
    result.plan.total_budget = budget;
    result.plan.wave_count = budget > config.two_wave_threshold ? 2U : 1U;
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
    fill_wave(result.plan.waves[0], first_wave_budget, budget, ecology, config,
        selection_rng, position_rng);
    if (result.plan.wave_count == 2U) {
        fill_wave(result.plan.waves[1], second_wave_budget, budget, ecology,
            config, selection_rng, position_rng);
    }
    if (!encounter_plan_legal(result.plan, config)) {
        result.fault = DungeonFault::invalid_rules;
        result.plan = {};
    }
    return result;
}

}  // namespace arpg::dungeon
