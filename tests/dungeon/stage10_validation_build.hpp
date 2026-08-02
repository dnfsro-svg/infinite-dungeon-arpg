#pragma once

#include "dungeon/dungeon_types.hpp"
#include "items/item_catalog.hpp"
#include "items/item_generation.hpp"
#include "passives/passive_tree_rules.hpp"

#include <algorithm>
#include <array>
#include <cstdint>

namespace arpg::test {

inline bool install_stage10_validation_build(
    dungeon::DungeonRunState& state) {
    constexpr std::array<std::uint8_t, 6> base_ids{{
        8U, 10U, 12U, 14U, 16U, 18U,
    }};
    constexpr std::array<std::array<items::AffixRoll, 6>, 6> affixes{{
        {{{1U, 1U, 0xFFU}, {2U, 1U, 0xFFU}, {3U, 1U, 0xFFU},
          {101U, 1U, 0xFFU}, {105U, 1U, 0xFFU}, {106U, 1U, 0xFFU}}},
        {{{7U, 1U, 0xFFU}, {11U, 1U, 0xFFU}, {12U, 1U, 0xFFU},
          {103U, 1U, 0xFFU}, {104U, 1U, 0xFFU}, {111U, 1U, 0xFFU}}},
        {{{7U, 1U, 0xFFU}, {11U, 1U, 0xFFU}, {12U, 1U, 0xFFU},
          {103U, 1U, 0xFFU}, {104U, 1U, 0xFFU}, {111U, 1U, 0xFFU}}},
        {{{3U, 1U, 0xFFU}, {11U, 1U, 0xFFU}, {12U, 1U, 0xFFU},
          {101U, 1U, 0xFFU}, {103U, 1U, 0xFFU}, {104U, 1U, 0xFFU}}},
        {{{7U, 1U, 0xFFU}, {11U, 1U, 0xFFU}, {12U, 1U, 0xFFU},
          {102U, 1U, 0xFFU}, {103U, 1U, 0xFFU}, {104U, 1U, 0xFFU}}},
        {{{3U, 1U, 0xFFU}, {11U, 1U, 0xFFU}, {12U, 1U, 0xFFU},
          {103U, 1U, 0xFFU}, {110U, 1U, 0xFFU}, {111U, 1U, 0xFFU}}},
    }};
    state.progression = {100U, 0U, 99U, 99U};
    state.item_ownership = {};
    state.item_ownership.items.reserve(6U);
    for (std::uint8_t index = 0U; index < 6U; ++index) {
        const std::uint64_t id = static_cast<std::uint64_t>(index) + 1U;
        auto item = items::generate_item({0xA8100000ULL + index,
            static_cast<items::ItemSlot>(index), 100U, id,
            items::ItemRarity::rare});
        if (!item.has_value()) return false;
        item->base_id = base_ids[index];
        item->affixes = {};
        std::copy(affixes[index].begin(), affixes[index].end(),
            item->affixes.begin());
        item->affix_count = 6U;
        item->required_level = 95U;
        item->reinforcement = 15U;
        if (!items::validate_item(*item)) return false;
        state.item_ownership.items.push_back(*item);
        state.item_ownership.equipment.equipped_ids[index] = id;
    }
    state.item_ownership.next_item_sequence = 7U;
    return true;
}

inline bool install_stage10_validation_survival_passives(
    dungeon::DungeonRunState& state) {
    constexpr std::array<passives::PassiveNodeId, 4> route_bases{{
        8U, 22U, 36U, 50U,
    }};
    const auto allocate = [&](passives::PassiveNodeId node) {
        const std::uint16_t unspent_before =
            state.progression.unspent_passive_points;
        const auto result = passives::allocate_node(
            state.passive_tree, state.progression, node);
        return result.changed
            && result.error == passives::PassiveTreeError::none
            && state.progression.unspent_passive_points + 1U
                == unspent_before;
    };
    for (passives::PassiveNodeId node = 1U; node <= 7U; ++node) {
        if (!allocate(node)) return false;
    }
    for (const passives::PassiveNodeId base : route_bases) {
        for (passives::PassiveNodeId offset = 0U; offset < 13U; ++offset) {
            if (!allocate(static_cast<passives::PassiveNodeId>(
                    base + offset))) {
                return false;
            }
        }
    }
    return true;
}

inline bool install_stage10_validation_offense_build(
    dungeon::DungeonRunState& state) {
    constexpr std::array<items::AffixRoll, 6> affixes{{
        {1U, 1U, 0xFFU},
        {2U, 1U, 0xFFU},
        {3U, 1U, 0xFFU},
        {101U, 1U, 0xFFU},
        {105U, 1U, 0xFFU},
        {106U, 1U, 0xFFU},
    }};
    state.progression = {100U, 0U, 99U, 99U};
    constexpr std::array<passives::PassiveNodeId, 3> offense_nodes{{
        3U, 4U, 6U,
    }};
    for (const passives::PassiveNodeId node : offense_nodes) {
        const auto allocated = passives::allocate_node(
            state.passive_tree, state.progression, node);
        if (!allocated.changed
                || allocated.error != passives::PassiveTreeError::none) {
            return false;
        }
    }
    state.item_ownership = {};
    state.item_ownership.items.reserve(1U);
    auto item = items::generate_item({
        0xA8100000ULL,
        items::ItemSlot::weapon,
        100U,
        1U,
        items::ItemRarity::rare,
    });
    if (!item.has_value()) return false;
    item->base_id = 8U;
    item->affixes = {};
    std::copy(affixes.begin(), affixes.end(), item->affixes.begin());
    item->affix_count = static_cast<std::uint8_t>(affixes.size());
    item->required_level = 95U;
    item->reinforcement = 15U;
    if (!items::validate_item(*item)) return false;
    state.item_ownership.items.push_back(*item);
    state.item_ownership.equipment.equipped_ids[0] = item->id;
    state.item_ownership.next_item_sequence = 2U;
    return true;
}

}  // namespace arpg::test
