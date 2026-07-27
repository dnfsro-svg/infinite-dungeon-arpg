#include "test_framework.hpp"

#include "allocation_probe.hpp"
#include "core/gameplay_limits.hpp"
#include "dungeon/room_affix.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace {

arpg::test::Failure catalog_has_scaled_weights_and_ranges() noexcept {
    using namespace arpg::dungeon;
    const auto* crowded = room_density_definition(RoomDensityAffix::crowded);
    const auto* dense = room_density_definition(RoomDensityAffix::dense);
    const auto* horde = room_density_definition(RoomDensityAffix::horde);
    ARPG_REQUIRE(crowded != nullptr);
    ARPG_REQUIRE(dense != nullptr);
    ARPG_REQUIRE(horde != nullptr);
    ARPG_REQUIRE(crowded->weight == 50U);
    ARPG_REQUIRE(crowded->minimum == 300U);
    ARPG_REQUIRE(crowded->maximum == 400U);
    ARPG_REQUIRE(dense->weight == 35U);
    ARPG_REQUIRE(dense->minimum == 425U);
    ARPG_REQUIRE(dense->maximum == 550U);
    ARPG_REQUIRE(horde->weight == 15U);
    ARPG_REQUIRE(horde->minimum == 575U);
    ARPG_REQUIRE(horde->maximum == 750U);
    ARPG_REQUIRE(room_density_definition(RoomDensityAffix::count) == nullptr);
    return {};
}

arpg::test::Failure abyss_preserves_base_roll_and_scales_count() noexcept {
    using namespace arpg::dungeon;
    for (std::uint64_t seed = 0U; seed < 100000U; ++seed) {
        const RoomDensityRoll normal = roll_room_density(seed, false);
        const RoomDensityRoll abyss = roll_room_density(seed, true);
        ARPG_REQUIRE(abyss.affix == normal.affix);
        ARPG_REQUIRE(abyss.base_count == normal.base_count);
        ARPG_REQUIRE(normal.total_count == normal.base_count);
        ARPG_REQUIRE(abyss.total_count
            == static_cast<std::uint16_t>(
                (static_cast<std::uint32_t>(normal.base_count) * 3U + 1U)
                / 2U));
        ARPG_REQUIRE(abyss.total_count <= 1125U);
        ARPG_REQUIRE(abyss.total_count <= arpg::limits::kRoomMonsterCapacity);
    }
    return {};
}

arpg::test::Failure weighted_distribution_covers_inclusive_endpoints() noexcept {
    using namespace arpg::dungeon;
    std::array<std::uint32_t, 3U> affix_counts{};
    std::array<bool, 751U> observed_counts{};
    for (std::uint64_t seed = 0U; seed < 100000U; ++seed) {
        const RoomDensityRoll roll = roll_room_density(seed, false);
        const auto index = static_cast<std::size_t>(roll.affix);
        const auto* definition = room_density_definition(roll.affix);
        ARPG_REQUIRE(index < affix_counts.size());
        ARPG_REQUIRE(definition != nullptr);
        ARPG_REQUIRE(roll.base_count >= definition->minimum);
        ARPG_REQUIRE(roll.base_count <= definition->maximum);
        ARPG_REQUIRE(roll.base_count % 25U == 0U);
        ARPG_REQUIRE(roll.total_count == roll.base_count);
        ++affix_counts[index];
        observed_counts[roll.base_count] = true;
    }

    constexpr std::array<std::uint32_t, 3U> expected{{
        50000U, 35000U, 15000U}};
    for (std::size_t index = 0U; index < affix_counts.size(); ++index) {
        const std::uint32_t deviation = affix_counts[index] > expected[index]
            ? affix_counts[index] - expected[index]
            : expected[index] - affix_counts[index];
        ARPG_REQUIRE(deviation < 1000U);
    }

    constexpr std::array<std::uint16_t, 6U> endpoints{{
        300U, 400U, 425U, 550U, 575U, 750U}};
    for (const std::uint16_t endpoint : endpoints) {
        ARPG_REQUIRE(observed_counts[endpoint]);
    }
    for (std::uint16_t count = 300U; count <= 400U; count += 25U) {
        ARPG_REQUIRE(observed_counts[count]);
    }
    for (std::uint16_t count = 425U; count <= 550U; count += 25U) {
        ARPG_REQUIRE(observed_counts[count]);
    }
    for (std::uint16_t count = 575U; count <= 750U; count += 25U) {
        ARPG_REQUIRE(observed_counts[count]);
    }
    return {};
}

arpg::test::Failure rolls_are_repeatable_and_allocate_nothing() noexcept {
    using namespace arpg::dungeon;
    for (std::uint64_t seed = 0U; seed < 100000U; ++seed) {
        const bool abyss = (seed & 1U) != 0U;
        const RoomDensityRoll first = roll_room_density(seed, abyss);
        const RoomDensityRoll second = roll_room_density(seed, abyss);
        ARPG_REQUIRE(first.affix == second.affix);
        ARPG_REQUIRE(first.base_count == second.base_count);
        ARPG_REQUIRE(first.total_count == second.total_count);
    }

    const std::uint64_t before = arpg::test::allocation_count();
    for (std::uint64_t seed = 0U; seed < 100000U; ++seed) {
        static_cast<void>(roll_room_density(seed, (seed & 1U) != 0U));
    }
    ARPG_REQUIRE(arpg::test::allocation_count() == before);
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"catalog has scaled weights and ranges",
     &catalog_has_scaled_weights_and_ranges},
    {"abyss preserves base roll and scales count",
     &abyss_preserves_base_roll_and_scales_count},
    {"weighted distribution covers inclusive endpoints",
     &weighted_distribution_covers_inclusive_endpoints},
    {"rolls are repeatable and allocate nothing",
     &rolls_are_repeatable_and_allocate_nothing},
};

}  // namespace

arpg::test::TestSuite room_affix_suite() noexcept {
    return arpg::test::make_suite("room_affix", kCases);
}
