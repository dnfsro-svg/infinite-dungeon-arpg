#include "loot_pickup_feedback.hpp"

#include "items/item_catalog.hpp"

#include <cstdio>

namespace arpg::platform {
namespace {

[[nodiscard]] const char* rarity_name(items::ItemRarity rarity) noexcept {
    switch (rarity) {
    case items::ItemRarity::normal: return "普通";
    case items::ItemRarity::magic: return "魔法";
    case items::ItemRarity::rare: return "稀有";
    }
    return nullptr;
}

[[nodiscard]] bool valid_receipt(const LootPickupReceipt& receipt) noexcept {
    return receipt.valid && receipt.commit_generation != 0U
        && receipt.item_id != 0U && receipt.item_level != 0U
        && receipt.item_level <= 100U
        && rarity_name(receipt.rarity) != nullptr
        && items::base_definition(receipt.base_id) != nullptr
        && (receipt.source == dungeon::GroundItemSource::monster_drop
            || receipt.source == dungeon::GroundItemSource::abyss_chest);
}

}  // namespace

LootPickupFeedback LootPickupFeedbackState::observe(
    const DungeonRenderStatus& status) noexcept {
    LootPickupFeedback feedback{};
    if (status.indicator == SaveIndicator::error || status.recovery_required) {
        attachment_ = Attachment::unattached;
        generation_ = 0U;
        item_id_ = 0U;
        return feedback;
    }
    const LootPickupReceipt& receipt = status.loot_pickup;
    if (!receipt.valid) {
        const bool empty = receipt.commit_generation == 0U
            && receipt.item_id == 0U && receipt.base_id == 0U
            && receipt.item_level == 0U;
        attachment_ = empty
            ? Attachment::empty_observed : Attachment::unattached;
        generation_ = 0U;
        item_id_ = 0U;
        return feedback;
    }
    if (!valid_receipt(receipt)) {
        attachment_ = Attachment::unattached;
        generation_ = 0U;
        item_id_ = 0U;
        return feedback;
    }
    if (attachment_ == Attachment::unattached) {
        generation_ = receipt.commit_generation;
        item_id_ = receipt.item_id;
        attachment_ = Attachment::receipt_baseline;
        return feedback;
    }
    if (attachment_ == Attachment::receipt_baseline
            && receipt.commit_generation == generation_
            && receipt.item_id == item_id_) {
        return feedback;
    }
    if (attachment_ == Attachment::receipt_baseline
            && receipt.commit_generation <= generation_) {
        attachment_ = Attachment::unattached;
        generation_ = 0U;
        item_id_ = 0U;
        return feedback;
    }

    const items::BaseDefinition* const base =
        items::base_definition(receipt.base_id);
    const int written = std::snprintf(feedback.text.bytes.data(),
        feedback.text.bytes.size(), u8"已拾取：%s %.*s · i%u",
        rarity_name(receipt.rarity), static_cast<int>(base->name.size()),
        base->name.data(), static_cast<unsigned>(receipt.item_level));
    feedback.text.bytes.back() = '\0';
    feedback.text.truncated = written < 0
        || static_cast<std::size_t>(written) >= feedback.text.bytes.size();
    feedback.ready = written >= 0;
    feedback.abyss = receipt.source == dungeon::GroundItemSource::abyss_chest;
    feedback.item_id = receipt.item_id;
    generation_ = receipt.commit_generation;
    item_id_ = receipt.item_id;
    attachment_ = Attachment::receipt_baseline;
    return feedback;
}

}  // namespace arpg::platform
