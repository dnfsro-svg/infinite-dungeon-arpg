#include "test_framework.hpp"

#include "allocation_probe.hpp"
#include "dungeon/room_affix.hpp"

#include <array>
#include <cstdint>

namespace {

arpg::test::Failure catalog_has_required_weights_and_ranges() noexcept {
    using namespace arpg::dungeon;
    const auto* crowded = room_density_definition(RoomDensityAffix::crowded);
    const auto* dense = room_density_definition(RoomDensityAffix::dense);
    const auto* horde = room_density_definition(RoomDensityAffix::horde);
    ARPG_REQUIRE(crowded != nullptr);
    ARPG_REQUIRE(dense != nullptr);
    ARPG_REQUIRE(horde != nullptr);
    ARPG_REQUIRE(crowded->weight == 50U);
    ARPG_REQUIRE(crowded->minimum == 12U);
    ARPG_REQUIRE(crowded->maximum == 16U);
    ARPG_REQUIRE(dense->weight == 35U);
    ARPG_REQUIRE(dense->minimum == 17U);
    ARPG_REQUIRE(dense->maximum == 22U);
    ARPG_REQUIRE(horde->weight == 15U);
    ARPG_REQUIRE(horde->minimum == 23U);
    ARPG_REQUIRE(horde->maximum == 30U);
    return {};
}

arpg::test::Failure abyss_preserves_base_roll_and_scales_count() noexcept {
    using namespace arpg::dungeon;
    for (std::uint64_t seed = 0; seed < 10000U; ++seed) {
        const RoomDensityRoll normal = roll_room_density(seed, false);
        const RoomDensityRoll abyss = roll_room_density(seed, true);
        ARPG_REQUIRE(abyss.affix == normal.affix);
        ARPG_REQUIRE(abyss.base_count == normal.base_count);
        ARPG_REQUIRE(abyss.monster_count
            == static_cast<std::uint8_t>((normal.base_count * 3U + 1U) / 2U));
        ARPG_REQUIRE(abyss.monster_count <= 45U);
    }
    return {};
}

arpg::test::Failure weighted_distribution_covers_every_inclusive_endpoint() noexcept {
    using namespace arpg::dungeon;
    std::array<std::uint32_t, 3> affix_counts{};
    std::array<bool, 31> endpoints{};
    for (std::uint64_t seed = 0; seed < 100000U; ++seed) {
        const RoomDensityRoll roll = roll_room_density(seed, false);
        const auto index = static_cast<std::size_t>(roll.affix);
        const auto* definition = room_density_definition(roll.affix);
        ARPG_REQUIRE(index < affix_counts.size());
        ARPG_REQUIRE(definition != nullptr);
        ARPG_REQUIRE(roll.base_count >= definition->minimum);
        ARPG_REQUIRE(roll.base_count <= definition->maximum);
        ++affix_counts[index];
        endpoints[roll.base_count] = true;
    }
    constexpr std::array<std::uint32_t, 3> expected{{50000U, 35000U, 15000U}};
    for (std::size_t index = 0; index < affix_counts.size(); ++index) {
        const std::uint32_t deviation = affix_counts[index] > expected[index]
            ? affix_counts[index] - expected[index]
            : expected[index] - affix_counts[index];
        ARPG_REQUIRE(deviation < 1000U);
    }
    for (std::uint8_t count = 12U; count <= 30U; ++count) {
        ARPG_REQUIRE(endpoints[count]);
    }
    return {};
}

arpg::test::Failure rolls_are_repeatable_and_allocate_nothing() noexcept {
    using namespace arpg::dungeon;
    for (std::uint64_t seed = 0; seed < 100000U; ++seed) {
        const RoomDensityRoll first = roll_room_density(seed, (seed & 1U) != 0U);
        const RoomDensityRoll second = roll_room_density(seed, (seed & 1U) != 0U);
        ARPG_REQUIRE(first.affix == second.affix);
        ARPG_REQUIRE(first.base_count == second.base_count);
        ARPG_REQUIRE(first.monster_count == second.monster_count);
    }
    const std::uint64_t before = arpg::test::allocation_count();
    for (std::uint64_t seed = 0; seed < 100000U; ++seed) {
        static_cast<void>(roll_room_density(seed, (seed & 1U) != 0U));
    }
    ARPG_REQUIRE(arpg::test::allocation_count() == before);
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"catalog has required weights and ranges", &catalog_has_required_weights_and_ranges},
    {"abyss preserves base roll and scales count", &abyss_preserves_base_roll_and_scales_count},
    {"weighted distribution covers every inclusive endpoint", &weighted_distribution_covers_every_inclusive_endpoint},
    {"rolls are repeatable and allocate nothing", &rolls_are_repeatable_and_allocate_nothing},
};

}  // namespace

arpg::test::TestSuite room_affix_suite() noexcept {
    return arpg::test::make_suite("room_affix", kCases);
}
