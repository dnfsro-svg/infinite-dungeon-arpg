#include "dungeon/dungeon_progression.hpp"
#include "dungeon/dungeon_session.hpp"
#include "dungeon/material_loot.hpp"
#include "dungeon_test_support.hpp"
#include "items/item_catalog.hpp"
#include "items/item_crafting.hpp"
#include "items/item_generation.hpp"
#include "items/material_catalog.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <optional>

namespace {

namespace dungeon = arpg::dungeon;
namespace items = arpg::items;

constexpr std::uint64_t kTrials = 1000000U;

bool require(bool condition, const char* message) noexcept {
    if (!condition) std::cerr << "FAIL: " << message << '\n';
    return condition;
}

bool commit(dungeon::DungeonSession& session) noexcept {
    const auto pending = session.pending_save();
    if (!pending.has_value()) return false;
    session.resolve_pending_save({dungeon::SaveDisposition::committed,
        pending->expected_generation, pending->next_state, pending->kind});
    return session.snapshot().phase != dungeon::RoomPhase::faulted;
}

std::optional<items::ItemInstance> generated(std::uint64_t id,
    std::uint8_t base, items::ItemRarity rarity) noexcept {
    const auto* definition = items::base_definition(base);
    if (definition == nullptr) return std::nullopt;
    for (std::uint64_t seed = 1U; seed < 4096U; ++seed) {
        const auto item = items::generate_item(
            {seed, definition->slot, 90U, id, rarity});
        if (item.has_value() && item->base_id == base) return item;
    }
    return std::nullopt;
}

bool same_item(const items::ItemInstance& left,
    const items::ItemInstance& right) noexcept {
    if (!(left.id == right.id && left.base_id == right.base_id
        && left.rarity == right.rarity && left.item_level == right.item_level
        && left.required_level == right.required_level
        && left.affix_count == right.affix_count
        && left.reinforcement == right.reinforcement)) return false;
    for (std::size_t index = 0U; index < left.affixes.size(); ++index) {
        const auto& a = left.affixes[index];
        const auto& b = right.affixes[index];
        if (a.affix_id != b.affix_id || a.tier != b.tier
            || a.variant != b.variant || a.value_roll_bp != b.value_roll_bp) {
            return false;
        }
    }
    return true;
}

bool distribution() noexcept {
    constexpr std::array<std::uint16_t, 3U> dangers{{0U, 10U, 27U}};
    for (const std::uint16_t danger : dangers) {
        std::uint64_t observed = 0U;
        for (std::uint64_t seed = 1U; seed <= kTrials; ++seed) {
            if (dungeon::roll_material_drop(seed, 17U, 90U, danger)) ++observed;
        }
        const double probability = static_cast<double>(
            dungeon::material_drop_chance_bp(danger)) / 10000.0;
        const double mean = static_cast<double>(kTrials) * probability;
        const double sigma = std::sqrt(static_cast<double>(kTrials)
            * probability * (1.0 - probability));
        // Six sigma leaves enough room for a deterministic PRNG while still
        // making a basis-point or denominator regression immediately visible.
        const double tolerance = 6.0 * sigma + 12.0;
        std::cout << "danger=" << danger << " expected_bp="
                  << dungeon::material_drop_chance_bp(danger)
                  << " observed=" << observed << " tolerance="
                  << static_cast<std::uint64_t>(tolerance) << '\n';
        if (!require(std::fabs(static_cast<double>(observed) - mean)
                <= tolerance, "material binomial tolerance")) return false;
    }
    return true;
}

bool replay_keeps_existing_items_stable() noexcept {
    for (std::uint64_t seed = 1U; seed <= 2000U; ++seed) {
        const auto first = items::generate_item(
            {seed, items::ItemSlot::weapon, 75U, seed, items::ItemRarity::rare});
        // New material streams may be sampled arbitrarily often: their domain
        // separation must not perturb the frozen equipment generator stream.
        static_cast<void>(dungeon::roll_material_drop(seed, 17U, 90U, 27U));
        static_cast<void>(dungeon::roll_coupon_drop(seed, 17U, 90U, 27U, false));
        static_cast<void>(dungeon::roll_abyss_material(seed, 90U, 2U));
        const auto second = items::generate_item(
            {seed, items::ItemSlot::weapon, 75U, seed, items::ItemRarity::rare});
        if (!first.has_value() || !second.has_value()
            || !same_item(*first, *second)) return false;
    }
    return true;
}

bool transaction_boundaries() noexcept {
    const auto rare = generated(1001U, 1U, items::ItemRarity::rare);
    const auto normal_a = generated(1002U, 2U, items::ItemRarity::normal);
    const auto normal_b = generated(1003U, 2U, items::ItemRarity::normal);
    const auto normal_c = generated(1004U, 2U, items::ItemRarity::normal);
    if (!rare || !normal_a || !normal_b || !normal_c) return false;

    auto built = dungeon::make_initial_run_state(0x16A0U, dungeon::DungeonRules{});
    if (built.fault != dungeon::DungeonFault::none) return false;
    auto state = built.state;
    state.item_ownership.items = {*rare, *normal_a, *normal_b, *normal_c};
    state.item_ownership.next_item_sequence = 2000U;
    const std::size_t chaos = items::material_index(items::MaterialId::chaos);
    const std::size_t coupon = items::material_index(items::MaterialId::coupon_12);
    state.item_ownership.materials[chaos] = 1U;
    state.item_ownership.materials[coupon] = 1U;
    dungeon::DungeonSession session{dungeon::DungeonRules{}, state};

    if (!require(session.request_craft(items::MaterialId::chaos, rare->id)
            == dungeon::RequestResult::accepted, "craft accepted")) return false;
    const auto craft_pending = session.pending_save();
    if (!require(craft_pending.has_value()
            && craft_pending->next_state.item_ownership.materials[chaos] == 0U
            && session.item_state().materials[chaos] == 1U,
            "craft atomic before commit")) return false;
    session.resolve_pending_save({dungeon::SaveDisposition::not_committed,
        craft_pending->expected_generation, craft_pending->next_state,
        craft_pending->kind});
    if (!require(session.item_state().materials[chaos] == 1U,
            "craft rollback preserves material")) return false;
    if (!require(session.request_craft(items::MaterialId::chaos, rare->id)
            == dungeon::RequestResult::accepted && commit(session),
            "craft committed")) return false;

    if (!require(session.request_coupon(items::MaterialId::coupon_12, rare->id)
            == dungeon::RequestResult::accepted && commit(session),
            "coupon committed")) return false;
    const auto coupon_item = std::find_if(session.item_state().items.begin(),
        session.item_state().items.end(), [&](const items::ItemInstance& item) {
            return item.id == rare->id;
        });
    if (!require(coupon_item != session.item_state().items.end()
            && coupon_item->reinforcement == 12U, "coupon face threshold")) return false;

    if (!require(session.request_recipe({{normal_a->id, normal_b->id,
            normal_c->id}}) == dungeon::RequestResult::accepted && commit(session),
            "three-to-one exact base")) return false;
    return true;
}

bool all_currency_and_reinforcement_boundaries() noexcept {
    const auto normal = generated(2001U, 1U, items::ItemRarity::normal);
    const auto rare = generated(2002U, 1U, items::ItemRarity::rare);
    if (!normal || !rare) return false;
    items::ItemInstance magic{};
    bool magic_ready = false;
    for (std::uint64_t nonce = 1U; nonce < 512U; ++nonce) {
        const auto result = items::craft_item({0x16C0U, nonce, *normal,
            items::MaterialId::transmute});
        if (result.applied && result.item.affix_count == 1U) {
            magic = result.item;
            magic_ready = true;
            break;
        }
    }
    if (!require(magic_ready, "transmute boundary")) return false;
    const auto augment = items::craft_item({0x16C1U, 1U, magic,
        items::MaterialId::augment});
    const auto regal = items::craft_item({0x16C2U, 1U, magic,
        items::MaterialId::regal});
    const auto chaos = items::craft_item({0x16C3U, 1U, *rare,
        items::MaterialId::chaos});
    const auto exalt = items::craft_item({0x16C4U, 1U, *rare,
        items::MaterialId::exalt});
    const auto annul = items::craft_item({0x16C5U, 1U, *rare,
        items::MaterialId::annul});
    const auto divine = items::craft_item({0x16C6U, 1U, *rare,
        items::MaterialId::divine});
    const auto scour = items::craft_item({0x16C7U, 1U, *rare,
        items::MaterialId::scour});
    bool directed = false;
    for (std::uint64_t nonce = 1U; nonce < 512U && !directed; ++nonce) {
        const auto result = items::craft_item({0x16C8U, nonce, *rare,
            items::MaterialId::directed, items::DirectedCategory::damage});
        directed = result.applied;
    }
    if (!require(augment.applied && regal.applied && chaos.applied
            && exalt.applied && annul.applied && divine.applied && scour.applied
            && directed, "all nine crafting currencies")) return false;

    constexpr std::array<std::uint32_t, 12U> targets{{1U, 3U, 4U, 5U,
        6U, 7U, 12U, 13U, 14U, 15U, 16U,
        (std::numeric_limits<std::uint32_t>::max)()}};
    constexpr std::array<std::uint16_t, 12U> chances{{10000U, 10000U,
        9000U, 8000U, 7000U, 7000U, 7000U, 6000U, 5700U, 5415U, 5144U,
        10U}};
    for (std::size_t index = 0U; index < targets.size(); ++index) {
        if (!require(items::reinforcement_success_chance_bp(targets[index])
                == chances[index], "reinforcement chance boundary")) return false;
    }
    if (!require(items::reinforcement_failure(6U)
            == items::ReinforcementFailure::unchanged
            && items::reinforcement_failure(7U)
                == items::ReinforcementFailure::reset_six
            && items::reinforcement_failure(10U)
                == items::ReinforcementFailure::reset_zero
            && items::reinforcement_failure(12U)
                == items::ReinforcementFailure::destroy,
            "reinforcement failure boundary")) return false;
    for (const auto coupon : {items::MaterialId::coupon_6,
            items::MaterialId::coupon_9, items::MaterialId::coupon_12,
            items::MaterialId::coupon_15}) {
        const auto applied = items::apply_coupon(*normal, coupon);
        if (!require(applied.has_value()
                && applied->reinforcement == items::coupon_reinforcement_level(coupon),
                "coupon face boundary")) return false;
    }
    return true;
}

bool pickup_vacuum_death_reload_and_saturation() noexcept {
    auto built = dungeon::make_initial_run_state(0x16B0U, dungeon::DungeonRules{});
    if (built.fault != dungeon::DungeonFault::none) return false;
    dungeon::DungeonSession session{dungeon::DungeonRules{}, built.state};
    const auto player = session.snapshot().combat->player.position;
    arpg::test::install_ground_material(session, 4U, items::MaterialId::chaos, player);
    if (!require(session.request_material_pickup(4U)
            == dungeon::RequestResult::accepted && commit(session),
            "material pickup committed")) return false;
    const auto persisted = session.item_state();
    dungeon::DungeonSession reload{dungeon::DungeonRules{},
        arpg::test::DungeonSessionTestAccess::stable_state(session)};
    static_cast<void>(reload);
    if (!require(persisted.materials[items::material_index(items::MaterialId::chaos)]
            == 1U, "pickup count")) return false;

    auto vacuum_state = arpg::test::DungeonSessionTestAccess::stable_state(session);
    dungeon::DungeonSession vacuum{dungeon::DungeonRules{}, vacuum_state};
    arpg::test::install_ground_material(vacuum, 6U, items::MaterialId::coupon_6,
        player, dungeon::GroundMaterialSource::monster_coupon);
    arpg::test::set_phase(vacuum, dungeon::RoomPhase::combat);
    arpg::test::prepare_room_clear(vacuum);
    if (!require(vacuum.pending_save().has_value()
            && vacuum.pending_save()->kind == dungeon::PendingSaveKind::room_clear
            && commit(vacuum), "room clear vacuum")) return false;

    auto death_state = arpg::test::DungeonSessionTestAccess::stable_state(vacuum);
    dungeon::DungeonSession death{dungeon::DungeonRules{}, death_state};
    arpg::test::install_ground_material(death, 8U, items::MaterialId::exalt, player);
    arpg::test::set_phase(death, dungeon::RoomPhase::combat);
    if (!arpg::test::kill_current_player_through_combat(death)) return false;
    arpg::test::DungeonSessionTestAccess::handle_player_defeat(death);
    if (!require(death.pending_save().has_value() && commit(death)
            && death.item_state().materials[items::material_index(items::MaterialId::exalt)]
                == 0U, "death loses unpicked material")) return false;

    return require(items::reinforced_base_value(1,
        (std::numeric_limits<std::uint32_t>::max)())
        == (std::numeric_limits<std::int64_t>::max)(), "reinforcement saturation");
}

}  // namespace

int main() {
    const bool passed = distribution()
        && require(dungeon::coupon_eligible(items::MaterialId::coupon_6, 16U, 6U),
            "coupon +6 threshold")
        && require(!dungeon::coupon_eligible(items::MaterialId::coupon_6, 15U, 6U),
            "coupon +6 depth boundary")
        && require(dungeon::coupon_eligible(items::MaterialId::coupon_9, 30U, 10U),
            "coupon +9 threshold")
        && require(dungeon::coupon_eligible(items::MaterialId::coupon_12, 60U, 14U),
            "coupon +12 threshold")
        && require(dungeon::coupon_eligible(items::MaterialId::coupon_15, 90U, 18U),
            "coupon +15 threshold")
        && replay_keeps_existing_items_stable()
        && transaction_boundaries()
        && all_currency_and_reinforcement_boundaries()
        && pickup_vacuum_death_reload_and_saturation();
    std::cout << "stage16 simulation=" << (passed ? "PASS" : "FAIL") << '\n';
    return passed ? 0 : 1;
}
