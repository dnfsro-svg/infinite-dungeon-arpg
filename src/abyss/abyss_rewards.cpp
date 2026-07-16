#include "abyss/abyss_rewards.hpp"

#include <algorithm>
#include <cstdint>

namespace arpg::abyss {
namespace {

struct RewardDeltas final {
    std::uint8_t item_count{};
    std::uint8_t item_level{};
    std::uint8_t normal_transfer{};
    std::uint8_t magic_gain{};
    std::uint8_t rare_gain{};
};

bool reward_deltas_for(
    AbyssDanger danger,
    RewardDeltas& deltas) noexcept {
    switch (danger) {
    case AbyssDanger::low:
        deltas = {1U, 1U, 10U, 5U, 5U};
        return true;
    case AbyssDanger::medium:
        deltas = {2U, 3U, 20U, 10U, 10U};
        return true;
    case AbyssDanger::high:
        deltas = {3U, 5U, 30U, 10U, 20U};
        return true;
    default:
        return false;
    }
}

}  // namespace

AbyssRewardProfile reward_profile_for(
    AbyssDanger danger,
    std::uint8_t base_item_level) noexcept {
    RewardDeltas deltas{};
    if (base_item_level == 0U || !reward_deltas_for(danger, deltas))
        return {};
    const std::uint16_t level = static_cast<std::uint16_t>(base_item_level)
        + static_cast<std::uint16_t>(deltas.item_level);
    return {deltas.item_count, static_cast<std::uint8_t>(
        (std::min)(level, std::uint16_t{100U}))};
}

std::optional<AbyssRarityWeights> shift_abyss_rarity_weights(
    AbyssRarityWeights base,
    AbyssDanger danger) noexcept {
    const std::uint64_t base_total = static_cast<std::uint64_t>(base.normal)
        + static_cast<std::uint64_t>(base.magic)
        + static_cast<std::uint64_t>(base.rare);
    RewardDeltas deltas{};
    if (base_total != 100U || !reward_deltas_for(danger, deltas))
        return std::nullopt;

    const std::uint32_t transfer = (std::min)(
        base.normal,
        static_cast<std::uint32_t>(deltas.normal_transfer));
    const std::uint32_t ratio_total = static_cast<std::uint32_t>(
        deltas.magic_gain + deltas.rare_gain);
    const std::uint64_t magic_product = static_cast<std::uint64_t>(transfer)
        * static_cast<std::uint64_t>(deltas.magic_gain);
    const std::uint64_t rare_product = static_cast<std::uint64_t>(transfer)
        * static_cast<std::uint64_t>(deltas.rare_gain);
    std::uint32_t magic_add = static_cast<std::uint32_t>(
        magic_product / ratio_total);
    std::uint32_t rare_add = static_cast<std::uint32_t>(
        rare_product / ratio_total);
    const std::uint32_t assigned = magic_add + rare_add;
    if (assigned < transfer) {
        const std::uint64_t magic_remainder = magic_product % ratio_total;
        const std::uint64_t rare_remainder = rare_product % ratio_total;
        if (magic_remainder >= rare_remainder)
            ++magic_add;
        else
            ++rare_add;
    }

    return AbyssRarityWeights{
        base.normal - transfer,
        base.magic + magic_add,
        base.rare + rare_add};
}

}  // namespace arpg::abyss
