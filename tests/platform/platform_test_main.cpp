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
arpg::test::TestSuite pause_menu_state_suite() noexcept;
arpg::test::TestSuite pause_menu_view_suite() noexcept;
arpg::test::TestSuite window_settings_suite() noexcept;
arpg::test::TestSuite pause_host_gate_suite() noexcept;
arpg::test::TestSuite control_hints_suite() noexcept;
arpg::test::TestSuite hud_view_model_suite() noexcept;
arpg::test::TestSuite hud_notice_state_suite() noexcept;
arpg::test::TestSuite loot_pickup_feedback_suite() noexcept;
arpg::test::TestSuite hud_layout_suite() noexcept;
arpg::test::TestSuite hud_font_suite() noexcept;
arpg::test::TestSuite hud_render_plan_suite() noexcept;
arpg::test::TestSuite hud_host_integration_suite() noexcept;
arpg::test::TestSuite ground_loot_view_suite() noexcept;
arpg::test::TestSuite material_asset_validation_suite() noexcept;
arpg::test::TestSuite material_animation_suite() noexcept;
arpg::test::TestSuite stage12_environment_render_suite() noexcept;
arpg::test::TestSuite stage12_actor_render_suite() noexcept;
arpg::test::TestSuite stage12_material_render_suite() noexcept;

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
        pause_host_gate_suite(),
        inventory_view_math_suite(),
        death_input_gate_suite(),
        death_overlay_view_suite(),
        pause_menu_state_suite(),
        pause_menu_view_suite(),
        window_settings_suite(),
        control_hints_suite(),
        hud_view_model_suite(),
        hud_notice_state_suite(),
        loot_pickup_feedback_suite(),
        hud_layout_suite(),
        hud_font_suite(),
        hud_render_plan_suite(),
        hud_host_integration_suite(),
        ground_loot_view_suite(),
        material_asset_validation_suite(),
        material_animation_suite(),
        stage12_environment_render_suite(),
        stage12_actor_render_suite(),
        stage12_material_render_suite(),
    };

    return arpg::test::run_suites(suites, 308, "stage 13 combat readability runtime");
}
