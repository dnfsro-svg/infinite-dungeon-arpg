#include "test_framework.hpp"

#include "items/item_catalog.hpp"
#include "items/item_crafting.hpp"
#include "items/item_generation.hpp"
#include "items/material_catalog.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <optional>

namespace {

using namespace arpg::items;

static_assert(kMaterialCount == 14U);

ItemInstance generated_with_count(ItemSlot slot,
    ItemRarity rarity,
    std::uint8_t count) noexcept {
    for (std::uint64_t seed = 1U; seed <= 4096U; ++seed) {
        const auto item = generate_item(
            {seed, slot, 100U, seed + 10000U, rarity});
        if (item.has_value() && item->affix_count == count) return *item;
    }
    return {};
}

CraftResult craft(MaterialId material,
    const ItemInstance& item,
    std::uint64_t seed = 0x123456789ABCDEF0ULL,
    std::uint64_t nonce = 17U,
    std::optional<DirectedCategory> category = std::nullopt) noexcept {
    CraftRequest request{};
    request.root_seed = seed;
    request.operation_nonce = nonce;
    request.item = item;
    request.material = material;
    request.directed_category = category;
    return craft_item(request);
}

bool same_item(const ItemInstance& left, const ItemInstance& right) noexcept {
    return std::memcmp(&left, &right, sizeof(ItemInstance)) == 0;
}

arpg::test::Failure material_catalog_has_fourteen_stable_entries() noexcept {
    constexpr std::array<MaterialId, kMaterialCount> expected{{
        MaterialId::transmute, MaterialId::augment, MaterialId::regal,
        MaterialId::chaos, MaterialId::exalt, MaterialId::annul,
        MaterialId::divine, MaterialId::scour, MaterialId::directed,
        MaterialId::reinforcement_stone, MaterialId::coupon_6,
        MaterialId::coupon_9, MaterialId::coupon_12, MaterialId::coupon_15}};
    ARPG_REQUIRE(validate_material_catalog());
    for (std::size_t index = 0U; index < expected.size(); ++index) {
        ARPG_REQUIRE(material_index(expected[index]) == index);
        const MaterialDefinition* definition = material_definition(expected[index]);
        ARPG_REQUIRE(definition != nullptr);
        ARPG_REQUIRE(definition->stable_id == index + 1U);
        ARPG_REQUIRE(!definition->name.empty());
    }
    ARPG_REQUIRE(material_definition(MaterialId::count) == nullptr);
    ARPG_REQUIRE(material_is_crafting_currency(MaterialId::transmute));
    ARPG_REQUIRE(material_is_crafting_currency(MaterialId::directed));
    ARPG_REQUIRE(!material_is_crafting_currency(MaterialId::reinforcement_stone));
    ARPG_REQUIRE(material_is_coupon(MaterialId::coupon_6));
    ARPG_REQUIRE(material_is_coupon(MaterialId::coupon_15));
    ARPG_REQUIRE(!material_is_coupon(MaterialId::divine));
    return {};
}

arpg::test::Failure reinforcement_success_uses_exact_basis_points() noexcept {
    ARPG_REQUIRE(reinforcement_success_chance_bp(0U) == 0U);
    ARPG_REQUIRE(reinforcement_success_chance_bp(1U) == 10000U);
    ARPG_REQUIRE(reinforcement_success_chance_bp(3U) == 10000U);
    ARPG_REQUIRE(reinforcement_success_chance_bp(4U) == 9000U);
    ARPG_REQUIRE(reinforcement_success_chance_bp(5U) == 8000U);
    ARPG_REQUIRE(reinforcement_success_chance_bp(6U) == 7000U);
    ARPG_REQUIRE(reinforcement_success_chance_bp(12U) == 7000U);
    ARPG_REQUIRE(reinforcement_success_chance_bp(13U) == 6000U);
    ARPG_REQUIRE(reinforcement_success_chance_bp(14U) == 5700U);
    ARPG_REQUIRE(reinforcement_success_chance_bp(15U) == 5415U);
    ARPG_REQUIRE(reinforcement_success_chance_bp(16U) == 5144U);
    ARPG_REQUIRE(reinforcement_success_chance_bp(
        (std::numeric_limits<std::uint32_t>::max)()) == 10U);
    return {};
}

arpg::test::Failure reinforcement_failure_and_preview_use_current_level() noexcept {
    ARPG_REQUIRE(reinforcement_failure(0U) == ReinforcementFailure::unchanged);
    ARPG_REQUIRE(reinforcement_failure(6U) == ReinforcementFailure::unchanged);
    ARPG_REQUIRE(reinforcement_failure(7U) == ReinforcementFailure::reset_six);
    ARPG_REQUIRE(reinforcement_failure(9U) == ReinforcementFailure::reset_six);
    ARPG_REQUIRE(reinforcement_failure(10U) == ReinforcementFailure::reset_zero);
    ARPG_REQUIRE(reinforcement_failure(11U) == ReinforcementFailure::reset_zero);
    ARPG_REQUIRE(reinforcement_failure(12U) == ReinforcementFailure::destroy);
    const ReinforcementPreview preview = reinforcement_preview(12U);
    ARPG_REQUIRE(preview.current == 12U);
    ARPG_REQUIRE(preview.target == 13U);
    ARPG_REQUIRE(preview.success_chance_bp == 6000U);
    ARPG_REQUIRE(preview.failure == ReinforcementFailure::destroy);
    ARPG_REQUIRE(preview.can_attempt);
    const ReinforcementPreview maximum = reinforcement_preview(
        (std::numeric_limits<std::uint32_t>::max)());
    ARPG_REQUIRE(!maximum.can_attempt);
    return {};
}

arpg::test::Failure reinforcement_values_round_each_increment_half_up() noexcept {
    ARPG_REQUIRE(reinforced_base_value(10, 0U) == 10);
    ARPG_REQUIRE(reinforced_base_value(10, 12U) == 22);
    ARPG_REQUIRE(reinforced_base_value(10, 13U) == 24);
    ARPG_REQUIRE(reinforced_base_value(10, 14U) == 26);
    ARPG_REQUIRE(reinforced_base_value(10, 15U) == 29);
    ARPG_REQUIRE(reinforced_base_value(-10, 13U) == -24);
    ARPG_REQUIRE(reinforced_accessory_bonus_bp(0U) == 0);
    ARPG_REQUIRE(reinforced_accessory_bonus_bp(12U) == 600);
    ARPG_REQUIRE(reinforced_accessory_bonus_bp(13U) == 675);
    ARPG_REQUIRE(reinforced_accessory_bonus_bp(14U) == 788);
    ARPG_REQUIRE(reinforced_accessory_bonus_bp(15U) == 957);
    return {};
}

arpg::test::Failure reinforcement_values_saturate_without_wrapping() noexcept {
    const auto maximum = (std::numeric_limits<std::int64_t>::max)();
    const auto minimum = (std::numeric_limits<std::int64_t>::min)();
    ARPG_REQUIRE(reinforced_base_value(maximum, 1U) == maximum);
    ARPG_REQUIRE(reinforced_base_value(minimum, 1U) == minimum);
    ARPG_REQUIRE(reinforced_base_value(1, 104U) == 4759015155419074LL);
    ARPG_REQUIRE(reinforced_base_value(1, 120U) == 3125915491181891220LL);
    ARPG_REQUIRE(reinforced_base_value(1, 125U) == maximum);
    ARPG_REQUIRE(reinforced_base_value(1, 1000U) == maximum);
    ARPG_REQUIRE(reinforced_base_value(-1, 1000U) == minimum);
    ARPG_REQUIRE(reinforced_base_value(0, 1000U) == 0);
    ARPG_REQUIRE(reinforced_accessory_bonus_bp(1000U)
        == (std::numeric_limits<std::int32_t>::max)());
    return {};
}

arpg::test::Failure coupons_only_raise_valid_items_to_the_face_value() noexcept {
    ItemInstance item = generated_with_count(
        ItemSlot::weapon, ItemRarity::normal, 0U);
    ARPG_REQUIRE(item.id != 0U);
    item.reinforcement = 8U;
    const auto plus_twelve = apply_coupon(item, MaterialId::coupon_12);
    ARPG_REQUIRE(plus_twelve.has_value());
    ARPG_REQUIRE(plus_twelve->reinforcement == 12U);
    ARPG_REQUIRE(plus_twelve->id == item.id);
    ARPG_REQUIRE(!apply_coupon(*plus_twelve, MaterialId::coupon_12).has_value());
    ARPG_REQUIRE(!apply_coupon(item, MaterialId::chaos).has_value());
    item.id = 0U;
    ARPG_REQUIRE(!apply_coupon(item, MaterialId::coupon_15).has_value());
    return {};
}

arpg::test::Failure transmute_augment_and_regal_follow_rarity_contracts() noexcept {
    const ItemInstance normal = generated_with_count(
        ItemSlot::weapon, ItemRarity::normal, 0U);
    const CraftResult transmuted = craft(MaterialId::transmute, normal);
    ARPG_REQUIRE(transmuted.applied && transmuted.consumed);
    ARPG_REQUIRE(transmuted.item.rarity == ItemRarity::magic);
    ARPG_REQUIRE(transmuted.item.affix_count >= 1U
        && transmuted.item.affix_count <= 2U);
    ARPG_REQUIRE(validate_item(transmuted.item));
    ARPG_REQUIRE(transmuted.item.id == normal.id);

    const ItemInstance one_affix = generated_with_count(
        ItemSlot::gloves, ItemRarity::magic, 1U);
    const CraftResult augmented = craft(MaterialId::augment, one_affix);
    ARPG_REQUIRE(augmented.applied && augmented.item.affix_count == 2U);
    ARPG_REQUIRE(validate_item(augmented.item));
    const CraftResult regal = craft(MaterialId::regal, augmented.item);
    ARPG_REQUIRE(regal.applied && regal.item.rarity == ItemRarity::rare);
    ARPG_REQUIRE(regal.item.affix_count == 3U);
    ARPG_REQUIRE(validate_item(regal.item));
    return {};
}

arpg::test::Failure chaos_and_exalt_follow_rare_contracts() noexcept {
    const ItemInstance rare_three = generated_with_count(
        ItemSlot::chest, ItemRarity::rare, 3U);
    const CraftResult chaos = craft(MaterialId::chaos, rare_three);
    ARPG_REQUIRE(chaos.applied);
    ARPG_REQUIRE(chaos.item.rarity == ItemRarity::rare);
    ARPG_REQUIRE(chaos.item.affix_count >= 3U && chaos.item.affix_count <= 6U);
    ARPG_REQUIRE(validate_item(chaos.item));
    ARPG_REQUIRE(!same_item(chaos.item, rare_three));

    const CraftResult exalt = craft(MaterialId::exalt, rare_three);
    ARPG_REQUIRE(exalt.applied && exalt.item.affix_count == 4U);
    ARPG_REQUIRE(validate_item(exalt.item));
    const ItemInstance rare_six = generated_with_count(
        ItemSlot::chest, ItemRarity::rare, 6U);
    const CraftResult rejected = craft(MaterialId::exalt, rare_six);
    ARPG_REQUIRE(!rejected.applied && !rejected.consumed);
    ARPG_REQUIRE(same_item(rejected.item, rare_six));
    return {};
}

arpg::test::Failure annul_downgrades_rarity_and_scour_clears_affixes() noexcept {
    const ItemInstance one_affix = generated_with_count(
        ItemSlot::boots, ItemRarity::magic, 1U);
    const CraftResult annulled_magic = craft(MaterialId::annul, one_affix);
    ARPG_REQUIRE(annulled_magic.applied);
    ARPG_REQUIRE(annulled_magic.item.rarity == ItemRarity::normal);
    ARPG_REQUIRE(annulled_magic.item.affix_count == 0U);
    ARPG_REQUIRE(annulled_magic.item.required_level == 1U);

    const ItemInstance rare_three = generated_with_count(
        ItemSlot::accessory, ItemRarity::rare, 3U);
    const CraftResult annulled_rare = craft(MaterialId::annul, rare_three);
    ARPG_REQUIRE(annulled_rare.applied);
    ARPG_REQUIRE(annulled_rare.item.rarity == ItemRarity::magic);
    ARPG_REQUIRE(annulled_rare.item.affix_count == 2U);
    ARPG_REQUIRE(validate_item(annulled_rare.item));

    ItemInstance reinforced = rare_three;
    reinforced.reinforcement = 19U;
    const CraftResult scoured = craft(MaterialId::scour, reinforced);
    ARPG_REQUIRE(scoured.applied && scoured.item.rarity == ItemRarity::normal);
    ARPG_REQUIRE(scoured.item.affix_count == 0U);
    ARPG_REQUIRE(scoured.item.reinforcement == 19U);
    ARPG_REQUIRE(validate_item(scoured.item));
    return {};
}

arpg::test::Failure divine_preserves_affix_identity_tier_and_variant() noexcept {
    const ItemInstance rare = generated_with_count(
        ItemSlot::accessory, ItemRarity::rare, 5U);
    const CraftResult divine = craft(MaterialId::divine, rare);
    ARPG_REQUIRE(divine.applied && divine.consumed);
    ARPG_REQUIRE(divine.item.id == rare.id);
    ARPG_REQUIRE(divine.item.rarity == rare.rarity);
    ARPG_REQUIRE(divine.item.affix_count == rare.affix_count);
    bool changed_effect = false;
    for (std::size_t index = 0U; index < rare.affix_count; ++index) {
        const AffixRoll& before = rare.affixes[index];
        const AffixRoll& after = divine.item.affixes[index];
        ARPG_REQUIRE(after.affix_id == before.affix_id);
        ARPG_REQUIRE(after.tier == before.tier);
        ARPG_REQUIRE(after.variant == before.variant);
        ARPG_REQUIRE(after.value_roll_bp >= kAffixValueRollMinimumBp);
        ARPG_REQUIRE(after.value_roll_bp <= kAffixValueRollMaximumBp);
        ARPG_REQUIRE(after.value_roll_bp != before.value_roll_bp);
        const auto before_value = affix_roll_value(before);
        const auto after_value = affix_roll_value(after);
        ARPG_REQUIRE(before_value.has_value() && after_value.has_value());
        changed_effect = changed_effect || *before_value != *after_value;
    }
    ARPG_REQUIRE(changed_effect);
    return {};
}

arpg::test::Failure divine_rejects_when_integer_effect_cannot_change() noexcept {
    ItemInstance item{};
    item.id = 0xD171EULL;
    item.base_id = 1U;
    item.rarity = ItemRarity::magic;
    item.item_level = 1U;
    item.required_level = 1U;
    item.affixes[0] = {1U, 8U, 0xFFU};
    item.affix_count = 1U;
    ARPG_REQUIRE(validate_item(item));
    ARPG_REQUIRE(affix_roll_value(item.affixes[0]) == 1);

    const CraftResult result = craft(MaterialId::divine, item);
    ARPG_REQUIRE(!result.applied);
    ARPG_REQUIRE(!result.consumed);
    ARPG_REQUIRE(same_item(result.item, item));
    return {};
}

arpg::test::Failure directed_replacement_preserves_count_and_is_compatible() noexcept {
    const ItemInstance rare = generated_with_count(
        ItemSlot::weapon, ItemRarity::rare, 4U);
    const CraftResult directed = craft(MaterialId::directed, rare,
        0xA55AA55AA55AA55AULL, 91U, DirectedCategory::damage);
    ARPG_REQUIRE(directed.applied);
    ARPG_REQUIRE(directed.item.rarity == rare.rarity);
    ARPG_REQUIRE(directed.item.affix_count == rare.affix_count);
    ARPG_REQUIRE(validate_item(directed.item));
    ARPG_REQUIRE(!same_item(directed.item, rare));
    std::size_t changed_count = 0U;
    for (std::size_t index = 0U; index < rare.affix_count; ++index) {
        if (std::memcmp(&directed.item.affixes[index], &rare.affixes[index],
                sizeof(AffixRoll)) != 0) {
            ++changed_count;
            const AffixDefinition* before =
                affix_definition(rare.affixes[index].affix_id);
            const AffixDefinition* after =
                affix_definition(directed.item.affixes[index].affix_id);
            ARPG_REQUIRE(before != nullptr && after != nullptr);
            ARPG_REQUIRE(before->group_id != after->group_id);
        }
    }
    ARPG_REQUIRE(changed_count == 1U);
    bool contains_damage_affix = false;
    for (std::size_t index = 0U; index < directed.item.affix_count; ++index) {
        const std::uint16_t id = directed.item.affixes[index].affix_id;
        contains_damage_affix = contains_damage_affix || id == 1U || id == 2U
            || id == 105U || id == 106U;
    }
    ARPG_REQUIRE(contains_damage_affix);
    ARPG_REQUIRE(!craft(MaterialId::directed, rare).applied);

    std::size_t successful_replays = 0U;
    for (std::uint64_t seed = 1U; seed <= 512U; ++seed) {
        const CraftResult replay = craft(MaterialId::directed, rare,
            seed, 91U, DirectedCategory::damage);
        if (!replay.applied) continue;
        ++successful_replays;
        std::size_t changed = 0U;
        for (std::size_t index = 0U; index < rare.affix_count; ++index) {
            if (std::memcmp(&replay.item.affixes[index], &rare.affixes[index],
                    sizeof(AffixRoll)) == 0) {
                continue;
            }
            ++changed;
            const AffixDefinition* before =
                affix_definition(rare.affixes[index].affix_id);
            const AffixDefinition* after =
                affix_definition(replay.item.affixes[index].affix_id);
            ARPG_REQUIRE(before != nullptr && after != nullptr);
            ARPG_REQUIRE(before->group_id != after->group_id);
        }
        ARPG_REQUIRE(changed == 1U);
    }
    ARPG_REQUIRE(successful_replays == 512U);

    constexpr std::array<std::uint16_t, 24> all_ids{{
        1U, 2U, 3U, 4U, 5U, 6U, 7U, 8U, 9U, 10U, 11U, 12U,
        101U, 102U, 103U, 104U, 105U, 106U, 107U, 108U, 109U,
        110U, 111U, 112U}};
    for (const std::uint16_t id : all_ids) {
        const bool damage = id == 1U || id == 2U || id == 105U || id == 106U;
        const bool defense = id == 11U || id == 12U || id == 103U
            || id == 104U || id == 111U;
        const bool speed = id == 101U || id == 102U;
        const bool element = (id >= 3U && id <= 10U)
            || (id >= 107U && id <= 110U) || id == 112U;
        ARPG_REQUIRE(affix_in_directed_category(
            DirectedCategory::damage, id) == damage);
        ARPG_REQUIRE(affix_in_directed_category(
            DirectedCategory::defense, id) == defense);
        ARPG_REQUIRE(affix_in_directed_category(
            DirectedCategory::speed, id) == speed);
        ARPG_REQUIRE(affix_in_directed_category(
            DirectedCategory::element, id) == element);
    }
    return {};
}

arpg::test::Failure crafting_rejects_illegal_targets_and_replays_bytes() noexcept {
    ItemInstance rare = generated_with_count(
        ItemSlot::helmet, ItemRarity::rare, 4U);
    rare.reinforcement = 27U;
    const CraftResult first = craft(MaterialId::chaos, rare, 123U, 456U);
    const CraftResult second = craft(MaterialId::chaos, rare, 123U, 456U);
    ARPG_REQUIRE(first.applied && second.applied);
    ARPG_REQUIRE(same_item(first.item, second.item));
    ARPG_REQUIRE(first.item.id == rare.id);
    ARPG_REQUIRE(first.item.base_id == rare.base_id);
    ARPG_REQUIRE(first.item.item_level == rare.item_level);
    ARPG_REQUIRE(first.item.reinforcement == rare.reinforcement);

    const ItemInstance normal = generated_with_count(
        ItemSlot::helmet, ItemRarity::normal, 0U);
    const ItemInstance magic_one = generated_with_count(
        ItemSlot::helmet, ItemRarity::magic, 1U);
    const ItemInstance magic_two = generated_with_count(
        ItemSlot::helmet, ItemRarity::magic, 2U);
    struct ReplayCase final {
        MaterialId material;
        ItemInstance item;
        std::optional<DirectedCategory> category;
    };
    const std::array<ReplayCase, 9> replay_cases{{
        {MaterialId::transmute, normal, std::nullopt},
        {MaterialId::augment, magic_one, std::nullopt},
        {MaterialId::regal, magic_two, std::nullopt},
        {MaterialId::chaos, rare, std::nullopt},
        {MaterialId::exalt, rare, std::nullopt},
        {MaterialId::annul, rare, std::nullopt},
        {MaterialId::divine, rare, std::nullopt},
        {MaterialId::scour, rare, std::nullopt},
        {MaterialId::directed, rare, DirectedCategory::defense},
    }};
    for (const ReplayCase& replay_case : replay_cases) {
        const CraftResult replay_first = craft(replay_case.material,
            replay_case.item, 909U, 707U, replay_case.category);
        const CraftResult replay_second = craft(replay_case.material,
            replay_case.item, 909U, 707U, replay_case.category);
        ARPG_REQUIRE(replay_first.applied && replay_second.applied);
        ARPG_REQUIRE(same_item(replay_first.item, replay_second.item));
    }

    const CraftResult wrong_currency = craft(MaterialId::augment, rare);
    ARPG_REQUIRE(!wrong_currency.applied && !wrong_currency.consumed);
    ARPG_REQUIRE(same_item(wrong_currency.item, rare));
    const CraftResult stone = craft(MaterialId::reinforcement_stone, rare);
    ARPG_REQUIRE(!stone.applied && !stone.consumed);
    ARPG_REQUIRE(same_item(stone.item, rare));
    ARPG_REQUIRE(!craft(MaterialId::transmute, magic_one).applied);
    ARPG_REQUIRE(!craft(MaterialId::augment, magic_two).applied);
    ARPG_REQUIRE(!craft(MaterialId::regal, rare).applied);
    ARPG_REQUIRE(!craft(MaterialId::chaos, magic_two).applied);
    ARPG_REQUIRE(!craft(MaterialId::annul, normal).applied);
    ARPG_REQUIRE(!craft(MaterialId::divine, normal).applied);
    ARPG_REQUIRE(!craft(MaterialId::scour, normal).applied);
    ARPG_REQUIRE(!craft(MaterialId::directed, normal,
        1U, 1U, DirectedCategory::defense).applied);
    rare.id = 0U;
    ARPG_REQUIRE(!craft(MaterialId::chaos, rare).applied);
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"stable material catalog", &material_catalog_has_fourteen_stable_entries},
    {"reinforcement success basis points",
        &reinforcement_success_uses_exact_basis_points},
    {"reinforcement failure preview",
        &reinforcement_failure_and_preview_use_current_level},
    {"reinforcement value rounding",
        &reinforcement_values_round_each_increment_half_up},
    {"reinforcement saturation",
        &reinforcement_values_saturate_without_wrapping},
    {"coupon face values", &coupons_only_raise_valid_items_to_the_face_value},
    {"transmute augment regal",
        &transmute_augment_and_regal_follow_rarity_contracts},
    {"chaos and exalt", &chaos_and_exalt_follow_rare_contracts},
    {"annul and scour", &annul_downgrades_rarity_and_scour_clears_affixes},
    {"divine identity", &divine_preserves_affix_identity_tier_and_variant},
    {"divine rejects rounded no effect",
        &divine_rejects_when_integer_effect_cannot_change},
    {"directed replacement",
        &directed_replacement_preserves_count_and_is_compatible},
    {"craft legality and replay",
        &crafting_rejects_illegal_targets_and_replays_bytes},
};

}  // namespace

arpg::test::TestSuite item_crafting_suite() noexcept {
    return arpg::test::make_suite("item_crafting", kCases);
}
