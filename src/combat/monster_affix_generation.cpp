#include "combat/monster_affix_generation.hpp"

#include "abyss/abyss_rules.hpp"
#include "combat/monster_affix_catalog.hpp"
#include "combat/monster_catalog.hpp"
#include "core/deterministic_rng.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace arpg::combat {
namespace detail {

[[nodiscard]] bool monster_affix_catalog_valid(
    const MonsterAffixCatalog& catalog) noexcept;
[[nodiscard]] std::uint64_t monster_affix_context_seed(
    std::uint64_t room_seed, std::uint64_t depth,
    std::uint8_t wave_index, std::uint8_t spawn_index) noexcept;
[[nodiscard]] std::optional<MonsterAffixSet>
generate_monster_affixes_with_catalog(
    std::uint64_t room_seed, std::uint64_t depth,
    std::uint8_t wave_index, std::uint8_t spawn_index,
    const MonsterDefinition& monster,
    const MonsterAffixCatalog& catalog) noexcept;
[[nodiscard]] std::optional<MonsterAffixSet>
supplement_abyss_affixes_with_catalog(
    std::uint64_t room_seed, std::uint64_t depth,
    std::uint8_t wave_index, std::uint8_t spawn_index,
    const MonsterDefinition& monster, MonsterAffixSet normal,
    const MonsterAffixCatalog& catalog) noexcept;
[[nodiscard]] std::uint16_t monster_affix_danger_score_with_catalog(
    const MonsterAffixSet& set,
    const MonsterAffixCatalog& catalog) noexcept;

}  // namespace detail
namespace {

struct AffixDepthBand final {
    std::uint64_t maximum_depth{};
    std::array<std::uint16_t, 4> count_weights{};
    std::array<std::uint16_t, 3> tier_weights{};
};

constexpr std::array<AffixDepthBand, 5> kAffixDepthBands{{
    {3U, {{100U, 0U, 0U, 0U}}, {{100U, 0U, 0U}}},
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
constexpr std::uint64_t kOrdinalAffixContextDomain =
    0x414646584F524431ULL;
constexpr std::uint64_t kOrdinalAffixDepthDomain =
    0x4146465844455031ULL;
constexpr std::uint64_t kOrdinalAffixVersionDomain =
    0x4146465856455231ULL;
constexpr std::uint64_t kOrdinalAffixSpawnDomain =
    0x4146465853504E31ULL;
constexpr std::uint64_t kAbyssAffixSelectionDomain =
    0x41425953454C3031ULL;
constexpr std::uint64_t kAbyssAffixTierDomain = 0x4142595449455231ULL;

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

[[nodiscard]] core::DeterministicRng affix_output_rng(
    std::uint64_t context_seed, std::uint64_t domain,
    std::size_t output_index) noexcept {
    auto domain_rng = core::DeterministicRng::derive_stream(context_seed,
        domain);
    return core::DeterministicRng::derive_stream(domain_rng.next_u64(),
        static_cast<std::uint64_t>(output_index));
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

[[nodiscard]] bool affix_set_valid_for_monster(
    const MonsterAffixSet& set,
    const MonsterDefinition& monster,
    const MonsterAffixCatalog& catalog) noexcept {
    if (set.count > set.values.size()) return false;
    MonsterAffixSet selected{};
    for (std::size_t index = 0U; index < set.count; ++index) {
        const MonsterAffixInstance instance = set.values[index];
        const MonsterAffixDefinition* const definition = catalog_definition(
            catalog, instance.id);
        if (definition == nullptr
                || static_cast<std::uint8_t>(instance.tier)
                    >= static_cast<std::uint8_t>(MonsterAffixTier::count)
                || !compatible_with_monster(*definition, monster)
                || selected_contains(selected, instance.id)
                || conflicts_with_selection(selected, *definition, catalog)) {
            return false;
        }
        selected.values[selected.count++] = instance;
    }
    return true;
}

[[nodiscard]] std::uint64_t derive_ordinal_affix_field(
    std::uint64_t root_seed,
    std::uint64_t domain,
    std::uint64_t value) noexcept {
    auto domain_stream = core::DeterministicRng::derive_stream(
        root_seed, domain);
    return core::DeterministicRng::derive_stream(
        domain_stream.next_u64(), value).next_u64();
}

[[nodiscard]] std::uint64_t ordinal_affix_context_seed(
    std::uint64_t room_seed,
    std::uint64_t depth,
    std::uint32_t generator_version,
    std::uint16_t spawn_ordinal) noexcept {
    auto context = core::DeterministicRng::derive_stream(
        room_seed, kOrdinalAffixContextDomain);
    std::uint64_t seed = context.next_u64();
    seed = derive_ordinal_affix_field(
        seed, kOrdinalAffixDepthDomain, depth);
    seed = derive_ordinal_affix_field(seed, kOrdinalAffixVersionDomain,
        static_cast<std::uint64_t>(generator_version));
    return derive_ordinal_affix_field(seed, kOrdinalAffixSpawnDomain,
        static_cast<std::uint64_t>(spawn_ordinal));
}

[[nodiscard]] std::optional<MonsterAffixSet>
generate_affixes_from_context(
    std::uint64_t seed,
    std::uint64_t depth,
    const MonsterDefinition& monster,
    const MonsterAffixCatalog& catalog) noexcept {
    auto count_rng = core::DeterministicRng::derive_stream(seed,
        kAffixCountDomain);
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
        auto selection_rng = affix_output_rng(seed, kAffixSelectionDomain,
            output_index);
        std::uint64_t cursor = selection_rng.next_bounded(selection_total)
            .value_or(selection_total);
        std::size_t selected_index = 0U;
        for (; selected_index < candidate_count; ++selected_index) {
            if (cursor < weights[selected_index]) break;
            cursor -= weights[selected_index];
        }
        if (selected_index >= candidate_count) return std::nullopt;
        auto tier_rng = affix_output_rng(seed, kAffixTierDomain, output_index);
        const std::size_t tier_index = weighted_index(tier_rng, tier_weights);
        if (tier_index >= tier_weights.size()) return std::nullopt;
        result.values[result.count++] = {
            candidates[selected_index].definition->id,
            static_cast<MonsterAffixTier>(tier_index),
        };
    }
    return result;
}

[[nodiscard]] std::optional<MonsterAffixSet>
supplement_abyss_affixes_from_context(
    std::uint64_t seed,
    std::uint64_t depth,
    const MonsterDefinition& monster,
    MonsterAffixSet normal,
    const MonsterAffixCatalog& catalog) noexcept {
    if (depth <= 3U) return MonsterAffixSet{};
    const std::uint8_t target_count = abyss::minimum_abyss_affixes(depth);
    if (normal.count >= target_count) return normal;

    const auto tier_weights = affix_tier_weights(depth);
    for (std::size_t output_index = normal.count;
         output_index < target_count; ++output_index) {
        std::array<Candidate, static_cast<std::size_t>(MonsterAffixId::count)>
            candidates{};
        const std::size_t candidate_count = collect_candidates(monster, normal,
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
        auto selection_rng = affix_output_rng(seed,
            kAbyssAffixSelectionDomain, output_index);
        std::uint64_t cursor = selection_rng.next_bounded(selection_total)
            .value_or(selection_total);
        std::size_t selected_index = 0U;
        for (; selected_index < candidate_count; ++selected_index) {
            if (cursor < weights[selected_index]) break;
            cursor -= weights[selected_index];
        }
        if (selected_index >= candidate_count) return std::nullopt;
        auto tier_rng = affix_output_rng(seed, kAbyssAffixTierDomain,
            output_index);
        const std::size_t tier_index = weighted_index(tier_rng, tier_weights);
        if (tier_index >= tier_weights.size()) return std::nullopt;
        normal.values[normal.count++] = {
            candidates[selected_index].definition->id,
            static_cast<MonsterAffixTier>(tier_index),
        };
    }
    return normal;
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

std::uint64_t detail::monster_affix_context_seed(
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
    return detail::generate_monster_affixes_with_catalog(room_seed, depth, wave_index,
        spawn_index, monster, monster_affix_catalog());
}

std::optional<MonsterAffixSet> supplement_abyss_affixes(
    std::uint64_t room_seed,
    std::uint64_t depth,
    std::uint8_t wave_index,
    std::uint8_t spawn_index,
    const MonsterDefinition& monster,
    MonsterAffixSet normal) noexcept {
    return detail::supplement_abyss_affixes_with_catalog(room_seed, depth,
        wave_index, spawn_index, monster, normal, monster_affix_catalog());
}

std::optional<MonsterAffixSet> generate_monster_affixes_for_ordinal(
    std::uint64_t room_seed,
    std::uint64_t depth,
    std::uint32_t generator_version,
    std::uint16_t spawn_ordinal,
    const MonsterDefinition& monster) noexcept {
    const MonsterAffixCatalog& catalog = monster_affix_catalog();
    const MonsterDefinition* const canonical = monster_definition(monster.id);
    if (generator_version == 0U
            || !detail::monster_affix_catalog_valid(catalog)
            || canonical == nullptr || canonical->tags != monster.tags) {
        return std::nullopt;
    }
    return generate_affixes_from_context(ordinal_affix_context_seed(
        room_seed, depth, generator_version, spawn_ordinal), depth,
        monster, catalog);
}

std::optional<MonsterAffixSet> supplement_abyss_affixes_for_ordinal(
    std::uint64_t room_seed,
    std::uint64_t depth,
    std::uint32_t generator_version,
    std::uint16_t spawn_ordinal,
    const MonsterDefinition& monster,
    MonsterAffixSet normal) noexcept {
    const MonsterAffixCatalog& catalog = monster_affix_catalog();
    const MonsterDefinition* const canonical = monster_definition(monster.id);
    if (generator_version == 0U
            || !detail::monster_affix_catalog_valid(catalog)
            || canonical == nullptr || canonical->tags != monster.tags
            || !affix_set_valid_for_monster(normal, monster, catalog)) {
        return std::nullopt;
    }
    return supplement_abyss_affixes_from_context(ordinal_affix_context_seed(
        room_seed, depth, generator_version, spawn_ordinal), depth,
        monster, normal, catalog);
}

namespace detail {

std::optional<MonsterAffixSet> generate_monster_affixes_with_catalog(
    std::uint64_t room_seed,
    std::uint64_t depth,
    std::uint8_t wave_index,
    std::uint8_t spawn_index,
    const MonsterDefinition& monster,
    const MonsterAffixCatalog& catalog) noexcept {
    if (!detail::monster_affix_catalog_valid(catalog)) return std::nullopt;
    const MonsterDefinition* const canonical = monster_definition(monster.id);
    if (canonical == nullptr || canonical->tags != monster.tags) {
        return std::nullopt;
    }
    const std::uint64_t seed = monster_affix_context_seed(room_seed, depth,
        wave_index, spawn_index);
    return generate_affixes_from_context(seed, depth, monster, catalog);
}

std::optional<MonsterAffixSet> supplement_abyss_affixes_with_catalog(
    std::uint64_t room_seed,
    std::uint64_t depth,
    std::uint8_t wave_index,
    std::uint8_t spawn_index,
    const MonsterDefinition& monster,
    MonsterAffixSet normal,
    const MonsterAffixCatalog& catalog) noexcept {
    if (!detail::monster_affix_catalog_valid(catalog)) return std::nullopt;
    const MonsterDefinition* const canonical = monster_definition(monster.id);
    if (canonical == nullptr || canonical->tags != monster.tags
            || !affix_set_valid_for_monster(normal, monster, catalog)) {
        return std::nullopt;
    }
    const std::uint64_t seed = monster_affix_context_seed(room_seed, depth,
        wave_index, spawn_index);
    return supplement_abyss_affixes_from_context(
        seed, depth, monster, normal, catalog);
}

std::uint16_t monster_affix_danger_score_with_catalog(
    const MonsterAffixSet& set,
    const MonsterAffixCatalog& catalog) noexcept {
    if (!detail::monster_affix_catalog_valid(catalog)) return 0U;
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

}  // namespace detail

std::uint16_t monster_affix_danger_score(
    const MonsterAffixSet& set) noexcept {
    return detail::monster_affix_danger_score_with_catalog(set,
        monster_affix_catalog());
}

}  // namespace arpg::combat

namespace arpg::combat::test_support {

bool monster_affix_catalog_valid(const MonsterAffixCatalog& catalog) noexcept {
    return detail::monster_affix_catalog_valid(catalog);
}

std::optional<MonsterAffixSet> generate_monster_affixes_with_catalog(
    std::uint64_t room_seed, std::uint64_t depth,
    std::uint8_t wave_index, std::uint8_t spawn_index,
    const MonsterDefinition& monster,
    const MonsterAffixCatalog& catalog) noexcept {
    return detail::generate_monster_affixes_with_catalog(room_seed, depth,
        wave_index, spawn_index, monster, catalog);
}

std::optional<MonsterAffixSet> supplement_abyss_affixes_with_catalog(
    std::uint64_t room_seed, std::uint64_t depth,
    std::uint8_t wave_index, std::uint8_t spawn_index,
    const MonsterDefinition& monster, MonsterAffixSet normal,
    const MonsterAffixCatalog& catalog) noexcept {
    return detail::supplement_abyss_affixes_with_catalog(room_seed, depth,
        wave_index, spawn_index, monster, normal, catalog);
}

std::uint64_t monster_affix_context_seed(
    std::uint64_t room_seed, std::uint64_t depth,
    std::uint8_t wave_index, std::uint8_t spawn_index) noexcept {
    return detail::monster_affix_context_seed(room_seed, depth, wave_index,
        spawn_index);
}

std::uint64_t monster_affix_ordinal_context_seed(
    std::uint64_t room_seed,
    std::uint64_t depth,
    std::uint32_t generator_version,
    std::uint16_t spawn_ordinal) noexcept {
    return ordinal_affix_context_seed(
        room_seed, depth, generator_version, spawn_ordinal);
}

std::uint64_t monster_affix_count_seed(
    std::uint64_t room_seed, std::uint64_t depth,
    std::uint8_t wave_index, std::uint8_t spawn_index) noexcept {
    return core::DeterministicRng::derive_stream(
        detail::monster_affix_context_seed(room_seed, depth, wave_index,
            spawn_index), kAffixCountDomain).next_u64();
}

std::uint64_t monster_affix_selection_seed(
    std::uint64_t room_seed, std::uint64_t depth,
    std::uint8_t wave_index, std::uint8_t spawn_index) noexcept {
    return core::DeterministicRng::derive_stream(
        detail::monster_affix_context_seed(room_seed, depth, wave_index,
            spawn_index), kAffixSelectionDomain).next_u64();
}

std::uint64_t monster_affix_tier_seed(
    std::uint64_t room_seed, std::uint64_t depth,
    std::uint8_t wave_index, std::uint8_t spawn_index) noexcept {
    return core::DeterministicRng::derive_stream(
        detail::monster_affix_context_seed(room_seed, depth, wave_index,
            spawn_index), kAffixTierDomain).next_u64();
}

}  // namespace arpg::combat::test_support
