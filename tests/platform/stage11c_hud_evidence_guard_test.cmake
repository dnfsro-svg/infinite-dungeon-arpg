if(NOT DEFINED SOURCE_ROOT)
    message(FATAL_ERROR "SOURCE_ROOT is required")
endif()
include("${CMAKE_CURRENT_LIST_DIR}/../dungeon/evidence_source_scan.cmake")
set(_host "${SOURCE_ROOT}/src/platform/raylib/raylib_host.cpp")
if(DEFINED HOST_OVERRIDE)
    set(_host "${HOST_OVERRIDE}")
endif()
set(_header "${SOURCE_ROOT}/src/platform/raylib/raylib_host.hpp")
set(_input_header
    "${SOURCE_ROOT}/src/platform/raylib/host_validation_input.hpp")
set(_input_source
    "${SOURCE_ROOT}/src/platform/raylib/host_validation_input.cpp")
if(DEFINED INPUT_OVERRIDE)
    set(_input_source "${INPUT_OVERRIDE}")
endif()
set(_stage_header
    "${SOURCE_ROOT}/src/platform/raylib/host_validation_stage11c.hpp")
if(DEFINED STAGE11C_HEADER_OVERRIDE)
    set(_stage_header "${STAGE11C_HEADER_OVERRIDE}")
endif()
set(_stage_source
    "${SOURCE_ROOT}/src/platform/raylib/host_validation_stage11c.cpp")
if(DEFINED STAGE11C_SOURCE_OVERRIDE)
    set(_stage_source "${STAGE11C_SOURCE_OVERRIDE}")
endif()
set(_formal "${SOURCE_ROOT}/tests/platform/stage11c_hud_formal_game_validation.cpp")
if(DEFINED FORMAL_OVERRIDE)
    set(_formal "${FORMAL_OVERRIDE}")
endif()
set(_validator "${SOURCE_ROOT}/tests/platform/stage11c_hud_formal_validator.ps1")
set(_bad "${SOURCE_ROOT}/tests/platform/stage11c_hud_bad_formal_input.txt")
foreach(_file IN ITEMS "${_host}" "${_header}" "${_input_header}" "${_input_source}"
        "${_stage_header}" "${_stage_source}" "${_formal}" "${_validator}" "${_bad}")
    if(NOT EXISTS "${_file}")
        message(FATAL_ERROR "Stage11C HUD evidence target is missing: ${_file}")
    endif()
endforeach()
file(READ "${_host}" _host_text)

function(stage11c_count_raw_token SOURCE TOKEN OUT_COUNT)
    string(LENGTH "${SOURCE}" _source_length)
    set(_scan 0)
    set(_count 0)
    while(_scan LESS _source_length)
        string(SUBSTRING "${SOURCE}" ${_scan} -1 _tail)
        string(FIND "${_tail}" "${TOKEN}" _relative)
        if(_relative EQUAL -1)
            break()
        endif()
        math(EXPR _position "${_scan} + ${_relative}")
        math(EXPR _count "${_count} + 1")
        string(LENGTH "${TOKEN}" _token_length)
        math(EXPR _scan "${_position} + ${_token_length}")
    endwhile()
    set(${OUT_COUNT} ${_count} PARENT_SCOPE)
endfunction()

