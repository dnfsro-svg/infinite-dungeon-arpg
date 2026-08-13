#include "ground_loot_view.hpp"

#include "combat/room_bounds.hpp"
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

[[nodiscard]] bool obstacle_free(LootLabelRect candidate,
    const LootLabelObstacleSet& obstacles) noexcept {
    for (std::size_t index = 0U; index < obstacles.count; ++index) {
        if (overlaps(candidate, obstacles.rects[index])) return false;
    }
    return true;
}

[[nodiscard]] bool place_label(GroundLootLabel& label, float width,
    float height, const LootLabelObstacleSet& obstacles,
    GroundLootViewDiagnostics& diagnostics) noexcept {
    LootLabelRect original{label.anchor_x - kLabelWidth * 0.5F,
        label.anchor_y - kLabelAnchorGap - kLabelHeight,
        kLabelWidth, kLabelHeight};
    clamp_to_safety_area(original, width, height);
    if (obstacle_free(original, obstacles)) {
        label.rect = original;
        return true;
    }
    ++diagnostics.overlap_adjustment_count;

    const float viewport_width = usable_dimension(width);
    const float viewport_height = usable_dimension(height);
    const float minimum_x = kGroundLootSafetyInset;
    const float minimum_y = kGroundLootSafetyInset;
    const float maximum_x = viewport_width - kGroundLootSafetyInset
        - original.width;
    const float maximum_y = viewport_height - kGroundLootSafetyInset
        - original.height;
    const float x_span = (std::max)(0.0F, maximum_x - minimum_x);
    const float y_span = (std::max)(0.0F, maximum_y - minimum_y);
    const std::size_t columns = (std::max)(std::size_t{1U},
        static_cast<std::size_t>(
            std::floor(x_span / (original.width + kOverlapGap))) + 1U);
    const std::size_t rows = (std::max)(std::size_t{1U},
        static_cast<std::size_t>(
            std::floor(y_span / (original.height + kOverlapGap))) + 1U);

    bool found{};
    LootLabelRect best{};
    std::uint8_t best_priority{};
    float best_distance{};
    for (std::size_t row = 0U; row < rows; ++row) {
        const float y = rows == 1U ? minimum_y
            : minimum_y + y_span * static_cast<float>(row)
                / static_cast<float>(rows - 1U);
        for (std::size_t column = 0U; column < columns; ++column) {
            const float x = columns == 1U ? minimum_x
                : minimum_x + x_span * static_cast<float>(column)
                    / static_cast<float>(columns - 1U);
            const LootLabelRect candidate{x, y, original.width, original.height};
            if (!obstacle_free(candidate, obstacles)) continue;
            const std::uint8_t priority = candidate.y < original.y
                ? 0U : candidate.y > original.y ? 2U : 1U;
            const float dx = candidate.x - original.x;
            const float dy = candidate.y - original.y;
            const float distance = dx * dx + dy * dy;
            const bool better = !found || priority < best_priority
                || (priority == best_priority
                    && (distance < best_distance
                        || (distance == best_distance
                            && (candidate.y < best.y
                                || (candidate.y == best.y
                                    && candidate.x < best.x)))));
            if (!better) continue;
            found = true;
            best = candidate;
            best_priority = priority;
            best_distance = distance;
        }
    }
    if (found) label.rect = best;
    return found;
}

