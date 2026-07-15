#include "test_framework.hpp"

#include "combat/monster_affix_generation.hpp"
#include "combat/monster_catalog.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace {

using namespace arpg::combat;

bool has_affix(const MonsterAffixSet& set, MonsterAffixId id) noexcept {
    for (std::size_t index = 0U; index < set.count; ++index) {
        if (set.values[index].id == id) return true;
    }
    return false;
}

bool affixes_are_unique(const MonsterAffixSet& set) noexcept {
    if (set.count > set.values.size()) return false;
    for (std::size_t left = 0U; left < set.count; ++left) {
        for (std::size_t right = left + 1U; right < set.count; ++right) {
            if (set.values[left].id == set.values[right].id) return false;
        }
    }
    return true;
}

arpg::test::Failure depth_bands_match_frozen_weights() noexcept {
    ARPG_REQUIRE((affix_count_weights(3U)
        == std::array<std::uint16_t, 4>{{80U, 20U, 0U, 0U}}));
    ARPG_REQUIRE((affix_count_weights(4U)
        == std::array<std::uint16_t, 4>{{55U, 38U, 7U, 0U}}));
    ARPG_REQUIRE((affix_count_weights(9U)
        == std::array<std::uint16_t, 4>{{55U, 38U, 7U, 0U}}));
    ARPG_REQUIRE((affix_count_weights(10U)
        == std::array<std::uint16_t, 4>{{30U, 45U, 20U, 5U}}));
    ARPG_REQUIRE((affix_count_weights(19U)
        == std::array<std::uint16_t, 4>{{30U, 45U, 20U, 5U}}));
    ARPG_REQUIRE((affix_count_weights(20U)
        == std::array<std::uint16_t, 4>{{15U, 35U, 35U, 15U}}));
    ARPG_REQUIRE((affix_count_weights(39U)
        == std::array<std::uint16_t, 4>{{15U, 35U, 35U, 15U}}));
    ARPG_REQUIRE((affix_count_weights(40U)
        == std::array<std::uint16_t, 4>{{5U, 20U, 40U, 35U}}));
    ARPG_REQUIRE((affix_tier_weights(3U)
        == std::array<std::uint16_t, 3>{{100U, 0U, 0U}}));
    ARPG_REQUIRE((affix_tier_weights(4U)
        == std::array<std::uint16_t, 3>{{80U, 20U, 0U}}));
    ARPG_REQUIRE((affix_tier_weights(10U)
        == std::array<std::uint16_t, 3>{{50U, 40U, 10U}}));
    ARPG_REQUIRE((affix_tier_weights(20U)
        == std::array<std::uint16_t, 3>{{25U, 50U, 25U}}));
    ARPG_REQUIRE((affix_tier_weights(40U)
        == std::array<std::uint16_t, 3>{{10U, 35U, 55U}}));
    return {};
}

arpg::test::Failure generation_is_deterministic_unique_and_scored() noexcept {
    const MonsterDefinition* const monster =
        monster_definition(MonsterId::lightning_shooter);
    ARPG_REQUIRE(monster != nullptr);
    const auto a = generate_monster_affixes(0xA11CEULL, 40U, 1U, 7U,
        *monster);
    const auto b = generate_monster_affixes(0xA11CEULL, 40U, 1U, 7U,
        *monster);
    ARPG_REQUIRE(a.has_value());
    ARPG_REQUIRE(b.has_value());
    ARPG_REQUIRE(*a == *b);
    ARPG_REQUIRE(affixes_are_unique(*a));
    ARPG_REQUIRE(monster_affix_danger_score(*a) <= 27U);
    return {};
}

arpg::test::Failure applicability_rules_and_three_high_risk_are_preserved() noexcept {
    const MonsterDefinition* const support =
        monster_definition(MonsterId::water_support);
    const MonsterDefinition* const hazard =
        monster_definition(MonsterId::chaos_hazard);
    const MonsterDefinition* const shooter =
        monster_definition(MonsterId::lightning_shooter);
    ARPG_REQUIRE(support != nullptr);
    ARPG_REQUIRE(hazard != nullptr);
    ARPG_REQUIRE(shooter != nullptr);

    bool found_three_high = false;
    for (std::uint64_t seed = 0U; seed < 4096U; ++seed) {
        const auto support_set = generate_monster_affixes(seed, 40U, 0U, 0U,
            *support);
        const auto hazard_set = generate_monster_affixes(seed, 40U, 0U, 1U,
            *hazard);
        const auto shooter_set = generate_monster_affixes(seed, 40U, 0U, 2U,
            *shooter);
        ARPG_REQUIRE(support_set.has_value());
        ARPG_REQUIRE(hazard_set.has_value());
        ARPG_REQUIRE(shooter_set.has_value());
        ARPG_REQUIRE(affixes_are_unique(*support_set));
        ARPG_REQUIRE(affixes_are_unique(*hazard_set));
        ARPG_REQUIRE(affixes_are_unique(*shooter_set));
        ARPG_REQUIRE(!has_affix(*support_set, MonsterAffixId::chilling));
        ARPG_REQUIRE(!has_affix(*support_set, MonsterAffixId::chain_lightning));
        ARPG_REQUIRE(!has_affix(*support_set, MonsterAffixId::chaos_corrosion));
        ARPG_REQUIRE(!has_affix(*hazard_set, MonsterAffixId::multishot));
        if (shooter_set->count == 3U
                && monster_affix_danger_score(*shooter_set) == 27U) {
            found_three_high = true;
            break;
        }
    }
    ARPG_REQUIRE(found_three_high);
    return {};
}

arpg::test::Failure invalid_monster_definition_fails_explicitly() noexcept {
    MonsterDefinition invalid{};
    invalid.id = MonsterId::count;
    invalid.tags = static_cast<std::uint16_t>(MonsterTag::support);
    ARPG_REQUIRE(!generate_monster_affixes(1U, 40U, 0U, 0U, invalid).has_value());
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"frozen depth bands", &depth_bands_match_frozen_weights},
    {"deterministic unique generation and score",
        &generation_is_deterministic_unique_and_scored},
    {"applicability and three high risk", &applicability_rules_and_three_high_risk_are_preserved},
    {"invalid monster fails explicitly", &invalid_monster_definition_fails_explicitly},
};

}  // namespace

arpg::test::TestSuite monster_affix_generation_suite() noexcept {
    return arpg::test::make_suite("monster_affix_generation", kCases);
}
