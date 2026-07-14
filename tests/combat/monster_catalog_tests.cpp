#include "test_framework.hpp"

#include "combat/monster_catalog.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace {

using namespace arpg::combat;

arpg::test::Failure catalog_contains_eight_unique_stable_ids() noexcept {
    std::array<bool, static_cast<std::size_t>(MonsterId::count)> seen{};
    for (std::uint8_t raw = 0;
         raw < static_cast<std::uint8_t>(MonsterId::count); ++raw) {
        const auto id = static_cast<MonsterId>(raw);
        const auto* definition = monster_definition(id);
        ARPG_REQUIRE(definition != nullptr);
        ARPG_REQUIRE(definition->id == id);
        ARPG_REQUIRE(!seen[raw]);
        seen[raw] = true;
    }
    ARPG_REQUIRE(monster_catalog_valid());
    return {};
}

arpg::test::Failure catalog_has_two_preferred_roles_per_ecology() noexcept {
    std::array<std::uint8_t, 4> counts{};
    for (std::uint8_t raw = 0;
         raw < static_cast<std::uint8_t>(MonsterId::count); ++raw) {
        const auto* definition =
            monster_definition(static_cast<MonsterId>(raw));
        ARPG_REQUIRE(definition->preferred_ecology < counts.size());
        ++counts[definition->preferred_ecology];
        ARPG_REQUIRE(definition->threat_cost > 0U);
        ARPG_REQUIRE(definition->max_hp > 0);
    }
    ARPG_REQUIRE((counts == std::array<std::uint8_t, 4>{{2, 2, 2, 2}}));
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"eight unique stable ids", &catalog_contains_eight_unique_stable_ids},
    {"two preferred roles per ecology",
     &catalog_has_two_preferred_roles_per_ecology},
};

}  // namespace

arpg::test::TestSuite monster_catalog_suite() noexcept {
    return arpg::test::make_suite("monster_catalog", kCases);
}
