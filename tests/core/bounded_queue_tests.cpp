#include "allocation_probe.hpp"
#include "test_framework.hpp"

#include "core/bounded_queue.hpp"

#include <cstdint>
#include <utility>

namespace {

struct ConstructedProbe final {
    ConstructedProbe(
        int initial,
        int* construction_count) noexcept
        : value(initial),
          constructions(construction_count) {
        ++(*constructions);
    }

    ConstructedProbe(const ConstructedProbe& other) noexcept
        : value(other.value),
          constructions(other.constructions) {
        ++(*constructions);
    }

    ConstructedProbe(ConstructedProbe&& other) noexcept
        : value(other.value),
          constructions(other.constructions) {
        ++(*constructions);
    }

    int value;
    int* constructions;
};

struct DestructionProbe final {
    explicit DestructionProbe(int* count) noexcept
        : destroyed(count) {}

    DestructionProbe(DestructionProbe&& other) noexcept
        : destroyed(other.destroyed) {
        other.destroyed = nullptr;
    }

    ~DestructionProbe() noexcept {
        if (destroyed != nullptr) {
            ++(*destroyed);
        }
    }

    int* destroyed;
};

arpg::test::Failure empty_queue_fails_cleanly() noexcept {
    arpg::core::BoundedQueue<int, 3> queue;
    ARPG_REQUIRE(queue.empty());
    ARPG_REQUIRE(!queue.full());
    ARPG_REQUIRE(queue.front() == nullptr);
    ARPG_REQUIRE(!queue.try_pop().has_value());
    ARPG_REQUIRE(queue.capacity() == 3);
    return {};
}

arpg::test::Failure full_queue_does_not_construct() noexcept {
    int constructions = 0;
    arpg::core::BoundedQueue<ConstructedProbe, 2> queue;
    ARPG_REQUIRE(queue.try_emplace(10, &constructions));
    ARPG_REQUIRE(queue.try_emplace(20, &constructions));
    ARPG_REQUIRE(constructions == 2);
    ARPG_REQUIRE(!queue.try_emplace(30, &constructions));
    ARPG_REQUIRE(constructions == 2);
    ARPG_REQUIRE(queue.full());
    ARPG_REQUIRE(queue.front()->value == 10);
    return {};
}

arpg::test::Failure queue_is_fifo_and_wraps() noexcept {
    arpg::core::BoundedQueue<int, 3> queue;
    ARPG_REQUIRE(queue.try_push(1));
    ARPG_REQUIRE(queue.try_push(2));
    ARPG_REQUIRE(queue.try_push(3));

    ARPG_REQUIRE(queue.try_pop().value() == 1);
    ARPG_REQUIRE(queue.try_pop().value() == 2);
    ARPG_REQUIRE(queue.try_push(4));
    ARPG_REQUIRE(queue.try_push(5));

    ARPG_REQUIRE(queue.try_pop().value() == 3);
    ARPG_REQUIRE(queue.try_pop().value() == 4);
    ARPG_REQUIRE(queue.try_pop().value() == 5);
    ARPG_REQUIRE(queue.empty());
    return {};
}

arpg::test::Failure queued_values_are_destroyed() noexcept {
    int destroyed = 0;
    {
        arpg::core::BoundedQueue<DestructionProbe, 2> queue;
        ARPG_REQUIRE(queue.try_emplace(&destroyed));
        ARPG_REQUIRE(queue.try_emplace(&destroyed));
        ARPG_REQUIRE(destroyed == 0);
    }
    ARPG_REQUIRE(destroyed == 2);
    return {};
}

arpg::test::Failure pressure_loop_has_no_allocations() noexcept {
    std::uint64_t checksum = 0;
    const auto before = arpg::test::allocation_count();
    {
        arpg::core::BoundedQueue<int, 4> queue;
        for (int value = 0; value < 10000; ++value) {
            ARPG_REQUIRE(queue.try_push(value));
            const auto popped = queue.try_pop();
            ARPG_REQUIRE(popped.has_value());
            checksum += static_cast<std::uint64_t>(*popped);
        }
    }
    const auto after = arpg::test::allocation_count();

    ARPG_REQUIRE(checksum == 49995000ULL);
    ARPG_REQUIRE(after == before);
    return {};
}

static_assert(
    noexcept(
        std::declval<arpg::core::BoundedQueue<int, 2>&>()
            .try_push(1)));

constexpr arpg::test::TestCase kCases[] = {
    {"empty queue", &empty_queue_fails_cleanly},
    {"full queue", &full_queue_does_not_construct},
    {"fifo wrap", &queue_is_fifo_and_wraps},
    {"destruction", &queued_values_are_destroyed},
    {"no allocations", &pressure_loop_has_no_allocations},
};

}  // namespace

arpg::test::TestSuite bounded_queue_suite() noexcept {
    return arpg::test::make_suite("bounded_queue", kCases);
}
