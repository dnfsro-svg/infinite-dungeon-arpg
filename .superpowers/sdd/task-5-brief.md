### Task 5: Fixed-Capacity Bounded Queue

**Worktree:** E:\game\.worktrees\m00-core-runtime

**Files:**
- Create: src/core/bounded_queue.hpp
- Create: tests/core/bounded_queue_tests.cpp
- Modify: tests/core/CMakeLists.txt
- Modify: tests/core/test_main.cpp

**Interfaces:**
- Produces: arpg::core::BoundedQueue<T, N>, try_emplace, try_push, try_pop, front, size, empty, full, and capacity.
- Queue storage is std::array<std::optional<T>, N>; normal empty/full results use bool, nullptr, or std::nullopt.
- No overwrite, blocking, dynamic growth, or mutable front pointer is exposed.

- [ ] **Step 1: Add failing queue behavior and pressure tests**

Create tests/core/bounded_queue_tests.cpp:

~~~cpp
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
    return {
        "bounded_queue",
        kCases,
        sizeof(kCases) / sizeof(kCases[0]),
    };
}
~~~

Replace tests/core/CMakeLists.txt with:

~~~cmake
add_executable(arpg_core_tests
    test_main.cpp
    allocation_probe.cpp
    fixed_step_tests.cpp
    deterministic_rng_tests.cpp
    fixed_pool_tests.cpp
    bounded_queue_tests.cpp)

target_compile_features(arpg_core_tests PRIVATE cxx_std_17)
target_link_libraries(arpg_core_tests PRIVATE arpg_core)
arpg_enable_project_warnings(arpg_core_tests)

add_test(NAME core.units COMMAND arpg_core_tests)
set_tests_properties(core.units PROPERTIES LABELS "headless;core")
~~~

Replace tests/core/test_main.cpp with:

~~~cpp
#include "test_framework.hpp"

#include <cstdio>

arpg::test::TestSuite fixed_step_suite() noexcept;
arpg::test::TestSuite deterministic_rng_suite() noexcept;
arpg::test::TestSuite fixed_pool_suite() noexcept;
arpg::test::TestSuite bounded_queue_suite() noexcept;

int main() {
    const arpg::test::TestSuite suites[] = {
        fixed_step_suite(),
        deterministic_rng_suite(),
        fixed_pool_suite(),
        bounded_queue_suite(),
    };

    int failures = 0;
    int checks = 0;
    for (const auto& suite : suites) {
        for (std::size_t index = 0; index < suite.count; ++index) {
            ++checks;
            const auto failure = suite.cases[index].function();
            if (failure.expression != nullptr) {
                ++failures;
                std::fprintf(
                    stderr,
                    "[FAIL] %s.%s: %s (%s:%d)\n",
                    suite.name,
                    suite.cases[index].name,
                    failure.expression,
                    failure.file,
                    failure.line);
            }
        }
    }

    std::printf("%d cases, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
~~~

- [ ] **Step 2: Build and verify the missing queue red state**

Run:

~~~powershell
.\scripts\Test.ps1 -Preset windows-msvc-core-debug
~~~

Expected: compilation fails because core/bounded_queue.hpp does not exist.

- [ ] **Step 3: Implement the bounded ring queue**

Create src/core/bounded_queue.hpp:

~~~cpp
#pragma once

#include <array>
#include <cstddef>
#include <optional>
#include <type_traits>
#include <utility>

namespace arpg::core {

template <typename T, std::size_t N>
class BoundedQueue final {
public:
    static_assert(N > 0, "BoundedQueue capacity must be positive");
    static_assert(
        std::is_nothrow_move_constructible_v<T>,
        "BoundedQueue values must be nothrow move constructible");
    static_assert(
        std::is_nothrow_destructible_v<T>,
        "BoundedQueue values must be nothrow destructible");

    BoundedQueue() noexcept = default;
    ~BoundedQueue() noexcept = default;

    BoundedQueue(const BoundedQueue&) = delete;
    BoundedQueue& operator=(const BoundedQueue&) = delete;
    BoundedQueue(BoundedQueue&&) = delete;
    BoundedQueue& operator=(BoundedQueue&&) = delete;

    template <
        typename... Args,
        std::enable_if_t<
            std::is_nothrow_constructible_v<T, Args...>,
            int> = 0>
    [[nodiscard]] bool try_emplace(Args&&... args) noexcept {
        if (full()) {
            return false;
        }

        slots_[tail_].emplace(std::forward<Args>(args)...);
        tail_ = (tail_ + 1) % N;
        ++size_;
        return true;
    }

    template <
        typename Value = T,
        std::enable_if_t<
            std::is_nothrow_copy_constructible_v<Value>,
            int> = 0>
    [[nodiscard]] bool try_push(const T& value) noexcept {
        return try_emplace(value);
    }

    [[nodiscard]] bool try_push(T&& value) noexcept {
        return try_emplace(std::move(value));
    }

    [[nodiscard]] std::optional<T> try_pop() noexcept {
        if (empty()) {
            return std::nullopt;
        }

        std::optional<T> result{
            std::move(*slots_[head_])};
        slots_[head_].reset();
        head_ = (head_ + 1) % N;
        --size_;
        return result;
    }

    [[nodiscard]] const T* front() const noexcept {
        return empty() ? nullptr : &(*slots_[head_]);
    }

    [[nodiscard]] std::size_t size() const noexcept {
        return size_;
    }

    [[nodiscard]] bool empty() const noexcept {
        return size_ == 0;
    }

    [[nodiscard]] bool full() const noexcept {
        return size_ == N;
    }

    [[nodiscard]] static constexpr std::size_t capacity() noexcept {
        return N;
    }

private:
    std::array<std::optional<T>, N> slots_{};
    std::size_t head_{0};
    std::size_t tail_{0};
    std::size_t size_{0};
};

}  // namespace arpg::core
~~~

- [ ] **Step 4: Run the complete core suite**

Run:

~~~powershell
.\scripts\Test.ps1 -Preset windows-msvc-core-debug
~~~

Expected: twenty-two cases pass across all four core suites; architecture.core_no_raylib passes.

- [ ] **Step 5: Inspect allocation guarantees and commit**

Run:

~~~powershell
rg -n "malloc|calloc|realloc|new " src/core/fixed_pool.hpp src/core/bounded_queue.hpp
git -c safe.directory=E:/game/.worktrees/m00-core-runtime diff --check
git -c safe.directory=E:/game/.worktrees/m00-core-runtime add src/core/bounded_queue.hpp tests/core
git -c safe.directory=E:/game/.worktrees/m00-core-runtime -c user.name=Codex -c user.email=codex@local commit -m "feat: add bounded event queue"
~~~

Expected: rg finds no direct allocation calls; all changes are committed and task/m00-core-runtime is clean.

---

