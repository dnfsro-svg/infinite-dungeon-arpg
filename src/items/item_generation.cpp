#include "items/item_generation.hpp"

#include "core/deterministic_rng.hpp"
#include "items/item_catalog.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace arpg::items {
namespace {

inline constexpr std::uint64_t kItemRarityDomain = 0x4954454D52415201ULL;
inline constexpr std::uint64_t kItemAffixCountDomain = 0x4954454D434E5401ULL;
inline constexpr std::uint64_t kItemAffixSelectionDomain = 0x4954454D53454C01ULL;
inline constexpr std::uint64_t kItemAffixTierDomain = 0x4954454D54494501ULL;
inline constexpr std::uint64_t kItemAffixVariantDomain = 0x4954454D56415201ULL;
inline constexpr std::uint64_t kRecipeIdDomain = 0x5245434950454944ULL;
inline constexpr std::uint64_t kRecipeAffixSeedDomain = 0x5245434950454146ULL;

struct Candidate final {
    const AffixDefinition* definition{};
};

bool valid_rarity(ItemRarity rarity) noexcept {
    return rarity == ItemRarity::normal || rarity == ItemRarity::magic
        || rarity == ItemRarity::rare;
}

std::uint64_t roll_bounded(
    std::uint64_t seed,
    std::uint64_t domain,
    std::uint64_t bound) noexcept {
    auto stream = core::DeterministicRng::derive_stream(seed, domain);
    return stream.next_bounded(bound).value_or(0U);
}

template <std::size_t N>
std::size_t roll_weighted_index(
    std::uint64_t seed,
    std::uint64_t domain,
    const std::array<std::uint32_t, N>& weights) noexcept {
    std::uint64_t total = 0U;
    for (const std::uint32_t weight : weights) total += weight;
    const std::uint64_t roll = roll_bounded(seed, domain, total);
    std::uint64_t cumulative = 0U;
    for (std::size_t index = 0U; index < weights.size(); ++index) {
        cumulative += weights[index];
        if (roll < cumulative) return index;
    }
    return weights.size() - 1U;
}

std::uint8_t affix_target_count(
    std::uint64_t seed,
    ItemRarity rarity) noexcept {
    if (rarity == ItemRarity::normal) return 0U;
    const std::uint64_t roll = roll_bounded(seed, kItemAffixCountDomain,
        rarity == ItemRarity::magic ? 2U : 10U);
    if (rarity == ItemRarity::magic)
        return static_cast<std::uint8_t>(1U + roll);
    if (roll < 4U) return 3U;
    if (roll < 7U) return 4U;
    if (roll < 9U) return 5U;
    return 6U;
}

std::optional<ItemRarity> select_rarity(
    std::uint64_t seed,
    std::uint8_t item_level) noexcept {
    const auto weights = rarity_weights(item_level);
    if (!weights.has_value()) return std::nullopt;
    const std::array<std::uint32_t, 3> values{{
        weights->normal, weights->magic, weights->rare}};
    return static_cast<ItemRarity>(
        roll_weighted_index(seed, kItemRarityDomain, values));
}

std::uint8_t select_tier(
    std::uint64_t seed,
    std::uint8_t item_level,
    std::size_t affix_index) noexcept {
    const auto weights = tier_weights(item_level);
    if (!weights.has_value()) return 0U;
    const std::size_t index = roll_weighted_index(seed,
        kItemAffixTierDomain + affix_index, *weights);
    return static_cast<std::uint8_t>(8U - index);
}

std::size_t collect_candidates(ItemSlot slot,
    std::array<Candidate, 24>& candidates) noexcept {
    std::size_t count = 0U;
    for (std::uint16_t id = 1U; id <= 12U; ++id) {
        const AffixDefinition* definition = affix_definition(id);
        if (definition != nullptr
            && (definition->slot_mask & slot_bit(slot)) != 0U)
            candidates[count++].definition = definition;
    }
    for (std::uint16_t id = 101U; id <= 112U; ++id) {
        const AffixDefinition* definition = affix_definition(id);
        if (definition != nullptr
            && (definition->slot_mask & slot_bit(slot)) != 0U)
            candidates[count++].definition = definition;
    }
    return count;
}

bool already_selected(const ItemInstance& item,
    std::uint16_t group_id) noexcept {
    for (std::size_t index = 0U; index < item.affix_count; ++index) {
        const AffixDefinition* definition =
            affix_definition(item.affixes[index].affix_id);
        if (definition != nullptr && definition->group_id == group_id)
            return true;
    }
    return false;
}

std::optional<ItemInstance> generate_with_rarity(
    std::uint64_t seed,
    ItemSlot slot,
    std::uint8_t item_level,
    std::uint64_t item_id,
    ItemRarity rarity) noexcept {
    ItemInstance item{};
    item.id = item_id;
    item.base_id = static_cast<std::uint8_t>(slot) + 1U;
    item.rarity = rarity;
    item.item_level = item_level;
    item.required_level = 1U;

    const std::uint8_t target_count = affix_target_count(seed, rarity);
    std::array<Candidate, 24> candidates{};
    const std::size_t candidate_count = collect_candidates(slot, candidates);
    std::size_t prefix_candidates = 0U;
    std::size_t suffix_candidates = 0U;
    for (std::size_t index = 0U; index < candidate_count; ++index) {
        if (candidates[index].definition->kind == AffixKind::prefix)
            ++prefix_candidates;
        else
            ++suffix_candidates;
    }
    const std::size_t target = target_count;
    if (prefix_candidates < (std::min)(std::size_t{3U}, target)
        || suffix_candidates < (std::min)(std::size_t{3U}, target)
        || prefix_candidates + suffix_candidates < target_count) {
        return std::nullopt;
    }

    std::uint8_t prefixes = 0U;
    std::uint8_t suffixes = 0U;
    while (item.affix_count < target_count) {
        std::array<const AffixDefinition*, 24> eligible{};
        std::size_t eligible_count = 0U;
        for (std::size_t index = 0U; index < candidate_count; ++index) {
            const AffixDefinition* definition = candidates[index].definition;
            if (already_selected(item, definition->group_id)) continue;
            if (definition->kind == AffixKind::prefix && prefixes >= 3U)
                continue;
            if (definition->kind == AffixKind::suffix && suffixes >= 3U)
                continue;
            eligible[eligible_count++] = definition;
        }
        if (eligible_count == 0U) return std::nullopt;
        const std::size_t output_index = item.affix_count;
        const std::size_t selected_index = static_cast<std::size_t>(
            roll_bounded(seed, kItemAffixSelectionDomain + output_index,
                eligible_count));
        const AffixDefinition& selected = *eligible[selected_index];
        const std::uint8_t tier = select_tier(seed, item_level, output_index);
        if (tier == 0U) return std::nullopt;
        const std::uint8_t variant = selected.id == 112U
            ? static_cast<std::uint8_t>(roll_bounded(seed,
                kItemAffixVariantDomain + output_index, 4U))
            : 0xFFU;
        item.affixes[output_index] = {selected.id, tier, variant};
        ++item.affix_count;
        prefixes += selected.kind == AffixKind::prefix ? 1U : 0U;
        suffixes += selected.kind == AffixKind::suffix ? 1U : 0U;
        const std::uint8_t level = tier_minimum_level(tier);
        if (level > item.required_level) item.required_level = level;
    }
    return validate_item(item) ? std::optional<ItemInstance>{item} : std::nullopt;
}

std::uint64_t derive_recipe_affix_seed(
    const std::array<std::uint64_t, 3>& ids) noexcept {
    const auto maximum = (std::numeric_limits<std::uint64_t>::max)();
    std::uint64_t seed = kRecipeAffixSeedDomain;
    for (const std::uint64_t id : ids) {
        auto stream = core::DeterministicRng::derive_stream(
            seed ^ id, kRecipeAffixSeedDomain);
        seed = stream.next_bounded(maximum).value_or(0U);
    }
    return seed;
}

}  // namespace

