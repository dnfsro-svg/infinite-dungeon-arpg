#pragma once

#include <cstdint>
#include <memory>

namespace arpg::combat {
struct CombatEvent;
struct MovementInput;
}

namespace arpg::dungeon {
class DungeonSession;
struct DungeonSnapshot;
enum class RequestResult : std::uint8_t;
}

namespace arpg::items {
struct ItemOwnershipState;
}

namespace arpg::settings {
enum class SettingsLoadStatus : std::uint8_t;
struct SettingsData;
}

namespace arpg::platform {

class InventoryRenderer;
struct PhysicalKeySnapshot;
struct RaylibHostConfig;
struct SubmittedFrameActions;

class HostValidationRuntime final {
public:
    static std::unique_ptr<HostValidationRuntime> create(
        const RaylibHostConfig&,
        settings::SettingsLoadStatus) noexcept;
    ~HostValidationRuntime();
    void set_render_readiness(bool cjk_font_ready,
        bool active_skill_atlases_ready) noexcept;
    PhysicalKeySnapshot inject_physical_edges(
        PhysicalKeySnapshot, const settings::SettingsData&,
        const dungeon::DungeonSnapshot&,
        bool gameplay_rearm_required) noexcept;
    [[nodiscard]] bool should_continue_death(
        const dungeon::DungeonSnapshot&) const noexcept;
    combat::MovementInput fixed_step_movement(
        dungeon::DungeonSession&, const dungeon::DungeonSnapshot&,
        combat::MovementInput production_input) noexcept;
    void observe_fixed_tick() noexcept;
    void observe_death_continue_result(
        dungeon::RequestResult) noexcept;
    void observe_post_fixed_tick(
        const dungeon::DungeonSnapshot&,
        const items::ItemOwnershipState*) noexcept;
    [[nodiscard]] bool fixed_step_target_reached(
        const dungeon::DungeonSnapshot&) const noexcept;
    void observe_combat_event(const combat::CombatEvent&) noexcept;
    void observe_snapshot(const dungeon::DungeonSnapshot&) noexcept;
    void observe_inventory(const InventoryRenderer&,
        const dungeon::DungeonSnapshot&) noexcept;
    void observe_submitted_actions(const SubmittedFrameActions&) noexcept;

private:
    struct Impl;
    friend struct HostValidationStateAccess;
    explicit HostValidationRuntime(std::unique_ptr<Impl>) noexcept;
    std::unique_ptr<Impl> impl_;
};

}  // namespace arpg::platform
