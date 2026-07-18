#pragma once

#include "control_hints.hpp"
#include "dungeon/dungeon_types.hpp"
#include "dungeon_runtime.hpp"

#include <array>
#include <cstdint>

namespace arpg::platform {

enum class HudStatusTagKind : std::uint8_t { slow, corrosion, invulnerable };

// Kept here because both the view model and the fixed notice queue own this
// stable player-visible vocabulary.  The queue remains the sole owner of
// priority and lifetime policy.
enum class HudNoticeKind : std::uint8_t {
    none, save_error, recovery_required, abyss_abandon,
    hole_interact, exit_ready, room_clear, reward, level_up,
    passive_points, inventory, passive_tree
};

struct HudNoticeView;

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
    HudText96 primary{};
    HudText96 ecology_label{};
    struct Element final {
        HudText96 label{};
        struct Color final {
            std::uint8_t r{};
            std::uint8_t g{};
            std::uint8_t b{};
            std::uint8_t a{255U};
        } color{};
    };
    std::array<Element, 4> elements{};
    std::uint8_t element_count{};
};

struct ContextHudModel final {
    HudText96 primary{};
    HudText96 secondary{};
    HudNoticeKind primary_kind{HudNoticeKind::none};
    HudNoticeKind secondary_kind{HudNoticeKind::none};
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
    ContextHudModel context{};
    HudBuildDiagnostics diagnostics{};
};

void build_hud_view_model(HudViewModel& output,
    const dungeon::DungeonSnapshot& snapshot,
    const DungeonRenderStatus& runtime_status,
    const ControlHints& hints) noexcept;
void attach_notice_view(HudViewModel&, const HudNoticeView&) noexcept;

}  // namespace arpg::platform