std::optional<RarityWeights> rarity_weights(
    std::uint8_t item_level) noexcept {
    if (item_level == 0U || item_level > 100U) return std::nullopt;
    const std::uint32_t x = static_cast<std::uint32_t>(item_level - 1U);
    RarityWeights result{};
    result.normal = 70U - (30U * x) / 99U;
    result.magic = 25U + (15U * x) / 99U;
    result.rare = 100U - result.normal - result.magic;
    return result;
}

std::optional<std::array<std::uint32_t, 8>> tier_weights(
    std::uint8_t item_level) noexcept {
    if (item_level == 0U || item_level > 100U) return std::nullopt;
    std::array<std::uint32_t, 8> result{};
    std::array<std::size_t, 8> unlocked{};
    std::size_t unlocked_count = 0U;
    for (std::uint8_t tier = 8U; tier >= 1U; --tier) {
        const std::size_t index = static_cast<std::size_t>(8U - tier);
        if (item_level >= tier_minimum_level(tier)) {
            result[index] = tier_base_weight(tier);
            unlocked[unlocked_count++] = index;
        }
        if (tier == 1U) break;
    }
    result[unlocked[unlocked_count - 1U]] += item_level / 5U;
    if (unlocked_count > 1U)
        result[unlocked[unlocked_count - 2U]] += item_level / 10U;
    return result;
}

