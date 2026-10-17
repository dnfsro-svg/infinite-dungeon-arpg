#include "test_framework.hpp"

#include "raylib_input.hpp"

namespace {

namespace platform = arpg::platform;

constexpr platform::FrameKeyState all_keys() noexcept {
    return {true, true, true, true, true,
        true, true, true, true, true, true};
}

arpg::test::Failure saving_allows_only_global_controls() noexcept {
    const auto gate = platform::death_input_gate(true, false, all_keys());
    ARPG_REQUIRE(!gate.continue_death);
    ARPG_REQUIRE(gate.screenshot);
    ARPG_REQUIRE(gate.debug_toggle);
    ARPG_REQUIRE(gate.exit);
    ARPG_REQUIRE(!gate.forward_gameplay);
    return {};
}

arpg::test::Failure pending_allows_continue_and_global_controls() noexcept {
    const auto gate = platform::death_input_gate(false, true, all_keys());
    ARPG_REQUIRE(gate.continue_death);
    ARPG_REQUIRE(gate.screenshot);
    ARPG_REQUIRE(gate.debug_toggle);
    ARPG_REQUIRE(gate.exit);
    ARPG_REQUIRE(!gate.forward_gameplay);
    return {};
}

arpg::test::Failure saving_wins_when_both_death_flags_are_set() noexcept {
    const auto gate = platform::death_input_gate(true, true, all_keys());
    ARPG_REQUIRE(!gate.continue_death);
    ARPG_REQUIRE(gate.screenshot);
    ARPG_REQUIRE(gate.debug_toggle);
    ARPG_REQUIRE(gate.exit);
    ARPG_REQUIRE(!gate.forward_gameplay);
    return {};
}

arpg::test::Failure nondeath_forwards_gameplay_without_stealing_e() noexcept {
    const auto gate = platform::death_input_gate(false, false, all_keys());
    ARPG_REQUIRE(!gate.continue_death);
    ARPG_REQUIRE(gate.screenshot);
    ARPG_REQUIRE(gate.debug_toggle);
    ARPG_REQUIRE(gate.exit);
    ARPG_REQUIRE(gate.forward_gameplay);
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"saving allows only global controls", &saving_allows_only_global_controls},
    {"pending allows continue and global controls", &pending_allows_continue_and_global_controls},
    {"saving wins when both flags are set", &saving_wins_when_both_death_flags_are_set},
    {"nondeath forwards gameplay without stealing e", &nondeath_forwards_gameplay_without_stealing_e},
};

}  // namespace

arpg::test::TestSuite death_input_gate_suite() noexcept {
    return arpg::test::make_suite("death_input_gate", kCases);
}
