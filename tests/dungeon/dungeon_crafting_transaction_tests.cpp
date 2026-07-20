#include "test_framework.hpp"

#include "dungeon/dungeon_progression.hpp"
#include "dungeon/dungeon_session.hpp"
#include "items/item_catalog.hpp"
#include "items/item_generation.hpp"
#include "items/material_catalog.hpp"

#include <array>
#include <cstdint>
#include <initializer_list>

namespace {

using arpg::dungeon::DungeonRules;
using arpg::dungeon::DungeonRunState;
using arpg::dungeon::DungeonSession;
using arpg::dungeon::PendingSaveKind;
using arpg::dungeon::RequestResult;
using arpg::dungeon::RoomPhase;
using arpg::dungeon::SaveDisposition;
using arpg::items::ItemInstance;
using arpg::items::ItemRarity;
using arpg::items::ItemSlot;
using arpg::items::MaterialId;

ItemInstance generated_item(std::uint64_t id, std::uint8_t base_id,
    ItemRarity rarity, std::uint8_t level = 100U) noexcept {
    const arpg::items::BaseDefinition* const base =
        arpg::items::base_definition(base_id);
    if (base == nullptr) return {};
    for (std::uint64_t seed = 1U; seed <= 4096U; ++seed) {
        const auto item = arpg::items::generate_item(
            {seed, base->slot, level, id, rarity});
        if (item.has_value() && item->base_id == base_id) return *item;
    }
    return {};
}

DungeonRunState state_with_items(std::initializer_list<ItemInstance> items) {
    DungeonRunState state = arpg::dungeon::make_initial_run_state(
        0xC0FFEE16ULL, DungeonRules{}).state;
    state.item_ownership.items.assign(items.begin(), items.end());
    state.item_ownership.next_item_sequence = 17U;
    return state;
}

const ItemInstance* find_item(const arpg::items::ItemOwnershipState& ownership,
    std::uint64_t id) noexcept {
    for (const ItemInstance& item : ownership.items)
        if (item.id == id) return &item;
    return nullptr;
}

bool commit_pending(DungeonSession& session) noexcept {
    const auto pending = session.pending_save();
    if (!pending.has_value()) return false;
    session.resolve_pending_save({SaveDisposition::committed,
        pending->expected_generation, pending->next_state});
    return session.snapshot().phase != RoomPhase::faulted;
}

arpg::test::Failure crafting_spends_only_after_a_successful_pure_craft() noexcept {
    const ItemInstance rare = generated_item(101U, 1U, ItemRarity::rare);
    ARPG_REQUIRE(arpg::items::validate_item(rare));
    DungeonRunState state = state_with_items({rare});
    const std::size_t chaos = arpg::items::material_index(MaterialId::chaos);
    const std::size_t augment = arpg::items::material_index(MaterialId::augment);
    state.item_ownership.materials[chaos] = 1U;
    state.item_ownership.materials[augment] = 1U;
    DungeonSession session{DungeonRules{}, state};

    ARPG_REQUIRE(session.request_craft(MaterialId::chaos, rare.id)
        == RequestResult::accepted);
    const auto pending = session.pending_save();
    ARPG_REQUIRE(pending.has_value());
    ARPG_REQUIRE(pending->kind == PendingSaveKind::craft);
    ARPG_REQUIRE(pending->next_state.item_ownership.materials[chaos] == 0U);
    const ItemInstance* const crafted = find_item(
        pending->next_state.item_ownership, rare.id);
    ARPG_REQUIRE(crafted != nullptr);
    ARPG_REQUIRE(crafted->id == rare.id);
    ARPG_REQUIRE(session.item_state().materials[chaos] == 1U);

    session.resolve_pending_save({SaveDisposition::not_committed,
        pending->expected_generation, pending->next_state});
    ARPG_REQUIRE(session.item_state().materials[chaos] == 1U);
    ARPG_REQUIRE(find_item(session.item_state(), rare.id) != nullptr);
    ARPG_REQUIRE(session.request_craft(MaterialId::augment, rare.id)
        == RequestResult::rejected);
    ARPG_REQUIRE(session.item_state().materials[augment] == 1U);
    ARPG_REQUIRE(!session.pending_save().has_value());
    return {};
}

arpg::test::Failure recipe_requires_exact_base_and_resets_reinforcement() noexcept {
    ItemInstance a = generated_item(201U, 2U, ItemRarity::normal, 60U);
    ItemInstance b = generated_item(202U, 2U, ItemRarity::normal, 61U);
    ItemInstance c = generated_item(203U, 2U, ItemRarity::normal, 62U);
    ItemInstance wrong_base = generated_item(204U, 9U, ItemRarity::normal, 63U);
    ARPG_REQUIRE(arpg::items::validate_item(a));
    ARPG_REQUIRE(arpg::items::validate_item(b));
    ARPG_REQUIRE(arpg::items::validate_item(c));
    ARPG_REQUIRE(arpg::items::validate_item(wrong_base));
    a.reinforcement = 7U;
    b.reinforcement = 4U;
    DungeonRunState state = state_with_items({a, b, c, wrong_base});
    DungeonSession session{DungeonRules{}, state};

    ARPG_REQUIRE(session.request_recipe({{a.id, b.id, wrong_base.id}})
        == RequestResult::rejected);
    ARPG_REQUIRE(session.request_recipe({{a.id, b.id, c.id}})
        == RequestResult::accepted);
    const auto pending = session.pending_save();
    ARPG_REQUIRE(pending.has_value());
    ARPG_REQUIRE(pending->kind == PendingSaveKind::recipe);
    ARPG_REQUIRE(pending->next_state.item_ownership.items.size() == 2U);
    const ItemInstance& product = pending->next_state.item_ownership.items[1];
    ARPG_REQUIRE(product.base_id == 2U);
    ARPG_REQUIRE(product.rarity == ItemRarity::normal);
    ARPG_REQUIRE(product.item_level == 61U);
    ARPG_REQUIRE(product.reinforcement == 0U);
    ARPG_REQUIRE(commit_pending(session));
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"craft spends only after pure success", &crafting_spends_only_after_a_successful_pure_craft},
    {"recipe exact base resets reinforcement", &recipe_requires_exact_base_and_resets_reinforcement},
};

}  // namespace

arpg::test::TestSuite dungeon_crafting_transaction_suite() noexcept {
    return arpg::test::make_suite("dungeon_crafting_transaction", kCases);
}
