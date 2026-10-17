### Task 3: Deterministic Random Streams

**Worktree:** E:\game\.worktrees\m00-core-runtime

**Files:**
- Create: src/core/deterministic_rng.hpp
- Create: src/core/deterministic_rng.cpp
- Create: tests/core/deterministic_rng_tests.cpp
- Modify: src/core/CMakeLists.txt
- Modify: tests/core/CMakeLists.txt
- Modify: tests/core/test_main.cpp

**Interfaces:**
- Produces: arpg::core::DeterministicRng::DeterministicRng(std::uint64_t) noexcept.
- Produces: std::uint64_t DeterministicRng::next_u64() noexcept.
- Produces: static DeterministicRng DeterministicRng::derive_stream(std::uint64_t root_seed, std::uint64_t stream_id) noexcept.
- Uses no standard-library random engine or distribution.

- [ ] **Step 1: Add the RNG interface and golden tests in a red state**

Create src/core/deterministic_rng.hpp:

~~~cpp
#pragma once

#include <array>
#include <cstdint>

namespace arpg::core {

class DeterministicRng final {
public:
    explicit DeterministicRng(std::uint64_t seed) noexcept;

    [[nodiscard]] std::uint64_t next_u64() noexcept;

    [[nodiscard]] static DeterministicRng derive_stream(
        std::uint64_t root_seed,
        std::uint64_t stream_id) noexcept;

private:
    std::array<std::uint64_t, 4> state_{};
};

}  // namespace arpg::core
~~~

Create src/core/deterministic_rng.cpp in the red state:

~~~cpp
#include "core/deterministic_rng.hpp"
~~~

Create tests/core/deterministic_rng_tests.cpp:

~~~cpp
#include "test_framework.hpp"

#include "core/deterministic_rng.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace {

using arpg::core::DeterministicRng;

template <std::size_t Count>
arpg::test::Failure require_sequence(
    DeterministicRng& rng,
    const std::array<std::uint64_t, Count>& expected) noexcept {
    for (const auto value : expected) {
        ARPG_REQUIRE(rng.next_u64() == value);
    }
    return {};
}

arpg::test::Failure seed_zero_golden_sequence() noexcept {
    DeterministicRng rng{0};
    constexpr std::array<std::uint64_t, 6> expected = {
        0x99EC5F36CB75F2B4ULL,
        0xBF6E1F784956452AULL,
        0x1A5F849D4933E6E0ULL,
        0x6AA594F1262D2D2CULL,
        0xBBA5AD4A1F842E59ULL,
        0xFFEF8375D9EBCACAULL,
    };
    return require_sequence(rng, expected);
}

arpg::test::Failure seed_one_golden_sequence() noexcept {
    DeterministicRng rng{1};
    constexpr std::array<std::uint64_t, 4> expected = {
        0xB3F2AF6D0FC710C5ULL,
        0x853B559647364CEAULL,
        0x92F89756082A4514ULL,
        0x642E1C7BC266A3A7ULL,
    };
    return require_sequence(rng, expected);
}

arpg::test::Failure identical_seeds_remain_identical() noexcept {
    DeterministicRng lhs{0xA55AA55A12345678ULL};
    DeterministicRng rhs{0xA55AA55A12345678ULL};
    for (int index = 0; index < 1000; ++index) {
        ARPG_REQUIRE(lhs.next_u64() == rhs.next_u64());
    }
    return {};
}

arpg::test::Failure derived_stream_golden_sequences() noexcept {
    constexpr std::uint64_t root = 0x0123456789ABCDEFULL;

    auto stream_zero = DeterministicRng::derive_stream(root, 0);
    constexpr std::array<std::uint64_t, 4> expected_zero = {
        0xDBA706AE738CF8D8ULL,
        0x11379A5E6A1305B6ULL,
        0xEF5DD13D35E639B8ULL,
        0x47A7F4426A216B33ULL,
    };
    auto failure = require_sequence(stream_zero, expected_zero);
    ARPG_REQUIRE(failure.expression == nullptr);

    auto stream_one = DeterministicRng::derive_stream(root, 1);
    constexpr std::array<std::uint64_t, 4> expected_one = {
        0x4D5D904D32A74EEEULL,
        0xD6AD6D7AF502CF07ULL,
        0x4565597487C3FB08ULL,
        0x97756A448E7CD5D9ULL,
    };
    failure = require_sequence(stream_one, expected_one);
    ARPG_REQUIRE(failure.expression == nullptr);

    auto stream_max = DeterministicRng::derive_stream(
        root,
        std::numeric_limits<std::uint64_t>::max());
    constexpr std::array<std::uint64_t, 4> expected_max = {
        0x95E14F3C01B793E8ULL,
        0x13CA973572CEBA9FULL,
        0x59AA56671F0B3124ULL,
        0xC7A20ADE941E50BFULL,
    };
    failure = require_sequence(stream_max, expected_max);
    ARPG_REQUIRE(failure.expression == nullptr);
    return {};
}

