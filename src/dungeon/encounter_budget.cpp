#include "dungeon/dungeon_rules.hpp"

#include <cstdint>
#include <limits>

namespace arpg::dungeon::detail {

bool checked_accumulate_threat_cost(
    std::uint8_t& total,
    std::uint8_t threat_cost) noexcept {
    if (threat_cost > (std::numeric_limits<std::uint8_t>::max)() - total) {
        return false;
    }
    total = static_cast<std::uint8_t>(total + threat_cost);
    return true;
}

}  // namespace arpg::dungeon::detail