# The input starts at a code-state-verified function signature. This streams
# lexical delimiters only; it never builds a sanitized copy of the host.
function(stage11c_find_stage11c_seam_markers SOURCE OUT_ENTRIES)
    string(ASCII 10 _newline)
    string(ASCII 13 _carriage_return)
    string(ASCII 34 _double_quote)
    string(ASCII 39 _single_quote)
    string(ASCII 92 _backslash)
    string(LENGTH "${SOURCE}" _source_length)
    set(_scan 0)
    set(_state code)
    set(_brace_depth 0)
    set(_entries)
    while(_scan LESS _source_length)
        if(_state STREQUAL "code")
            set(_next -1)
            foreach(_delimiter IN ITEMS "/" "${_double_quote}" "${_single_quote}" "{" "}")
                evidence_find_cpp_from_offset("${SOURCE}" ${_scan}
                    "${_delimiter}" _candidate)
                if(NOT _candidate EQUAL -1
                        AND (_next EQUAL -1 OR _candidate LESS _next))
                    set(_next ${_candidate})
                endif()
            endforeach()
            if(_next EQUAL -1)
                break()
            endif()
            string(SUBSTRING "${SOURCE}" ${_next} 1 _character)
            if(_character STREQUAL "/")
                math(EXPR _after_slash "${_next} + 1")
                evidence_cpp_logical_next_character("${SOURCE}" ${_after_slash}
                    _next_character _next_found)
                if(_next_found AND _next_character STREQUAL "/")
                    string(SUBSTRING "${SOURCE}" ${_next} -1 _comment_tail)
                    foreach(_name IN ITEMS observation reached presented_capture)
                        foreach(_kind IN ITEMS BEGIN END)
                            set(_marker
                                "// STAGE11C_HUD_VALIDATION_SEAM_${_kind} ${_name}")
                            string(FIND "${_comment_tail}" "${_marker}" _marker_at)
                            if(_marker_at EQUAL 0)
                                list(APPEND _entries
                                    "${_name}|${_kind}|${_next}|${_brace_depth}")
                            endif()
                        endforeach()
                    endforeach()
                    set(_state line_comment)
                elseif(_next_found AND _next_character STREQUAL "*")
                    set(_state block_comment)
                endif()
            elseif(_character STREQUAL "${_double_quote}")
                set(_state string_literal)
            elseif(_character STREQUAL "{")
                math(EXPR _brace_depth "${_brace_depth} + 1")
            elseif(_character STREQUAL "}")
                math(EXPR _brace_depth "${_brace_depth} - 1")
            else()
                set(_state character_literal)
            endif()
            math(EXPR _scan "${_next} + 1")
        elseif(_state STREQUAL "line_comment")
            set(_next -1)
            foreach(_delimiter IN ITEMS "${_backslash}" "${_newline}" "${_carriage_return}")
                evidence_find_cpp_from_offset("${SOURCE}" ${_scan}
                    "${_delimiter}" _candidate)
                if(NOT _candidate EQUAL -1
                        AND (_next EQUAL -1 OR _candidate LESS _next))
                    set(_next ${_candidate})
                endif()
            endforeach()
            if(_next EQUAL -1)
                break()
            endif()
            string(SUBSTRING "${SOURCE}" ${_next} 1 _character)
            if(_character STREQUAL "${_backslash}")
                evidence_is_cpp_splice("${SOURCE}" ${_next} _is_splice)
                if(_is_splice)
                    math(EXPR _after_backslash "${_next} + 1")
                    string(SUBSTRING "${SOURCE}" ${_after_backslash} 1 _splice_character)
                    if(_splice_character STREQUAL "${_carriage_return}")
                        math(EXPR _scan "${_next} + 3")
                    else()
                        math(EXPR _scan "${_next} + 2")
                    endif()
                    continue()
                endif()
            elseif(_character STREQUAL "${_newline}"
                    OR _character STREQUAL "${_carriage_return}")
                set(_state code)
            endif()
            math(EXPR _scan "${_next} + 1")
        elseif(_state STREQUAL "block_comment")
            evidence_find_cpp_from_offset("${SOURCE}" ${_scan} "*" _next)
            if(_next EQUAL -1)
                break()
            endif()
            math(EXPR _after_star "${_next} + 1")
            evidence_cpp_logical_next_character("${SOURCE}" ${_after_star}
                _next_character _next_found)
            if(_next_found AND _next_character STREQUAL "/")
                set(_state code)
            endif()
            math(EXPR _scan "${_next} + 1")
        else()
            if(_state STREQUAL "string_literal")
                set(_quote "${_double_quote}")
            else()
                set(_quote "${_single_quote}")
            endif()
            set(_next -1)
            foreach(_delimiter IN ITEMS "${_backslash}" "${_quote}")
                evidence_find_cpp_from_offset("${SOURCE}" ${_scan}
                    "${_delimiter}" _candidate)
                if(NOT _candidate EQUAL -1
                        AND (_next EQUAL -1 OR _candidate LESS _next))
                    set(_next ${_candidate})
                endif()
            endforeach()
            if(_next EQUAL -1)
                break()
            endif()
            string(SUBSTRING "${SOURCE}" ${_next} 1 _character)
            if(_character STREQUAL "${_backslash}")
                evidence_is_cpp_splice("${SOURCE}" ${_next} _is_splice)
                if(_is_splice)
                    math(EXPR _after_backslash "${_next} + 1")
                    string(SUBSTRING "${SOURCE}" ${_after_backslash} 1 _splice_character)
                    if(_splice_character STREQUAL "${_carriage_return}")
                        math(EXPR _scan "${_next} + 3")
                    else()
                        math(EXPR _scan "${_next} + 2")
                    endif()
                else()
                    math(EXPR _scan "${_next} + 2")
                endif()
            else()
                set(_state code)
                math(EXPR _scan "${_next} + 1")
            endif()
        endif()
    endwhile()
    set(${OUT_ENTRIES} "${_entries}" PARENT_SCOPE)
endfunction()

evidence_find_cpp_code_token("${_host_text}"
    "HostExitCode run_raylib_host(" _stage11c_runtime_begin)
if(_stage11c_runtime_begin EQUAL -1)
    message(FATAL_ERROR "Stage11C evidence guard cannot bind run_raylib_host")
endif()
string(SUBSTRING "${_host_text}" ${_stage11c_runtime_begin} -1
    _stage11c_runtime_tail)
evidence_find_cpp_code_token("${_stage11c_runtime_tail}" "audio.shutdown();"
    _stage11c_runtime_shutdown_relative)
if(_stage11c_runtime_shutdown_relative EQUAL -1)
    message(FATAL_ERROR "Stage11C evidence guard cannot bind run_raylib_host")
endif()
math(EXPR _stage11c_runtime_length
    "${_stage11c_runtime_shutdown_relative} + 17")
string(SUBSTRING "${_stage11c_runtime_tail}" 0 ${_stage11c_runtime_length}
    _stage11c_runtime_crop)
stage11c_find_stage11c_seam_markers("${_stage11c_runtime_crop}"
    _stage11c_runtime_seam_markers)

function(stage11c_lookup_runtime_seam_marker NAME KIND OUT_POSITION OUT_DEPTH)
    set(_matches)
    foreach(_entry IN LISTS _stage11c_runtime_seam_markers)
        string(REPLACE "|" ";" _parts "${_entry}")
        list(GET _parts 0 _entry_name)
        list(GET _parts 1 _entry_kind)
        if(_entry_name STREQUAL "${NAME}" AND _entry_kind STREQUAL "${KIND}")
            list(APPEND _matches "${_entry}")
        endif()
    endforeach()
    list(LENGTH _matches _match_count)
    if(NOT _match_count EQUAL 1)
        message(FATAL_ERROR "Stage11C evidence guard cannot bind ${NAME} seam marker")
    endif()
    list(GET _matches 0 _match)
    string(REPLACE "|" ";" _match_parts "${_match}")
    list(GET _match_parts 2 _position)
    list(GET _match_parts 3 _depth)
    set(${OUT_POSITION} ${_position} PARENT_SCOPE)
    set(${OUT_DEPTH} ${_depth} PARENT_SCOPE)
endfunction()

