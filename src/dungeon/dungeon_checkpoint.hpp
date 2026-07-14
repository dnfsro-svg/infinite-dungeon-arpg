#pragma once

#include "passives/passive_tree_types.hpp"
#include "progression/progression_types.hpp"

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

struct DungeonRunState final {
    std::uint64_t root_seed{};
    std::uint64_t commit_generation{1};
    std::array<std::uint32_t, 4> biases{};
    RoomDescriptor current_room{};
    TransitionKind last_transition{TransitionKind::none};
    ExitDirection last_direction{ExitDirection::none};
    progression::ProgressionState progression{};
    passives::PassiveTreeState passive_tree{};
};

}  // namespace arpg::dungeon::checkpoint
