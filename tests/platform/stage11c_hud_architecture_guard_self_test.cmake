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
    "${_production_hud_root}/host_validation_runtime.cpp"
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

function(arpg_expect_hud_guard_rejects_after_replace
        NAME TARGET_FILE SEARCH REPLACEMENT TAIL_MUTATION REASON)
    set(_mutation_root "${GUARD_TEST_ROOT}/${NAME}")
    file(MAKE_DIRECTORY "${_mutation_root}")
    foreach(_production_source IN LISTS _production_hud_sources)
        get_filename_component(_source_name "${_production_source}" NAME)
        file(COPY_FILE "${_production_source}"
            "${_mutation_root}/${_source_name}" ONLY_IF_DIFFERENT)
    endforeach()

    set(_mutated_source "${_mutation_root}/${TARGET_FILE}")
    file(READ "${_mutated_source}" _mutated_text)
    set(_original_text "${_mutated_text}")
    string(REPLACE "${SEARCH}" "${REPLACEMENT}" _mutated_text "${_mutated_text}")
    if(_mutated_text STREQUAL _original_text)
        message(FATAL_ERROR
            "Stage11C replacement mutation ${NAME} anchor is missing: ${SEARCH}")
    endif()
    if(NOT TAIL_MUTATION STREQUAL "")
        string(APPEND _mutated_text "\n${TAIL_MUTATION}\n")
    endif()
    file(WRITE "${_mutated_source}" "${_mutated_text}")

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
            "Stage11C HUD replacement mutation ${NAME} failed for wrong reason; "
            "expected '${REASON}', got: ${_combined}")
    endif()
endfunction()

function(arpg_expect_hud_guard_rejects_source NAME TARGET_FILE MUTATED REASON)
    set(_mutation_root "${GUARD_TEST_ROOT}/${NAME}")
    file(MAKE_DIRECTORY "${_mutation_root}")
    foreach(_production_source IN LISTS _production_hud_sources)
        get_filename_component(_source_name "${_production_source}" NAME)
        file(COPY_FILE "${_production_source}"
            "${_mutation_root}/${_source_name}" ONLY_IF_DIFFERENT)
    endforeach()
    set(_mutated_source "${_mutation_root}/${TARGET_FILE}")
    file(WRITE "${_mutated_source}" "${MUTATED}")
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

function(arpg_expect_hud_guard_accepts_source NAME TARGET_FILE MUTATED)
    set(_mutation_root "${GUARD_TEST_ROOT}/${NAME}")
    file(MAKE_DIRECTORY "${_mutation_root}")
    foreach(_production_source IN LISTS _production_hud_sources)
        get_filename_component(_source_name "${_production_source}" NAME)
        file(COPY_FILE "${_production_source}"
            "${_mutation_root}/${_source_name}" ONLY_IF_DIFFERENT)
    endforeach()
    file(WRITE "${_mutation_root}/${TARGET_FILE}" "${MUTATED}")
    execute_process(
        COMMAND "${CMAKE_COMMAND}"
            "-DSOURCE_ROOT=${SOURCE_ROOT}"
            "-DHUD_SOURCE_ROOT=${_mutation_root}"
            -P "${GUARD_SCRIPT}"
        RESULT_VARIABLE _result
        OUTPUT_VARIABLE _stdout
        ERROR_VARIABLE _stderr)
    if(NOT _result EQUAL 0)
        message(FATAL_ERROR
            "Stage11C HUD guard rejected harmless ${NAME} variant: ${_stdout}${_stderr}")
    endif()
endfunction()

set(_task7c_mask_hud_observer
    "            validation_runtime->observe_hud(\n                current, renderer.hud_model(), renderer.hud_notice_view(),\n                draw_debug, GetScreenWidth(), GetScreenHeight());")
set(_task7c_mask_hud_observer_get
    "            validation_runtime.get()->observe_hud(\n                current, renderer.hud_model(), renderer.hud_notice_view(),\n                draw_debug, GetScreenWidth(), GetScreenHeight());")
set(_task7c_mask_renderer_hud_anchor
    "            renderer.observe_presented_hud_frame(hud_presented_frame,")