function(stage11c_extract_seam NAME SCOPE_LABEL EXPECTED_DEPTH OUT)
    set(_begin "// STAGE11C_HUD_VALIDATION_SEAM_BEGIN ${NAME}")
    set(_end "// STAGE11C_HUD_VALIDATION_SEAM_END ${NAME}")
    stage11c_count_raw_token("${_host_text}" "${_begin}" _raw_begin_count)
    stage11c_count_raw_token("${_host_text}" "${_end}" _raw_end_count)
    if(NOT _raw_begin_count EQUAL 1 OR NOT _raw_end_count EQUAL 1)
        message(FATAL_ERROR "Stage11C evidence guard cannot bind ${NAME} seam marker")
    endif()
    stage11c_lookup_runtime_seam_marker("${NAME}" BEGIN _begin_at _begin_depth)
    stage11c_lookup_runtime_seam_marker("${NAME}" END _end_at _end_depth)
    if(NOT _begin_at LESS _end_at)
        message(FATAL_ERROR "Stage11C evidence guard cannot isolate ${NAME} seam")
    endif()
    if(NOT _begin_depth EQUAL _end_depth
            OR NOT _begin_depth EQUAL EXPECTED_DEPTH)
        message(FATAL_ERROR "Stage11C evidence guard rejected ${SCOPE_LABEL} seam scope: expected ${EXPECTED_DEPTH}, begin ${_begin_depth}, end ${_end_depth}")
    endif()
    string(LENGTH "${_end}" _end_length)
    math(EXPR _length "${_end_at} - ${_begin_at} + ${_end_length}")
    string(SUBSTRING "${_stage11c_runtime_crop}" ${_begin_at} ${_length} _raw)
    evidence_sanitize_cpp_for_scan("${_raw}" _code)
    set(${OUT} "${_code}" PARENT_SCOPE)
    set("_stage11c_${NAME}_begin" ${_begin_at} PARENT_SCOPE)
    set("_stage11c_${NAME}_end" ${_end_at} PARENT_SCOPE)
endfunction()
stage11c_extract_seam(observation "observation" 3 _stage11c_observation)
stage11c_extract_seam(reached "reached" 3 _stage11c_reached_seam)
stage11c_extract_seam(presented_capture "presented capture" 3
    _stage11c_presented_capture)
if(NOT _stage11c_observation_end LESS _stage11c_reached_begin
        OR NOT _stage11c_reached_end LESS _stage11c_presented_capture_begin)
    message(FATAL_ERROR "Stage11C evidence guard rejected seam marker order")
endif()
file(READ "${_header}" _header_text)
file(READ "${_input_header}" _input_header_text)
file(READ "${_input_source}" _input_source_text)
file(READ "${_stage_header}" _stage_header_text)
file(READ "${_stage_source}" _stage_source_text)
file(READ "${_formal}" _formal_text)
file(READ "${_validator}" _validator_text)
set(_combined "${_header_text}\n${_input_header_text}\n${_input_source_text}\n${_stage_header_text}\n${_stage_source_text}\n${_host_text}\n${_formal_text}")

foreach(_required IN ITEMS
        "Stage11CHudValidationScenario"
        "normal_combat" "low_health_status" "cleared_exit"
        "abyss_abandon" "level_up_points" "debug_overlay"
        "platform::run_raylib_host(config)"
        "std::system(command.c_str())"
        "stage11c_production_snapshot_hash"
        "production_snapshot_hash"
        "present_frame_and_maybe_capture"
        "font_ready" "status_tags" "notice_kinds" "notice_texts" "navigation_values")
    string(FIND "${_combined}" "${_required}" _found)
    if(_found EQUAL -1)
        message(FATAL_ERROR "Stage11C evidence guard missing required production token: ${_required}")
    endif()
endforeach()

foreach(_forbidden IN ITEMS
        "HudViewModel direct_model"
        "HudNoticeState injected_notice"
        "CombatSnapshot injected_combat"
        "TestAccess"
        "session.queue_action"
        "queue_logical_action"
        "state.progression ="
        "result=pass"
        "LoadImageFromScreen()")
    string(FIND "${_formal_text}" "${_forbidden}" _found)
    if(NOT _found EQUAL -1)
        if(_forbidden STREQUAL "HudViewModel direct_model")
            set(_reason "direct ViewModel assignment")
        elseif(_forbidden STREQUAL "state.progression =")
            set(_reason "bypassed Session progression")
        elseif(_forbidden STREQUAL "result=pass")
            set(_reason "fake result pass")
        elseif(_forbidden STREQUAL "session.queue_action" OR _forbidden STREQUAL "queue_logical_action")
            set(_reason "logical action queue")
        else()
            set(_reason "forbidden formal injection ${_forbidden}")
        endif()
        message(FATAL_ERROR "Stage11C evidence guard rejected ${_reason}")
    endif()
endforeach()

foreach(_required_input IN ITEMS
        "void inject_validation_action("
        "void inject_validation_movement("
        "settings::binding_for(settings_data, action)"
        "snapshot.down[index] = true;"
        "if (pressed) snapshot.pressed[index] = true;")
    string(FIND "${_input_source_text}" "${_required_input}" _found)
    if(_found EQUAL -1)
        message(FATAL_ERROR
            "Stage11C evidence guard rejected skipped stable binding: ${_required_input}")
    endif()
endforeach()
set(_physical_input_forbidden
    ".queue_action(" "request_descent(" "request_passive_"
    "TestAccess" "HudViewModel direct_model")
foreach(_forbidden IN LISTS _physical_input_forbidden)
    string(FIND "${_input_source_text}" "${_forbidden}" _found)
    if(NOT _found EQUAL -1)
        message(FATAL_ERROR
            "Stage11C evidence guard rejected non-physical shared input: ${_forbidden}")
    endif()
endforeach()

evidence_extract_cpp_function_block("${_stage_source_text}"
    "PhysicalKeySnapshot inject_stage11c_physical_edges(" _stage11c_driver)
evidence_extract_cpp_function_block("${_stage_source_text}"
    "std::uint64_t stage11c_production_snapshot_hash(" _stage11c_hash_function)
evidence_extract_cpp_function_block("${_stage_source_text}"
    "bool stage11c_hud_validation_reached(" _stage11c_reached_function)
