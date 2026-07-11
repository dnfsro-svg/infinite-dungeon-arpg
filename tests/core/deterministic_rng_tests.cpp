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
    return arpg::test::make_suite("deterministic_rng", kCases);
}
