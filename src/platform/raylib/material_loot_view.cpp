#include "material_loot_view.hpp"

#include "combat_view_math.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>

namespace arpg::platform {
namespace {

constexpr float kLabelWidth = 190.0F;
constexpr float kLabelHeight = 23.0F;
constexpr float kLabelGap = 13.0F;
constexpr float kPlacementGap = 3.0F;
constexpr float kAnchorExclusionSize = 40.0F;
constexpr float kAnchorExclusionHalfSize = kAnchorExclusionSize * 0.5F;
constexpr std::size_t kLabelCapacity = dungeon::kGroundMaterialCapacity
    + dungeon::kGroundHealthPotionCapacity;
constexpr std::size_t kOccupancyWordBits = 64U;
constexpr std::size_t kOccupancyWordCount =
    (kLabelCapacity + kOccupancyWordBits - 1U) / kOccupancyWordBits;

[[nodiscard]] float usable_screen_extent(float extent) noexcept {
    return std::isfinite(extent)
        ? (std::max)(kGroundLootSafetyInset * 2.0F, extent)
        : kGroundLootSafetyInset * 2.0F;
}

void clamp_rect(LootLabelRect& rect, float width, float height) noexcept {
    const float usable_width = usable_screen_extent(width);
    const float usable_height = usable_screen_extent(height);
    rect.width = (std::min)(rect.width, usable_width - kGroundLootSafetyInset * 2.0F);
    rect.height = (std::min)(rect.height, usable_height - kGroundLootSafetyInset * 2.0F);
    rect.x = std::clamp(rect.x, kGroundLootSafetyInset,
        usable_width - kGroundLootSafetyInset - rect.width);
    rect.y = std::clamp(rect.y, kGroundLootSafetyInset,
        usable_height - kGroundLootSafetyInset - rect.height);
}

void insert_label(MaterialLootView& view, MaterialLootLabel label) noexcept {
    std::size_t insertion = view.count;
    while (insertion > 0U && label.ordinal < view.labels[insertion - 1U].ordinal) {
        view.labels[insertion] = view.labels[insertion - 1U];
        --insertion;
    }
    view.labels[insertion] = label;
    ++view.count;
}

struct PlacementGrid final {
    std::array<LootLabelRect, kLabelCapacity> slots{};
    std::size_t count{};
};

struct LootAnchor final {
    float x{};
    float y{};
};

struct LootAnchorSet final {
    std::array<LootAnchor, kLabelCapacity> anchors{};
    std::size_t count{};
};

[[nodiscard]] LootAnchorSet make_anchor_set(
    const MaterialLootView& view) noexcept {
    LootAnchorSet set{};
    set.count = view.count;
    for (std::size_t index = 0U; index < view.count; ++index) {
        set.anchors[index] = {
            view.labels[index].anchor_x, view.labels[index].anchor_y};
    }
    return set;
}

[[nodiscard]] LootLabelRect anchor_exclusion_rect(
    LootAnchor anchor) noexcept {
    return {anchor.x - kAnchorExclusionHalfSize,
        anchor.y - kAnchorExclusionHalfSize,
        kAnchorExclusionSize, kAnchorExclusionSize};
}

[[nodiscard]] bool same_anchor(
    const MaterialLootLabel& label, LootAnchor anchor) noexcept {
    return label.anchor_x == anchor.x && label.anchor_y == anchor.y;
}

[[nodiscard]] bool overlaps_distinct_anchor(
    LootLabelRect candidate, const MaterialLootLabel& label,
    const LootAnchorSet& anchors) noexcept {
    for (std::size_t index = 0U; index < anchors.count; ++index) {
        const LootAnchor anchor = anchors.anchors[index];
        if (same_anchor(label, anchor)) continue;
        if (loot_label_rects_overlap(
                candidate, anchor_exclusion_rect(anchor))) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] std::size_t axis_slot_count(
    float span, float minimum_step) noexcept {
    std::size_t count = 1U;
    while (count < kLabelCapacity
            && minimum_step * static_cast<float>(count) <= span) {
        ++count;
    }
    return count;
}

[[nodiscard]] float axis_slot_position(float minimum, float maximum,
    std::size_t index, std::size_t count) noexcept {
    if (count == 1U) return minimum + (maximum - minimum) * 0.5F;
    return minimum + (maximum - minimum)
        * (static_cast<float>(index) / static_cast<float>(count - 1U));
}

void compress_axis_counts(std::size_t natural_columns,
    std::size_t natural_rows, std::size_t& columns,
    std::size_t& rows) noexcept {
    columns = natural_columns;
    rows = natural_rows;
    if (natural_rows == 0U
            || natural_columns <= kLabelCapacity / natural_rows) {
        return;
    }

    const float scale = std::sqrt(
        static_cast<float>(kLabelCapacity)
        / static_cast<float>(natural_columns * natural_rows));
    columns = (std::max)(std::size_t{1U},
        static_cast<std::size_t>(
            std::floor(static_cast<float>(natural_columns) * scale)));
    rows = (std::max)(std::size_t{1U},
        static_cast<std::size_t>(
            std::floor(static_cast<float>(natural_rows) * scale)));

    for (;;) {
        const bool can_add_column = columns < natural_columns
            && columns + 1U <= kLabelCapacity / rows;
        const bool can_add_row = rows < natural_rows
            && columns <= kLabelCapacity / (rows + 1U);
        if (!can_add_column && !can_add_row) return;
        if (can_add_column && can_add_row) {
            const float column_fraction = static_cast<float>(columns)
                / static_cast<float>(natural_columns);
            const float row_fraction = static_cast<float>(rows)
                / static_cast<float>(natural_rows);
            if (column_fraction <= row_fraction) {
                ++columns;
            } else {
                ++rows;
            }
        } else if (can_add_column) {
            ++columns;
        } else {
            ++rows;
        }
    }
}

void append_unique_slot(PlacementGrid& grid, LootLabelRect slot) noexcept {
    for (std::size_t index = 0U; index < grid.count; ++index) {
        const LootLabelRect existing = grid.slots[index];
        if (existing.x == slot.x && existing.y == slot.y
                && existing.width == slot.width
                && existing.height == slot.height) {
            return;
        }
    }
    if (grid.count < grid.slots.size()) {
        grid.slots[grid.count++] = slot;
    }
}

[[nodiscard]] PlacementGrid make_placement_grid(
    const MaterialLootView& view, float width, float height) noexcept {
    PlacementGrid grid{};
    if (view.count == 0U) return grid;

    const float label_width = view.labels[0].rect.width;
    const float label_height = view.labels[0].rect.height;
    const float minimum_x = kGroundLootSafetyInset;
    const float minimum_y = kGroundLootSafetyInset;
    const float maximum_x = usable_screen_extent(width)
        - kGroundLootSafetyInset - label_width;
    const float maximum_y = usable_screen_extent(height)
        - kGroundLootSafetyInset - label_height;
    const std::size_t natural_column_count = axis_slot_count(
        maximum_x - minimum_x, label_width + kPlacementGap);
    const std::size_t natural_row_count = axis_slot_count(
        maximum_y - minimum_y, label_height + kPlacementGap);
    std::size_t column_count{};
    std::size_t row_count{};
    compress_axis_counts(natural_column_count, natural_row_count,
        column_count, row_count);
    for (std::size_t row = 0U;
         row < row_count; ++row) {
        const float y = axis_slot_position(
            minimum_y, maximum_y, row, row_count);
        for (std::size_t column = 0U;
             column < column_count; ++column) {
            const float x = axis_slot_position(
                minimum_x, maximum_x, column, column_count);
            append_unique_slot(
                grid, {x, y, label_width, label_height});
        }
    }
    return grid;
}

[[nodiscard]] bool occupancy_test(
    const std::array<std::uint64_t, kOccupancyWordCount>& occupancy,
    std::size_t slot) noexcept {
    return (occupancy[slot / kOccupancyWordBits]
        & (std::uint64_t{1U} << (slot % kOccupancyWordBits))) != 0U;
}

void occupancy_set(std::array<std::uint64_t, kOccupancyWordCount>& occupancy,
    std::size_t slot) noexcept {
    occupancy[slot / kOccupancyWordBits] |=
        std::uint64_t{1U} << (slot % kOccupancyWordBits);
}

[[nodiscard]] bool overlaps_obstacles(const LootLabelObstacleSet& obstacles,
    LootLabelRect candidate,
    MaterialLootPlacementDiagnostics* diagnostics) noexcept {
    for (std::size_t index = 0U; index < obstacles.count; ++index) {
        if (diagnostics != nullptr) {
            ++diagnostics->direct_collision_check_count;
        }
        if (loot_label_rects_overlap(candidate, obstacles.rects[index])) {
            return true;
        }
    }
    return false;
}

void mark_intersecting_slots(const PlacementGrid& grid,
    LootLabelRect placed,
    std::array<std::uint64_t, kOccupancyWordCount>& occupancy,
    MaterialLootPlacementDiagnostics* diagnostics) noexcept {
    for (std::size_t slot = 0U; slot < grid.count; ++slot) {
        if (diagnostics != nullptr) {
            ++diagnostics->occupancy_mark_check_count;
        }
        if (loot_label_rects_overlap(placed, grid.slots[slot])) {
            occupancy_set(occupancy, slot);
        }
    }
}

void mark_anchor_exclusion_slots(const PlacementGrid& grid,
    const LootAnchorSet& anchors,
    std::array<std::uint64_t, kOccupancyWordCount>& occupancy) noexcept {
    for (std::size_t anchor_index = 0U;
         anchor_index < anchors.count; ++anchor_index) {
        const LootLabelRect exclusion = anchor_exclusion_rect(
            anchors.anchors[anchor_index]);
        for (std::size_t slot = 0U; slot < grid.count; ++slot) {
            if (loot_label_rects_overlap(exclusion, grid.slots[slot])) {
                occupancy_set(occupancy, slot);
            }
        }
    }
}

[[nodiscard]] bool try_preferred_upward(MaterialLootLabel& label,
    const LootLabelObstacleSet& obstacles,
    const LootAnchorSet& anchors, float width, float height,
    MaterialLootPlacementDiagnostics* diagnostics) noexcept {
    LootLabelRect candidate = label.rect;
    const float minimum_upward_step = label.rect.height + kPlacementGap;
    candidate.y -= minimum_upward_step;
    clamp_rect(candidate, width, height);
    if (candidate.y > label.rect.y - minimum_upward_step) return false;
    if (overlaps_obstacles(obstacles, candidate, diagnostics)) {
        return false;
    }
    if (overlaps_distinct_anchor(candidate, label, anchors)) return false;
    label.rect = candidate;
    return true;
}

[[nodiscard]] std::uint8_t candidate_direction_priority(
    LootLabelRect original, LootLabelRect candidate) noexcept {
    const float vertical_step = original.height + kPlacementGap;
    if (candidate.y <= original.y - vertical_step) return 0U;
    if (candidate.y >= original.y + vertical_step) return 2U;
    return 1U;
}

[[nodiscard]] bool place_in_fixed_grid(MaterialLootLabel& label,
    const PlacementGrid& grid,
    const std::array<std::uint64_t, kOccupancyWordCount>& occupancy,
    MaterialLootPlacementDiagnostics* diagnostics) noexcept {
    std::size_t best_slot = grid.count;
    std::uint8_t best_priority{};
    float best_distance{};
    for (std::size_t slot = 0U; slot < grid.count; ++slot) {
        if (diagnostics != nullptr) {
            ++diagnostics->candidate_probe_count;
        }
        if (occupancy_test(occupancy, slot)) continue;
        const LootLabelRect candidate = grid.slots[slot];
        const std::uint8_t priority = candidate_direction_priority(
            label.rect, candidate);
        const float x_distance = candidate.x - label.rect.x;
        const float y_distance = candidate.y - label.rect.y;
        const float distance = x_distance * x_distance
            + y_distance * y_distance;
        const bool better = best_slot == grid.count
            || priority < best_priority
            || (priority == best_priority
                && (distance < best_distance
                    || (distance == best_distance
                        && (candidate.y < grid.slots[best_slot].y
                            || (candidate.y == grid.slots[best_slot].y
                                && candidate.x
                                    < grid.slots[best_slot].x)))));
        if (!better) continue;
        best_slot = slot;
        best_priority = priority;
        best_distance = distance;
    }
    if (best_slot == grid.count) return false;
    label.rect = grid.slots[best_slot];
    return true;
}

void resolve_label_overlaps(MaterialLootView& view,
    float width, float height, LootLabelObstacleSet& obstacles,
    MaterialLootPlacementDiagnostics* diagnostics) noexcept {
    const PlacementGrid grid = make_placement_grid(view, width, height);
    const LootAnchorSet anchors = make_anchor_set(view);
    std::array<std::uint64_t, kOccupancyWordCount> occupancy{};
    mark_anchor_exclusion_slots(grid, anchors, occupancy);
    for (std::size_t index = 0U; index < obstacles.count; ++index) {
        mark_intersecting_slots(
            grid, obstacles.rects[index], occupancy, diagnostics);
    }
    const std::size_t source_count = view.count;
    std::size_t retained_count{};
    for (std::size_t index = 0U; index < source_count; ++index) {
        MaterialLootLabel label = view.labels[index];
        bool placed = !overlaps_obstacles(obstacles, label.rect, diagnostics)
            && !overlaps_distinct_anchor(label.rect, label, anchors);
        if (!placed) {
            placed = try_preferred_upward(label, obstacles,
                anchors, width, height, diagnostics);
        }
        if (!placed) {
            placed = place_in_fixed_grid(
                label, grid, occupancy, diagnostics);
        }
        if (!placed || !obstacles.append(label.rect)) {
            ++view.label_drop_count;
            continue;
        }
        view.labels[retained_count++] = label;
        mark_intersecting_slots(
            grid, label.rect, occupancy, diagnostics);
    }
    for (std::size_t index = retained_count; index < source_count; ++index) {
        view.labels[index] = {};
    }
    view.count = retained_count;
}

}  // namespace

bool loot_label_rects_overlap(
    LootLabelRect left, LootLabelRect right) noexcept {
    return left.x < right.x + right.width
        && left.x + left.width > right.x
        && left.y < right.y + right.height
        && left.y + left.height > right.y;
}

Rgba8 material_color(items::MaterialId id) noexcept {
    switch (id) {
    case items::MaterialId::transmute: return {100U, 180U, 255U, 255U};
    case items::MaterialId::augment: return {160U, 115U, 255U, 255U};
    case items::MaterialId::regal: return {255U, 196U, 77U, 255U};
    case items::MaterialId::chaos: return {224U, 84U, 160U, 255U};
    case items::MaterialId::exalt: return {255U, 190U, 56U, 255U};
    case items::MaterialId::annul: return {132U, 222U, 218U, 255U};
    case items::MaterialId::divine: return {255U, 122U, 103U, 255U};
    case items::MaterialId::scour: return {173U, 182U, 197U, 255U};
    case items::MaterialId::directed: return {68U, 236U, 186U, 255U};
    case items::MaterialId::reinforcement_stone: return {212U, 222U, 235U, 255U};
    case items::MaterialId::coupon_6: return {122U, 207U, 255U, 255U};
    case items::MaterialId::coupon_9: return {188U, 125U, 255U, 255U};
    case items::MaterialId::coupon_12: return {255U, 202U, 66U, 255U};
    case items::MaterialId::coupon_15: return {255U, 89U, 89U, 255U};
    case items::MaterialId::count: break;
    }
    return {220U, 220U, 220U, 255U};
}

std::string_view material_label(items::MaterialId id) noexcept {
    switch (id) {
    case items::MaterialId::transmute: return "蜕变石";
    case items::MaterialId::augment: return "增幅石";
    case items::MaterialId::regal: return "富豪石";
    case items::MaterialId::chaos: return "混沌石";
    case items::MaterialId::exalt: return "崇高石";
    case items::MaterialId::annul: return "剥离石";
    case items::MaterialId::divine: return "神圣石";
    case items::MaterialId::scour: return "重铸石";
    case items::MaterialId::directed: return "定向核心";
    case items::MaterialId::reinforcement_stone: return "强化石";
    case items::MaterialId::coupon_6: return "+6强化券";
    case items::MaterialId::coupon_9: return "+9强化券";
    case items::MaterialId::coupon_12: return "+12强化券";
    case items::MaterialId::coupon_15: return "+15强化券";
    case items::MaterialId::count: break;
    }
    return "未知材料";
}

bool material_is_emphasized(items::MaterialId id) noexcept {
    return id == items::MaterialId::exalt || id == items::MaterialId::directed
        || id == items::MaterialId::coupon_12 || id == items::MaterialId::coupon_15;
}

MaterialSpriteId material_loot_sprite(items::MaterialId id) noexcept {
    switch (id) {
    case items::MaterialId::transmute: return MaterialSpriteId::material_transmute;
    case items::MaterialId::augment: return MaterialSpriteId::material_augment;
    case items::MaterialId::regal: return MaterialSpriteId::material_regal;
    case items::MaterialId::chaos: return MaterialSpriteId::material_chaos;
    case items::MaterialId::exalt: return MaterialSpriteId::material_exalt;
    case items::MaterialId::annul: return MaterialSpriteId::material_annul;
    case items::MaterialId::divine: return MaterialSpriteId::material_divine;
    case items::MaterialId::scour: return MaterialSpriteId::material_scour;
    case items::MaterialId::directed: return MaterialSpriteId::material_directed;
    case items::MaterialId::reinforcement_stone:
        return MaterialSpriteId::material_reinforcement;
    case items::MaterialId::coupon_6: return MaterialSpriteId::material_coupon_6;
    case items::MaterialId::coupon_9: return MaterialSpriteId::material_coupon_9;
    case items::MaterialId::coupon_12: return MaterialSpriteId::material_coupon_12;
    case items::MaterialId::coupon_15: return MaterialSpriteId::material_coupon_15;
    case items::MaterialId::count: break;
    }
    return MaterialSpriteId::missing;
}

namespace {

MaterialLootView build_material_loot_view_internal(
    const dungeon::DungeonSnapshot& snapshot, float width, float height,
    MaterialLootPlacementDiagnostics* diagnostics,
    LootLabelObstacleSet& obstacles) noexcept {
    MaterialLootView view{};
    const std::size_t source_count = (std::min)(
        static_cast<std::size_t>(snapshot.ground_material_count),
        snapshot.ground_materials.size());
    view.capacity_saturation_count = static_cast<std::uint32_t>(
        static_cast<std::size_t>(snapshot.ground_material_count) - source_count);
    for (std::size_t index = 0U; index < source_count; ++index) {
        const dungeon::GroundMaterialSnapshot& material = snapshot.ground_materials[index];
        if (items::material_definition(material.material) == nullptr) {
            ++view.invalid_material_count;
            continue;
        }
        if (view.count == view.labels.size()) {
            ++view.capacity_saturation_count;
            continue;
        }
        const ScreenProjection projection = project_combat_position(
            material.position, width, height);
        MaterialLootLabel label{};
        label.kind = SecondaryLootKind::material;
        label.ordinal = material.ordinal;
        label.anchor_x = projection.x;
        label.anchor_y = projection.y;
        label.rect = {projection.x - kLabelWidth * 0.5F,
            projection.y - kLabelGap - kLabelHeight, kLabelWidth, kLabelHeight};
        clamp_rect(label.rect, width, height);
        label.text_color = material_color(material.material);
        label.sprite = material_loot_sprite(material.material);
        label.emphasized = material_is_emphasized(material.material);
        label.text = material_label(material.material);
        insert_label(view, label);
    }
    const std::size_t potion_source_count = (std::min)(
        static_cast<std::size_t>(snapshot.ground_health_potion_count),
        snapshot.ground_health_potions.size());
    view.capacity_saturation_count += static_cast<std::uint32_t>(
        static_cast<std::size_t>(snapshot.ground_health_potion_count)
        - potion_source_count);
    for (std::size_t index = 0U; index < potion_source_count; ++index) {
        if (view.count == view.labels.size()) {
            ++view.capacity_saturation_count;
            continue;
        }
        const dungeon::GroundHealthPotionSnapshot& potion =
            snapshot.ground_health_potions[index];
        const ScreenProjection projection = project_combat_position(
            potion.position, width, height);
        MaterialLootLabel label{};
        label.kind = SecondaryLootKind::health_potion;
        label.ordinal = potion.claim_ordinal;
        label.anchor_x = projection.x;
        label.anchor_y = projection.y;
        label.rect = {projection.x - kLabelWidth * 0.5F,
            projection.y - kLabelGap - kLabelHeight, kLabelWidth, kLabelHeight};
        clamp_rect(label.rect, width, height);
        label.text_color = {255U, 48U, 48U, 255U};
        label.sprite = MaterialSpriteId::health_potion;
        label.emphasized = true;
        label.text = "生命药";
        insert_label(view, label);
    }
    resolve_label_overlaps(view, width, height, obstacles, diagnostics);
    return view;
}

}  // namespace

MaterialLootView build_material_loot_view(
    const dungeon::DungeonSnapshot& snapshot, float width, float height) noexcept {
    LootLabelObstacleSet obstacles{};
    return build_material_loot_view_internal(
        snapshot, width, height, nullptr, obstacles);
}

MaterialLootView build_material_loot_view(
    const dungeon::DungeonSnapshot& snapshot, float width, float height,
    LootLabelObstacleSet& obstacles) noexcept {
    return build_material_loot_view_internal(
        snapshot, width, height, nullptr, obstacles);
}

MaterialLootView build_material_loot_view_with_diagnostics(
    const dungeon::DungeonSnapshot& snapshot, float width, float height,
    MaterialLootPlacementDiagnostics& diagnostics) noexcept {
    diagnostics = {};
    LootLabelObstacleSet obstacles{};
    return build_material_loot_view_internal(
        snapshot, width, height, &diagnostics, obstacles);
}

MaterialLootView build_material_loot_view_with_diagnostics(
    const dungeon::DungeonSnapshot& snapshot, float width, float height,
    MaterialLootPlacementDiagnostics& diagnostics,
    LootLabelObstacleSet& obstacles) noexcept {
    diagnostics = {};
    return build_material_loot_view_internal(
        snapshot, width, height, &diagnostics, obstacles);
}

void MaterialPickupFeedbackState::update(float frame_seconds,
    bool paused) noexcept {
    if (paused || !(frame_seconds > 0.0F) || seconds_left_ <= 0.0F) return;
    if (frame_seconds < seconds_left_) {
        seconds_left_ -= frame_seconds;
        return;
    }
    accumulated_.fill(0U);
    seconds_left_ = 0.0F;
}

void MaterialPickupFeedbackState::enqueue_feedback(
    MaterialPickupFeedback feedback, PendingFeedbackKind kind) noexcept {
    if (!feedback.ready) return;
    if (kind == PendingFeedbackKind::material) {
        for (std::size_t index = 0U;
             index < pending_feedback_count_; ++index) {
            if (pending_feedback_[index].kind == PendingFeedbackKind::material) {
                pending_feedback_[index].value = feedback;
                return;
            }
        }
    }
    if (pending_feedback_count_ == pending_feedback_.size()) return;

    std::size_t insertion = pending_feedback_count_;
    if (kind == PendingFeedbackKind::health_potion) {
        for (std::size_t index = 0U;
             index < pending_feedback_count_; ++index) {
            if (pending_feedback_[index].kind == PendingFeedbackKind::material) {
                insertion = index;
                break;
            }
        }
    }
    for (std::size_t index = pending_feedback_count_; index > insertion; --index) {
        pending_feedback_[index] = pending_feedback_[index - 1U];
    }
    pending_feedback_[insertion] = {feedback, kind};
    ++pending_feedback_count_;
}

MaterialPickupFeedback MaterialPickupFeedbackState::publish_next() noexcept {
    if (pending_feedback_count_ == 0U) return {};
    const MaterialPickupFeedback published = pending_feedback_[0].value;
    for (std::size_t index = 1U; index < pending_feedback_count_; ++index) {
        pending_feedback_[index - 1U] = pending_feedback_[index];
    }
    --pending_feedback_count_;
    pending_feedback_[pending_feedback_count_] = {};
    return published;
}

MaterialPickupFeedback MaterialPickupFeedbackState::observe(
    const dungeon::DungeonSnapshot& snapshot) noexcept {
    MaterialPickupFeedback material_feedback{};
    MaterialPickupFeedback potion_feedback{};
    const dungeon::MaterialPickupReceipt& material_receipt =
        snapshot.material_pickup_receipt;
    const dungeon::HealthPotionPickupReceipt& potion_receipt =
        snapshot.health_potion_pickup_receipt;
    if (!baseline_set_) {
        baseline_set_ = true;
        generation_ = material_receipt.valid
            ? material_receipt.commit_generation : 0U;
        health_potion_generation_ = potion_receipt.valid
            ? potion_receipt.commit_generation : 0U;
        return {};
    }
    if (material_receipt.valid && material_receipt.commit_generation != 0U
            && material_receipt.commit_generation > generation_) {
        generation_ = material_receipt.commit_generation;
        bool received{};
        for (std::size_t index = 0U; index < material_receipt.counts.size(); ++index) {
            if (material_receipt.counts[index] == 0U) continue;
            accumulated_[index] += material_receipt.counts[index];
            received = true;
        }
        if (received) {
            seconds_left_ = 2.0F;
            int written = std::snprintf(material_feedback.text.bytes.data(),
                material_feedback.text.bytes.size(), "已拾取：");
            if (written >= 0) {
                std::size_t used = static_cast<std::size_t>(written);
                for (std::size_t index = 0U; index < accumulated_.size(); ++index) {
                    if (accumulated_[index] == 0U) continue;
                    const items::MaterialId id = static_cast<items::MaterialId>(index);
                    const std::string_view label = material_label(id);
                    const int appended = std::snprintf(
                        material_feedback.text.bytes.data() + used,
                        material_feedback.text.bytes.size() - used,
                        "%s%.*s x%llu",
                        used == sizeof("已拾取：") - 1U ? "" : "、",
                        static_cast<int>(label.size()), label.data(),
                        static_cast<unsigned long long>(accumulated_[index]));
                    if (appended < 0) {
                        material_feedback = {};
                        break;
                    }
                    if (static_cast<std::size_t>(appended)
                            >= material_feedback.text.bytes.size() - used) {
                        material_feedback.text.truncated = true;
                        break;
                    }
                    used += static_cast<std::size_t>(appended);
                    material_feedback.emphasized = material_feedback.emphasized
                        || material_is_emphasized(id);
                }
                if (material_feedback.text.bytes[0] != '\0') {
                    material_feedback.text.bytes.back() = '\0';
                    material_feedback.ready = true;
                }
            }
        }
    }
    if (potion_receipt.valid && potion_receipt.commit_generation != 0U
            && potion_receipt.commit_generation > health_potion_generation_) {
        health_potion_generation_ = potion_receipt.commit_generation;
        std::snprintf(potion_feedback.text.bytes.data(),
            potion_feedback.text.bytes.size(), "生命药恢复 +%d HP",
            potion_receipt.restored_hp);
        potion_feedback.ready = potion_receipt.restored_hp > 0;
        potion_feedback.emphasized = potion_feedback.ready;
        if (potion_feedback.ready) seconds_left_ = 2.0F;
    }
    enqueue_feedback(potion_feedback, PendingFeedbackKind::health_potion);
    enqueue_feedback(material_feedback, PendingFeedbackKind::material);
    return publish_next();
}

}  // namespace arpg::platform
