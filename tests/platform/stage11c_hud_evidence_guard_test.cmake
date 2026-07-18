if(NOT DEFINED SOURCE_ROOT)
    message(FATAL_ERROR "SOURCE_ROOT is required")
endif()
set(_host "${SOURCE_ROOT}/src/platform/raylib/raylib_host.cpp")
if(DEFINED HOST_OVERRIDE)
    set(_host "${HOST_OVERRIDE}")
endif()
set(_header "${SOURCE_ROOT}/src/platform/raylib/raylib_host.hpp")
set(_formal "${SOURCE_ROOT}/tests/platform/stage11c_hud_formal_game_validation.cpp")
if(DEFINED FORMAL_OVERRIDE)
    set(_formal "${FORMAL_OVERRIDE}")
endif()
set(_validator "${SOURCE_ROOT}/tests/platform/stage11c_hud_formal_validator.ps1")
set(_bad "${SOURCE_ROOT}/tests/platform/stage11c_hud_bad_formal_input.txt")
foreach(_file IN ITEMS "${_host}" "${_header}" "${_formal}" "${_validator}" "${_bad}")
    if(NOT EXISTS "${_file}")
        message(FATAL_ERROR "Stage11C HUD evidence target is missing: ${_file}")
    endif()
endforeach()
file(READ "${_host}" _host_text)
file(READ "${_header}" _header_text)
file(READ "${_formal}" _formal_text)
file(READ "${_validator}" _validator_text)
set(_combined "${_header_text}\n${_host_text}\n${_formal_text}")

foreach(_required IN ITEMS
        "Stage11CHudValidationScenario"
        "normal_combat" "low_health_status" "cleared_exit"
        "abyss_abandon" "level_up_points" "debug_overlay"
        "platform::run_raylib_host(config)"
        "std::system(command.c_str())"
        "stage11c_production_snapshot_hash"
        "production_snapshot_hash"
        "present_frame_and_maybe_capture"
        "font_ready" "status_tags" "notice_kinds" "navigation_values")
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

string(FIND "${_host_text}" "void inject_stage11c_binding" _stage11c_begin)
string(FIND "${_host_text}" "combat::MovementInput stage10_validation_input" _stage11c_end)
if(_stage11c_begin EQUAL -1 OR _stage11c_end EQUAL -1 OR NOT _stage11c_begin LESS _stage11c_end)
    message(FATAL_ERROR "Stage11C evidence guard cannot isolate physical scenario driver")
endif()
math(EXPR _stage11c_length "${_stage11c_end} - ${_stage11c_begin}")
string(SUBSTRING "${_host_text}" ${_stage11c_begin} ${_stage11c_length} _stage11c_driver)
foreach(_forbidden IN ITEMS ".queue_action(" "request_descent(" "request_passive_" "TestAccess" "HudViewModel direct_model")
    string(FIND "${_stage11c_driver}" "${_forbidden}" _found)
    if(NOT _found EQUAL -1)
        message(FATAL_ERROR "Stage11C evidence guard rejected non-physical scenario driver: ${_forbidden}")
    endif()
endforeach()

unset(_previous_order)
foreach(_ordered IN ITEMS
        "const PhysicalKeySnapshot sampled_physical_keys = sample_physical_keys();"
        "const PhysicalKeySnapshot physical_keys = inject_stage11c_physical_edges("
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
        "GetPixel" "font_ready" "production_snapshot_hash")
    string(FIND "${_validator_text}" "${_required}" _found)
    if(_found EQUAL -1)
        message(FATAL_ERROR "Stage11C evidence validator missing check: ${_required}")
    endif()
endforeach()
