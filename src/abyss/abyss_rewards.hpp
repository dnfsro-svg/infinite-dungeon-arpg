#pragma once

#include "abyss/abyss_types.hpp"

#include <cstdint>
#include <optional>

namespace arpg::abyss {

struct AbyssRewardProfile final {
    std::uint8_t item_count{};
    std::uint8_t item_level{};
};

struct AbyssRarityWeights final {
    std::uint32_t normal{};
    std::uint32_t magic{};
    std::uint32_t rare{};
};

[[nodiscard]] AbyssRewardProfile reward_profile_for(
    AbyssDanger danger,
    std::uint8_t base_item_level) noexcept;
[[nodiscard]] std::optional<AbyssRarityWeights> shift_abyss_rarity_weights(
    AbyssRarityWeights base,
    AbyssDanger danger) noexcept;

}  // namespace arpg::abyss
