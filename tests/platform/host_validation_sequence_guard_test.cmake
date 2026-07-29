if(NOT DEFINED SOURCE_ROOT)
    message(FATAL_ERROR "SOURCE_ROOT is required")
endif()

include("${CMAKE_CURRENT_LIST_DIR}/../dungeon/evidence_source_scan.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/cmake_source_registration_scan.cmake")

set(_host "${SOURCE_ROOT}/src/platform/raylib/raylib_host.cpp")
if(DEFINED HOST_OVERRIDE)
    set(_host "${HOST_OVERRIDE}")
endif()
if(NOT EXISTS "${_host}")
    message(FATAL_ERROR "Host validation sequence target is missing: ${_host}")
endif()

file(READ "${_host}" _host_text)
evidence_find_cpp_code_token("${_host_text}" "HostExitCode run_raylib_host("
    _runtime_candidate)
if(_runtime_candidate EQUAL -1)
    message(FATAL_ERROR "Host validation sequence guard missing run_raylib_host candidate")
endif()
string(SUBSTRING "${_host_text}" ${_runtime_candidate} -1 _host_runtime_candidate)
evidence_extract_cpp_function_block("${_host_runtime_candidate}"
    "HostExitCode run_raylib_host(" _host_runtime)
evidence_sanitize_cpp_for_scan("${_host_runtime}" _sanitized)

set(_input_header
    "${SOURCE_ROOT}/src/platform/raylib/host_validation_input.hpp")
set(_input_source
    "${SOURCE_ROOT}/src/platform/raylib/host_validation_input.cpp")
set(_navigation_header
    "${SOURCE_ROOT}/src/platform/raylib/host_validation_navigation.hpp")
set(_navigation_source
    "${SOURCE_ROOT}/src/platform/raylib/host_validation_navigation.cpp")
set(_stage10_11_header
    "${SOURCE_ROOT}/src/platform/raylib/host_validation_stage10_11.hpp")
set(_stage10_11_source
    "${SOURCE_ROOT}/src/platform/raylib/host_validation_stage10_11.cpp")
set(_stage11b_header
    "${SOURCE_ROOT}/src/platform/raylib/host_validation_stage11b.hpp")
if(DEFINED STAGE11B_HEADER_OVERRIDE)
    set(_stage11b_header "${STAGE11B_HEADER_OVERRIDE}")
endif()
set(_stage11b_source
    "${SOURCE_ROOT}/src/platform/raylib/host_validation_stage11b.cpp")
set(_stage11c_header
    "${SOURCE_ROOT}/src/platform/raylib/host_validation_stage11c.hpp")
if(DEFINED STAGE11C_HEADER_OVERRIDE)
    set(_stage11c_header "${STAGE11C_HEADER_OVERRIDE}")
endif()
set(_stage11c_source
    "${SOURCE_ROOT}/src/platform/raylib/host_validation_stage11c.cpp")
if(DEFINED STAGE11C_SOURCE_OVERRIDE)
    set(_stage11c_source "${STAGE11C_SOURCE_OVERRIDE}")
endif()
set(_stage11d_header
    "${SOURCE_ROOT}/src/platform/raylib/host_validation_stage11d.hpp")
if(DEFINED STAGE11D_HEADER_OVERRIDE)
    set(_stage11d_header "${STAGE11D_HEADER_OVERRIDE}")
endif()
set(_stage11d_runtime
    "${SOURCE_ROOT}/src/platform/raylib/host_validation_stage11d_runtime.cpp")
if(DEFINED STAGE11D_RUNTIME_OVERRIDE)
    set(_stage11d_runtime "${STAGE11D_RUNTIME_OVERRIDE}")
endif()
set(_stage11d_report
    "${SOURCE_ROOT}/src/platform/raylib/host_validation_stage11d_report.cpp")
if(DEFINED STAGE11D_REPORT_OVERRIDE)
    set(_stage11d_report "${STAGE11D_REPORT_OVERRIDE}")
endif()
set(_raylib_cmake "${SOURCE_ROOT}/src/platform/raylib/CMakeLists.txt")
if(DEFINED CMAKE_OVERRIDE)
    set(_raylib_cmake "${CMAKE_OVERRIDE}")
endif()
foreach(_required IN ITEMS
        "${_input_header}" "${_input_source}"
        "${_navigation_header}" "${_navigation_source}"
        "${_stage10_11_header}" "${_stage10_11_source}"
        "${_stage11b_header}" "${_stage11b_source}"
        "${_stage11c_header}" "${_stage11c_source}"
        "${_stage11d_header}" "${_stage11d_runtime}"
        "${_stage11d_report}" "${_raylib_cmake}")
    if(NOT EXISTS "${_required}")
        message(FATAL_ERROR "Host validation boundary target is missing: ${_required}")
    endif()
