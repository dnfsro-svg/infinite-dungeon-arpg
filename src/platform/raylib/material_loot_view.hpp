#pragma once

#include "ground_loot_view.hpp"
#include "hud_view_model.hpp"
#include "items/material_catalog.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace arpg::platform {

struct MaterialLootLabel final {
    std::uint16_t ordinal{};
    float anchor_x{};
    float anchor_y{};
    LootLabelRect rect{};
    Rgba8 text_color{};
    bool emphasized{};
    std::array<char, kGroundLootTextCapacity> text{};
};

struct MaterialLootView final {
    std::array<MaterialLootLabel, dungeon::kGroundMaterialCapacity> labels{};
    std::size_t count{};
    std::uint32_t invalid_material_count{};
    std::uint32_t capacity_saturation_count{};
};

struct MaterialPickupFeedback final {
    bool ready{};
    bool emphasized{};
    HudText96 text{};
};

class MaterialPickupFeedbackState final {
public:
    void update(float frame_seconds, bool paused) noexcept;
    [[nodiscard]] MaterialPickupFeedback observe(
        const dungeon::DungeonSnapshot&) noexcept;

private:
    std::array<std::uint64_t, items::kMaterialCount> accumulated_{};
    std::uint64_t generation_{};
    float seconds_left_{};
    bool baseline_set_{};
};

[[nodiscard]] Rgba8 material_color(items::MaterialId) noexcept;
[[nodiscard]] std::string_view material_label(items::MaterialId) noexcept;
[[nodiscard]] bool material_is_emphasized(items::MaterialId) noexcept;
[[nodiscard]] MaterialLootView build_material_loot_view(
    const dungeon::DungeonSnapshot&, CombatCameraView view,
    float width, float height) noexcept;
[[nodiscard]] MaterialLootView build_material_loot_view(
    const dungeon::DungeonSnapshot&, float width, float height) noexcept;

}  // namespace arpg::platform
