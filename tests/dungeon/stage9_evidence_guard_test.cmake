if(NOT DEFINED TRACE_SOURCE OR NOT DEFINED FIXTURE_SOURCE
        OR NOT DEFINED HOST_HEADER OR NOT DEFINED HOST_SOURCE)
    message(FATAL_ERROR "Stage 9 evidence guard requires trace, fixture, and formal-host sources")
endif()

file(READ "${TRACE_SOURCE}" trace_source)
file(READ "${FIXTURE_SOURCE}" fixture_source)
file(READ "${HOST_HEADER}" host_header)
file(READ "${HOST_SOURCE}" host_source)

if(trace_source MATCHES "bool[ \t\r\n]+relay_all_defeats")
    message(FATAL_ERROR
        "1000-room trace still has relay_all_defeats: direct CombatEvent injection is not real combat evidence")
endif()

if(trace_source MATCHES "relay_defeated\\(")
    message(FATAL_ERROR
        "1000-room trace still calls relay_defeated: rewards must originate in CombatWorld::defeat_monster")
endif()

if(fixture_source MATCHES "v4_reload_consistent=1")
    message(FATAL_ERROR
        "Stage 9 fixture still prints V4 consistency as a literal instead of a real comparison")
endif()

if(NOT host_header MATCHES "validation_exit_after_presented_frames")
    message(FATAL_ERROR
        "Formal Raylib host has no test-only presented-frame exit control for a captured real game frame")
endif()

if(NOT host_source MATCHES "validation_exit_after_presented_frames")
    message(FATAL_ERROR
        "Formal Raylib host does not consume the presented-frame exit control")
endif()
