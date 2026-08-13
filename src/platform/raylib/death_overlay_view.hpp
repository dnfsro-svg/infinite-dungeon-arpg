#pragma once

#include "dungeon/dungeon_types.hpp"
#include "material_asset_types.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace arpg::platform {

inline constexpr std::size_t kDeathOverlayLineCapacity = 26U;
inline constexpr std::size_t kDeathOverlayTextCapacity = 160U;

enum class DeathOverlayColumn : unsigned char {
    full,
    left,
    right,
};

struct DeathOverlayLine final {
    std::array<char, kDeathOverlayTextCapacity> text{};
    DeathOverlayColumn column{DeathOverlayColumn::full};
    bool heading{};
};

struct DeathOverlayView final {
    bool visible{};
    std::array<char, 32U> title{};
    std::array<DeathOverlayLine, kDeathOverlayLineCapacity> lines{};
    std::size_t line_count{};
    std::array<char, 64U> prompt{};
};

struct DeathOverlayRect final {
    float x{};
    float y{};
    float width{};
    float height{};
};

struct DeathOverlayLayout final {
    DeathOverlayRect panel{};
    DeathOverlayRect title{};
    std::array<DeathOverlayRect, kDeathOverlayLineCapacity> line_bounds{};
    DeathOverlayRect prompt{};
    int title_font_size{};
    int heading_font_size{};
    int body_font_size{};
    int prompt_font_size{};
};

struct DeathOverlayMaterialPlan final {
    bool visible{};
    MaterialSpriteId panel{MaterialSpriteId::ui_warning_modal};
    MaterialSpriteId title_plate{MaterialSpriteId::ui_label_plate};
    float panel_border_pixels{32.0F};
    std::uint8_t dimmer_alpha{232U};
};

[[nodiscard]] DeathOverlayView build_death_overlay_view(
    const dungeon::DungeonSnapshot& snapshot) noexcept;

[[nodiscard]] DeathOverlayView build_death_overlay_ascii_view(
    const dungeon::DungeonSnapshot& snapshot) noexcept;

[[nodiscard]] DeathOverlayLayout death_overlay_layout(
    const DeathOverlayView& view,
    int screen_width,
    int screen_height) noexcept;

[[nodiscard]] DeathOverlayMaterialPlan death_overlay_material_plan(
    bool visible) noexcept;

}  // namespace arpg::platform
