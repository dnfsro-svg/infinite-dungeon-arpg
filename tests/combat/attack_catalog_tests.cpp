#include "test_framework.hpp"

#include "combat/attack_catalog.hpp"

namespace {

using namespace arpg::combat;

static_assert(kAttackCount == 6);

arpg::test::Failure catalog_has_six_attacks() noexcept {
    ARPG_REQUIRE(find_attack_definition(AttackId::j1) != nullptr);
    ARPG_REQUIRE(find_attack_definition(AttackId::j2) != nullptr);
    ARPG_REQUIRE(find_attack_definition(AttackId::j3) != nullptr);
    ARPG_REQUIRE(find_attack_definition(AttackId::heavy) != nullptr);
    ARPG_REQUIRE(find_attack_definition(AttackId::launcher) != nullptr);
    ARPG_REQUIRE(find_attack_definition(AttackId::air_j) != nullptr);
    ARPG_REQUIRE(find_attack_definition(AttackId::none) == nullptr);
    return {};
}

arpg::test::Failure j1_definition_is_stable() noexcept {
    const AttackDefinition& j1 = *find_attack_definition(AttackId::j1);
    ARPG_REQUIRE(j1.startup_ticks == 5);
    ARPG_REQUIRE(j1.active_ticks == 3);
    ARPG_REQUIRE(j1.recovery_ticks == 9);
    ARPG_REQUIRE(j1.damage == 28);
    ARPG_REQUIRE(j1.break_damage == 10);
    return {};
}

arpg::test::Failure phases_are_half_open_and_catalog_is_valid() noexcept {
    const AttackDefinition& j1 = *find_attack_definition(AttackId::j1);
    ARPG_REQUIRE(attack_phase_at(j1, 4) == AttackPhase::startup);
    ARPG_REQUIRE(attack_phase_at(j1, 5) == AttackPhase::active);
    ARPG_REQUIRE(attack_phase_at(j1, 8) == AttackPhase::recovery);
    ARPG_REQUIRE(attack_phase_at(j1, 17) == AttackPhase::finished);
    ARPG_REQUIRE(validate_attack_catalog());
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"six attacks", &catalog_has_six_attacks},
    {"stable j1 definition", &j1_definition_is_stable},
    {"half-open phases and validation", &phases_are_half_open_and_catalog_is_valid},
};

}  // namespace

arpg::test::TestSuite attack_catalog_suite() noexcept {
    return {
        "attack_catalog",
        kCases,
        sizeof(kCases) / sizeof(kCases[0]),
    };
}
