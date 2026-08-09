#include "test_framework.hpp"

#include <cstdlib>

#if defined(_WIN32) && defined(_DEBUG)
#include <crtdbg.h>
#include <cstdlib>
#endif

arpg::test::TestSuite combat_view_math_suite() noexcept;
arpg::test::TestSuite monster_view_suite() noexcept;
arpg::test::TestSuite combat_feedback_suite() noexcept;
arpg::test::TestSuite audio_pack_suite() noexcept;
arpg::test::TestSuite audio_routing_suite() noexcept;
arpg::test::TestSuite audio_scene_suite() noexcept;
arpg::test::TestSuite stage15_audio_pack_suite() noexcept;
arpg::test::TestSuite host_input_suite() noexcept;
arpg::test::TestSuite host_validation_exit_suite() noexcept;
arpg::test::TestSuite active_skill_input_suite() noexcept;
arpg::test::TestSuite raylib_input_suite() noexcept;
arpg::test::TestSuite active_skill_view_suite() noexcept;
arpg::test::TestSuite active_skill_asset_suite() noexcept;
arpg::test::TestSuite active_skill_loadout_view_suite() noexcept;
arpg::test::TestSuite dungeon_view_math_suite() noexcept;
arpg::test::TestSuite task6_exit_unlock_view_suite() noexcept;
arpg::test::TestSuite passive_tree_view_suite() noexcept;
arpg::test::TestSuite host_launch_options_suite() noexcept;
arpg::test::TestSuite dungeon_runtime_suite() noexcept;
arpg::test::TestSuite dungeon_runtime_v10_migration_suite() noexcept;
arpg::test::TestSuite inventory_view_math_suite() noexcept;
arpg::test::TestSuite death_input_gate_suite() noexcept;
arpg::test::TestSuite death_overlay_view_suite() noexcept;
arpg::test::TestSuite pause_menu_state_suite() noexcept;
arpg::test::TestSuite pause_menu_view_suite() noexcept;
arpg::test::TestSuite window_settings_suite() noexcept;
arpg::test::TestSuite pause_host_gate_suite() noexcept;
arpg::test::TestSuite host_settings_runtime_suite() noexcept;
arpg::test::TestSuite host_window_lifetime_suite() noexcept;
arpg::test::TestSuite control_hints_suite() noexcept;
arpg::test::TestSuite hud_view_model_suite() noexcept;
arpg::test::TestSuite hud_notice_state_suite() noexcept;
arpg::test::TestSuite loot_pickup_feedback_suite() noexcept;
arpg::test::TestSuite hud_layout_suite() noexcept;
arpg::test::TestSuite hud_font_suite() noexcept;
arpg::test::TestSuite hud_render_plan_suite() noexcept;
arpg::test::TestSuite hud_host_integration_suite() noexcept;
arpg::test::TestSuite ground_loot_view_suite() noexcept;
arpg::test::TestSuite material_loot_view_suite() noexcept;
arpg::test::TestSuite material_bag_renderer_suite() noexcept;
arpg::test::TestSuite ui_material_slice_suite() noexcept;
arpg::test::TestSuite audio_asset_validation_suite() noexcept;
arpg::test::TestSuite material_asset_validation_suite() noexcept;
arpg::test::TestSuite material_animation_suite() noexcept;
arpg::test::TestSuite fire_room_material_slice_suite() noexcept;
arpg::test::TestSuite ecology_material_coverage_suite() noexcept;
arpg::test::TestSuite room_background_render_plan_suite() noexcept;
arpg::test::TestSuite stage12_environment_render_suite() noexcept;
arpg::test::TestSuite stage12_actor_render_suite() noexcept;
arpg::test::TestSuite stage12_material_render_suite() noexcept;
arpg::test::TestSuite large_room_render_plan_suite() noexcept;

