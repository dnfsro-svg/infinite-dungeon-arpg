#pragma once

#include "ground_loot_view.hpp"
#include "hud_view_model.hpp"
#include "items/material_catalog.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace arpg::platform {

enum class SecondaryLootKind : std::uint8_t {
    material,
    health_potion,
};

struct MaterialLootLabel final {
    SecondaryLootKind kind{SecondaryLootKind::material};
    std::uint16_t ordinal{};
    float anchor_x{};
    float anchor_y{};
    LootLabelRect rect{};
    Rgba8 text_color{};
    MaterialSpriteId sprite{MaterialSpriteId::missing};
    bool emphasized{};
    // Label text is selected exclusively from static string literals.
    std::string_view text{};
};

struct MaterialLootView final {
    std::array<MaterialLootLabel,
        dungeon::kGroundMaterialCapacity + dungeon::kGroundHealthPotionCapacity>
        labels{};
    std::size_t count{};
    std::uint32_t invalid_material_count{};
    std::uint32_t capacity_saturation_count{};
    std::uint32_t label_drop_count{};
};

struct MaterialLootPlacementDiagnostics final {
    std::uint64_t direct_collision_check_count{};
    std::uint64_t candidate_probe_count{};
    std::uint64_t occupancy_mark_check_count{};
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
    enum class PendingFeedbackKind : std::uint8_t {
        material,
        health_potion,
    };

    struct PendingFeedback final {
        MaterialPickupFeedback value{};
        PendingFeedbackKind kind{PendingFeedbackKind::material};
    };

    static constexpr std::size_t kPendingFeedbackCapacity = 2U;
    void enqueue_feedback(
        MaterialPickupFeedback, PendingFeedbackKind) noexcept;
    [[nodiscard]] MaterialPickupFeedback publish_next() noexcept;

    std::array<std::uint64_t, items::kMaterialCount> accumulated_{};
    std::array<PendingFeedback, kPendingFeedbackCapacity> pending_feedback_{};
    std::size_t pending_feedback_count_{};
    std::uint64_t generation_{};
    std::uint64_t health_potion_generation_{};
    float seconds_left_{};
    bool baseline_set_{};
};

[[nodiscard]] Rgba8 material_color(items::MaterialId) noexcept;
[[nodiscard]] std::string_view material_label(items::MaterialId) noexcept;
[[nodiscard]] bool material_is_emphasized(items::MaterialId) noexcept;
[[nodiscard]] MaterialSpriteId material_loot_sprite(
    items::MaterialId) noexcept;
[[nodiscard]] bool loot_label_rects_overlap(
    LootLabelRect left, LootLabelRect right) noexcept;
[[nodiscard]] MaterialLootView build_material_loot_view(
    const dungeon::DungeonSnapshot&, float width, float height) noexcept;
[[nodiscard]] MaterialLootView build_material_loot_view(
    const dungeon::DungeonSnapshot&, float width, float height,
    LootLabelObstacleSet&) noexcept;
[[nodiscard]] MaterialLootView build_material_loot_view_with_diagnostics(
    const dungeon::DungeonSnapshot&, float width, float height,
    MaterialLootPlacementDiagnostics&) noexcept;
[[nodiscard]] MaterialLootView build_material_loot_view_with_diagnostics(
    const dungeon::DungeonSnapshot&, float width, float height,
    MaterialLootPlacementDiagnostics&, LootLabelObstacleSet&) noexcept;

}  // namespace arpg::platform
