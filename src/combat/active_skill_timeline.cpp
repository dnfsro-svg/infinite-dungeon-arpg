#include "combat/active_skill_timeline.hpp"

#include <array>
#include <cstddef>

namespace arpg::combat {
namespace {

constexpr ActiveSkillClip kDrawSlashClip{90U, 36U, 24U};
constexpr ActiveSkillClip kStormSwordsClip{360U, 144U, 24U};

constexpr std::uint8_t kFinisherOrdinal = 0xFFU;

constexpr std::array<ActiveSkillTimelineEvent, 5U> kDrawSlashEvents{{
    {0U, ActiveSkillTimelineEventKind::phase_startup, 0U},
    {45U, ActiveSkillTimelineEventKind::phase_strikes, 0U},
    {46U, ActiveSkillTimelineEventKind::damage, 0U},
    {66U, ActiveSkillTimelineEventKind::phase_recovery, 0U},
    {90U, ActiveSkillTimelineEventKind::clear_transients, 0U},
}};

constexpr std::array<ActiveSkillTimelineEvent, 44U> kStormSwordsEvents{{
    {0U, ActiveSkillTimelineEventKind::phase_startup, 0U},
    {0U, ActiveSkillTimelineEventKind::pull, 0U},
    {12U, ActiveSkillTimelineEventKind::spawn_sword, 0U},
    {18U, ActiveSkillTimelineEventKind::spawn_sword, 1U},
    {24U, ActiveSkillTimelineEventKind::spawn_sword, 2U},
    {30U, ActiveSkillTimelineEventKind::spawn_sword, 3U},
    {36U, ActiveSkillTimelineEventKind::spawn_sword, 4U},
    {42U, ActiveSkillTimelineEventKind::spawn_sword, 5U},
    {48U, ActiveSkillTimelineEventKind::spawn_sword, 6U},
    {54U, ActiveSkillTimelineEventKind::spawn_sword, 7U},
    {60U, ActiveSkillTimelineEventKind::spawn_sword, 8U},
    {66U, ActiveSkillTimelineEventKind::spawn_sword, 9U},
    {72U, ActiveSkillTimelineEventKind::phase_strikes, 0U},
    {72U, ActiveSkillTimelineEventKind::spawn_sword, 10U},
    {78U, ActiveSkillTimelineEventKind::spawn_sword, 11U},
    {84U, ActiveSkillTimelineEventKind::spawn_sword, 12U},
    {90U, ActiveSkillTimelineEventKind::spawn_sword, 13U},
    {96U, ActiveSkillTimelineEventKind::invulnerability_on, 0U},
    {96U, ActiveSkillTimelineEventKind::spawn_sword, 14U},
    {102U, ActiveSkillTimelineEventKind::spawn_sword, 15U},
    {108U, ActiveSkillTimelineEventKind::spawn_sword, 16U},
    {108U, ActiveSkillTimelineEventKind::damage, 0U},
    {114U, ActiveSkillTimelineEventKind::spawn_sword, 17U},
    {120U, ActiveSkillTimelineEventKind::spawn_sword, 18U},
    {126U, ActiveSkillTimelineEventKind::spawn_sword, 19U},
    {126U, ActiveSkillTimelineEventKind::damage, 1U},
    {132U, ActiveSkillTimelineEventKind::spawn_sword, 20U},
    {138U, ActiveSkillTimelineEventKind::spawn_sword, 21U},
    {144U, ActiveSkillTimelineEventKind::spawn_sword, 22U},
    {144U, ActiveSkillTimelineEventKind::damage, 2U},
    {150U, ActiveSkillTimelineEventKind::spawn_sword, 23U},
    {162U, ActiveSkillTimelineEventKind::damage, 3U},
    {180U, ActiveSkillTimelineEventKind::damage, 4U},
    {198U, ActiveSkillTimelineEventKind::damage, 5U},
    {216U, ActiveSkillTimelineEventKind::damage, 6U},
    {234U, ActiveSkillTimelineEventKind::damage, 7U},
    {252U, ActiveSkillTimelineEventKind::damage, 8U},
    {270U, ActiveSkillTimelineEventKind::damage, 9U},
    {288U, ActiveSkillTimelineEventKind::damage, 10U},
    {306U, ActiveSkillTimelineEventKind::damage, 11U},
    {324U, ActiveSkillTimelineEventKind::phase_finisher, 0U},
    {324U, ActiveSkillTimelineEventKind::damage, kFinisherOrdinal},
    {342U, ActiveSkillTimelineEventKind::invulnerability_off, 0U},
    {360U, ActiveSkillTimelineEventKind::clear_transients, 0U},
}};
template <std::size_t EventCount>
[[nodiscard]] constexpr bool events_are_sorted(
    const std::array<ActiveSkillTimelineEvent, EventCount>& events) noexcept {
    for (std::size_t index = 1U; index < EventCount; ++index) {
        if (events[index - 1U].tick > events[index].tick) return false;
    }
    return true;
}

static_assert(events_are_sorted(kDrawSlashEvents),
    "draw slash events must remain sorted");
static_assert(events_are_sorted(kStormSwordsEvents),
    "storm swords events must remain sorted");

template <std::size_t EventCount>
[[nodiscard]] ActiveSkillTimelineEventRange events_at(
    const std::array<ActiveSkillTimelineEvent, EventCount>& events,
    const std::uint16_t tick) noexcept {
    std::size_t first = EventCount;
    std::size_t count = 0U;
    for (std::size_t index = 0U; index < EventCount; ++index) {
        if (events[index].tick == tick) {
            if (count == 0U) first = index;
            ++count;
        } else if (count != 0U) {
            break;
        }
    }
    return count == 0U
        ? ActiveSkillTimelineEventRange{}
        : ActiveSkillTimelineEventRange{events.data() + first, count};
}

}  // namespace

const ActiveSkillClip* active_skill_clip(const skills::ActiveSkillId id) noexcept {
    switch (id) {
    case skills::ActiveSkillId::draw_slash: return &kDrawSlashClip;
    case skills::ActiveSkillId::storm_swords: return &kStormSwordsClip;
    case skills::ActiveSkillId::count:
    case skills::ActiveSkillId::none: break;
    }
    return nullptr;
}

ActiveSkillTimelineEventRange active_skill_events_at(
    const skills::ActiveSkillId id,
    const std::uint16_t tick) noexcept {
    switch (id) {
    case skills::ActiveSkillId::draw_slash: return events_at(kDrawSlashEvents, tick);
    case skills::ActiveSkillId::storm_swords: return events_at(kStormSwordsEvents, tick);
    case skills::ActiveSkillId::count:
    case skills::ActiveSkillId::none: break;
    }
    return {};
}

}  // namespace arpg::combat
