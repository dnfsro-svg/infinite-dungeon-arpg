#include "test_framework.hpp"

#include "combat_audio.hpp"
#include "combat_feedback.hpp"

#include <cstddef>

namespace {

using namespace arpg::combat;
using namespace arpg::platform;

arpg::test::Failure multi_target_feedback_uses_one_max_shake() noexcept {
    CombatEvent level_hit{};
    level_hit.kind = CombatEventKind::hit;
    level_hit.target_index = 0;
    CombatFeedback levels;
    level_hit.feedback = FeedbackLevel::light;
    levels.consume(level_hit);
    ARPG_REQUIRE(arpg::test::near(
        levels.shake_amplitude(), 2.0, 1.0e-4));
    levels.update(0.09F);
    ARPG_REQUIRE(levels.shake_amplitude() > 0.0F);
    levels.update(0.02F);
    ARPG_REQUIRE(levels.shake_amplitude() == 0.0F);

    levels.clear();
    level_hit.feedback = FeedbackLevel::medium;
    levels.consume(level_hit);
    ARPG_REQUIRE(arpg::test::near(
        levels.shake_amplitude(), 5.0, 1.0e-4));
    levels.update(0.10F);
    ARPG_REQUIRE(levels.shake_amplitude() > 0.0F);
    levels.update(0.05F);
    ARPG_REQUIRE(levels.shake_amplitude() == 0.0F);

    CombatFeedback feedback;
    for (std::uint8_t index = 0; index < 3; ++index) {
        CombatEvent hit{};
        hit.kind = CombatEventKind::hit;
        hit.tick = 10;
        hit.target_index = index;
        hit.feedback = FeedbackLevel::heavy;
        hit.position = Vec3{static_cast<float>(index), 0.0F, 0.0F};
        hit.value = 90;
        feedback.consume(hit);
    }

    ARPG_REQUIRE(feedback.active_count() == 6);
    ARPG_REQUIRE(arpg::test::near(
        feedback.shake_amplitude(), 9.0, 1.0e-4));
    level_hit.feedback = FeedbackLevel::light;
    feedback.consume(level_hit);
    ARPG_REQUIRE(arpg::test::near(
        feedback.shake_amplitude(), 9.0, 1.0e-4));
    ARPG_REQUIRE(feedback.target_flash_seconds(0) > 0.0F);
    ARPG_REQUIRE(feedback.target_flash_seconds(1) > 0.0F);
    ARPG_REQUIRE(feedback.target_flash_seconds(2) > 0.0F);
    const CameraOffset first = feedback.camera_offset();
    feedback.update(0.05F);
    const CameraOffset second = feedback.camera_offset();
    ARPG_REQUIRE(first.x != second.x || first.y != second.y);
    feedback.update(1.0F);
    ARPG_REQUIRE(feedback.shake_amplitude() > 0.0F);
    feedback.update(1.0F);
    ARPG_REQUIRE(arpg::test::near(
        feedback.shake_amplitude(), 0.0, 1.0e-4));
    return {};
}

arpg::test::Failure fixed_pool_overflow_and_reset_are_exact() noexcept {
    CombatFeedback feedback;
    VisualEffect effect{};
    effect.active = true;
    effect.kind = VisualEffectKind::spark;
    effect.position = Vec3{1.0F, 2.0F, 3.0F};
    effect.lifetime_seconds = 1.0F;

    for (std::size_t index = 0; index < CombatFeedback::kCapacity; ++index) {
        ARPG_REQUIRE(feedback.try_spawn(effect));
    }
    ARPG_REQUIRE(feedback.active_count() == CombatFeedback::kCapacity);
    ARPG_REQUIRE(!feedback.try_spawn(effect));
    ARPG_REQUIRE(feedback.dropped_count() == 1);

    CombatEvent reset{};
    reset.kind = CombatEventKind::reset;
    feedback.consume(reset);
    ARPG_REQUIRE(feedback.active_count() == 0);
    ARPG_REQUIRE(feedback.dropped_count() == 0);
    ARPG_REQUIRE(arpg::test::near(
        feedback.shake_amplitude(), 0.0, 1.0e-4));
    for (std::size_t index = 0; index < kDummyCount; ++index) {
        ARPG_REQUIRE(feedback.target_flash_seconds(index) == 0.0F);
    }
    return {};
}

arpg::test::Failure player_hit_keeps_a_short_source_indicator() noexcept {
    CombatFeedback feedback;
    CombatEvent hit{};
    hit.kind = CombatEventKind::player_hit;
    hit.position = Vec3{-2.0F, 1.5F, 0.0F};
    feedback.consume(hit);
    ARPG_REQUIRE(arpg::test::near(
        feedback.player_hit_indicator_seconds(), 0.55, 1.0e-4));
    const Vec3 source = feedback.player_hit_source();
    ARPG_REQUIRE(source.x == -2.0F && source.y == 1.5F);
    for (int frame = 0; frame < 6; ++frame) {
        feedback.update(0.1F);
    }
    ARPG_REQUIRE(arpg::test::near(
        feedback.player_hit_indicator_seconds(), 0.0, 1.0e-4));
    return {};
}

arpg::test::Failure defeated_event_spawns_one_distinct_marker() noexcept {
    CombatFeedback feedback;
    CombatEvent defeated{};
    defeated.kind = CombatEventKind::defeated;
    defeated.position = Vec3{3.0F, -1.0F, 0.0F};
    feedback.consume(defeated);
    ARPG_REQUIRE(feedback.active_count() == 1U);
    const auto& effects = feedback.effects();
    ARPG_REQUIRE(effects[0].active);
    ARPG_REQUIRE(effects[0].kind == VisualEffectKind::defeat_marker);
    for (int frame = 0; frame < 7; ++frame) {
        feedback.update(0.1F);
    }
    ARPG_REQUIRE(feedback.active_count() == 0U);
    return {};
}

arpg::test::Failure audio_routes_weapon_material_and_low_once() noexcept {
    CombatEvent event{};
    event.kind = CombatEventKind::swing;
    ARPG_REQUIRE(route_audio_cues(event)
                 == audio_cue_mask(AudioCue::weapon));

    event.kind = CombatEventKind::hit;
    event.feedback = FeedbackLevel::heavy;
    ARPG_REQUIRE(route_audio_cues(event)
                 == audio_cue_mask(AudioCue::material));

    event.kind = CombatEventKind::impact_summary;
    event.feedback = FeedbackLevel::heavy;
    ARPG_REQUIRE(route_audio_cues(event)
                 == audio_cue_mask(AudioCue::low));
    event.feedback = FeedbackLevel::medium;
    ARPG_REQUIRE(route_audio_cues(event) == 0);

    event.kind = CombatEventKind::reset;
    ARPG_REQUIRE(route_audio_cues(event) == 0);
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"multi-target max feedback",
     &multi_target_feedback_uses_one_max_shake},
    {"fixed pool overflow and reset",
     &fixed_pool_overflow_and_reset_are_exact},
    {"player hit source indicator", &player_hit_keeps_a_short_source_indicator},
    {"defeated marker", &defeated_event_spawns_one_distinct_marker},
    {"pure audio cue routing",
     &audio_routes_weapon_material_and_low_once},
};

}  // namespace

arpg::test::TestSuite combat_feedback_suite() noexcept {
    return arpg::test::make_suite("combat_feedback", kCases);
}
