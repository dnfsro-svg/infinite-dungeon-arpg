#include "dungeon/abyss_reward.hpp"

#include "abyss/abyss_rewards.hpp"
#include "abyss/abyss_rules.hpp"
#include "core/deterministic_rng.hpp"
#include "items/item_generation.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace arpg::dungeon {
namespace {

constexpr std::uint64_t kRewardNamespace = 0x4142595353525731ULL;
constexpr std::uint64_t kRewardRulesVersionDomain = 0x4142595352565231ULL;
constexpr std::uint64_t kRewardOrdinalDomain = 0x414259534F524431ULL;
constexpr std::uint64_t kRewardSlotDomain = 0x41425953534C5431ULL;
constexpr std::uint64_t kRewardRarityDomain = 0x4142595352525431ULL;
constexpr std::uint64_t kRewardItemSeedDomain = 0x4142595353454431ULL;
constexpr std::uint64_t kRewardItemIdDomain = 0x4142595349544431ULL;
constexpr std::array<combat::Vec3, 3> kRewardPositions{{
    {-0.75F, 0.0F, 0.0F},
    {0.0F, 0.0F, 0.0F},
    {0.75F, 0.0F, 0.0F},
}};

std::uint64_t ordinal_root(
    std::uint64_t room_seed,
    std::uint8_t reward_ordinal) noexcept {
    auto namespaced = core::DeterministicRng::derive_stream(
        room_seed, kRewardNamespace);
    auto version = core::DeterministicRng::derive_stream(
        namespaced.next_u64(),
        kRewardRulesVersionDomain ^ abyss::kAbyssRulesVersion);
    auto ordinal = core::DeterministicRng::derive_stream(
        version.next_u64(),
        kRewardOrdinalDomain ^ static_cast<std::uint64_t>(reward_ordinal));
    return ordinal.next_u64();
}

items::ItemRarity roll_rarity(
    std::uint64_t root,
    const abyss::AbyssRarityWeights& weights) noexcept {
    auto stream = core::DeterministicRng::derive_stream(
        root, kRewardRarityDomain);
    const std::uint64_t roll = stream.next_bounded(100U).value_or(0U);
    if (roll < weights.normal) return items::ItemRarity::normal;
    if (roll < static_cast<std::uint64_t>(weights.normal) + weights.magic)
        return items::ItemRarity::magic;
    return items::ItemRarity::rare;
}

bool same_item_instance(
    const items::ItemInstance& left,
    const items::ItemInstance& right) noexcept {
    if (left.id != right.id || left.base_id != right.base_id
            || left.rarity != right.rarity
            || left.item_level != right.item_level
            || left.required_level != right.required_level
            || left.affix_count != right.affix_count
            || left.reserved != right.reserved
            || left.reinforcement != right.reinforcement
            || left.extension_reserved != right.extension_reserved) {
        return false;
    }
    for (std::size_t index = 0U; index < left.affixes.size(); ++index) {
        if (left.affixes[index].affix_id != right.affixes[index].affix_id
                || left.affixes[index].tier != right.affixes[index].tier
                || left.affixes[index].variant
                    != right.affixes[index].variant
                || left.affixes[index].value_roll_bp
                    != right.affixes[index].value_roll_bp) {
            return false;
        }
    }
    return true;
}

}  // namespace

