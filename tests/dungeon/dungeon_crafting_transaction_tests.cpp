#include "test_framework.hpp"

#include "dungeon/dungeon_progression.hpp"
#include "dungeon/dungeon_session.hpp"
#include "dungeon_test_support.hpp"
#include "items/item_catalog.hpp"
#include "items/item_generation.hpp"
#include "items/material_catalog.hpp"

#include <array>
#include <cstdint>
#include <initializer_list>
#include <limits>

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

arpg::test::Failure reinforcement_failure_bands_apply_through_saved_transactions() noexcept {
    const ItemInstance source = generated_item(301U, 1U, ItemRarity::normal);
    ARPG_REQUIRE(arpg::items::validate_item(source));
    const std::size_t stone = arpg::items::material_index(
        MaterialId::reinforcement_stone);

    const auto exercise_failure = [&](ItemInstance source_item,
        std::uint32_t current,
        std::uint32_t expected, bool expect_destroy) noexcept {
        for (std::uint64_t seed = 1U; seed <= 256U; ++seed) {
            ItemInstance item = source_item;
            item.reinforcement = current;
            DungeonRunState state = state_with_items({item});
            state.root_seed = seed;
            state.item_ownership.materials[stone] = 1U;
            if (expect_destroy) {
                const auto* const base = arpg::items::base_definition(item.base_id);
                if (base == nullptr) return false;
                state.item_ownership.equipment.equipped_ids[
                    static_cast<std::size_t>(base->slot)] = item.id;
            }
            DungeonSession session{DungeonRules{}, state};
            if (expect_destroy && (!session.snapshot().combat.has_value()
                    || session.snapshot().combat->player.armor == 0)) {
                return false;
            }
            if (session.request_reinforcement(item.id)
                    != RequestResult::accepted) {
                continue;
            }
            const auto pending = session.pending_save();
            if (!pending.has_value()) return false;
            const ItemInstance* const after = find_item(
                pending->next_state.item_ownership, item.id);
            if (expect_destroy ? after != nullptr
                    : after == nullptr || after->reinforcement != expected) {
                continue;
            }
            if (expect_destroy && pending->next_state.item_ownership.equipment
                    .equipped_ids[static_cast<std::size_t>(
                        arpg::items::base_definition(item.base_id)->slot)] != 0U) {
                return false;
            }
            if (pending->next_state.item_ownership.materials[stone] != 0U
                    || pending->kind != PendingSaveKind::reinforcement
                    || !commit_pending(session)) {
                return false;
            }
            const ItemInstance* const committed = find_item(
                session.item_state(), item.id);
            return expect_destroy ? committed == nullptr
                    && session.snapshot().combat.has_value()
                    && session.snapshot().combat->player.armor == 0
                                  : committed != nullptr
                && committed->reinforcement == expected;
        }
        return false;
    };

    ARPG_REQUIRE(exercise_failure(source, 7U, 6U, false));
    ARPG_REQUIRE(exercise_failure(source, 10U, 0U, false));
    ARPG_REQUIRE(exercise_failure(
        generated_item(302U, 2U, ItemRarity::normal), 12U, 0U, true));
    return {};
}

arpg::test::Failure coupon_fifteen_sets_level_through_atomic_transaction() noexcept {
    ItemInstance item = generated_item(401U, 1U, ItemRarity::normal);
    ARPG_REQUIRE(arpg::items::validate_item(item));
    item.reinforcement = 3U;
    DungeonRunState state = state_with_items({item});
    const std::size_t coupon = arpg::items::material_index(MaterialId::coupon_15);
    const std::size_t transmute = arpg::items::material_index(
        MaterialId::transmute);
    state.item_ownership.materials[coupon] = 1U;
    state.item_ownership.materials[transmute] = 1U;
    DungeonSession session{DungeonRules{}, state};

    ARPG_REQUIRE(session.request_coupon(MaterialId::coupon_15, item.id)
        == RequestResult::accepted);
    const auto pending = session.pending_save();
    ARPG_REQUIRE(pending.has_value());
    ARPG_REQUIRE(pending->kind == PendingSaveKind::reinforcement);
    const ItemInstance* const after = find_item(
        pending->next_state.item_ownership, item.id);
    ARPG_REQUIRE(after != nullptr);
    ARPG_REQUIRE(after->reinforcement == 15U);
    ARPG_REQUIRE(pending->next_state.item_ownership.materials[coupon] == 0U);
    ARPG_REQUIRE(commit_pending(session));
    ARPG_REQUIRE(find_item(session.item_state(), item.id)->reinforcement == 15U);
    ARPG_REQUIRE(session.snapshot().reinforcement_receipt.valid);
    ARPG_REQUIRE(session.request_craft(MaterialId::transmute, item.id)
        == RequestResult::accepted);
    ARPG_REQUIRE(commit_pending(session));
    ARPG_REQUIRE(!session.snapshot().reinforcement_receipt.valid);
    return {};
}

