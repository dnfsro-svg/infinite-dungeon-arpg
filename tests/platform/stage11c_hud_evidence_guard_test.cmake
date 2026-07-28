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
    "${_stage11c_runtime}\n${_stage11c_capture}\n${_stage11c_summary}")

string(REGEX MATCHALL
    "stage11c_validation_state\\.model[ \t\r\n]*="
    _stage11c_model_assignments "${_stage11c_host_surface}")
list(LENGTH _stage11c_model_assignments _stage11c_model_assignment_count)
if(NOT _stage11c_model_assignment_count EQUAL 1)
    message(FATAL_ERROR "Stage11C evidence guard rejected direct model overwrite")
endif()
string(REGEX MATCH
    "stage11c_validation_state\\.model[ \t\r\n]*\\.[A-Za-z_]"
    _stage11c_model_member_overwrite "${_stage11c_host_surface}")
if(_stage11c_model_member_overwrite)
    message(FATAL_ERROR "Stage11C evidence guard rejected direct model overwrite")
endif()

string(REGEX MATCHALL
    "stage11c_validation_state\\.production_snapshot_hash[ \t\r\n]*="
    _stage11c_hash_assignments "${_stage11c_host_surface}")
list(LENGTH _stage11c_hash_assignments _stage11c_hash_assignment_count)
if(NOT _stage11c_hash_assignment_count EQUAL 1)
    message(FATAL_ERROR "Stage11C evidence guard rejected fake snapshot hash")
endif()
string(FIND "${_stage11c_host_surface}"
    "stage11c_validation_state.production_snapshot_hash =\n                    host_validation::stage11c_production_snapshot_hash(current);"
    _stage11c_real_hash)
if(_stage11c_real_hash EQUAL -1)
    message(FATAL_ERROR "Stage11C evidence guard rejected fake snapshot hash")
endif()

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
