cmake_minimum_required(VERSION 3.25)

get_filename_component(REPO_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
set(RAYLIB_SOURCE_DIR "${REPO_ROOT}/src/platform/raylib")
file(READ "${RAYLIB_SOURCE_DIR}/raylib_host.cpp" HOST_SOURCE)
file(READ "${RAYLIB_SOURCE_DIR}/host_window_lifetime.cpp" OWNER_SOURCE)
include("${REPO_ROOT}/tests/platform/cpp_source_lexer.cmake")
include("${REPO_ROOT}/tests/dungeon/evidence_source_scan.cmake")

# Copied verbatim from input_latency_source_test.cmake: the diagnostic must use
# the same unconditional-source preparation as the formal input guard.
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
                message(FATAL_ERROR "Stage17 injector conditional is unbalanced")
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
        message(FATAL_ERROR "Stage17 injector conditional is unbalanced")
    endif()
    set("${OUT_SURFACE}" "${SURFACE}" PARENT_SCOPE)
endfunction()

# Copied verbatim from host_validation_source_test.cmake for the controlled
# comparison requested by Task 2 debugging.
function(host_validation_unconditional_cpp_surface SOURCE OUT_SURFACE)
    arpg_sanitize_cpp_source("${SOURCE}" _logical_source)
    string(LENGTH "${_logical_source}" _source_length)
    set(_cursor 0)
    set(_conditional_depth 0)
    set(_surface "")
    while(_cursor LESS _source_length)
        string(SUBSTRING "${_logical_source}" ${_cursor} -1 _tail)
        string(FIND "${_tail}" "\n" _newline)
        if(_newline EQUAL -1)
            set(_line "${_tail}")
            set(_line_length -1)
        else()
            math(EXPR _line_length "${_newline} + 1")
            string(SUBSTRING "${_tail}" 0 ${_line_length} _line)
        endif()
        set(_mask_line FALSE)
        if(_line MATCHES
                "^[ \t]*(#|%:)[ \t]*(if|ifdef|ifndef)([ \t\r\n(]|$)")
            math(EXPR _conditional_depth "${_conditional_depth} + 1")
            set(_mask_line TRUE)
        elseif(_line MATCHES
                "^[ \t]*(#|%:)[ \t]*endif([ \t\r\n]|$)")
            if(_conditional_depth EQUAL 0)
                message(FATAL_ERROR
                    "Host validation inactive preprocessor surface is unbalanced")
            endif()
            set(_mask_line TRUE)
            math(EXPR _conditional_depth "${_conditional_depth} - 1")
        elseif(_conditional_depth GREATER 0)
            set(_mask_line TRUE)
        endif()
        if(_mask_line)
            string(REGEX REPLACE "[^\r\n]" " " _line "${_line}")
        endif()
        string(APPEND _surface "${_line}")
        if(_newline EQUAL -1)
            break()
        endif()
        math(EXPR _cursor "${_cursor} + ${_line_length}")
    endwhile()
    if(NOT _conditional_depth EQUAL 0)
        message(FATAL_ERROR "Host validation inactive preprocessor surface is unbalanced")
    endif()
    set(${OUT_SURFACE} "${_surface}" PARENT_SCOPE)
endfunction()

# Copied verbatim from input_latency_source_test.cmake.
function(arpg_latency_cpp_executable_surface SOURCE OUT_SOURCE)
    evidence_cpp_direct_execution_surface_in_sanitized(
        "${SOURCE}" DIRECT_SOURCE)
    set(${OUT_SOURCE} "${DIRECT_SOURCE}" PARENT_SCOPE)
endfunction()

function(diagnostic_first_difference ACTUAL EXPECTED LABEL)
    string(LENGTH "${ACTUAL}" ACTUAL_LENGTH)
    string(LENGTH "${EXPECTED}" EXPECTED_LENGTH)
    string(SHA256 ACTUAL_HASH "${ACTUAL}")
    string(SHA256 EXPECTED_HASH "${EXPECTED}")
    math(EXPR LIMIT "${ACTUAL_LENGTH} < ${EXPECTED_LENGTH} ? ${ACTUAL_LENGTH} : ${EXPECTED_LENGTH}")
    set(DIFF_POSITION -1)
    if(LIMIT GREATER 0)
        math(EXPR LAST "${LIMIT} - 1")
        foreach(POSITION RANGE 0 ${LAST})
            string(SUBSTRING "${ACTUAL}" ${POSITION} 1 ACTUAL_CHAR)
            string(SUBSTRING "${EXPECTED}" ${POSITION} 1 EXPECTED_CHAR)
            if(NOT ACTUAL_CHAR STREQUAL EXPECTED_CHAR)
                set(DIFF_POSITION ${POSITION})
                break()
            endif()
        endforeach()
    endif()
    if(DIFF_POSITION EQUAL -1 AND NOT ACTUAL_LENGTH EQUAL EXPECTED_LENGTH)
        set(DIFF_POSITION ${LIMIT})
    endif()
    message(STATUS "diag ${LABEL}: actual_length=${ACTUAL_LENGTH} expected_length=${EXPECTED_LENGTH} actual_sha256=${ACTUAL_HASH} expected_sha256=${EXPECTED_HASH} first_diff=${DIFF_POSITION}")
endfunction()

function(diagnose_window_boundary)
stage17_unconditional_cpp_surface("${HOST_SOURCE}" HOST_ACTIVE_SOURCE)
stage17_unconditional_cpp_surface("${OWNER_SOURCE}" OWNER_ACTIVE_SOURCE)
host_validation_unconditional_cpp_surface("${OWNER_SOURCE}" OWNER_MASKED_ACTIVE_SOURCE)
arpg_sanitize_cpp_source("${OWNER_SOURCE}" OWNER_LEXICAL_SOURCE)
arpg_latency_cpp_executable_surface("${HOST_SOURCE}" HOST_ENTRY_DIRECT_SOURCE)
evidence_window_lifetime_boundary_is_valid(
    "${HOST_ENTRY_DIRECT_SOURCE}" "${OWNER_ACTIVE_SOURCE}"
    "${OWNER_LEXICAL_SOURCE}" BOUNDARY_VALID)
message(STATUS "diag boundary_result=${BOUNDARY_VALID}")
evidence_window_lifetime_boundary_is_valid(
    "${HOST_ENTRY_DIRECT_SOURCE}" "${OWNER_MASKED_ACTIVE_SOURCE}"
    "${OWNER_LEXICAL_SOURCE}" MASKED_BOUNDARY_VALID)
message(STATUS "diag masked_boundary_result=${MASKED_BOUNDARY_VALID}")
foreach(SURFACE_ID IN ITEMS delete masked lexical)
    if(SURFACE_ID STREQUAL delete)
        set(SURFACE "${OWNER_ACTIVE_SOURCE}")
    elseif(SURFACE_ID STREQUAL masked)
        set(SURFACE "${OWNER_MASKED_ACTIVE_SOURCE}")
    else()
        set(SURFACE "${OWNER_LEXICAL_SOURCE}")
    endif()
    evidence_cpp_mask_preprocessor_directives_in_sanitized(
        "${SURFACE}" SURFACE_SCOPE)
    evidence_cpp_normalize_token_whitespace("${SURFACE_SCOPE}" SURFACE_TOKEN)
    string(FIND "${SURFACE_TOKEN}" "void set_config_flags_noexcept("
        SURFACE_SET_CONFIG)
    string(FIND "${SURFACE_TOKEN}" "void* detach_ime_context_noexcept("
        SURFACE_DETACH)
    if(NOT SURFACE_SET_CONFIG EQUAL -1)
        evidence_cpp_prefix_has_exact_namespace_scope_in_sanitized(
            "${SURFACE_TOKEN}" ${SURFACE_SET_CONFIG} "<anonymous>"
            SURFACE_SET_CONFIG_SCOPE_VALID)
    else()
        set(SURFACE_SET_CONFIG_SCOPE_VALID FALSE)
    endif()
    if(NOT SURFACE_DETACH EQUAL -1)
        evidence_cpp_prefix_has_exact_namespace_scope_in_sanitized(
            "${SURFACE_TOKEN}" ${SURFACE_DETACH} "<anonymous>"
            SURFACE_DETACH_SCOPE_VALID)
    else()
        set(SURFACE_DETACH_SCOPE_VALID FALSE)
    endif()
    message(STATUS "diag ${SURFACE_ID}_surface set_config_pos=${SURFACE_SET_CONFIG} set_config_scope=${SURFACE_SET_CONFIG_SCOPE_VALID} detach_pos=${SURFACE_DETACH} detach_scope=${SURFACE_DETACH_SCOPE_VALID}")
endforeach()

string(REGEX REPLACE "[ \t\r\n]+" "" OWNER_LEXICAL_COMPACT "${OWNER_LEXICAL_SOURCE}")
set(WRAPPER_SEAM
    "#include\"host_window_lifetime.hpp\"#include\"raylib_host.hpp\"#include\"window_settings.hpp\"#undefSetConfigFlags#undefInitWindow#undefIsWindowReady#undefCloseWindow#include<raylib.h>#undefSetConfigFlags#undefInitWindow#undefIsWindowReady#undefCloseWindow#ifdef_WIN32extern__declspec(dllimport)void*__stdcallImmAssociateContext(void*,void*);#endifnamespace{voidset_config_flags_noexcept(unsignedintflags)noexcept{SetConfigFlags(flags);}voidinit_window_noexcept(intwidth,intheight,constchar*title)noexcept{InitWindow(width,height,title);}boolis_window_ready_noexcept()noexcept{returnIsWindowReady();}void*detach_ime_context_noexcept()noexcept{#ifdef_WIN32void*constwindow=GetWindowHandle();returnwindow!=nullptr?ImmAssociateContext(window,nullptr):nullptr;#elsereturnnullptr;#endif}voidrestore_ime_context_noexcept(void*context)noexcept{#ifdef_WIN32void*constwindow=GetWindowHandle();if(window!=nullptr){static_cast<void>(ImmAssociateContext(window,context));}#elsestatic_cast<void>(context);#endif}voidclose_window_noexcept()noexcept{CloseWindow();}}")
string(FIND "${OWNER_LEXICAL_COMPACT}" "${WRAPPER_SEAM}" WRAPPER_SEAM_POSITION)
evidence_count_literal_occurrences("${OWNER_LEXICAL_COMPACT}"
    "#include<raylib.h>" RAYLIB_INCLUDE_COUNT)
foreach(LIFECYCLE_IDENTIFIER IN ITEMS
        SetConfigFlags InitWindow IsWindowReady CloseWindow)
    evidence_count_literal_occurrences("${OWNER_LEXICAL_COMPACT}"
        "#undef${LIFECYCLE_IDENTIFIER}" UNDEF_COUNT)
    if(NOT UNDEF_COUNT EQUAL 2)
        message(STATUS "diag fail=1162 identifier=${LIFECYCLE_IDENTIFIER} actual_count=${UNDEF_COUNT}")
        return()
    endif()
endforeach()
if(NOT WRAPPER_SEAM_POSITION EQUAL 0 OR NOT RAYLIB_INCLUDE_COUNT EQUAL 1)
    message(STATUS "diag fail=1167 seam_position=${WRAPPER_SEAM_POSITION} raylib_include_count=${RAYLIB_INCLUDE_COUNT}")
    return()
endif()

evidence_cpp_mask_preprocessor_directives_in_sanitized(
    "${OWNER_ACTIVE_SOURCE}" OWNER_SCOPE_SURFACE)
evidence_cpp_normalize_token_whitespace(
    "${OWNER_SCOPE_SURFACE}" OWNER_TOKEN_SURFACE)
string(FIND "${OWNER_TOKEN_SURFACE}" "namespace{" OWNER_ANONYMOUS_NAMESPACE_POSITION)
string(FIND "${OWNER_TOKEN_SURFACE}" "voidset_config_flags_noexcept(" OWNER_SET_CONFIG_POSITION)
string(LENGTH "${OWNER_TOKEN_SURFACE}" OWNER_TOKEN_LENGTH)
string(SHA256 OWNER_TOKEN_HASH "${OWNER_TOKEN_SURFACE}")
message(STATUS "diag owner_token length=${OWNER_TOKEN_LENGTH} sha256=${OWNER_TOKEN_HASH} namespace_position=${OWNER_ANONYMOUS_NAMESPACE_POSITION} set_config_position=${OWNER_SET_CONFIG_POSITION}")
evidence_try_find_unique_cpp_function_in_namespace_in_sanitized(
    "${OWNER_TOKEN_SURFACE}" "bool HostWindowLifetime::initialize("
    "arpg::platform" INITIALIZE_BEGIN INITIALIZE_OPEN INITIALIZE_END INITIALIZE_VALID)
if(NOT INITIALIZE_VALID)
    message(STATUS "diag fail=1180 initialize_valid=${INITIALIZE_VALID}")
    return()
endif()
math(EXPR INITIALIZE_LENGTH "${INITIALIZE_END} - ${INITIALIZE_BEGIN} + 1")
string(SUBSTRING "${OWNER_TOKEN_SURFACE}" ${INITIALIZE_BEGIN}
    ${INITIALIZE_LENGTH} INITIALIZE_FUNCTION)
evidence_cpp_contains_local_type_keyword_in_sanitized(
    "${INITIALIZE_FUNCTION}" INITIALIZE_HAS_LOCAL_TYPE)
if(INITIALIZE_HAS_LOCAL_TYPE)
    message(STATUS "diag fail=1190 initialize_has_local_type=${INITIALIZE_HAS_LOCAL_TYPE}")
    return()
endif()
evidence_cpp_direct_execution_surface_in_sanitized(
    "${INITIALIZE_FUNCTION}" INITIALIZE_DIRECT)

evidence_try_find_unique_cpp_function_in_namespace_in_sanitized(
    "${OWNER_TOKEN_SURFACE}" "HostWindowBackend raylib_host_window_backend("
    "arpg::platform" BACKEND_BEGIN BACKEND_OPEN BACKEND_END BACKEND_VALID)
if(NOT BACKEND_VALID)
    message(STATUS "diag fail=1201 backend_valid=${BACKEND_VALID}")
    return()
endif()
math(EXPR BACKEND_LENGTH "${BACKEND_END} - ${BACKEND_BEGIN} + 1")
string(SUBSTRING "${OWNER_TOKEN_SURFACE}" ${BACKEND_BEGIN} ${BACKEND_LENGTH}
    BACKEND_FUNCTION)
evidence_cpp_direct_execution_surface_in_sanitized(
    "${BACKEND_FUNCTION}" BACKEND_DIRECT)

string(REGEX REPLACE "[ \t\r\n]+" "" HOST_COMPACT "${HOST_ENTRY_DIRECT_SOURCE}")
string(REGEX REPLACE "[ \t\r\n]+" "" INITIALIZE_COMPACT "${INITIALIZE_DIRECT}")
string(REGEX REPLACE "[ \t\r\n]+" "" BACKEND_COMPACT "${BACKEND_DIRECT}")
set(HOST_INITIALIZE_CALL "window.initialize(config,committed_settings)")
string(LENGTH "${HOST_COMPACT}" BEFORE_LENGTH)
string(REPLACE "${HOST_INITIALIZE_CALL}" "" WITHOUT_INITIALIZE "${HOST_COMPACT}")
string(LENGTH "${WITHOUT_INITIALIZE}" AFTER_LENGTH)
string(LENGTH "${HOST_INITIALIZE_CALL}" INITIALIZE_CALL_LENGTH)
math(EXPR HOST_INITIALIZE_COUNT
    "(${BEFORE_LENGTH} - ${AFTER_LENGTH}) / ${INITIALIZE_CALL_LENGTH}")
string(FIND "${HOST_COMPACT}"
    "if(!${HOST_INITIALIZE_CALL})" HOST_INITIALIZE_GUARD)
if(NOT HOST_INITIALIZE_COUNT EQUAL 1 OR HOST_INITIALIZE_GUARD EQUAL -1)
    message(STATUS "diag fail=1228 initialize_count=${HOST_INITIALIZE_COUNT} guard_position=${HOST_INITIALIZE_GUARD}")
    return()
endif()

set(EXPECTED_INITIALIZE
    "boolHostWindowLifetime::initialize(constRaylibHostConfig&config,constsettings::SettingsData&settings)noexcept{if(!backend_.set_config_flags||!backend_.init_window||!backend_.is_window_ready||!backend_.detach_ime_context||!backend_.restore_ime_context||!backend_.close_window){ready_=false;returnfalse;}backend_.set_config_flags(initial_window_flags(settings));backend_.init_window(config.window_width,config.window_height,config.window_title);ready_=backend_.is_window_ready();if(ready_){detached_ime_context_=backend_.detach_ime_context();ime_context_detached_=true;}returnready_;}")
set(EXPECTED_BACKEND
    "HostWindowBackendraylib_host_window_backend()noexcept{return{&set_config_flags_noexcept,&init_window_noexcept,&is_window_ready_noexcept,&detach_ime_context_noexcept,&restore_ime_context_noexcept,&close_window_noexcept};}")
if(NOT INITIALIZE_COMPACT STREQUAL EXPECTED_INITIALIZE
        OR NOT BACKEND_COMPACT STREQUAL EXPECTED_BACKEND)
    message(STATUS "diag fail=1246 compact_initialize_or_backend_mismatch")
    diagnostic_first_difference("${INITIALIZE_COMPACT}" "${EXPECTED_INITIALIZE}" "1246_initialize")
    diagnostic_first_difference("${BACKEND_COMPACT}" "${EXPECTED_BACKEND}" "1246_backend")
    return()
endif()

set(wrapper_signature_set_config "void set_config_flags_noexcept(")
set(wrapper_signature_init "void init_window_noexcept(")
set(wrapper_signature_ready "bool is_window_ready_noexcept(")
set(wrapper_signature_detach "void* detach_ime_context_noexcept(")
set(wrapper_signature_restore "void restore_ime_context_noexcept(")
set(wrapper_signature_close "void close_window_noexcept(")
set(wrapper_expected_set_config "voidset_config_flags_noexcept(unsignedintflags)noexcept{SetConfigFlags(flags);}")
set(wrapper_expected_init "voidinit_window_noexcept(intwidth,intheight,constchar*title)noexcept{InitWindow(width,height,title);}")
set(wrapper_expected_ready "boolis_window_ready_noexcept()noexcept{returnIsWindowReady();}")
set(wrapper_expected_detach "void*detach_ime_context_noexcept()noexcept{}")
set(wrapper_expected_restore "voidrestore_ime_context_noexcept(void*context)noexcept{}")
set(wrapper_expected_close "voidclose_window_noexcept()noexcept{CloseWindow();}")
foreach(WRAPPER_ID IN ITEMS set_config init ready detach restore close)
    set(SIGNATURE_VARIABLE "wrapper_signature_${WRAPPER_ID}")
    set(EXPECTED_VARIABLE "wrapper_expected_${WRAPPER_ID}")
    string(FIND "${OWNER_TOKEN_SURFACE}" "${${SIGNATURE_VARIABLE}}"
        WRAPPER_CANDIDATE_BEGIN)
    if(NOT WRAPPER_CANDIDATE_BEGIN EQUAL -1)
        string(SUBSTRING "${OWNER_TOKEN_SURFACE}" ${WRAPPER_CANDIDATE_BEGIN}
            -1 WRAPPER_CANDIDATE_TAIL)
        evidence_try_find_cpp_function_bounds_in_sanitized(
            "${WRAPPER_CANDIDATE_TAIL}" "${${SIGNATURE_VARIABLE}}"
            CANDIDATE_RELATIVE_BEGIN CANDIDATE_RELATIVE_OPEN CANDIDATE_RELATIVE_END
            CANDIDATE_FUNCTION_VALID)
        evidence_cpp_prefix_has_exact_namespace_scope_in_sanitized(
            "${OWNER_TOKEN_SURFACE}" ${WRAPPER_CANDIDATE_BEGIN} "<anonymous>"
            CANDIDATE_NAMESPACE_VALID)
        message(STATUS "diag wrapper=${WRAPPER_ID} candidate_begin=${WRAPPER_CANDIDATE_BEGIN} function_valid=${CANDIDATE_FUNCTION_VALID} anonymous_namespace_valid=${CANDIDATE_NAMESPACE_VALID}")
    else()
        message(STATUS "diag wrapper=${WRAPPER_ID} candidate_begin=-1")
    endif()
    evidence_try_find_unique_cpp_function_in_namespace_in_sanitized(
        "${OWNER_TOKEN_SURFACE}" "${${SIGNATURE_VARIABLE}}" "<anonymous>"
        WRAPPER_BEGIN WRAPPER_OPEN WRAPPER_END WRAPPER_VALID)
    if(NOT WRAPPER_VALID)
        message(STATUS "diag fail=1281 wrapper=${WRAPPER_ID} valid=${WRAPPER_VALID}")
        return()
    endif()
    math(EXPR WRAPPER_LENGTH "${WRAPPER_END} - ${WRAPPER_BEGIN} + 1")
    string(SUBSTRING "${OWNER_TOKEN_SURFACE}" ${WRAPPER_BEGIN}
        ${WRAPPER_LENGTH} WRAPPER_FUNCTION)
    string(REGEX REPLACE "[ \t\r\n]+" "" WRAPPER_COMPACT
        "${WRAPPER_FUNCTION}")
    if(NOT WRAPPER_COMPACT STREQUAL "${${EXPECTED_VARIABLE}}")
        message(STATUS "diag fail=1290 wrapper=${WRAPPER_ID}")
        diagnostic_first_difference("${WRAPPER_COMPACT}"
            "${${EXPECTED_VARIABLE}}" "1290_${WRAPPER_ID}")
        return()
    endif()
endforeach()

evidence_window_lifecycle_owner_surface_is_valid(
    "${OWNER_ACTIVE_SOURCE}" "${OWNER_LEXICAL_SOURCE}" TRUE OWNER_SURFACE_VALID)
if(NOT OWNER_SURFACE_VALID)
    message(STATUS "diag fail=1297 owner_surface_valid=${OWNER_SURFACE_VALID}")
    return()
endif()
message(STATUS "diag success=all_boundary_checks_pass")
endfunction()
diagnose_window_boundary()

# Reproduce the formal guard's per-production-source preparation for the
# death overlay without scanning the rest of the repository.
set(DEATH_OVERLAY_SOURCE_PATH
    "${RAYLIB_SOURCE_DIR}/death_overlay_renderer.cpp")
file(READ "${DEATH_OVERLAY_SOURCE_PATH}" DEATH_OVERLAY_SOURCE)
stage17_unconditional_cpp_surface(
    "${DEATH_OVERLAY_SOURCE}" DEATH_OVERLAY_ACTIVE)
arpg_sanitize_cpp_source(
    "${DEATH_OVERLAY_SOURCE}" DEATH_OVERLAY_LEXICAL)
evidence_raylib_lifecycle_source_role(
    "${DEATH_OVERLAY_SOURCE_PATH}" "${REPO_ROOT}"
    DEATH_OVERLAY_ROLE)
evidence_window_lifecycle_source_surface_is_valid(
    "${DEATH_OVERLAY_ACTIVE}" "${DEATH_OVERLAY_LEXICAL}"
    "${DEATH_OVERLAY_ROLE}" DEATH_OVERLAY_VALID)
message(STATUS
    "diag death_overlay role=${DEATH_OVERLAY_ROLE} valid=${DEATH_OVERLAY_VALID}")

foreach(IDENTIFIER IN ITEMS
        SetConfigFlags InitWindow IsWindowReady CloseWindow WindowShouldClose
        GetWindowHandle ImmAssociateContext)
    set(EXPECTED_ACTIVE 0)
    set(EXPECTED_LEXICAL 0)
    if(IDENTIFIER STREQUAL IsWindowReady
            AND DEATH_OVERLAY_ROLE STREQUAL ready-2)
        set(EXPECTED_ACTIVE 2)
        set(EXPECTED_LEXICAL 2)
    endif()
    evidence_count_cpp_identifier(
        "${DEATH_OVERLAY_ACTIVE}" "${IDENTIFIER}" ACTUAL_ACTIVE)
    evidence_count_cpp_identifier(
        "${DEATH_OVERLAY_LEXICAL}" "${IDENTIFIER}" ACTUAL_LEXICAL)
    message(STATUS
        "diag death_overlay identifier=${IDENTIFIER} active=${ACTUAL_ACTIVE}/${EXPECTED_ACTIVE} lexical=${ACTUAL_LEXICAL}/${EXPECTED_LEXICAL}")
endforeach()
