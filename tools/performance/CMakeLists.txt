add_executable(arpg_save_commit_probe save_commit_probe.cpp)

target_compile_features(arpg_save_commit_probe PRIVATE cxx_std_17)
target_link_libraries(
    arpg_save_commit_probe PRIVATE arpg_persistence arpg_dungeon)
arpg_enable_project_warnings(arpg_save_commit_probe)

if(WIN32 AND ARPG_BUILD_GRAPHICS)
    if(NOT DEFINED ARPG_TASK11_RENDER_BENCHMARK_VARIANT)
        set(ARPG_TASK11_RENDER_BENCHMARK_VARIANT current)
    endif()
    if(ARPG_TASK11_RENDER_BENCHMARK_VARIANT STREQUAL "baseline")
        set(_arpg_task11_current_render_prep 0)
    elseif(ARPG_TASK11_RENDER_BENCHMARK_VARIANT STREQUAL "current")
        set(_arpg_task11_current_render_prep 1)
    else()
        message(FATAL_ERROR
            "ARPG_TASK11_RENDER_BENCHMARK_VARIANT must be baseline or current")
    endif()

    add_executable(arpg_room_render_prep_probe room_render_prep_probe.cpp)
    target_compile_features(arpg_room_render_prep_probe PRIVATE cxx_std_17)
    target_compile_definitions(arpg_room_render_prep_probe PRIVATE
        ARPG_TASK11_CURRENT_RENDER_PREP=${_arpg_task11_current_render_prep}
        ARPG_TASK11_BENCHMARK_REVISION="${ARPG_TASK11_BENCHMARK_REVISION}")
    target_link_libraries(arpg_room_render_prep_probe PRIVATE arpg_raylib)
    arpg_enable_project_warnings(arpg_room_render_prep_probe)
endif()