evidence_extract_cpp_function_block("${_stage_source_text}"
    "void write_stage11c_hud_validation_summary(" _stage11c_summary_function)
function(stage11c_require_function_tokens LABEL SURFACE)
    foreach(_token IN LISTS ARGN)
        string(FIND "${SURFACE}" "${_token}" _token_index)
        if(_token_index EQUAL -1)
            message(FATAL_ERROR "Stage11C evidence guard missing ${LABEL} token: ${_token}")
        endif()
    endforeach()
endfunction()
function(stage11c_code_brace_depth SURFACE POSITION OUT_DEPTH)
    if(POSITION EQUAL 0)
        set(${OUT_DEPTH} 0 PARENT_SCOPE)
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
    set(${OUT_DEPTH} ${_depth} PARENT_SCOPE)
endfunction()

function(stage11c_require_unique_hash_core_token SURFACE TOKEN OUT_POSITION)
    string(FIND "${SURFACE}" "${TOKEN}" _position)
    if(_position EQUAL -1)
        message(FATAL_ERROR "Stage11C evidence guard rejected production hash core")
    endif()
    math(EXPR _after "${_position} + 1")
    string(SUBSTRING "${SURFACE}" ${_after} -1 _tail)
    string(FIND "${_tail}" "${TOKEN}" _duplicate)
    if(NOT _duplicate EQUAL -1)
        message(FATAL_ERROR "Stage11C evidence guard rejected production hash core")
    endif()
    set(${OUT_POSITION} ${_position} PARENT_SCOPE)
endfunction()

function(stage11c_matching_brace_position SURFACE OPEN_POSITION OUT_POSITION)
    string(LENGTH "${SURFACE}" _length)
    set(_depth 0)
    set(_close -1)
    while(OPEN_POSITION LESS _length)
        string(SUBSTRING "${SURFACE}" ${OPEN_POSITION} 1 _character)
        if(_character STREQUAL "{")
            math(EXPR _depth "${_depth} + 1")
        elseif(_character STREQUAL "}")
            math(EXPR _depth "${_depth} - 1")
            if(_depth EQUAL 0)
                set(_close ${OPEN_POSITION})
                break()
            endif()
        endif()
        math(EXPR OPEN_POSITION "${OPEN_POSITION} + 1")
    endwhile()
    if(_close EQUAL -1)
        message(FATAL_ERROR "Stage11C evidence guard rejected production hash core")
    endif()
    set(${OUT_POSITION} ${_close} PARENT_SCOPE)
endfunction()

function(stage11c_require_hash_core SURFACE)
    stage11c_require_unique_hash_core_token("${SURFACE}"
        "std::uint64_t hash = 1469598103934665603ULL;" _initialization)
    stage11c_require_unique_hash_core_token("${SURFACE}" "hash ^= value;" _xor)
    stage11c_require_unique_hash_core_token("${SURFACE}"
        "hash *= 1099511628211ULL;" _prime)
    set(_mix_signature "const auto mix = [&hash](std::uint64_t value) noexcept {")
    stage11c_require_unique_hash_core_token("${SURFACE}" "${_mix_signature}"
        _mix_begin)
    stage11c_require_unique_hash_core_token("${SURFACE}" "return hash;"
        _final_return)
    string(SUBSTRING "${SURFACE}" ${_mix_begin} -1 _mix_tail)
    string(FIND "${_mix_tail}" "{" _mix_open_relative)
    math(EXPR _mix_open "${_mix_begin} + ${_mix_open_relative}")
    stage11c_matching_brace_position("${SURFACE}" ${_mix_open} _mix_close)
    stage11c_code_brace_depth("${SURFACE}" ${_initialization} _initialization_depth)
    stage11c_code_brace_depth("${SURFACE}" ${_mix_open} _mix_depth)
    stage11c_code_brace_depth("${SURFACE}" ${_xor} _xor_depth)
    stage11c_code_brace_depth("${SURFACE}" ${_prime} _prime_depth)
    stage11c_code_brace_depth("${SURFACE}" ${_final_return} _return_depth)
    if(NOT _initialization LESS _mix_begin OR NOT _mix_begin LESS _xor
            OR NOT _xor LESS _prime OR NOT _prime LESS _mix_close
            OR NOT _initialization_depth EQUAL 1 OR NOT _mix_depth EQUAL 1
            OR NOT _xor_depth EQUAL 2 OR NOT _prime_depth EQUAL 2
            OR NOT _return_depth EQUAL 1)
        message(FATAL_ERROR "Stage11C evidence guard rejected production hash core")
    endif()
    set(_previous_chain_position -1)
    foreach(_token IN ITEMS
            "mix(snapshot.session_tick);" "mix(snapshot.root_seed);"
            "mix(snapshot.commit_generation);" "mix(snapshot.room_index);"
            "mix(snapshot.room_seed);" "mix(snapshot.depth);"
            "mix(snapshot.floor_room_index);"
            "mix(static_cast<std::uint64_t>(snapshot.phase));"
            "mix(snapshot.is_abyss ? 1U : 0U);"
            "mix(snapshot.abyss_exit_confirmation_armed ? 1U : 0U);"
            "mix(snapshot.remaining_targets);" "mix(snapshot.progression.level);"
            "mix(snapshot.progression.experience);"
            "mix(snapshot.progression.unspent_passive_points);")
        stage11c_require_unique_hash_core_token("${SURFACE}" "${_token}"
            _chain_position)
        stage11c_code_brace_depth("${SURFACE}" ${_chain_position} _chain_depth)
        if(NOT _chain_depth EQUAL 1
                OR NOT _previous_chain_position LESS _chain_position)
            message(FATAL_ERROR "Stage11C evidence guard rejected production hash core")
        endif()
        set(_previous_chain_position ${_chain_position})
    endforeach()
endfunction()

