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

# Frozen after the canonical secondary-ordinal marker became part of V9.
set(EXPECTED_canonical_none
    "13823|bdba4164210dad98123380edee8484ace2d1e48ee73d48613607a0a2305f8227")
set(EXPECTED_active_normal
    "16826|2304bcb1264f5ab1ffdf5e546096da82cdedf112dff326b1b3b1992b7f7a0615")
set(EXPECTED_started_abyss
    "16826|c62b3f2ca64869200d8bec6ecbf2a4a376c84eb783c0c7d229daf147cce76395")
set(EXPECTED_death_pending
    "16869|62d396810971eb2ad30bffd92a4c5ed0612e1be9ecc759bf844b56e376d45ca7")
set(EXPECTED_cleared_abyss_rewards
    "13903|275f3f016084cfada53247ccc610666646b4d7dd7d188ceda34fe79bb6d3efa4")

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
