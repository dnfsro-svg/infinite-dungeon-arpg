#include "material_loot_view.hpp"

#include "combat_view_math.hpp"

#include <algorithm>
#include <cstdio>

namespace arpg::platform {
namespace {

constexpr float kLabelWidth = 190.0F;
constexpr float kLabelHeight = 23.0F;
constexpr float kLabelGap = 13.0F;

void clamp_rect(LootLabelRect& rect, float width, float height) noexcept {
    const float usable_width = (std::max)(kGroundLootSafetyInset * 2.0F, width);
    const float usable_height = (std::max)(kGroundLootSafetyInset * 2.0F, height);
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

[[nodiscard]] bool overlaps_previous(const MaterialLootView& view,
    std::size_t index, LootLabelRect rect) noexcept {
    for (std::size_t previous = 0U; previous < index; ++previous) {
        if (loot_label_rects_overlap(rect, view.labels[previous].rect)) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] bool place_without_overlap(MaterialLootView& view,
    std::size_t index, float width, float height) noexcept {
    const LootLabelRect original = view.labels[index].rect;
    LootLabelRect candidate = original;

    for (std::size_t attempt = 0U; attempt < view.labels.size(); ++attempt) {
        if (!overlaps_previous(view, index, candidate)) {
            view.labels[index].rect = candidate;
            return true;
        }
        const float previous_y = candidate.y;
        candidate.y -= kLabelHeight + 3.0F;
        clamp_rect(candidate, width, height);
        if (candidate.y == previous_y) break;
    }

    LootLabelRect row = original;
    for (std::size_t vertical = 0U;
         vertical < view.labels.size(); ++vertical) {
        for (std::size_t side = 0U; side < 2U; ++side) {
            candidate = row;
            const float direction = side == 0U ? 1.0F : -1.0F;
            for (std::size_t horizontal = 0U;
                 horizontal < view.labels.size(); ++horizontal) {
                const float previous_x = candidate.x;
                candidate.x += direction * (original.width + 3.0F);
                clamp_rect(candidate, width, height);
                if (candidate.x == previous_x) break;
                if (!overlaps_previous(view, index, candidate)) {
                    view.labels[index].rect = candidate;
                    return true;
                }
            }
        }
        const float previous_y = row.y;
        row.y -= kLabelHeight + 3.0F;
        clamp_rect(row, width, height);
        if (row.y == previous_y) break;
    }
    return false;
}

void resolve_label_overlaps(MaterialLootView& view,
    float width, float height) noexcept {
    std::size_t index = 1U;
    while (index < view.count) {
        if (place_without_overlap(view, index, width, height)) {
            ++index;
            continue;
        }
        for (std::size_t shift = index + 1U; shift < view.count; ++shift) {
            view.labels[shift - 1U] = view.labels[shift];
        }
        --view.count;
        view.labels[view.count] = {};
        ++view.capacity_saturation_count;
    }
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

MaterialLootView build_material_loot_view(
    const dungeon::DungeonSnapshot& snapshot, float width, float height) noexcept {
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
        const std::string_view text = material_label(material.material);
        static_cast<void>(std::snprintf(label.text.data(), label.text.size(), "%.*s",
            static_cast<int>(text.size()), text.data()));
        label.text.back() = '\0';
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
        static_cast<void>(std::snprintf(
            label.text.data(), label.text.size(), "%s", "生命药"));
        label.text.back() = '\0';
        insert_label(view, label);
    }
    resolve_label_overlaps(view, width, height);
    return view;
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
    if (pending_material_feedback_.ready) {
        const MaterialPickupFeedback pending = pending_material_feedback_;
        pending_material_feedback_ = {};
        return pending;
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
                    if (appended < 0) return {};
                    if (static_cast<std::size_t>(appended)
                            >= material_feedback.text.bytes.size() - used) {
                        material_feedback.text.truncated = true;
                        break;
                    }
                    used += static_cast<std::size_t>(appended);
                    material_feedback.emphasized = material_feedback.emphasized
                        || material_is_emphasized(id);
                }
                material_feedback.text.bytes.back() = '\0';
                material_feedback.ready = true;
            }
        }
    }
    if (potion_receipt.valid && potion_receipt.commit_generation != 0U
            && potion_receipt.commit_generation > health_potion_generation_) {
        health_potion_generation_ = potion_receipt.commit_generation;
        std::snprintf(potion_feedback.text.bytes.data(),
            potion_feedback.text.bytes.size(), "生命药 +%d HP",
            potion_receipt.restored_hp);
        potion_feedback.ready = potion_receipt.restored_hp > 0;
        potion_feedback.emphasized = potion_feedback.ready;
        if (potion_feedback.ready) seconds_left_ = 2.0F;
    }
    if (potion_feedback.ready && material_feedback.ready) {
        pending_material_feedback_ = material_feedback;
    }
    return potion_feedback.ready ? potion_feedback : material_feedback;
}

}  // namespace arpg::platform
