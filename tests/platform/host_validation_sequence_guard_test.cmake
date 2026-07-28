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
    "if \\(!step_death\\) \\{ if \\(config\\.stage11_validation != Stage11ValidationScenario::none\\) \\{ step_movement = stage11_validation_input\\(.*\\); \\} else if \\(config\\.stage10_validation != Stage10ValidationScenario::none\\) \\{ step_movement = stage10_validation_input\\(.*\\); \\} else \\{ step_movement = movement; \\} \\}"
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
