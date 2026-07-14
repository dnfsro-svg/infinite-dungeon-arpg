#pragma once

#include <cstdint>

namespace arpg::progression {

struct ProgressionState final {
    std::uint8_t level{1U};
    std::uint64_t experience{};
    std::uint8_t earned_passive_points{};
    std::uint8_t unspent_passive_points{};
};

struct ProgressionAward final {
    ProgressionState state{};
    std::uint8_t levels_gained{};
};

}  // namespace arpg::progression
