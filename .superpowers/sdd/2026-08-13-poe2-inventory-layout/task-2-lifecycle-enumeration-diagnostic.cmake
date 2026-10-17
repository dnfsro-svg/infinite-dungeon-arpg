cmake_minimum_required(VERSION 3.25)

get_filename_component(REPO_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
include("${REPO_ROOT}/tests/platform/cpp_source_lexer.cmake")
include("${REPO_ROOT}/tests/dungeon/evidence_source_scan.cmake")

# Verbatim preparation used by input_latency_source_test.cmake for each
# production source before lifecycle-role validation.
function(stage17_unconditional_cpp_surface SOURCE OUT_SURFACE)
    arpg_sanitize_cpp_source("${SOURCE}" LOGICAL_SOURCE)
    string(LENGTH "${LOGICAL_SOURCE}" SOURCE_LENGTH)
    set(CURSOR 0)
    set(CONDITIONAL_DEPTH 0)
    set(SURFACE "")
    while(CURSOR LESS SOURCE_LENGTH)
        string(SUBSTRING "${LOGICAL_SOURCE}" ${CURSOR} -1 TAIL)
        string(FIND "${TAIL}" "\n" NEWLINE)
        if(NEWLINE EQUAL -1)
            set(LINE "${TAIL}")
            set(LINE_LENGTH -1)
        else()
            math(EXPR LINE_LENGTH "${NEWLINE} + 1")
            string(SUBSTRING "${TAIL}" 0 ${LINE_LENGTH} LINE)
        endif()
        if(LINE MATCHES
                "^[ \t]*(#|%:)[ \t]*(if|ifdef|ifndef)([ \t\r\n(]|$)")
            math(EXPR CONDITIONAL_DEPTH "${CONDITIONAL_DEPTH} + 1")
        elseif(LINE MATCHES
                "^[ \t]*(#|%:)[ \t]*endif([ \t\r\n]|$)")
            math(EXPR CONDITIONAL_DEPTH "${CONDITIONAL_DEPTH} - 1")
            if(CONDITIONAL_DEPTH LESS 0)
                message(FATAL_ERROR
                    "Lifecycle enumeration conditional is unbalanced")
            endif()
        elseif(CONDITIONAL_DEPTH EQUAL 0)
            string(APPEND SURFACE "${LINE}")
        endif()
        if(NEWLINE EQUAL -1)
            break()
        endif()
        math(EXPR CURSOR "${CURSOR} + ${LINE_LENGTH}")
    endwhile()
    if(NOT CONDITIONAL_DEPTH EQUAL 0)
        message(FATAL_ERROR
            "Lifecycle enumeration conditional is unbalanced")
    endif()
    set(${OUT_SURFACE} "${SURFACE}" PARENT_SCOPE)
endfunction()

function(set_lifecycle_role_expectations ROLE)
    foreach(IDENTIFIER IN ITEMS
            SetConfigFlags InitWindow IsWindowReady CloseWindow
            WindowShouldClose)
        set("EXPECTED_ACTIVE_${IDENTIFIER}" 0 PARENT_SCOPE)
        set("EXPECTED_LEXICAL_${IDENTIFIER}" 0 PARENT_SCOPE)
    endforeach()
    if(ROLE STREQUAL owner)
        foreach(IDENTIFIER IN ITEMS
                SetConfigFlags InitWindow IsWindowReady CloseWindow)
            set("EXPECTED_ACTIVE_${IDENTIFIER}" 3 PARENT_SCOPE)
            set("EXPECTED_LEXICAL_${IDENTIFIER}" 3 PARENT_SCOPE)
        endforeach()
    elseif(ROLE STREQUAL policy)
        foreach(IDENTIFIER IN ITEMS
                SetConfigFlags InitWindow IsWindowReady CloseWindow
                WindowShouldClose)
            set("EXPECTED_LEXICAL_${IDENTIFIER}" 1 PARENT_SCOPE)
        endforeach()
    elseif(ROLE MATCHES "^ready-([0-9]+)$")
        set(EXPECTED_ACTIVE_IsWindowReady "${CMAKE_MATCH_1}" PARENT_SCOPE)
        set(EXPECTED_LEXICAL_IsWindowReady "${CMAKE_MATCH_1}" PARENT_SCOPE)
    elseif(ROLE STREQUAL should-close-1)
        set(EXPECTED_ACTIVE_WindowShouldClose 1 PARENT_SCOPE)
        set(EXPECTED_LEXICAL_WindowShouldClose 1 PARENT_SCOPE)
    endif()
endfunction()

evidence_collect_project_production_code_like_files(
    "${REPO_ROOT}" PRODUCTION_SOURCES)
set(INVALID_FILES)
foreach(PRODUCTION_SOURCE IN LISTS PRODUCTION_SOURCES)
    file(READ "${PRODUCTION_SOURCE}" PRODUCTION_TEXT)
    arpg_sanitize_cpp_source("${PRODUCTION_TEXT}" PRODUCTION_LEXICAL)
    stage17_unconditional_cpp_surface(
        "${PRODUCTION_TEXT}" PRODUCTION_ACTIVE)
    evidence_raylib_lifecycle_source_role(
        "${PRODUCTION_SOURCE}" "${REPO_ROOT}" PRODUCTION_ROLE)
    evidence_window_lifecycle_source_surface_is_valid(
        "${PRODUCTION_ACTIVE}" "${PRODUCTION_LEXICAL}"
        "${PRODUCTION_ROLE}" PRODUCTION_VALID)
    if(PRODUCTION_VALID)
        continue()
    endif()

    file(RELATIVE_PATH PRODUCTION_RELATIVE
        "${REPO_ROOT}" "${PRODUCTION_SOURCE}")
    file(TO_CMAKE_PATH "${PRODUCTION_RELATIVE}" PRODUCTION_RELATIVE)
    list(APPEND INVALID_FILES "${PRODUCTION_RELATIVE}")
    set_lifecycle_role_expectations("${PRODUCTION_ROLE}")
    message(STATUS
        "lifecycle invalid file=${PRODUCTION_RELATIVE} role=${PRODUCTION_ROLE}")
    foreach(IDENTIFIER IN ITEMS
            SetConfigFlags InitWindow IsWindowReady CloseWindow
            WindowShouldClose)
        evidence_count_cpp_identifier(
            "${PRODUCTION_ACTIVE}" "${IDENTIFIER}" ACTUAL_ACTIVE)
        evidence_count_cpp_identifier(
            "${PRODUCTION_LEXICAL}" "${IDENTIFIER}" ACTUAL_LEXICAL)
        message(STATUS
            "lifecycle identifier=${IDENTIFIER} active=${ACTUAL_ACTIVE}/${EXPECTED_ACTIVE_${IDENTIFIER}} lexical=${ACTUAL_LEXICAL}/${EXPECTED_LEXICAL_${IDENTIFIER}}")
    endforeach()
endforeach()

list(LENGTH INVALID_FILES INVALID_COUNT)
list(JOIN INVALID_FILES "," INVALID_LIST)
message(STATUS "lifecycle invalid_count=${INVALID_COUNT}")
message(STATUS "lifecycle invalid_list=${INVALID_LIST}")
