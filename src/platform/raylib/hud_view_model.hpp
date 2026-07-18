#pragma once

#include "control_hints.hpp"
#include "dungeon/dungeon_types.hpp"
#include "dungeon_runtime.hpp"

#include <array>
#include <cstdint>

namespace arpg::platform {

enum class HudStatusTagKind : std::uint8_t { slow, corrosion, invulnerable };

struct HudText96 final {
    std::array<char, 96> bytes{};
    bool truncated{};
};

struct PlayerHudModel final {
    bool visible{};
    int hp{};
    int max_hp{};
    int barrier{};
    int max_barrier{};
    float hp_ratio{};
    float barrier_ratio{};
    std::uint8_t level{};
    std::uint64_t experience{};
    std::uint64_t required_experience{};
    float experience_ratio{};
    std::uint8_t unspent_passive_points{};
    std::array<HudStatusTagKind, 3> status_tags{};
    std::uint8_t status_tag_count{};
};

struct RoomHudModel final {
    HudText96 objective{};
    HudText96 secondary{};
    bool abyss{};
    std::uint8_t remaining_targets{};
};

struct NavigationHudModel final {
    std::uint64_t depth{};
    std::uint64_t floor_room{};
    dungeon::DungeonElement ecology{};
    std::array<std::uint32_t, 4> biases{};
};

struct HudBuildDiagnostics final {
    std::uint32_t clamped_values{};
    std::uint32_t truncated_texts{};
    bool combat_snapshot_missing{};
};

struct HudViewModel final {
    PlayerHudModel player{};
    RoomHudModel room{};
    NavigationHudModel navigation{};
    HudBuildDiagnostics diagnostics{};
};

void build_hud_view_model(HudViewModel& output,
    const dungeon::DungeonSnapshot& snapshot,
    const DungeonRenderStatus& runtime_status,
    const ControlHints& hints) noexcept;

}  // namespace arpg::platform
