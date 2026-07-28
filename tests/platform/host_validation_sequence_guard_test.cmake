if(NOT DEFINED SOURCE_ROOT)
    message(FATAL_ERROR "SOURCE_ROOT is required")
endif()

set(_host "${SOURCE_ROOT}/src/platform/raylib/raylib_host.cpp")
if(DEFINED HOST_OVERRIDE)
    set(_host "${HOST_OVERRIDE}")
endif()
if(NOT EXISTS "${_host}")
    message(FATAL_ERROR "Host validation sequence target is missing: ${_host}")
endif()

file(READ "${_host}" _host_text)

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
set(_stage11b_source
    "${SOURCE_ROOT}/src/platform/raylib/host_validation_stage11b.cpp")
set(_raylib_cmake "${SOURCE_ROOT}/src/platform/raylib/CMakeLists.txt")
foreach(_required IN ITEMS
        "${_input_header}" "${_input_source}"
        "${_navigation_header}" "${_navigation_source}"
        "${_stage10_11_header}" "${_stage10_11_source}"
        "${_stage11b_header}" "${_stage11b_source}" "${_raylib_cmake}")
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
file(READ "${_raylib_cmake}" _raylib_cmake_text)

foreach(_header_text IN ITEMS
        "${_input_header_text}" "${_navigation_header_text}"
        "${_stage10_11_header_text}")
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
if(_host_text MATCHES "struct Stage11BValidationState final")
    message(FATAL_ERROR
        "Host validation Stage11B state definition remains in raylib_host.cpp")
endif()

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
        "host_validation_stage10_11.cpp" "host_validation_stage11b.cpp")
    string(FIND "${_raylib_cmake_text}" "${_registered_source}" _registered)
    if(_registered EQUAL -1)
        message(FATAL_ERROR "arpg_raylib does not register ${_registered_source}")
    endif()
endforeach()

# Keep seam comments from satisfying a source-order assertion.
string(REGEX REPLACE "/\\*([^*]|\\*+[^*/])*\\*+/" "" _sanitized "${_host_text}")
string(REGEX REPLACE "//[^\r\n]*" "" _sanitized "${_sanitized}")

string(FIND "${_sanitized}" "while (!exit_requested) {" _loop_begin)
string(FIND "${_sanitized}" "audio.shutdown();" _loop_end)
if(_loop_begin EQUAL -1 OR _loop_end EQUAL -1 OR NOT _loop_begin LESS _loop_end)
    message(FATAL_ERROR "Host validation sequence guard cannot isolate host loop and summary")
endif()
math(EXPR _loop_length "${_loop_end} - ${_loop_begin}")
string(SUBSTRING "${_sanitized}" ${_loop_begin} ${_loop_length} _host_loop)

function(assert_unique_ordered_host_tokens LABEL)
    set(_previous -1)
    foreach(_token IN ITEMS ${ARGN})
        string(FIND "${_host_loop}" "${_token}" _position)
        if(_position EQUAL -1)
            message(FATAL_ERROR "Host validation sequence guard missing ${LABEL} token: ${_token}")
        endif()
        math(EXPR _after "${_position} + 1")
        string(SUBSTRING "${_host_loop}" ${_after} -1 _remainder)
        string(FIND "${_remainder}" "${_token}" _duplicate)
        if(NOT _duplicate EQUAL -1)
            message(FATAL_ERROR "Host validation sequence guard found duplicate ${LABEL} token: ${_token}")
        endif()
        if(NOT _previous EQUAL -1 AND _position LESS _previous)
            message(FATAL_ERROR "Host validation sequence guard rejected ${LABEL} order")
        endif()
        set(_previous ${_position})
    endforeach()
endfunction()

assert_unique_ordered_host_tokens("input injection chain"
    "sample_physical_keys()"
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

assert_unique_ordered_host_tokens("validation summary write"
    "write_stage11b_validation_summary("
    "write_stage11c_hud_validation_summary("
    "write_stage11d_loot_validation_summary("
    "write_stage17_validation_summary(")