void layout_labels(GroundLootView& view, float width, float height,
    LootLabelObstacleSet& obstacles) noexcept {
    const std::size_t source_count = view.count;
    std::size_t retained_count{};
    for (std::size_t index = 0U; index < source_count; ++index) {
        GroundLootLabel label = view.labels[index];
        if (!place_label(label, width, height, obstacles, view.diagnostics)
                || !obstacles.append(label.rect)) {
            ++view.diagnostics.label_drop_count;
            continue;
        }
        view.labels[retained_count++] = label;
    }
    for (std::size_t index = retained_count; index < source_count; ++index) {
        view.labels[index] = {};
    }
    view.count = retained_count;
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

MaterialSpriteId ground_loot_item_sprite(items::ItemSlot slot) noexcept {
    switch (slot) {
    case items::ItemSlot::weapon: return MaterialSpriteId::item_weapon;
    case items::ItemSlot::helmet: return MaterialSpriteId::item_helmet;
    case items::ItemSlot::chest: return MaterialSpriteId::item_chest;
    case items::ItemSlot::gloves: return MaterialSpriteId::item_gloves;
    case items::ItemSlot::boots: return MaterialSpriteId::item_boots;
    case items::ItemSlot::accessory: return MaterialSpriteId::item_accessory;
    case items::ItemSlot::count: break;
    }
    return MaterialSpriteId::missing;
}

MaterialSpriteId ground_loot_rarity_sprite(
    items::ItemRarity rarity, bool abyss) noexcept {
    if (abyss) return MaterialSpriteId::loot_rarity_abyss;
    switch (rarity) {
    case items::ItemRarity::normal: return MaterialSpriteId::loot_rarity_normal;
    case items::ItemRarity::magic: return MaterialSpriteId::loot_rarity_magic;
    case items::ItemRarity::rare: return MaterialSpriteId::loot_rarity_rare;
    }
    return MaterialSpriteId::missing;
}

Rgba8 ground_loot_rarity_color(
    items::ItemRarity rarity, bool abyss) noexcept {
    return abyss ? kAbyssBorderColor : rarity_color(rarity);
}

GroundLootView build_ground_loot_view(
    const dungeon::DungeonRenderSnapshot& snapshot,
    settings::LootFilterMode mode,
    const CombatCameraView& camera,
    float width,
    float height) noexcept {
    LootLabelObstacleSet obstacles{};
    return build_ground_loot_view(
        snapshot, mode, camera, width, height, obstacles);
}

GroundLootView build_ground_loot_view(
    const dungeon::DungeonRenderSnapshot& snapshot,
    settings::LootFilterMode mode,
    const CombatCameraView& camera,
    float width,
    float height,
    LootLabelObstacleSet& obstacles) noexcept {
    GroundLootView view{};
    const std::size_t source_count = (std::min)(
        static_cast<std::size_t>(snapshot.equipment_count),
        snapshot.equipment.size());
    view.diagnostics.capacity_saturation_count =
        static_cast<std::uint32_t>(snapshot.equipment_count - source_count);

    for (std::size_t index = 0U; index < source_count; ++index) {
        const dungeon::GroundItemSnapshot& item = snapshot.equipment[index];
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
        label.item_sprite = ground_loot_item_sprite(item.slot);
        label.rarity_sprite = ground_loot_rarity_sprite(
            item.rarity, label.abyss);
        const ScreenProjection projection =
            project_combat_position(item.position, camera, width, height);
        label.anchor_x = projection.x;
        label.anchor_y = projection.y;
        format_label_text(label, item, view.diagnostics);
        insert_by_ordinal(view, label);
    }

    layout_labels(view, width, height, obstacles);
    return view;
}

GroundLootView build_ground_loot_view(
    const dungeon::DungeonSnapshot& snapshot,
    settings::LootFilterMode mode,
    const CombatCameraView& camera,
    float width,
    float height) noexcept {
    LootLabelObstacleSet obstacles{};
    return build_ground_loot_view(
        snapshot, mode, camera, width, height, obstacles);
}

GroundLootView build_ground_loot_view(
    const dungeon::DungeonSnapshot& snapshot,
    settings::LootFilterMode mode,
    const CombatCameraView& camera,
    float width,
    float height,
    LootLabelObstacleSet& obstacles) noexcept {
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
        label.item_sprite = ground_loot_item_sprite(item.slot);
        label.rarity_sprite = ground_loot_rarity_sprite(
            item.rarity, label.abyss);
        const ScreenProjection projection =
            project_combat_position(item.position, camera, width, height);
        label.anchor_x = projection.x;
        label.anchor_y = projection.y;
        format_label_text(label, item, view.diagnostics);
        insert_by_ordinal(view, label);
    }

    layout_labels(view, width, height, obstacles);
    return view;
}

GroundLootView build_ground_loot_view(
    const dungeon::DungeonSnapshot& snapshot,
    settings::LootFilterMode mode,
    float width,
    float height) noexcept {
    LootLabelObstacleSet obstacles{};
    return build_ground_loot_view(
        snapshot, mode, width, height, obstacles);
}

GroundLootView build_ground_loot_view(
    const dungeon::DungeonSnapshot& snapshot,
    settings::LootFilterMode mode,
    float width,
    float height,
    LootLabelObstacleSet& obstacles) noexcept {
    const CombatCameraView full_room_camera{
        {}, combat::room_bounds::width, combat::room_bounds::depth};
    return build_ground_loot_view(
        snapshot, mode, full_room_camera, width, height, obstacles);
}

}  // namespace arpg::platform
