#pragma once

#include "combat/combat_types.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace arpg::combat {

inline constexpr std::size_t kPlayerDamageHistoryTicks = 300U;

class PlayerDamageHistory final {
public:
    void begin_tick(std::uint64_t tick) noexcept;
    void record(const ResolvedPlayerDamage& damage) noexcept;
    [[nodiscard]] std::array<std::uint64_t,
        modifiers::kDamageTypeCount> totals() const noexcept;

private:
    std::array<std::array<std::uint64_t, modifiers::kDamageTypeCount>,
        kPlayerDamageHistoryTicks> buckets_{};
    std::uint64_t active_tick_{};
    bool initialized_{};
};

}  // namespace arpg::combat
