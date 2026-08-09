if(NOT DEFINED RAYLIB_SOURCE_DIR)
    message(FATAL_ERROR "RAYLIB_SOURCE_DIR is required")
endif()

set(POISON_HEADER "${RAYLIB_SOURCE_DIR}/direct_input_poison.hpp")
if(NOT EXISTS "${POISON_HEADER}")
    message(FATAL_ERROR
        "direct input poison header is required at ${POISON_HEADER}")
endif()

file(READ "${RAYLIB_SOURCE_DIR}/raylib_host.cpp" HOST_SOURCE)
set(HOST_WINDOW_LIFETIME_PATH
    "${RAYLIB_SOURCE_DIR}/host_window_lifetime.cpp")
if(NOT EXISTS "${HOST_WINDOW_LIFETIME_PATH}")
    message(FATAL_ERROR
        "host window lifetime source is required: ${HOST_WINDOW_LIFETIME_PATH}")
endif()
file(READ "${HOST_WINDOW_LIFETIME_PATH}" HOST_WINDOW_LIFETIME_SOURCE)
set(HOST_VALIDATION_RUNTIME_PATH
    "${RAYLIB_SOURCE_DIR}/host_validation_runtime.cpp")
if(NOT EXISTS "${HOST_VALIDATION_RUNTIME_PATH}")
    message(FATAL_ERROR
        "host validation runtime is required: ${HOST_VALIDATION_RUNTIME_PATH}")
endif()
file(READ "${HOST_VALIDATION_RUNTIME_PATH}" HOST_VALIDATION_RUNTIME_SOURCE)
set(STAGE17_REPORT_SOURCE
    "${RAYLIB_SOURCE_DIR}/host_validation_stage17_report.cpp")
if(NOT EXISTS "${STAGE17_REPORT_SOURCE}")
    message(FATAL_ERROR
        "Stage17 report source is required: ${STAGE17_REPORT_SOURCE}")
endif()
file(READ "${STAGE17_REPORT_SOURCE}" STAGE17_REPORT_SOURCE_TEXT)
file(READ "${RAYLIB_SOURCE_DIR}/host_validation_stage17_runtime.cpp"
    STAGE17_RUNTIME_SOURCE)
file(READ "${RAYLIB_SOURCE_DIR}/inventory_renderer.cpp" INVENTORY_SOURCE)
file(READ "${RAYLIB_SOURCE_DIR}/host_input.cpp" INPUT_AUTHORITY_SOURCE)
file(READ "${RAYLIB_SOURCE_DIR}/../../dungeon/dungeon_session.cpp"
    DUNGEON_SESSION_SOURCE)
file(READ "${POISON_HEADER}" POISON_SOURCE)
include("${CMAKE_CURRENT_LIST_DIR}/cpp_source_lexer.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/../dungeon/evidence_source_scan.cmake")

function(require_match_count SOURCE PATTERN EXPECTED LABEL)
    string(REGEX MATCHALL "${PATTERN}" MATCHES "${SOURCE}")
    list(LENGTH MATCHES ACTUAL)
    if(NOT ACTUAL EQUAL EXPECTED)
        message(FATAL_ERROR
            "${LABEL}: expected ${EXPECTED} matches, found ${ACTUAL}")
    endif()
endfunction()

string(FIND "${INVENTORY_SOURCE}"
    "EQUIPMENT INVENTORY  ·  I / ESC CLOSE"
    LEGACY_INVENTORY_BINDING_TITLE)
string(FIND "${INVENTORY_SOURCE}"
    "技能石背包  ·  I / ESC 关闭"
    LEGACY_SKILL_BINDING_TITLE)
if(NOT LEGACY_INVENTORY_BINDING_TITLE EQUAL -1
        OR NOT LEGACY_SKILL_BINDING_TITLE EQUAL -1)
    message(FATAL_ERROR
        "inventory page titles must not hard-code the default I binding")
endif()
require_match_count("${INVENTORY_SOURCE}"
    "page_title\\.data\\(\\)" 2
    "inventory renderer dynamic page-title consumers")
set(DYNAMIC_INVENTORY_TITLE [=[const auto page_title = inventory_page_title(
        page_, inventory_binding_label);]=])
string(FIND "${INVENTORY_SOURCE}" "${DYNAMIC_INVENTORY_TITLE}"
    DYNAMIC_INVENTORY_TITLE_INDEX)
if(DYNAMIC_INVENTORY_TITLE_INDEX EQUAL -1)
    message(FATAL_ERROR
        "inventory renderer must format titles with its binding argument")
