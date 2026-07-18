#include "test_framework.hpp"

#include "core/fixed_step.hpp"
#include "dungeon/dungeon_session.hpp"
#include "raylib_host.hpp"

#include <cstdint>

namespace {

namespace combat = arpg::combat;
namespace core = arpg::core;
namespace dungeon = arpg::dungeon;
namespace platform = arpg::platform;

void run_frame(dungeon::DungeonSession& session,
    const platform::HostFrameGateResult& frame,
    bool request_action) noexcept {
    if (frame.forward_gameplay && request_action) {
        static_cast<void>(session.queue_action(combat::Action::light));
    }
    for (std::uint32_t step = 0U; step < frame.fixed_step.steps; ++step) {
        session.tick({1, 0});
    }
}

arpg::test::Failure six_hundred_paused_presented_frames_freeze_simulation()
    noexcept {
    dungeon::DungeonSession session{};
    core::FixedStepRunner fixed_step{};
    bool pause_latched = false;

    const auto partial = platform::gate_host_frame(
        fixed_step, pause_latched, false,
        core::FixedStepRunner::kStepSeconds * 0.5);
    ARPG_REQUIRE(partial.forward_gameplay);
    ARPG_REQUIRE(partial.fixed_step.steps == 0U);

    const auto entered = platform::gate_host_frame(
        fixed_step, pause_latched, true,
        core::FixedStepRunner::kStepSeconds);
    ARPG_REQUIRE(!entered.forward_gameplay);
    ARPG_REQUIRE(entered.fixed_step.steps == 0U);
    ARPG_REQUIRE(pause_latched);

    const dungeon::DungeonSnapshot frozen = session.snapshot();
    for (int frame_index = 0; frame_index < 600; ++frame_index) {
        const auto paused = platform::gate_host_frame(
            fixed_step, pause_latched, true, 1.0);
        ARPG_REQUIRE(!paused.forward_gameplay);
        ARPG_REQUIRE(paused.fixed_step.steps == 0U);
        run_frame(session, paused, true);
    }

    const dungeon::DungeonSnapshot after = session.snapshot();
    ARPG_REQUIRE(after.session_tick == frozen.session_tick);
    ARPG_REQUIRE(after.phase == frozen.phase);
    ARPG_REQUIRE(after.room_seed == frozen.room_seed);
    ARPG_REQUIRE(after.remaining_targets == frozen.remaining_targets);
    ARPG_REQUIRE(after.combat.has_value() == frozen.combat.has_value());
    if (after.combat.has_value()) {
        ARPG_REQUIRE(after.combat->tick == frozen.combat->tick);
        ARPG_REQUIRE(after.combat->player.position.x
            == frozen.combat->player.position.x);
        ARPG_REQUIRE(after.combat->player.position.y
            == frozen.combat->player.position.y);
        ARPG_REQUIRE(after.combat->player.hp == frozen.combat->player.hp);
        ARPG_REQUIRE(after.combat->diagnostics.input_size
            == frozen.combat->diagnostics.input_size);
    }

    const auto resumed = platform::gate_host_frame(
        fixed_step, pause_latched, false,
        core::FixedStepRunner::kStepSeconds);
    ARPG_REQUIRE(resumed.forward_gameplay);
    ARPG_REQUIRE(!pause_latched);
    ARPG_REQUIRE(resumed.fixed_step.steps == 1U);
    ARPG_REQUIRE(resumed.fixed_step.steps
        <= core::FixedStepRunner::kMaxStepsPerFrame);
    run_frame(session, resumed, false);
    ARPG_REQUIRE(session.snapshot().session_tick == frozen.session_tick + 1U);
    return {};
}

arpg::test::Failure pause_entry_discards_same_frame_action_and_accumulator()
    noexcept {
    dungeon::DungeonSession session{};
    core::FixedStepRunner fixed_step{};
    bool pause_latched = false;

    const auto partial = platform::gate_host_frame(
        fixed_step, pause_latched, false,
        core::FixedStepRunner::kStepSeconds * 0.75);
    run_frame(session, partial, false);
    const auto before = session.snapshot();

    const auto entered = platform::gate_host_frame(
        fixed_step, pause_latched, true,
        core::FixedStepRunner::kStepSeconds);
    run_frame(session, entered, true);
    ARPG_REQUIRE(!entered.forward_gameplay);
    ARPG_REQUIRE(session.snapshot().session_tick == before.session_tick);

    const auto resumed = platform::gate_host_frame(
        fixed_step, pause_latched, false,
        core::FixedStepRunner::kStepSeconds * 0.25);
    ARPG_REQUIRE(resumed.fixed_step.steps == 0U);
    ARPG_REQUIRE(resumed.fixed_step.total_ticks == 0U);
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"600 paused frames freeze simulation",
        &six_hundred_paused_presented_frames_freeze_simulation},
    {"pause entry discards action and accumulator",
        &pause_entry_discards_same_frame_action_and_accumulator},
};

}  // namespace

arpg::test::TestSuite pause_host_gate_suite() noexcept {
    return arpg::test::make_suite("pause_host_gate", kCases);
}
