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

arpg::test::Failure catalog_matches_approved_health_and_damage_table() noexcept {
    struct Expected final {
        MonsterId id;
        int max_hp;
        arpg::modifiers::DamageType damage_type;
        int damage;
    };
    constexpr std::array<Expected, 8U> kExpected{{
        {MonsterId::fire_bomber, 100, arpg::modifiers::DamageType::fire, 36},
        {MonsterId::fire_charger, 190, arpg::modifiers::DamageType::fire, 27},
        {MonsterId::water_bulwark, 315, arpg::modifiers::DamageType::water, 21},
        {MonsterId::water_support, 135, arpg::modifiers::DamageType::water, 0},
        {MonsterId::lightning_shooter, 110,
         arpg::modifiers::DamageType::lightning, 12},
        {MonsterId::lightning_dasher, 125,
         arpg::modifiers::DamageType::lightning, 18},
        {MonsterId::chaos_chaser, 120, arpg::modifiers::DamageType::chaos, 14},
        {MonsterId::chaos_hazard, 145, arpg::modifiers::DamageType::chaos, 11},
    }};
    for (const Expected expected : kExpected) {
        const MonsterDefinition* definition = monster_definition(expected.id);
        ARPG_REQUIRE(definition != nullptr);
        ARPG_REQUIRE(definition->max_hp == expected.max_hp);
        for (std::size_t index = 0U;
             index < definition->contact_damage.amount.size(); ++index) {
            const int amount = index == arpg::modifiers::damage_index(expected.damage_type)
                ? expected.damage : 0;
            ARPG_REQUIRE(definition->contact_damage.amount[index] == amount);
        }
    }
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"eight unique stable ids", &catalog_contains_eight_unique_stable_ids},
    {"two preferred roles per ecology",
     &catalog_has_two_preferred_roles_per_ecology},
    {"catalog matches approved health and damage table",
     &catalog_matches_approved_health_and_damage_table},
};

}  // namespace

arpg::test::TestSuite monster_catalog_suite() noexcept {
    return arpg::test::make_suite("monster_catalog", kCases);
}