namespace {

bool task11_large_room_only() noexcept {
    char* value = nullptr;
    std::size_t length = 0U;
    const errno_t error = _dupenv_s(&value, &length,
        "ARPG_TASK11_LARGE_ROOM_ONLY");
    const bool enabled = error == 0 && value != nullptr;
    std::free(value);
    return enabled;
}

bool task9_environment_render_only() noexcept {
    char* value = nullptr;
    std::size_t length = 0U;
    const errno_t error = _dupenv_s(&value, &length,
        "ARPG_TASK9_ENVIRONMENT_RENDER_ONLY");
    const bool enabled = error == 0 && value != nullptr;
    std::free(value);
    return enabled;
}

bool v10_migration_only() noexcept {
    char* value = nullptr;
    std::size_t length = 0U;
    const errno_t error = _dupenv_s(&value, &length,
        "ARPG_V10_MIGRATION_ONLY");
    const bool enabled = error == 0 && value != nullptr;
    std::free(value);
    return enabled;
}

bool active_skill_view_only() noexcept {
    char* value = nullptr;
    std::size_t length = 0U;
    const errno_t error = _dupenv_s(&value, &length,
        "ARPG_ACTIVE_SKILL_VIEW_ONLY");
    const bool enabled = error == 0 && value != nullptr;
    std::free(value);
    return enabled;
}

bool cplay014_pause_menu_only() noexcept {
    char* value = nullptr;
    std::size_t length = 0U;
    const errno_t error = _dupenv_s(&value, &length,
        "ARPG_CPLAY014_PAUSE_MENU_ONLY");
    const bool enabled = error == 0 && value != nullptr;
    std::free(value);
    return enabled;
}

bool cplay015_passive_feedback_only() noexcept {
    char* value = nullptr;
    std::size_t length = 0U;
    const errno_t error = _dupenv_s(&value, &length,
        "ARPG_CPLAY015_PASSIVE_FEEDBACK_ONLY");
    const bool enabled = error == 0 && value != nullptr;
    std::free(value);
    return enabled;
}

bool cplay016_ground_potion_label_only() noexcept {
    char* value = nullptr;
    std::size_t length = 0U;
    const errno_t error = _dupenv_s(&value, &length,
        "ARPG_CPLAY016_GROUND_POTION_LABEL_ONLY");
    const bool enabled = error == 0 && value != nullptr;
    std::free(value);
    return enabled;
}

}  // namespace

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
    if (cplay016_ground_potion_label_only()) {
        const arpg::test::TestSuite cplay016_only[] = {
            hud_host_integration_suite(),
            material_loot_view_suite(),
        };
        return arpg::test::run_suites(cplay016_only, 31,
            "CPLAY-016 compact ground potion labels");
    }

    if (cplay015_passive_feedback_only()) {
        const arpg::test::TestSuite cplay015_only[] = {
            passive_tree_view_suite(),
            control_hints_suite(),
            hud_notice_state_suite(),
        };
        return arpg::test::run_suites(cplay015_only, 31,
            "CPLAY-015 blocked passive tree feedback");
    }

    if (cplay014_pause_menu_only()) {
        const arpg::test::TestSuite cplay014_only[] = {
            pause_menu_view_suite(),
            material_asset_validation_suite(),
        };
        return arpg::test::run_suites(cplay014_only, 53,
            "CPLAY-014 pause menu visual contract");
    }

    if (active_skill_view_only()) {
        const arpg::test::TestSuite active_skill_only[] = {
            active_skill_view_suite(),
        };
        return arpg::test::run_suites(active_skill_only, 11,
            "active skill HUD numpad labels");
    }

    if (task11_large_room_only()) {
        const arpg::test::TestSuite task11_only[] = {
            large_room_render_plan_suite(),
        };
        return arpg::test::run_suites(task11_only, 4,
            "task 11 large room render preparation validation");
    }

    if (task9_environment_render_only()) {
        const arpg::test::TestSuite task9_only[] = {
            material_asset_validation_suite(),
            room_background_render_plan_suite(),
            stage12_environment_render_suite(),
        };
        return arpg::test::run_suites(task9_only, 57,
            "task 9 deterministic world environment render");
    }

    if (v10_migration_only()) {
        const arpg::test::TestSuite migration_only[] = {
            dungeon_runtime_v10_migration_suite(),
        };
        return arpg::test::run_suites(migration_only, 1,
            "v9 to v10 runtime migration");
    }

    const arpg::test::TestSuite suites[] = {
        combat_view_math_suite(),
        monster_view_suite(),
        combat_feedback_suite(),
        audio_pack_suite(),
        audio_routing_suite(),
        audio_scene_suite(),
        stage15_audio_pack_suite(),
        host_input_suite(),
        host_validation_exit_suite(),
        active_skill_input_suite(),
        raylib_input_suite(),
        active_skill_view_suite(),
        active_skill_asset_suite(),
        active_skill_loadout_view_suite(),
        dungeon_view_math_suite(),
        task6_exit_unlock_view_suite(),
        passive_tree_view_suite(),
        host_launch_options_suite(),
        dungeon_runtime_suite(),
        dungeon_runtime_v10_migration_suite(),
        pause_host_gate_suite(),
        host_settings_runtime_suite(),
        host_window_lifetime_suite(),
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
        material_loot_view_suite(),
        material_bag_renderer_suite(),
        ui_material_slice_suite(),
        audio_asset_validation_suite(),
        material_asset_validation_suite(),
        material_animation_suite(),
        fire_room_material_slice_suite(),
        ecology_material_coverage_suite(),
        room_background_render_plan_suite(),
        stage12_environment_render_suite(),
        stage12_actor_render_suite(),
        stage12_material_render_suite(),
        large_room_render_plan_suite(),
    };

    return arpg::test::run_suites(suites, 643,
        "host settings runtime contract");
}
