#include "combat/monster_affix_generation.hpp"

#include "combat/monster_affix_catalog.hpp"
#include "combat/monster_catalog.hpp"
#include "core/deterministic_rng.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace arpg::combat {
namespace {

struct AffixDepthBand final {
    std::uint64_t maximum_depth{};
    std::array<std::uint16_t, 4> count_weights{};
    std::array<std::uint16_t, 3> tier_weights{};
};

constexpr std::array<AffixDepthBand, 5> kAffixDepthBands{{
    {3U, {{80U, 20U, 0U, 0U}}, {{100U, 0U, 0U}}},
    {9U, {{55U, 38U, 7U, 0U}}, {{80U, 20U, 0U}}},
    {19U, {{30U, 45U, 20U, 5U}}, {{50U, 40U, 10U}}},
    {39U, {{15U, 35U, 35U, 15U}}, {{25U, 50U, 25U}}},
    {(std::numeric_limits<std::uint64_t>::max)(),
        {{5U, 20U, 40U, 35U}}, {{10U, 35U, 55U}}},
}};

constexpr std::uint64_t kAffixCountDomain = 0x41464658434E5431ULL;
constexpr std::uint64_t kAffixSelectionDomain = 0x4146465853454C31ULL;
constexpr std::uint64_t kAffixTierDomain = 0x4146465854494552ULL;
constexpr std::uint64_t kAffixContextDomain = 0x4146465843545831ULL;

struct Candidate final {
    const MonsterAffixDefinition* definition{};
};

[[nodiscard]] const MonsterAffixDefinition* catalog_definition(
    const MonsterAffixCatalog& catalog, MonsterAffixId id) noexcept {
    const std::size_t index = static_cast<std::size_t>(id);
    return index < catalog.size() && catalog[index].id == id
        ? &catalog[index] : nullptr;
}

[[nodiscard]] const AffixDepthBand& depth_band(std::uint64_t depth) noexcept {
    for (const AffixDepthBand& band : kAffixDepthBands) {
        if (depth <= band.maximum_depth) return band;
    }
    return kAffixDepthBands.back();
}

[[nodiscard]] std::uint64_t total_weight(
    const std::uint16_t* weights, std::size_t count) noexcept {
    std::uint64_t total = 0U;
    for (std::size_t index = 0U; index < count; ++index) total += weights[index];
    return total;
}

template <std::size_t N>
[[nodiscard]] std::size_t weighted_index(
    core::DeterministicRng& rng,
    const std::array<std::uint16_t, N>& weights) noexcept {
    const std::uint64_t total = total_weight(weights.data(), weights.size());
    if (total == 0U) return weights.size();
    std::uint64_t cursor = rng.next_bounded(total).value_or(total);
    for (std::size_t index = 0U; index < weights.size(); ++index) {
        if (cursor < weights[index]) return index;
        cursor -= weights[index];
    }
    return weights.size();
}

[[nodiscard]] bool compatible_with_monster(
    const MonsterAffixDefinition& definition,
    const MonsterDefinition& monster) noexcept {
    return (monster.tags & definition.required_tags) == definition.required_tags
        && (monster.tags & definition.forbidden_tags) == 0U;
}

[[nodiscard]] bool selected_contains(
    const MonsterAffixSet& set, MonsterAffixId id) noexcept {
    for (std::size_t index = 0U; index < set.count; ++index) {
        if (set.values[index].id == id) return true;
    }
    return false;
}

[[nodiscard]] bool conflicts_with_selection(
    const MonsterAffixSet& set,
    const MonsterAffixDefinition& candidate,
    const MonsterAffixCatalog& catalog) noexcept {
    const std::uint16_t candidate_bit = static_cast<std::uint16_t>(
        1U << static_cast<std::uint8_t>(candidate.id));
    for (std::size_t index = 0U; index < set.count; ++index) {
        const MonsterAffixDefinition* selected = catalog_definition(catalog,
            set.values[index].id);
        if (selected == nullptr || (candidate.conflict_mask
                & static_cast<std::uint16_t>(1U << static_cast<std::uint8_t>(
                    selected->id))) != 0U
            || (selected->conflict_mask & candidate_bit) != 0U) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] std::size_t collect_candidates(
    const MonsterDefinition& monster,
    const MonsterAffixSet& selected,
    const MonsterAffixCatalog& catalog,
    std::array<Candidate, static_cast<std::size_t>(MonsterAffixId::count)>&
        candidates) noexcept {
    std::size_t count = 0U;
    for (std::uint8_t raw = 0U;
         raw < static_cast<std::uint8_t>(MonsterAffixId::count); ++raw) {
        const MonsterAffixDefinition* definition = catalog_definition(catalog,
            static_cast<MonsterAffixId>(raw));
        if (definition == nullptr || definition->weight == 0U
                || selected_contains(selected, definition->id)
                || !compatible_with_monster(*definition, monster)
                || conflicts_with_selection(selected, *definition, catalog)) {
            continue;
        }
        candidates[count++].definition = definition;
    }
    return count;
}

[[nodiscard]] std::uint16_t danger_value(MonsterAffixDanger danger) noexcept {
    switch (danger) {
    case MonsterAffixDanger::low: return 1U;
    case MonsterAffixDanger::medium: return 2U;
    case MonsterAffixDanger::high: return 3U;
    }
    return 0U;
}

}  // namespace

std::array<std::uint16_t, 4> affix_count_weights(
    std::uint64_t depth) noexcept {
    return depth_band(depth).count_weights;
}

std::array<std::uint16_t, 3> affix_tier_weights(
    std::uint64_t depth) noexcept {
    return depth_band(depth).tier_weights;
}

std::uint64_t monster_affix_context_seed(
    std::uint64_t room_seed,
    std::uint64_t depth,
    std::uint8_t wave_index,
    std::uint8_t spawn_index) noexcept {
    auto context = core::DeterministicRng::derive_stream(room_seed,
        kAffixContextDomain);
    context = core::DeterministicRng::derive_stream(context.next_u64(), depth);
    context = core::DeterministicRng::derive_stream(context.next_u64(),
        static_cast<std::uint64_t>(wave_index));
    context = core::DeterministicRng::derive_stream(context.next_u64(),
        static_cast<std::uint64_t>(spawn_index));
    return context.next_u64();
}

std::optional<MonsterAffixSet> generate_monster_affixes(
    std::uint64_t room_seed,
    std::uint64_t depth,
    std::uint8_t wave_index,
    std::uint8_t spawn_index,
    const MonsterDefinition& monster) noexcept {
    return generate_monster_affixes_with_catalog(room_seed, depth, wave_index,
        spawn_index, monster, monster_affix_catalog());
}

std::optional<MonsterAffixSet> generate_monster_affixes_with_catalog(
    std::uint64_t room_seed,
    std::uint64_t depth,
    std::uint8_t wave_index,
    std::uint8_t spawn_index,
    const MonsterDefinition& monster,
    const MonsterAffixCatalog& catalog) noexcept {
    if (!monster_affix_catalog_valid(catalog)) return std::nullopt;
    const MonsterDefinition* const canonical = monster_definition(monster.id);
    if (canonical == nullptr || canonical->tags != monster.tags) {
        return std::nullopt;
    }
    const std::uint64_t seed = monster_affix_context_seed(room_seed, depth,
        wave_index, spawn_index);
    auto count_rng = core::DeterministicRng::derive_stream(seed,
        kAffixCountDomain);
    auto selection_rng = core::DeterministicRng::derive_stream(seed,
        kAffixSelectionDomain);
    auto tier_rng = core::DeterministicRng::derive_stream(seed,
        kAffixTierDomain);
    const std::size_t target_count = weighted_index(count_rng,
        affix_count_weights(depth));
    if (target_count > 3U) return std::nullopt;

    MonsterAffixSet result{};
    const auto tier_weights = affix_tier_weights(depth);
    for (std::size_t output_index = 0U; output_index < target_count;
         ++output_index) {
        std::array<Candidate, static_cast<std::size_t>(MonsterAffixId::count)>
            candidates{};
        const std::size_t candidate_count = collect_candidates(monster, result,
            catalog, candidates);
        if (candidate_count == 0U) return std::nullopt;
        std::array<std::uint16_t,
            static_cast<std::size_t>(MonsterAffixId::count)> weights{};
        for (std::size_t index = 0U; index < candidate_count; ++index) {
            weights[index] = candidates[index].definition->weight;
        }
        const std::uint64_t selection_total = total_weight(weights.data(),
            candidate_count);
        if (selection_total == 0U) return std::nullopt;
        std::uint64_t cursor = selection_rng.next_bounded(selection_total)
            .value_or(selection_total);
        std::size_t selected_index = 0U;
        for (; selected_index < candidate_count; ++selected_index) {
            if (cursor < weights[selected_index]) break;
            cursor -= weights[selected_index];
        }
        if (selected_index >= candidate_count) return std::nullopt;
        const std::size_t tier_index = weighted_index(tier_rng, tier_weights);
        if (tier_index >= tier_weights.size()) return std::nullopt;
        result.values[result.count++] = {candidates[selected_index].definition->id,
            static_cast<MonsterAffixTier>(tier_index)};
    }
    return result;
}

std::uint16_t monster_affix_danger_score(
    const MonsterAffixSet& set) noexcept {
    return monster_affix_danger_score_with_catalog(set, monster_affix_catalog());
}

std::uint16_t monster_affix_danger_score_with_catalog(
    const MonsterAffixSet& set,
    const MonsterAffixCatalog& catalog) noexcept {
    if (!monster_affix_catalog_valid(catalog)) return 0U;
    if (set.count > set.values.size()) return 0U;
    std::uint16_t score = 0U;
    for (std::size_t index = 0U; index < set.count; ++index) {
        const MonsterAffixInstance& instance = set.values[index];
        const MonsterAffixDefinition* definition = catalog_definition(catalog,
            instance.id);
        const std::uint8_t tier = static_cast<std::uint8_t>(instance.tier);
        if (definition == nullptr
                || tier >= static_cast<std::uint8_t>(MonsterAffixTier::count)
                || selected_contains(MonsterAffixSet{set.values,
                    static_cast<std::uint8_t>(index)}, instance.id)) {
            return 0U;
        }
        score = static_cast<std::uint16_t>(score + danger_value(
            definition->danger) * static_cast<std::uint16_t>(tier + 1U));
    }
    return score;
}

}  // namespace arpg::combat
