#include "test_framework.hpp"

#include "items/item_catalog.hpp"
#include "items/item_generation.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace {

using namespace arpg::items;

ItemInstance generated_material(std::uint64_t seed,
    std::uint64_t id,
    ItemSlot slot,
    std::uint8_t level,
    ItemRarity rarity) noexcept {
    return generate_item({seed, slot, level, id, rarity}).value();
}

bool same_affixes(const ItemInstance& left,
    const ItemInstance& right) noexcept {
    return std::memcmp(left.affixes.data(), right.affixes.data(),
        sizeof(left.affixes)) == 0;
}

arpg::test::Failure recipe_is_order_independent_and_averages_level() noexcept {
    const ItemInstance a = generated_material(
        11U, 300U, ItemSlot::accessory, 95U, ItemRarity::rare);
    const ItemInstance b = generated_material(
        22U, 100U, ItemSlot::accessory, 96U, ItemRarity::rare);
    const ItemInstance c = generated_material(
        33U, 200U, ItemSlot::accessory, 100U, ItemRarity::rare);
    const auto abc = generate_recipe_item(0xABCDEFULL, 9U, a, b, c);
    const auto cba = generate_recipe_item(0xABCDEFULL, 9U, c, b, a);
    const auto bac = generate_recipe_item(0xABCDEFULL, 9U, b, a, c);
    ARPG_REQUIRE(abc.has_value());
    ARPG_REQUIRE(cba.has_value());
    ARPG_REQUIRE(bac.has_value());
    ARPG_REQUIRE(std::memcmp(&*abc, &*cba, sizeof(ItemInstance)) == 0);
    ARPG_REQUIRE(std::memcmp(&*abc, &*bac, sizeof(ItemInstance)) == 0);
    ARPG_REQUIRE(abc->id != 0U);
    const auto accessory_bases = base_ids_for_slot(ItemSlot::accessory);
    ARPG_REQUIRE(std::find(accessory_bases.begin(), accessory_bases.end(),
        abc->base_id) != accessory_bases.end());
    ARPG_REQUIRE(abc->rarity == ItemRarity::rare);
    ARPG_REQUIRE(abc->item_level == 97U);
    ARPG_REQUIRE(validate_item(*abc));
    return {};
}

arpg::test::Failure recipe_fully_rerolls_magic_and_rare_affixes() noexcept {
    const ItemInstance rare_a = generated_material(
        101U, 41U, ItemSlot::weapon, 100U, ItemRarity::rare);
    const ItemInstance rare_b = generated_material(
        202U, 42U, ItemSlot::weapon, 100U, ItemRarity::rare);
    const ItemInstance rare_c = generated_material(
        303U, 43U, ItemSlot::weapon, 100U, ItemRarity::rare);
    const auto rare = generate_recipe_item(88U, 3U,
        rare_a, rare_b, rare_c);
    ARPG_REQUIRE(rare.has_value());
    ARPG_REQUIRE(rare->affix_count >= 3U && rare->affix_count <= 6U);
    ARPG_REQUIRE(!same_affixes(*rare, rare_a));
    ARPG_REQUIRE(!same_affixes(*rare, rare_b));
    ARPG_REQUIRE(!same_affixes(*rare, rare_c));

    const ItemInstance magic_a = generated_material(
        404U, 51U, ItemSlot::gloves, 75U, ItemRarity::magic);
    const ItemInstance magic_b = generated_material(
        505U, 52U, ItemSlot::gloves, 88U, ItemRarity::magic);
    const ItemInstance magic_c = generated_material(
        606U, 53U, ItemSlot::gloves, 95U, ItemRarity::magic);
    const auto magic = generate_recipe_item(99U, 4U,
        magic_a, magic_b, magic_c);
    ARPG_REQUIRE(magic.has_value());
    ARPG_REQUIRE(magic->item_level == 86U);
    ARPG_REQUIRE(magic->affix_count >= 1U && magic->affix_count <= 2U);
    ARPG_REQUIRE(validate_item(*magic));

    const ItemInstance normal_a = generated_material(
        1U, 61U, ItemSlot::boots, 1U, ItemRarity::normal);
    const ItemInstance normal_b = generated_material(
        2U, 62U, ItemSlot::boots, 16U, ItemRarity::normal);
    const ItemInstance normal_c = generated_material(
        3U, 63U, ItemSlot::boots, 30U, ItemRarity::normal);
    const auto normal = generate_recipe_item(100U, 5U,
        normal_a, normal_b, normal_c);
    ARPG_REQUIRE(normal.has_value());
    ARPG_REQUIRE(normal->item_level == 15U);
    ARPG_REQUIRE(normal->affix_count == 0U);
    ARPG_REQUIRE(normal->required_level == 1U);
    return {};
}

arpg::test::Failure recipe_rejects_invalid_material_contracts() noexcept {
    const ItemInstance a = generated_material(
        1U, 71U, ItemSlot::helmet, 100U, ItemRarity::magic);
    const ItemInstance b = generated_material(
        2U, 72U, ItemSlot::helmet, 100U, ItemRarity::magic);
    const ItemInstance c = generated_material(
        3U, 73U, ItemSlot::helmet, 100U, ItemRarity::magic);
    ARPG_REQUIRE(!generate_recipe_item(1U, 0U, a, b, c).has_value());

    ItemInstance invalid = a;
    invalid.required_level = 99U;
    ARPG_REQUIRE(!generate_recipe_item(1U, 1U, invalid, b, c).has_value());
    invalid = a;
    invalid.id = 0U;
    ARPG_REQUIRE(!generate_recipe_item(1U, 1U, invalid, b, c).has_value());
    invalid = b;
    invalid.id = a.id;
    ARPG_REQUIRE(!generate_recipe_item(1U, 1U, a, invalid, c).has_value());
    invalid = generated_material(
        4U, 74U, ItemSlot::chest, 100U, ItemRarity::magic);
    ARPG_REQUIRE(!generate_recipe_item(1U, 1U, a, b, invalid).has_value());
    invalid = generated_material(
        5U, 75U, ItemSlot::helmet, 100U, ItemRarity::rare);
    ARPG_REQUIRE(!generate_recipe_item(1U, 1U, a, b, invalid).has_value());
    return {};
}

arpg::test::Failure recipe_rejects_output_id_collision() noexcept {
    ItemInstance a = generated_material(
        1U, 81U, ItemSlot::boots, 30U, ItemRarity::normal);
    const ItemInstance b = generated_material(
        2U, 82U, ItemSlot::boots, 30U, ItemRarity::normal);
    const ItemInstance c = generated_material(
        3U, 83U, ItemSlot::boots, 30U, ItemRarity::normal);
    std::optional<ItemInstance> baseline{};
    std::uint64_t root_seed = 1U;
    for (; root_seed <= 1024U && !baseline.has_value(); ++root_seed)
        baseline = generate_recipe_item(root_seed, 7U, a, b, c);
    ARPG_REQUIRE(baseline.has_value());
    a.id = baseline->id;
    ARPG_REQUIRE(!generate_recipe_item(
        root_seed - 1U, 7U, a, b, c).has_value());
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"order-independent average recipe",
        &recipe_is_order_independent_and_averages_level},
    {"complete affix reroll", &recipe_fully_rerolls_magic_and_rare_affixes},
    {"invalid recipe contracts", &recipe_rejects_invalid_material_contracts},
    {"output id collision", &recipe_rejects_output_id_collision},
};

}  // namespace

arpg::test::TestSuite item_recipe_suite() noexcept {
    return arpg::test::make_suite("item_recipe", kCases);
}
