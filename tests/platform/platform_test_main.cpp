#include "test_framework.hpp"

arpg::test::TestSuite combat_view_math_suite() noexcept;
arpg::test::TestSuite monster_view_suite() noexcept;
arpg::test::TestSuite combat_feedback_suite() noexcept;
arpg::test::TestSuite combat_key_bindings_suite() noexcept;
arpg::test::TestSuite dungeon_view_math_suite() noexcept;
arpg::test::TestSuite host_launch_options_suite() noexcept;
arpg::test::TestSuite dungeon_runtime_suite() noexcept;

int main() {
    const arpg::test::TestSuite suites[] = {
        combat_view_math_suite(),
        monster_view_suite(),
        combat_feedback_suite(),
        combat_key_bindings_suite(),
        dungeon_view_math_suite(),
        host_launch_options_suite(),
        dungeon_runtime_suite(),
    };

    return arpg::test::run_suites(suites, 45, "stage 6 platform");
}
