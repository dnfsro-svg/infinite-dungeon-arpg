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

assert_unique_ordered_host_tokens("fixed-step movement priority"
    "stage11_validation_input("
    "stage10_validation_input("
    "step_movement = movement;")

assert_unique_ordered_host_tokens("validation summary write"
    "write_stage11b_validation_summary("
    "write_stage11c_hud_validation_summary("
    "write_stage11d_loot_validation_summary("
    "write_stage17_validation_summary(")
