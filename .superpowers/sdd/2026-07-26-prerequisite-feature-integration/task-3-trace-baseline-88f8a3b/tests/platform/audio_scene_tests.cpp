#include "test_framework.hpp"

#include "allocation_probe.hpp"
#include "audio_scene.hpp"

#include <cstdint>

namespace {

using namespace arpg::platform;

arpg::test::Failure initial_scene_starts_without_silence() noexcept {
    AudioSceneState state{};
    const auto plan = state.update({}, {}, 0.0F);
    ARPG_REQUIRE(plan.music == MusicTrack::explore);
    ARPG_REQUIRE(plan.ambience == AmbienceTrack::room);
    ARPG_REQUIRE(plan.music_changed && plan.ambience_changed);
    ARPG_REQUIRE(arpg::test::near(plan.music_gain[0], 0.45, 1.0e-6));
    ARPG_REQUIRE(arpg::test::near(plan.ambience_gain[0], 0.35, 1.0e-6));
    return {};
}

arpg::test::Failure public_scene_flags_select_tracks() noexcept {
    AudioSceneState state{};
    static_cast<void>(state.update({}, {}, 0.0F));
    const auto plan = state.update({true, true, false, false}, {}, 0.0F);
    ARPG_REQUIRE(plan.music == MusicTrack::combat);
    ARPG_REQUIRE(plan.ambience == AmbienceTrack::abyss);
    ARPG_REQUIRE(plan.music_changed && plan.ambience_changed);
    return {};
}

arpg::test::Failure music_crossfades_in_four_tenths() noexcept {
    AudioSceneState state{};
    static_cast<void>(state.update({}, {}, 0.0F));
    const auto half = state.update({true, false, false, false}, {}, 0.20F);
    ARPG_REQUIRE(arpg::test::near(half.music_gain[0], 0.225, 1.0e-6));
    ARPG_REQUIRE(arpg::test::near(half.music_gain[1], 0.225, 1.0e-6));
    const auto done = state.update({true, false, false, false}, {}, 0.20F);
    ARPG_REQUIRE(arpg::test::near(done.music_gain[0], 0.0));
    ARPG_REQUIRE(arpg::test::near(done.music_gain[1], 0.45, 1.0e-6));
    return {};
}

arpg::test::Failure ambience_crossfades_in_quarter_second() noexcept {
    AudioSceneState state{};
    static_cast<void>(state.update({}, {}, 0.0F));
    const auto half = state.update({false, true, false, false}, {}, 0.125F);
    ARPG_REQUIRE(arpg::test::near(half.ambience_gain[0], 0.175, 1.0e-6));
    ARPG_REQUIRE(arpg::test::near(half.ambience_gain[1], 0.175, 1.0e-6));
    const auto done = state.update({false, true, false, false}, {}, 0.125F);
    ARPG_REQUIRE(arpg::test::near(done.ambience_gain[0], 0.0));
    ARPG_REQUIRE(arpg::test::near(done.ambience_gain[1], 0.35, 1.0e-6));
    return {};
}

arpg::test::Failure stable_targets_do_not_restart() noexcept {
    AudioSceneState state{};
    static_cast<void>(state.update({true, true, false, false}, {}, 0.0F));
    const auto plan = state.update({true, true, false, false}, {}, 5.0F);
    ARPG_REQUIRE(!plan.music_changed && !plan.ambience_changed);
    return {};
}

arpg::test::Failure overlays_duck_only_long_streams() noexcept {
    AudioSceneState state{};
    const auto paused = state.update({false, false, true, false}, {}, 0.0F);
    ARPG_REQUIRE(arpg::test::near(paused.music_gain[0], 0.45 * 0.55, 1.0e-6));
    ARPG_REQUIRE(arpg::test::near(paused.ambience_gain[0], 0.35 * 0.55, 1.0e-6));
    ARPG_REQUIRE(arpg::test::near(paused.sfx_gain, 1.0));
    ARPG_REQUIRE(arpg::test::near(paused.ui_gain, 0.8, 1.0e-6));
    state.reset();
    const auto dead = state.update({false, false, true, true}, {}, 0.0F);
    ARPG_REQUIRE(arpg::test::near(dead.music_gain[0], 0.45 * 0.35, 1.0e-6));
    return {};
}

arpg::test::Failure five_bus_gain_is_multiplicative_and_clamped() noexcept {
    AudioSceneState state{};
    const AudioBusLevels levels{50, 80, 60, 40, 20};
    const auto plan = state.update({}, levels, 0.0F);
    ARPG_REQUIRE(arpg::test::near(plan.sfx_gain, 0.4, 1.0e-6));
    ARPG_REQUIRE(arpg::test::near(plan.music_gain[0], 0.3, 1.0e-6));
    ARPG_REQUIRE(arpg::test::near(plan.ambience_gain[0], 0.2, 1.0e-6));
    ARPG_REQUIRE(arpg::test::near(plan.ui_gain, 0.1, 1.0e-6));
    state.reset();
    const auto clamped = state.update({}, {255,255,255,255,255}, 0.0F);
    ARPG_REQUIRE(arpg::test::near(clamped.sfx_gain, 1.0));
    return {};
}

arpg::test::Failure ui_edges_and_scene_updates_allocate_nothing() noexcept {
    UiAudioCueGate gate{};
    const auto navigate = ui_audio_cue_mask(UiAudioCue::navigate);
    const auto confirm = ui_audio_cue_mask(UiAudioCue::confirm);
    ARPG_REQUIRE(gate.rising(navigate) == navigate);
    ARPG_REQUIRE(gate.rising(navigate) == 0U);
    ARPG_REQUIRE(gate.rising(static_cast<UiAudioCueMask>(navigate | confirm)) == confirm);
    ARPG_REQUIRE(gate.rising(0U) == 0U);
    ARPG_REQUIRE(gate.rising(navigate) == navigate);

    AudioSceneState state{};
    std::uint64_t checksum{};
    const auto before = arpg::test::allocation_count();
    for (std::uint32_t index{}; index < 100000U; ++index) {
        const auto plan = state.update(
            {(index & 1U) != 0U, (index & 2U) != 0U, false, false},
            {}, 1.0F / 60.0F);
        checksum += static_cast<std::uint64_t>(plan.music_gain[0] * 1000.0F);
    }
    const auto after = arpg::test::allocation_count();
    ARPG_REQUIRE(after == before);
    ARPG_REQUIRE(checksum != 0U);
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"initial scene", &initial_scene_starts_without_silence},
    {"scene track selection", &public_scene_flags_select_tracks},
    {"music crossfade", &music_crossfades_in_four_tenths},
    {"ambience crossfade", &ambience_crossfades_in_quarter_second},
    {"stable target", &stable_targets_do_not_restart},
    {"overlay duck", &overlays_duck_only_long_streams},
    {"five bus gain", &five_bus_gain_is_multiplicative_and_clamped},
    {"UI edges and zero allocation", &ui_edges_and_scene_updates_allocate_nothing},
};

}  // namespace

arpg::test::TestSuite audio_scene_suite() noexcept {
    return arpg::test::make_suite("audio_scene", kCases);
}
