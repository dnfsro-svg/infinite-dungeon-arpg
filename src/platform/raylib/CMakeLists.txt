add_library(arpg_raylib STATIC
    active_skill_renderer.cpp
    active_skill_assets.cpp
    active_skill_view.cpp
    actor_renderer.cpp
    audio_pack.cpp
    audio_asset_validation.cpp
    audio_routing.cpp
    audio_scene.cpp
    combat_audio.cpp
    combat_feedback.cpp
    combat_renderer.cpp
    control_hints.cpp
    combat_view_math.cpp
    death_overlay_font.cpp
    death_overlay_renderer.cpp
    death_overlay_view.cpp
    debug_renderer.cpp
    debug_overlay_renderer.cpp
    fire_room_material_slice.cpp
    dungeon_view_math.cpp
    dungeon_runtime.cpp
    environment_prop_layout.cpp
    game_audio.cpp
    ground_loot_view.cpp
    host_frame_gate.cpp
    host_settings_runtime.cpp
    host_window_lifetime.cpp
    host_input.cpp
    host_launch_options.cpp
    host_validation_input.cpp
    host_validation_navigation.cpp
    host_validation_runtime.cpp
    host_validation_stage10_11.cpp
    host_validation_stage11b.cpp
    host_validation_stage11c.cpp
    host_validation_stage11d_report.cpp
    host_validation_stage11d_runtime.cpp
    host_validation_stage17_report.cpp
    host_validation_stage17_runtime.cpp
    hud_font.cpp
    hud_view_model.cpp
    hud_notice_state.cpp
    loot_pickup_feedback.cpp
    loot_suction_animation.cpp
    material_bag_renderer.cpp
    material_loot_view.cpp
    hud_layout.cpp
    hud_renderer.cpp
    inventory_renderer.cpp
    inventory_view_math.cpp
    ui_text_bounds_audit.cpp
    ui_text_renderer.cpp
    material_animation.cpp
    material_asset_validation.cpp
    material_pack.cpp
    material_residency.cpp
    monster_material_presenter.cpp
    passive_tree_renderer.cpp
    passive_tree_view_math.cpp
    pause_menu_state.cpp
    pause_menu_view.cpp
    pause_menu_renderer.cpp
    procedural_audio.cpp
    raylib_input.cpp
    raylib_host.cpp
    room_background_render_plan.cpp
    room_renderer.cpp
    stable_key_raylib.cpp
    stream_pack.cpp
    ui_audio_pack.cpp
    window_settings.cpp)
target_sources(arpg_raylib PRIVATE water_room_material_slice.cpp)
target_sources(arpg_raylib PRIVATE lightning_room_material_slice.cpp)
target_sources(arpg_raylib PRIVATE chaos_room_material_slice.cpp)

target_include_directories(arpg_raylib
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_features(arpg_raylib PUBLIC cxx_std_17)
target_link_libraries(arpg_raylib
    PUBLIC arpg_dungeon arpg_persistence raylib
    PRIVATE arpg_core arpg_settings)

set(_arpg_raylib_lifecycle_policy
    "${CMAKE_CURRENT_SOURCE_DIR}/raylib_lifecycle_poison.hpp")
if(MSVC)
    target_compile_options(arpg_raylib PRIVATE
        "/FI${_arpg_raylib_lifecycle_policy}")
else()
    target_compile_options(arpg_raylib PRIVATE
        -include "${_arpg_raylib_lifecycle_policy}")
endif()

set_source_files_properties(host_window_lifetime.cpp PROPERTIES
    COMPILE_DEFINITIONS
        "ARPG_ALLOW_RAYLIB_SET_CONFIG_FLAGS;ARPG_ALLOW_RAYLIB_INIT_WINDOW;ARPG_ALLOW_RAYLIB_IS_WINDOW_READY;ARPG_ALLOW_RAYLIB_CLOSE_WINDOW")
set_source_files_properties(
    combat_renderer.cpp
    death_overlay_renderer.cpp
    hud_renderer.cpp
    pause_menu_renderer.cpp
    window_settings.cpp
    PROPERTIES COMPILE_DEFINITIONS ARPG_ALLOW_RAYLIB_IS_WINDOW_READY)
set_source_files_properties(raylib_host.cpp PROPERTIES
    COMPILE_DEFINITIONS ARPG_ALLOW_RAYLIB_WINDOW_SHOULD_CLOSE)

if(WIN32)
    target_link_libraries(arpg_raylib PRIVATE user32)
endif()

arpg_enable_project_warnings(arpg_raylib)
if(MSVC)
    target_compile_options(arpg_raylib PRIVATE /utf-8)
endif()
