#include "test_framework.hpp"

#include "combat_key_bindings.hpp"

namespace {

using namespace arpg;

static_assert(platform::kInventoryKey == KEY_I);
static_assert(platform::kPassiveOverlayKey == KEY_P);
static_assert(platform::kInventoryKey != platform::kPassiveOverlayKey);

test::Failure combat_keys_are_j_k_and_l_without_u() noexcept {
    ARPG_REQUIRE(platform::kCombatKeyBindings.size() == 3);
    ARPG_REQUIRE(platform::kCombatKeyBindings[0].key == KEY_J);
    ARPG_REQUIRE(
        platform::kCombatKeyBindings[0].action == combat::Action::light);
    ARPG_REQUIRE(platform::kCombatKeyBindings[1].key == KEY_K);
    ARPG_REQUIRE(
        platform::kCombatKeyBindings[1].action == combat::Action::jump);
    ARPG_REQUIRE(platform::kCombatKeyBindings[2].key == KEY_L);
    ARPG_REQUIRE(
        platform::kCombatKeyBindings[2].action == combat::Action::launcher);
    for (const platform::CombatKeyBinding& binding
         : platform::kCombatKeyBindings) {
        ARPG_REQUIRE(binding.key != KEY_U);
    }
    return {};
}

test::Failure overlay_keys_are_i_and_p() noexcept {
    return {};
}

constexpr test::TestCase kCases[] = {
    {"J K L bindings without U", &combat_keys_are_j_k_and_l_without_u},
    {"I P overlay bindings", &overlay_keys_are_i_and_p},
};

}  // namespace

arpg::test::TestSuite combat_key_bindings_suite() noexcept {
    return arpg::test::make_suite("combat_key_bindings", kCases);
}