endforeach()
file(READ "${_input_header}" _input_header_text)
file(READ "${_input_source}" _input_source_text)
file(READ "${_navigation_header}" _navigation_header_text)
file(READ "${_navigation_source}" _navigation_source_text)
file(READ "${_stage10_11_header}" _stage10_11_header_text)
file(READ "${_stage10_11_source}" _stage10_11_source_text)
file(READ "${_stage11b_header}" _stage11b_header_text)
file(READ "${_stage11b_source}" _stage11b_source_text)
file(READ "${_stage11c_header}" _stage11c_header_text)
file(READ "${_stage11c_source}" _stage11c_source_text)
file(READ "${_stage11d_header}" _stage11d_header_text)
file(READ "${_stage11d_runtime}" _stage11d_runtime_text)
file(READ "${_stage11d_report}" _stage11d_report_text)
file(READ "${_raylib_cmake}" _raylib_cmake_text)

function(stage11d_find_host_code_token TOKEN OUT_POSITION)
    string(FIND "${_host_text}" "${TOKEN}" _raw_position)
    if(_raw_position EQUAL -1)
        set(${OUT_POSITION} -1 PARENT_SCOPE)
        return()
    endif()
    evidence_find_cpp_code_token("${_host_text}" "${TOKEN}" _code_position)
    set(${OUT_POSITION} ${_code_position} PARENT_SCOPE)
endfunction()

function(assert_unique_cpp_definition LABEL SURFACE TOKEN)
    string(FIND "${SURFACE}" "${TOKEN}" _position)
    if(_position EQUAL -1)
        message(FATAL_ERROR
            "Host validation ${LABEL} definition is missing: ${TOKEN}")
    endif()
    string(LENGTH "${TOKEN}" _token_length)
    math(EXPR _after "${_position} + ${_token_length}")
    string(SUBSTRING "${SURFACE}" ${_after} -1 _remainder)
    string(FIND "${_remainder}" "${TOKEN}" _duplicate)
    if(NOT _duplicate EQUAL -1)
        message(FATAL_ERROR
            "Host validation ${LABEL} definition is duplicated: ${TOKEN}")
    endif()
    string(SUBSTRING "${SURFACE}" ${_position} -1 _tail)
    string(FIND "${_tail}" "{" _open)
    string(FIND "${_tail}" ";" _semicolon)
    if(_open EQUAL -1 OR (NOT _semicolon EQUAL -1 AND _semicolon LESS _open))
        message(FATAL_ERROR
            "Host validation ${LABEL} is only a forward declaration: ${TOKEN}")
    endif()
    evidence_find_cpp_function_bounds_in_sanitized("${SURFACE}" "${TOKEN}"
        _function_begin _function_open _function_end)
endfunction()

foreach(_header_text IN ITEMS
        "${_input_header_text}" "${_navigation_header_text}"
        "${_stage10_11_header_text}" "${_stage11b_header_text}"
        "${_stage11c_header_text}" "${_stage11d_header_text}")
    if(_header_text MATCHES "raylib[.]h|renderer|persistence|test")
        message(FATAL_ERROR "Host validation boundary header has a forbidden dependency")
    endif()
endforeach()

foreach(_stage10_11_definition_token IN ITEMS
        "combat::MovementInput stage10_validation_input("
        "combat::MovementInput stage11_validation_input("
        "bool stage10_validation_reached("
        "bool stage11_validation_reached(")
    string(FIND "${_stage10_11_source_text}" "${_stage10_11_definition_token}"
        _stage10_11_definition)
    if(_stage10_11_definition EQUAL -1)
        message(FATAL_ERROR
            "Host validation Stage10/11 definition is missing: ${_stage10_11_definition_token}")
    endif()
    string(FIND "${_host_text}" "${_stage10_11_definition_token}"
        _host_stage10_11_definition)
    if(NOT _host_stage10_11_definition EQUAL -1)
        message(FATAL_ERROR
            "Host validation Stage10/11 definition remains in raylib_host.cpp: ${_stage10_11_definition_token}")
    endif()
endforeach()

