#include "test_framework.hpp"

arpg::test::TestSuite combat_view_math_suite() noexcept;
arpg::test::TestSuite combat_feedback_suite() noexcept;
arpg::test::TestSuite combat_key_bindings_suite() noexcept;
arpg::test::TestSuite dungeon_view_math_suite() noexcept;

int main() {
    const arpg::test::TestSuite suites[] = {
        combat_view_math_suite(),
        combat_feedback_suite(),
        combat_key_bindings_suite(),
        dungeon_view_math_suite(),
    };

    return arpg::test::run_suites(suites, 12, "platform");
}