std::optional<AbyssRewardSlot> derive_abyss_reward_slot(
    std::uint64_t room_seed,
    abyss::AbyssDanger danger,
    std::uint8_t base_item_level,
    std::uint8_t reward_ordinal) noexcept {
    const abyss::AbyssRewardProfile profile =
        abyss::reward_profile_for(danger, base_item_level);
    if (profile.item_count == 0U || reward_ordinal >= profile.item_count)
        return std::nullopt;
    const auto base_weights = items::rarity_weights(profile.item_level);
    if (!base_weights.has_value()) return std::nullopt;
    const auto shifted = abyss::shift_abyss_rarity_weights({
        base_weights->normal, base_weights->magic, base_weights->rare}, danger);
    if (!shifted.has_value()) return std::nullopt;

    const std::uint64_t root = ordinal_root(room_seed, reward_ordinal);
    auto slot_stream = core::DeterministicRng::derive_stream(
        root, kRewardSlotDomain);
    auto seed_stream = core::DeterministicRng::derive_stream(
        root, kRewardItemSeedDomain);
    auto id_stream = core::DeterministicRng::derive_stream(
        root, kRewardItemIdDomain);
    std::uint64_t item_id = id_stream.next_u64();
    if (item_id == 0U) item_id = 1U;
    return AbyssRewardSlot{
        reward_ordinal,
        static_cast<items::ItemSlot>(
            slot_stream.next_bounded(
                static_cast<std::uint8_t>(items::ItemSlot::count)).value_or(0U)),
        roll_rarity(root, *shifted),
        profile.item_level,
        seed_stream.next_u64(),
        item_id,
    };
}

std::optional<combat::Vec3> abyss_reward_position(
    std::uint8_t reward_ordinal) noexcept {
    if (reward_ordinal >= kRewardPositions.size()) return std::nullopt;
    return kRewardPositions[reward_ordinal];
}

std::optional<GroundItem> derive_abyss_ground_item(
    std::uint64_t room_seed,
    abyss::AbyssDanger danger,
    std::uint8_t base_item_level,
    std::uint8_t reward_ordinal,
    std::uint16_t ground_index) noexcept {
    const auto slot = derive_abyss_reward_slot(
        room_seed, danger, base_item_level, reward_ordinal);
    const auto position = abyss_reward_position(reward_ordinal);
    if (!slot.has_value() || !position.has_value()) return std::nullopt;
    const auto item = items::generate_item({
        slot->item_seed,
        slot->item_slot,
        slot->item_level,
        slot->item_id,
        slot->rarity,
    });
    if (!item.has_value()) return std::nullopt;
    return GroundItem{
        true,
        ground_index,
        GroundItemSource::abyss_chest,
        reward_ordinal,
        *position,
        *item,
    };
}

bool same_ground_item(
    const GroundItem& left,
    const GroundItem& right) noexcept {
    return left.active == right.active
        && left.drop_ordinal == right.drop_ordinal
        && left.source == right.source
        && left.abyss_reward_ordinal == right.abyss_reward_ordinal
        && left.position.x == right.position.x
        && left.position.y == right.position.y
        && left.position.z == right.position.z
        && same_item_instance(left.item, right.item);
}

bool apply_abyss_failure_resolution(
    DungeonRunState& next,
    const DungeonRunState& previous) noexcept {
    if (!previous.current_room.is_abyss
            || previous.abyss.lifecycle != abyss::AbyssLifecycle::started) {
        return false;
    }
    const auto selection = abyss::select_abyss_rule(
        previous.current_room.seed, previous.current_room.depth);
    if (!selection.has_value()
            || previous.abyss.danger != selection->danger
            || previous.abyss.rule != selection->rule
            || previous.abyss.rules_version != selection->rules_version) {
        return false;
    }
    const std::uint8_t total = abyss::reward_profile_for(
        previous.abyss.danger, 1U).item_count;
    if (total == 0U || total > 3U) return false;

    next.current_room.is_abyss = false;
    next.abyss = previous.abyss;
    next.abyss.lifecycle = abyss::AbyssLifecycle::failed;
    next.abyss.reward_total = 0U;
    next.abyss.generated_mask = 0U;
    next.abyss.claimed_mask = 0U;
    next.abyss.abandoned_mask = 0U;
    next.abyss.reward_revision = 0U;
    next.last_abyss_resolution = {
        true,
        previous.current_room.seed,
        previous.abyss.rule,
        total,
        0U,
        0U,
        total,
    };
    return true;
}

}  // namespace arpg::dungeon
