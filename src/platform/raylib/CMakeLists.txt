add_library(arpg_raylib STATIC
    combat_audio.cpp
    combat_feedback.cpp
    combat_renderer.cpp
    combat_view_math.cpp
    dungeon_view_math.cpp
    host_launch_options.cpp
    raylib_host.cpp)

target_include_directories(arpg_raylib
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_features(arpg_raylib PUBLIC cxx_std_17)
target_link_libraries(arpg_raylib
    PUBLIC arpg_dungeon raylib
    PRIVATE arpg_core)

arpg_enable_project_warnings(arpg_raylib)
