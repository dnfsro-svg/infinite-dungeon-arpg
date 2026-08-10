cmake_minimum_required(VERSION 3.25)

if(NOT DEFINED SOURCE_ROOT OR NOT DEFINED MUTATION_ROOT)
    message(FATAL_ERROR "SOURCE_ROOT and MUTATION_ROOT are required")
endif()

set(guard "${SOURCE_ROOT}/tests/platform/task6_recovery_screen_source_test.cmake")
set(host "${SOURCE_ROOT}/src/platform/raylib/raylib_host.cpp")
file(REMOVE_RECURSE "${MUTATION_ROOT}")
file(MAKE_DIRECTORY "${MUTATION_ROOT}")

execute_process(COMMAND "${CMAKE_COMMAND}" "-DSOURCE_ROOT=${SOURCE_ROOT}"
    -P "${guard}" RESULT_VARIABLE pristine_result)
if(NOT pristine_result EQUAL 0)
    message(FATAL_ERROR "Task 6 recovery source guard rejected pristine source")
endif()

file(READ "${host}" source)
function(expect_rejected label mutated)
    set(path "${MUTATION_ROOT}/${label}.cpp")
    file(WRITE "${path}" "${mutated}")
    execute_process(COMMAND "${CMAKE_COMMAND}" "-DSOURCE_ROOT=${SOURCE_ROOT}"
        "-DHOST_SOURCE_OVERRIDE=${path}" -P "${guard}"
        RESULT_VARIABLE result)
    if(result EQUAL 0)
        message(FATAL_ERROR "Task 6 recovery source guard accepted mutation: ${label}")
    endif()
endfunction()

string(REPLACE
    "draw_recovery_screen(shared_ui_font.font(), runtime.render_status());"
    "draw_recovery_screen(Font{}, runtime.render_status());"
    unshared_font "${source}")
if(unshared_font STREQUAL source)
    message(FATAL_ERROR "Task 6 shared-font mutation anchor is missing")
endif()
expect_rejected("unshared-font" "${unshared_font}")

string(REPLACE
    "draw_recovery_screen(shared_ui_font.font(), runtime.render_status());"
    "static_cast<void>(present_frame_and_maybe_capture(nullptr));\n                draw_recovery_screen(shared_ui_font.font(), runtime.render_status());"
    present_before_draw "${source}")
if(present_before_draw STREQUAL source)
    message(FATAL_ERROR "Task 6 present-order mutation anchor is missing")
endif()
expect_rejected("present-before-draw" "${present_before_draw}")

string(REPLACE "draw_crisp_ui_text(font, text"
    "DrawTextEx(font, text" default_text "${source}")
if(default_text STREQUAL source)
    message(FATAL_ERROR "Task 6 CJK draw mutation anchor is missing")
endif()
expect_rejected("default-text" "${default_text}")

message(STATUS "Task 6 recovery source guard self-test rejected all mutations")
