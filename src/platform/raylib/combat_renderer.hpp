#pragma once

#include "combat/combat_types.hpp"
#include "combat_feedback.hpp"

namespace arpg::platform {

class CombatRenderer final {
public:
    void consume_event(const combat::CombatEvent& event) noexcept;
    void draw(
        const combat::CombatSnapshot& previous,
        const combat::CombatSnapshot& current,
        float interpolation_alpha,
        bool draw_debug,
        const CombatFeedback& feedback,
        bool audio_ready) const noexcept;

private:
    combat::CombatEvent last_event_{};
    bool has_last_event_{};
};

}  // namespace arpg::platform