std::optional<ItemInstance> generate_item(
    const ItemGenerationRequest& request) noexcept {
    if (request.item_id == 0U || request.item_level == 0U
        || request.item_level > 100U || slot_bit(request.slot) == 0U
        || (request.forced_rarity.has_value()
            && !valid_rarity(*request.forced_rarity))) {
        return std::nullopt;
    }
    const auto rarity = request.forced_rarity.has_value()
        ? request.forced_rarity
        : select_rarity(request.seed, request.item_level);
    if (!rarity.has_value()) return std::nullopt;
    return generate_with_rarity(request.seed, request.slot,
        request.item_level, request.item_id, *rarity);
}

std::optional<ItemInstance> generate_recipe_item(
    std::uint64_t root_seed,
    std::uint64_t next_sequence,
    const ItemInstance& a,
    const ItemInstance& b,
    const ItemInstance& c) noexcept {
    if (next_sequence == 0U || !validate_item(a) || !validate_item(b)
        || !validate_item(c) || a.id == 0U || b.id == 0U || c.id == 0U
        || a.id == b.id || a.id == c.id || b.id == c.id
        || a.rarity != b.rarity || a.rarity != c.rarity) {
        return std::nullopt;
    }
    const BaseDefinition* a_base = base_definition(a.base_id);
    const BaseDefinition* b_base = base_definition(b.base_id);
    const BaseDefinition* c_base = base_definition(c.base_id);
    if (a_base == nullptr || b_base == nullptr || c_base == nullptr
        || a_base->slot != b_base->slot || a_base->slot != c_base->slot) {
        return std::nullopt;
    }

    std::array<std::uint64_t, 3> ids{{a.id, b.id, c.id}};
    std::sort(ids.begin(), ids.end());
    const auto maximum = (std::numeric_limits<std::uint64_t>::max)();
    auto id_stream = core::DeterministicRng::derive_stream(
        root_seed + next_sequence, kRecipeIdDomain);
    std::uint64_t output_id =
        id_stream.next_bounded(maximum).value_or(0U);
    if (output_id == 0U) output_id = 1U;
    if (output_id == ids[0] || output_id == ids[1] || output_id == ids[2])
        return std::nullopt;

    const std::uint16_t level_sum = static_cast<std::uint16_t>(a.item_level)
        + static_cast<std::uint16_t>(b.item_level)
        + static_cast<std::uint16_t>(c.item_level);
    const std::uint8_t output_level =
        static_cast<std::uint8_t>(level_sum / 3U);
    return generate_with_rarity(derive_recipe_affix_seed(ids), a_base->slot,
        output_level, output_id, a.rarity);
}

}  // namespace arpg::items
