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

}  // namespace

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

MaterialLootView build_material_loot_view(
    const dungeon::DungeonSnapshot& snapshot, CombatCameraView camera,
    float width, float height) noexcept {
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
            material.position, camera, width, height);
        MaterialLootLabel label{};
        label.ordinal = material.ordinal;
        label.anchor_x = projection.x;
        label.anchor_y = projection.y;
        label.rect = {projection.x - kLabelWidth * 0.5F,
            projection.y - kLabelGap - kLabelHeight, kLabelWidth, kLabelHeight};
        clamp_rect(label.rect, width, height);
        label.text_color = material_color(material.material);
        label.emphasized = material_is_emphasized(material.material);
        const std::string_view text = material_label(material.material);
        static_cast<void>(std::snprintf(label.text.data(), label.text.size(), "%.*s",
            static_cast<int>(text.size()), text.data()));
        label.text.back() = '\0';
        insert_label(view, label);
    }
    return view;
}

MaterialLootView build_material_loot_view(
    const dungeon::DungeonSnapshot& snapshot,
    float width,
    float height) noexcept {
    return build_material_loot_view(snapshot,
        make_combat_camera_view({}, width, height), width, height);
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
    MaterialPickupFeedback feedback{};
    const dungeon::MaterialPickupReceipt& receipt =
        snapshot.material_pickup_receipt;
    if (!baseline_set_) {
        baseline_set_ = true;
        if (!receipt.valid) return feedback;
    }
    if (!receipt.valid || receipt.commit_generation == 0U
            || receipt.commit_generation <= generation_) {
        return feedback;
    }
    generation_ = receipt.commit_generation;
    bool received{};
    for (std::size_t index = 0U; index < receipt.counts.size(); ++index) {
        if (receipt.counts[index] == 0U) continue;
        accumulated_[index] += receipt.counts[index];
        received = true;
    }
    if (!received) return feedback;
    seconds_left_ = 2.0F;

    int written = std::snprintf(feedback.text.bytes.data(),
        feedback.text.bytes.size(), "已拾取：");
    if (written < 0) return feedback;
    std::size_t used = static_cast<std::size_t>(written);
    for (std::size_t index = 0U; index < accumulated_.size(); ++index) {
        if (accumulated_[index] == 0U) continue;
        const items::MaterialId id = static_cast<items::MaterialId>(index);
        const std::string_view label = material_label(id);
        const int appended = std::snprintf(feedback.text.bytes.data() + used,
            feedback.text.bytes.size() - used, "%s%.*s x%llu",
            used == sizeof("已拾取：") - 1U ? "" : "、",
            static_cast<int>(label.size()), label.data(),
            static_cast<unsigned long long>(accumulated_[index]));
        if (appended < 0) return {};
        if (static_cast<std::size_t>(appended) >= feedback.text.bytes.size() - used) {
            feedback.text.truncated = true;
            break;
        }
        used += static_cast<std::size_t>(appended);
        feedback.emphasized = feedback.emphasized || material_is_emphasized(id);
    }
    feedback.text.bytes.back() = '\0';
    feedback.ready = true;
    return feedback;
}

}  // namespace arpg::platform
