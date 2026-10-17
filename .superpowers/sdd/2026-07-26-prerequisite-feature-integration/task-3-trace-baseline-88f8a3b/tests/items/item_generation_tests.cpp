#include "test_framework.hpp"

#include "items/item_catalog.hpp"
#include "items/item_generation.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>

namespace {

using namespace arpg::items;

static_assert(std::is_trivially_copyable_v<ItemInstance>);

arpg::test::Failure rarity_weights_follow_frozen_integer_formula() noexcept {
    ARPG_REQUIRE(!rarity_weights(0U).has_value());
    ARPG_REQUIRE(!rarity_weights(101U).has_value());
    const auto level_one = rarity_weights(1U);
    const auto level_hundred = rarity_weights(100U);
    ARPG_REQUIRE(level_one.has_value());
    ARPG_REQUIRE(level_hundred.has_value());
    ARPG_REQUIRE(level_one->normal == 70U);
    ARPG_REQUIRE(level_one->magic == 25U);
    ARPG_REQUIRE(level_one->rare == 5U);
    ARPG_REQUIRE(level_hundred->normal == 40U);
    ARPG_REQUIRE(level_hundred->magic == 40U);
    ARPG_REQUIRE(level_hundred->rare == 20U);
    for (std::uint8_t level = 1U; level <= 100U; ++level) {
        const auto weights = rarity_weights(level);
        ARPG_REQUIRE(weights.has_value());
        const std::uint32_t x = static_cast<std::uint32_t>(level - 1U);
        ARPG_REQUIRE(weights->normal == 70U - (30U * x) / 99U);
        ARPG_REQUIRE(weights->magic == 25U + (15U * x) / 99U);
        ARPG_REQUIRE(weights->normal + weights->magic + weights->rare == 100U);
    }
    return {};
}

arpg::test::Failure tier_weights_unlock_and_boost_highest_two_tiers() noexcept {
    ARPG_REQUIRE(!tier_weights(0U).has_value());
    ARPG_REQUIRE(!tier_weights(101U).has_value());
    for (std::uint8_t level = 1U; level <= 100U; ++level) {
        const auto weights = tier_weights(level);
        ARPG_REQUIRE(weights.has_value());
        std::array<std::uint8_t, 8> unlocked{};
        std::size_t unlocked_count = 0U;
        for (std::uint8_t tier = 8U; tier >= 1U; --tier) {
            const std::size_t index = static_cast<std::size_t>(8U - tier);
            if (level >= tier_minimum_level(tier))
                unlocked[unlocked_count++] = tier;
            else
                ARPG_REQUIRE((*weights)[index] == 0U);
            if (tier == 1U) break;
        }
        ARPG_REQUIRE(unlocked_count != 0U);
        for (std::size_t index = 0U; index < unlocked_count; ++index) {
            const std::uint8_t tier = unlocked[index];
            std::uint32_t expected = tier_base_weight(tier);
            if (index + 1U == unlocked_count)
                expected += level / 5U;
            else if (index + 2U == unlocked_count)
                expected += level / 10U;
            ARPG_REQUIRE((*weights)[8U - tier] == expected);
        }
    }
    const auto sixteen = tier_weights(16U);
    ARPG_REQUIRE((*sixteen)[0] == 65U);
    ARPG_REQUIRE((*sixteen)[1] == 51U);
    const auto hundred = tier_weights(100U);
    ARPG_REQUIRE((*hundred)[6] == 21U);
    ARPG_REQUIRE((*hundred)[7] == 28U);
    return {};
}

arpg::test::Failure generation_rejects_invalid_local_inputs() noexcept {
    ItemGenerationRequest request{1U, ItemSlot::weapon, 1U, 1U, ItemRarity::normal};
    ARPG_REQUIRE(generate_item(request).has_value());
    request.item_level = 0U;
    ARPG_REQUIRE(!generate_item(request).has_value());
    request.item_level = 101U;
    ARPG_REQUIRE(!generate_item(request).has_value());
    request.item_level = 1U;
    request.item_id = 0U;
    ARPG_REQUIRE(!generate_item(request).has_value());
    request.item_id = 1U;
    request.slot = ItemSlot::count;
    ARPG_REQUIRE(!generate_item(request).has_value());
    request.slot = ItemSlot::weapon;
    request.forced_rarity = static_cast<ItemRarity>(3U);
    ARPG_REQUIRE(!generate_item(request).has_value());
    return {};
}

arpg::test::Failure generation_is_byte_deterministic_and_items_are_valid() noexcept {
    const ItemGenerationRequest request{
        0x123456789ABCDEF0ULL, ItemSlot::accessory, 100U, 77U, ItemRarity::rare};
    const auto first = generate_item(request);
    const auto second = generate_item(request);
    ARPG_REQUIRE(first.has_value());
    ARPG_REQUIRE(second.has_value());
    ARPG_REQUIRE(std::memcmp(&*first, &*second, sizeof(ItemInstance)) == 0);
    ARPG_REQUIRE(first->id == 77U);
    const auto accessory_bases = base_ids_for_slot(ItemSlot::accessory);
    ARPG_REQUIRE(std::find(accessory_bases.begin(), accessory_bases.end(),
        first->base_id) != accessory_bases.end());
    ARPG_REQUIRE(first->rarity == ItemRarity::rare);
    ARPG_REQUIRE(first->item_level == 100U);
    ARPG_REQUIRE(validate_item(*first));
    std::uint8_t required = 1U;
    std::uint8_t prefixes = 0U;
    std::uint8_t suffixes = 0U;
    for (std::size_t index = 0U; index < first->affix_count; ++index) {
        const auto* definition = affix_definition(first->affixes[index].affix_id);
        ARPG_REQUIRE(definition != nullptr);
        required = (std::max)(required,
            tier_minimum_level(first->affixes[index].tier));
        prefixes += definition->kind == AffixKind::prefix ? 1U : 0U;
        suffixes += definition->kind == AffixKind::suffix ? 1U : 0U;
        for (std::size_t prior = 0U; prior < index; ++prior) {
            const auto* prior_definition =
                affix_definition(first->affixes[prior].affix_id);
            ARPG_REQUIRE(prior_definition->group_id != definition->group_id);
        }
    }
    ARPG_REQUIRE(prefixes <= 3U);
    ARPG_REQUIRE(suffixes <= 3U);
    ARPG_REQUIRE(first->required_level == required);
    return {};
}

arpg::test::Failure generated_items_choose_all_three_bases_from_a_separate_domain() noexcept {
    for (std::uint8_t raw_slot = 0U;
         raw_slot < static_cast<std::uint8_t>(ItemSlot::count); ++raw_slot) {
        const ItemSlot slot = static_cast<ItemSlot>(raw_slot);
        const auto ids = base_ids_for_slot(slot);
        std::array<bool, 3> observed{};
        for (std::uint64_t seed = 1U; seed <= 4096U; ++seed) {
            const auto item = generate_item(
                {seed, slot, 100U, seed, ItemRarity::normal});
            ARPG_REQUIRE(item.has_value());
            const auto it = std::find(ids.begin(), ids.end(), item->base_id);
            ARPG_REQUIRE(it != ids.end());
            observed[static_cast<std::size_t>(it - ids.begin())] = true;
        }
        ARPG_REQUIRE(observed[0] && observed[1] && observed[2]);
    }
    return {};
}

arpg::test::Failure rarity_counts_and_all_slots_support_six_affixes() noexcept {
    const auto normal = generate_item(
        {11U, ItemSlot::helmet, 100U, 1U, ItemRarity::normal});
    ARPG_REQUIRE(normal.has_value());
    ARPG_REQUIRE(normal->affix_count == 0U);
    ARPG_REQUIRE(normal->required_level == 1U);

    std::array<bool, 3> magic_counts{};
    std::array<bool, 7> rare_counts{};
    for (std::uint64_t seed = 1U; seed <= 4096U; ++seed) {
        const auto magic = generate_item(
            {seed, ItemSlot::helmet, 100U, seed, ItemRarity::magic});
        const auto rare = generate_item(
            {seed, ItemSlot::helmet, 100U, seed, ItemRarity::rare});
        ARPG_REQUIRE(magic.has_value());
        ARPG_REQUIRE(rare.has_value());
        magic_counts[magic->affix_count] = true;
        rare_counts[rare->affix_count] = true;
    }
    ARPG_REQUIRE(magic_counts[1U] && magic_counts[2U]);
    ARPG_REQUIRE(rare_counts[3U] && rare_counts[4U]
        && rare_counts[5U] && rare_counts[6U]);

    for (std::uint8_t raw_slot = 0U;
         raw_slot < static_cast<std::uint8_t>(ItemSlot::count); ++raw_slot) {
        bool found_six = false;
        for (std::uint64_t seed = 1U; seed <= 4096U && !found_six; ++seed) {
            const auto item = generate_item({seed,
                static_cast<ItemSlot>(raw_slot), 100U,
                static_cast<std::uint64_t>(raw_slot) + 1U,
                ItemRarity::rare});
            ARPG_REQUIRE(item.has_value());
            found_six = item->affix_count == 6U;
        }
        ARPG_REQUIRE(found_six);
    }
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"frozen rarity weights", &rarity_weights_follow_frozen_integer_formula},
    {"tier unlock and highest-two boosts",
        &tier_weights_unlock_and_boost_highest_two_tiers},
    {"invalid generation inputs", &generation_rejects_invalid_local_inputs},
    {"deterministic valid generation",
        &generation_is_byte_deterministic_and_items_are_valid},
    {"three base choices use an independent domain",
        &generated_items_choose_all_three_bases_from_a_separate_domain},
    {"rarity counts and six-affix slots",
        &rarity_counts_and_all_slots_support_six_affixes},
};

}  // namespace

arpg::test::TestSuite item_generation_suite() noexcept {
    return arpg::test::make_suite("item_generation", kCases);
}