foreach(_stage10_11_state_token IN ITEMS
        "struct Stage10ValidationState final"
        "struct Stage11ValidationState final")
    string(FIND "${_stage10_11_header_text}" "${_stage10_11_state_token}"
        _stage10_11_state_definition)
    if(_stage10_11_state_definition EQUAL -1)
        message(FATAL_ERROR
            "Host validation Stage10/11 state definition is missing: ${_stage10_11_state_token}")
    endif()
    string(FIND "${_host_text}" "${_stage10_11_state_token}"
        _host_stage10_11_state_definition)
    if(NOT _host_stage10_11_state_definition EQUAL -1)
        message(FATAL_ERROR
            "Host validation Stage10/11 state definition remains in raylib_host.cpp: ${_stage10_11_state_token}")
    endif()
endforeach()

foreach(_stage11b_definition_token IN ITEMS
        "PhysicalKeySnapshot inject_stage11b_physical_edges("
        "bool stage11b_validation_complete("
        "std::uint64_t stage11b_snapshot_hash("
        "void write_stage11b_validation_summary(")
    string(FIND "${_stage11b_source_text}" "${_stage11b_definition_token}"
        _stage11b_definition)
    if(_stage11b_definition EQUAL -1)
        message(FATAL_ERROR
            "Host validation Stage11B definition is missing: ${_stage11b_definition_token}")
    endif()
    string(FIND "${_host_text}" "${_stage11b_definition_token}"
        _host_stage11b_definition)
    if(NOT _host_stage11b_definition EQUAL -1)
        message(FATAL_ERROR
            "Host validation Stage11B definition remains in raylib_host.cpp: ${_stage11b_definition_token}")
    endif()
endforeach()
if(NOT _stage11b_header_text MATCHES "struct Stage11BValidationState final")
    message(FATAL_ERROR "Host validation Stage11B state definition is missing")
endif()

foreach(_stage11c_definition_token IN ITEMS
        "PhysicalKeySnapshot inject_stage11c_physical_edges("
        "std::uint64_t stage11c_production_snapshot_hash("
        "bool stage11c_hud_validation_reached("
        "void write_stage11c_hud_validation_summary(")
    evidence_extract_cpp_function_block("${_stage11c_source_text}"
        "${_stage11c_definition_token}" _stage11c_function)
    string(FIND "${_stage11c_function}" "{" _stage11c_definition)
    string(FIND "${_stage11c_function}" ";" _stage11c_forward_declaration)
    if(_stage11c_definition EQUAL -1
            OR (NOT _stage11c_forward_declaration EQUAL -1
                AND _stage11c_forward_declaration LESS _stage11c_definition))
        message(FATAL_ERROR
            "Host validation Stage11C definition is missing: ${_stage11c_definition_token}")
    endif()
    string(FIND "${_host_text}" "${_stage11c_definition_token}"
        _host_stage11c_definition)
    if(NOT _host_stage11c_definition EQUAL -1)
        message(FATAL_ERROR
            "Host validation Stage11C definition remains in raylib_host.cpp: ${_stage11c_definition_token}")
    endif()
endforeach()
evidence_sanitize_cpp_for_scan("${_stage11c_header_text}" _stage11c_header_code)
if(NOT _stage11c_header_code MATCHES
        "struct[ \t\r\n]+Stage11CHudValidationState[ \t\r\n]+final[ \t\r\n]*[{]")
    message(FATAL_ERROR "Host validation Stage11C state definition is missing")
endif()
if(_host_text MATCHES "struct Stage11CHudValidationState final")
    message(FATAL_ERROR
        "Host validation Stage11C state definition remains in raylib_host.cpp")
endif()
if(_host_text MATCHES "struct Stage11BValidationState final")
    message(FATAL_ERROR
        "Host validation Stage11B state definition remains in raylib_host.cpp")
endif()

foreach(_stage11d_definition_token IN ITEMS
        "bool stage11d_has_three_ordinary_rarities("
        "const dungeon::GroundItemSnapshot* stage11d_nearest_ground("
        "const combat::MonsterSnapshot* stage11d_priority_monster("
        "bool stage11d_attack_lane("
        "combat::MovementInput stage11d_safe_movement_toward("
        "PhysicalKeySnapshot inject_stage11d_physical_edges("
        "bool stage11d_validation_active("
        "void observe_stage11d_abyss_claim(")
    string(FIND "${_stage11d_runtime_text}"
        "${_stage11d_definition_token}" _stage11d_definition)
    if(_stage11d_definition EQUAL -1)
        message(FATAL_ERROR
            "Host validation Stage11D runtime definition is missing: ${_stage11d_definition_token}")
    endif()
    stage11d_find_host_code_token("${_stage11d_definition_token}"
        _host_stage11d_definition)
    if(NOT _host_stage11d_definition EQUAL -1)
        message(FATAL_ERROR
            "Host validation Stage11D runtime definition remains in raylib_host.cpp: ${_stage11d_definition_token}")
    endif()
