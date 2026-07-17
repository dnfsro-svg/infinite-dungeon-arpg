add_library(arpg_raylib STATIC
    actor_renderer.cpp
    combat_audio.cpp
    combat_feedback.cpp
    combat_renderer.cpp
    combat_view_math.cpp
    death_overlay_font.cpp
    death_overlay_renderer.cpp
    death_overlay_view.cpp
    debug_renderer.cpp
    dungeon_view_math.cpp
    dungeon_runtime.cpp
    host_launch_options.cpp
    hud_renderer.cpp
    inventory_renderer.cpp
    inventory_view_math.cpp
    passive_tree_renderer.cpp
    passive_tree_view_math.cpp
    raylib_input.cpp
    raylib_host.cpp
    room_renderer.cpp)

target_include_directories(arpg_raylib
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_features(arpg_raylib PUBLIC cxx_std_17)
target_link_libraries(arpg_raylib
    PUBLIC arpg_dungeon arpg_persistence raylib
    PRIVATE arpg_core)
if(WIN32)
    target_link_libraries(arpg_raylib PRIVATE user32)
endif()

arpg_enable_project_warnings(arpg_raylib)
if(MSVC)
    target_compile_options(arpg_raylib PRIVATE /utf-8)
endif()
