#pragma once

#include <cstdint>

namespace arpg::dungeon {

[[nodiscard]] bool reinforcement_succeeds(
    std::uint64_t root_seed,
    std::uint64_t item_id,
    std::uint32_t current,
    std::uint64_t nonce,
    std::uint16_t chance_bp) noexcept;

}  // namespace arpg::dungeon