function(stage11c_require_unique_seam_token LABEL SURFACE TOKEN EXPECTED_DEPTH)
    string(FIND "${SURFACE}" "${TOKEN}" _position)
    if(_position EQUAL -1)
        message(FATAL_ERROR "Stage11C evidence guard rejected ${LABEL}")
    endif()
    math(EXPR _after "${_position} + 1")
    string(SUBSTRING "${SURFACE}" ${_after} -1 _tail)
    string(FIND "${_tail}" "${TOKEN}" _duplicate)
    if(NOT _duplicate EQUAL -1)
        message(FATAL_ERROR "Stage11C evidence guard rejected ${LABEL}")
    endif()
    stage11c_code_brace_depth("${SURFACE}" ${_position} _depth)
    if(NOT _depth EQUAL EXPECTED_DEPTH)
        message(FATAL_ERROR "Stage11C evidence guard rejected ${LABEL}")
    endif()
    set(_stage11c_unique_token_position ${_position} PARENT_SCOPE)
endfunction()

function(stage11c_require_ordered_seam_tokens LABEL SURFACE)
    set(_previous -1)
    foreach(_entry IN LISTS ARGN)
        string(REPLACE "|" ";" _parts "${_entry}")
        list(GET _parts 0 _token)
        list(GET _parts 1 _depth)
        stage11c_require_unique_seam_token("${LABEL}" "${SURFACE}"
            "${_token}" ${_depth})
        set(_position ${_stage11c_unique_token_position})
        if(NOT _previous EQUAL -1 AND _position LESS _previous)
            message(FATAL_ERROR "Stage11C evidence guard rejected ${LABEL}")
        endif()
        set(_previous ${_position})
    endforeach()
endfunction()

set(_stage11c_layout_capture
    "stage11c_validation_state.layout = make_hud_layout(\n                    GetScreenWidth(), GetScreenHeight(), true)")
set(_stage11c_hash_capture
    "stage11c_validation_state.production_snapshot_hash =\n                    host_validation::stage11c_production_snapshot_hash(current)")
stage11c_require_unique_seam_token("fake layout capture"
    "${_stage11c_observation}" "${_stage11c_layout_capture}" 1)
stage11c_require_ordered_seam_tokens("observation scope"
    "${_stage11c_observation}"
    "const bool stage11c_target_visible = host_validation::stage11c_hud_validation_reached(|0"
    "++stage11c_validation_state.target_presented_frames|1"
    "${_stage11c_hash_capture}|1"
    "stage11c_validation_state.model = renderer.hud_model()|1"
    "stage11c_validation_state.notices = renderer.hud_notice_view()|1"
    "${_stage11c_layout_capture}|1")
stage11c_require_unique_seam_token("reached scope"
    "${_stage11c_reached_seam}"
    "const bool stage11c_reached = stage11c_target_visible" 0)
stage11c_require_unique_seam_token("capture scope"
    "${_stage11c_presented_capture}"
    "stage11c_validation_state.captured = true;" 1)
stage11c_require_ordered_seam_tokens("capture ordering"
    "${_stage11c_presented_capture}"
    "const bool capture_succeeded =|0"
    "present_frame_and_maybe_capture(capture_path.has_value()|0"
    "++presented_frame_count|0"
    "const bool captured_stage10_frame = captured_stage10_target|0"
    "if (captured_stage10_frame && stage11c_reached) {|0"
    "stage11c_validation_state.captured = true|1")
string(FIND "${_stage11c_presented_capture}"
    "const bool captured_stage10_frame = captured_stage10_target\n                && capture_succeeded;"
    _stage11c_capture_dependency)
if(_stage11c_capture_dependency EQUAL -1)
    message(FATAL_ERROR "Stage11C evidence guard rejected capture ordering")
endif()

function(stage11c_return_positions SURFACE OUT_POSITIONS)
    string(LENGTH "${SURFACE}" _length)
    set(_scan 0)
    set(_positions)
    while(_scan LESS _length)
        string(SUBSTRING "${SURFACE}" ${_scan} -1 _tail)
        string(FIND "${_tail}" "return" _relative)
        if(_relative EQUAL -1)
            break()
        endif()
        math(EXPR _position "${_scan} + ${_relative}")
        set(_code_token TRUE)
        if(_position GREATER 0)
            math(EXPR _previous_position "${_position} - 1")
            string(SUBSTRING "${SURFACE}" ${_previous_position} 1 _previous)
            if(_previous MATCHES "[A-Za-z0-9_]")
                set(_code_token FALSE)
            endif()
        endif()
        math(EXPR _after_return "${_position} + 6")
        if(_after_return LESS _length)
            string(SUBSTRING "${SURFACE}" ${_after_return} 1 _next)
            if(_code_token AND NOT _next MATCHES "[A-Za-z0-9_]")
                list(APPEND _positions ${_position})
            endif()
        endif()
        math(EXPR _scan "${_position} + 6")
    endwhile()
    set(${OUT_POSITIONS} "${_positions}" PARENT_SCOPE)
endfunction()

function(stage11c_require_single_final_return LABEL SURFACE EXPECTED)
    stage11c_return_positions("${SURFACE}" _returns)
    list(LENGTH _returns _return_count)
    if(NOT _return_count EQUAL 1)
        message(FATAL_ERROR "Stage11C evidence guard rejected ${LABEL} return inventory")
    endif()
    list(GET _returns 0 _return_position)
    string(LENGTH "${EXPECTED}" _expected_length)
    string(SUBSTRING "${SURFACE}" ${_return_position} ${_expected_length}
        _actual_return)
    if(NOT _actual_return STREQUAL "${EXPECTED}")
        message(FATAL_ERROR "Stage11C evidence guard rejected ${LABEL} return inventory")
    endif()
    foreach(_required IN LISTS ARGN)
        string(FIND "${SURFACE}" "${_required}" _required_position)
        if(_required_position EQUAL -1 OR _return_position LESS _required_position)
            message(FATAL_ERROR "Stage11C evidence guard rejected ${LABEL} return inventory")
        endif()
    endforeach()
