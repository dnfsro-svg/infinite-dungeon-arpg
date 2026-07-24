#pragma once

#include "control_hints.hpp"
#include "dungeon/dungeon_types.hpp"
#include "dungeon_runtime.hpp"
#include "hud_color.hpp"

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
    passive_points, inventory, passive_tree, loot_pickup
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
    HudText96 movement{};
    std::array<HudText96, 3U> controls{};
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
        HudPaletteId color_id{HudPaletteId::fire};
    };
    std::array<Element, 4> elements{};
    std::uint8_t element_count{};
};

struct ContextHudModel final {
    HudText96 primary{};
    HudText96 secondary{};
    HudNoticeKind primary_kind{HudNoticeKind::none};
    HudNoticeKind secondary_kind{HudNoticeKind::none};
    bool primary_abyss{};
    bool secondary_abyss{};
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

struct HudStaticFormattingDiagnostics final {
    std::uint64_t objective_rebuilds{};
    std::uint64_t navigation_rebuilds{};
    std::uint64_t control_hint_rebuilds{};
};

[[nodiscard]] constexpr bool operator==(
    const HudStaticFormattingDiagnostics& lhs,
    const HudStaticFormattingDiagnostics& rhs) noexcept {
    return lhs.objective_rebuilds == rhs.objective_rebuilds
        && lhs.navigation_rebuilds == rhs.navigation_rebuilds
        && lhs.control_hint_rebuilds == rhs.control_hint_rebuilds;
}

class HudViewModelProjector final {
public:
    void build(HudViewModel& output,
        const dungeon::DungeonSnapshot& snapshot,
        const DungeonRenderStatus& runtime_status,
        const ControlHints& hints) noexcept;
    [[nodiscard]] HudStaticFormattingDiagnostics
        static_formatting_diagnostics() const noexcept;

private:
    struct ObjectiveKey final {
        bool is_abyss{};
        dungeon::RoomPhase phase{dungeon::RoomPhase::locked};
        std::uint8_t wave_index{};
        std::uint8_t wave_count{};
        std::uint8_t remaining_targets{};
        abyss::AbyssDanger abyss_danger{abyss::AbyssDanger::low};
        abyss::AbyssRuleId abyss_rule{abyss::AbyssRuleId::none};
        std::uint8_t abyss_pending_rewards{};
        std::uint8_t abyss_unpicked_rewards{};
    };

    struct NavigationKey final {
        std::uint64_t depth{};
        std::uint64_t floor_room{};
        dungeon::DungeonElement ecology{dungeon::DungeonElement::fire};
        std::array<std::uint32_t, 4> biases{};
    };

    bool objective_ready_{};
    bool navigation_ready_{};
    bool control_hints_ready_{};
    ObjectiveKey objective_key_{};
    NavigationKey navigation_key_{};
    std::uint64_t control_hints_revision_{};
    RoomHudModel cached_objective_{};
    NavigationHudModel cached_navigation_{};
    HudText96 cached_movement_hint_{};
    std::array<HudText96, 3U> cached_control_hint_lines_{};
    std::uint32_t cached_objective_truncations_{};
    std::uint32_t cached_navigation_truncations_{};
    std::uint32_t cached_control_hint_truncations_{};
    HudStaticFormattingDiagnostics static_formatting_diagnostics_{};
};

void build_hud_view_model(HudViewModel& output,
    const dungeon::DungeonSnapshot& snapshot,
    const DungeonRenderStatus& runtime_status,
    const ControlHints& hints) noexcept;
void attach_notice_view(HudViewModel&, const HudNoticeView&) noexcept;

}  // namespace arpg::platform
