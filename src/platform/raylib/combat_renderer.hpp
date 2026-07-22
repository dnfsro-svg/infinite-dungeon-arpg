#pragma once

#include "active_skill_renderer.hpp"
#include "active_skill_view.hpp"
#include "combat_feedback.hpp"
#include "death_overlay_renderer.hpp"
#include "debug_overlay_renderer.hpp"
#include "dungeon_view_math.hpp"
#include "ground_loot_view.hpp"
#include "hud_notice_state.hpp"
#include "hud_renderer.hpp"
#include "loot_pickup_feedback.hpp"
#include "material_loot_view.hpp"
#include "material_pack.hpp"
#include "monster_material_presenter.hpp"

#include <cstdint>
#include <array>
#include <optional>
#include <cstddef>

namespace arpg::platform {

struct DungeonRenderStatus;
struct ControlHints;
enum class HudPresentedFrame : std::uint8_t {
    normal,
    recovery,
    death_overlay,
    count,
};

enum class CombatRenderStage : std::uint8_t {
    room,
    actors,
    ground_loot_labels,
    normal_hud,
};

struct CombatRenderPlan final {
    GroundLootView ground_loot{};
    MaterialLootView material_loot{};
    std::array<CombatRenderStage, 4> stages{};
    std::size_t stage_count{};
};

struct MonsterMaterialDrawRuntimeStatus final {
    bool presenter_visible{};
    bool use_material_frame{};
    MaterialAtlasId atlas{MaterialAtlasId::count};
    std::uint16_t frame_index{};
    bool drawn{};
};

[[nodiscard]] CombatRenderPlan make_combat_render_plan(
    const dungeon::DungeonSnapshot& snapshot,
    settings::LootFilterMode mode,
    float width,
    float height) noexcept;

[[nodiscard]] std::optional<std::size_t> hud_presented_frame_index(
    HudPresentedFrame) noexcept;

struct DoorRenderDecision final {
    const char* label{};
    const char* arrow{};
    Rgba8 frame{};
    Rgba8 text{};
    Rgba8 locked_interior{};
    bool draw_locked_interior{};
};

[[nodiscard]] DoorRenderDecision door_render_decision(
    DoorVisualMode mode,
    dungeon::ExitDirection direction) noexcept;

class CombatRenderer final {
public:
    [[nodiscard]] bool initialize_resources() noexcept;
    void shutdown_resources() noexcept;
    [[nodiscard]] bool active_skill_assets_ready() const noexcept;
    [[nodiscard]] bool material_pipeline_ready() const noexcept;
    [[nodiscard]] bool material_ecology_ready(
        MaterialEcology ecology) const noexcept;
    [[nodiscard]] bool material_atlas_available(
        MaterialAtlasId atlas) const noexcept;
    [[nodiscard]] const MaterialPack& material_pack() const noexcept;
    [[nodiscard]] std::uint64_t material_sprite_draw_count(
        MaterialSpriteId sprite) const noexcept;
    [[nodiscard]] std::uint64_t material_direct_stretch_draw_count(
        MaterialSpriteId sprite) const noexcept;
    [[nodiscard]] MonsterMaterialDrawRuntimeStatus monster_material_draw_status(
        combat::MonsterId monster) const noexcept;
    void consume_event(const combat::CombatEvent& event) noexcept;
    void consume_dungeon_event(const dungeon::DungeonEvent& event) noexcept;
    void clear_combat_transients() noexcept;
    void set_loot_filter_mode(settings::LootFilterMode mode) noexcept;
    void update(float frame_seconds) noexcept;
    void observe_hud(
        const dungeon::DungeonSnapshot& previous,
        const dungeon::DungeonSnapshot& current,
        const DungeonRenderStatus& runtime_status,
        const ControlHints& control_hints,
        float frame_seconds,
        bool paused) noexcept;
    void observe_presented_hud_frame(
        HudPresentedFrame,
        const dungeon::DungeonSnapshot& previous,
        const dungeon::DungeonSnapshot& current,
        const DungeonRenderStatus& runtime_status,
        const ControlHints& control_hints,
        float frame_seconds,
        bool paused) noexcept;
    [[nodiscard]] const HudViewModel& hud_model() const noexcept;
    [[nodiscard]] HudNoticeView hud_notice_view() const noexcept;
    [[nodiscard]] std::uint64_t hud_binding_revision() const noexcept;
    [[nodiscard]] std::uint64_t hud_observation_count() const noexcept;
    [[nodiscard]] HudStaticFormattingDiagnostics
        hud_static_formatting_diagnostics() const noexcept;
    [[nodiscard]] std::uint64_t hud_presented_frame_count(
        HudPresentedFrame) const noexcept;
    [[nodiscard]] const ActiveSkillHudModel& active_skill_hud_model()
        const noexcept;
    [[nodiscard]] Font hud_font() const noexcept;
    [[nodiscard]] bool hud_font_ready() const noexcept;
    [[nodiscard]] GroundLootView draw(
        const dungeon::DungeonSnapshot& previous,
        const dungeon::DungeonSnapshot& current,
        const DungeonRenderStatus& runtime_status,
        float interpolation_alpha,
        bool draw_debug,
        const CombatFeedback& feedback,
        bool audio_ready) noexcept;

private:
    void draw_room(
        const dungeon::DungeonSnapshot& current,
        const GroundLootView& ground_loot,
        const MaterialLootView& material_loot) const noexcept;
    void draw_actors(
        const dungeon::DungeonSnapshot& previous,
        const dungeon::DungeonSnapshot& current,
        float interpolation_alpha,
        bool draw_debug,
        const CombatFeedback& feedback) noexcept;
    void draw_hud() const noexcept;
    void draw_abyss_hud(
        const dungeon::DungeonSnapshot& current,
        float x,
        int& y,
        int line_step) const noexcept;
    void draw_debug_world_volumes(
        const combat::CombatSnapshot& snapshot,
        float width,
        float height) const noexcept;
    combat::CombatEvent last_event_{};
    bool has_last_event_{};
    TransitionVisualState transition_{};
    DeathOverlayRenderer death_overlay_{};
    HudRenderer hud_renderer_{};
    ActiveSkillRenderer active_skill_renderer_{};
    DebugOverlayRenderer debug_overlay_{};
    HudNoticeState hud_notices_{};
    LootPickupFeedbackState loot_pickup_feedback_{};
    MaterialPickupFeedbackState material_pickup_feedback_{};
    MaterialPack material_pack_{};
    MonsterMaterialPresenter monster_presenter_{};
    std::array<MonsterMaterialDrawRuntimeStatus,
        static_cast<std::size_t>(combat::MonsterId::count)>
        monster_material_draw_statuses_{};
    HudViewModelProjector hud_projector_{};
    HudViewModel hud_model_{};
    ActiveSkillHudModel active_skill_hud_model_{};
    HudLayout hud_layout_{};
    std::uint64_t hud_binding_revision_{};
    std::uint64_t hud_observation_count_{};
    std::array<std::uint64_t,
        static_cast<std::size_t>(HudPresentedFrame::count)> hud_presented_frame_counts_{};
    settings::LootFilterMode loot_filter_mode_{
        settings::LootFilterMode::show_all};
};

}  // namespace arpg::platform