endfunction()

function(stage11c_require_switch_return_inventory SURFACE)
    string(FIND "${SURFACE}" "switch (scenario) {" _switch_position)
    if(_switch_position EQUAL -1)
        message(FATAL_ERROR "Stage11C evidence guard rejected reached predicate return inventory")
    endif()
    stage11c_code_brace_depth("${SURFACE}" ${_switch_position} _switch_depth)
    if(NOT _switch_depth EQUAL 1)
        message(FATAL_ERROR "Stage11C evidence guard rejected reached predicate return inventory")
    endif()
    stage11c_return_positions("${SURFACE}" _returns)
    list(LENGTH _returns _return_count)
    if(NOT _return_count EQUAL 9)
        message(FATAL_ERROR "Stage11C evidence guard rejected reached predicate return inventory")
    endif()
    list(GET _returns 0 _pre_switch_return)
    string(SUBSTRING "${SURFACE}" ${_pre_switch_return} 13 _pre_switch_value)
    if(NOT _pre_switch_value STREQUAL "return false;")
        message(FATAL_ERROR "Stage11C evidence guard rejected reached predicate return inventory")
    endif()
    math(EXPR _last_return_index "${_return_count} - 1")
    foreach(_return_index RANGE 1 ${_last_return_index})
        list(GET _returns ${_return_index} _return_position)
        if(_return_position LESS _switch_position)
            message(FATAL_ERROR "Stage11C evidence guard rejected reached predicate return inventory")
        endif()
    endforeach()
endfunction()

function(stage11c_reject_summary_returns SURFACE)
    stage11c_return_positions("${SURFACE}" _returns)
    if(_returns)
        message(FATAL_ERROR "Stage11C evidence guard rejected summary return inventory")
    endif()
endfunction()

function(stage11c_reject_hash_direct_overwrite SURFACE)
    string(FIND "${SURFACE}" "std::uint64_t hash =" _initialization)
    if(_initialization EQUAL -1)
        message(FATAL_ERROR "Stage11C evidence guard rejected production hash direct overwrite")
    endif()
    string(SUBSTRING "${SURFACE}" ${_initialization} -1 _initialization_tail)
    string(FIND "${_initialization_tail}" ";" _initialization_end_relative)
    if(_initialization_end_relative EQUAL -1)
        message(FATAL_ERROR "Stage11C evidence guard rejected production hash direct overwrite")
    endif()
    math(EXPR _initialization_end
        "${_initialization} + ${_initialization_end_relative}")
    string(LENGTH "${SURFACE}" _length)
    set(_scan 0)
    while(_scan LESS _length)
        string(SUBSTRING "${SURFACE}" ${_scan} -1 _tail)
        string(FIND "${_tail}" "hash" _relative)
        if(_relative EQUAL -1)
            break()
        endif()
        math(EXPR _position "${_scan} + ${_relative}")
        math(EXPR _after_hash "${_position} + 4")
        string(SUBSTRING "${SURFACE}" ${_after_hash} -1 _after_hash_text)
        string(REGEX MATCH "^[ \t\r\n]*=" _direct_assignment
            "${_after_hash_text}")
        string(REGEX MATCH "^[ \t\r\n]*==" _comparison
            "${_after_hash_text}")
        if(_direct_assignment AND NOT _comparison
                AND _position GREATER _initialization_end)
            message(FATAL_ERROR
                "Stage11C evidence guard rejected production hash direct overwrite")
        endif()
        math(EXPR _scan "${_after_hash}")
    endwhile()
endfunction()
stage11c_require_function_tokens("physical driver" "${_stage11c_driver}"
    "++state.injected_frames;" "nearest_living_monster(combat_snapshot)"
    "inject_validation_movement(" "inject_validation_action(")
stage11c_require_function_tokens("production hash" "${_stage11c_hash_function}"
    "mix(snapshot.session_tick);" "mix(snapshot.root_seed);"
    "mix(snapshot.progression.unspent_passive_points);"
    "mix(monster.spawn_ordinal);" "return hash;")
stage11c_require_hash_core("${_stage11c_hash_function}")
stage11c_require_function_tokens("reached predicate" "${_stage11c_reached_function}"
    "case Scenario::low_health_status:" "player.hp > 0"
    "case Scenario::cleared_exit:" "case Scenario::abyss_abandon:"
    "case Scenario::level_up_points:" "case Scenario::debug_overlay:")
stage11c_require_function_tokens("summary" "${_stage11c_summary_function}"
    "state.model.player" "player.status_tag_count"
    "state.notices.primary.kind" "state.notices.secondary.kind"
    "state.model.navigation.depth" "state.cjk_font_ready"
    "state.production_snapshot_hash" "stage11c_validation_result")
stage11c_require_single_final_return("physical driver" "${_stage11c_driver}"
    "return snapshot;"
    "++state.injected_frames;" "nearest_living_monster(combat_snapshot)"
    "inject_validation_movement(" "inject_validation_action(")
stage11c_require_single_final_return("production hash"
    "${_stage11c_hash_function}" "return hash;"
    "mix(snapshot.session_tick);" "mix(snapshot.root_seed);"
    "mix(snapshot.progression.unspent_passive_points);"
    "mix(monster.spawn_ordinal);")
stage11c_require_switch_return_inventory("${_stage11c_reached_function}")
stage11c_reject_summary_returns("${_stage11c_summary_function}")
stage11c_reject_hash_direct_overwrite("${_stage11c_hash_function}")
foreach(_forbidden IN LISTS _physical_input_forbidden)
    string(FIND "${_stage11c_driver}" "${_forbidden}" _found)
    if(NOT _found EQUAL -1)
        message(FATAL_ERROR "Stage11C evidence guard rejected non-physical scenario driver: ${_forbidden}")
    endif()
endforeach()

