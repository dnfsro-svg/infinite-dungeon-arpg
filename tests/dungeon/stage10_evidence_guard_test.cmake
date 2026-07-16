foreach(required FIXTURE_SOURCE VALIDATION_GAME_SOURCE FORMAL_SOURCE
        CAPTURE_SCRIPT FORMAL_CAPTURE_SCRIPT STRESS_SOURCE HOST_HEADER HOST_SOURCE)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "Stage 10 evidence guard missing ${required}")
    endif()
endforeach()

file(READ "${FIXTURE_SOURCE}" fixture_source)
file(READ "${VALIDATION_GAME_SOURCE}" validation_game_source)
file(READ "${FORMAL_SOURCE}" formal_source)
file(READ "${CAPTURE_SCRIPT}" capture_script)
file(READ "${FORMAL_CAPTURE_SCRIPT}" formal_capture_script)
file(READ "${STRESS_SOURCE}" stress_source)
file(READ "${HOST_HEADER}" host_header)
file(READ "${HOST_SOURCE}" host_source)
set(formal_evidence "${fixture_source}\n${validation_game_source}\n${formal_source}\n${capture_script}\n${formal_capture_script}\n${host_source}")

foreach(forbidden
        "DungeonSessionTestAccess"
        "CombatWorldTestAccess"
        "force_defeat"
        "relay_defeated"
        "defeat_monster\\("
        "set_phase\\("
        "stable_state_"
        "phase_[ \\t]*="
        "generated_mask[ \\t]*="
        "claimed_mask[ \\t]*="
        "abandoned_mask[ \\t]*=")
    if(formal_evidence MATCHES "${forbidden}")
        message(FATAL_ERROR "Forbidden Stage 10 evidence injection: ${forbidden}")
    endif()
endforeach()

if(NOT fixture_source MATCHES "SaveStore"
        OR NOT fixture_source MATCHES "request_pickup"
        OR NOT fixture_source MATCHES "abyss_exit_warning"
        OR NOT fixture_source MATCHES "abyss_exit_confirmation_armed"
        OR NOT fixture_source MATCHES "MovementInput")
    message(FATAL_ERROR "Stage 10 fixture must use real save, pickup, warning, confirmation, and movement APIs")
endif()
if(NOT validation_game_source MATCHES "run_raylib_host"
        OR NOT formal_source MATCHES "make_door_transition"
        OR NOT formal_source MATCHES "run_raylib_host"
        OR NOT host_source MATCHES "queue_action"
        OR NOT host_source MATCHES "request_descent"
        OR NOT host_source MATCHES "reset_current_room")
    message(FATAL_ERROR "Formal Stage 10 evidence must use generation and normal host inputs")
endif()

if(stress_source MATCHES "make_full_ground_pool"
        OR stress_source MATCHES "std::array<[^>]*GroundItem")
    message(FATAL_ERROR "Stage 10 stress may not substitute a local GroundItem array for the production session pool")
endif()
if(NOT stress_source MATCHES "DungeonSession"
        OR NOT stress_source MATCHES "fill_ground_pool"
        OR NOT stress_source MATCHES "ground_items\\(session\\)"
        OR NOT stress_source MATCHES "session.tick\\("
        OR NOT stress_source MATCHES "ground_saturation_count"
        OR NOT stress_source MATCHES "production_resolution"
        OR NOT stress_source MATCHES "last_abyss_resolution"
        OR NOT stress_source MATCHES "encode_checkpoint"
        OR NOT stress_source MATCHES "generate_long_trace\\(\\*first, 0U\\)"
        OR NOT stress_source MATCHES "generate_long_trace\\(\\*second, 37U\\)")
    message(FATAL_ERROR "Stage 10 stress must exercise production session ground, resolution, and restarted traces")
endif()

string(ASCII 9 whitespace_tab)
string(ASCII 10 whitespace_lf)
string(ASCII 13 whitespace_cr)
string(REPLACE " " "" host_compact "${host_source}")
string(REPLACE "${whitespace_tab}" "" host_compact "${host_compact}")
string(REPLACE "${whitespace_lf}" "" host_compact "${host_compact}")
string(REPLACE "${whitespace_cr}" "" host_compact "${host_compact}")
set(capture_helper
    "voidcapture_after_presented_frame(constchar*path)noexcept{")
string(FIND "${host_compact}" "${capture_helper}" capture_helper_index)
set(helper_export_index -1)
if(NOT capture_helper_index EQUAL -1)
    string(SUBSTRING "${host_compact}" ${capture_helper_index} 160 capture_helper_body)
    string(FIND "${capture_helper_body}" "export_screenshot(path)" helper_export_index)
endif()
set(after_present_anchor "EndDrawing();++presented_frame_count;")
string(FIND "${host_compact}" "${after_present_anchor}" after_present_index)
set(capture_after_present_index -1)
if(NOT after_present_index EQUAL -1)
    string(SUBSTRING "${host_compact}" ${after_present_index} -1 host_after_present)
    string(FIND "${host_after_present}"
        "capture_after_presented_frame(" capture_after_present_index)
endif()
if(NOT host_header MATCHES "validation_exit_after_presented_frames")
    message(FATAL_ERROR "Stage 10 host lacks presented-frame exit control")
endif()
if(capture_helper_index EQUAL -1 OR helper_export_index EQUAL -1)
    message(FATAL_ERROR
        "Stage 10 host lacks the post-present capture helper "
        "(helper=${capture_helper_index}, export=${helper_export_index})")
endif()
if(after_present_index EQUAL -1 OR capture_after_present_index EQUAL -1)
    message(FATAL_ERROR
        "Stage 10 screenshot capture is not structurally after EndDrawing "
        "(present=${after_present_index}, capture=${capture_after_present_index})")
endif()

foreach(script_text capture_script formal_capture_script)
    if(NOT ${script_text} MATCHES "LastWriteTimeUtc"
            OR NOT ${script_text} MATCHES "System.Drawing"
            OR NOT ${script_text} MATCHES "HashSet"
            OR NOT ${script_text} MATCHES "nonBackground")
        message(FATAL_ERROR "Stage 10 capture script lacks freshness and pixel-content checks")
    endif()
endforeach()
