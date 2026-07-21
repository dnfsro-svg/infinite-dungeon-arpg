#pragma once

#include "hud_layout.hpp"
#include "hud_palette.hpp"
#include "hud_view_model.hpp"
#include "ground_loot_view.hpp"
#include "material_loot_view.hpp"

#include <raylib.h>

#include <array>
#include <cstdint>

namespace arpg::combat {
struct MonsterSnapshot;
}

namespace arpg::platform {

enum class HudBarKind : std::uint8_t { health, barrier, experience };

struct HudBarPlan final {
    HudRect bounds{};
    float ratio{};
    HudBarKind kind{HudBarKind::health};
};

struct PlayerPanelPlan final {
    std::array<HudBarPlan, 3> bars{};
    std::uint8_t bar_count{};
    HudText96 progression_text{};
    bool low_health_emphasis{};
    std::array<HudStatusTagKind, 3> tags{};
    std::uint8_t tag_count{};
    bool experience_maxed{};
};

struct MonsterBarPlan final {
    bool visible{};
    float ratio{};
    HudPaletteId palette_id{HudPaletteId::health};
};

struct MonsterBarVisualPlan final {
    std::array<MonsterBarPlan, 3> bars{};
    bool world_space{true};
};

struct ObjectivePanelPlan final {
    bool visible{};
    bool abyss{};
    HudRect bounds{};
    HudText96 primary{};
    HudText96 secondary{};
};

struct NavigationPanelPlan final {
    bool visible{};
    HudRect bounds{};
    HudText96 primary{};
    HudText96 ecology{};
    std::array<NavigationHudModel::Element, 4> elements{};
    std::uint8_t element_count{};
};

struct ContextPanelPlan final {
    bool primary_visible{};
    bool secondary_visible{};
    HudRect primary_bounds{};
    HudRect secondary_bounds{};
    HudText96 primary{};
    HudText96 secondary{};
    HudNoticeKind primary_kind{HudNoticeKind::none};
    HudNoticeKind secondary_kind{HudNoticeKind::none};
    bool primary_abyss{};
    bool secondary_abyss{};
};

using HudTextMeasureFn = float (*)(const char*, float, void*) noexcept;

struct HudTextDrawPlan final {
    bool visible{};
    bool truncated{};
    HudText96 text{};
    float font_size{};
};

struct HudReadabilityStyle final {
    float panel_minimum_font_size{15.0F};
    float player_bar_font_size{19.0F};
    float progression_font_size{17.0F};
    float status_tag_font_size{15.0F};
    float objective_primary_font_size{22.0F};
    float objective_secondary_font_size{18.0F};
    float navigation_primary_font_size{20.0F};
    float navigation_secondary_font_size{18.0F};
    float navigation_element_font_size{16.0F};
    int outline_pixels{2};
};

[[nodiscard]] PlayerPanelPlan make_player_panel_plan(
    const PlayerHudModel&, const HudLayout&, float presentation_seconds) noexcept;
[[nodiscard]] MonsterBarVisualPlan make_monster_bar_visual_plan(
    const combat::MonsterSnapshot&) noexcept;
[[nodiscard]] ObjectivePanelPlan make_objective_panel_plan(
    const RoomHudModel&, const HudLayout&) noexcept;
[[nodiscard]] NavigationPanelPlan make_navigation_panel_plan(
    const NavigationHudModel&, const HudLayout&) noexcept;
[[nodiscard]] ContextPanelPlan make_context_panel_plan(
    const ContextHudModel&, const HudLayout&) noexcept;
[[nodiscard]] HudTextDrawPlan make_hud_text_draw_plan(const HudText96&,
    float bounds_width, float preferred_font_size, float minimum_font_size,
    HudTextMeasureFn, void*) noexcept;
[[nodiscard]] HudReadabilityStyle hud_readability_style() noexcept;

class HudRenderer final {
public:
    HudRenderer() noexcept = default;
    ~HudRenderer() noexcept;
    HudRenderer(const HudRenderer&) = delete;
    HudRenderer& operator=(const HudRenderer&) = delete;

    [[nodiscard]] bool initialize() noexcept;
    void shutdown() noexcept;
    [[nodiscard]] bool font_ready() const noexcept;
    [[nodiscard]] Font hud_font() const noexcept;
    void draw_ground_loot(const GroundLootView&) const noexcept;
    void draw_material_loot(const MaterialLootView&) const noexcept;
    void draw(const HudViewModel&, const HudLayout&) const noexcept;

private:
    Font font_{};
    bool font_ready_{};
};

}  // namespace arpg::platform