endforeach()
evidence_sanitize_cpp_for_scan("${_stage11d_header_text}"
    _stage11d_header_code)
if(NOT _stage11d_header_code MATCHES
        "struct[ \t\r\n]+Stage11DLootValidationState[ \t\r\n]+final[ \t\r\n]*[{]")
    message(FATAL_ERROR "Host validation Stage11D state definition is missing")
endif()
stage11d_find_host_code_token("struct Stage11DLootValidationState final"
    _host_stage11d_state_definition)
if(NOT _host_stage11d_state_definition EQUAL -1)
    message(FATAL_ERROR
        "Host validation Stage11D state definition remains in raylib_host.cpp")
endif()

evidence_sanitize_cpp_for_scan("${_stage11d_report_text}"
    _stage11d_report_code)
foreach(_stage11d_report_definition IN ITEMS
        "bool stage11d_view_has_rarity("
        "void stage11d_record_semantics("
        "bool stage11d_target_visible("
        "const char* stage11d_scenario_name("
        "void write_stage11d_loot_validation_summary(")
    assert_unique_cpp_definition("Stage11D report"
        "${_stage11d_report_code}" "${_stage11d_report_definition}")
    stage11d_find_host_code_token("${_stage11d_report_definition}"
        _host_stage11d_report_definition)
    if(NOT _host_stage11d_report_definition EQUAL -1)
        message(FATAL_ERROR
            "Host validation Stage11D report definition remains in raylib_host.cpp: ${_stage11d_report_definition}")
    endif()
endforeach()

foreach(_stage11d_report_declaration IN ITEMS
        "bool stage11d_target_visible("
        "void stage11d_record_semantics("
        "void write_stage11d_loot_validation_summary(")
    string(FIND "${_stage11d_header_code}"
        "${_stage11d_report_declaration}" _declaration)
    if(_declaration EQUAL -1)
        message(FATAL_ERROR
            "Host validation Stage11D report declaration is missing: ${_stage11d_report_declaration}")
    endif()
    string(LENGTH "${_stage11d_report_declaration}" _declaration_length)
    math(EXPR _after "${_declaration} + ${_declaration_length}")
    string(SUBSTRING "${_stage11d_header_code}" ${_after} -1 _remainder)
    string(FIND "${_remainder}" "${_stage11d_report_declaration}" _duplicate)
    if(NOT _duplicate EQUAL -1)
        message(FATAL_ERROR
            "Host validation Stage11D report declaration is duplicated: ${_stage11d_report_declaration}")
    endif()
    string(SUBSTRING "${_stage11d_header_code}" ${_declaration} -1 _tail)
    string(FIND "${_tail}" ";" _semicolon)
    string(FIND "${_tail}" "{" _open)
    if(_semicolon EQUAL -1 OR (NOT _open EQUAL -1 AND _open LESS _semicolon))
        message(FATAL_ERROR
            "Host validation Stage11D report declaration is not header-only: ${_stage11d_report_declaration}")
    endif()
endforeach()

set(_input_definition_tokens
    "void inject_validation_pressed("
    "void inject_validation_action("
    "void inject_validation_movement(")
set(_navigation_definition_tokens
    "combat::MovementInput validation_route_fire_movement("
    "const combat::MonsterSnapshot* nearest_living_monster("
    "combat::MovementInput validation_movement_toward("
    "combat::Vec3 validation_door_position("
    "combat::MovementInput validation_exit_movement("
    "bool validation_attack_lane("
    "dungeon::ExitDirection validation_direction(")
foreach(_token IN LISTS _input_definition_tokens)
    string(FIND "${_input_source_text}" "${_token}" _definition)
    if(_definition EQUAL -1)
        message(FATAL_ERROR "Host validation input definition is missing: ${_token}")
    endif()
    string(FIND "${_host_text}" "${_token}" _host_definition)
    if(NOT _host_definition EQUAL -1)
        message(FATAL_ERROR "Host validation input helper remains in raylib_host.cpp: ${_token}")
    endif()
