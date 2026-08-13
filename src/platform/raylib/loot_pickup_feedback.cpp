#include "loot_pickup_feedback.hpp"

#include "items/item_catalog.hpp"

#include <cmath>
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
        && std::isfinite(receipt.position.x) && std::isfinite(receipt.position.y)
        && std::isfinite(receipt.position.z)
        && static_cast<std::size_t>(receipt.slot)
            < static_cast<std::size_t>(items::ItemSlot::count)
        && rarity_name(receipt.rarity) != nullptr
        && items::base_definition(receipt.base_id) != nullptr
        && (receipt.source == dungeon::GroundItemSource::monster_drop
            || receipt.source == dungeon::GroundItemSource::abyss_chest);
}

[[nodiscard]] bool strictly_empty_receipt(
    const LootPickupReceipt& receipt) noexcept {
    return !receipt.valid && receipt.commit_generation == 0U
        && receipt.item_id == 0U && receipt.base_id == 0U
        && receipt.item_level == 0U
        && receipt.rarity == items::ItemRarity::normal
        && receipt.source == dungeon::GroundItemSource::monster_drop
        && receipt.position.x == 0.0F && receipt.position.y == 0.0F
        && receipt.position.z == 0.0F
        && receipt.slot == items::ItemSlot::count;
}

[[nodiscard]] LootPickupFeedback format_feedback(
    const LootPickupReceipt& receipt) noexcept {
    LootPickupFeedback feedback{};
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
    return feedback;
}

}  // namespace

LootPickupFeedback LootPickupFeedbackState::observe(
    const DungeonRenderStatus& status) noexcept {
    LootPickupFeedback feedback{};
    const LootPickupReceipt& receipt = status.loot_pickup;
    const bool blocked = status.indicator == SaveIndicator::error
        || status.recovery_required || status.faulted;
    if (blocked) {
        if (valid_receipt(receipt)
                && (!has_high_water_
                    || receipt.commit_generation > generation_)) {
            generation_ = receipt.commit_generation;
            item_id_ = receipt.item_id;
            has_high_water_ = true;
        } else if (!strictly_empty_receipt(receipt)
                && !valid_receipt(receipt)) {
            attachment_ = Attachment::unattached;
            preserve_attachment_after_block_ = false;
            return feedback;
        }
        attachment_ = Attachment::live;
        preserve_attachment_after_block_ = true;
        return feedback;
    }
    if (strictly_empty_receipt(receipt)) {
        attachment_ = Attachment::live;
        preserve_attachment_after_block_ = false;
        return feedback;
    }
    if (!valid_receipt(receipt)) {
        attachment_ = Attachment::unattached;
        preserve_attachment_after_block_ = false;
        return feedback;
    }
    if (!has_high_water_) {
        generation_ = receipt.commit_generation;
        item_id_ = receipt.item_id;
        has_high_water_ = true;
        if (attachment_ == Attachment::unattached) {
            attachment_ = Attachment::live;
            preserve_attachment_after_block_ = false;
            return feedback;
        }
        attachment_ = Attachment::live;
        preserve_attachment_after_block_ = false;
        return format_feedback(receipt);
    }
    if (receipt.commit_generation < generation_) {
        if (!preserve_attachment_after_block_) {
            attachment_ = Attachment::unattached;
        }
        return feedback;
    }
    if (receipt.commit_generation == generation_) {
        if (receipt.item_id != item_id_) {
            attachment_ = Attachment::unattached;
            preserve_attachment_after_block_ = false;
        }
        return feedback;
    }
    generation_ = receipt.commit_generation;
    item_id_ = receipt.item_id;
    if (attachment_ == Attachment::unattached) {
        attachment_ = Attachment::live;
        preserve_attachment_after_block_ = false;
        return feedback;
    }
    attachment_ = Attachment::live;
    preserve_attachment_after_block_ = false;
    return format_feedback(receipt);
}

}  // namespace arpg::platform
