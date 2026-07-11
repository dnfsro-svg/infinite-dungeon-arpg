#include "test_framework.hpp"

#include "combat/input_buffer.hpp"

namespace {

using namespace arpg::combat;

static_assert(InputBuffer::kCapacity == 32);
static_assert(InputBuffer::kLifetimeTicks == 8);

arpg::test::Failure consumes_oldest_match_without_head_of_line_blocking() noexcept {
    InputBuffer buffer;
    ARPG_REQUIRE(buffer.push(Action::jump));
    ARPG_REQUIRE(buffer.push(Action::light));
    buffer.age(false);
    ARPG_REQUIRE(buffer.push(Action::launcher));
    ARPG_REQUIRE(buffer.push(Action::light));

    ARPG_REQUIRE(buffer.consume(Action::light));
    for (int tick = 0; tick < 7; ++tick) {
        buffer.age(false);
    }

    ARPG_REQUIRE(buffer.expired_count() == 1);
    ARPG_REQUIRE(buffer.consume(Action::light));
    ARPG_REQUIRE(buffer.consume(Action::launcher));
    ARPG_REQUIRE(buffer.size() == 0);
    return {};
}

arpg::test::Failure entries_live_for_eight_active_ticks_and_pause_does_not_age() noexcept {
    InputBuffer buffer;
    ARPG_REQUIRE(buffer.push(Action::light));
    for (int tick = 0; tick < 16; ++tick) {
        buffer.age(true);
    }
    for (int tick = 0; tick < 7; ++tick) {
        buffer.age(false);
    }
    ARPG_REQUIRE(buffer.consume(Action::light));

    ARPG_REQUIRE(buffer.push(Action::launcher));
    buffer.age(true);
    for (int tick = 0; tick < 8; ++tick) {
        buffer.age(false);
    }
    ARPG_REQUIRE(!buffer.consume(Action::launcher));
    ARPG_REQUIRE(buffer.expired_count() == 1);
    return {};
}

arpg::test::Failure thirty_third_push_overflows_without_mutating_entries() noexcept {
    InputBuffer buffer;
    for (std::size_t index = 0; index < InputBuffer::kCapacity; ++index) {
        ARPG_REQUIRE(buffer.push(Action::light));
    }

    ARPG_REQUIRE(!buffer.push(Action::launcher));
    ARPG_REQUIRE(buffer.size() == InputBuffer::kCapacity);
    ARPG_REQUIRE(buffer.overflow_count() == 1);
    ARPG_REQUIRE(buffer.expired_count() == 0);
    ARPG_REQUIRE(!buffer.consume(Action::launcher));
    ARPG_REQUIRE(buffer.size() == InputBuffer::kCapacity);
    return {};
}

arpg::test::Failure expiry_is_stable_and_clear_preserves_diagnostics() noexcept {
    InputBuffer buffer;
    ARPG_REQUIRE(buffer.push(Action::light));
    buffer.age(false);
    ARPG_REQUIRE(buffer.push(Action::launcher));
    buffer.age(false);
    ARPG_REQUIRE(buffer.push(Action::jump));
    buffer.age(false);
    ARPG_REQUIRE(buffer.push(Action::launcher));

    for (int tick = 0; tick < 5; ++tick) {
        buffer.age(false);
    }
    ARPG_REQUIRE(buffer.expired_count() == 1);
    ARPG_REQUIRE(buffer.size() == 3);

    ARPG_REQUIRE(buffer.consume(Action::launcher));
    buffer.age(false);
    ARPG_REQUIRE(buffer.expired_count() == 1);
    ARPG_REQUIRE(buffer.consume(Action::launcher));
    ARPG_REQUIRE(buffer.consume(Action::jump));

    for (std::size_t index = 0; index < InputBuffer::kCapacity; ++index) {
        ARPG_REQUIRE(buffer.push(Action::jump));
    }
    ARPG_REQUIRE(!buffer.push(Action::launcher));
    buffer.clear();
    ARPG_REQUIRE(buffer.size() == 0);
    ARPG_REQUIRE(!buffer.consume(Action::jump));
    ARPG_REQUIRE(buffer.expired_count() == 1);
    ARPG_REQUIRE(buffer.overflow_count() == 1);

    buffer.reset_diagnostics();
    ARPG_REQUIRE(buffer.expired_count() == 0);
    ARPG_REQUIRE(buffer.overflow_count() == 0);
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"oldest match without head-of-line blocking",
     &consumes_oldest_match_without_head_of_line_blocking},
    {"eight active ticks and paused aging",
     &entries_live_for_eight_active_ticks_and_pause_does_not_age},
    {"bounded overflow", &thirty_third_push_overflows_without_mutating_entries},
    {"stable expiry, clear, and diagnostics",
     &expiry_is_stable_and_clear_preserves_diagnostics},
};

}  // namespace

arpg::test::TestSuite input_buffer_suite() noexcept {
    return arpg::test::make_suite("input_buffer", kCases);
}