endforeach()
foreach(_token IN LISTS _navigation_definition_tokens)
    string(FIND "${_navigation_source_text}" "${_token}" _definition)
    if(_definition EQUAL -1)
        message(FATAL_ERROR "Host validation navigation definition is missing: ${_token}")
    endif()
    string(FIND "${_host_text}" "${_token}" _host_definition)
    if(NOT _host_definition EQUAL -1)
        message(FATAL_ERROR "Host validation navigation helper remains in raylib_host.cpp: ${_token}")
    endif()
endforeach()

foreach(_old_helper IN ITEMS
        "void inject_stage11b_pressed(" "void inject_stage11c_binding("
        "void inject_stage11c_movement(")
    string(FIND "${_host_text}" "${_old_helper}" _old_definition)
    if(NOT _old_definition EQUAL -1)
        message(FATAL_ERROR "Host validation boundary old helper remains: ${_old_helper}")
    endif()
endforeach()

foreach(_registered_source IN ITEMS
        "host_validation_input.cpp" "host_validation_navigation.cpp"
        "host_validation_stage10_11.cpp" "host_validation_stage11b.cpp"
        "host_validation_stage11c.cpp"
        "host_validation_stage11d_report.cpp"
        "host_validation_stage11d_runtime.cpp")
    arpg_cmake_count_arpg_raylib_source("${_raylib_cmake_text}"
        "${_registered_source}" _registered_count)
    if(NOT _registered_count EQUAL 1)
        message(FATAL_ERROR "arpg_raylib does not register ${_registered_source}")
    endif()
endforeach()

string(FIND "${_sanitized}" "while (!exit_requested) {" _loop_begin)
if(_loop_begin EQUAL -1)
    message(FATAL_ERROR "Host validation sequence guard cannot isolate host loop")
endif()
string(SUBSTRING "${_sanitized}" ${_loop_begin} -1 _loop_tail)
string(FIND "${_loop_tail}" "{" _loop_open_relative)
if(_loop_open_relative EQUAL -1)
    message(FATAL_ERROR "Host validation sequence guard cannot find host loop brace")
endif()
math(EXPR _loop_open "${_loop_begin} + ${_loop_open_relative}")
string(LENGTH "${_sanitized}" _runtime_length)
math(EXPR _runtime_last "${_runtime_length} - 1")
set(_loop_depth 0)
set(_loop_end -1)
foreach(_index RANGE ${_loop_open} ${_runtime_last})
    string(SUBSTRING "${_sanitized}" ${_index} 1 _character)
    if(_character STREQUAL "{")
        math(EXPR _loop_depth "${_loop_depth} + 1")
    elseif(_character STREQUAL "}")
        math(EXPR _loop_depth "${_loop_depth} - 1")
        if(_loop_depth EQUAL 0)
            set(_loop_end ${_index})
            break()
        endif()
    endif()
endforeach()
if(_loop_end EQUAL -1)
    message(FATAL_ERROR "Host validation sequence guard found unbalanced host loop")
endif()
math(EXPR _loop_length "${_loop_end} - ${_loop_begin} + 1")
string(SUBSTRING "${_sanitized}" ${_loop_begin} ${_loop_length} _host_loop)

function(host_token_brace_depth SURFACE POSITION OUTPUT)
    if(POSITION EQUAL 0)
        set(${OUTPUT} 0 PARENT_SCOPE)
        return()
    endif()
    string(SUBSTRING "${SURFACE}" 0 ${POSITION} _prefix)
    string(REGEX REPLACE "[^{}]" "" _braces "${_prefix}")
    string(LENGTH "${_braces}" _brace_length)
    set(_depth 0)
    if(_brace_length GREATER 0)
        math(EXPR _brace_last "${_brace_length} - 1")
        foreach(_brace_index RANGE 0 ${_brace_last})
            string(SUBSTRING "${_braces}" ${_brace_index} 1 _brace)
            if(_brace STREQUAL "{")
                math(EXPR _depth "${_depth} + 1")
            else()
                math(EXPR _depth "${_depth} - 1")
            endif()
        endforeach()
    endif()
    set(${OUTPUT} ${_depth} PARENT_SCOPE)
endfunction()