arpg::test::Failure successful_non_item_commit_clears_reinforcement_receipt() noexcept {
    ItemInstance item = generated_item(501U, 1U, ItemRarity::normal);
    ARPG_REQUIRE(arpg::items::validate_item(item));
    DungeonRunState state = state_with_items({item});
    state.progression = {2U, 0U, 1U, 1U};
    const std::size_t coupon = arpg::items::material_index(MaterialId::coupon_15);
    state.item_ownership.materials[coupon] = 1U;
    DungeonSession session{DungeonRules{}, state};

    ARPG_REQUIRE(session.request_coupon(MaterialId::coupon_15, item.id)
        == RequestResult::accepted);
    ARPG_REQUIRE(commit_pending(session));
    ARPG_REQUIRE(session.snapshot().reinforcement_receipt.valid);

    arpg::test::EventSummary events{};
    ARPG_REQUIRE(arpg::test::drive_until_cleared(session, events));
    if (session.snapshot().phase == RoomPhase::cleared) session.tick({});
    ARPG_REQUIRE(session.snapshot().phase == RoomPhase::awaiting_exit);
    ARPG_REQUIRE(session.request_passive_allocation(2U));
    const auto failed = session.pending_save();
    ARPG_REQUIRE(failed.has_value());
    session.resolve_pending_save({SaveDisposition::not_committed,
        failed->expected_generation, failed->next_state});
    ARPG_REQUIRE(session.snapshot().reinforcement_receipt.valid);

    ARPG_REQUIRE(session.request_passive_allocation(2U));
    ARPG_REQUIRE(commit_pending(session));
    ARPG_REQUIRE(!session.snapshot().reinforcement_receipt.valid);
    return {};
}

arpg::test::Failure equipped_extreme_reinforcement_saves_with_saturated_hit_packet() noexcept {
    ItemInstance item = generated_item(601U, 1U, ItemRarity::normal);
    ARPG_REQUIRE(arpg::items::validate_item(item));
    item.reinforcement = (std::numeric_limits<std::uint32_t>::max)();
    DungeonSession session{DungeonRules{}, state_with_items({item})};

    ARPG_REQUIRE(session.request_equip(item.id) == RequestResult::accepted);
    ARPG_REQUIRE(commit_pending(session));
    const auto& build = arpg::test::player_build(session);
    ARPG_REQUIRE(build.weapon_physical
        == (std::numeric_limits<std::int64_t>::max)());
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"craft spends only after pure success", &crafting_spends_only_after_a_successful_pure_craft},
    {"recipe exact base resets reinforcement", &recipe_requires_exact_base_and_resets_reinforcement},
    {"reinforcement failure bands save atomically", &reinforcement_failure_bands_apply_through_saved_transactions},
    {"coupon fifteen saves atomically", &coupon_fifteen_sets_level_through_atomic_transaction},
    {"non-item success clears reinforcement receipt",
        &successful_non_item_commit_clears_reinforcement_receipt},
    {"equipped extreme reinforcement saves with saturation",
        &equipped_extreme_reinforcement_saves_with_saturated_hit_packet},
};

}  // namespace

arpg::test::TestSuite dungeon_crafting_transaction_suite() noexcept {
    return arpg::test::make_suite("dungeon_crafting_transaction", kCases);
}