set(_stage11c_runtime_alias
    "host_validation::Stage11CHudValidationState& stage11c_validation_state =\n            validation_states->stage11c;")
string(REGEX MATCHALL
    "host_validation::Stage11CHudValidationState&[ \t\r\n]+stage11c_validation_state[ \t\r\n]*="
    _stage11c_runtime_aliases "${_host_text}")
list(LENGTH _stage11c_runtime_aliases _stage11c_runtime_alias_count)
if(NOT _stage11c_runtime_alias_count EQUAL 1)
    message(FATAL_ERROR "Stage11C evidence guard cannot bind actual Stage11C runtime alias")
endif()
string(FIND "${_host_text}" "${_stage11c_runtime_alias}"
    _stage11c_runtime_begin)
string(FIND "${_host_text}"
    "while (!exit_requested) {"
    _stage11c_runtime_end)
if(_stage11c_runtime_begin EQUAL -1 OR _stage11c_runtime_end EQUAL -1
        OR NOT _stage11c_runtime_begin LESS _stage11c_runtime_end)
    message(FATAL_ERROR "Stage11C evidence guard cannot bind actual Stage11C runtime alias")
endif()
math(EXPR _stage11c_runtime_length
    "${_stage11c_runtime_end} - ${_stage11c_runtime_begin}")
string(SUBSTRING "${_host_text}" ${_stage11c_runtime_begin}
    ${_stage11c_runtime_length} _stage11c_runtime)

string(FIND "${_host_text}"
    "const bool stage11c_target_visible = host_validation::stage11c_hud_validation_reached("
    _stage11c_capture_begin)
string(FIND "${_host_text}"
    "stage10_validation_captured = stage10_validation_captured"
    _stage11c_capture_end)
if(_stage11c_capture_begin EQUAL -1 OR _stage11c_capture_end EQUAL -1
        OR NOT _stage11c_capture_begin LESS _stage11c_capture_end)
    message(FATAL_ERROR "Stage11C evidence guard cannot bind production capture assignment")
endif()
math(EXPR _stage11c_capture_length
    "${_stage11c_capture_end} - ${_stage11c_capture_begin}")
string(SUBSTRING "${_host_text}" ${_stage11c_capture_begin}
    ${_stage11c_capture_length} _stage11c_capture)

string(FIND "${_host_text}"
    "write_stage11b_validation_summary(config,"
    _stage11c_summary_begin)
string(FIND "${_host_text}" "audio.shutdown();" _stage11c_summary_end)
if(_stage11c_summary_begin EQUAL -1 OR _stage11c_summary_end EQUAL -1
        OR NOT _stage11c_summary_begin LESS _stage11c_summary_end)
    message(FATAL_ERROR "Stage11C evidence guard cannot isolate validation summary")
endif()
math(EXPR _stage11c_summary_length
    "${_stage11c_summary_end} - ${_stage11c_summary_begin}")
string(SUBSTRING "${_host_text}" ${_stage11c_summary_begin}
    ${_stage11c_summary_length} _stage11c_summary)
string(FIND "${_stage11c_summary}"
    "host_validation::write_stage11c_hud_validation_summary(config,"
    _stage11c_summary_write)
if(_stage11c_summary_write EQUAL -1)
    message(FATAL_ERROR "Stage11C evidence guard missing HUD validation summary write")
endif()

set(_stage11c_host_surface
    "${_stage11c_observation}\n${_stage11c_reached_seam}\n${_stage11c_presented_capture}")
set(_stage11c_host_surface_code "${_stage11c_host_surface}")
string(REGEX MATCHALL "stage11c_validation_state\\.captured[ \t\r\n]*=[ \t\r\n]*true"
    _stage11c_captured_assignments "${_stage11c_presented_capture}")
list(LENGTH _stage11c_captured_assignments _stage11c_captured_count)
if(NOT _stage11c_captured_count EQUAL 1)
    message(FATAL_ERROR "Stage11C evidence guard rejected captured overwrite")
endif()
string(FIND "${_stage11c_presented_capture}"
    "stage11c_validation_state.captured = true;" _stage11c_captured_statement)
if(_stage11c_captured_statement EQUAL -1)
    message(FATAL_ERROR "Stage11C evidence guard rejected missing captured statement")
endif()

string(REGEX MATCHALL
    "stage11c_validation_state\\.model[ \t\r\n]*="
    _stage11c_model_assignments "${_stage11c_host_surface_code}")
list(LENGTH _stage11c_model_assignments _stage11c_model_assignment_count)
if(NOT _stage11c_model_assignment_count EQUAL 1)
    message(FATAL_ERROR "Stage11C evidence guard rejected direct model overwrite")
endif()
string(REGEX MATCH
    "stage11c_validation_state\\.model[ \t\r\n]*\\.[A-Za-z_]"
    _stage11c_model_member_overwrite "${_stage11c_host_surface_code}")
if(_stage11c_model_member_overwrite)
    message(FATAL_ERROR "Stage11C evidence guard rejected direct model overwrite")
endif()

string(REGEX MATCHALL
    "stage11c_validation_state\\.production_snapshot_hash[ \t\r\n]*="
    _stage11c_hash_assignments "${_stage11c_host_surface_code}")
list(LENGTH _stage11c_hash_assignments _stage11c_hash_assignment_count)
if(NOT _stage11c_hash_assignment_count EQUAL 1)
    message(FATAL_ERROR "Stage11C evidence guard rejected fake snapshot hash")
endif()
string(FIND "${_stage11c_host_surface_code}"
    "stage11c_validation_state.production_snapshot_hash =\n                    host_validation::stage11c_production_snapshot_hash(current);"
    _stage11c_real_hash)
if(_stage11c_real_hash EQUAL -1)
    message(FATAL_ERROR "Stage11C evidence guard rejected fake snapshot hash")
