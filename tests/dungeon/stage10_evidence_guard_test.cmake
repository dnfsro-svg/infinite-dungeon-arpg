if(NOT DEFINED FIXTURE_SOURCE OR NOT DEFINED FORMAL_SOURCE
        OR NOT DEFINED HOST_HEADER OR NOT DEFINED HOST_SOURCE)
    message(FATAL_ERROR "Stage 10 evidence guard requires fixture, formal, and host sources")
endif()

file(READ "${FIXTURE_SOURCE}" fixture_source)
file(READ "${FORMAL_SOURCE}" formal_source)
file(READ "${HOST_HEADER}" host_header)
file(READ "${HOST_SOURCE}" host_source)
set(all_evidence "${fixture_source}\n${formal_source}\n${host_source}")

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
    if(all_evidence MATCHES "${forbidden}")
        message(FATAL_ERROR "Forbidden Stage 10 evidence injection: ${forbidden}")
    endif()
endforeach()

if(NOT fixture_source MATCHES "SaveStore"
        OR NOT fixture_source MATCHES "request_pickup"
        OR NOT fixture_source MATCHES "MovementInput")
    message(FATAL_ERROR "Stage 10 fixture must use SaveStore and public pickup/movement APIs")
endif()
if(NOT formal_source MATCHES "make_door_transition"
        OR NOT formal_source MATCHES "run_raylib_host"
        OR NOT host_source MATCHES "queue_action"
        OR NOT host_source MATCHES "request_descent"
        OR NOT host_source MATCHES "reset_current_room")
    message(FATAL_ERROR "Formal Stage 10 evidence must use generation and normal host inputs")
endif()
if(NOT host_header MATCHES "validation_exit_after_presented_frames"
        OR NOT host_source MATCHES "EndDrawing\\(\\)"
        OR NOT host_source MATCHES "export_screenshot")
    message(FATAL_ERROR "Stage 10 screenshots must come from a presented Raylib frame")
endif()
