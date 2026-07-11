#include "allocation_probe.hpp"
#include "test_framework.hpp"

#include "core/fixed_pool.hpp"

#include <cstdint>
#include <type_traits>
#include <utility>

namespace {

struct Item final {
    explicit Item(int initial) noexcept : value(initial) {}
    int value;
};

struct Tracked final {
    explicit Tracked(int* destroyed_count) noexcept
        : destroyed(destroyed_count) {}

    ~Tracked() noexcept {
        ++(*destroyed);
    }

    int* destroyed;
};

struct alignas(64) AlignedItem final {
    explicit AlignedItem(int initial) noexcept : values{initial} {}
    int values[16];
};

using ItemPool = arpg::core::FixedPool<Item, 2>;

static_assert(!std::is_default_constructible_v<Item>);
static_assert(sizeof(ItemPool::index_type) == sizeof(std::uint32_t));
static_assert(sizeof(ItemPool::generation_type) == sizeof(std::uint32_t));
static_assert(sizeof(ItemPool::Handle) == 2 * sizeof(std::uint32_t));

arpg::test::Failure initial_state_and_capacity() noexcept {
    ItemPool pool;
    ARPG_REQUIRE(pool.empty());
    ARPG_REQUIRE(!pool.full());
    ARPG_REQUIRE(pool.size() == 0);
    ARPG_REQUIRE(pool.capacity() == 2);
    return {};
}

arpg::test::Failure full_pool_preserves_objects() noexcept {
    ItemPool pool;
    const auto first = pool.try_emplace(10);
    const auto second = pool.try_emplace(20);
    ARPG_REQUIRE(first.has_value());
    ARPG_REQUIRE(second.has_value());

    Item* const first_address = pool.get(*first);
    Item* const second_address = pool.get(*second);
    const auto rejected = pool.try_emplace(30);

    ARPG_REQUIRE(!rejected.has_value());
    ARPG_REQUIRE(pool.full());
    ARPG_REQUIRE(pool.size() == 2);
    ARPG_REQUIRE(pool.contains(*first));
    ARPG_REQUIRE(pool.contains(*second));
    ARPG_REQUIRE(pool.get(*first) == first_address);
    ARPG_REQUIRE(pool.get(*second) == second_address);
    ARPG_REQUIRE(first_address->value == 10);
    ARPG_REQUIRE(second_address->value == 20);

    const auto& const_pool = pool;
    ARPG_REQUIRE(const_pool.get(*first) == first_address);
    return {};
}

arpg::test::Failure stale_and_invalid_handles_fail() noexcept {
    using Pool = arpg::core::FixedPool<Item, 1>;
    Pool pool;
    const auto old_handle = pool.try_emplace(7);
    ARPG_REQUIRE(old_handle.has_value());
    Item* const old_address = pool.get(*old_handle);
    ARPG_REQUIRE(pool.release(*old_handle));
    ARPG_REQUIRE(!pool.release(*old_handle));

    const auto fresh_handle = pool.try_emplace(9);
    ARPG_REQUIRE(fresh_handle.has_value());
    ARPG_REQUIRE(fresh_handle->index == old_handle->index);
    ARPG_REQUIRE(fresh_handle->generation != old_handle->generation);
    ARPG_REQUIRE(fresh_handle->generation != 0);
    ARPG_REQUIRE(pool.get(*fresh_handle) == old_address);
    ARPG_REQUIRE(pool.get(*old_handle) == nullptr);
    ARPG_REQUIRE(!pool.contains(*old_handle));

    const Pool::Handle out_of_range{99, 1};
    const Pool::Handle zero_generation{0, 0};
    const Pool::Handle wrong_generation{
        fresh_handle->index,
        static_cast<Pool::generation_type>(fresh_handle->generation + 1)};
    ARPG_REQUIRE(pool.get(out_of_range) == nullptr);
    ARPG_REQUIRE(!pool.contains(out_of_range));
    ARPG_REQUIRE(!pool.release(out_of_range));
    ARPG_REQUIRE(pool.get(zero_generation) == nullptr);
    ARPG_REQUIRE(!pool.contains(zero_generation));
    ARPG_REQUIRE(!pool.release(zero_generation));
    ARPG_REQUIRE(pool.get(wrong_generation) == nullptr);
    ARPG_REQUIRE(!pool.contains(wrong_generation));
    ARPG_REQUIRE(!pool.release(wrong_generation));
    ARPG_REQUIRE(pool.size() == 1);
    return {};
}

arpg::test::Failure values_are_destroyed_once() noexcept {
    int destroyed = 0;
    {
        arpg::core::FixedPool<Tracked, 2> pool;
        const auto first = pool.try_emplace(&destroyed);
        const auto second = pool.try_emplace(&destroyed);
        ARPG_REQUIRE(first.has_value());
        ARPG_REQUIRE(second.has_value());
        ARPG_REQUIRE(pool.release(*first));
        ARPG_REQUIRE(destroyed == 1);
        ARPG_REQUIRE(!pool.release(*first));
        ARPG_REQUIRE(destroyed == 1);
    }
    ARPG_REQUIRE(destroyed == 2);
    return {};
}

arpg::test::Failure pressure_loop_has_no_allocations() noexcept {
    std::uint64_t checksum = 0;
    const auto before = arpg::test::allocation_count();
    {
        arpg::core::FixedPool<Item, 4> pool;
        for (int value = 0; value < 10000; ++value) {
            const auto handle = pool.try_emplace(value);
            ARPG_REQUIRE(handle.has_value());
            ARPG_REQUIRE(handle->generation != 0);
            checksum +=
                static_cast<std::uint64_t>(pool.get(*handle)->value);
            ARPG_REQUIRE(pool.release(*handle));
        }

        arpg::core::FixedPool<AlignedItem, 1> aligned_pool;
        const auto aligned_handle = aligned_pool.try_emplace(42);
        ARPG_REQUIRE(aligned_handle.has_value());
        const AlignedItem* const aligned = aligned_pool.get(*aligned_handle);
        ARPG_REQUIRE(aligned != nullptr);
        ARPG_REQUIRE(aligned->values[0] == 42);
        ARPG_REQUIRE(
            reinterpret_cast<std::uintptr_t>(aligned) % alignof(AlignedItem) ==
            0);
    }
    const auto after = arpg::test::allocation_count();

    ARPG_REQUIRE(checksum == 49995000ULL);
    ARPG_REQUIRE(after == before);
    return {};
}

static_assert(
    noexcept(
        std::declval<arpg::core::FixedPool<Item, 1>&>()
            .try_emplace(1)));

constexpr arpg::test::TestCase kCases[] = {
    {"initial state", &initial_state_and_capacity},
    {"full pool", &full_pool_preserves_objects},
    {"stale handles", &stale_and_invalid_handles_fail},
    {"destruction", &values_are_destroyed_once},
    {"no allocations", &pressure_loop_has_no_allocations},
};

}  // namespace

arpg::test::TestSuite fixed_pool_suite() noexcept {
    return {
        "fixed_pool",
        kCases,
        sizeof(kCases) / sizeof(kCases[0]),
    };
}
