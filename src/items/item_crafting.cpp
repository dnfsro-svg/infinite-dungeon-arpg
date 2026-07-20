#include "items/item_crafting.hpp"

#include "core/deterministic_rng.hpp"
#include "items/item_catalog.hpp"
#include "items/item_generation.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>

namespace arpg::items {
namespace {

inline constexpr std::uint64_t kCraftDomain = 0x4352414654494E47ULL;

struct WideUnsigned final {
    std::array<std::uint64_t, 4> limbs{};
};

inline constexpr std::uint32_t kWideBits = 256U;

bool multiply_three(WideUnsigned& value) noexcept {
    std::uint64_t carry = 0U;
    for (std::uint64_t& limb : value.limbs) {
        const std::uint64_t doubled = limb + limb;
        const std::uint64_t high_doubled = doubled < limb ? 1U : 0U;
        const std::uint64_t tripled = doubled + limb;
        std::uint64_t high = high_doubled + (tripled < doubled ? 1U : 0U);
        const std::uint64_t with_carry = tripled + carry;
        high += with_carry < tripled ? 1U : 0U;
        limb = with_carry;
        carry = high;
    }
    return carry == 0U;
}

bool wide_bit(const WideUnsigned& value, std::uint32_t bit) noexcept {
    if (bit >= kWideBits) return false;
    return ((value.limbs[bit / 64U] >> (bit % 64U)) & 1U) != 0U;
}

bool add_wide(WideUnsigned& left, const WideUnsigned& right) noexcept {
    std::uint64_t carry = 0U;
    for (std::size_t index = 0U; index < left.limbs.size(); ++index) {
        const std::uint64_t first = left.limbs[index] + right.limbs[index];
        const bool first_overflow = first < left.limbs[index];
        const std::uint64_t second = first + carry;
        const bool second_overflow = second < first;
        left.limbs[index] = second;
        carry = first_overflow || second_overflow ? 1U : 0U;
    }
    return carry == 0U;
}

bool shift_left_one(WideUnsigned& value) noexcept {
    std::uint64_t carry = 0U;
    for (std::uint64_t& limb : value.limbs) {
        const std::uint64_t next_carry = limb >> 63U;
        limb = (limb << 1U) | carry;
        carry = next_carry;
    }
    return carry == 0U;
}

WideUnsigned rounded_divide_power_two_wide(
    const WideUnsigned& value,
    std::uint32_t shift) noexcept {
    WideUnsigned result{};
    if (shift >= kWideBits) return result;
    for (std::uint32_t source_bit = shift;
         source_bit < kWideBits; ++source_bit) {
        if (!wide_bit(value, source_bit)) continue;
        const std::uint32_t target_bit = source_bit - shift;
        result.limbs[target_bit / 64U] |=
            std::uint64_t{1} << (target_bit % 64U);
    }
    if (shift != 0U && wide_bit(value, shift - 1U)) {
        WideUnsigned one{{1U, 0U, 0U, 0U}};
        static_cast<void>(add_wide(result, one));
    }
    return result;
}

std::uint64_t rounded_divide_power_two(
    const WideUnsigned& value,
    std::uint32_t shift) noexcept {
    const auto maximum = (std::numeric_limits<std::uint64_t>::max)();
    if (shift >= kWideBits) return 0U;
    for (std::uint32_t bit = shift + 64U; bit < kWideBits; ++bit) {
        if (wide_bit(value, bit)) return maximum;
    }
    std::uint64_t result = 0U;
    for (std::uint32_t bit = 0U; bit < 64U; ++bit) {
        if (wide_bit(value, shift + bit)) result |= std::uint64_t{1} << bit;
    }
    if (shift != 0U && wide_bit(value, shift - 1U)) {
        if (result == maximum) return maximum;
        ++result;
    }
    return result;
}

std::uint64_t reinforcement_bonus(
    std::uint64_t base_increment,
    std::uint32_t level,
    std::uint64_t limit,
    bool& saturated) noexcept {
    saturated = false;
    const std::uint32_t fixed_levels = level < 12U ? level : 12U;
    std::uint64_t sum = base_increment * fixed_levels;
    if (sum >= limit) {
        saturated = true;
        return limit;
    }
    if (level <= 12U) return sum;

    WideUnsigned numerator{{base_increment, 0U, 0U, 0U}};
    const std::uint32_t requested = level - 12U;
    for (std::uint32_t exponent = 1U; exponent <= requested; ++exponent) {
        if (!multiply_three(numerator)) {
            saturated = true;
            return limit;
        }
        const std::uint64_t increment =
            rounded_divide_power_two(numerator, exponent);
        if (increment >= limit - sum) {
            saturated = true;
            return limit;
        }
        sum += increment;
    }
    return sum;
}

WideUnsigned reinforcement_factor_wide(
    std::uint32_t level,
    bool& overflow) noexcept {
    overflow = false;
    const std::uint32_t fixed_levels = level < 12U ? level : 12U;
    WideUnsigned sum{{1000U * fixed_levels, 0U, 0U, 0U}};
    if (level > 12U) {
        WideUnsigned numerator{{1000U, 0U, 0U, 0U}};
        const std::uint32_t requested = level - 12U;
        for (std::uint32_t exponent = 1U;
             exponent <= requested; ++exponent) {
            if (!multiply_three(numerator)) {
                overflow = true;
                return {};
            }
            const WideUnsigned increment =
                rounded_divide_power_two_wide(numerator, exponent);
            if (!add_wide(sum, increment)) {
                overflow = true;
                return {};
            }
        }
    }
    const WideUnsigned base_factor{{10000U, 0U, 0U, 0U}};
    if (!add_wide(sum, base_factor)) {
        overflow = true;
        return {};
    }
    return sum;
}

bool multiply_wide_u64(
    const WideUnsigned& value,
    std::uint64_t multiplier,
    WideUnsigned& output) noexcept {
    output = {};
    WideUnsigned addend = value;
    for (std::uint32_t bit = 0U; bit < 64U; ++bit) {
        if (((multiplier >> bit) & 1U) != 0U
            && !add_wide(output, addend)) {
            return false;
        }
        if (bit == 63U) break;
        if (!shift_left_one(addend)
            && (multiplier >> (bit + 1U)) != 0U) {
            return false;
        }
    }
    return true;
}

std::uint64_t divide_wide_round_saturated(
    const WideUnsigned& value,
    std::uint64_t divisor,
    std::uint64_t limit) noexcept {
    std::uint64_t quotient = 0U;
    std::uint64_t remainder = 0U;
    for (std::uint32_t bit = kWideBits; bit-- > 0U;) {
        remainder = remainder * 2U + (wide_bit(value, bit) ? 1U : 0U);
        if (remainder < divisor) continue;
        remainder -= divisor;
        if (bit >= 64U) return limit;
        quotient |= std::uint64_t{1} << bit;
    }
    if (quotient > limit) return limit;
    if (remainder * 2U >= divisor) {
        if (quotient == limit) return limit;
        ++quotient;
    }
    return quotient > limit ? limit : quotient;
}

std::uint64_t magnitude(std::int64_t value) noexcept {
    if (value >= 0) return static_cast<std::uint64_t>(value);
    return static_cast<std::uint64_t>(-(value + 1)) + 1U;
}

bool valid_category(DirectedCategory category) noexcept {
    return category == DirectedCategory::damage
        || category == DirectedCategory::defense
        || category == DirectedCategory::speed
        || category == DirectedCategory::element;
}

bool category_contains(DirectedCategory category, std::uint16_t id) noexcept {
    switch (category) {
    case DirectedCategory::damage:
        return id == 1U || id == 2U || id == 105U || id == 106U;
    case DirectedCategory::defense:
        return id == 11U || id == 12U || id == 103U || id == 104U;
    case DirectedCategory::speed:
        return id == 101U || id == 102U;
    case DirectedCategory::element:
        return (id >= 3U && id <= 10U) || (id >= 107U && id <= 112U);
    case DirectedCategory::count:
        return false;
    }
    return false;
}

std::size_t collect_affixes(
    std::array<const AffixDefinition*, 24>& definitions) noexcept {
    std::size_t count = 0U;
    for (std::uint16_t id = 1U; id <= 12U; ++id)
        definitions[count++] = affix_definition(id);
    for (std::uint16_t id = 101U; id <= 112U; ++id)
        definitions[count++] = affix_definition(id);
    return count;
}

bool group_used(const ItemInstance& item,
    std::uint16_t group,
    std::optional<std::size_t> ignored = std::nullopt) noexcept {
    for (std::size_t index = 0U; index < item.affix_count; ++index) {
        if (ignored.has_value() && index == *ignored) continue;
        const AffixDefinition* definition =
            affix_definition(item.affixes[index].affix_id);
        if (definition != nullptr && definition->group_id == group) return true;
    }
    return false;
}

void kind_counts(const ItemInstance& item,
    std::optional<std::size_t> ignored,
    std::uint8_t& prefixes,
    std::uint8_t& suffixes) noexcept {
    prefixes = 0U;
    suffixes = 0U;
    for (std::size_t index = 0U; index < item.affix_count; ++index) {
        if (ignored.has_value() && index == *ignored) continue;
        const AffixDefinition* definition =
            affix_definition(item.affixes[index].affix_id);
        if (definition == nullptr) continue;
        prefixes += definition->kind == AffixKind::prefix ? 1U : 0U;
        suffixes += definition->kind == AffixKind::suffix ? 1U : 0U;
    }
}

bool candidate_compatible(const ItemInstance& item,
    const AffixDefinition& candidate,
    ItemSlot slot,
    std::optional<std::size_t> ignored = std::nullopt) noexcept {
    if ((candidate.slot_mask & slot_bit(slot)) == 0U
        || group_used(item, candidate.group_id, ignored)) {
        return false;
    }
    std::uint8_t prefixes = 0U;
    std::uint8_t suffixes = 0U;
    kind_counts(item, ignored, prefixes, suffixes);
    return candidate.kind == AffixKind::prefix ? prefixes < 3U : suffixes < 3U;
}

std::uint8_t roll_tier(core::DeterministicRng& rng,
    std::uint8_t item_level) noexcept {
    const auto weights = tier_weights(item_level);
    if (!weights.has_value()) return 0U;
    std::uint64_t total = 0U;
    for (const std::uint32_t weight : *weights) total += weight;
    const std::uint64_t roll = rng.next_bounded(total).value_or(0U);
    std::uint64_t cumulative = 0U;
    for (std::size_t index = 0U; index < weights->size(); ++index) {
        cumulative += (*weights)[index];
        if (roll < cumulative) return static_cast<std::uint8_t>(8U - index);
    }
    return 0U;
}

AffixRoll roll_affix(const AffixDefinition& definition,
    std::uint8_t item_level,
    core::DeterministicRng& rng) noexcept {
    const std::uint8_t tier = roll_tier(rng, item_level);
    const std::uint8_t variant = definition.id == 112U
        ? static_cast<std::uint8_t>(rng.next_bounded(4U).value_or(0U))
        : 0xFFU;
    return {definition.id, tier, variant};
}

bool append_random_affix(ItemInstance& item,
    ItemSlot slot,
    core::DeterministicRng& rng) noexcept {
    if (item.affix_count >= item.affixes.size()) return false;
    std::array<const AffixDefinition*, 24> all{};
    const std::size_t all_count = collect_affixes(all);
    std::array<const AffixDefinition*, 24> eligible{};
    std::size_t eligible_count = 0U;
    for (std::size_t index = 0U; index < all_count; ++index) {
        if (all[index] != nullptr
            && candidate_compatible(item, *all[index], slot)) {
            eligible[eligible_count++] = all[index];
        }
    }
    if (eligible_count == 0U) return false;
    const std::size_t selected = static_cast<std::size_t>(
        rng.next_bounded(eligible_count).value_or(0U));
    item.affixes[item.affix_count++] =
        roll_affix(*eligible[selected], item.item_level, rng);
    return item.affixes[item.affix_count - 1U].tier != 0U;
}

void refresh_required_level(ItemInstance& item) noexcept {
    item.required_level = 1U;
    for (std::size_t index = 0U; index < item.affix_count; ++index) {
        const std::uint8_t required = tier_minimum_level(item.affixes[index].tier);
        if (required > item.required_level) item.required_level = required;
    }
}

void clear_affixes(ItemInstance& item) noexcept {
    item.affixes = {};
    item.affix_count = 0U;
    item.required_level = 1U;
}

bool replace_directed(ItemInstance& item,
    ItemSlot slot,
    DirectedCategory category,
    core::DeterministicRng& rng) noexcept {
    std::array<const AffixDefinition*, 24> all{};
    const std::size_t all_count = collect_affixes(all);
    std::array<std::size_t, 6> replaceable{};
    std::size_t replaceable_count = 0U;
    for (std::size_t old_index = 0U; old_index < item.affix_count; ++old_index) {
        const AffixDefinition* old_definition =
            affix_definition(item.affixes[old_index].affix_id);
        if (old_definition == nullptr) continue;
        bool has_candidate = false;
        for (std::size_t index = 0U; index < all_count; ++index) {
            const AffixDefinition* candidate = all[index];
            if (candidate != nullptr && category_contains(category, candidate->id)
                && candidate->group_id != old_definition->group_id
                && candidate_compatible(item, *candidate, slot, old_index)) {
                has_candidate = true;
                break;
            }
        }
        if (has_candidate) replaceable[replaceable_count++] = old_index;
    }
    if (replaceable_count == 0U) return false;
    const std::size_t old_index = replaceable[static_cast<std::size_t>(
        rng.next_bounded(replaceable_count).value_or(0U))];
    std::array<const AffixDefinition*, 24> eligible{};
    std::size_t eligible_count = 0U;
    const AffixDefinition* old_definition =
        affix_definition(item.affixes[old_index].affix_id);
    if (old_definition == nullptr) return false;
    for (std::size_t index = 0U; index < all_count; ++index) {
        const AffixDefinition* candidate = all[index];
        if (candidate != nullptr && category_contains(category, candidate->id)
            && candidate->group_id != old_definition->group_id
            && candidate_compatible(item, *candidate, slot, old_index)) {
            eligible[eligible_count++] = candidate;
        }
    }
    if (eligible_count == 0U) return false;
    const AffixDefinition& selected = *eligible[static_cast<std::size_t>(
        rng.next_bounded(eligible_count).value_or(0U))];
    item.affixes[old_index] = roll_affix(selected, item.item_level, rng);
    return item.affixes[old_index].tier != 0U;
}

CraftResult rejected(const ItemInstance& item) noexcept {
    return {false, false, item};
}

}  // namespace

CraftResult craft_item(CraftRequest request) noexcept {
    if (request.item.id == 0U || !validate_item(request.item)
        || !material_is_crafting_currency(request.material)
        || (request.directed_category.has_value()
            && !valid_category(*request.directed_category))) {
        return rejected(request.item);
    }
    const BaseDefinition* base = base_definition(request.item.base_id);
    if (base == nullptr) return rejected(request.item);
    if (request.material == MaterialId::directed
        && !request.directed_category.has_value()) {
        return rejected(request.item);
    }

    const std::uint64_t stream_id = kCraftDomain
        ^ request.operation_nonce
        ^ (static_cast<std::uint64_t>(request.material) << 56U);
    core::DeterministicRng rng = core::DeterministicRng::derive_stream(
        request.root_seed ^ request.item.id, stream_id);
    ItemInstance output = request.item;
    bool success = false;

    switch (request.material) {
    case MaterialId::transmute:
        if (output.rarity == ItemRarity::normal && output.affix_count == 0U) {
            output.rarity = ItemRarity::magic;
            const std::uint8_t count = static_cast<std::uint8_t>(
                1U + rng.next_bounded(2U).value_or(0U));
            success = true;
            while (output.affix_count < count && success)
                success = append_random_affix(output, base->slot, rng);
        }
        break;
    case MaterialId::augment:
        if (output.rarity == ItemRarity::magic && output.affix_count == 1U)
            success = append_random_affix(output, base->slot, rng);
        break;
    case MaterialId::regal:
        if (output.rarity == ItemRarity::magic) {
            output.rarity = ItemRarity::rare;
            success = true;
            while (output.affix_count < 3U && success)
                success = append_random_affix(output, base->slot, rng);
        }
        break;
    case MaterialId::chaos:
        if (output.rarity == ItemRarity::rare) {
            clear_affixes(output);
            output.rarity = ItemRarity::rare;
            const std::uint64_t roll = rng.next_bounded(10U).value_or(0U);
            const std::uint8_t count = roll < 4U ? 3U
                : roll < 7U ? 4U : roll < 9U ? 5U : 6U;
            success = true;
            while (output.affix_count < count && success)
                success = append_random_affix(output, base->slot, rng);
        }
        break;
    case MaterialId::exalt:
        if (output.rarity == ItemRarity::rare && output.affix_count < 6U)
            success = append_random_affix(output, base->slot, rng);
        break;
    case MaterialId::annul:
        if (output.affix_count != 0U) {
            const std::size_t removed = static_cast<std::size_t>(
                rng.next_bounded(output.affix_count).value_or(0U));
            for (std::size_t index = removed + 1U;
                 index < output.affix_count; ++index) {
                output.affixes[index - 1U] = output.affixes[index];
            }
            output.affixes[--output.affix_count] = {};
            if (output.affix_count == 0U) output.rarity = ItemRarity::normal;
            else if (output.affix_count <= 2U) output.rarity = ItemRarity::magic;
            success = true;
        }
        break;
    case MaterialId::divine:
        // Affix values are catalog-derived from the stored id/tier/variant tuple.
        // There is no additional mutable intra-tier roll in this item model.
        success = output.affix_count != 0U;
        break;
    case MaterialId::scour:
        if (output.rarity == ItemRarity::magic
            || output.rarity == ItemRarity::rare) {
            clear_affixes(output);
            output.rarity = ItemRarity::normal;
            success = true;
        }
        break;
    case MaterialId::directed:
        if ((output.rarity == ItemRarity::magic
                || output.rarity == ItemRarity::rare)
            && output.affix_count != 0U) {
            success = replace_directed(output, base->slot,
                *request.directed_category, rng);
        }
        break;
    default:
        break;
    }

    if (!success) return rejected(request.item);
    refresh_required_level(output);
    if (!validate_item(output)) return rejected(request.item);
    return {true, true, output};
}

std::uint16_t reinforcement_success_chance_bp(
    std::uint32_t target) noexcept {
    if (target == 0U) return 0U;
    if (target <= 3U) return 10000U;
    if (target == 4U) return 9000U;
    if (target == 5U) return 8000U;
    if (target <= 12U) return 7000U;
    if (target == 13U) return 6000U;

    constexpr std::uint64_t kScale = 1000000000000ULL;
    std::uint64_t scaled = 6000U * kScale;
    const std::uint32_t requested = target - 13U;
    for (std::uint32_t exponent = 0U; exponent < requested; ++exponent) {
        scaled = (scaled * 95U + 50U) / 100U;
        const std::uint64_t rounded = (scaled + kScale / 2U) / kScale;
        if (rounded <= 10U) return 10U;
    }
    const std::uint64_t chance = (scaled + kScale / 2U) / kScale;
    return static_cast<std::uint16_t>(chance < 10U ? 10U : chance);
}

ReinforcementFailure reinforcement_failure(std::uint32_t current) noexcept {
    if (current <= 6U) return ReinforcementFailure::unchanged;
    if (current <= 9U) return ReinforcementFailure::reset_six;
    if (current <= 11U) return ReinforcementFailure::reset_zero;
    return ReinforcementFailure::destroy;
}

std::int64_t reinforced_base_value(
    std::int64_t base,
    std::uint32_t level) noexcept {
    if (base == 0) return 0;
    bool factor_overflow = false;
    const WideUnsigned factor =
        reinforcement_factor_wide(level, factor_overflow);
    WideUnsigned product{};
    if (factor_overflow
        || !multiply_wide_u64(factor, magnitude(base), product)) {
        return base < 0 ? (std::numeric_limits<std::int64_t>::min)()
                        : (std::numeric_limits<std::int64_t>::max)();
    }
    const std::uint64_t limit = base < 0
        ? std::uint64_t{1} << 63U
        : static_cast<std::uint64_t>(
            (std::numeric_limits<std::int64_t>::max)());
    const std::uint64_t result =
        divide_wide_round_saturated(product, 10000U, limit);
    if (base >= 0) return static_cast<std::int64_t>(result);
    if (result == (std::uint64_t{1} << 63U))
        return (std::numeric_limits<std::int64_t>::min)();
    return -static_cast<std::int64_t>(result);
}

std::int32_t reinforced_accessory_bonus_bp(std::uint32_t level) noexcept {
    const auto maximum = static_cast<std::uint64_t>(
        (std::numeric_limits<std::int32_t>::max)());
    bool saturated = false;
    return static_cast<std::int32_t>(
        reinforcement_bonus(50U, level, maximum, saturated));
}

ReinforcementPreview reinforcement_preview(std::uint32_t current) noexcept {
    const std::uint32_t maximum =
        (std::numeric_limits<std::uint32_t>::max)();
    const bool can_attempt = current != maximum;
    const std::uint32_t target = can_attempt ? current + 1U : current;
    return {current, target,
        can_attempt ? reinforcement_success_chance_bp(target) : 0U,
        reinforcement_failure(current), can_attempt};
}

ReinforcementPreview reinforcement_preview(const ItemInstance& item) noexcept {
    return reinforcement_preview(item.reinforcement);
}

std::optional<ItemInstance> apply_coupon(
    ItemInstance item,
    MaterialId coupon) noexcept {
    const std::uint32_t face = coupon_reinforcement_level(coupon);
    if (face == 0U || item.id == 0U || !validate_item(item)
        || item.reinforcement >= face) {
        return std::nullopt;
    }
    item.reinforcement = face;
    return item;
}

}  // namespace arpg::items