file(READ "${_production_hud_root}/raylib_host.cpp"
    _task7c_mask_host_source)

string(REPLACE "${_task7c_mask_hud_observer}"
    "            if (false) {\n${_task7c_mask_hud_observer}\n            }"
    _task7c_m08_mask_host "${_task7c_mask_host_source}")
string(REPLACE "${_task7c_mask_renderer_hud_anchor}"
    "${_task7c_mask_hud_observer_get}\n${_task7c_mask_renderer_hud_anchor}"
    _task7c_m08_mask_host "${_task7c_m08_mask_host}")
arpg_expect_hud_guard_rejects_source(task7c_m08_dead_decoy_get_call
    raylib_host.cpp "${_task7c_m08_mask_host}" "T7C-M08")

string(REPLACE "${_task7c_mask_hud_observer}"
    "            const auto task7c_hud_observer_decoy = [&] {\n${_task7c_mask_hud_observer}\n            };"
    _task7c_m09_mask_host "${_task7c_mask_host_source}")
string(REPLACE "            BeginDrawing();"
    "            BeginDrawing();\n${_task7c_mask_hud_observer_get}"
    _task7c_m09_mask_host "${_task7c_m09_mask_host}")
arpg_expect_hud_guard_rejects_source(task7c_m09_lambda_decoy_get_call
    raylib_host.cpp "${_task7c_m09_mask_host}" "T7C-M09")

string(REPLACE "${_task7c_mask_hud_observer}"
    "${_task7c_mask_hud_observer_get}"
    _task7c_get_accessor_host "${_task7c_mask_host_source}")
arpg_expect_hud_guard_accepts_source(task7c_get_accessor_equivalent
    raylib_host.cpp "${_task7c_get_accessor_host}")

if(DEFINED STAGE11C_TASK7C_HOST_MASK_ONLY)
    message(STATUS
        "Stage11C HUD architecture Task7C Host-mask cases passed")
    return()
endif()

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
arpg_expect_hud_guard_rejects(legacy_budget hud_renderer.cpp ""
    "constexpr const char* stage11c_bad_text = \"Budget\";"
    "Normal HUD rejects legacy Budget text")
arpg_expect_hud_guard_rejects(fourth_status_tag hud_view_model.cpp ""
    "void stage11c_bad_fourth(HudViewModel& output) { append_status_tag(output.player, HudStatusTagKind::slow); }"
    "HUD visible status tag limit rejects fourth tag")
arpg_expect_hud_guard_rejects_after_replace(stage_header_comment_decoy
    host_validation_stage11c.hpp
    "struct Stage11CHudValidationState final {"
    "struct Stage11CHudValidationState;\n// struct Stage11CHudValidationState final {"
    ""
    "Stage11C HUD state definition is missing from private Stage header")
arpg_expect_hud_guard_rejects_after_replace(stage_source_comment_string_decoy
    host_validation_stage11c.cpp
    "std::uint64_t stage11c_production_snapshot_hash("
    "// std::uint64_t stage11c_production_snapshot_hash(\nconstexpr const char* stage11c_hash_decoy = \"std::uint64_t stage11c_production_snapshot_hash(\";\nstd::uint64_t stage11c_missing_snapshot_hash("
    ""
    "Evidence validation function is missing: std::uint64_t")
arpg_expect_hud_guard_rejects_after_replace(stage_source_forward_decoy
    host_validation_stage11c.cpp
    "bool stage11c_hud_validation_reached(\n    const dungeon::DungeonSnapshot& snapshot,\n    Stage11CHudValidationScenario scenario,\n    const Stage11CHudValidationState& state, bool draw_debug) noexcept {"
    "bool stage11c_hud_validation_reached(\n    const dungeon::DungeonSnapshot& snapshot,\n    Stage11CHudValidationScenario scenario,\n    const Stage11CHudValidationState& state, bool draw_debug) noexcept;\nbool stage11c_hud_validation_reached_moved(\n    const dungeon::DungeonSnapshot& snapshot,\n    Stage11CHudValidationScenario scenario,\n    const Stage11CHudValidationState& state, bool draw_debug) noexcept {"
    ""
    "Stage11C HUD definition is missing from Stage source")