endif()
foreach(_capture_assignment IN ITEMS
        "stage11c_validation_state.notices = renderer.hud_notice_view();"
        "stage11c_validation_state.layout = make_hud_layout("
        "++stage11c_validation_state.target_presented_frames;"
        "present_frame_and_maybe_capture(capture_path.has_value()"
        "stage11c_validation_state.captured = true;")
    string(FIND "${_stage11c_host_surface_code}" "${_capture_assignment}" _capture_index)
    if(_capture_index EQUAL -1)
        message(FATAL_ERROR "Stage11C evidence guard missing capture surface token: ${_capture_assignment}")
    endif()
endforeach()
string(FIND "${_stage11c_presented_capture}"
    "const bool captured_stage10_frame = captured_stage10_target\n                && capture_succeeded;"
    _captured_stage10_frame)
if(_captured_stage10_frame EQUAL -1)
    message(FATAL_ERROR "Stage11C evidence guard rejected missing capture success")
endif()
foreach(_field_rhs IN ITEMS
        "model|renderer.hud_model();"
        "notices|renderer.hud_notice_view();"
        "layout|make_hud_layout(")
    string(REPLACE "|" ";" _field_rhs_parts "${_field_rhs}")
    list(GET _field_rhs_parts 0 _field)
    list(GET _field_rhs_parts 1 _rhs)
    string(REGEX MATCHALL
        "stage11c_validation_state\\.${_field}[ \t\r\n]*=" _assignments
        "${_stage11c_host_surface_code}")
    list(LENGTH _assignments _assignment_count)
    if(NOT _assignment_count EQUAL 1)
        message(FATAL_ERROR "Stage11C evidence guard rejected ${_field} overwrite")
    endif()
    string(FIND "${_stage11c_host_surface_code}"
        "stage11c_validation_state.${_field} = ${_rhs}" _rhs_index)
    if(_rhs_index EQUAL -1)
        message(FATAL_ERROR "Stage11C evidence guard rejected fake ${_field} capture")
    endif()
endforeach()

string(FIND "${_stage11c_summary_function}"
    "const bool stage11c_validation_result = state.captured\n            && state.cjk_font_ready && state.production_snapshot_hash != 0U;"
    _stage11c_summary_gate)
if(_stage11c_summary_gate EQUAL -1)
    message(FATAL_ERROR "Stage11C evidence guard rejected fake summary state")
endif()

string(REGEX MATCH
    "(current|previous|snapshot|state)\\.progression\\.[A-Za-z_]+[ \t\r\n]*=[^=]"
    _stage11c_progression_bypass "${_stage11c_host_surface}")
if(NOT _stage11c_progression_bypass)
    string(REGEX MATCH
        "(current|previous|snapshot|state)\\.progression[ \t\r\n]*=[^=]"
        _stage11c_progression_bypass "${_stage11c_host_surface}")
endif()
if(_stage11c_progression_bypass)
    message(FATAL_ERROR "Stage11C evidence guard rejected bypassed Session progression")
endif()

unset(_previous_order)
foreach(_ordered IN ITEMS
        "const PhysicalKeySnapshot sampled_physical_keys = sample_physical_keys();"
        "const PhysicalKeySnapshot stage11b_physical_keys ="
        "const PhysicalKeySnapshot stage11c_physical_keys = host_validation::inject_stage11c_physical_edges("
        "const PhysicalKeySnapshot physical_keys = inject_stage11d_physical_edges("
        "HostFrameInput frame_input = map_host_frame_input("
        "HostFrameGateResult host_gate"
        "submit_frame_actions(*session, frame_input)")
    string(FIND "${_host_text}" "${_ordered}" _index)
    if(_index EQUAL -1)
        message(FATAL_ERROR "Stage11C evidence guard missing production input stage: ${_ordered}")
    endif()
    if(DEFINED _previous_order AND _index LESS _previous_order)
        message(FATAL_ERROR "Stage11C evidence guard rejected physical sample-map-pause-submit order")
    endif()
    set(_previous_order ${_index})
endforeach()

string(FIND "${_host_text}" "EndDrawing();" _present)
string(FIND "${_host_text}" "Image image = LoadImageFromScreen();" _capture)
if(_present EQUAL -1 OR _capture EQUAL -1 OR NOT _present LESS _capture)
    message(FATAL_ERROR "Stage11C evidence guard rejected pre-Present capture")
endif()
string(REGEX MATCHALL "LoadImageFromScreen[ \t\n]*\\(" _loads "${_host_text}")
list(LENGTH _loads _load_count)
if(NOT _load_count EQUAL 1)
    message(FATAL_ERROR "Stage11C evidence guard requires one post-Present capture helper")
endif()
string(FIND "${_host_text}"
    "stage11c_validation_state.cjk_font_ready = hud_resources_ready;"
    _real_font)
if(_real_font EQUAL -1)
    message(FATAL_ERROR "Stage11C evidence guard rejected fake font-ready")
endif()
foreach(_required IN ITEMS
        "stage11c_hud_validation_reached("
        "stage11c_validation_state.production_snapshot_hash ="
        "stage11c_validation_state.model = renderer.hud_model();"
        "stage11c_validation_state.notices = renderer.hud_notice_view();"
        "present_frame_and_maybe_capture(capture_path.has_value()"
        "stage11c_validation_state.captured = true;")
    string(FIND "${_host_text}" "${_required}" _found)
    if(_found EQUAL -1)
        message(FATAL_ERROR "Stage11C evidence guard missing post-production evidence step: ${_required}")
    endif()
endforeach()

foreach(_required IN ITEMS
        "137,80,78,71,13,10,26,10" "1280" "720" "LastWriteTimeUtc"
        "Measure-HudTextRegion" "HollowBoxes" "notice_texts"
        "non-positive HUD rect" "snapshot hash mismatch"
        "font_ready" "production_snapshot_hash")
    string(FIND "${_validator_text}" "${_required}" _found)
    if(_found EQUAL -1)
        message(FATAL_ERROR "Stage11C evidence validator missing check: ${_required}")
    endif()
endforeach()
