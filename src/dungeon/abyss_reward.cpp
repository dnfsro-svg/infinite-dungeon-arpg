#include "dungeon/abyss_reward.hpp"

#include "abyss/abyss_rewards.hpp"
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

}  // namespace arpg::dungeon