function(assert_unique_ordered_host_tokens LABEL SURFACE REQUIRED_DEPTH)
    set(_previous -1)
    foreach(_token IN ITEMS ${ARGN})
        string(FIND "${SURFACE}" "${_token}" _position)
        if(_position EQUAL -1)
            message(FATAL_ERROR "Host validation sequence guard missing ${LABEL} token: ${_token}")
        endif()
        math(EXPR _after "${_position} + 1")
        string(SUBSTRING "${SURFACE}" ${_after} -1 _remainder)
        string(FIND "${_remainder}" "${_token}" _duplicate)
        if(NOT _duplicate EQUAL -1)
            message(FATAL_ERROR "Host validation sequence guard found duplicate ${LABEL} token: ${_token}")
        endif()
        if(NOT _previous EQUAL -1 AND _position LESS _previous)
            message(FATAL_ERROR "Host validation sequence guard rejected ${LABEL} order")
        endif()
        host_token_brace_depth("${SURFACE}" ${_position} _token_depth)
        if(NOT _token_depth EQUAL REQUIRED_DEPTH)
            message(FATAL_ERROR "Host validation sequence guard rejected ${LABEL} token outside direct host scope: ${_token}")
        endif()
        set(_previous ${_position})
    endforeach()
endfunction()

assert_unique_ordered_host_tokens("input injection chain" "${_host_loop}" 1
    "const PhysicalKeySnapshot sampled_physical_keys = sample_physical_keys();"
    "inject_stage11b_physical_edges("
    "inject_stage11c_physical_edges("
    "inject_stage11d_physical_edges("
    "inject_stage17_physical_edges("
    "map_host_frame_input(")

string(FIND "${_host_loop}" "if (!step_death) {" _fixed_step_begin)
string(FIND "${_host_loop}" "runtime.fixed_tick(step_movement,"
    _fixed_step_end)
if(_fixed_step_begin EQUAL -1 OR _fixed_step_end EQUAL -1
        OR NOT _fixed_step_begin LESS _fixed_step_end)
    message(FATAL_ERROR "Host validation sequence guard cannot isolate fixed-step movement branch")
endif()
math(EXPR _fixed_step_length "${_fixed_step_end} - ${_fixed_step_begin}")
string(SUBSTRING "${_host_loop}" ${_fixed_step_begin} ${_fixed_step_length}
    _fixed_step_branch)
string(REGEX REPLACE "[ \t\r\n]+" " " _fixed_step_normalized
    "${_fixed_step_branch}")
string(REGEX MATCH
    "if \\(!step_death\\) \\{ if \\(config\\.stage11_validation != Stage11ValidationScenario::none\\) \\{ step_movement = host_validation::stage11_validation_input\\(.*\\); \\} else if \\(config\\.stage10_validation != Stage10ValidationScenario::none\\) \\{ step_movement = host_validation::stage10_validation_input\\(.*\\); \\} else \\{ step_movement = movement; \\} \\}"
    _fixed_step_priority_structure "${_fixed_step_normalized}")
if(NOT _fixed_step_priority_structure)
    message(FATAL_ERROR "Host validation sequence guard rejected fixed-step movement priority structure")
endif()

function(assert_unique_fixed_step_token TOKEN)
    string(FIND "${_fixed_step_branch}" "${TOKEN}" _position)
    if(_position EQUAL -1)
        message(FATAL_ERROR "Host validation sequence guard missing fixed-step movement token: ${TOKEN}")
    endif()
    math(EXPR _after "${_position} + 1")
    string(SUBSTRING "${_fixed_step_branch}" ${_after} -1 _remainder)
    string(FIND "${_remainder}" "${TOKEN}" _duplicate)
    if(NOT _duplicate EQUAL -1)
        message(FATAL_ERROR "Host validation sequence guard found duplicate fixed-step movement token: ${TOKEN}")
    endif()
endfunction()

foreach(_fixed_step_token IN ITEMS
        "stage11_validation_input("
        "stage10_validation_input("
        "step_movement = movement;")
    assert_unique_fixed_step_token("${_fixed_step_token}")
endforeach()

# run_raylib_host owns the frame loop and summary calls inside its single
# top-level try block, so direct execution statements are at brace depth two
# relative to the complete function block (function body plus try body).
assert_unique_ordered_host_tokens("validation summary write" "${_host_runtime}" 2
    "write_stage11b_validation_summary("
    "write_stage11c_hud_validation_summary("
    "write_stage11d_loot_validation_summary("
    "write_stage17_validation_summary(")
foreach(_summary_token IN ITEMS
        "write_stage11b_validation_summary("
        "write_stage11c_hud_validation_summary("
        "write_stage11d_loot_validation_summary("
        "write_stage17_validation_summary(")
    string(FIND "${_host_runtime}" "${_summary_token}" _summary_position)
    if(NOT _summary_position GREATER _loop_end)
        message(FATAL_ERROR
            "Host validation sequence guard rejected summary write before loop end: ${_summary_token}")
    endif()
endforeach()
