#pragma once

#include "skills/active_skill_types.hpp"

#include <cstddef>
#include <cstdint>

namespace arpg::combat {

enum class ActiveSkillTimelineEventKind : std::uint8_t {
    phase_startup,
    phase_strikes,
    phase_finisher,
    phase_recovery,
    damage,
    invulnerability_on,
    invulnerability_off,
    pull,
    spawn_sword,
    clear_transients,
};

struct ActiveSkillTimelineEvent final {
    std::uint16_t tick{};
    ActiveSkillTimelineEventKind kind{};
    std::uint8_t ordinal{};
};

struct ActiveSkillTimelineEventRange final {
    const ActiveSkillTimelineEvent* events{};
    std::size_t count{};

    [[nodiscard]] constexpr const ActiveSkillTimelineEvent* begin() const noexcept {
        return events;
    }
    [[nodiscard]] constexpr const ActiveSkillTimelineEvent* end() const noexcept {
        return events + count;
    }
    [[nodiscard]] constexpr bool empty() const noexcept { return count == 0U; }
    [[nodiscard]] constexpr std::size_t size() const noexcept { return count; }
    [[nodiscard]] constexpr const ActiveSkillTimelineEvent& operator[](
        const std::size_t index) const noexcept {
        return events[index];
    }
};

struct ActiveSkillClip final {
    std::uint16_t duration_ticks{};
    std::uint16_t frame_count{};
    std::uint8_t frames_per_second{};
};

[[nodiscard]] const ActiveSkillClip* active_skill_clip(
    skills::ActiveSkillId id) noexcept;

[[nodiscard]] ActiveSkillTimelineEventRange active_skill_events_at(
    skills::ActiveSkillId id, std::uint16_t tick) noexcept;

}  // namespace arpg::combat