endif()
set(DYNAMIC_INVENTORY_DRAW [=[inventory.draw(*session, current, runtime.render_status(),
                    renderer.material_pack(), stable_key_label(
                        settings::binding_for(input_settings,
                            settings::SettingAction::inventory)),
                    renderer.hud_font(),]=])
string(FIND "${HOST_SOURCE}" "${DYNAMIC_INVENTORY_DRAW}"
    DYNAMIC_INVENTORY_DRAW_INDEX)
if(DYNAMIC_INVENTORY_DRAW_INDEX EQUAL -1)
    message(FATAL_ERROR
        "inventory renderer must receive the applied input_settings binding")
endif()
if(ARPG_CPLAY027_INVENTORY_BINDING_LABEL_ONLY)
    message(STATUS
        "inventory page titles consume the applied input binding")
    return()
endif()

function(host_large_state_construction_valid SOURCE OUT_VALID)
    set(WS "[ \t\r\n]*")
    set(WS1 "[ \t\r\n]+")
    string(REGEX MATCHALL
        "(^|[^A-Za-z0-9_])(const${WS1})?(dungeon::)?DungeonSnapshot${WS1}[A-Za-z_][A-Za-z0-9_]*${WS}(\\{|\\(|;|=)"
        AUTOMATIC_SNAPSHOT_DECLARATIONS "${SOURCE}")
    list(LENGTH AUTOMATIC_SNAPSHOT_DECLARATIONS
        AUTOMATIC_SNAPSHOT_DECLARATION_COUNT)
    string(REGEX MATCHALL
        "(^|[^A-Za-z0-9_])DungeonSnapshot${WS}(\\{|\\()"
        DIRECT_SNAPSHOT_CONSTRUCTIONS "${SOURCE}")
    list(LENGTH DIRECT_SNAPSHOT_CONSTRUCTIONS
        DIRECT_SNAPSHOT_CONSTRUCTION_COUNT)
    string(REGEX MATCHALL
        "(^|[^A-Za-z0-9_])(const${WS1})?HostValidationStates${WS1}[A-Za-z_][A-Za-z0-9_]*${WS}(\\{|\\(|;|=)"
        AUTOMATIC_VALIDATION_DECLARATIONS "${SOURCE}")
    list(LENGTH AUTOMATIC_VALIDATION_DECLARATIONS
        AUTOMATIC_VALIDATION_DECLARATION_COUNT)
    string(REGEX MATCHALL
        "(^|[^A-Za-z0-9_])HostValidationStates${WS}(\\{|\\()"
        DIRECT_VALIDATION_CONSTRUCTIONS "${SOURCE}")
    list(LENGTH DIRECT_VALIDATION_CONSTRUCTIONS
        DIRECT_VALIDATION_CONSTRUCTION_COUNT)
    string(REGEX MATCHALL
        "HostValidationRuntime::create${WS}\\("
        APPROVED_VALIDATION_RUNTIME_FACTORIES "${SOURCE}")
    list(LENGTH APPROVED_VALIDATION_RUNTIME_FACTORIES
        APPROVED_VALIDATION_RUNTIME_FACTORY_COUNT)
    if(AUTOMATIC_SNAPSHOT_DECLARATION_COUNT EQUAL 0
            AND DIRECT_SNAPSHOT_CONSTRUCTION_COUNT EQUAL 0
            AND AUTOMATIC_VALIDATION_DECLARATION_COUNT EQUAL 0
            AND DIRECT_VALIDATION_CONSTRUCTION_COUNT EQUAL 0
            AND APPROVED_VALIDATION_RUNTIME_FACTORY_COUNT EQUAL 1)
        set("${OUT_VALID}" TRUE PARENT_SCOPE)
    else()
        set("${OUT_VALID}" FALSE PARENT_SCOPE)
    endif()
endfunction()

function(require_host_large_construction_mutations_rejected SOURCE)
    set(AUTO_LARGE_STATE_ACCEPTANCES)
    set(AUTO_NAMESPACED_SNAPSHOT_MUTATION
        "${SOURCE}\nauto stack_snapshot = dungeon::DungeonSnapshot{};\n")
    host_large_state_construction_valid(
        "${AUTO_NAMESPACED_SNAPSHOT_MUTATION}" AUTO_NAMESPACED_SNAPSHOT_VALID)
    if(AUTO_NAMESPACED_SNAPSHOT_VALID)
        list(APPEND AUTO_LARGE_STATE_ACCEPTANCES
            "auto-namespaced-DungeonSnapshot")
    endif()
    set(AUTO_UNQUALIFIED_SNAPSHOT_MUTATION
        "${SOURCE}\nconst auto stack_snapshot =\n    DungeonSnapshot { };\n")
    host_large_state_construction_valid(
        "${AUTO_UNQUALIFIED_SNAPSHOT_MUTATION}" AUTO_UNQUALIFIED_SNAPSHOT_VALID)
    if(AUTO_UNQUALIFIED_SNAPSHOT_VALID)
        list(APPEND AUTO_LARGE_STATE_ACCEPTANCES
            "auto-unqualified-DungeonSnapshot")
    endif()
    set(AUTO_CONST_SNAPSHOT_MUTATION
        "${SOURCE}\nauto const stack_snapshot = dungeon::DungeonSnapshot{};\n")
    host_large_state_construction_valid(
        "${AUTO_CONST_SNAPSHOT_MUTATION}" AUTO_CONST_SNAPSHOT_VALID)
    if(AUTO_CONST_SNAPSHOT_VALID)
        list(APPEND AUTO_LARGE_STATE_ACCEPTANCES
            "auto-const-DungeonSnapshot")
    endif()
    set(AUTO_GLOBAL_NAMESPACE_SNAPSHOT_MUTATION
        "${SOURCE}\nauto stack_snapshot = ::dungeon::DungeonSnapshot{};\n")
    host_large_state_construction_valid(
        "${AUTO_GLOBAL_NAMESPACE_SNAPSHOT_MUTATION}"
        AUTO_GLOBAL_NAMESPACE_SNAPSHOT_VALID)
    if(AUTO_GLOBAL_NAMESPACE_SNAPSHOT_VALID)
        list(APPEND AUTO_LARGE_STATE_ACCEPTANCES
            "global-namespace-DungeonSnapshot")
    endif()
    set(AUTO_SPACED_NAMESPACE_SNAPSHOT_MUTATION
        "${SOURCE}\nauto stack_snapshot = dungeon :: DungeonSnapshot{};\n")
    host_large_state_construction_valid(
        "${AUTO_SPACED_NAMESPACE_SNAPSHOT_MUTATION}"
        AUTO_SPACED_NAMESPACE_SNAPSHOT_VALID)
    if(AUTO_SPACED_NAMESPACE_SNAPSHOT_VALID)
        list(APPEND AUTO_LARGE_STATE_ACCEPTANCES
            "spaced-namespace-DungeonSnapshot")
    endif()
    set(PAREN_SNAPSHOT_MUTATION
        "${SOURCE}\nauto stack_snapshot = dungeon::DungeonSnapshot();\n")
    host_large_state_construction_valid(
        "${PAREN_SNAPSHOT_MUTATION}" PAREN_SNAPSHOT_VALID)
    if(PAREN_SNAPSHOT_VALID)
        list(APPEND AUTO_LARGE_STATE_ACCEPTANCES
            "paren-DungeonSnapshot")
    endif()
    set(AUTO_VALIDATION_MUTATION
        "${SOURCE}\nauto stack_validation =\n    HostValidationStates { };\n")
    host_large_state_construction_valid(
        "${AUTO_VALIDATION_MUTATION}" AUTO_VALIDATION_VALID)
    if(AUTO_VALIDATION_VALID)
        list(APPEND AUTO_LARGE_STATE_ACCEPTANCES
            "auto-HostValidationStates")
    endif()
    set(AUTO_CONST_VALIDATION_MUTATION
        "${SOURCE}\nauto const stack_validation = HostValidationStates{};\n")
    host_large_state_construction_valid(
        "${AUTO_CONST_VALIDATION_MUTATION}" AUTO_CONST_VALIDATION_VALID)
    if(AUTO_CONST_VALIDATION_VALID)
        list(APPEND AUTO_LARGE_STATE_ACCEPTANCES
            "auto-const-HostValidationStates")
    endif()
    set(AUTO_GLOBAL_VALIDATION_MUTATION
        "${SOURCE}\nauto stack_validation = ::HostValidationStates{};\n")
    host_large_state_construction_valid(
        "${AUTO_GLOBAL_VALIDATION_MUTATION}" AUTO_GLOBAL_VALIDATION_VALID)
    if(AUTO_GLOBAL_VALIDATION_VALID)
        list(APPEND AUTO_LARGE_STATE_ACCEPTANCES
            "global-HostValidationStates")
    endif()
    set(PAREN_VALIDATION_MUTATION
        "${SOURCE}\nauto stack_validation = HostValidationStates();\n")
    host_large_state_construction_valid(
        "${PAREN_VALIDATION_MUTATION}" PAREN_VALIDATION_VALID)
    if(PAREN_VALIDATION_VALID)
        list(APPEND AUTO_LARGE_STATE_ACCEPTANCES
            "paren-HostValidationStates")
    endif()
    if(AUTO_LARGE_STATE_ACCEPTANCES)
        list(JOIN AUTO_LARGE_STATE_ACCEPTANCES ", " AUTO_LARGE_STATE_NAMES)
        message(FATAL_ERROR
            "host large-state guard accepted mutations: ${AUTO_LARGE_STATE_NAMES}")
    endif()
endfunction()

if(ARPG_HOST_AUTO_GUARD_SELF_TEST_ONLY)
    set(HOST_AUTO_GUARD_REFERENCE [=[
const auto current_storage = std::make_unique<dungeon::DungeonSnapshot>();
dungeon::DungeonSnapshot& current = *current_storage;
const auto validation_runtime =
    HostValidationRuntime::create(config, load_status);
]=])
    host_large_state_construction_valid(
        "${HOST_AUTO_GUARD_REFERENCE}" HOST_AUTO_GUARD_REFERENCE_VALID)
    if(NOT HOST_AUTO_GUARD_REFERENCE_VALID)
        message(FATAL_ERROR
            "host large-state construction guard rejected heap owners/reference aliases")
    endif()
    require_host_large_construction_mutations_rejected(
        "${HOST_AUTO_GUARD_REFERENCE}")
    return()
endif()

# arpg_sanitize_cpp_source performs translation phase 2 before returning its
# logical-line source. Conditional code is never unconditional evidence, so
# mask every complete conditional group after the shared lexer has folded LF
# and CRLF splices.
function(mask_cpp_inactive_preprocessor_regions SOURCE OUT_SOURCE)
    string(LENGTH "${SOURCE}" SOURCE_LENGTH)
    set(MASKED_SOURCE "")
    set(BRANCH_BOUNDARY "__ARPG_PREPROCESSOR_BRANCH_BOUNDARY__;")
    set(LINE_CURSOR 0)
    set(CONDITIONAL_DEPTH 0)
    while(LINE_CURSOR LESS SOURCE_LENGTH)
        string(SUBSTRING "${SOURCE}" ${LINE_CURSOR} -1 SOURCE_TAIL)
        string(FIND "${SOURCE_TAIL}" "\n" NEWLINE_RELATIVE)
        if(NEWLINE_RELATIVE EQUAL -1)
            set(SOURCE_LINE "${SOURCE_TAIL}")
            set(HAS_NEWLINE FALSE)
            set(LINE_CURSOR ${SOURCE_LENGTH})
        else()
            string(SUBSTRING "${SOURCE_TAIL}"
                0 ${NEWLINE_RELATIVE} SOURCE_LINE)
            set(HAS_NEWLINE TRUE)
            math(EXPR LINE_CURSOR
                "${LINE_CURSOR} + ${NEWLINE_RELATIVE} + 1")
        endif()

        set(CONDITIONAL_DIRECTIVE FALSE)
        set(DIRECTIVE_COMMAND "")
        if("${SOURCE_LINE}" MATCHES
                "^[ \\t]*(#|%:)[ \\t]*([A-Za-z_][A-Za-z0-9_]*)(.*)$")
            set(DIRECTIVE_COMMAND "${CMAKE_MATCH_2}")
        endif()
        if(DIRECTIVE_COMMAND STREQUAL "if"
                OR DIRECTIVE_COMMAND STREQUAL "ifdef"
                OR DIRECTIVE_COMMAND STREQUAL "ifndef")
            set(CONDITIONAL_DIRECTIVE TRUE)
            math(EXPR CONDITIONAL_DEPTH "${CONDITIONAL_DEPTH} + 1")
        elseif(DIRECTIVE_COMMAND STREQUAL "elif"
                OR DIRECTIVE_COMMAND STREQUAL "else")
            set(CONDITIONAL_DIRECTIVE TRUE)
            if(CONDITIONAL_DEPTH EQUAL 0)
                message(FATAL_ERROR
                    "inactive preprocessor scan found ${DIRECTIVE_COMMAND} without #if")
            endif()
        elseif(DIRECTIVE_COMMAND STREQUAL "endif")
            set(CONDITIONAL_DIRECTIVE TRUE)
            if(CONDITIONAL_DEPTH EQUAL 0)
                message(FATAL_ERROR
                    "inactive preprocessor scan found #endif without #if")
            endif()
            math(EXPR CONDITIONAL_DEPTH "${CONDITIONAL_DEPTH} - 1")
        endif()

        if(CONDITIONAL_DIRECTIVE)
            string(APPEND MASKED_SOURCE "${BRANCH_BOUNDARY}")
        elseif(CONDITIONAL_DEPTH EQUAL 0)
            string(APPEND MASKED_SOURCE "${SOURCE_LINE}")
        endif()
        if(HAS_NEWLINE OR CONDITIONAL_DIRECTIVE)
            string(APPEND MASKED_SOURCE "\n")
        endif()
    endwhile()
    if(NOT CONDITIONAL_DEPTH EQUAL 0)
        message(FATAL_ERROR
            "inactive preprocessor scan found unterminated conditional")
    endif()
    set("${OUT_SOURCE}" "${MASKED_SOURCE}" PARENT_SCOPE)
endfunction()

arpg_sanitize_cpp_source("${HOST_SOURCE}" SANITIZED_HOST_SOURCE)
arpg_sanitize_cpp_source("${HOST_WINDOW_LIFETIME_SOURCE}"
    SANITIZED_HOST_WINDOW_LIFETIME_SOURCE)
arpg_sanitize_cpp_source("${HOST_VALIDATION_RUNTIME_SOURCE}"
    SANITIZED_HOST_VALIDATION_RUNTIME_SOURCE)
arpg_sanitize_cpp_source("${STAGE17_RUNTIME_SOURCE}"
    SANITIZED_STAGE17_RUNTIME_SOURCE)
arpg_sanitize_cpp_source(
    "${DUNGEON_SESSION_SOURCE}" SANITIZED_DUNGEON_SESSION_SOURCE)
mask_cpp_inactive_preprocessor_regions(
    "${SANITIZED_HOST_SOURCE}" ACTIVE_SANITIZED_HOST_SOURCE)
mask_cpp_inactive_preprocessor_regions(
    "${SANITIZED_HOST_WINDOW_LIFETIME_SOURCE}"
    ACTIVE_SANITIZED_HOST_WINDOW_LIFETIME_SOURCE)
mask_cpp_inactive_preprocessor_regions(
    "${SANITIZED_HOST_VALIDATION_RUNTIME_SOURCE}"
    ACTIVE_SANITIZED_HOST_VALIDATION_RUNTIME_SOURCE)
mask_cpp_inactive_preprocessor_regions(
    "${SANITIZED_STAGE17_RUNTIME_SOURCE}"
    ACTIVE_SANITIZED_STAGE17_RUNTIME_SOURCE)
mask_cpp_inactive_preprocessor_regions(
    "${SANITIZED_DUNGEON_SESSION_SOURCE}"
    ACTIVE_SANITIZED_DUNGEON_SESSION_SOURCE)

# The shared evidence scanner finds balanced function bodies in already
# sanitized C++. Require a unique definition first so a forward declaration
# cannot borrow the next function's opening brace.
function(find_balanced_cpp_scope_end_from_open
        SOURCE OPEN_INDEX OUT_END OUT_VALID)
    string(LENGTH "${SOURCE}" SOURCE_LENGTH)
    if(OPEN_INDEX LESS 0 OR OPEN_INDEX GREATER_EQUAL SOURCE_LENGTH)
        set("${OUT_END}" -1 PARENT_SCOPE)
        set("${OUT_VALID}" FALSE PARENT_SCOPE)
        return()
    endif()
    string(SUBSTRING "${SOURCE}" ${OPEN_INDEX} 1 OPEN_CHARACTER)
    if(NOT OPEN_CHARACTER STREQUAL "{")
        set("${OUT_END}" -1 PARENT_SCOPE)
        set("${OUT_VALID}" FALSE PARENT_SCOPE)
        return()
    endif()
    set(BRACE_CURSOR ${OPEN_INDEX})
    set(BRACE_DEPTH 0)
    while(BRACE_CURSOR LESS SOURCE_LENGTH)
        string(SUBSTRING "${SOURCE}" ${BRACE_CURSOR} -1 BRACE_TAIL)
        string(FIND "${BRACE_TAIL}" "{" NEXT_OPEN_RELATIVE)
        string(FIND "${BRACE_TAIL}" "}" NEXT_CLOSE_RELATIVE)
        if(NEXT_CLOSE_RELATIVE EQUAL -1)
            set("${OUT_END}" -1 PARENT_SCOPE)
            set("${OUT_VALID}" FALSE PARENT_SCOPE)
            return()
        endif()
        if(NOT NEXT_OPEN_RELATIVE EQUAL -1
                AND NEXT_OPEN_RELATIVE LESS NEXT_CLOSE_RELATIVE)
            math(EXPR BRACE_CURSOR
                "${BRACE_CURSOR} + ${NEXT_OPEN_RELATIVE} + 1")
            math(EXPR BRACE_DEPTH "${BRACE_DEPTH} + 1")
        else()
            math(EXPR CLOSE_INDEX
                "${BRACE_CURSOR} + ${NEXT_CLOSE_RELATIVE}")
            math(EXPR BRACE_DEPTH "${BRACE_DEPTH} - 1")
            if(BRACE_DEPTH EQUAL 0)
                set("${OUT_END}" ${CLOSE_INDEX} PARENT_SCOPE)
                set("${OUT_VALID}" TRUE PARENT_SCOPE)
                return()
            endif()
            math(EXPR BRACE_CURSOR "${CLOSE_INDEX} + 1")
        endif()
    endwhile()
    set("${OUT_END}" -1 PARENT_SCOPE)
    set("${OUT_VALID}" FALSE PARENT_SCOPE)
endfunction()

function(try_extract_cpp_function_definition_from_sanitized
        SOURCE SIGNATURE OUT_SOURCE OUT_VALID)
    string(FIND "${SOURCE}" "${SIGNATURE}" FUNCTION_BEGIN)
    if(FUNCTION_BEGIN EQUAL -1)
        set("${OUT_SOURCE}" "" PARENT_SCOPE)
        set("${OUT_VALID}" FALSE PARENT_SCOPE)
        return()
    endif()
    string(LENGTH "${SIGNATURE}" SIGNATURE_LENGTH)
    math(EXPR AFTER_SIGNATURE "${FUNCTION_BEGIN} + ${SIGNATURE_LENGTH}")
    string(SUBSTRING "${SOURCE}" ${AFTER_SIGNATURE} -1 AFTER_SIGNATURE_SOURCE)
    string(FIND "${AFTER_SIGNATURE_SOURCE}" "${SIGNATURE}" SECOND_SIGNATURE)
    if(NOT SECOND_SIGNATURE EQUAL -1)
        set("${OUT_SOURCE}" "" PARENT_SCOPE)
        set("${OUT_VALID}" FALSE PARENT_SCOPE)
        return()
    endif()
    string(SUBSTRING "${SOURCE}" ${FUNCTION_BEGIN} -1 FUNCTION_TAIL)
    string(FIND "${FUNCTION_TAIL}" "{" OPEN_RELATIVE)
    string(FIND "${FUNCTION_TAIL}" ";" SEMICOLON_RELATIVE)
    if(OPEN_RELATIVE EQUAL -1
            OR (NOT SEMICOLON_RELATIVE EQUAL -1
                AND SEMICOLON_RELATIVE LESS OPEN_RELATIVE))
        set("${OUT_SOURCE}" "" PARENT_SCOPE)
        set("${OUT_VALID}" FALSE PARENT_SCOPE)
        return()
    endif()
    math(EXPR FUNCTION_OPEN "${FUNCTION_BEGIN} + ${OPEN_RELATIVE}")
    find_balanced_cpp_scope_end_from_open(
        "${SOURCE}" ${FUNCTION_OPEN} FUNCTION_END FUNCTION_SCOPE_VALID)
    if(NOT FUNCTION_SCOPE_VALID)
        set("${OUT_SOURCE}" "" PARENT_SCOPE)
        set("${OUT_VALID}" FALSE PARENT_SCOPE)
        return()
    endif()
    math(EXPR FUNCTION_LENGTH "${FUNCTION_END} - ${FUNCTION_BEGIN} + 1")
    string(SUBSTRING "${SOURCE}"
        ${FUNCTION_BEGIN} ${FUNCTION_LENGTH} FUNCTION_SOURCE)
    set("${OUT_SOURCE}" "${FUNCTION_SOURCE}" PARENT_SCOPE)
    set("${OUT_VALID}" TRUE PARENT_SCOPE)
endfunction()

function(require_cpp_function_definition_from_sanitized
        SOURCE SIGNATURE LABEL OUT_SOURCE)
    try_extract_cpp_function_definition_from_sanitized(
        "${SOURCE}" "${SIGNATURE}" FUNCTION_SOURCE FUNCTION_VALID)
    if(NOT FUNCTION_VALID)
        message(FATAL_ERROR
            "${LABEL} must have one concrete function definition")
    endif()
    set("${OUT_SOURCE}" "${FUNCTION_SOURCE}" PARENT_SCOPE)
endfunction()

# Retain normal control-flow blocks, but mask nested lambdas and statically dead
# branches. Evidence inside those scopes is not direct executable evidence for
# the named production function.
function(mask_cpp_non_direct_executable_scopes SOURCE OUT_SOURCE)
    string(LENGTH "${SOURCE}" SOURCE_LENGTH)
    if(SOURCE_LENGTH EQUAL 0)
        set("${OUT_SOURCE}" "" PARENT_SCOPE)
        return()
    endif()
    set(MASKED_SOURCE "")
    set(COPY_CURSOR 0)
    while(COPY_CURSOR LESS SOURCE_LENGTH)
        string(SUBSTRING "${SOURCE}" ${COPY_CURSOR} -1 SOURCE_TAIL)
        string(REGEX MATCH
            "\\][ \\t\\r\\n]*(\\([^{};]*\\))?[ \\t\\r\\n]*(mutable[ \\t\\r\\n]*)?(noexcept([ \\t\\r\\n]*\\([^{};]*\\))?[ \\t\\r\\n]*)?(->[^{;]*)?[ \\t\\r\\n]*\\{"
            LAMBDA_SCOPE_MATCH "${SOURCE_TAIL}")
        string(REGEX MATCH
            "(if|while)[ \\t\\r\\n]*(constexpr[ \\t\\r\\n]*)?\\([ \\t\\r\\n]*(false|0[uUlL]*|![ \\t\\r\\n]*true)[ \\t\\r\\n]*\\)[ \\t\\r\\n]*(do[ \\t\\r\\n]*)?([^{;]*\\{|[^{};]*;)"
            DEAD_SCOPE_MATCH "${SOURCE_TAIL}")
        set(SCOPE_MATCH "")
        set(SCOPE_MATCH_RELATIVE -1)
        set(SCOPE_MATCH_KIND "")
        if(NOT LAMBDA_SCOPE_MATCH STREQUAL "")
            string(FIND "${SOURCE_TAIL}"
                "${LAMBDA_SCOPE_MATCH}" SCOPE_MATCH_RELATIVE)
            set(SCOPE_MATCH "${LAMBDA_SCOPE_MATCH}")
            set(SCOPE_MATCH_KIND lambda)
        endif()
        if(NOT DEAD_SCOPE_MATCH STREQUAL "")
            string(FIND "${SOURCE_TAIL}"
                "${DEAD_SCOPE_MATCH}" DEAD_SCOPE_RELATIVE)
            if(SCOPE_MATCH_RELATIVE EQUAL -1
                    OR DEAD_SCOPE_RELATIVE LESS SCOPE_MATCH_RELATIVE)
                set(SCOPE_MATCH "${DEAD_SCOPE_MATCH}")
                set(SCOPE_MATCH_RELATIVE ${DEAD_SCOPE_RELATIVE})
                set(SCOPE_MATCH_KIND dead-control)
            endif()
        endif()
        if(SCOPE_MATCH_RELATIVE EQUAL -1)
            string(APPEND MASKED_SOURCE "${SOURCE_TAIL}")
            break()
        endif()
        string(FIND "${SCOPE_MATCH}" "{" OPEN_IN_MATCH)
        math(EXPR MATCH_INDEX "${COPY_CURSOR} + ${SCOPE_MATCH_RELATIVE}")
        if(SCOPE_MATCH_KIND STREQUAL "dead-control")
            set(REMOVE_BEGIN ${MATCH_INDEX})
        else()
            math(EXPR REMOVE_BEGIN "${MATCH_INDEX} + ${OPEN_IN_MATCH}")
        endif()
        math(EXPR COPY_LENGTH "${REMOVE_BEGIN} - ${COPY_CURSOR}")
        if(COPY_LENGTH GREATER 0)
            string(SUBSTRING "${SOURCE}"
                ${COPY_CURSOR} ${COPY_LENGTH} COPY_CHUNK)
            string(APPEND MASKED_SOURCE "${COPY_CHUNK}")
        endif()
        if(OPEN_IN_MATCH EQUAL -1)
            string(LENGTH "${SCOPE_MATCH}" SCOPE_MATCH_LENGTH)
            math(EXPR SCOPE_END
                "${MATCH_INDEX} + ${SCOPE_MATCH_LENGTH} - 1")
        else()
            math(EXPR OPEN_INDEX "${MATCH_INDEX} + ${OPEN_IN_MATCH}")
            find_balanced_cpp_scope_end_from_open(
                "${SOURCE}" ${OPEN_INDEX} SCOPE_END SCOPE_VALID)
            if(NOT SCOPE_VALID)
                message(FATAL_ERROR
                    "direct executable scope mutation has no closing brace")
            endif()
        endif()
        math(EXPR COPY_CURSOR "${SCOPE_END} + 1")
    endwhile()
    set("${OUT_SOURCE}" "${MASKED_SOURCE}" PARENT_SCOPE)
endfunction()

function(direct_cpp_fragment_present FUNCTION_SOURCE FRAGMENT OUT_VALID)
    mask_cpp_non_direct_executable_scopes(
        "${FUNCTION_SOURCE}" DIRECT_FUNCTION_SOURCE)
    string(FIND "${DIRECT_FUNCTION_SOURCE}" "${FRAGMENT}" FRAGMENT_INDEX)
    if(FRAGMENT_INDEX EQUAL -1)
        set("${OUT_VALID}" FALSE PARENT_SCOPE)
    else()
        set("${OUT_VALID}" TRUE PARENT_SCOPE)
    endif()
endfunction()

require_cpp_function_definition_from_sanitized(
    "${ACTIVE_SANITIZED_HOST_SOURCE}" "HostExitCode run_raylib_host("
    "raylib host entry" SANITIZED_HOST_ENTRY_SOURCE)
mask_cpp_non_direct_executable_scopes(
    "${SANITIZED_HOST_ENTRY_SOURCE}" DIRECT_HOST_ENTRY_SOURCE)
require_cpp_function_definition_from_sanitized(
    "${ACTIVE_SANITIZED_HOST_VALIDATION_RUNTIME_SOURCE}"
    "PhysicalKeySnapshot HostValidationRuntime::inject_physical_edges("
    "Host validation physical input composition"
    HOST_VALIDATION_INJECT_SOURCE)
mask_cpp_non_direct_executable_scopes(
    "${HOST_VALIDATION_INJECT_SOURCE}" DIRECT_HOST_VALIDATION_INJECT_SOURCE)
require_cpp_function_definition_from_sanitized(
    "${ACTIVE_SANITIZED_HOST_VALIDATION_RUNTIME_SOURCE}"
    "void HostValidationRuntime::write_summaries("
    "Host validation summary owner" HOST_VALIDATION_SUMMARY_SOURCE)
mask_cpp_non_direct_executable_scopes(
    "${HOST_VALIDATION_SUMMARY_SOURCE}" DIRECT_HOST_VALIDATION_SUMMARY_SOURCE)
require_cpp_function_definition_from_sanitized(
    "${ACTIVE_SANITIZED_STAGE17_RUNTIME_SOURCE}"
    "void observe_stage17_draw_runtime("
    "Stage17 draw runtime observer" STAGE17_DRAW_RUNTIME_SOURCE)
require_cpp_function_definition_from_sanitized(
    "${ACTIVE_SANITIZED_STAGE17_RUNTIME_SOURCE}"
    "bool stage17_prepare_isolated_storm("
    "Stage17 isolated storm preparation" STAGE17_PREPARE_STORM_SOURCE)
# Reuse the shared evidence scanner on this bounded critical helper. The fast
# outer extractor avoids rescanning the entire translation unit.
evidence_find_cpp_function_bounds_in_sanitized(
    "${STAGE17_PREPARE_STORM_SOURCE}"
    "bool stage17_prepare_isolated_storm("
    STAGE17_PREPARE_VERIFIED_BEGIN STAGE17_PREPARE_VERIFIED_OPEN
    STAGE17_PREPARE_VERIFIED_END)
string(LENGTH "${STAGE17_PREPARE_STORM_SOURCE}"
    STAGE17_PREPARE_VERIFIED_LENGTH)
math(EXPR STAGE17_PREPARE_EXPECTED_END
    "${STAGE17_PREPARE_VERIFIED_LENGTH} - 1")
if(NOT STAGE17_PREPARE_VERIFIED_BEGIN EQUAL 0
        OR NOT STAGE17_PREPARE_VERIFIED_END EQUAL
            STAGE17_PREPARE_EXPECTED_END)
    message(FATAL_ERROR
        "Stage17 isolated storm preparation bounds are unreliable")
endif()
require_cpp_function_definition_from_sanitized(
    "${ACTIVE_SANITIZED_STAGE17_RUNTIME_SOURCE}"
    "PhysicalKeySnapshot inject_stage17_physical_edges("
    "Stage17 physical input injection" STAGE17_INJECT_SOURCE)
require_cpp_function_definition_from_sanitized(
    "${ACTIVE_SANITIZED_STAGE17_RUNTIME_SOURCE}"
    "void observe_stage17_snapshot("
    "Stage17 snapshot observer" STAGE17_SNAPSHOT_SOURCE)
require_cpp_function_definition_from_sanitized(
    "${ACTIVE_SANITIZED_STAGE17_RUNTIME_SOURCE}"
    "void observe_stage17_inventory("
    "Stage17 inventory observer" STAGE17_INVENTORY_SOURCE)
require_cpp_function_definition_from_sanitized(
    "${ACTIVE_SANITIZED_DUNGEON_SESSION_SOURCE}"
    "void DungeonSession::tick("
    "DungeonSession::tick" DUNGEON_TICK_SOURCE)
require_cpp_function_definition_from_sanitized(
    "${ACTIVE_SANITIZED_DUNGEON_SESSION_SOURCE}"
    "void DungeonSession::prepare_room_clear("
    "DungeonSession::prepare_room_clear" PREPARE_ROOM_CLEAR_SOURCE)
foreach(FUNCTION_SOURCE_NAME IN ITEMS
        STAGE17_DRAW_RUNTIME_SOURCE STAGE17_PREPARE_STORM_SOURCE
        STAGE17_INJECT_SOURCE STAGE17_SNAPSHOT_SOURCE
        STAGE17_INVENTORY_SOURCE DUNGEON_TICK_SOURCE
        PREPARE_ROOM_CLEAR_SOURCE)
    mask_cpp_non_direct_executable_scopes(
        "${${FUNCTION_SOURCE_NAME}}" DIRECT_${FUNCTION_SOURCE_NAME})
endforeach()
host_large_state_construction_valid(
    "${SANITIZED_HOST_ENTRY_SOURCE}" HOST_LARGE_STATE_CONSTRUCTION_VALID)
if(NOT HOST_LARGE_STATE_CONSTRUCTION_VALID)
    message(FATAL_ERROR
        "raylib host must not create automatic large state objects")
endif()
require_host_large_construction_mutations_rejected(
    "${SANITIZED_HOST_ENTRY_SOURCE}")

require_match_count(
    "${SANITIZED_HOST_SOURCE}"
    "HostValidationRuntime::create[ \t\r\n]*\\("
    1
    "raylib host validation-runtime factory call")
require_match_count(
    "${SANITIZED_HOST_ENTRY_SOURCE}"
    "HostValidationStates"
    0
    "raylib host direct aggregate validation-state ownership")
foreach(SNAPSHOT_NAME IN ITEMS current previous presented_snapshot)
    require_match_count(
        "${SANITIZED_HOST_SOURCE}"
        "const[ \t\r\n]+auto[ \t\r\n]+${SNAPSHOT_NAME}_storage[ \t\r\n]*=[ \t\r\n]*std::make_unique[ \t\r\n]*<[ \t\r\n]*dungeon::DungeonSnapshot[ \t\r\n]*>[ \t\r\n]*\\([ \t\r\n]*\\)"
        1
        "raylib host ${SNAPSHOT_NAME} owning heap snapshot storage")
    require_match_count(
        "${SANITIZED_HOST_SOURCE}"
        "dungeon::DungeonSnapshot[ \t\r\n]*&[ \t\r\n]*${SNAPSHOT_NAME}[ \t\r\n]*=[ \t\r\n]*\\*[ \t\r\n]*${SNAPSHOT_NAME}_storage"
        1
        "raylib host ${SNAPSHOT_NAME} snapshot reference alias")
endforeach()
require_match_count(
    "${SANITIZED_HOST_SOURCE}"
    "(runtime\\.session\\(\\)|session)->snapshot[ \t\r\n]*\\([ \t\r\n]*current[ \t\r\n]*\\)"
    9
    "raylib host preallocated snapshot captures")
require_match_count(
    "${SANITIZED_HOST_SOURCE}"
    "(runtime\\.session\\(\\)|session)->snapshot[ \t\r\n]*\\([ \t\r\n]*\\)"
    0
    "raylib host by-value snapshot captures")
require_match_count(
    "${SANITIZED_HOST_SOURCE}"
    "(^|[^A-Za-z0-9_])(const[ \t\r\n]+)?dungeon::DungeonSnapshot[ \t\r\n]+[A-Za-z_][A-Za-z0-9_]*[ \t\r\n]*(\\{|;|=)"
    0
    "raylib host automatic DungeonSnapshot declarations")
string(FIND "${DIRECT_HOST_ENTRY_SOURCE}"
    "HostValidationRuntime::create(config, loaded.status)"
    VALIDATION_RUNTIME_OWNER_INDEX)
string(FIND "${DIRECT_HOST_ENTRY_SOURCE}"
    "if (validation_runtime == nullptr)" VALIDATION_RUNTIME_NULL_INDEX)
string(FIND "${DIRECT_HOST_ENTRY_SOURCE}"
    "HostWindowLifetime window{raylib_host_window_backend()};"
    WINDOW_OWNER_INDEX)
string(FIND "${DIRECT_HOST_ENTRY_SOURCE}"
    "std::unique_ptr<CombatRenderer> renderer_storage;"
    RENDERER_OWNER_INDEX)
string(FIND "${DIRECT_HOST_ENTRY_SOURCE}" "try {" HOST_TRY_INDEX)
evidence_window_lifetime_boundary_is_valid(
    "${DIRECT_HOST_ENTRY_SOURCE}"
    "${ACTIVE_SANITIZED_HOST_WINDOW_LIFETIME_SOURCE}"
    "${SANITIZED_HOST_WINDOW_LIFETIME_SOURCE}"
    WINDOW_LIFETIME_BOUNDARY_VALID)
if(NOT WINDOW_LIFETIME_BOUNDARY_VALID)
    message(FATAL_ERROR
        "Host must delegate the ordered window initialization boundary exactly once")
endif()
get_filename_component(RAYLIB_REPO_ROOT
    "${RAYLIB_SOURCE_DIR}/../../.." ABSOLUTE)
evidence_collect_project_production_code_like_files(
    "${RAYLIB_REPO_ROOT}" RAYLIB_PRODUCTION_SOURCES)
foreach(RAYLIB_PRODUCTION_SOURCE IN LISTS RAYLIB_PRODUCTION_SOURCES)
    file(READ "${RAYLIB_PRODUCTION_SOURCE}" RAYLIB_PRODUCTION_TEXT)
    arpg_sanitize_cpp_source("${RAYLIB_PRODUCTION_TEXT}"
        RAYLIB_PRODUCTION_LEXICAL)
    mask_cpp_inactive_preprocessor_regions(
        "${RAYLIB_PRODUCTION_LEXICAL}" RAYLIB_PRODUCTION_ACTIVE)
    evidence_raylib_lifecycle_source_role(
        "${RAYLIB_PRODUCTION_SOURCE}" "${RAYLIB_REPO_ROOT}"
        RAYLIB_LIFECYCLE_SOURCE_ROLE)
    evidence_window_lifecycle_source_surface_is_valid(
        "${RAYLIB_PRODUCTION_ACTIVE}" "${RAYLIB_PRODUCTION_LEXICAL}"
        "${RAYLIB_LIFECYCLE_SOURCE_ROLE}" WINDOW_LIFETIME_OWNER_VALID)
    if(NOT WINDOW_LIFETIME_OWNER_VALID)
        message(FATAL_ERROR
            "project lifecycle ownership violation: ${RAYLIB_PRODUCTION_SOURCE}")
    endif()
endforeach()
string(FIND "${DIRECT_HOST_ENTRY_SOURCE}"
    "window.initialize(config, committed_settings)"
    WINDOW_INITIALIZE_INDEX)
string(FIND "${DIRECT_HOST_ENTRY_SOURCE}"
    "renderer_storage = std::make_unique<CombatRenderer>()"
    RENDERER_ALLOCATION_INDEX)
if(VALIDATION_RUNTIME_OWNER_INDEX EQUAL -1
        OR VALIDATION_RUNTIME_NULL_INDEX EQUAL -1
        OR WINDOW_OWNER_INDEX EQUAL -1
        OR RENDERER_OWNER_INDEX EQUAL -1
        OR HOST_TRY_INDEX EQUAL -1
        OR WINDOW_INITIALIZE_INDEX EQUAL -1
        OR RENDERER_ALLOCATION_INDEX EQUAL -1
        OR NOT WINDOW_OWNER_INDEX LESS RENDERER_OWNER_INDEX
        OR NOT RENDERER_OWNER_INDEX LESS HOST_TRY_INDEX
        OR NOT HOST_TRY_INDEX LESS VALIDATION_RUNTIME_OWNER_INDEX
        OR NOT VALIDATION_RUNTIME_OWNER_INDEX LESS VALIDATION_RUNTIME_NULL_INDEX
        OR NOT VALIDATION_RUNTIME_NULL_INDEX LESS WINDOW_INITIALIZE_INDEX
        OR NOT WINDOW_INITIALIZE_INDEX LESS RENDERER_ALLOCATION_INDEX)
    message(FATAL_ERROR
        "renderer ownership, HostValidationRuntime, window, and renderer allocation order is invalid")
endif()
foreach(AUTOMATIC_VALIDATION_STATE IN ITEMS
        "Stage10ValidationState stage10_validation_state{}"
        "Stage11ValidationState stage11_validation_state{}"
        "Stage11BValidationState stage11b_validation_state{}"
        "Stage11CHudValidationState stage11c_validation_state{}"
        "Stage11DLootValidationState stage11d_validation_state{}")
    string(FIND "${SANITIZED_HOST_SOURCE}"
        "${AUTOMATIC_VALIDATION_STATE}" AUTOMATIC_VALIDATION_STATE_INDEX)
    if(NOT AUTOMATIC_VALIDATION_STATE_INDEX EQUAL -1)
        message(FATAL_ERROR
            "raylib host validation states must not return to the stack")
    endif()
endforeach()

set(PRODUCTION_FINAL_TARGET_CLEAR_GUARD [=[
handle_player_defeat();

        if (phase_ == RoomPhase::combat) {
            if (!room_progress_.exits_unlocked
                    && room_progress_.required_kills != 0U
                    && room_progress_.defeated_monster_count
                        >= room_progress_.required_kills) {
                prepare_room_unlock();
            } else if (remaining_targets() == 0U) {
                prepare_room_clear();
            }
        }
]=])
string(FIND "${DIRECT_DUNGEON_TICK_SOURCE}"
    "${PRODUCTION_FINAL_TARGET_CLEAR_GUARD}"
    PRODUCTION_FINAL_TARGET_CLEAR_GUARD_INDEX)
direct_cpp_fragment_present(
    "${DUNGEON_TICK_SOURCE}" "${PRODUCTION_FINAL_TARGET_CLEAR_GUARD}"
    PRODUCTION_FINAL_TARGET_CLEAR_GUARD_VALID)
if(PRODUCTION_FINAL_TARGET_CLEAR_GUARD_INDEX EQUAL -1
        OR NOT PRODUCTION_FINAL_TARGET_CLEAR_GUARD_VALID)
    message(FATAL_ERROR
        "production room-clear must preserve threshold unlock and final-target gates")
endif()
string(LENGTH "${PRODUCTION_FINAL_TARGET_CLEAR_GUARD}"
    PRODUCTION_FINAL_TARGET_CLEAR_GUARD_LENGTH)
math(EXPR PRODUCTION_FINAL_TARGET_CLEAR_GUARD_END
    "${PRODUCTION_FINAL_TARGET_CLEAR_GUARD_INDEX} + ${PRODUCTION_FINAL_TARGET_CLEAR_GUARD_LENGTH}")
string(SUBSTRING "${DIRECT_DUNGEON_TICK_SOURCE}"
    ${PRODUCTION_FINAL_TARGET_CLEAR_GUARD_END} -1
    DUNGEON_SESSION_AFTER_PRODUCTION_CLEAR)
string(FIND "${DUNGEON_SESSION_AFTER_PRODUCTION_CLEAR}"
    "${PRODUCTION_FINAL_TARGET_CLEAR_GUARD}"
    DUPLICATE_PRODUCTION_FINAL_TARGET_CLEAR_GUARD_INDEX)
if(NOT DUPLICATE_PRODUCTION_FINAL_TARGET_CLEAR_GUARD_INDEX EQUAL -1)
    message(FATAL_ERROR
        "production room-clear final-target gate must remain unique")
endif()
set(HEALTH_POTION_RETRY_GUARD [=[
if (phase_ != RoomPhase::combat || pending_save_.has_value()
                || !combat_.has_value() || remaining_targets() != 0U) {
            enter_fault(DungeonFault::save_receipt_mismatch);
        } else {
            prepare_room_clear();
        }
]=])
string(FIND "${DIRECT_DUNGEON_TICK_SOURCE}"
    "${HEALTH_POTION_RETRY_GUARD}" HEALTH_POTION_RETRY_GUARD_INDEX)
direct_cpp_fragment_present(
    "${DUNGEON_TICK_SOURCE}" "${HEALTH_POTION_RETRY_GUARD}"
    HEALTH_POTION_RETRY_GUARD_VALID)
if(HEALTH_POTION_RETRY_GUARD_INDEX EQUAL -1
        OR NOT HEALTH_POTION_RETRY_GUARD_VALID)
    message(FATAL_ERROR
        "health-potion retry must reject live targets before room clear")
endif()

set(ROOM_CLEAR_PRECONDITION_GUARD [=[
if (!combat_.has_value() || pending_save_.has_value()
            || !room_progress_.exits_unlocked
            || room_progress_.initial_monster_count == 0U
            || room_progress_.defeated_monster_count
                != room_progress_.initial_monster_count
            || remaining_targets() != 0U) {
        enter_fault(started_abyss
            ? DungeonFault::invalid_abyss_state
            : DungeonFault::invalid_item_state);
        return;
    }
]=])
string(FIND "${DIRECT_PREPARE_ROOM_CLEAR_SOURCE}"
    "${ROOM_CLEAR_PRECONDITION_GUARD}" ROOM_CLEAR_PRECONDITION_GUARD_INDEX)
direct_cpp_fragment_present(
    "${PREPARE_ROOM_CLEAR_SOURCE}" "${ROOM_CLEAR_PRECONDITION_GUARD}"
    ROOM_CLEAR_PRECONDITION_GUARD_VALID)
if(ROOM_CLEAR_PRECONDITION_GUARD_INDEX EQUAL -1
        OR NOT ROOM_CLEAR_PRECONDITION_GUARD_VALID)
    message(FATAL_ERROR
        "room clear must require threshold unlock and a full clear")
endif()

set(STAGE17_AUTHORITATIVE_LOADOUT_GUARD [=[
if (state.step == Stage17ValidationStep::open_inventory
            && inventory.is_open()) {
        state.step = Stage17ValidationStep::open_skill_page;
    } else if (state.step == Stage17ValidationStep::remove_slot_1
            && current.skill_loadout.slots[0].active
                == skills::ActiveSkillId::none) {
        state.step = Stage17ValidationStep::select_draw_inventory;
    } else if (state.step == Stage17ValidationStep::equip_slot_5
            && current.skill_loadout.slots[4].active
                == skills::ActiveSkillId::draw_slash) {
        state.step = Stage17ValidationStep::swap_slots_2_5;
    } else if (state.step == Stage17ValidationStep::swap_slots_2_5
            && stage17_final_loadout(current.skill_loadout)) {
        state.final_loadout = current.skill_loadout;
        state.production_transactions =
            !current.pending_save_kind.has_value();
        if (state.production_transactions) {
            state.step = Stage17ValidationStep::close_inventory;
        }
    } else if (state.step == Stage17ValidationStep::close_inventory
            && !inventory.is_open() && current.combat.has_value()) {
        state.production_inventory_closed = true;
        state.production_cooldown_start_ticks =
            current.combat->skill_cooldowns[0U];
        state.production_cooldown_wait_start_tick = current.combat->tick;
        state.production_cooldown_wait_started =
            state.production_cooldown_start_ticks != 0U;
        state.step = Stage17ValidationStep::cooldown_drain;
]=])
string(FIND "${DIRECT_STAGE17_INVENTORY_SOURCE}"
    "${STAGE17_AUTHORITATIVE_LOADOUT_GUARD}"
    STAGE17_AUTHORITATIVE_LOADOUT_GUARD_INDEX)
if(STAGE17_AUTHORITATIVE_LOADOUT_GUARD_INDEX EQUAL -1)
    message(FATAL_ERROR
        "Stage17 must wait for each authoritative loadout publication")
endif()

set(STAGE17_NATURAL_COOLDOWN_DRAIN_GUARD [=[
if (state.step == Stage17ValidationStep::cooldown_drain
            && state.production_cooldown_wait_started
            && stage17_all_cooldowns_zero(combat_state)) {
        state.production_cooldown_wait_end_tick = combat_state.tick;
        state.production_cooldowns_zero_before_shutdown =
            state.production_cooldown_wait_end_tick
                > state.production_cooldown_wait_start_tick;
        state.step = Stage17ValidationStep::complete;
    }
]=])
string(FIND "${DIRECT_STAGE17_SNAPSHOT_SOURCE}"
    "${STAGE17_NATURAL_COOLDOWN_DRAIN_GUARD}"
    STAGE17_NATURAL_COOLDOWN_DRAIN_GUARD_INDEX)
if(STAGE17_NATURAL_COOLDOWN_DRAIN_GUARD_INDEX EQUAL -1)
    message(FATAL_ERROR
        "Stage17 must naturally drain cooldown through authoritative fixed ticks before shutdown")
endif()
require_match_count(
    "${DIRECT_STAGE17_INJECT_SOURCE}"
    "case[ \t\r\n]+Stage17ValidationStep::cooldown_drain:[ \t\r\n]+break"
    1
    "Stage17 cooldown drain injects empty gameplay input")
set(STAGE17_EXACT_SHUTDOWN_READY_GUARD [=[
impl_->states.stage17.clean_shutdown_exact_ready =
        clean_shutdown_state == CleanShutdownState::ready;
]=])
string(FIND "${DIRECT_HOST_VALIDATION_SUMMARY_SOURCE}"
    "${STAGE17_EXACT_SHUTDOWN_READY_GUARD}"
    STAGE17_EXACT_SHUTDOWN_READY_GUARD_INDEX)
if(STAGE17_EXACT_SHUTDOWN_READY_GUARD_INDEX EQUAL -1)
    message(FATAL_ERROR
        "Stage17 summary must record completed clean-shutdown exact save")
endif()
set(HOST_VALIDATION_SUMMARY_CALL_GUARD [=[
validation_runtime->write_summaries(
            runtime.clean_shutdown_state(), pause_menu);
]=])
string(FIND "${DIRECT_HOST_ENTRY_SOURCE}"
    "${HOST_VALIDATION_SUMMARY_CALL_GUARD}"
    HOST_VALIDATION_SUMMARY_CALL_GUARD_INDEX)
if(HOST_VALIDATION_SUMMARY_CALL_GUARD_INDEX EQUAL -1)
    message(FATAL_ERROR
        "Host must forward final clean-shutdown state to validation summaries")
endif()

set(STAGE17_STORM_LOCK_ONCE_GUARD [=[
if (!state.storm_approach_locked) {
        const Stage17IsolatedStormTarget target =
            stage17_isolated_storm_target(combat_state);
        if (target.monster == nullptr) return false;
        state.storm_approach_center = target.projected_center;
        state.storm_approach_target_start = target.monster->position;
        state.storm_approach_player_start = combat_state.player.position;
        state.storm_approach_initial_clearance =
            std::sqrt(target.clearance_squared);
        state.storm_approach_target_ordinal = target.monster->monster_ordinal;
        state.storm_approach_locked = true;
    }
]=])
string(FIND "${DIRECT_STAGE17_PREPARE_STORM_SOURCE}" "${STAGE17_STORM_LOCK_ONCE_GUARD}"
    STAGE17_STORM_LOCK_ONCE_GUARD_INDEX)
if(STAGE17_STORM_LOCK_ONCE_GUARD_INDEX EQUAL -1)
    message(FATAL_ERROR
        "Stage17 storm isolation must be selected and locked exactly once")
endif()
set(STAGE17_STORM_FAIL_CLOSED_GUARD [=[
if (targets_in_finisher != 1U || !locked_target_in_finisher) {
        state.storm_isolation_invalidated = true;
        state.storm_isolation_invalidated_frame = state.storm_approach_frames;
        state.storm_isolation_invalidated_target_count = targets_in_finisher;
        return false;
    }
]=])
string(FIND "${DIRECT_STAGE17_PREPARE_STORM_SOURCE}" "${STAGE17_STORM_FAIL_CLOSED_GUARD}"
    STAGE17_STORM_FAIL_CLOSED_GUARD_INDEX)
if(STAGE17_STORM_FAIL_CLOSED_GUARD_INDEX EQUAL -1)
    message(FATAL_ERROR
        "Stage17 locked storm isolation must fail closed instead of reselecting")
endif()
set(STAGE17_STORM_PRELUDE_GUARD [=[
if (config.stage17_skill_stones_validation
                == Stage17SkillStonesValidationScenario::storm_sequence) {
            state.suppress_draw_captures = true;
            state.step = Stage17ValidationStep::approach_draw;
]=])
string(FIND "${DIRECT_STAGE17_SNAPSHOT_SOURCE}" "${STAGE17_STORM_PRELUDE_GUARD}"
    STAGE17_STORM_PRELUDE_GUARD_INDEX)
if(STAGE17_STORM_PRELUDE_GUARD_INDEX EQUAL -1)
    message(FATAL_ERROR
        "Stage17 storm scenario must use an explicit draw prelude")
endif()
set(STAGE17_DRAW_TO_TRANSACTIONS_GUARD [=[
} else if (state.draw_windup_captured && state.draw_captured) {
            state.step = Stage17ValidationStep::open_inventory;
]=])
string(FIND "${DIRECT_STAGE17_SNAPSHOT_SOURCE}" "${STAGE17_DRAW_TO_TRANSACTIONS_GUARD}"
    STAGE17_DRAW_TO_TRANSACTIONS_GUARD_INDEX)
if(STAGE17_DRAW_TO_TRANSACTIONS_GUARD_INDEX EQUAL -1)
    message(FATAL_ERROR
        "Stage17 production draw must proceed directly to authoritative transactions")
endif()
set(STAGE17_PRELUDE_HIT_GATE_GUARD [=[
if (state.draw_accepted && state.draw_hit_count == 2U) {
                state.step = Stage17ValidationStep::approach_storm;
]=])
string(FIND "${DIRECT_STAGE17_SNAPSHOT_SOURCE}" "${STAGE17_PRELUDE_HIT_GATE_GUARD}"
    STAGE17_PRELUDE_HIT_GATE_GUARD_INDEX)
if(STAGE17_PRELUDE_HIT_GATE_GUARD_INDEX EQUAL -1)
    message(FATAL_ERROR
        "Stage17 storm prelude must complete exactly two production hits")
endif()
set(STAGE17_STORM_THREAT_PULL_GUARD [=[
if (state.storm_threat_pull_locked) {
            stage17_apply_movement(
                snapshot, input_settings, state.storm_threat_pull_movement);
        }
]=])
string(FIND "${DIRECT_STAGE17_INJECT_SOURCE}"
    "${STAGE17_STORM_THREAT_PULL_GUARD}"
    STAGE17_STORM_THREAT_PULL_GUARD_INDEX)
if(STAGE17_STORM_THREAT_PULL_GUARD_INDEX EQUAL -1)
    message(FATAL_ERROR
        "Stage17 storm threat pull must remain fixed normal movement input")
endif()
set(STAGE17_RENDERER_OBSERVER_GUARD [=[
if (config.stage17_skill_stones_validation
            == Stage17SkillStonesValidationScenario::none
        || config.stage17_skill_stones_validation
            == Stage17SkillStonesValidationScenario::restarted_loadout
        || !presented.combat.has_value()) {
]=])
string(FIND "${DIRECT_STAGE17_DRAW_RUNTIME_SOURCE}" "${STAGE17_RENDERER_OBSERVER_GUARD}"
    STAGE17_RENDERER_OBSERVER_GUARD_INDEX)
if(STAGE17_RENDERER_OBSERVER_GUARD_INDEX EQUAL -1)
    message(FATAL_ERROR
        "Stage17 renderer evidence must observe production and storm scenarios")
endif()

function(require_direct_scope_mutations_rejected
        FUNCTION_SOURCE FRAGMENT MUTATION_PREFIX)
    set(ACCEPTED_MUTATIONS)
    foreach(MUTATION_KIND IN ITEMS
            uncalled-lambda dead-if-braced dead-if-unbraced
            dead-zero-u-braced dead-not-true-braced dead-do-while)
        if(MUTATION_KIND STREQUAL "uncalled-lambda")
            set(MUTATION_REPLACEMENT
                "const auto evidence_spoof = [&]() {\n${FRAGMENT}\n};")
        elseif(MUTATION_KIND STREQUAL "dead-if-braced")
            set(MUTATION_REPLACEMENT "if (false) {\n${FRAGMENT}\n}")
        elseif(MUTATION_KIND STREQUAL "dead-if-unbraced")
            set(MUTATION_REPLACEMENT "if (false)\n${FRAGMENT}")
        elseif(MUTATION_KIND STREQUAL "dead-zero-u-braced")
            set(MUTATION_REPLACEMENT "if (0U) {\n${FRAGMENT}\n}")
        elseif(MUTATION_KIND STREQUAL "dead-not-true-braced")
            set(MUTATION_REPLACEMENT "if (!true) {\n${FRAGMENT}\n}")
        else()
            set(MUTATION_REPLACEMENT
                "if (false) do {\n${FRAGMENT}\n} while (false);")
        endif()
        string(REPLACE "${FRAGMENT}" "${MUTATION_REPLACEMENT}"
            MUTATED_FUNCTION_SOURCE "${FUNCTION_SOURCE}")
        if(MUTATED_FUNCTION_SOURCE STREQUAL FUNCTION_SOURCE)
            message(FATAL_ERROR
                "${MUTATION_PREFIX}-${MUTATION_KIND} mutation setup did not modify its function")
        endif()
        direct_cpp_fragment_present(
            "${MUTATED_FUNCTION_SOURCE}" "${FRAGMENT}" MUTATION_VALID)
        if(MUTATION_VALID)
            list(APPEND ACCEPTED_MUTATIONS
                "${MUTATION_PREFIX}-${MUTATION_KIND}")
        endif()
    endforeach()
    if(ACCEPTED_MUTATIONS)
        list(JOIN ACCEPTED_MUTATIONS ", " ACCEPTED_MUTATION_NAMES)
        message(FATAL_ERROR
            "direct executable guard accepted mutations: ${ACCEPTED_MUTATION_NAMES}")
    endif()
endfunction()

function(require_cross_scope_mutation_rejected
        SANITIZED_SOURCE SIGNATURE FRAGMENT MUTATION_NAME)
    string(REPLACE "${FRAGMENT}" ""
        MUTATED_SOURCE "${SANITIZED_SOURCE}")
    if(MUTATED_SOURCE STREQUAL SANITIZED_SOURCE)
        message(FATAL_ERROR
            "${MUTATION_NAME} mutation setup did not remove target evidence")
    endif()
    string(APPEND MUTATED_SOURCE
        "\nvoid evidence_cross_scope_spoof() {\n${FRAGMENT}\n}\n")
    try_extract_cpp_function_definition_from_sanitized(
        "${MUTATED_SOURCE}" "${SIGNATURE}"
        MUTATED_FUNCTION MUTATED_FUNCTION_VALID)
    if(NOT MUTATED_FUNCTION_VALID)
        message(FATAL_ERROR
            "${MUTATION_NAME} mutation setup lost the target function definition")
    endif()
    direct_cpp_fragment_present(
        "${MUTATED_FUNCTION}" "${FRAGMENT}" MUTATION_VALID)
    if(MUTATION_VALID)
        message(FATAL_ERROR
            "direct executable guard accepted mutation: ${MUTATION_NAME}")
    endif()
endfunction()

function(require_inactive_preprocessor_mutations_rejected
        SANITIZED_SOURCE SIGNATURE FRAGMENT MUTATION_PREFIX)
    set(ACCEPTED_MUTATIONS)
    foreach(MUTATION_KIND IN ITEMS
            ordinary digraph nested unknown-first-elif-zero
            unknown-first-elif-one-else hex-zero)
        if(MUTATION_KIND STREQUAL "ordinary")
            set(MUTATION_REPLACEMENT
                "#if(0)\n${FRAGMENT}\n#endif")
        elseif(MUTATION_KIND STREQUAL "digraph")
            set(MUTATION_REPLACEMENT
                "%:if(0)\n${FRAGMENT}\n%:endif")
        elseif(MUTATION_KIND STREQUAL "nested")
            set(MUTATION_REPLACEMENT
                "#if 0\n#if 0\n#else\n${FRAGMENT}\n#endif\n#elif false\n${FRAGMENT}\n#else\n#endif")
        elseif(MUTATION_KIND STREQUAL "unknown-first-elif-zero")
            set(MUTATION_REPLACEMENT
                "#if ARPG_REVIEW_UNKNOWN\n#elif 0\n${FRAGMENT}\n#else\n#endif")
        elseif(MUTATION_KIND STREQUAL "unknown-first-elif-one-else")
            set(MUTATION_REPLACEMENT
                "#if ARPG_REVIEW_UNKNOWN\n#elif 1\n#else\n${FRAGMENT}\n#endif")
        else()
            set(MUTATION_REPLACEMENT
                "#if 0x0\n${FRAGMENT}\n#endif")
        endif()
        string(REPLACE "${FRAGMENT}" "${MUTATION_REPLACEMENT}"
            MUTATED_SOURCE "${SANITIZED_SOURCE}")
        if(MUTATED_SOURCE STREQUAL SANITIZED_SOURCE)
            message(FATAL_ERROR
                "${MUTATION_PREFIX}-${MUTATION_KIND} inactive mutation setup did not modify source")
        endif()
        mask_cpp_inactive_preprocessor_regions(
            "${MUTATED_SOURCE}" ACTIVE_MUTATED_SOURCE)
        try_extract_cpp_function_definition_from_sanitized(
            "${ACTIVE_MUTATED_SOURCE}" "${SIGNATURE}"
            MUTATED_FUNCTION MUTATED_FUNCTION_VALID)
        if(NOT MUTATED_FUNCTION_VALID)
            message(FATAL_ERROR
                "${MUTATION_PREFIX}-${MUTATION_KIND} inactive mutation lost target function")
        endif()
        direct_cpp_fragment_present(
            "${MUTATED_FUNCTION}" "${FRAGMENT}" MUTATION_VALID)
        if(MUTATION_VALID)
            list(APPEND ACCEPTED_MUTATIONS
                "${MUTATION_PREFIX}-${MUTATION_KIND}")
        endif()
    endforeach()
    if(ACCEPTED_MUTATIONS)
        list(JOIN ACCEPTED_MUTATIONS ", " ACCEPTED_MUTATION_NAMES)
        message(FATAL_ERROR
            "inactive preprocessor guard accepted mutations: ${ACCEPTED_MUTATION_NAMES}")
    endif()
endfunction()

function(require_mutually_exclusive_fragment_split_rejected
        FUNCTION_SOURCE SIGNATURE FRAGMENT MUTATION_NAME)
    string(FIND "${FRAGMENT}" "\n\n" SPLIT_INDEX)
    if(SPLIT_INDEX LESS 1)
        message(FATAL_ERROR
            "${MUTATION_NAME} requires a non-empty first statement before a double newline")
    endif()
    math(EXPR SECOND_BEGIN "${SPLIT_INDEX} + 2")
    string(LENGTH "${FRAGMENT}" FRAGMENT_LENGTH)
    if(SECOND_BEGIN GREATER_EQUAL FRAGMENT_LENGTH)
        message(FATAL_ERROR
            "${MUTATION_NAME} requires a non-empty fragment after the double newline")
    endif()
    string(SUBSTRING "${FRAGMENT}" 0 ${SPLIT_INDEX} FIRST_BRANCH_FRAGMENT)
    string(SUBSTRING "${FRAGMENT}" ${SECOND_BEGIN} -1 SECOND_BRANCH_FRAGMENT)
    set(MUTATION_REPLACEMENT
        "#if ARPG_REVIEW_UNKNOWN\n${FIRST_BRANCH_FRAGMENT}\n#else\n${SECOND_BRANCH_FRAGMENT}\n#endif")
    string(REPLACE "${FRAGMENT}" "${MUTATION_REPLACEMENT}"
        MUTATED_SOURCE "${FUNCTION_SOURCE}")
    if(MUTATED_SOURCE STREQUAL FUNCTION_SOURCE)
        message(FATAL_ERROR
            "${MUTATION_NAME} mutation setup did not split the production fragment")
    endif()
    mask_cpp_inactive_preprocessor_regions(
        "${MUTATED_SOURCE}" ACTIVE_MUTATED_SOURCE)
    try_extract_cpp_function_definition_from_sanitized(
        "${ACTIVE_MUTATED_SOURCE}" "${SIGNATURE}"
        MUTATED_FUNCTION MUTATED_FUNCTION_VALID)
    if(NOT MUTATED_FUNCTION_VALID)
        message(FATAL_ERROR
            "${MUTATION_NAME} mutation setup lost the target function")
    endif()
    direct_cpp_fragment_present(
        "${MUTATED_FUNCTION}" "${FRAGMENT}" MUTATION_VALID)
    if(MUTATION_VALID)
        message(FATAL_ERROR
            "inactive preprocessor guard accepted mutation: ${MUTATION_NAME}")
    endif()
endfunction()

function(require_inactive_function_definition_rejected
        FUNCTION_SOURCE SIGNATURE MUTATION_NAME)
    set(MUTATED_SOURCE
        "#if 0\n${FUNCTION_SOURCE}\n#else\nvoid evidence_active_branch() {}\n#endif")
    mask_cpp_inactive_preprocessor_regions(
        "${MUTATED_SOURCE}" ACTIVE_MUTATED_SOURCE)
    try_extract_cpp_function_definition_from_sanitized(
        "${ACTIVE_MUTATED_SOURCE}" "${SIGNATURE}"
        MUTATED_FUNCTION MUTATED_FUNCTION_VALID)
    if(MUTATED_FUNCTION_VALID)
        message(FATAL_ERROR
            "inactive preprocessor guard accepted mutation: ${MUTATION_NAME}")
    endif()
endfunction()

function(require_spliced_inactive_preprocessor_mutations_rejected)
    string(ASCII 10 LF)
    string(ASCII 13 CR)
    string(ASCII 92 BACKSLASH)
    set(SPLICE_FRAGMENT "evidence_marker();")
    set(ACCEPTED_MUTATIONS)
    foreach(MUTATION_KIND IN ITEMS spliced-lf spliced-crlf)
        if(MUTATION_KIND STREQUAL "spliced-lf")
            set(SPLICE_EOL "${LF}")
        else()
            set(SPLICE_EOL "${CR}${LF}")
        endif()
        set(RAW_FIXTURE
            "void evidence_splice_fixture() {\n#${BACKSLASH}${SPLICE_EOL}if 0\n${SPLICE_FRAGMENT}\n#${BACKSLASH}${SPLICE_EOL}endif\n}\n")
        arpg_sanitize_cpp_source("${RAW_FIXTURE}" SANITIZED_FIXTURE)
        string(FIND "${SANITIZED_FIXTURE}" "#if 0" FOLDED_IF_INDEX)
        string(FIND "${SANITIZED_FIXTURE}" "#endif" FOLDED_ENDIF_INDEX)
        if(FOLDED_IF_INDEX EQUAL -1 OR FOLDED_ENDIF_INDEX EQUAL -1)
            message(FATAL_ERROR
                "inactive preprocessor ${MUTATION_KIND} setup did not fold phase-2 splice")
        endif()
        mask_cpp_inactive_preprocessor_regions(
            "${SANITIZED_FIXTURE}" ACTIVE_FIXTURE)
        try_extract_cpp_function_definition_from_sanitized(
            "${ACTIVE_FIXTURE}" "void evidence_splice_fixture("
            FIXTURE_FUNCTION FIXTURE_FUNCTION_VALID)
        if(NOT FIXTURE_FUNCTION_VALID)
            message(FATAL_ERROR
                "inactive preprocessor ${MUTATION_KIND} setup lost fixture function")
        endif()
        direct_cpp_fragment_present(
            "${FIXTURE_FUNCTION}" "${SPLICE_FRAGMENT}" MUTATION_VALID)
        if(MUTATION_VALID)
            list(APPEND ACCEPTED_MUTATIONS
                "inactive-preprocessor-${MUTATION_KIND}")
        endif()
    endforeach()
    if(ACCEPTED_MUTATIONS)
        list(JOIN ACCEPTED_MUTATIONS ", " ACCEPTED_MUTATION_NAMES)
        message(FATAL_ERROR
            "inactive preprocessor guard accepted mutations: ${ACCEPTED_MUTATION_NAMES}")
    endif()
endfunction()

# Named negative controls prove that semantic evidence cannot be borrowed from
# non-executed or unrelated scopes.
require_direct_scope_mutations_rejected(
    "${STAGE17_PREPARE_STORM_SOURCE}"
    "${STAGE17_STORM_LOCK_ONCE_GUARD}"
    "stage17-storm-lock")
require_direct_scope_mutations_rejected(
    "${STAGE17_SNAPSHOT_SOURCE}"
    "${STAGE17_NATURAL_COOLDOWN_DRAIN_GUARD}"
    "stage17-cooldown-drain")
require_direct_scope_mutations_rejected(
    "${HOST_VALIDATION_SUMMARY_SOURCE}"
    "${STAGE17_EXACT_SHUTDOWN_READY_GUARD}"
    "stage17-exact-shutdown")
require_direct_scope_mutations_rejected(
    "${SANITIZED_HOST_ENTRY_SOURCE}"
    "${HOST_VALIDATION_SUMMARY_CALL_GUARD}"
    "host-validation-summary-call")
require_cross_scope_mutation_rejected(
    "${ACTIVE_SANITIZED_STAGE17_RUNTIME_SOURCE}"
    "PhysicalKeySnapshot inject_stage17_physical_edges("
    "${STAGE17_STORM_THREAT_PULL_GUARD}"
    "stage17-storm-threat-pull-cross-scope")

set(STAGE17_PREPARE_STORM_FORWARD_MUTATION
    "bool stage17_prepare_isolated_storm();\n${ACTIVE_SANITIZED_STAGE17_RUNTIME_SOURCE}")
try_extract_cpp_function_definition_from_sanitized(
    "${STAGE17_PREPARE_STORM_FORWARD_MUTATION}"
    "bool stage17_prepare_isolated_storm("
    STAGE17_PREPARE_STORM_FORWARD_FUNCTION
    STAGE17_PREPARE_STORM_FORWARD_VALID)
if(STAGE17_PREPARE_STORM_FORWARD_VALID)
    message(FATAL_ERROR
        "function definition guard accepted mutation: stage17-prepare-isolated-storm-forward")
endif()

require_direct_scope_mutations_rejected(
    "${DUNGEON_TICK_SOURCE}"
    "${PRODUCTION_FINAL_TARGET_CLEAR_GUARD}"
    "dungeon-tick-final-target")
require_direct_scope_mutations_rejected(
    "${DUNGEON_TICK_SOURCE}"
    "${HEALTH_POTION_RETRY_GUARD}"
    "dungeon-tick-health-potion-retry")
require_direct_scope_mutations_rejected(
    "${PREPARE_ROOM_CLEAR_SOURCE}"
    "${ROOM_CLEAR_PRECONDITION_GUARD}"
    "dungeon-prepare-room-clear-precondition")
require_inactive_preprocessor_mutations_rejected(
    "${STAGE17_PREPARE_STORM_SOURCE}"
    "bool stage17_prepare_isolated_storm("
    "${STAGE17_STORM_LOCK_ONCE_GUARD}"
    "stage17-storm-lock-inactive")
require_inactive_preprocessor_mutations_rejected(
    "${DUNGEON_TICK_SOURCE}"
    "void DungeonSession::tick("
    "${HEALTH_POTION_RETRY_GUARD}"
    "dungeon-tick-health-potion-inactive")
require_inactive_preprocessor_mutations_rejected(
    "${PREPARE_ROOM_CLEAR_SOURCE}"
    "void DungeonSession::prepare_room_clear("
    "${ROOM_CLEAR_PRECONDITION_GUARD}"
    "dungeon-prepare-room-clear-inactive")
require_mutually_exclusive_fragment_split_rejected(
    "${DUNGEON_TICK_SOURCE}"
    "void DungeonSession::tick("
    "${PRODUCTION_FINAL_TARGET_CLEAR_GUARD}"
    "dungeon-tick-final-target-mutually-exclusive-branches")
require_inactive_function_definition_rejected(
    "${STAGE17_PREPARE_STORM_SOURCE}"
    "bool stage17_prepare_isolated_storm("
    "stage17-prepare-function-inactive")
require_inactive_function_definition_rejected(
    "${DUNGEON_TICK_SOURCE}"
    "void DungeonSession::tick("
    "dungeon-tick-function-inactive")
require_inactive_function_definition_rejected(
    "${PREPARE_ROOM_CLEAR_SOURCE}"
    "void DungeonSession::prepare_room_clear("
    "dungeon-prepare-room-clear-function-inactive")
require_spliced_inactive_preprocessor_mutations_rejected()

set(SOURCE_LINE_START "(^|\n)[ \t]*")
set(SOURCE_LINE_END "[ \t]*(\r?\n|$)")
set(POISON_INCLUDE_LINE_PATTERN
    "${SOURCE_LINE_START}#[ \t]*include[ \t]+\"direct_input_poison\\.hpp\"${SOURCE_LINE_END}")
set(ANY_INCLUDE_LINE_PATTERN
    "${SOURCE_LINE_START}#[ \t]*include[ \t]+")
set(ACTIVE_ASSERT_PATTERN
    "${SOURCE_LINE_START}static_assert[ \t]*\\([ \t]*arpg::platform::direct_input_poison::active[ \t]*(,|\\))")
set(SAMPLE_CALL_LINE_PATTERN
    "${SOURCE_LINE_START}const[ \t]+PhysicalKeySnapshot[ \t]+sampled_physical_keys[ \t]*=[ \t]*sample_physical_keys[ \t]*\\([ \t]*\\)")
set(STAGE11B_INJECT_CALL_LINE_PATTERN
    "${SOURCE_LINE_START}[ \t]*host_validation::inject_stage11b_physical_edges[ \t]*\\(")
set(STAGE11C_INJECT_CALL_LINE_PATTERN
    "${SOURCE_LINE_START}[ \t]*host_validation::inject_stage11c_physical_edges[ \t]*\\(")
set(STAGE11D_INJECT_CALL_LINE_PATTERN
    "${SOURCE_LINE_START}[ \t]*host_validation::inject_stage11d_physical_edges[ \t]*\\(")
set(STAGE17_INJECT_CALL_LINE_PATTERN
    "${SOURCE_LINE_START}[ \t]*return[ \t]+host_validation::inject_stage17_physical_edges[ \t]*\\(")
set(FACADE_INJECT_CALL_LINE_PATTERN
    "${SOURCE_LINE_START}[ \t]*validation_runtime->[ \t]*inject_physical_edges[ \t]*\\(")
set(CACHED_INPUT_ACCESS_LINE_PATTERN
    "${SOURCE_LINE_START}[ \t]*validation_runtime->[ \t]*death_input_snapshot[ \t]*\\(")
set(MAP_CALL_LINE_PATTERN
    "${SOURCE_LINE_START}HostFrameInput[ \t]+frame_input[ \t]*=[ \t]*map_host_frame_input[ \t]*\\(")
set(ACTIVE_SENTINEL_PATTERN
    "${SOURCE_LINE_START}inline[ \t]+constexpr[ \t]+bool[ \t]+active[ \t]*=[ \t]*true")

function(poison_macro_line_pattern API OUT_PATTERN)
    set("${OUT_PATTERN}"
        "(^|\n)#define[ \t]+${API}[ \t]+::arpg::platform::direct_input_poison::blocked${SOURCE_LINE_END}"
        PARENT_SCOPE)
endfunction()

set(COMMENT_ONLY_STRUCTURE [=[
// #include "direct_input_poison.hpp"
// static_assert(arpg::platform::direct_input_poison::active);
// const PhysicalKeySnapshot sampled_physical_keys = sample_physical_keys();
// const PhysicalKeySnapshot stage17_physical_keys =
//     validation_runtime->inject_physical_edges(
//         sampled_physical_keys, input_settings, current,
//         gameplay_rearm_was_required);
// const PhysicalKeySnapshot& physical_keys =
//     validation_runtime->death_input_snapshot();
// HostFrameInput frame_input = map_host_frame_input(settings, stage17_physical_keys);
// host_validation::inject_stage11b_physical_edges(
// host_validation::inject_stage11c_physical_edges(
// host_validation::inject_stage11d_physical_edges(
// return host_validation::inject_stage17_physical_edges(
// #define IsKeyDown ::arpg::platform::direct_input_poison::blocked
]=])
set(REAL_STRUCTURE [=[
#include "direct_input_poison.hpp"
static_assert(arpg::platform::direct_input_poison::active, "active");
const PhysicalKeySnapshot sampled_physical_keys = sample_physical_keys();
const bool gameplay_rearm_was_required =
    runtime.gameplay_rearm_required();
const PhysicalKeySnapshot stage17_physical_keys =
    validation_runtime->inject_physical_edges(
        sampled_physical_keys, input_settings, current,
        gameplay_rearm_was_required);
const PhysicalKeySnapshot& physical_keys =
    validation_runtime->death_input_snapshot();
if (gameplay_rearm_was_required
        && gameplay_controls_physically_released(stage17_physical_keys)) {
    runtime.acknowledge_gameplay_rearmed();
}
HostFrameInput frame_input = map_host_frame_input(
    input_settings, stage17_physical_keys);
DeathInputGate death_gate = host_death_input_gate(
    death_saving, death_pending, frame_input.keys, physical_keys);
const bool pause_blocks_gameplay = pause_open || pause_was_open;
const bool gameplay_armed = runtime.authority_requests_enabled()
    && !gameplay_rearm_was_required;
HostFrameGateResult host_gate{};
if (pause_open) {
    host_gate = gate_host_frame(fixed_step, pause_latched, true,
        static_cast<double>(frame_seconds));
} else if (!inventory.is_open() && !inventory_toggled_this_frame) {
    host_gate = gate_host_frame(fixed_step, pause_latched, false,
        static_cast<double>(frame_seconds));
}
const PassiveOverlayInputGate passive_input_gate = passive_overlay_input_gate(
    passive_overlay_open);
const InventoryInputGate inventory_gate = inventory_input_gate(
    inventory.is_open() || inventory_toggled_this_frame);
const bool forward_actions = passive_input_gate.forward_actions
    && inventory_gate.forward_actions
    && death_gate.forward_gameplay
    && host_gate.forward_gameplay && !pause_blocks_gameplay
    && gameplay_armed;
const bool forward_movement = passive_input_gate.forward_movement
    && inventory_gate.forward_movement
    && death_gate.forward_gameplay
    && host_gate.forward_gameplay && !pause_blocks_gameplay
    && gameplay_armed;
const bool forward_descent = passive_input_gate.forward_descent
    && inventory_gate.forward_descent
    && death_gate.forward_gameplay
    && host_gate.forward_gameplay && !pause_blocks_gameplay
    && gameplay_armed;
if (forward_actions) {
    const SubmittedFrameActions submitted_actions =
        submit_frame_actions(*session, frame_input);
}
if (forward_descent && frame_input.keys.e) {
    session->snapshot(current);
    const bool in_range = current.combat.has_value()
        && can_prompt_descent(current, current.combat->player.position);
    static_cast<void>(session->request_descent(in_range));
}
const combat::MovementInput movement = forward_movement
    ? frame_input.movement : combat::MovementInput{};
PhysicalKeySnapshot HostValidationRuntime::inject_physical_edges(
    PhysicalKeySnapshot snapshot, const settings::SettingsData& input_settings,
    const dungeon::DungeonSnapshot& dungeon_snapshot,
    bool gameplay_rearm_required) noexcept {
    const PhysicalKeySnapshot stage11b_physical_keys =
        host_validation::inject_stage11b_physical_edges(
            snapshot, *impl_->config, impl_->states.stage11b);
    const PhysicalKeySnapshot stage11c_physical_keys =
        host_validation::inject_stage11c_physical_edges(
            stage11b_physical_keys, *impl_->config, input_settings,
            dungeon_snapshot, impl_->states.stage11c);
    const PhysicalKeySnapshot stage11d_physical_keys =
        host_validation::inject_stage11d_physical_edges(
            stage11c_physical_keys, *impl_->config, input_settings,
            dungeon_snapshot, impl_->states.stage11d);
    impl_->death_input_snapshot = stage11d_physical_keys;
    impl_->states.stage17.suspend_injection = gameplay_rearm_required;
    return host_validation::inject_stage17_physical_edges(
        stage11d_physical_keys, *impl_->config, input_settings,
        dungeon_snapshot, impl_->states.stage17);
}
#define IsKeyDown ::arpg::platform::direct_input_poison::blocked
]=])
poison_macro_line_pattern(IsKeyDown SELF_TEST_MACRO_PATTERN)
foreach(STRUCTURE_PATTERN IN ITEMS
        POISON_INCLUDE_LINE_PATTERN
        ACTIVE_ASSERT_PATTERN
        SAMPLE_CALL_LINE_PATTERN
        FACADE_INJECT_CALL_LINE_PATTERN
        CACHED_INPUT_ACCESS_LINE_PATTERN
        STAGE11B_INJECT_CALL_LINE_PATTERN
        STAGE11C_INJECT_CALL_LINE_PATTERN
        STAGE11D_INJECT_CALL_LINE_PATTERN
        STAGE17_INJECT_CALL_LINE_PATTERN
        MAP_CALL_LINE_PATTERN
        SELF_TEST_MACRO_PATTERN)
    require_match_count(
        "${COMMENT_ONLY_STRUCTURE}"
        "${${STRUCTURE_PATTERN}}"
        0
        "commented ${STRUCTURE_PATTERN}")
    require_match_count(
        "${REAL_STRUCTURE}"
        "${${STRUCTURE_PATTERN}}"
        1
        "real ${STRUCTURE_PATTERN}")
endforeach()

function(require_poison_consumer SOURCE LABEL)
    require_match_count(
        "${SOURCE}"
        "${POISON_INCLUDE_LINE_PATTERN}"
        1
        "${LABEL} poison includes")
    string(REGEX MATCH
        "${POISON_INCLUDE_LINE_PATTERN}"
        POISON_INCLUDE_LINE
        "${SOURCE}")
    string(FIND "${SOURCE}" "${POISON_INCLUDE_LINE}" INCLUDE_INDEX)
    string(LENGTH "${POISON_INCLUDE_LINE}" INCLUDE_LENGTH)
    math(EXPR AFTER_INCLUDE "${INCLUDE_INDEX} + ${INCLUDE_LENGTH}")
    string(SUBSTRING "${SOURCE}" ${AFTER_INCLUDE} -1 INCLUDE_SUFFIX)
    if(INCLUDE_SUFFIX MATCHES "${ANY_INCLUDE_LINE_PATTERN}")
        message(FATAL_ERROR
            "${LABEL} poison must be the final normal include")
    endif()
    require_match_count(
        "${INCLUDE_SUFFIX}"
        "${ACTIVE_ASSERT_PATTERN}"
        1
        "${LABEL} active poison assertions")
endfunction()

require_poison_consumer("${HOST_SOURCE}" "raylib host")
require_poison_consumer("${INVENTORY_SOURCE}" "inventory renderer")
require_match_count(
    "${INPUT_AUTHORITY_SOURCE}"
    "${POISON_INCLUDE_LINE_PATTERN}"
    0
    "host input authority poison includes")

require_match_count(
    "${HOST_SOURCE}"
    "${SAMPLE_CALL_LINE_PATTERN}"
    1
    "host physical snapshot calls")
require_match_count(
    "${HOST_SOURCE}"
    "${FACADE_INJECT_CALL_LINE_PATTERN}"
    1
    "host validation facade physical injection calls")
require_match_count(
    "${HOST_SOURCE}"
    "${CACHED_INPUT_ACCESS_LINE_PATTERN}"
    1
    "host cached Stage11D physical snapshot access")
require_match_count(
    "${HOST_VALIDATION_RUNTIME_SOURCE}"
    "${STAGE11B_INJECT_CALL_LINE_PATTERN}"
    1
    "validation runtime Stage11B physical injection calls")
require_match_count(
    "${HOST_VALIDATION_RUNTIME_SOURCE}"
    "${STAGE11C_INJECT_CALL_LINE_PATTERN}"
    1
    "validation runtime Stage11C physical injection calls")
require_match_count(
    "${HOST_VALIDATION_RUNTIME_SOURCE}"
    "${STAGE11D_INJECT_CALL_LINE_PATTERN}"
    1
    "validation runtime Stage11D physical injection calls")
require_match_count(
    "${HOST_VALIDATION_RUNTIME_SOURCE}"
    "${STAGE17_INJECT_CALL_LINE_PATTERN}"
    1
    "validation runtime Stage17 physical injection calls")
require_match_count(
    "${HOST_SOURCE}"
    "${MAP_CALL_LINE_PATTERN}"
    1
    "host logical mapping calls")
require_match_count(
    "${SANITIZED_HOST_SOURCE}"
    "HostValidationStateAccess"
    0
    "removed host validation state-access shim")
require_match_count(
    "${SANITIZED_HOST_VALIDATION_RUNTIME_SOURCE}"
    "HostValidationStateAccess"
    0
    "removed runtime validation state-access shim")
foreach(OLD_DIRECT_INJECTOR IN ITEMS
        inject_stage11b_physical_edges inject_stage11c_physical_edges
        inject_stage11d_physical_edges inject_stage17_physical_edges)
    require_match_count(
        "${DIRECT_HOST_ENTRY_SOURCE}"
        "${OLD_DIRECT_INJECTOR}[ \t\r\n]*\\("
        0
        "host migrated ${OLD_DIRECT_INJECTOR} calls")
endforeach()

function(physical_input_chain_valid SOURCE OUT_VARIABLE)
    arpg_sanitize_cpp_source("${SOURCE}" SOURCE)
    set(WS "[ \t\r\n]*")
    set(WS1 "[ \t\r\n]+")
    string(FIND "${SOURCE}" "const PhysicalKeySnapshot sampled_physical_keys = sample_physical_keys()" SAMPLE_INDEX)
    string(FIND "${SOURCE}" "const bool gameplay_rearm_was_required =" REARM_INDEX)
    string(FIND "${SOURCE}" "const PhysicalKeySnapshot stage17_physical_keys =" FACADE_INDEX)
    string(FIND "${SOURCE}" "const PhysicalKeySnapshot& physical_keys =" CACHED_INDEX)
    string(FIND "${SOURCE}" "gameplay_controls_physically_released(" RELEASED_INDEX)
    string(FIND "${SOURCE}" "runtime.acknowledge_gameplay_rearmed()" ACK_INDEX)
    string(FIND "${SOURCE}" "HostFrameInput frame_input = map_host_frame_input(" MAP_INDEX)
    string(FIND "${SOURCE}" "DeathInputGate death_gate = host_death_input_gate(" DEATH_GATE_INDEX)
    string(FIND "${SOURCE}" "const bool pause_blocks_gameplay =" PAUSE_BLOCK_INDEX)
    string(FIND "${SOURCE}" "const bool gameplay_armed =" GAMEPLAY_ARMED_INDEX)
    string(FIND "${SOURCE}" "HostFrameGateResult host_gate{}" HOST_GATE_INDEX)
    string(FIND "${SOURCE}" "const PassiveOverlayInputGate passive_input_gate =" PASSIVE_GATE_INDEX)
    string(FIND "${SOURCE}" "const InventoryInputGate inventory_gate =" INVENTORY_GATE_INDEX)
    string(FIND "${SOURCE}" "const bool forward_actions =" FORWARD_ACTIONS_INDEX)
    string(FIND "${SOURCE}" "const bool forward_movement =" FORWARD_MOVEMENT_DECL_INDEX)
    string(FIND "${SOURCE}" "const bool forward_descent =" FORWARD_DESCENT_DECL_INDEX)
    string(FIND "${SOURCE}" "if (forward_actions) {" FORWARD_ACTIONS_IF_INDEX)
    string(FIND "${SOURCE}" "submit_frame_actions(*session, frame_input)" SUBMIT_ACTIONS_INDEX)
    string(FIND "${SOURCE}" "if (forward_descent && frame_input.keys.e)" FORWARD_DESCENT_INDEX)
    string(FIND "${SOURCE}" "session->request_descent(in_range)" REQUEST_DESCENT_INDEX)
    string(FIND "${SOURCE}" "const combat::MovementInput movement = forward_movement" MOVEMENT_INPUT_INDEX)
    if(SAMPLE_INDEX EQUAL -1 OR REARM_INDEX EQUAL -1 OR FACADE_INDEX EQUAL -1
            OR CACHED_INDEX EQUAL -1 OR RELEASED_INDEX EQUAL -1
            OR ACK_INDEX EQUAL -1
            OR MAP_INDEX EQUAL -1 OR DEATH_GATE_INDEX EQUAL -1
            OR PAUSE_BLOCK_INDEX EQUAL -1 OR GAMEPLAY_ARMED_INDEX EQUAL -1
            OR HOST_GATE_INDEX EQUAL -1 OR PASSIVE_GATE_INDEX EQUAL -1 OR INVENTORY_GATE_INDEX EQUAL -1
            OR FORWARD_ACTIONS_INDEX EQUAL -1 OR FORWARD_MOVEMENT_DECL_INDEX EQUAL -1
            OR FORWARD_DESCENT_DECL_INDEX EQUAL -1
            OR FORWARD_ACTIONS_IF_INDEX EQUAL -1 OR SUBMIT_ACTIONS_INDEX EQUAL -1
            OR FORWARD_DESCENT_INDEX EQUAL -1 OR REQUEST_DESCENT_INDEX EQUAL -1
            OR MOVEMENT_INPUT_INDEX EQUAL -1
            OR NOT SAMPLE_INDEX LESS REARM_INDEX
            OR NOT REARM_INDEX LESS FACADE_INDEX
            OR NOT FACADE_INDEX LESS CACHED_INDEX
            OR NOT CACHED_INDEX LESS RELEASED_INDEX
            OR NOT RELEASED_INDEX LESS ACK_INDEX
            OR NOT ACK_INDEX LESS MAP_INDEX
            OR NOT MAP_INDEX LESS DEATH_GATE_INDEX
            OR NOT DEATH_GATE_INDEX LESS PAUSE_BLOCK_INDEX
            OR NOT PAUSE_BLOCK_INDEX LESS GAMEPLAY_ARMED_INDEX
            OR NOT GAMEPLAY_ARMED_INDEX LESS HOST_GATE_INDEX
            OR NOT HOST_GATE_INDEX LESS PASSIVE_GATE_INDEX OR NOT PASSIVE_GATE_INDEX LESS INVENTORY_GATE_INDEX
            OR NOT INVENTORY_GATE_INDEX LESS FORWARD_ACTIONS_INDEX
            OR NOT FORWARD_ACTIONS_INDEX LESS FORWARD_MOVEMENT_DECL_INDEX
            OR NOT FORWARD_MOVEMENT_DECL_INDEX LESS FORWARD_DESCENT_DECL_INDEX
            OR NOT FORWARD_DESCENT_DECL_INDEX LESS FORWARD_ACTIONS_IF_INDEX
            OR NOT FORWARD_ACTIONS_IF_INDEX LESS SUBMIT_ACTIONS_INDEX
            OR NOT SUBMIT_ACTIONS_INDEX LESS FORWARD_DESCENT_INDEX
            OR NOT FORWARD_DESCENT_INDEX LESS REQUEST_DESCENT_INDEX
            OR NOT REQUEST_DESCENT_INDEX LESS MOVEMENT_INPUT_INDEX)
        set("${OUT_VARIABLE}" FALSE PARENT_SCOPE)
        return()
    endif()

    math(EXPR FACADE_LENGTH "${CACHED_INDEX} - ${FACADE_INDEX}")
    string(SUBSTRING "${SOURCE}" ${FACADE_INDEX} ${FACADE_LENGTH} FACADE_SOURCE)
    math(EXPR CACHED_LENGTH "${RELEASED_INDEX} - ${CACHED_INDEX}")
    string(SUBSTRING "${SOURCE}" ${CACHED_INDEX} ${CACHED_LENGTH} CACHED_SOURCE)
    math(EXPR RELEASED_LENGTH "${MAP_INDEX} - ${RELEASED_INDEX}")
    string(SUBSTRING "${SOURCE}" ${RELEASED_INDEX} ${RELEASED_LENGTH} RELEASED_SOURCE)
    string(SUBSTRING "${SOURCE}" ${MAP_INDEX} -1 MAP_SOURCE)
    math(EXPR HOST_GATE_LENGTH "${PASSIVE_GATE_INDEX} - ${HOST_GATE_INDEX}")
    string(SUBSTRING "${SOURCE}" ${HOST_GATE_INDEX} ${HOST_GATE_LENGTH} HOST_GATE_SOURCE)
    math(EXPR GAMEPLAY_ARMED_LENGTH "${HOST_GATE_INDEX} - ${GAMEPLAY_ARMED_INDEX}")
    string(SUBSTRING "${SOURCE}" ${GAMEPLAY_ARMED_INDEX}
        ${GAMEPLAY_ARMED_LENGTH} GAMEPLAY_ARMED_SOURCE)
    math(EXPR HOST_CHAIN_LENGTH "${MOVEMENT_INPUT_INDEX} - ${MAP_INDEX}")
    string(SUBSTRING "${SOURCE}" ${MAP_INDEX} ${HOST_CHAIN_LENGTH} HOST_CHAIN_SOURCE)
    math(EXPR FORWARD_ACTIONS_LENGTH "${FORWARD_ACTIONS_IF_INDEX} - ${FORWARD_ACTIONS_INDEX}")
    string(SUBSTRING "${SOURCE}" ${FORWARD_ACTIONS_INDEX} ${FORWARD_ACTIONS_LENGTH} FORWARD_ACTIONS_SOURCE)
    math(EXPR FORWARD_MOVEMENT_LENGTH
        "${FORWARD_DESCENT_DECL_INDEX} - ${FORWARD_MOVEMENT_DECL_INDEX}")
    string(SUBSTRING "${SOURCE}" ${FORWARD_MOVEMENT_DECL_INDEX}
        ${FORWARD_MOVEMENT_LENGTH} FORWARD_MOVEMENT_SOURCE)
    math(EXPR CONTROLLED_SUBMIT_LENGTH "${FORWARD_DESCENT_INDEX} - ${FORWARD_ACTIONS_IF_INDEX}")
    string(SUBSTRING "${SOURCE}" ${FORWARD_ACTIONS_IF_INDEX} ${CONTROLLED_SUBMIT_LENGTH} CONTROLLED_SUBMIT_SOURCE)
    math(EXPR FORWARD_DESCENT_LENGTH "${FORWARD_DESCENT_INDEX} - ${FORWARD_DESCENT_DECL_INDEX}")
    string(SUBSTRING "${SOURCE}" ${FORWARD_DESCENT_DECL_INDEX} ${FORWARD_DESCENT_LENGTH} FORWARD_DESCENT_SOURCE)
    math(EXPR CONTROLLED_DESCENT_LENGTH "${MOVEMENT_INPUT_INDEX} - ${FORWARD_DESCENT_INDEX}")
    string(SUBSTRING "${SOURCE}" ${FORWARD_DESCENT_INDEX} ${CONTROLLED_DESCENT_LENGTH} CONTROLLED_DESCENT_SOURCE)

    string(REGEX MATCHALL "submit_frame_actions${WS}\\(" SUBMIT_ACTION_CALLS "${SOURCE}")
    list(LENGTH SUBMIT_ACTION_CALLS SUBMIT_ACTION_CALL_COUNT)
    string(REGEX MATCHALL "host_gate${WS}=${WS}gate_host_frame${WS}\\(" HOST_GATE_CALLS "${HOST_GATE_SOURCE}")
    list(LENGTH HOST_GATE_CALLS HOST_GATE_CALL_COUNT)
    string(REGEX MATCHALL "session->request_descent${WS}\\(" REQUEST_DESCENT_CALLS "${SOURCE}")
    list(LENGTH REQUEST_DESCENT_CALLS REQUEST_DESCENT_CALL_COUNT)
    string(REGEX MATCHALL "forward_descent${WS}=" FORWARD_DESCENT_ASSIGNMENTS "${HOST_CHAIN_SOURCE}")
    list(LENGTH FORWARD_DESCENT_ASSIGNMENTS FORWARD_DESCENT_ASSIGNMENT_COUNT)
    string(REGEX MATCHALL "gameplay_armed${WS}=" GAMEPLAY_ARMED_ASSIGNMENTS
        "${HOST_CHAIN_SOURCE}")
    list(LENGTH GAMEPLAY_ARMED_ASSIGNMENTS GAMEPLAY_ARMED_ASSIGNMENT_COUNT)
    string(REGEX MATCHALL "forward_movement${WS}=" FORWARD_MOVEMENT_ASSIGNMENTS
        "${HOST_CHAIN_SOURCE}")
    list(LENGTH FORWARD_MOVEMENT_ASSIGNMENTS FORWARD_MOVEMENT_ASSIGNMENT_COUNT)

    string(REGEX MATCHALL
        "validation_runtime->inject_physical_edges${WS}\\("
        FACADE_INJECT_CALLS "${SOURCE}")
    list(LENGTH FACADE_INJECT_CALLS FACADE_INJECT_CALL_COUNT)
    string(REGEX MATCHALL
        "validation_runtime->death_input_snapshot${WS}\\("
        CACHED_INPUT_CALLS "${SOURCE}")
    list(LENGTH CACHED_INPUT_CALLS CACHED_INPUT_CALL_COUNT)
    if(NOT FACADE_SOURCE MATCHES "validation_runtime->inject_physical_edges${WS}\\(${WS}sampled_physical_keys,${WS}input_settings,${WS}current,${WS}gameplay_rearm_was_required${WS}\\)"
            OR NOT CACHED_SOURCE MATCHES "validation_runtime->death_input_snapshot${WS}\\(${WS}\\)"
            OR NOT RELEASED_SOURCE MATCHES "gameplay_controls_physically_released${WS}\\(${WS}stage17_physical_keys${WS}\\)"
            OR NOT RELEASED_SOURCE MATCHES "runtime\\.acknowledge_gameplay_rearmed${WS}\\(${WS}\\)"
            OR NOT MAP_SOURCE MATCHES "map_host_frame_input${WS}\\(${WS}input_settings,${WS}stage17_physical_keys${WS}\\)"
            OR NOT SOURCE MATCHES "DeathInputGate death_gate =${WS}host_death_input_gate${WS}\\(${WS}death_saving,${WS}death_pending,${WS}frame_input\\.keys,${WS}physical_keys${WS}\\)${WS};"
            OR NOT FACADE_INJECT_CALL_COUNT EQUAL 1
            OR NOT CACHED_INPUT_CALL_COUNT EQUAL 1
            OR NOT HOST_GATE_CALL_COUNT EQUAL 2
            OR NOT HOST_GATE_SOURCE MATCHES "if${WS}\\(${WS}pause_open${WS}\\)${WS}\\{${WS}host_gate${WS}=${WS}gate_host_frame${WS}\\(${WS}fixed_step,${WS}pause_latched,${WS}true,${WS}static_cast<double>${WS}\\(${WS}frame_seconds${WS}\\)${WS}\\)${WS};"
            OR NOT HOST_GATE_SOURCE MATCHES "else${WS1}if${WS}\\(${WS}!inventory\\.is_open${WS}\\(${WS}\\)${WS}&&${WS}!inventory_toggled_this_frame${WS}\\)${WS}\\{${WS}host_gate${WS}=${WS}gate_host_frame${WS}\\(${WS}fixed_step,${WS}pause_latched,${WS}false,${WS}static_cast<double>${WS}\\(${WS}frame_seconds${WS}\\)${WS}\\)${WS};"
            OR HOST_CHAIN_SOURCE MATCHES "host_gate\\.forward_gameplay${WS}=${WS}[^=]"
            OR NOT SOURCE MATCHES "const PassiveOverlayInputGate passive_input_gate =${WS}passive_overlay_input_gate${WS}\\(${WS}passive_overlay_open${WS}\\)${WS};"
            OR NOT SOURCE MATCHES "const InventoryInputGate inventory_gate =${WS}inventory_input_gate${WS}\\(${WS}inventory\\.is_open${WS}\\(${WS}\\)${WS}\\|\\|${WS}inventory_toggled_this_frame${WS}\\)${WS};"
            OR NOT GAMEPLAY_ARMED_ASSIGNMENT_COUNT EQUAL 1
            OR NOT GAMEPLAY_ARMED_SOURCE MATCHES "const bool gameplay_armed =${WS}runtime\\.authority_requests_enabled${WS}\\(${WS}\\)${WS}&&${WS}!gameplay_rearm_was_required${WS};"
            OR NOT SUBMIT_ACTION_CALL_COUNT EQUAL 1
            OR NOT FORWARD_ACTIONS_SOURCE MATCHES "const bool forward_actions =${WS}passive_input_gate\\.forward_actions${WS}&&${WS}inventory_gate\\.forward_actions${WS}&&${WS}death_gate\\.forward_gameplay${WS}&&${WS}host_gate\\.forward_gameplay${WS}&&${WS}!pause_blocks_gameplay${WS}&&${WS}gameplay_armed${WS};"
            OR NOT CONTROLLED_SUBMIT_SOURCE MATCHES "if${WS}\\(${WS}forward_actions${WS}\\)${WS}\\{${WS}const${WS1}SubmittedFrameActions${WS1}submitted_actions${WS}=${WS}submit_frame_actions${WS}\\(${WS}\\*session,${WS}frame_input${WS}\\)${WS};"
            OR NOT FORWARD_MOVEMENT_ASSIGNMENT_COUNT EQUAL 1
            OR NOT FORWARD_MOVEMENT_SOURCE MATCHES "const bool forward_movement =${WS}passive_input_gate\\.forward_movement${WS}&&${WS}inventory_gate\\.forward_movement${WS}&&${WS}death_gate\\.forward_gameplay${WS}&&${WS}host_gate\\.forward_gameplay${WS}&&${WS}!pause_blocks_gameplay${WS}&&${WS}gameplay_armed${WS};"
            OR NOT SOURCE MATCHES "const combat::MovementInput movement =${WS}forward_movement${WS}\\?${WS}frame_input\\.movement${WS}:${WS}combat::MovementInput${WS}\\{${WS}\\}${WS};"
            OR NOT FORWARD_DESCENT_ASSIGNMENT_COUNT EQUAL 1
            OR NOT FORWARD_DESCENT_SOURCE MATCHES "const bool forward_descent =${WS}passive_input_gate\\.forward_descent${WS}&&${WS}inventory_gate\\.forward_descent${WS}&&${WS}death_gate\\.forward_gameplay${WS}&&${WS}host_gate\\.forward_gameplay${WS}&&${WS}!pause_blocks_gameplay${WS}&&${WS}gameplay_armed${WS};"
            OR NOT REQUEST_DESCENT_CALL_COUNT EQUAL 1
            OR NOT CONTROLLED_DESCENT_SOURCE MATCHES "if${WS}\\(${WS}forward_descent${WS}&&${WS}frame_input\\.keys\\.e${WS}\\)${WS}\\{${WS}session->snapshot${WS}\\(${WS}current${WS}\\)${WS};${WS}const bool in_range =${WS}current\\.combat\\.has_value${WS}\\(${WS}\\)${WS}&&${WS}can_prompt_descent${WS}\\(${WS}current,${WS}current\\.combat->player\\.position${WS}\\)${WS};${WS}static_cast<void>${WS}\\(${WS}session->request_descent${WS}\\(${WS}in_range${WS}\\)${WS}\\)${WS};")
        set("${OUT_VARIABLE}" FALSE PARENT_SCOPE)
        return()
    endif()
    set("${OUT_VARIABLE}" TRUE PARENT_SCOPE)
endfunction()

function(validation_runtime_input_chain_valid SANITIZED_FUNCTION_SOURCE OUT_VARIABLE)
    mask_cpp_non_direct_executable_scopes(
        "${SANITIZED_FUNCTION_SOURCE}" DIRECT_FUNCTION_SOURCE)

    set(WS "[ \t\r\n]*")
    string(FIND "${DIRECT_FUNCTION_SOURCE}"
        "const PhysicalKeySnapshot stage11b_physical_keys =" STAGE11B_INDEX)
    string(FIND "${DIRECT_FUNCTION_SOURCE}"
        "const PhysicalKeySnapshot stage11c_physical_keys =" STAGE11C_INDEX)
    string(FIND "${DIRECT_FUNCTION_SOURCE}"
        "const PhysicalKeySnapshot stage11d_physical_keys =" STAGE11D_INDEX)
    string(FIND "${DIRECT_FUNCTION_SOURCE}"
        "impl_->death_input_snapshot = stage11d_physical_keys" CACHE_INDEX)
    string(FIND "${DIRECT_FUNCTION_SOURCE}"
        "impl_->states.stage17.suspend_injection = gameplay_rearm_required"
        SUSPEND_INDEX)
    string(FIND "${DIRECT_FUNCTION_SOURCE}"
        "return host_validation::inject_stage17_physical_edges("
        STAGE17_INDEX)
    if(STAGE11B_INDEX EQUAL -1 OR STAGE11C_INDEX EQUAL -1
            OR STAGE11D_INDEX EQUAL -1 OR CACHE_INDEX EQUAL -1
            OR SUSPEND_INDEX EQUAL -1 OR STAGE17_INDEX EQUAL -1
            OR NOT STAGE11B_INDEX LESS STAGE11C_INDEX
            OR NOT STAGE11C_INDEX LESS STAGE11D_INDEX
            OR NOT STAGE11D_INDEX LESS CACHE_INDEX
            OR NOT CACHE_INDEX LESS SUSPEND_INDEX
            OR NOT SUSPEND_INDEX LESS STAGE17_INDEX)
        set("${OUT_VARIABLE}" FALSE PARENT_SCOPE)
        return()
    endif()

    string(SUBSTRING "${DIRECT_FUNCTION_SOURCE}" 0 ${STAGE11B_INDEX}
        FUNCTION_PREFIX)
    string(REGEX MATCHALL
        "(^|[;{}])${WS}return([ \t\r\n;({]|$)"
        EARLY_RETURNS "${FUNCTION_PREFIX}")
    list(LENGTH EARLY_RETURNS EARLY_RETURN_COUNT)
    foreach(INJECTOR IN ITEMS
            inject_stage11b_physical_edges inject_stage11c_physical_edges
            inject_stage11d_physical_edges inject_stage17_physical_edges)
        string(REGEX MATCHALL "${INJECTOR}${WS}\\("
            ${INJECTOR}_CALLS "${DIRECT_FUNCTION_SOURCE}")
        list(LENGTH ${INJECTOR}_CALLS ${INJECTOR}_CALL_COUNT)
        if(NOT ${INJECTOR}_CALL_COUNT EQUAL 1)
            set("${OUT_VARIABLE}" FALSE PARENT_SCOPE)
            return()
        endif()
    endforeach()
    if(NOT EARLY_RETURN_COUNT EQUAL 0
            OR NOT DIRECT_FUNCTION_SOURCE MATCHES
                "inject_stage11b_physical_edges${WS}\\(${WS}snapshot,${WS}\\*impl_->config,${WS}impl_->states\\.stage11b${WS}\\)"
            OR NOT DIRECT_FUNCTION_SOURCE MATCHES
                "inject_stage11c_physical_edges${WS}\\(${WS}stage11b_physical_keys,${WS}\\*impl_->config,${WS}input_settings,${WS}dungeon_snapshot,${WS}impl_->states\\.stage11c${WS}\\)"
            OR NOT DIRECT_FUNCTION_SOURCE MATCHES
                "inject_stage11d_physical_edges${WS}\\(${WS}stage11c_physical_keys,${WS}\\*impl_->config,${WS}input_settings,${WS}dungeon_snapshot,${WS}impl_->states\\.stage11d${WS}\\)"
            OR NOT DIRECT_FUNCTION_SOURCE MATCHES
                "inject_stage17_physical_edges${WS}\\(${WS}stage11d_physical_keys,${WS}\\*impl_->config,${WS}input_settings,${WS}dungeon_snapshot,${WS}impl_->states\\.stage17${WS}\\)")
        set("${OUT_VARIABLE}" FALSE PARENT_SCOPE)
        return()
    endif()
    set("${OUT_VARIABLE}" TRUE PARENT_SCOPE)
endfunction()

physical_input_chain_valid("${HOST_SOURCE}" HOST_INPUT_CHAIN_VALID)
if(NOT HOST_INPUT_CHAIN_VALID)
    message(FATAL_ERROR
        "host input chain must apply Stage11B, Stage11C, then Stage11D before mapping")
endif()
validation_runtime_input_chain_valid("${HOST_VALIDATION_INJECT_SOURCE}"
    HOST_VALIDATION_RUNTIME_INPUT_CHAIN_VALID)
if(NOT HOST_VALIDATION_RUNTIME_INPUT_CHAIN_VALID)
    message(FATAL_ERROR
        "host validation runtime must compose Stage11B, Stage11C, Stage11D, then Stage17 input")
endif()
physical_input_chain_valid("${REAL_STRUCTURE}" REFERENCE_INPUT_CHAIN_VALID)
if(NOT REFERENCE_INPUT_CHAIN_VALID)
    message(FATAL_ERROR "host input chain self-check rejected reference")
endif()
set(GAMEPLAY_ARMED_DEFINITION [=[const bool gameplay_armed = runtime.authority_requests_enabled()
    && !gameplay_rearm_was_required;]=])
string(REPLACE "${GAMEPLAY_ARMED_DEFINITION}"
    "const bool gameplay_armed = true;"
    UNCONDITIONAL_GAMEPLAY_ARMED_STRUCTURE "${REAL_STRUCTURE}")
if(UNCONDITIONAL_GAMEPLAY_ARMED_STRUCTURE STREQUAL REAL_STRUCTURE)
    message(FATAL_ERROR "gameplay_armed mutation setup did not modify reference")
endif()
physical_input_chain_valid("${UNCONDITIONAL_GAMEPLAY_ARMED_STRUCTURE}"
    UNCONDITIONAL_GAMEPLAY_ARMED_STRUCTURE_VALID)
if(UNCONDITIONAL_GAMEPLAY_ARMED_STRUCTURE_VALID)
    message(FATAL_ERROR "host input chain accepted gameplay_armed = true")
endif()
set(FORWARD_MOVEMENT_DEFINITION [=[const bool forward_movement = passive_input_gate.forward_movement
    && inventory_gate.forward_movement
    && death_gate.forward_gameplay
    && host_gate.forward_gameplay && !pause_blocks_gameplay
    && gameplay_armed;]=])
string(REPLACE "${FORWARD_MOVEMENT_DEFINITION}"
    "const bool forward_movement = true;"
    UNCONDITIONAL_FORWARD_MOVEMENT_STRUCTURE "${REAL_STRUCTURE}")
if(UNCONDITIONAL_FORWARD_MOVEMENT_STRUCTURE STREQUAL REAL_STRUCTURE)
    message(FATAL_ERROR "forward_movement mutation setup did not modify reference")
endif()
physical_input_chain_valid("${UNCONDITIONAL_FORWARD_MOVEMENT_STRUCTURE}"
    UNCONDITIONAL_FORWARD_MOVEMENT_STRUCTURE_VALID)
if(UNCONDITIONAL_FORWARD_MOVEMENT_STRUCTURE_VALID)
    message(FATAL_ERROR "host input chain accepted forward_movement = true")
endif()
set(INPUT_CHAIN_SPOOF_ACCEPTANCES)
set(COMMENT_ONLY_INPUT_CHAIN "/*${REAL_STRUCTURE}*/")
set(STRING_ONLY_INPUT_CHAIN "R\"arpg(${REAL_STRUCTURE})arpg\"")
physical_input_chain_valid("${COMMENT_ONLY_INPUT_CHAIN}"
    COMMENT_ONLY_INPUT_CHAIN_VALID)
if(COMMENT_ONLY_INPUT_CHAIN_VALID)
    list(APPEND INPUT_CHAIN_SPOOF_ACCEPTANCES "comment-only-chain")
endif()
physical_input_chain_valid("${STRING_ONLY_INPUT_CHAIN}"
    STRING_ONLY_INPUT_CHAIN_VALID)
if(STRING_ONLY_INPUT_CHAIN_VALID)
    list(APPEND INPUT_CHAIN_SPOOF_ACCEPTANCES "string-only-chain")
endif()
if(INPUT_CHAIN_SPOOF_ACCEPTANCES)
    list(JOIN INPUT_CHAIN_SPOOF_ACCEPTANCES ", " INPUT_CHAIN_SPOOF_NAMES)
    message(FATAL_ERROR
        "host input chain accepted source spoofs: ${INPUT_CHAIN_SPOOF_NAMES}")
endif()
string(REPLACE
    "stage11b_physical_keys, *impl_->config, input_settings,"
    "snapshot, *impl_->config, input_settings,"
    BYPASSED_STAGE11C_STRUCTURE "${HOST_VALIDATION_INJECT_SOURCE}")
validation_runtime_input_chain_valid("${BYPASSED_STAGE11C_STRUCTURE}"
    BYPASSED_STAGE11C_STRUCTURE_VALID)
if(BYPASSED_STAGE11C_STRUCTURE_VALID)
    message(FATAL_ERROR "host input chain accepted Stage11C bypass mutation")
endif()
string(REPLACE
    "stage11c_physical_keys, *impl_->config, input_settings,"
    "stage11b_physical_keys, *impl_->config, input_settings,"
    BYPASSED_STAGE11D_STRUCTURE "${HOST_VALIDATION_INJECT_SOURCE}")
validation_runtime_input_chain_valid("${BYPASSED_STAGE11D_STRUCTURE}"
    BYPASSED_STAGE11D_STRUCTURE_VALID)
if(BYPASSED_STAGE11D_STRUCTURE_VALID)
    message(FATAL_ERROR "host input chain accepted Stage11D bypass mutation")
endif()
set(_MAP_DECLARATION [=[HostFrameInput frame_input = map_host_frame_input(
    input_settings, stage17_physical_keys);
]=])
string(REPLACE "${_MAP_DECLARATION}" ""
    MAP_BEFORE_STAGE11C_STRUCTURE "${REAL_STRUCTURE}")
string(REPLACE
    "const PhysicalKeySnapshot stage17_physical_keys ="
    "${_MAP_DECLARATION}const PhysicalKeySnapshot stage17_physical_keys ="
    MAP_BEFORE_STAGE11C_STRUCTURE "${MAP_BEFORE_STAGE11C_STRUCTURE}")
physical_input_chain_valid("${MAP_BEFORE_STAGE11C_STRUCTURE}"
    MAP_BEFORE_STAGE11C_STRUCTURE_VALID)
if(MAP_BEFORE_STAGE11C_STRUCTURE_VALID)
    message(FATAL_ERROR "host input chain accepted map-before-Stage11C mutation")
endif()

set(SUBMIT_DECLARATION [=[
                const SubmittedFrameActions submitted_actions =
                    submit_frame_actions(*session, frame_input);
]=])
set(HOST_GATE_DECLARATION
    "            HostFrameGateResult host_gate{};")
string(REPLACE "${SUBMIT_DECLARATION}" ""
    SUBMIT_BEFORE_GATE_STRUCTURE "${HOST_SOURCE}")
string(REPLACE "${HOST_GATE_DECLARATION}"
    "${SUBMIT_DECLARATION}\n${HOST_GATE_DECLARATION}"
    SUBMIT_BEFORE_GATE_STRUCTURE "${SUBMIT_BEFORE_GATE_STRUCTURE}")
string(REPLACE "            if (forward_actions) {"
    "            {"
    UNCONDITIONAL_SUBMIT_STRUCTURE "${HOST_SOURCE}")
set(ACCEPTED_SUBMIT_MUTATIONS)
physical_input_chain_valid("${SUBMIT_BEFORE_GATE_STRUCTURE}"
    SUBMIT_BEFORE_GATE_STRUCTURE_VALID)
if(SUBMIT_BEFORE_GATE_STRUCTURE_VALID)
    list(APPEND ACCEPTED_SUBMIT_MUTATIONS "submit-before-gate")
endif()
physical_input_chain_valid("${UNCONDITIONAL_SUBMIT_STRUCTURE}"
    UNCONDITIONAL_SUBMIT_STRUCTURE_VALID)
if(UNCONDITIONAL_SUBMIT_STRUCTURE_VALID)
    list(APPEND ACCEPTED_SUBMIT_MUTATIONS "unconditional-submit-bypass")
endif()
if(ACCEPTED_SUBMIT_MUTATIONS)
    list(JOIN ACCEPTED_SUBMIT_MUTATIONS ", " ACCEPTED_SUBMIT_MUTATION_NAMES)
    message(FATAL_ERROR
        "host input chain accepted mutations: ${ACCEPTED_SUBMIT_MUTATION_NAMES}")
endif()

set(HOST_GATE_TRUE_ASSIGNMENT [=[
                host_gate = gate_host_frame(fixed_step, pause_latched, true,
                    static_cast<double>(frame_seconds));
]=])
set(HOST_GATE_FALSE_ASSIGNMENT [=[
                host_gate = gate_host_frame(fixed_step, pause_latched, false,
                    static_cast<double>(frame_seconds));
]=])
string(REPLACE "${HOST_GATE_TRUE_ASSIGNMENT}"
    "                host_gate.forward_gameplay = true;"
    HOST_GATE_BYPASS_STRUCTURE "${HOST_SOURCE}")
string(REPLACE "${HOST_GATE_FALSE_ASSIGNMENT}"
    "                host_gate.forward_gameplay = true;"
    HOST_GATE_BYPASS_STRUCTURE "${HOST_GATE_BYPASS_STRUCTURE}")
set(FORWARD_DESCENT_DECLARATION [=[
            const bool forward_descent = passive_input_gate.forward_descent
                && inventory_gate.forward_descent
                && death_gate.forward_gameplay
                && host_gate.forward_gameplay && !pause_blocks_gameplay
                && gameplay_armed;
]=])
string(REPLACE "${FORWARD_DESCENT_DECLARATION}"
    "            const bool forward_descent = true;"
    UNCONDITIONAL_DESCENT_STRUCTURE "${HOST_SOURCE}")
if(HOST_GATE_BYPASS_STRUCTURE STREQUAL HOST_SOURCE
        OR UNCONDITIONAL_DESCENT_STRUCTURE STREQUAL HOST_SOURCE)
    message(FATAL_ERROR "host gate/descent mutation setup did not modify production source")
endif()
set(ACCEPTED_GATE_DESCENT_MUTATIONS)
physical_input_chain_valid("${HOST_GATE_BYPASS_STRUCTURE}"
    HOST_GATE_BYPASS_STRUCTURE_VALID)
if(HOST_GATE_BYPASS_STRUCTURE_VALID)
    list(APPEND ACCEPTED_GATE_DESCENT_MUTATIONS "host-gate-bypass")
endif()
physical_input_chain_valid("${UNCONDITIONAL_DESCENT_STRUCTURE}"
    UNCONDITIONAL_DESCENT_STRUCTURE_VALID)
if(UNCONDITIONAL_DESCENT_STRUCTURE_VALID)
    list(APPEND ACCEPTED_GATE_DESCENT_MUTATIONS
        "unconditional-descent-bypass")
endif()
if(ACCEPTED_GATE_DESCENT_MUTATIONS)
    list(JOIN ACCEPTED_GATE_DESCENT_MUTATIONS ", "
        ACCEPTED_GATE_DESCENT_MUTATION_NAMES)
    message(FATAL_ERROR
        "host input chain accepted mutations: ${ACCEPTED_GATE_DESCENT_MUTATION_NAMES}")
endif()

function(strip_cpp_noncode SOURCE OUT_VARIABLE)
    set(CODE "${SOURCE}")
    string(REGEX REPLACE "/\\*([^*]|\\*[^/])*\\*/" "" CODE "${CODE}")
    string(REGEX REPLACE "//[^\r\n]*" "" CODE "${CODE}")
    string(REGEX REPLACE "\"[^\"]*\"" "\"\"" CODE "${CODE}")
    set("${OUT_VARIABLE}" "${CODE}" PARENT_SCOPE)
endfunction()

function(hud_present_structure_valid SOURCE OUT_VARIABLE)
    strip_cpp_noncode("${SOURCE}" CODE)
    string(REGEX MATCHALL "renderer\\.observe_presented_hud_frame\\(" OBSERVES "${CODE}")
    list(LENGTH OBSERVES OBSERVE_COUNT)
    string(REGEX MATCHALL "present_frame_and_maybe_capture\\(" PRESENTS "${CODE}")
    list(LENGTH PRESENTS PRESENT_COUNT)
    if(NOT OBSERVE_COUNT EQUAL 2 OR NOT PRESENT_COUNT EQUAL 3)
        set("${OUT_VARIABLE}" FALSE PARENT_SCOPE)
        return()
    endif()

    set(CAPTURE_PATH_ARGUMENT_PATTERN
        "capture_path[.]has_value\\([ \t\r\n]*\\)[ \t\r\n]*\\?[ \t\r\n]*capture_path->c_str\\([ \t\r\n]*\\)[ \t\r\n]*:[ \t\r\n]*nullptr")
    set(RECOVERY_PRESENT_PATTERN
        "static_cast[ \t\r\n]*<[ \t\r\n]*void[ \t\r\n]*>[ \t\r\n]*\\([ \t\r\n]*present_frame_and_maybe_capture[ \t\r\n]*\\([ \t\r\n]*${CAPTURE_PATH_ARGUMENT_PATTERN}[ \t\r\n]*\\)[ \t\r\n]*\\)[ \t\r\n]*;")
    string(FIND "${CODE}" "if (runtime.state() == DungeonRuntimeState::recovery_required)" RECOVERY_START)
    string(FIND "${CODE}" "draw_recovery_screen(runtime.render_status());" RECOVERY_DRAW)
    string(REGEX MATCH "${RECOVERY_PRESENT_PATTERN}"
        RECOVERY_PRESENT_CALL_MATCH "${CODE}")
    set(RECOVERY_PRESENT -1)
    if(NOT RECOVERY_PRESENT_CALL_MATCH STREQUAL "")
        string(FIND "${CODE}" "${RECOVERY_PRESENT_CALL_MATCH}"
            RECOVERY_PRESENT)
    endif()
    string(FIND "${CODE}" "renderer.observe_presented_hud_frame(" FIRST_OBSERVE)
    if(RECOVERY_START LESS 0 OR RECOVERY_DRAW LESS 0 OR RECOVERY_PRESENT LESS 0
            OR FIRST_OBSERVE LESS RECOVERY_START OR FIRST_OBSERVE GREATER RECOVERY_DRAW
            OR RECOVERY_DRAW GREATER RECOVERY_PRESENT)
        set("${OUT_VARIABLE}" FALSE PARENT_SCOPE)
        return()
    endif()

    math(EXPR AFTER_FIRST_OBSERVE "${FIRST_OBSERVE} + 1")
    string(SUBSTRING "${CODE}" ${AFTER_FIRST_OBSERVE} -1 AFTER_FIRST_OBSERVE_CODE)
    string(FIND "${AFTER_FIRST_OBSERVE_CODE}" "renderer.observe_presented_hud_frame(" SECOND_OBSERVE_RELATIVE)
    if(SECOND_OBSERVE_RELATIVE LESS 0)
        set("${OUT_VARIABLE}" FALSE PARENT_SCOPE)
        return()
    endif()
    math(EXPR SECOND_OBSERVE "${AFTER_FIRST_OBSERVE} + ${SECOND_OBSERVE_RELATIVE}")
    math(EXPR AFTER_RECOVERY_PRESENT "${RECOVERY_PRESENT} + 1")
    string(SUBSTRING "${CODE}" ${AFTER_RECOVERY_PRESENT} -1 AFTER_RECOVERY_CODE)
    string(FIND "${AFTER_RECOVERY_CODE}" "BeginDrawing();" NORMAL_BEGIN_RELATIVE)
    set(NORMAL_PRESENT_PATTERN
        "const[ \t\r\n]+bool[ \t\r\n]+capture_succeeded[ \t\r\n]*=[ \t\r\n]*present_frame_and_maybe_capture[ \t\r\n]*\\([ \t\r\n]*${CAPTURE_PATH_ARGUMENT_PATTERN}[ \t\r\n]*\\)[ \t\r\n]*;")
    string(REGEX MATCH "${NORMAL_PRESENT_PATTERN}"
        NORMAL_PRESENT_CALL_MATCH "${AFTER_RECOVERY_CODE}")
    set(NORMAL_PRESENT_RELATIVE -1)
    if(NOT NORMAL_PRESENT_CALL_MATCH STREQUAL "")
        string(FIND "${AFTER_RECOVERY_CODE}" "${NORMAL_PRESENT_CALL_MATCH}"
            NORMAL_PRESENT_RELATIVE)
    endif()
    if(NORMAL_BEGIN_RELATIVE LESS 0 OR NORMAL_PRESENT_RELATIVE LESS 0)
        set("${OUT_VARIABLE}" FALSE PARENT_SCOPE)
        return()
    endif()
    math(EXPR NORMAL_BEGIN "${AFTER_RECOVERY_PRESENT} + ${NORMAL_BEGIN_RELATIVE}")
    math(EXPR NORMAL_PRESENT "${AFTER_RECOVERY_PRESENT} + ${NORMAL_PRESENT_RELATIVE}")
    if(SECOND_OBSERVE LESS RECOVERY_PRESENT OR SECOND_OBSERVE GREATER NORMAL_BEGIN
            OR NORMAL_BEGIN GREATER NORMAL_PRESENT)
        set("${OUT_VARIABLE}" FALSE PARENT_SCOPE)
        return()
    endif()
    math(EXPR RECOVERY_LENGTH "${RECOVERY_PRESENT} - ${RECOVERY_START}")
    string(SUBSTRING "${CODE}" ${RECOVERY_START} ${RECOVERY_LENGTH} RECOVERY_BRANCH)
    string(REGEX MATCH
        "renderer\\.observe_presented_hud_frame\\([ \t\r\n]*HudPresentedFrame::recovery"
        RECOVERY_OWNER_MATCH "${RECOVERY_BRANCH}")
    math(EXPR NORMAL_LENGTH "${NORMAL_PRESENT} - ${AFTER_RECOVERY_PRESENT}")
    string(SUBSTRING "${CODE}" ${AFTER_RECOVERY_PRESENT} ${NORMAL_LENGTH} NORMAL_BRANCH)
    string(REGEX MATCH
        "current\\.death\\.has_value\\(\\)[ \t\r\n]*\\?[ \t\r\n]*HudPresentedFrame::death_overlay[ \t\r\n]*:[ \t\r\n]*HudPresentedFrame::normal"
        NORMAL_DEATH_OWNER_MATCH "${NORMAL_BRANCH}")
    string(REGEX MATCH
        "renderer\\.observe_presented_hud_frame\\([ \t\r\n]*hud_presented_frame"
        NORMAL_OBSERVE_OWNER_MATCH "${NORMAL_BRANCH}")
    if(RECOVERY_OWNER_MATCH STREQUAL "" OR NORMAL_DEATH_OWNER_MATCH STREQUAL ""
            OR NORMAL_OBSERVE_OWNER_MATCH STREQUAL "")
        set("${OUT_VARIABLE}" FALSE PARENT_SCOPE)
        return()
    endif()
    set("${OUT_VARIABLE}" TRUE PARENT_SCOPE)
endfunction()

# Bind exactly two actual paths to the production seam after stripping comments
# and strings. Self fixtures reject a third present, a post-BeginDrawing seam,
# and comment-only tokens without registering a separate Task8 guard.
hud_present_structure_valid("${HOST_SOURCE}" HOST_PRESENT_STRUCTURE_VALID)
if(NOT HOST_PRESENT_STRUCTURE_VALID)
    message(FATAL_ERROR "host HUD presentation seam structure is invalid")
endif()
set(RECOVERY_PRESENT_MULTILINE [=[static_cast<void>(present_frame_and_maybe_capture(
                    capture_path.has_value() ? capture_path->c_str() : nullptr));]=])
set(RECOVERY_PRESENT_SINGLE_LINE
    "static_cast<void>(present_frame_and_maybe_capture(capture_path.has_value() ? capture_path->c_str() : nullptr));")
string(REPLACE "${RECOVERY_PRESENT_MULTILINE}" "${RECOVERY_PRESENT_SINGLE_LINE}"
    SINGLE_LINE_RECOVERY_PRESENT_SOURCE "${HOST_SOURCE}")
if(SINGLE_LINE_RECOVERY_PRESENT_SOURCE STREQUAL HOST_SOURCE)
    message(FATAL_ERROR "single-line recovery present fixture did not mutate")
endif()
hud_present_structure_valid("${SINGLE_LINE_RECOVERY_PRESENT_SOURCE}"
    SINGLE_LINE_RECOVERY_PRESENT_VALID)
if(NOT SINGLE_LINE_RECOVERY_PRESENT_VALID)
    message(FATAL_ERROR "single-line recovery present formatting must validate")
endif()
set(RECOVERY_NULL_ARGUMENT_PRESENT
    "static_cast<void>(present_frame_and_maybe_capture(nullptr));")
string(REPLACE "${RECOVERY_PRESENT_MULTILINE}"
    "${RECOVERY_NULL_ARGUMENT_PRESENT}"
    RECOVERY_NULL_ARGUMENT_SOURCE "${HOST_SOURCE}")
if(RECOVERY_NULL_ARGUMENT_SOURCE STREQUAL HOST_SOURCE)
    message(FATAL_ERROR "recovery null-argument fixture did not mutate")
endif()
hud_present_structure_valid("${RECOVERY_NULL_ARGUMENT_SOURCE}"
    RECOVERY_NULL_ARGUMENT_VALID)
if(RECOVERY_NULL_ARGUMENT_VALID)
    message(FATAL_ERROR "recovery present must retain the capture-path argument")
endif()
set(NORMAL_PRESENT_MULTILINE [=[const bool capture_succeeded =
                present_frame_and_maybe_capture(capture_path.has_value()
                    ? capture_path->c_str() : nullptr);]=])
set(NORMAL_NULL_ARGUMENT_PRESENT [=[const bool capture_succeeded =
                present_frame_and_maybe_capture(nullptr);]=])
string(REPLACE "${NORMAL_PRESENT_MULTILINE}"
    "${NORMAL_NULL_ARGUMENT_PRESENT}"
    NORMAL_NULL_ARGUMENT_SOURCE "${HOST_SOURCE}")
if(NORMAL_NULL_ARGUMENT_SOURCE STREQUAL HOST_SOURCE)
    message(FATAL_ERROR "normal null-argument fixture did not mutate")
endif()
hud_present_structure_valid("${NORMAL_NULL_ARGUMENT_SOURCE}"
    NORMAL_NULL_ARGUMENT_VALID)
if(NORMAL_NULL_ARGUMENT_VALID)
    message(FATAL_ERROR "normal present must retain the capture-path argument")
endif()
string(REPLACE "draw_recovery_screen(runtime.render_status());" ""
    RECOVERY_PRESENT_BEFORE_DRAW_SOURCE "${HOST_SOURCE}")
set(RECOVERY_PRESENT_WITH_LATE_DRAW
    "${RECOVERY_PRESENT_MULTILINE}\n                draw_recovery_screen(runtime.render_status());")
string(REPLACE "${RECOVERY_PRESENT_MULTILINE}"
    "${RECOVERY_PRESENT_WITH_LATE_DRAW}"
    RECOVERY_PRESENT_BEFORE_DRAW_SOURCE
    "${RECOVERY_PRESENT_BEFORE_DRAW_SOURCE}")
if(RECOVERY_PRESENT_BEFORE_DRAW_SOURCE STREQUAL HOST_SOURCE)
    message(FATAL_ERROR "recovery present-before-draw fixture did not mutate")
endif()
hud_present_structure_valid("${RECOVERY_PRESENT_BEFORE_DRAW_SOURCE}"
    RECOVERY_PRESENT_BEFORE_DRAW_VALID)
if(RECOVERY_PRESENT_BEFORE_DRAW_VALID)
    message(FATAL_ERROR "recovery present before draw must not validate")
endif()
set(COMMENT_ONLY_HUD_PRESENT [=[
// renderer.observe_presented_hud_frame(HudPresentedFrame::recovery);
// present_frame_and_maybe_capture(capture_path.has_value() ? path : nullptr);
]=])
hud_present_structure_valid("${COMMENT_ONLY_HUD_PRESENT}" COMMENT_ONLY_HUD_PRESENT_VALID)
if(COMMENT_ONLY_HUD_PRESENT_VALID)
    message(FATAL_ERROR "comment-only HUD presentation tokens must not validate")
endif()
set(THIRD_PRESENT_HUD_SOURCE "${HOST_SOURCE}\npresent_frame_and_maybe_capture(nullptr);")
hud_present_structure_valid("${THIRD_PRESENT_HUD_SOURCE}" THIRD_PRESENT_HUD_VALID)
if(THIRD_PRESENT_HUD_VALID)
    message(FATAL_ERROR "third HUD present path must not validate")
endif()
string(REPLACE "renderer.observe_presented_hud_frame(hud_presented_frame,"
    "BeginDrawing();\nrenderer.observe_presented_hud_frame(hud_presented_frame,"
    MOVED_HUD_OBSERVE_SOURCE "${HOST_SOURCE}")
hud_present_structure_valid("${MOVED_HUD_OBSERVE_SOURCE}" MOVED_HUD_OBSERVE_VALID)
if(MOVED_HUD_OBSERVE_VALID)
    message(FATAL_ERROR "HUD seam after BeginDrawing must not validate")
endif()
string(REPLACE "HudPresentedFrame::recovery" "HudPresentedFrame::normal"
    RECOVERY_OWNER_MUTATION_SOURCE "${HOST_SOURCE}")
hud_present_structure_valid("${RECOVERY_OWNER_MUTATION_SOURCE}" RECOVERY_OWNER_MUTATION_VALID)
if(RECOVERY_OWNER_MUTATION_VALID)
    message(FATAL_ERROR "recovery owner mutation must not validate")
endif()
string(REPLACE "HudPresentedFrame::death_overlay" "HudPresentedFrame::normal"
    DEATH_OWNER_MUTATION_SOURCE "${HOST_SOURCE}")
hud_present_structure_valid("${DEATH_OWNER_MUTATION_SOURCE}" DEATH_OWNER_MUTATION_VALID)
if(DEATH_OWNER_MUTATION_VALID)
    message(FATAL_ERROR "death owner mutation must not validate")
endif()
string(REPLACE "? HudPresentedFrame::death_overlay : HudPresentedFrame::normal"
    "? HudPresentedFrame::normal : HudPresentedFrame::death_overlay"
    TERNARY_OWNER_MUTATION_SOURCE "${HOST_SOURCE}")
hud_present_structure_valid("${TERNARY_OWNER_MUTATION_SOURCE}" TERNARY_OWNER_MUTATION_VALID)
if(TERNARY_OWNER_MUTATION_VALID)
    message(FATAL_ERROR "death owner ternary mutation must not validate")
endif()

set(POISON_TARGET
    "::arpg::platform::direct_input_poison::blocked")
set(DIRECT_INPUT_APIS
    platform_key_pressed
    platform_key_down
    IsKeyPressed
    IsKeyPressedRepeat
    IsKeyDown
    IsKeyReleased
    IsKeyUp
    GetKeyPressed
    GetCharPressed
    IsMouseButtonPressed
    IsMouseButtonDown
    IsMouseButtonReleased
    IsMouseButtonUp
    GetMouseX
    GetMouseY
    GetMousePosition
    GetMouseDelta
    GetMouseWheelMove
    GetMouseWheelMoveV
    IsWindowFocused)

list(LENGTH DIRECT_INPUT_APIS DIRECT_INPUT_API_COUNT)
if(NOT DIRECT_INPUT_API_COUNT EQUAL 20)
    message(FATAL_ERROR "direct input poison API table must contain 20 entries")
endif()

foreach(DIRECT_INPUT_API IN LISTS DIRECT_INPUT_APIS)
    poison_macro_line_pattern(
        "${DIRECT_INPUT_API}" DIRECT_INPUT_MACRO_PATTERN)
    require_match_count(
        "${POISON_SOURCE}"
        "${DIRECT_INPUT_MACRO_PATTERN}"
        1
        "poison macro ${DIRECT_INPUT_API}")
endforeach()

require_match_count(
    "${POISON_SOURCE}"
    "(^|\n)#define[ \t]+[A-Za-z_][A-Za-z0-9_]*[ \t]+${POISON_TARGET}"
    20
    "complete direct input poison table")
require_match_count(
    "${POISON_SOURCE}"
    "${ACTIVE_SENTINEL_PATTERN}"
    1
    "direct input poison active sentinels")
