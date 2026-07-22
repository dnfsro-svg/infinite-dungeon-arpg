#include "test_framework.hpp"

#include "combat/active_skill_timeline.hpp"

#include <cstdint>

namespace {

using arpg::combat::ActiveSkillTimelineEventKind;
using arpg::combat::ActiveSkillTimelineEventRange;
using arpg::combat::active_skill_clip;
using arpg::combat::active_skill_events_at;
using arpg::skills::ActiveSkillId;

struct EventCounts final {
    int damage{};
    int finisher_damage{};
    int invulnerability_on{};
    int invulnerability_off{};
    int spawn_sword{};
};

arpg::test::Failure count_events(
    const ActiveSkillId id,
    const std::uint16_t duration,
    EventCounts& counts) noexcept {
    std::uint16_t previous_tick = 0U;
    bool saw_event = false;
    for (std::uint16_t tick = 0U; tick <= duration; ++tick) {
        for (const auto& event : active_skill_events_at(id, tick)) {
            ARPG_REQUIRE(event.tick == tick);
            if (saw_event) {
                ARPG_REQUIRE(previous_tick <= event.tick);
            }
            previous_tick = event.tick;
            saw_event = true;
            if (event.kind == ActiveSkillTimelineEventKind::damage) {
                if (event.ordinal == 0xFFU) {
                    ++counts.finisher_damage;
                } else {
                    ++counts.damage;
                }
            } else if (event.kind == ActiveSkillTimelineEventKind::invulnerability_on) {
                ++counts.invulnerability_on;
            } else if (event.kind == ActiveSkillTimelineEventKind::invulnerability_off) {
                ++counts.invulnerability_off;
            } else if (event.kind == ActiveSkillTimelineEventKind::spawn_sword) {
                ++counts.spawn_sword;
            }
        }
    }
    return {};
}

arpg::test::Failure draw_slash_clip_has_locked_timing() noexcept {
    const auto* clip = active_skill_clip(ActiveSkillId::draw_slash);
    ARPG_REQUIRE(clip != nullptr);
    ARPG_REQUIRE(clip->duration_ticks == 90U);
    ARPG_REQUIRE(clip->frame_count == 36U);
    ARPG_REQUIRE(clip->frames_per_second == 24U);
    EventCounts counts{};
    const auto failure = count_events(ActiveSkillId::draw_slash, clip->duration_ticks, counts);
    ARPG_REQUIRE(failure.expression == nullptr);
    ARPG_REQUIRE(counts.damage == 1);
    ARPG_REQUIRE(counts.finisher_damage == 0);
    ARPG_REQUIRE(counts.spawn_sword == 0);
    return {};
}

arpg::test::Failure storm_swords_clip_has_locked_event_counts() noexcept {
    const auto* clip = active_skill_clip(ActiveSkillId::storm_swords);
    ARPG_REQUIRE(clip != nullptr);
    ARPG_REQUIRE(clip->duration_ticks == 360U);
    EventCounts counts{};
    const auto failure = count_events(ActiveSkillId::storm_swords, clip->duration_ticks, counts);
    ARPG_REQUIRE(failure.expression == nullptr);
    ARPG_REQUIRE(counts.spawn_sword >= 24);
    ARPG_REQUIRE(counts.damage == 12);
    ARPG_REQUIRE(counts.finisher_damage == 1);
    ARPG_REQUIRE(counts.invulnerability_on == 1);
    ARPG_REQUIRE(counts.invulnerability_off == 1);
    return {};
}

arpg::test::Failure invalid_skills_have_no_timeline() noexcept {
    ARPG_REQUIRE(active_skill_clip(ActiveSkillId::none) == nullptr);
    ARPG_REQUIRE(active_skill_events_at(ActiveSkillId::none, 0U).empty());
    ARPG_REQUIRE(active_skill_events_at(static_cast<ActiveSkillId>(99U), 0U).empty());
    return {};
}

arpg::test::Failure timeline_events_use_a_fixed_capacity_cxx17_view() noexcept {
    const ActiveSkillTimelineEventRange empty =
        active_skill_events_at(ActiveSkillId::none, 0U);
    ARPG_REQUIRE(empty.empty());
    const ActiveSkillTimelineEventRange strike =
        active_skill_events_at(ActiveSkillId::draw_slash, 46U);
    ARPG_REQUIRE(strike.size() == 1U);
    ARPG_REQUIRE(strike[0U].kind == ActiveSkillTimelineEventKind::damage);
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"draw slash clip has locked timing", &draw_slash_clip_has_locked_timing},
    {"storm swords clip has locked event counts", &storm_swords_clip_has_locked_event_counts},
    {"invalid skills have no timeline", &invalid_skills_have_no_timeline},
    {"events use a fixed capacity C++17 view",
        &timeline_events_use_a_fixed_capacity_cxx17_view},
};

}  // namespace

arpg::test::TestSuite active_skill_timeline_suite() noexcept {
    return arpg::test::make_suite("active_skill_timeline", kCases);
}
