#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

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
enum class LootFilterMode : std::uint8_t;
enum class SettingsLoadStatus : std::uint8_t;
struct SettingsData;
}

namespace arpg::platform {

class InventoryRenderer;
struct ActiveSkillDrawRuntimeStatus;
enum class CleanShutdownState : std::uint8_t;
struct DungeonRenderStatus;
struct GroundLootView;
struct HudNoticeView;
struct HudViewModel;
struct PauseMenuState;
struct PhysicalKeySnapshot;
struct RaylibHostConfig;
struct SubmittedFrameActions;

enum class CaptureOwner : std::uint8_t {
    none,
    generic_validation,
    stage17,
};

struct PresentationDecision final {
    bool validation_complete{};
    bool generic_capture_visible{};
    bool generic_capture_complete{};
    CaptureOwner capture_owner{CaptureOwner::none};
    std::optional<std::string> stage17_capture_path{};
};

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
    [[nodiscard]] const PhysicalKeySnapshot&
    death_input_snapshot() const noexcept;
    void observe_pause_transition(bool was_open, bool is_open) noexcept;
    void prepare_hud_snapshot(dungeon::DungeonSnapshot&) noexcept;
    void observe_hud(const dungeon::DungeonSnapshot&,
        const HudViewModel&, HudNoticeView, bool draw_debug,
        int screen_width, int screen_height) noexcept;
    void observe_active_skill_draw(const dungeon::DungeonSnapshot&,
        const ActiveSkillDrawRuntimeStatus&) noexcept;
    void observe_ground_loot(const dungeon::DungeonSnapshot&,
        const PauseMenuState&, const DungeonRenderStatus&,
        settings::LootFilterMode, const GroundLootView&, HudNoticeView,
        const items::ItemOwnershipState*, int screen_width,
        int screen_height) noexcept;
    [[nodiscard]] PresentationDecision observe_presented_frame(
        const dungeon::DungeonSnapshot&, const PauseMenuState&,
        bool pause_cjk_ready) noexcept;
    void observe_capture_result(CaptureOwner, bool succeeded) noexcept;
    void write_summaries(CleanShutdownState,
        const PauseMenuState&) noexcept;
    void observe_combat_event(const combat::CombatEvent&) noexcept;
    void observe_snapshot(const dungeon::DungeonSnapshot&) noexcept;
    void observe_inventory(const InventoryRenderer&,
        const dungeon::DungeonSnapshot&) noexcept;
    void observe_submitted_actions(const SubmittedFrameActions&) noexcept;

private:
    struct Impl;
    explicit HostValidationRuntime(std::unique_ptr<Impl>) noexcept;
    std::unique_ptr<Impl> impl_;
};

}  // namespace arpg::platform
