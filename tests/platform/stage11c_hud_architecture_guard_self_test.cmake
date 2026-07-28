foreach(_required_variable IN ITEMS
        SOURCE_ROOT GUARD_SCRIPT BAD_SOURCE GUARD_TEST_ROOT)
    if(NOT DEFINED ${_required_variable})
        message(FATAL_ERROR
            "Stage11C HUD guard self-test requires ${_required_variable}")
    endif()
endforeach()
foreach(_required_file IN ITEMS "${GUARD_SCRIPT}" "${BAD_SOURCE}")
    if(NOT EXISTS "${_required_file}")
        message(FATAL_ERROR "Stage11C HUD guard fixture is missing: ${_required_file}")
    endif()
endforeach()

set(_production_hud_root "${SOURCE_ROOT}/platform/raylib")
file(GLOB _production_hud_sources LIST_DIRECTORIES FALSE
    "${_production_hud_root}/hud_*.h"
    "${_production_hud_root}/hud_*.hpp"
    "${_production_hud_root}/hud_*.c"
    "${_production_hud_root}/hud_*.cc"
    "${_production_hud_root}/hud_*.cpp"
    "${_production_hud_root}/hud_*.cxx")
list(APPEND _production_hud_sources
    "${_production_hud_root}/combat_renderer.hpp"
    "${_production_hud_root}/combat_renderer.cpp"
    "${_production_hud_root}/debug_overlay_renderer.hpp"
    "${_production_hud_root}/debug_overlay_renderer.cpp"
    "${_production_hud_root}/host_validation_input.hpp"
    "${_production_hud_root}/host_validation_input.cpp"
    "${_production_hud_root}/host_validation_navigation.hpp"
    "${_production_hud_root}/host_validation_navigation.cpp"
    "${_production_hud_root}/host_validation_stage11c.hpp"
    "${_production_hud_root}/host_validation_stage11c.cpp"
    "${_production_hud_root}/CMakeLists.txt"
    "${_production_hud_root}/raylib_host.cpp")
if(NOT _production_hud_sources)
    message(FATAL_ERROR "Stage11C production HUD sources are missing")
endif()

file(READ "${BAD_SOURCE}" _bad_source_text)
foreach(_fixture_token IN ITEMS
        "GetKeyPressed" "DungeonSession" "fixed_tick" "SettingsStore"
        "std::string" "std::vector" "Budget" "FOURTH_STATUS_TAG")
    string(FIND "${_bad_source_text}" "${_fixture_token}" _fixture_index)
    if(_fixture_index EQUAL -1)
        message(FATAL_ERROR
            "Stage11C bad-source manifest lacks ${_fixture_token}")
    endif()
endforeach()

file(MAKE_DIRECTORY "${GUARD_TEST_ROOT}")

function(arpg_expect_hud_guard_rejects
        NAME TARGET_FILE INSERT_BEFORE MUTATION REASON)
    set(_mutation_root "${GUARD_TEST_ROOT}/${NAME}")
    file(MAKE_DIRECTORY "${_mutation_root}")
    foreach(_production_source IN LISTS _production_hud_sources)
        get_filename_component(_source_name "${_production_source}" NAME)
        file(COPY_FILE "${_production_source}"
            "${_mutation_root}/${_source_name}" ONLY_IF_DIFFERENT)
    endforeach()

    set(_mutated_source "${_mutation_root}/${TARGET_FILE}")
    if(NOT EXISTS "${_mutated_source}")
        message(FATAL_ERROR
            "Stage11C mutation ${NAME} target is missing: ${TARGET_FILE}")
    endif()
    if(INSERT_BEFORE STREQUAL "")
        file(APPEND "${_mutated_source}"
            "\n// Stage11C ${NAME} mutation\n${MUTATION}\n")
    else()
        file(READ "${_mutated_source}" _mutated_text)
        set(_original_text "${_mutated_text}")
        string(REPLACE "${INSERT_BEFORE}"
            "${MUTATION}\n${INSERT_BEFORE}" _mutated_text "${_mutated_text}")
        if(_mutated_text STREQUAL _original_text)
            message(FATAL_ERROR
                "Stage11C mutation ${NAME} anchor is missing: ${INSERT_BEFORE}")
        endif()
        file(WRITE "${_mutated_source}" "${_mutated_text}")
    endif()

    execute_process(
        COMMAND "${CMAKE_COMMAND}"
            "-DSOURCE_ROOT=${SOURCE_ROOT}"
            "-DHUD_SOURCE_ROOT=${_mutation_root}"
            -P "${GUARD_SCRIPT}"
        RESULT_VARIABLE _result
        OUTPUT_VARIABLE _stdout
        ERROR_VARIABLE _stderr)
    if(_result EQUAL 0)
        message(FATAL_ERROR
            "Stage11C HUD guard accepted ${NAME} production-source mutation")
    endif()
    set(_combined "${_stdout}\n${_stderr}")
    string(FIND "${_combined}" "${REASON}" _reason_index)
    if(_reason_index EQUAL -1)
        message(FATAL_ERROR
            "Stage11C HUD mutation ${NAME} failed for wrong reason; "
            "expected '${REASON}', got: ${_combined}")
    endif()
endfunction()

arpg_expect_hud_guard_rejects(get_key_pressed combat_renderer.cpp ""
    "int stage11c_bad_key() { return GetKeyPressed(); }"
    "HUD boundary rejects physical input sampling")
arpg_expect_hud_guard_rejects(dungeon_session debug_overlay_renderer.hpp ""
    "DungeonSession* stage11c_bad_session = nullptr;"
    "HUD boundary rejects DungeonSession access")
arpg_expect_hud_guard_rejects(fixed_tick raylib_host.cpp
    "GetFrameTime(), true);"
    "runtime.fixed_tick({});"
    "HUD boundary rejects fixed tick access")
arpg_expect_hud_guard_rejects(settings_store combat_renderer.hpp ""
    "SettingsStore* stage11c_bad_store = nullptr;"
    "HUD boundary rejects SettingsStore access")
arpg_expect_hud_guard_rejects(dynamic_string debug_overlay_renderer.cpp ""
    "std::string stage11c_bad_string;"
    "HUD boundary rejects dynamic std::string")
arpg_expect_hud_guard_rejects(dynamic_vector raylib_host.cpp
    "const GroundLootView ground_loot_view = [&]() noexcept {"
    "std::vector<int> stage11c_bad_vector;"
    "HUD boundary rejects dynamic std::vector")
arpg_expect_hud_guard_rejects(legacy_budget raylib_host.cpp
    "const GroundLootView ground_loot_view = [&]() noexcept {"
    "constexpr const char* stage11c_bad_text = \"Budget\";"
    "Normal HUD rejects legacy Budget text")
arpg_expect_hud_guard_rejects(fourth_status_tag hud_view_model.cpp ""
    "void stage11c_bad_fourth(HudViewModel& output) { append_status_tag(output.player, HudStatusTagKind::slow); }"
    "HUD visible status tag limit rejects fourth tag")

message(STATUS
    "Stage 11C HUD architecture guard rejected all eight production-source mutations")
