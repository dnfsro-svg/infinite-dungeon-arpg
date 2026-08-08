include("${CMAKE_CURRENT_LIST_DIR}/evidence_source_scan.cmake")

foreach(required GUARD_SCRIPT VALID_FIXTURE VALIDATION_GAME_SOURCE FORMAL_SOURCE
        CAPTURE_SCRIPT FORMAL_CAPTURE_SCRIPT STRESS_SOURCE
        VALIDATION_BUILD_SOURCE VALID_HOST_HEADER
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
            -DVALIDATION_BUILD_SOURCE=${VALIDATION_BUILD_SOURCE}
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

function(expect_validation_build_injection_rejection)
    file(READ "${VALIDATION_BUILD_SOURCE}" build_source)
    string(APPEND build_source
        "\nvoid forbidden_stage10_build_fixture() { DungeonSessionTestAccess access; }\n")
    set(mutation_file
        "${CMAKE_CURRENT_BINARY_DIR}/stage10_validation_build_injected.hpp")
    file(WRITE "${mutation_file}" "${build_source}")
    execute_process(
        COMMAND "${CMAKE_COMMAND}"
            -DFIXTURE_SOURCE=${VALID_FIXTURE}
            -DVALIDATION_GAME_SOURCE=${VALIDATION_GAME_SOURCE}
            -DFORMAL_SOURCE=${FORMAL_SOURCE}
            -DCAPTURE_SCRIPT=${CAPTURE_SCRIPT}
            -DFORMAL_CAPTURE_SCRIPT=${FORMAL_CAPTURE_SCRIPT}
            -DSTRESS_SOURCE=${STRESS_SOURCE}
            -DVALIDATION_BUILD_SOURCE=${mutation_file}
            -DHOST_HEADER=${VALID_HOST_HEADER}
            -DHOST_SOURCE=${VALID_HOST_SOURCE}
            -DSTAGE_SOURCE=${VALID_STAGE_SOURCE}
            -P ${GUARD_SCRIPT}
        RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
    file(REMOVE "${mutation_file}")
    set(combined "${output}\n${error}")
    if(result EQUAL 0)
        message(FATAL_ERROR
            "validation_build_injection: mutated evidence was accepted")
    endif()
    string(FIND "${combined}"
        "Forbidden Stage 10 evidence injection: DungeonSessionTestAccess"
        reason_index)
    if(reason_index EQUAL -1)
        message(FATAL_ERROR
            "validation_build_injection: wrong rejection reason: ${combined}")
    endif()
endfunction()

expect_validation_build_injection_rejection()

function(expect_generated_mask_assignment_rejection)
    file(READ "${VALIDATION_BUILD_SOURCE}" build_source)
    string(APPEND build_source
        "\nvoid forbidden_stage10_mask_fixture() { arpg::abyss::AbyssCheckpoint active{}; active.generated_mask = 0U; }\n")
    set(mutation_file
        "${CMAKE_CURRENT_BINARY_DIR}/stage10_generated_mask_injected.hpp")
    file(WRITE "${mutation_file}" "${build_source}")
    execute_process(
        COMMAND "${CMAKE_COMMAND}"
            -DFIXTURE_SOURCE=${VALID_FIXTURE}
            -DVALIDATION_GAME_SOURCE=${VALIDATION_GAME_SOURCE}
            -DFORMAL_SOURCE=${FORMAL_SOURCE}
            -DCAPTURE_SCRIPT=${CAPTURE_SCRIPT}
            -DFORMAL_CAPTURE_SCRIPT=${FORMAL_CAPTURE_SCRIPT}
            -DSTRESS_SOURCE=${STRESS_SOURCE}
            -DVALIDATION_BUILD_SOURCE=${mutation_file}
            -DHOST_HEADER=${VALID_HOST_HEADER}
            -DHOST_SOURCE=${VALID_HOST_SOURCE}
            -DSTAGE_SOURCE=${VALID_STAGE_SOURCE}
            -P ${GUARD_SCRIPT}
        RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
    file(REMOVE "${mutation_file}")
    set(combined "${output}\n${error}")
    if(result EQUAL 0)
        message(FATAL_ERROR
            "generated_mask_assignment: mutated evidence was accepted")
    endif()
    string(FIND "${combined}"
        "Forbidden Stage 10 evidence injection: generated_mask[ \\t]*=[^=]"
        reason_index)
    if(reason_index EQUAL -1)
        message(FATAL_ERROR
            "generated_mask_assignment: wrong rejection reason: ${combined}")
    endif()
endfunction()

expect_generated_mask_assignment_rejection()

set_property(GLOBAL PROPERTY STAGE10_TASK7C_CASE_COUNT 0)
set_property(GLOBAL PROPERTY STAGE10_SEMANTIC_CASE_COUNT 0)
function(expect_task7c_host_replacement_rejection
        name old_fragment new_fragment expected)
    file(READ "${VALID_HOST_SOURCE}" source)
    assert_unique_anchor("${source}" "${old_fragment}" "${name}")
    string(REPLACE "${old_fragment}" "${new_fragment}" mutated "${source}")
    set(mutation_file
        "${CMAKE_CURRENT_BINARY_DIR}/stage10_task7c_${name}.cpp")
    file(WRITE "${mutation_file}" "${mutated}")
    expect_guard_rejection("${name}" "${mutation_file}" "${expected}")
    file(REMOVE "${mutation_file}")
    get_property(case_count GLOBAL PROPERTY STAGE10_TASK7C_CASE_COUNT)
    math(EXPR case_count "${case_count} + 1")
    set_property(GLOBAL PROPERTY STAGE10_TASK7C_CASE_COUNT ${case_count})
endfunction()

function(expect_stage10_runtime_replacement_rejection
        name old_fragment new_fragment expected)
    get_filename_component(runtime_directory "${VALID_HOST_HEADER}" DIRECTORY)
    set(runtime_source_path
        "${runtime_directory}/host_validation_runtime.cpp")
    file(READ "${runtime_source_path}" runtime_source)
    assert_unique_anchor("${runtime_source}" "${old_fragment}" "${name}")
    string(REPLACE "${old_fragment}" "${new_fragment}"
        mutated_runtime "${runtime_source}")
    set(mutation_file
        "${CMAKE_CURRENT_BINARY_DIR}/stage10_runtime_${name}.cpp")
    file(WRITE "${mutation_file}" "${mutated_runtime}")
    execute_process(
        COMMAND "${CMAKE_COMMAND}"
            -DFIXTURE_SOURCE=${VALID_FIXTURE}
            -DVALIDATION_GAME_SOURCE=${VALIDATION_GAME_SOURCE}
            -DFORMAL_SOURCE=${FORMAL_SOURCE}
            -DCAPTURE_SCRIPT=${CAPTURE_SCRIPT}
            -DFORMAL_CAPTURE_SCRIPT=${FORMAL_CAPTURE_SCRIPT}
            -DSTRESS_SOURCE=${STRESS_SOURCE}
            -DVALIDATION_BUILD_SOURCE=${VALIDATION_BUILD_SOURCE}
            -DHOST_HEADER=${VALID_HOST_HEADER}
            -DHOST_SOURCE=${VALID_HOST_SOURCE}
            -DSTAGE_SOURCE=${VALID_STAGE_SOURCE}
            -DHOST_VALIDATION_RUNTIME_SOURCE=${mutation_file}
            -P ${GUARD_SCRIPT}
        RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
    file(REMOVE "${mutation_file}")
    set(combined "${output}\n${error}")
    if(result EQUAL 0)
        message(FATAL_ERROR "${name}: mutated runtime evidence was accepted")
    endif()
    string(FIND "${combined}" "${expected}" reason_index)
    if(reason_index EQUAL -1)
        message(FATAL_ERROR
            "${name}: wrong runtime rejection reason; expected '${expected}', got: ${combined}")
    endif()
    get_property(case_count GLOBAL PROPERTY STAGE10_SEMANTIC_CASE_COUNT)
    math(EXPR case_count "${case_count} + 1")
    set_property(GLOBAL PROPERTY STAGE10_SEMANTIC_CASE_COUNT ${case_count})
endfunction()

function(expect_task7c_m21_decoy_rejection name decoy_kind)
    file(READ "${VALID_HOST_SOURCE}" source)
    set(real_callback [=[validation_runtime->observe_capture_result(
                selected_capture_owner,
                selected_validation_capture_succeeded);]=])
    set(broken_callback [=[validation_runtime->observe_capture_result(
                CaptureOwner::none,
                false);]=])
    if(decoy_kind STREQUAL "comment")
        set(decoy "/* ${real_callback} */")
    elseif(decoy_kind STREQUAL "string")
        string(REGEX REPLACE "[ \t\r\n]+" " " compact_callback
            "${real_callback}")
        set(decoy "const char* task7c_m21_decoy = \"${compact_callback}\";")
    elseif(decoy_kind STREQUAL "raw")
        set(decoy
            "const char* task7c_m21_decoy = R\"guard(${real_callback})guard\";")
    elseif(decoy_kind STREQUAL "inactive")
        set(decoy "#if 0\n${real_callback}\n#endif")
    elseif(decoy_kind STREQUAL "lambda")
        set(decoy
            "const auto task7c_m21_decoy = [&] {\n${real_callback}\n            };")
    elseif(decoy_kind STREQUAL "dead")
        set(decoy "if (false) {\n${real_callback}\n            }")
    elseif(decoy_kind STREQUAL "cross_scope")
        set(decoy "")
    else()
        message(FATAL_ERROR "${name}: unknown M21 decoy kind ${decoy_kind}")
    endif()
    assert_unique_anchor("${source}" "${real_callback}" "${name}")
    if(decoy_kind STREQUAL "cross_scope")
        string(REPLACE "${real_callback}" "${broken_callback}" mutated
            "${source}")
        string(APPEND mutated
            "\nvoid task7c_m21_cross_scope_decoy() {\n${real_callback}\n}\n")
    else()
        string(REPLACE "${real_callback}"
            "${broken_callback}\n            ${decoy}" mutated "${source}")
    endif()
    set(mutation_file
        "${CMAKE_CURRENT_BINARY_DIR}/stage10_task7c_${name}.cpp")
    file(WRITE "${mutation_file}" "${mutated}")
    expect_guard_rejection("${name}" "${mutation_file}" "T7C-M21")
    file(REMOVE "${mutation_file}")
    get_property(case_count GLOBAL PROPERTY STAGE10_TASK7C_CASE_COUNT)
    math(EXPR case_count "${case_count} + 1")
    set_property(GLOBAL PROPERTY STAGE10_TASK7C_CASE_COUNT ${case_count})
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
            -DVALIDATION_BUILD_SOURCE=${VALIDATION_BUILD_SOURCE}
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

# The Stage10 facade guard also pressure-tests the real runtime owner. These
# cases are separate from the fixed M15/M17/M18/M21/M23 inventory below.
set(stage10_runtime_branch [=[if (impl_->config->stage10_validation
            == Stage10ValidationScenario::chaos_expansion) {
        if (stage10_target_visible) {
            ++impl_->states.stage10.chaos_presented_frames;
        } else {
            impl_->states.stage10.chaos_presented_frames = 0U;
        }
    }

    const bool stage10_reached = stage10_target_visible
        && (impl_->config->stage10_validation
                != Stage10ValidationScenario::chaos_expansion
            || impl_->states.stage10.chaos_presented_frames >= 16U);]=])
expect_stage10_runtime_replacement_rejection(stage10_target_fabricated
    "host_validation::stage10_validation_reached(\n            snapshot, *impl_->config, impl_->states.stage10);"
    "false;"
    "T7C Stage10 target owner contract")
expect_stage10_runtime_replacement_rejection(
    stage10_chaos_condition_discarded "${stage10_runtime_branch}"
    [=[if (impl_->config->stage10_validation
            == Stage10ValidationScenario::chaos_expansion) {
        ++impl_->states.stage10.chaos_presented_frames;
    }

    const bool stage10_reached = stage10_target_visible
        && (impl_->config->stage10_validation
                != Stage10ValidationScenario::chaos_expansion
            || impl_->states.stage10.chaos_presented_frames >= 16U);]=]
    "T7C Stage10 chaos branch contract")
expect_stage10_runtime_replacement_rejection(stage10_chaos_threshold_changed
    "impl_->states.stage10.chaos_presented_frames >= 16U"
    "impl_->states.stage10.chaos_presented_frames >= 15U"
    "T7C Stage10 chaos branch contract")
expect_stage10_runtime_replacement_rejection(stage10_completion_discarded
    "stage10_reached || stage11_reached"
    "stage11_reached"
    "T7C Stage10 decision completion contract")
expect_stage10_runtime_replacement_rejection(stage10_default_decision_return
    "return decision;" "return {};"
    "T7C Stage10 decision completion contract")
string(REGEX REPLACE "[ \t\r\n]+" " " stage10_runtime_branch_compact
    "${stage10_runtime_branch}")
foreach(stage10_decoy_kind IN ITEMS comment string raw inactive lambda dead)
    if(stage10_decoy_kind STREQUAL "comment")
        set(stage10_runtime_decoy "/* ${stage10_runtime_branch} */")
    elseif(stage10_decoy_kind STREQUAL "string")
        set(stage10_runtime_decoy
            "constexpr const char* stage10_decoy = \"${stage10_runtime_branch_compact}\";")
    elseif(stage10_decoy_kind STREQUAL "raw")
        set(stage10_runtime_decoy
            "constexpr const char* stage10_decoy = R\"guard(${stage10_runtime_branch})guard\";")
    elseif(stage10_decoy_kind STREQUAL "inactive")
        set(stage10_runtime_decoy
            "#if 0\n${stage10_runtime_branch}\n#endif")
    elseif(stage10_decoy_kind STREQUAL "lambda")
        set(stage10_runtime_decoy
            "const auto stage10_decoy = [&] {\n${stage10_runtime_branch}\n    };")
    else()
        set(stage10_runtime_decoy
            "if (false) {\n${stage10_runtime_branch}\n    }")
    endif()
    expect_stage10_runtime_replacement_rejection(
        "stage10_${stage10_decoy_kind}_decoy"
        "${stage10_runtime_branch}" "${stage10_runtime_decoy}"
        "T7C Stage10 chaos branch contract")
endforeach()

# Task 7C Stage10 inventory: M15 protects both occupied-path polarity and the
# conjunction itself; M17 protects both omission and inversion of the manual
# override failure; M18 is the post-EndDrawing callback boundary; M21 covers
# owner and success fabrication plus every harmless-decoy category; M23 keeps
# current-frame generic success in the clean-exit predicate.
set(m15_condition [=[if (!capture_path.has_value()
                    && decision.capture_owner
                        == CaptureOwner::generic_validation) {]=])
expect_task7c_host_replacement_rejection(m15_missing_occupied_guard
    "${m15_condition}"
    [=[if (decision.capture_owner == CaptureOwner::generic_validation) {]=]
    "T7C-M15")
expect_task7c_host_replacement_rejection(m15_inverted_occupied_guard
    "${m15_condition}"
    [=[if (capture_path.has_value()
                    && decision.capture_owner
                        == CaptureOwner::generic_validation) {]=]
    "T7C-M15")
expect_task7c_host_replacement_rejection(m15_or_occupied_guard
    "${m15_condition}"
    [=[if (!capture_path.has_value()
                    || decision.capture_owner
                        == CaptureOwner::generic_validation) {]=]
    "T7C-M15")
set(m15_generic_block [=[            if (!capture_path.has_value()
                    && decision.capture_owner
                        == CaptureOwner::generic_validation) {
                capture_path = config.validation_capture_file->string();
                selected_capture_owner = CaptureOwner::generic_validation;
                generic_capture_path_selected = true;
            }
]=])
set(m15_stage12_anchor
    "            if (!capture_path.has_value() && stage12_item_baseline_frame) {")
file(READ "${VALID_HOST_SOURCE}" m15_reorder_source)
assert_unique_anchor("${m15_reorder_source}" "${m15_generic_block}"
    "m15_before_stage12")
assert_unique_anchor("${m15_reorder_source}" "${m15_stage12_anchor}"
    "m15_before_stage12")
string(REPLACE "${m15_generic_block}" "" m15_reorder_source
    "${m15_reorder_source}")
string(REPLACE "${m15_stage12_anchor}"
    "${m15_generic_block}${m15_stage12_anchor}" m15_reorder_source
    "${m15_reorder_source}")
set(m15_reorder_file
    "${CMAKE_CURRENT_BINARY_DIR}/stage10_task7c_m15_before_stage12.cpp")
file(WRITE "${m15_reorder_file}" "${m15_reorder_source}")
expect_guard_rejection(m15_before_stage12 "${m15_reorder_file}"
    "T7C-M15")
file(REMOVE "${m15_reorder_file}")
get_property(stage10_task7c_case_count GLOBAL PROPERTY
    STAGE10_TASK7C_CASE_COUNT)
math(EXPR stage10_task7c_case_count "${stage10_task7c_case_count} + 1")
set_property(GLOBAL PROPERTY STAGE10_TASK7C_CASE_COUNT
    ${stage10_task7c_case_count})

expect_task7c_host_replacement_rejection(m17_manual_reset_missing
    "                generic_capture_path_selected = false;\n"
    ""
    "T7C-M17")
expect_task7c_host_replacement_rejection(m17_manual_reset_inverted
    "                generic_capture_path_selected = false;"
    "                generic_capture_path_selected = true;"
    "T7C-M17")
expect_task7c_host_replacement_rejection(m17_manual_override_ignored
    "selected_capture_owner == CaptureOwner::generic_validation\n                && generic_capture_path_selected && capture_succeeded"
    "selected_capture_owner == CaptureOwner::generic_validation\n                && capture_succeeded"
    "T7C-M17")

set(m18_callback [=[            validation_runtime->observe_capture_result(
                selected_capture_owner,
                selected_validation_capture_succeeded);
]=])
set(m18_capture_anchor
    "            const bool capture_succeeded =\n")
file(READ "${VALID_HOST_SOURCE}" m18_source)
assert_unique_anchor("${m18_source}" "${m18_callback}" "m18_before_end_drawing")
assert_unique_anchor("${m18_source}" "${m18_capture_anchor}"
    "m18_before_end_drawing")
string(REPLACE "${m18_callback}" "" m18_source "${m18_source}")
string(REPLACE "${m18_capture_anchor}"
    "${m18_callback}${m18_capture_anchor}" m18_source "${m18_source}")
set(m18_file "${CMAKE_CURRENT_BINARY_DIR}/stage10_task7c_m18.cpp")
file(WRITE "${m18_file}" "${m18_source}")
expect_guard_rejection(m18_before_end_drawing "${m18_file}"
    "T7C-M18")
file(REMOVE "${m18_file}")
get_property(stage10_task7c_case_count GLOBAL PROPERTY
    STAGE10_TASK7C_CASE_COUNT)
math(EXPR stage10_task7c_case_count "${stage10_task7c_case_count} + 1")
set_property(GLOBAL PROPERTY STAGE10_TASK7C_CASE_COUNT
    ${stage10_task7c_case_count})

foreach(m21_pair IN ITEMS
        "selected_capture_owner|CaptureOwner::generic_validation|owner_generic"
        "selected_capture_owner|CaptureOwner::none|owner_none"
        "selected_capture_owner|decision.capture_owner|owner_decision"
        "selected_validation_capture_succeeded|true|success_true"
        "selected_validation_capture_succeeded|false|success_false"
        "selected_validation_capture_succeeded|capture_succeeded|success_raw")
    string(REPLACE "|" ";" m21_parts "${m21_pair}")
    list(GET m21_parts 0 m21_before)
    list(GET m21_parts 1 m21_after)
    list(GET m21_parts 2 m21_name)
    set(m21_callback_before
        "validation_runtime->observe_capture_result(\n                selected_capture_owner,\n                selected_validation_capture_succeeded);")
    string(REPLACE "${m21_before}" "${m21_after}"
        m21_callback_after "${m21_callback_before}")
    expect_task7c_host_replacement_rejection("m21_${m21_name}"
        "${m21_callback_before}" "${m21_callback_after}"
        "T7C-M21")
endforeach()
set(m21_selected_owner [=[CaptureOwner selected_capture_owner =
                decision.capture_owner == CaptureOwner::stage17
                ? CaptureOwner::stage17 : CaptureOwner::none;]=])
expect_task7c_host_replacement_rejection(m21_owner_producer_constant
    "${m21_selected_owner}"
    "CaptureOwner selected_capture_owner = CaptureOwner::none;"
    "T7C-M21")
set(m21_selected_success [=[const bool selected_validation_capture_succeeded =
                selected_capture_owner == CaptureOwner::stage17
                ? capture_succeeded
                : selected_generic_capture_succeeded_now;]=])
expect_task7c_host_replacement_rejection(m21_success_producer_true
    "${m21_selected_success}"
    "const bool selected_validation_capture_succeeded = true;"
    "T7C-M21")
expect_task7c_host_replacement_rejection(m21_success_producer_false
    "${m21_selected_success}"
    "const bool selected_validation_capture_succeeded = false;"
    "T7C-M21")
foreach(m21_decoy_kind IN ITEMS comment string raw inactive lambda dead cross_scope)
    expect_task7c_m21_decoy_rejection("m21_${m21_decoy_kind}_decoy"
        "${m21_decoy_kind}")
endforeach()

set(m23_effective [=[const bool effective_generic_complete =
                decision.generic_capture_complete
                || selected_generic_capture_succeeded_now;]=])
expect_task7c_host_replacement_rejection(m23_current_frame_success_omitted
    "${m23_effective}"
    [=[const bool effective_generic_complete =
                decision.generic_capture_complete;]=]
    "T7C-M23")

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

get_property(stage10_task7c_case_count GLOBAL PROPERTY
    STAGE10_TASK7C_CASE_COUNT)
if(NOT stage10_task7c_case_count EQUAL 25)
    message(FATAL_ERROR
        "Stage 10 Task7C mutation inventory drifted: expected 25, got ${stage10_task7c_case_count}")
endif()
get_property(stage10_semantic_case_count GLOBAL PROPERTY
    STAGE10_SEMANTIC_CASE_COUNT)
if(NOT stage10_semantic_case_count EQUAL 11)
    message(FATAL_ERROR
        "Stage 10 semantic mutation inventory drifted: expected 11, got ${stage10_semantic_case_count}")
endif()
message(STATUS
    "Stage 10 guard mutation self-test passed (Task7C cases=25: M15=4, M17=3, M18=1, M21=16, M23=1; semantic hardening=11)")
