#pragma once

#include "abyss/abyss_types.hpp"
#include "passives/passive_tree_types.hpp"
#include "progression/progression_types.hpp"
#include "items/item_types.hpp"

#include <array>
#include <cstdint>

namespace arpg::dungeon::checkpoint {

enum class ExitDirection : std::uint8_t {
    up = 0,
    down = 1,
    left = 2,
    right = 3,
    none = 0xFF,
};

enum class EntrySide : std::uint8_t {
    initial = 0,
    top = 1,
    bottom = 2,
    left = 3,
    right = 4,
};

enum class DungeonElement : std::uint8_t {
    fire = 0,
    water = 1,
    lightning = 2,
    chaos = 3,
};

enum class TransitionKind : std::uint8_t {
    door = 0,
    descent = 1,
    none = 0xFF,
};

struct RoomDescriptor final {
    std::uint64_t index{};
    std::uint64_t seed{};
    std::uint64_t depth{1};
    std::uint64_t floor_room_index{1};
    EntrySide entry{EntrySide::initial};
    DungeonElement ecology{DungeonElement::fire};
    bool has_hole{};
    bool is_abyss{};
};

struct AbyssCheckpoint final {
    abyss::AbyssLifecycle lifecycle{abyss::AbyssLifecycle::none};
    abyss::AbyssDanger danger{abyss::AbyssDanger::low};
    abyss::AbyssRuleId rule{abyss::AbyssRuleId::none};
    std::uint32_t rules_version{};
    std::uint8_t reward_total{};
    std::uint8_t generated_mask{};
    std::uint8_t claimed_mask{};
    std::uint8_t abandoned_mask{};
    std::uint32_t reward_revision{};
};

struct LastAbyssResolution final {
    bool valid{};
    std::uint64_t room_seed{};
    abyss::AbyssRuleId rule{abyss::AbyssRuleId::none};
    std::uint8_t total{};
    std::uint8_t generated{};
    std::uint8_t claimed{};
    std::uint8_t abandoned{};
};

struct DungeonRunState final {
    std::uint64_t root_seed{};
    std::uint64_t commit_generation{1};
    std::array<std::uint32_t, 4> biases{};
    RoomDescriptor current_room{};
    AbyssCheckpoint abyss{};
    LastAbyssResolution last_abyss_resolution{};
    TransitionKind last_transition{TransitionKind::none};
    ExitDirection last_direction{ExitDirection::none};
    progression::ProgressionState progression{};
    passives::PassiveTreeState passive_tree{};
    items::ItemOwnershipState item_ownership{};
};

[[nodiscard]] constexpr bool valid_abyss_door_origin(
    const DungeonRunState& state,
    bool rolled) noexcept {
    if (!rolled || !state.current_room.is_abyss
            || state.last_transition != TransitionKind::door) {
        return false;
    }
    switch (state.last_direction) {
    case ExitDirection::up:
        return state.current_room.entry == EntrySide::bottom;
    case ExitDirection::down:
        return state.current_room.entry == EntrySide::top;
    case ExitDirection::left:
        return state.current_room.entry == EntrySide::right;
    case ExitDirection::right:
        return state.current_room.entry == EntrySide::left;
    case ExitDirection::none:
        return false;
    }
    return false;
}

}  // namespace arpg::dungeon::checkpoint
