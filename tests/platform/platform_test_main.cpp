#include "test_framework.hpp"

#if defined(_WIN32) && defined(_DEBUG)
#include <crtdbg.h>
#include <cstdlib>
#endif

arpg::test::TestSuite combat_view_math_suite() noexcept;
arpg::test::TestSuite monster_view_suite() noexcept;
arpg::test::TestSuite combat_feedback_suite() noexcept;
arpg::test::TestSuite host_input_suite() noexcept;
arpg::test::TestSuite dungeon_view_math_suite() noexcept;
arpg::test::TestSuite passive_tree_view_suite() noexcept;
arpg::test::TestSuite host_launch_options_suite() noexcept;
arpg::test::TestSuite dungeon_runtime_suite() noexcept;
arpg::test::TestSuite inventory_view_math_suite() noexcept;
arpg::test::TestSuite death_input_gate_suite() noexcept;
arpg::test::TestSuite death_overlay_view_suite() noexcept;

int main() {
#if defined(_WIN32) && defined(_DEBUG)
    _set_error_mode(_OUT_TO_STDERR);
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
    _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
    _set_abort_behavior(_WRITE_ABORT_MSG,
        _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
    const arpg::test::TestSuite suites[] = {
        combat_view_math_suite(),
        monster_view_suite(),
        combat_feedback_suite(),
        host_input_suite(),
        dungeon_view_math_suite(),
        passive_tree_view_suite(),
        host_launch_options_suite(),
        dungeon_runtime_suite(),
        inventory_view_math_suite(),
        death_input_gate_suite(),
        death_overlay_view_suite(),
    };

    return arpg::test::run_suites(suites, 114, "stage 11b task 4 host input");
}
