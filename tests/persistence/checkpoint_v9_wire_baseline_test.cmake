if(NOT DEFINED PROBE OR NOT EXISTS "${PROBE}")
    message(FATAL_ERROR "PROBE must name the V9 baseline executable")
endif()
if(NOT DEFINED OUTPUT_DIR)
    message(FATAL_ERROR "OUTPUT_DIR is required")
endif()

file(MAKE_DIRECTORY "${OUTPUT_DIR}")

set(SCENARIOS
    canonical_none
    active_normal
    started_abyss
    death_pending
    cleared_abyss_rewards)

# Frozen from the pre-refactor V9 encoder at commit 5ba484f.
set(EXPECTED_canonical_none
    "13822|f3b1558d2808508b2a9ec044a39809b2ba98eae42adb209a14a947bfaebcc1a6")
set(EXPECTED_active_normal
    "16825|5551d2b0bd82b76bb87031f5281389f557acd010ab3200bc94426df78aab177e")
set(EXPECTED_started_abyss
    "16825|6ef9aa75b25c0db2a35d3ffd7a9e41a2d12f10e4511a2496826100f16931d62d")
set(EXPECTED_death_pending
    "16868|b40b01a2a57e8755f80bd374d631e0a4280bc0bc26c4e4ed08d49e28aab4b934")
set(EXPECTED_cleared_abyss_rewards
    "13902|3c70cf7631087f983f6e7ff82a62781ed6f7ab64131477e28f71111ef4e21ef3")

set(FAILURES "")
foreach(SCENARIO IN LISTS SCENARIOS)
    set(OUTPUT "${OUTPUT_DIR}/${SCENARIO}.v9.bin")
    file(REMOVE "${OUTPUT}")
    execute_process(
        COMMAND "${PROBE}" "${SCENARIO}" "${OUTPUT}"
        RESULT_VARIABLE RESULT
        OUTPUT_VARIABLE STDOUT
        ERROR_VARIABLE STDERR)
    if(NOT RESULT EQUAL 0)
        string(APPEND FAILURES
            "${SCENARIO}: probe exit ${RESULT}; stdout='${STDOUT}'; stderr='${STDERR}'\n")
        continue()
    endif()
    if(NOT EXISTS "${OUTPUT}")
        string(APPEND FAILURES
            "${SCENARIO}: probe returned success without writing '${OUTPUT}'\n")
        continue()
    endif()
    file(SIZE "${OUTPUT}" ACTUAL_SIZE)
    file(SHA256 "${OUTPUT}" ACTUAL_SHA256)
    set(EXPECTED "${EXPECTED_${SCENARIO}}")
    string(REPLACE "|" ";" EXPECTED_PARTS "${EXPECTED}")
    list(GET EXPECTED_PARTS 0 EXPECTED_SIZE)
    list(GET EXPECTED_PARTS 1 EXPECTED_SHA256)
    if(NOT ACTUAL_SIZE STREQUAL EXPECTED_SIZE
            OR NOT ACTUAL_SHA256 STREQUAL EXPECTED_SHA256)
        string(APPEND FAILURES
            "${SCENARIO}: expected ${EXPECTED_SIZE}|${EXPECTED_SHA256}, actual ${ACTUAL_SIZE}|${ACTUAL_SHA256}\n")
    endif()
endforeach()

if(NOT FAILURES STREQUAL "")
    message(FATAL_ERROR "V9 wire baseline mismatch:\n${FAILURES}")
endif()
