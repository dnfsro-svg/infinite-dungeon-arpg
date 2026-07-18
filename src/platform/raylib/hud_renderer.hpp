#pragma once

#include "hud_layout.hpp"
#include "hud_view_model.hpp"

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
    bool low_health_emphasis{};
    std::array<HudStatusTagKind, 3> tags{};
    std::uint8_t tag_count{};
    bool experience_maxed{};
};

enum class HudPaletteId : std::uint8_t { health, barrier, experience };

struct MonsterBarPlan final {
    bool visible{};
    float ratio{};
    HudPaletteId palette_id{HudPaletteId::health};
};

struct MonsterBarVisualPlan final {
    std::array<MonsterBarPlan, 3> bars{};
    bool world_space{true};
};

[[nodiscard]] PlayerPanelPlan make_player_panel_plan(
    const PlayerHudModel&, const HudLayout&, float presentation_seconds) noexcept;
[[nodiscard]] MonsterBarVisualPlan make_monster_bar_visual_plan(
    const combat::MonsterSnapshot&) noexcept;
[[nodiscard]] Color hud_palette_color(HudPaletteId palette_id) noexcept;

class HudRenderer final {
public:
    HudRenderer() noexcept = default;
    ~HudRenderer() noexcept;
    HudRenderer(const HudRenderer&) = delete;
    HudRenderer& operator=(const HudRenderer&) = delete;

    [[nodiscard]] bool initialize() noexcept;
    void shutdown() noexcept;
    [[nodiscard]] bool font_ready() const noexcept;
    void draw(const HudViewModel&, const HudLayout&) const noexcept;

private:
    Font font_{};
    bool font_ready_{};
};

}  // namespace arpg::platform
