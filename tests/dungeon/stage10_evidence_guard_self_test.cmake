include("${CMAKE_CURRENT_LIST_DIR}/evidence_source_scan.cmake")

foreach(required GUARD_SCRIPT VALID_FIXTURE VALIDATION_GAME_SOURCE FORMAL_SOURCE
        CAPTURE_SCRIPT FORMAL_CAPTURE_SCRIPT STRESS_SOURCE VALID_HOST_HEADER
        VALID_HOST_SOURCE BAD_CAPTURE_ORDER BAD_PRE_CAPTURE_DUMMY)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "Stage 10 guard self-test missing ${required}")
    endif()
endforeach()

get_filename_component(_stage_directory "${VALID_HOST_HEADER}" DIRECTORY)
set(VALID_STAGE_SOURCE "${_stage_directory}/host_validation_stage10_11.cpp")
if(NOT EXISTS "${VALID_STAGE_SOURCE}")
    message(FATAL_ERROR "Stage 10 guard self-test missing production stage source")
endif()

function(expect_guard_rejection name host expected)
    execute_process(
        COMMAND "${CMAKE_COMMAND}"
            -DFIXTURE_SOURCE=${VALID_FIXTURE}
            -DVALIDATION_GAME_SOURCE=${VALIDATION_GAME_SOURCE}
            -DFORMAL_SOURCE=${FORMAL_SOURCE}
            -DCAPTURE_SCRIPT=${CAPTURE_SCRIPT}
            -DFORMAL_CAPTURE_SCRIPT=${FORMAL_CAPTURE_SCRIPT}
            -DSTRESS_SOURCE=${STRESS_SOURCE}
            -DHOST_HEADER=${VALID_HOST_HEADER}
            -DHOST_SOURCE=${host}
            -DSTAGE_SOURCE=${VALID_STAGE_SOURCE}
            -P ${GUARD_SCRIPT}
        RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
    set(combined "${output}\n${error}")
    if(result EQUAL 0)
        message(FATAL_ERROR "${name}: mutated evidence was accepted")
    endif()
    string(FIND "${combined}" "${expected}" reason_index)
    if(reason_index EQUAL -1)
        message(FATAL_ERROR
            "${name}: wrong rejection reason; expected '${expected}', got: ${combined}")
    endif()
endfunction()

function(assert_unique_anchor source anchor name)
    set(remaining "${source}")
    set(match_count 0)
    string(LENGTH "${anchor}" anchor_length)
    while(TRUE)
        string(FIND "${remaining}" "${anchor}" anchor_index)
        if(anchor_index EQUAL -1)
            break()
        endif()
        math(EXPR match_count "${match_count} + 1")
        math(EXPR next_index "${anchor_index} + ${anchor_length}")
        string(SUBSTRING "${remaining}" ${next_index} -1 remaining)
    endwhile()
    if(NOT match_count EQUAL 1)
        message(FATAL_ERROR
            "${name}: expected one replacement anchor, found ${match_count}")
    endif()
endfunction()

function(expect_stage_route_rejection name token inject_brace_noise decoy_kind)
    file(READ "${VALID_STAGE_SOURCE}" stage_source)
    evidence_find_cpp_function_bounds("${stage_source}"
        "combat::MovementInput stage10_validation_input("
        stage10_begin stage10_open stage10_end)
    math(EXPR stage10_input_length "${stage10_end} - ${stage10_begin} + 1")
    string(SUBSTRING "${stage_source}" ${stage10_begin} ${stage10_input_length}
        stage10_input)
    string(REPLACE "${token}" "" mutated_input "${stage10_input}")
    if(mutated_input STREQUAL stage10_input)
        message(FATAL_ERROR "${name}: mutation token was not found")
    endif()
    set(injected_noise "")
    if(inject_brace_noise)
        string(APPEND injected_noise
            "\n    // { line-comment brace\n    /* { block-comment brace } */\n    const char* evidence_brace_string = \"{\\\"}\";\n    const char evidence_brace_character = '{';\n")
    endif()
    string(ASCII 92 splice_backslash)
    if(decoy_kind STREQUAL "continued_line")
        string(APPEND injected_noise
            "\n    // decoy follows ${splice_backslash}\n    ${token};\n")
    elseif(decoy_kind STREQUAL "spliced_slashes")
        string(APPEND injected_noise
            "\n    /${splice_backslash}\n/ ${token};\n")
    elseif(NOT decoy_kind STREQUAL "none")
        message(FATAL_ERROR "${name}: unknown decoy kind ${decoy_kind}")
    endif()
    if(NOT injected_noise STREQUAL "")
        math(EXPR stage10_open_after "${stage10_open} - ${stage10_begin} + 1")
        string(SUBSTRING "${mutated_input}" 0 ${stage10_open_after}
            input_prefix)
        string(SUBSTRING "${mutated_input}" ${stage10_open_after} -1
            input_suffix)
        set(mutated_input "${input_prefix}${injected_noise}${input_suffix}")
    endif()
    string(REPLACE "${stage10_input}" "${mutated_input}" mutated_source
        "${stage_source}")
    set(mutation_file "${CMAKE_CURRENT_BINARY_DIR}/stage10_${name}.cpp")
    file(WRITE "${mutation_file}" "${mutated_source}")
    execute_process(
        COMMAND "${CMAKE_COMMAND}"
            -DFIXTURE_SOURCE=${VALID_FIXTURE}
            -DVALIDATION_GAME_SOURCE=${VALIDATION_GAME_SOURCE}
            -DFORMAL_SOURCE=${FORMAL_SOURCE}
            -DCAPTURE_SCRIPT=${CAPTURE_SCRIPT}
            -DFORMAL_CAPTURE_SCRIPT=${FORMAL_CAPTURE_SCRIPT}
            -DSTRESS_SOURCE=${STRESS_SOURCE}
            -DHOST_HEADER=${VALID_HOST_HEADER}
            -DHOST_SOURCE=${VALID_HOST_SOURCE}
            -DSTAGE_SOURCE=${mutation_file}
            -P ${GUARD_SCRIPT}
        RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
    file(REMOVE "${mutation_file}")
    set(combined "${output}\n${error}")
    if(result EQUAL 0)
        message(FATAL_ERROR "${name}: stage route mutation was accepted")
    endif()
    string(FIND "${combined}" "Stage 10 validation input lacks production route"
        reason_index)
    if(reason_index EQUAL -1)
        message(FATAL_ERROR
            "${name}: missing Stage 10 route rejection, got: ${combined}")
    endif()
    string(FIND "${combined}" "${token}" token_index)
    if(token_index EQUAL -1)
        message(FATAL_ERROR
            "${name}: missing rejected route token '${token}', got: ${combined}")
    endif()
endfunction()

file(READ "${VALID_HOST_SOURCE}" valid_host_source)
set(helper_order_anchor
    "    EndDrawing();\n    if (path == nullptr) return true;\n    Image image = LoadImageFromScreen();")
set(helper_order_mutation
    "    Image image = LoadImageFromScreen();\n    if (path == nullptr) return true;\n    EndDrawing();")
assert_unique_anchor("${valid_host_source}" "${helper_order_anchor}"
    "capture_helper_order")
string(REPLACE "${helper_order_anchor}" "${helper_order_mutation}"
    helper_order_source "${valid_host_source}")
if(helper_order_source STREQUAL valid_host_source)
    message(FATAL_ERROR "capture_helper_order: replacement did not occur")
endif()
set(helper_order_file
    "${CMAKE_CURRENT_BINARY_DIR}/stage10_capture_helper_order.cpp")
file(WRITE "${helper_order_file}" "${helper_order_source}")
expect_guard_rejection(capture_helper_order "${helper_order_file}"
    "Stage 10 capture helper must call EndDrawing before LoadImageFromScreen")
file(REMOVE "${helper_order_file}")

set(capture_call_anchor
    "            const bool capture_succeeded =\n                present_frame_and_maybe_capture(capture_path.has_value()\n                    ? capture_path->c_str() : nullptr);")
set(pre_present_capture
    "            Image pre_present_image = LoadImageFromScreen();\n            static_cast<void>(ExportImage(pre_present_image,\n                \"stage10-duplicate-before-present.png\"));\n            UnloadImage(pre_present_image);\n")
assert_unique_anchor("${valid_host_source}" "${capture_call_anchor}"
    "capture_duplicate_before_present")
string(REPLACE "${capture_call_anchor}"
    "${pre_present_capture}${capture_call_anchor}"
    duplicate_capture_source "${valid_host_source}")
if(duplicate_capture_source STREQUAL valid_host_source)
    message(FATAL_ERROR "capture_duplicate_before_present: replacement did not occur")
endif()
set(duplicate_capture_file
    "${CMAKE_CURRENT_BINARY_DIR}/stage10_capture_duplicate_before_present.cpp")
file(WRITE "${duplicate_capture_file}" "${duplicate_capture_source}")
expect_guard_rejection(capture_duplicate_before_present "${duplicate_capture_file}"
    "Stage 10 capture/order must contain exactly one EndDrawing")
file(REMOVE "${duplicate_capture_file}")
expect_stage_route_rejection(missing_descent "session.request_descent(true)" FALSE none)
expect_stage_route_rejection(missing_queue_action
    "session.queue_action(combat::Action::light)" FALSE none)
expect_stage_route_rejection(missing_descent_with_brace_noise
    "session.request_descent(true)" TRUE none)
expect_stage_route_rejection(missing_queue_action_with_brace_noise
    "session.queue_action(combat::Action::light)" TRUE none)
expect_stage_route_rejection(missing_descent_with_continued_line_comment
    "session.request_descent(true)" FALSE continued_line)
expect_stage_route_rejection(missing_queue_with_continued_line_comment
    "session.queue_action(combat::Action::light)" FALSE continued_line)
expect_stage_route_rejection(missing_descent_with_spliced_slashes
    "session.request_descent(true)" FALSE spliced_slashes)
expect_stage_route_rejection(missing_queue_with_spliced_slashes
    "session.queue_action(combat::Action::light)" FALSE spliced_slashes)

message(STATUS "Stage 10 guard mutation self-test passed")