arpg_expect_hud_guard_rejects_after_replace(stage_cmake_comment_quoted_decoy
    CMakeLists.txt
    "    host_validation_stage11c.cpp"
    "    # decoy;host_validation_stage11c.cpp\nmessage(STATUS \"host_validation_stage11c.cpp\")"
    ""
    "arpg_raylib does not register host_validation_stage11c.cpp exactly once")
arpg_expect_hud_guard_rejects_after_replace(stage_cmake_bracket_comment_decoy
    CMakeLists.txt
    "    host_validation_stage11c.cpp"
    [==[
    #[=[
    host_validation_stage11c.cpp
    ]]
    ]=]
]==]
    ""
    "arpg_raylib does not register host_validation_stage11c.cpp exactly once")
arpg_expect_hud_guard_rejects_after_replace(stage_cmake_bracket_argument_decoy
    CMakeLists.txt
    "    host_validation_stage11c.cpp"
    [===[
    [==[
    host_validation_stage11c.cpp
    # ; (add_library(arpg_raylib fake_target))
    ]==]
]===]
    ""
    "arpg_raylib does not register host_validation_stage11c.cpp exactly once")

set(_task7c_hud_observer
    "            validation_runtime->observe_hud(\n                current, renderer.hud_model(), renderer.hud_notice_view(),\n                draw_debug, GetScreenWidth(), GetScreenHeight());")
set(_task7c_renderer_hud_anchor
    "            renderer.observe_presented_hud_frame(hud_presented_frame,")
file(READ "${_production_hud_root}/raylib_host.cpp" _task7c_host_source)
string(REPLACE "${_task7c_hud_observer}\n" ""
    _task7c_m08_host "${_task7c_host_source}")
string(REPLACE "${_task7c_renderer_hud_anchor}"
    "${_task7c_hud_observer}\n${_task7c_renderer_hud_anchor}"
    _task7c_m08_host "${_task7c_m08_host}")
if(_task7c_m08_host STREQUAL _task7c_host_source)
    message(FATAL_ERROR "Task7C M08 HUD-order mutation anchor is missing")
endif()
arpg_expect_hud_guard_rejects_source(task7c_m08_hud_before_renderer
    raylib_host.cpp "${_task7c_m08_host}"
    "T7C-M08")

string(REPLACE "${_task7c_hud_observer}\n" ""
    _task7c_m09_host "${_task7c_host_source}")
string(REPLACE "            BeginDrawing();"
    "            BeginDrawing();\n${_task7c_hud_observer}"
    _task7c_m09_host "${_task7c_m09_host}")
if(_task7c_m09_host STREQUAL _task7c_host_source)
    message(FATAL_ERROR "Task7C M09 HUD-order mutation anchor is missing")
endif()
arpg_expect_hud_guard_rejects_source(task7c_m09_hud_after_begin_drawing
    raylib_host.cpp "${_task7c_m09_host}"
    "T7C-M09")

string(REPLACE "HostExitCode run_raylib_host("
    "#if 0\nHostExitCode run_raylib_host() { validation_runtime->observe_hud(current, renderer.hud_model(), renderer.hud_notice_view(), draw_debug, GetScreenWidth(), GetScreenHeight()); }\n#endif\nHostExitCode run_raylib_host("
    _inactive_host_decoy "${_task7c_host_source}")
arpg_expect_hud_guard_accepts_source(inactive_host_facade_decoy
    raylib_host.cpp "${_inactive_host_decoy}")
file(READ "${_production_hud_root}/host_validation_runtime.cpp"
    _task7c_runtime_source)
string(REPLACE "void HostValidationRuntime::observe_hud("
    "#if 0\nvoid HostValidationRuntime::observe_hud() {}\n#endif\nvoid HostValidationRuntime::observe_hud("
    _inactive_runtime_decoy "${_task7c_runtime_source}")
arpg_expect_hud_guard_accepts_source(inactive_runtime_facade_decoy
    host_validation_runtime.cpp "${_inactive_runtime_decoy}")

message(STATUS
    "Stage 11C HUD architecture guard rejected 16 mutations and accepted 3 harmless variants")
