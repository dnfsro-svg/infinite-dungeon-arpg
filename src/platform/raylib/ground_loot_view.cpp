#include "ground_loot_view.hpp"

#include "combat_view_math.hpp"
#include "items/item_catalog.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace arpg::platform {
namespace {

constexpr float kLabelWidth = 220.0F;
constexpr float kLabelHeight = 24.0F;
constexpr float kLabelAnchorGap = 14.0F;
constexpr float kOverlapGap = 4.0F;
constexpr Rgba8 kNormalColor{232U, 232U, 232U, 255U};
constexpr Rgba8 kMagicColor{96U, 170U, 255U, 255U};
constexpr Rgba8 kRareColor{255U, 205U, 70U, 255U};
constexpr Rgba8 kAbyssBorderColor{184U, 96U, 255U, 255U};

[[nodiscard]] const char* rarity_name(items::ItemRarity rarity) noexcept {
    switch (rarity) {
    case items::ItemRarity::normal: return "普通";
    case items::ItemRarity::magic: return "魔法";
    case items::ItemRarity::rare: return "稀有";
    }
    return "普通";
}

[[nodiscard]] Rgba8 rarity_color(items::ItemRarity rarity) noexcept {
    switch (rarity) {
    case items::ItemRarity::normal: return kNormalColor;
    case items::ItemRarity::magic: return kMagicColor;
    case items::ItemRarity::rare: return kRareColor;
    }
    return kNormalColor;
}

[[nodiscard]] bool overlaps(LootLabelRect lhs, LootLabelRect rhs) noexcept {
    return lhs.x < rhs.x + rhs.width && rhs.x < lhs.x + lhs.width
        && lhs.y < rhs.y + rhs.height && rhs.y < lhs.y + lhs.height;
}

void format_label_text(GroundLootLabel& label,
    const dungeon::GroundItemSnapshot& item,
    GroundLootViewDiagnostics& diagnostics) noexcept {
    const items::BaseDefinition* const base =
        items::base_definition(item.base_id);
    const char* const rarity = rarity_name(item.rarity);
    const char* const base_name = base == nullptr
        ? "未知装备" : base->name.data();
    const int base_name_length = base == nullptr
        ? static_cast<int>(sizeof("未知装备") - 1U)
        : static_cast<int>(base->name.size());
    if (base == nullptr) {
        ++diagnostics.invalid_base_count;
    }

    const int written = std::snprintf(label.text.data(), label.text.size(),
        "%s %.*s · i%u", rarity, base_name_length, base_name,
        static_cast<unsigned>(item.item_level));
    if (written < 0
            || static_cast<std::size_t>(written) >= label.text.size()) {
        ++diagnostics.text_truncation_count;
        static_cast<void>(std::snprintf(label.text.data(), label.text.size(),
            "%s #%u · i%u", rarity, static_cast<unsigned>(item.base_id),
            static_cast<unsigned>(item.item_level)));
    }
    label.text.back() = '\0';
}

void insert_by_ordinal(GroundLootView& view,
    GroundLootLabel label) noexcept {
    std::size_t insertion = view.count;
    while (insertion > 0U
            && label.ordinal < view.labels[insertion - 1U].ordinal) {
        view.labels[insertion] = view.labels[insertion - 1U];
        --insertion;
    }
    view.labels[insertion] = label;
    ++view.count;
}

[[nodiscard]] float usable_dimension(float value) noexcept {
    return std::isfinite(value) && value > kGroundLootSafetyInset * 2.0F
        ? value : kGroundLootSafetyInset * 2.0F;
}

void clamp_to_safety_area(LootLabelRect& rect,
    float width,
    float height) noexcept {
    const float viewport_width = usable_dimension(width);
    const float viewport_height = usable_dimension(height);
    rect.width = (std::min)(rect.width,
        viewport_width - kGroundLootSafetyInset * 2.0F);
    rect.height = (std::min)(rect.height,
        viewport_height - kGroundLootSafetyInset * 2.0F);
    rect.x = std::clamp(rect.x, kGroundLootSafetyInset,
        viewport_width - kGroundLootSafetyInset - rect.width);
    rect.y = std::clamp(rect.y, kGroundLootSafetyInset,
        viewport_height - kGroundLootSafetyInset - rect.height);
}

void layout_labels(GroundLootView& view,
    float width,
    float height) noexcept {
    for (std::size_t index = 0U; index < view.count; ++index) {
        GroundLootLabel& label = view.labels[index];
        LootLabelRect& rect = label.rect;
        rect = {label.anchor_x - kLabelWidth * 0.5F,
            label.anchor_y - kLabelAnchorGap - kLabelHeight,
            kLabelWidth, kLabelHeight};

        for (std::size_t attempt = 0U;
             attempt < dungeon::kGroundDropCapacity; ++attempt) {
            bool collision = false;
            for (std::size_t previous = 0U; previous < index; ++previous) {
                if (overlaps(rect, view.labels[previous].rect)) {
                    collision = true;
                    break;
                }
            }
            if (!collision) break;
            rect.y -= kLabelHeight + kOverlapGap;
            ++view.diagnostics.overlap_adjustment_count;
        }
        clamp_to_safety_area(rect, width, height);
    }
}

}  // namespace

bool ground_loot_visible(const dungeon::GroundItemSnapshot& item,
    settings::LootFilterMode mode) noexcept {
    if (item.source == dungeon::GroundItemSource::abyss_chest) {
        return true;
    }
    switch (mode) {
    case settings::LootFilterMode::show_all:
        return true;
    case settings::LootFilterMode::magic_or_better:
        return item.rarity == items::ItemRarity::magic
            || item.rarity == items::ItemRarity::rare;
    case settings::LootFilterMode::rare_only:
        return item.rarity == items::ItemRarity::rare;
    }
    return true;
}

GroundLootView build_ground_loot_view(
    const dungeon::DungeonSnapshot& snapshot,
    settings::LootFilterMode mode,
    CombatCameraView camera,
    float width,
    float height) noexcept {
    GroundLootView view{};
    const std::size_t source_count = (std::min)(
        static_cast<std::size_t>(snapshot.ground_item_count),
        dungeon::kGroundDropCapacity);
    view.diagnostics.capacity_saturation_count =
        static_cast<std::uint32_t>(snapshot.ground_item_count - source_count);

    for (std::size_t index = 0U; index < source_count; ++index) {
        const dungeon::GroundItemSnapshot& item = snapshot.ground_items[index];
        if (!ground_loot_visible(item, mode)) continue;
        if (view.count == view.labels.size()) {
            ++view.diagnostics.capacity_saturation_count;
            continue;
        }

        GroundLootLabel label{};
        label.ordinal = item.ordinal;
        label.abyss = item.source == dungeon::GroundItemSource::abyss_chest;
        label.text_color = rarity_color(item.rarity);
        label.border_color = label.abyss
            ? kAbyssBorderColor : label.text_color;
        const ScreenProjection projection =
            project_combat_position(item.position, camera, width, height);
        label.anchor_x = projection.x;
        label.anchor_y = projection.y;
        format_label_text(label, item, view.diagnostics);
        insert_by_ordinal(view, label);
    }

    layout_labels(view, width, height);
    return view;
}

GroundLootView build_ground_loot_view(
    const dungeon::DungeonSnapshot& snapshot,
    settings::LootFilterMode mode,
    float width,
    float height) noexcept {
    return build_ground_loot_view(snapshot, mode,
        make_combat_camera_view({}, width, height), width, height);
}

}  // namespace arpg::platform
