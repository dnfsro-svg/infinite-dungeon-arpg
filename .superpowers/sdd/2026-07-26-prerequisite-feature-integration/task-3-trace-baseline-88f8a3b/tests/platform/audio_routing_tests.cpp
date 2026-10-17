#include "test_framework.hpp"

#include "allocation_probe.hpp"
#include "audio_routing.hpp"

#include <array>
#include <cstdint>
#include <limits>

namespace {

using arpg::combat::AttackId;
using arpg::combat::CombatEvent;
using arpg::combat::CombatEventKind;
using arpg::combat::FeedbackLevel;
using arpg::platform::AudioAssetId;
using arpg::platform::AudioCue;
using arpg::platform::AudioPlaybackBudget;
using arpg::platform::AudioSelectionState;
using arpg::platform::route_audio_plan;

arpg::test::Failure light_swing_attacks_share_the_light_cue() noexcept {
    constexpr std::array<AttackId, 3> kLightAttacks{{
        AttackId::j1, AttackId::j2, AttackId::air_j,
    }};
    CombatEvent event{};
    event.kind = CombatEventKind::swing;
    for (const AttackId attack : kLightAttacks) {
        event.attack = attack;
        ARPG_REQUIRE(route_audio_plan(event).cues
            == arpg::platform::audio_cue_mask(AudioCue::swing_light));
    }
    event.attack = AttackId::none;
    ARPG_REQUIRE(route_audio_plan(event).cues == 0U);
    return {};
}

arpg::test::Failure finisher_and_launcher_use_distinct_swing_cues() noexcept {
    CombatEvent event{};
    event.kind = CombatEventKind::swing;
    event.attack = AttackId::j3;
    ARPG_REQUIRE(route_audio_plan(event).cues
        == arpg::platform::audio_cue_mask(AudioCue::swing_finisher));
    event.attack = AttackId::launcher;
    ARPG_REQUIRE(route_audio_plan(event).cues
        == arpg::platform::audio_cue_mask(AudioCue::swing_launcher));
    return {};
}

arpg::test::Failure combat_events_route_their_dedicated_cues() noexcept {
    CombatEvent event{};
    event.kind = CombatEventKind::hit;
    ARPG_REQUIRE(route_audio_plan(event).cues
        == arpg::platform::audio_cue_mask(AudioCue::impact));

    event.kind = CombatEventKind::impact_summary;
    event.feedback = FeedbackLevel::heavy;
    ARPG_REQUIRE(route_audio_plan(event).cues
        == arpg::platform::audio_cue_mask(AudioCue::impact_low));
    event.feedback = FeedbackLevel::medium;
    ARPG_REQUIRE(route_audio_plan(event).cues == 0U);

    event.kind = CombatEventKind::player_hit;
    ARPG_REQUIRE(route_audio_plan(event).cues
        == arpg::platform::audio_cue_mask(AudioCue::player_hurt));
    event.kind = CombatEventKind::landing;
    ARPG_REQUIRE(route_audio_plan(event).cues
        == arpg::platform::audio_cue_mask(AudioCue::landing));
    event.kind = CombatEventKind::defeated;
    ARPG_REQUIRE(route_audio_plan(event).cues
        == arpg::platform::audio_cue_mask(AudioCue::enemy_defeat));

    constexpr std::array<CombatEventKind, 6> kSilentKinds{{
        CombatEventKind::break_started, CombatEventKind::respawned,
        CombatEventKind::reset, CombatEventKind::player_hurt_started,
        CombatEventKind::player_health_reset, CombatEventKind::player_defeated,
    }};
    for (const CombatEventKind kind : kSilentKinds) {
        event.kind = kind;
        ARPG_REQUIRE(route_audio_plan(event).cues == 0U);
    }
    return {};
}

arpg::test::Failure warning_events_route_independent_cues() noexcept {
    constexpr std::array<CombatEventKind, 3> kWarningEvents{{
        CombatEventKind::affix_blink_warning,
        CombatEventKind::affix_chain_warning,
        CombatEventKind::affix_death_warning,
    }};
    constexpr std::array<AudioCue, 3> kWarningCues{{
        AudioCue::warning_blink, AudioCue::warning_chain,
        AudioCue::warning_death,
    }};
    CombatEvent event{};
    for (std::size_t index{}; index < kWarningEvents.size(); ++index) {
        event.kind = kWarningEvents[index];
        ARPG_REQUIRE(route_audio_plan(event).cues
            == arpg::platform::audio_cue_mask(kWarningCues[index]));
    }
    return {};
}

arpg::test::Failure selection_rotates_two_and_three_way_variants() noexcept {
    AudioSelectionState selection{};
    ARPG_REQUIRE(selection.select(AudioCue::swing_light)
        == AudioAssetId::swing_light_1);
    ARPG_REQUIRE(selection.select(AudioCue::swing_light)
        == AudioAssetId::swing_light_2);
    ARPG_REQUIRE(selection.select(AudioCue::swing_light)
        == AudioAssetId::swing_light_1);

    ARPG_REQUIRE(selection.select(AudioCue::impact) == AudioAssetId::impact_1);
    ARPG_REQUIRE(selection.select(AudioCue::impact) == AudioAssetId::impact_2);
    ARPG_REQUIRE(selection.select(AudioCue::impact) == AudioAssetId::impact_3);
    ARPG_REQUIRE(selection.select(AudioCue::impact) == AudioAssetId::impact_1);
    return {};
}

arpg::test::Failure selection_reset_restarts_every_rotation() noexcept {
    AudioSelectionState selection{};
    static_cast<void>(selection.select(AudioCue::swing_light));
    static_cast<void>(selection.select(AudioCue::impact));
    static_cast<void>(selection.select(AudioCue::impact));
    selection.reset();
    ARPG_REQUIRE(selection.select(AudioCue::swing_light)
        == AudioAssetId::swing_light_1);
    ARPG_REQUIRE(selection.select(AudioCue::impact) == AudioAssetId::impact_1);
    ARPG_REQUIRE(selection.select(AudioCue::swing_finisher)
        == AudioAssetId::swing_finisher);
    ARPG_REQUIRE(selection.select(AudioCue::swing_launcher)
        == AudioAssetId::swing_launcher);
    ARPG_REQUIRE(selection.select(AudioCue::impact_low)
        == AudioAssetId::impact_low);
    ARPG_REQUIRE(selection.select(AudioCue::player_hurt)
        == AudioAssetId::player_hurt);
    ARPG_REQUIRE(selection.select(AudioCue::landing) == AudioAssetId::landing);
    ARPG_REQUIRE(selection.select(AudioCue::enemy_defeat)
        == AudioAssetId::enemy_defeat);
    ARPG_REQUIRE(selection.select(AudioCue::warning_blink)
        == AudioAssetId::warning_blink);
    ARPG_REQUIRE(selection.select(AudioCue::warning_chain)
        == AudioAssetId::warning_chain);
    ARPG_REQUIRE(selection.select(AudioCue::warning_death)
        == AudioAssetId::warning_death);
    return {};
}

arpg::test::Failure playback_budget_enforces_each_cue_category_per_tick() noexcept {
    constexpr std::uint64_t kTick = 67U;
    AudioPlaybackBudget budget{};
    ARPG_REQUIRE(budget.allow(AudioCue::swing_light, kTick));
    ARPG_REQUIRE(!budget.allow(AudioCue::swing_finisher, kTick));
    ARPG_REQUIRE(!budget.allow(AudioCue::swing_launcher, kTick));

    ARPG_REQUIRE(budget.allow(AudioCue::impact, kTick));
    ARPG_REQUIRE(budget.allow(AudioCue::impact, kTick));
    ARPG_REQUIRE(budget.allow(AudioCue::impact, kTick));
    ARPG_REQUIRE(!budget.allow(AudioCue::impact, kTick));

    constexpr std::array<AudioCue, 6> kSingleVoiceCues{{
        AudioCue::impact_low, AudioCue::player_hurt, AudioCue::landing,
        AudioCue::warning_blink, AudioCue::warning_chain, AudioCue::warning_death,
    }};
    for (const AudioCue cue : kSingleVoiceCues) {
        ARPG_REQUIRE(budget.allow(cue, kTick));
        ARPG_REQUIRE(!budget.allow(cue, kTick));
    }

    ARPG_REQUIRE(budget.allow(AudioCue::enemy_defeat, kTick));
    ARPG_REQUIRE(budget.allow(AudioCue::enemy_defeat, kTick));
    ARPG_REQUIRE(!budget.allow(AudioCue::enemy_defeat, kTick));
    return {};
}

arpg::test::Failure warning_budget_throttles_resets_wraps_and_never_allocates() noexcept {
    AudioPlaybackBudget budget{};
    ARPG_REQUIRE(budget.allow(AudioCue::warning_blink, 100U));
    ARPG_REQUIRE(!budget.allow(AudioCue::warning_blink, 111U));
    ARPG_REQUIRE(budget.allow(AudioCue::warning_chain, 111U));
    ARPG_REQUIRE(budget.allow(AudioCue::warning_death, 111U));
    ARPG_REQUIRE(budget.allow(AudioCue::warning_blink, 112U));
    budget.reset();
    ARPG_REQUIRE(budget.allow(AudioCue::warning_blink, 112U));

    AudioPlaybackBudget near_wrap{};
    constexpr std::uint64_t kMax = (std::numeric_limits<std::uint64_t>::max)();
    ARPG_REQUIRE(near_wrap.allow(AudioCue::warning_blink, kMax - 5U));
    ARPG_REQUIRE(!near_wrap.allow(AudioCue::warning_blink, kMax - 1U));
    ARPG_REQUIRE(near_wrap.allow(AudioCue::warning_blink, 2U));

    AudioSelectionState selection{};
    CombatEvent event{};
    event.kind = CombatEventKind::swing;
    event.attack = AttackId::j1;
    std::uint64_t checksum{};
    const std::uint64_t before = arpg::test::allocation_count();
    for (std::uint64_t tick{}; tick < 100000U; ++tick) {
        event.tick = tick;
        const auto plan = route_audio_plan(event);
        if (plan.cues != 0U && budget.allow(AudioCue::swing_light, tick)) {
            checksum += static_cast<std::uint8_t>(
                selection.select(AudioCue::swing_light));
        }
    }
    const std::uint64_t after = arpg::test::allocation_count();
    ARPG_REQUIRE(after == before);
    ARPG_REQUIRE(checksum != 0U);
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"light swing cue routing", &light_swing_attacks_share_the_light_cue},
    {"finisher and launcher cue routing", &finisher_and_launcher_use_distinct_swing_cues},
    {"combat event cue routing", &combat_events_route_their_dedicated_cues},
    {"warning cue routing", &warning_events_route_independent_cues},
    {"two and three way selection", &selection_rotates_two_and_three_way_variants},
    {"selection reset", &selection_reset_restarts_every_rotation},
    {"per tick playback budget", &playback_budget_enforces_each_cue_category_per_tick},
    {"warning throttle wrap and allocation", &warning_budget_throttles_resets_wraps_and_never_allocates},
};

}  // namespace

arpg::test::TestSuite audio_routing_suite() noexcept {
    return arpg::test::make_suite("audio_routing", kCases);
}