arpg::test::Failure streams_are_isolated() noexcept {
    constexpr std::uint64_t root = 0x0123456789ABCDEFULL;
    auto advanced_zero = DeterministicRng::derive_stream(root, 0);
    for (int index = 0; index < 100; ++index) {
        static_cast<void>(advanced_zero.next_u64());
    }

    auto stream_one_after = DeterministicRng::derive_stream(root, 1);
    auto stream_one_fresh = DeterministicRng::derive_stream(root, 1);
    for (int index = 0; index < 64; ++index) {
        ARPG_REQUIRE(
            stream_one_after.next_u64() ==
            stream_one_fresh.next_u64());
    }
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"seed zero golden", &seed_zero_golden_sequence},
    {"seed one golden", &seed_one_golden_sequence},
    {"identical seeds", &identical_seeds_remain_identical},
    {"derived streams golden", &derived_stream_golden_sequences},
    {"stream isolation", &streams_are_isolated},
};

}  // namespace

arpg::test::TestSuite deterministic_rng_suite() noexcept {
    return {
        "deterministic_rng",
        kCases,
        sizeof(kCases) / sizeof(kCases[0]),
    };
}
~~~

Add deterministic_rng.cpp to src/core/CMakeLists.txt:

~~~cmake
add_library(arpg_core STATIC
    fixed_step.cpp
    deterministic_rng.cpp)
~~~

Replace tests/core/CMakeLists.txt with:

~~~cmake
add_executable(arpg_core_tests
    test_main.cpp
    fixed_step_tests.cpp
    deterministic_rng_tests.cpp)

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

int main() {
    const arpg::test::TestSuite suites[] = {
        fixed_step_suite(),
        deterministic_rng_suite(),
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

- [ ] **Step 2: Run the RNG tests and verify the red state**

Run from E:\game\.worktrees\m00-core-runtime:

~~~powershell
.\scripts\Test.ps1 -Preset windows-msvc-core-debug -Fresh
~~~

Expected: linking fails because the DeterministicRng constructor, next_u64, and derive_stream are declared but not defined.

- [ ] **Step 3: Implement SplitMix64 initialization and xoshiro256\*\***

Replace src/core/deterministic_rng.cpp with:

~~~cpp
#include "core/deterministic_rng.hpp"

#include <cstdint>

namespace arpg::core {
namespace {

[[nodiscard]] std::uint64_t rotate_left(
    std::uint64_t value,
    unsigned int shift) noexcept {
    return (value << shift) | (value >> (64U - shift));
}

[[nodiscard]] std::uint64_t splitmix64_next(
    std::uint64_t& state) noexcept {
    state += 0x9E3779B97F4A7C15ULL;
    std::uint64_t value = state;
    value =
        (value ^ (value >> 30U)) * 0xBF58476D1CE4E5B9ULL;
    value =
        (value ^ (value >> 27U)) * 0x94D049BB133111EBULL;
    return value ^ (value >> 31U);
}

}  // namespace

DeterministicRng::DeterministicRng(std::uint64_t seed) noexcept {
    std::uint64_t splitmix_state = seed;
    for (auto& value : state_) {
        value = splitmix64_next(splitmix_state);
    }

    if ((state_[0] | state_[1] | state_[2] | state_[3]) == 0) {
        state_[0] = 0x9E3779B97F4A7C15ULL;
    }
}

std::uint64_t DeterministicRng::next_u64() noexcept {
    const std::uint64_t result =
        rotate_left(state_[1] * 5ULL, 7U) * 9ULL;
    const std::uint64_t shifted = state_[1] << 17U;

    state_[2] ^= state_[0];
    state_[3] ^= state_[1];
    state_[1] ^= state_[2];
    state_[0] ^= state_[3];
    state_[2] ^= shifted;
    state_[3] = rotate_left(state_[3], 45U);
    return result;
}

DeterministicRng DeterministicRng::derive_stream(
    std::uint64_t root_seed,
    std::uint64_t stream_id) noexcept {
    std::uint64_t id_state = stream_id;
    const std::uint64_t mixed_id = splitmix64_next(id_state);
    std::uint64_t outer_state = root_seed ^ mixed_id;
    const std::uint64_t child_seed =
        splitmix64_next(outer_state);
    return DeterministicRng{child_seed};
}

}  // namespace arpg::core
~~~

- [ ] **Step 4: Run all core tests**

Run:

~~~powershell
.\scripts\Test.ps1 -Preset windows-msvc-core-debug
~~~

Expected: twelve cases pass across fixed_step and deterministic_rng; architecture.core_no_raylib also passes.

- [ ] **Step 5: Commit deterministic streams**

Run:

~~~powershell
git -c safe.directory=E:/game/.worktrees/m00-core-runtime diff --check
git -c safe.directory=E:/game/.worktrees/m00-core-runtime add src/core tests/core
git -c safe.directory=E:/game/.worktrees/m00-core-runtime -c user.name=Codex -c user.email=codex@local commit -m "feat: add deterministic random streams"
~~~

Expected: task/m00-core-runtime is clean after one RNG commit.

---

