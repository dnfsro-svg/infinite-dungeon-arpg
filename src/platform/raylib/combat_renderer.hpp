#pragma once

#include "combat/combat_types.hpp"

#include <array>
#include <cstdint>

namespace arpg::platform {

class CombatRenderer final {
public:
    void consume_event(const combat::CombatEvent& event) noexcept;
    void draw(
        const combat::CombatSnapshot& previous,
        const combat::CombatSnapshot& current,
        float interpolation_alpha,
        bool draw_debug) const noexcept;

private:
    combat::CombatEvent last_event_{};
    std::array<std::uint64_t, combat::kDummyCount> flash_until_tick_{};
    bool has_last_event_{};
};

}  // namespace arpg::platform
