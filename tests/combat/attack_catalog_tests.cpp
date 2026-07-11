#include "test_framework.hpp"

#include "combat/attack_catalog.hpp"
#include "combat/input_buffer.hpp"

#include <type_traits>

namespace {

using namespace arpg::combat;

template <typename T, typename = void>
struct has_heavy_member : std::false_type {};

template <typename T>
struct has_heavy_member<T, std::void_t<decltype(T::heavy)>>
    : std::true_type {};

static_assert(!has_heavy_member<Action>::value);
static_assert(!has_heavy_member<AttackId>::value);
static_assert(kAttackCount == 5);

arpg::test::Failure catalog_has_five_attacks() noexcept {
    ARPG_REQUIRE(find_attack_definition(AttackId::j1) != nullptr);
    ARPG_REQUIRE(find_attack_definition(AttackId::j2) != nullptr);
    ARPG_REQUIRE(find_attack_definition(AttackId::j3) != nullptr);
    ARPG_REQUIRE(find_attack_definition(AttackId::launcher) != nullptr);
    ARPG_REQUIRE(find_attack_definition(AttackId::air_j) != nullptr);
    ARPG_REQUIRE(find_attack_definition(AttackId::none) == nullptr);
    return {};
}

arpg::test::Failure launcher_definition_is_stable() noexcept {
    const AttackDefinition& launcher =
        *find_attack_definition(AttackId::launcher);
    ARPG_REQUIRE(launcher.startup_ticks == 7);
    ARPG_REQUIRE(launcher.active_ticks == 4);
    ARPG_REQUIRE(launcher.recovery_ticks == 17);
    ARPG_REQUIRE(launcher.damage == 38);
    ARPG_REQUIRE(launcher.break_damage == 18);
    ARPG_REQUIRE(launcher.impact == ImpactKind::launch);
    ARPG_REQUIRE(arpg::test::near(launcher.knockback_speed, 1.2, 1.0e-4));
    ARPG_REQUIRE(arpg::test::near(launcher.launch_speed, 9.5, 1.0e-4));
    ARPG_REQUIRE(launcher.feedback == FeedbackLevel::medium);
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
    {"five attacks", &catalog_has_five_attacks},
    {"stable launcher definition", &launcher_definition_is_stable},
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
