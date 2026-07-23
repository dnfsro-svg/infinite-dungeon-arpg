#include "combat/combat_types.hpp"
#include "combat/monster_catalog.hpp"
#include "combat/room_bounds.hpp"
#include "core/deterministic_rng.hpp"
#include "dungeon/dungeon_checkpoint.hpp"
#include "dungeon/dungeon_rules.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace arpg::dungeon::detail {
namespace {

namespace bounds = combat::room_bounds;

struct TagCounts final {
    std::uint16_t high_priority{}, ranged{}, support{}, ground_hazard{};

    [[nodiscard]] bool fits(const combat::MonsterDefinition& monster,
        std::uint8_t budget, const EncounterDirectorConfig& config) const noexcept {
        const std::uint8_t priority_limit = budget > config.two_wave_threshold
            ? config.high_budget_priority_limit
            : config.normal_high_priority_limit;
        return (!combat::has_tag(monster, combat::MonsterTag::high_priority)
                   || high_priority < priority_limit)
            && (!combat::has_tag(monster, combat::MonsterTag::ranged)
                   || ranged < config.ranged_limit)
            && (!combat::has_tag(monster, combat::MonsterTag::support)
                   || support < config.support_limit)
            && (!combat::has_tag(monster, combat::MonsterTag::ground_hazard)
                   || ground_hazard < config.ground_hazard_limit);
    }

    void add(const combat::MonsterDefinition& monster) noexcept {
        high_priority += combat::has_tag(monster, combat::MonsterTag::high_priority);
        ranged += combat::has_tag(monster, combat::MonsterTag::ranged);
        support += combat::has_tag(monster, combat::MonsterTag::support);
        ground_hazard += combat::has_tag(monster, combat::MonsterTag::ground_hazard);
    }
};

struct Candidate final {
    const combat::MonsterDefinition* monster{};
    std::uint64_t weight{};
};

struct WaveBuilder final {
    combat::EncounterWave& wave;
    std::uint8_t wave_budget;
    std::uint8_t encounter_budget;
    checkpoint::DungeonElement ecology;
    const EncounterDirectorConfig& config;
    core::DeterministicRng& selection_rng;
    core::DeterministicRng& position_rng;
    TagCounts counts{};

    [[nodiscard]] combat::Vec3 next_position() noexcept {
        const std::uint64_t raw_x = position_rng.next_bounded(16001U).value_or(0U);
        const std::uint64_t raw_y = position_rng.next_bounded(7001U).value_or(0U);
        const float x = bounds::min_x + static_cast<float>(raw_x) / 1000.0F
            * bounds::width / 16.0F;
        const float y = bounds::min_y + static_cast<float>(raw_y) / 1000.0F
            * bounds::depth / 7.0F;
        return {std::clamp(x, bounds::min_x, bounds::max_x),
            std::clamp(y, bounds::min_y, bounds::max_y), 0.0F};
    }

    [[nodiscard]] bool append(combat::MonsterId id) noexcept {
        if (wave.spawn_count >= wave.spawns.size()) return false;
        wave.spawns[wave.spawn_count++] = {id, next_position()};
        return true;
    }

    [[nodiscard]] bool append_cheapest_direct_target() noexcept {
        const combat::MonsterDefinition* best = nullptr;
        for (std::uint8_t raw = 0U;
             raw < static_cast<std::uint8_t>(combat::MonsterId::count); ++raw) {
            const auto* monster = combat::monster_definition(
                static_cast<combat::MonsterId>(raw));
            if (monster == nullptr
                    || !combat::has_tag(*monster, combat::MonsterTag::direct_target)
                    || monster->threat_cost > wave_budget
                    || !counts.fits(*monster, encounter_budget, config)) {
                continue;
            }
            // IDs are visited in ascending order, preserving the original tie-break.
            if (best == nullptr || monster->threat_cost < best->threat_cost) {
                best = monster;
            }
        }
        if (best == nullptr || !append(best->id)) return false;
        wave.spent_budget = best->threat_cost;
        counts.add(*best);
        return true;
    }

    void fill() noexcept {
        if (!append_cheapest_direct_target()) {
            const auto* fallback = combat::monster_definition(
                combat::MonsterId::chaos_chaser);
            if (fallback != nullptr && append(fallback->id)) {
                wave.spent_budget = fallback->threat_cost;
                counts.add(*fallback);
            }
        }

        while (wave.spent_budget < wave_budget
                && wave.spawn_count < wave.spawns.size()) {
            const std::uint16_t remaining = static_cast<std::uint16_t>(
                wave_budget - wave.spent_budget);
            std::array<Candidate,
                static_cast<std::size_t>(combat::MonsterId::count)> candidates{};
            std::size_t candidate_count = 0U;
            std::uint64_t total_weight = 0U;
            for (std::uint8_t raw = 0U;
                 raw < static_cast<std::uint8_t>(combat::MonsterId::count); ++raw) {
                const auto* monster = combat::monster_definition(
                    static_cast<combat::MonsterId>(raw));
                if (monster == nullptr || monster->threat_cost > remaining
                        || !counts.fits(*monster, encounter_budget, config)) {
                    continue;
                }
                const std::uint64_t weight = monster->preferred_ecology
                        == static_cast<std::uint8_t>(ecology)
                    ? config.matching_ecology_weight : config.off_ecology_weight;
                if (weight == 0U || total_weight >
                        (std::numeric_limits<std::uint64_t>::max)() - weight) {
                    continue;
                }
                candidates[candidate_count++] = {monster, weight};
                total_weight += weight;
            }
            if (total_weight == 0U) break;

            std::uint64_t cursor = selection_rng.next_bounded(total_weight).value_or(0U);
            std::size_t selected = 0U;
            while (selected + 1U < candidate_count
                    && cursor >= candidates[selected].weight) {
                cursor -= candidates[selected++].weight;
            }
            const auto& monster = *candidates[selected].monster;
            if (!append(monster.id)) break;
            wave.spent_budget = static_cast<std::uint8_t>(
                wave.spent_budget + monster.threat_cost);
            counts.add(monster);
        }
    }
};

}  // namespace

void fill_encounter_wave(combat::EncounterWave& wave,
    std::uint8_t wave_budget, std::uint8_t encounter_budget_value,
    checkpoint::DungeonElement ecology, const EncounterDirectorConfig& config,
    core::DeterministicRng& selection_rng,
    core::DeterministicRng& position_rng) noexcept {
    WaveBuilder{wave, wave_budget, encounter_budget_value, ecology, config,
        selection_rng, position_rng}.fill();
}

bool encounter_wave_tags_legal(const combat::EncounterWave& wave,
    std::uint8_t encounter_budget_value,
    const EncounterDirectorConfig& config) noexcept {
    if (wave.spawn_count > wave.spawns.size()) return false;
    TagCounts counts{};
    for (std::size_t index = 0U; index < wave.spawn_count; ++index) {
        const auto* monster = combat::monster_definition(wave.spawns[index].id);
        if (monster == nullptr
                || !counts.fits(*monster, encounter_budget_value, config)) {
            return false;
        }
        counts.add(*monster);
    }
    return true;
}

}  // namespace arpg::dungeon::detail
