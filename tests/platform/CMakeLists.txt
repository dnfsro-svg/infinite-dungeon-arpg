function(arpg_tokenize_dependency_value INPUT_VALUE OUT_TOKENS)
    set(_arpg_token_text "${INPUT_VALUE}")
    foreach(_arpg_delimiter IN ITEMS
            "$" "<" ">" "{" "}" "(" ")" "," ":" "/" "\\")
        string(REPLACE
            "${_arpg_delimiter}" ";" _arpg_token_text "${_arpg_token_text}")
    endforeach()

    set(_arpg_tokens "")
    foreach(_arpg_token IN LISTS _arpg_token_text)
        string(STRIP "${_arpg_token}" _arpg_token)
        if(NOT _arpg_token STREQUAL "")
            list(APPEND _arpg_tokens "${_arpg_token}")
        endif()
    endforeach()
    set("${OUT_TOKENS}" "${_arpg_tokens}" PARENT_SCOPE)
endfunction()

function(arpg_value_has_forbidden_dependency
        INPUT_VALUE OUT_FOUND OUT_MATCH)
    string(TOLOWER "${INPUT_VALUE}" _arpg_value_lower)
    arpg_tokenize_dependency_value("${_arpg_value_lower}" _arpg_tokens)
    foreach(_arpg_token IN LISTS _arpg_tokens)
        if(_arpg_token MATCHES
                "^(lib)?(raylib|rlgl|raymath)([-_][a-z0-9.+-]+)?(\\.(lib|a))?$")
            set("${OUT_FOUND}" TRUE PARENT_SCOPE)
            set("${OUT_MATCH}" "${_arpg_token}" PARENT_SCOPE)
            return()
        endif()
    endforeach()

    set("${OUT_FOUND}" FALSE PARENT_SCOPE)
    set("${OUT_MATCH}" "" PARENT_SCOPE)
endfunction()

function(arpg_extract_target_candidates INPUT_VALUE OUT_TARGETS)
    set(_arpg_targets "")
    if(TARGET "${INPUT_VALUE}")
        list(APPEND _arpg_targets "${INPUT_VALUE}")
    endif()

    string(REGEX MATCHALL
        "[A-Za-z0-9_.+-]+::[A-Za-z0-9_.+-]+"
        _arpg_namespaced_candidates
        "${INPUT_VALUE}")
    foreach(_arpg_candidate IN LISTS _arpg_namespaced_candidates)
        if(TARGET "${_arpg_candidate}")
            list(APPEND _arpg_targets "${_arpg_candidate}")
        endif()
    endforeach()

    arpg_tokenize_dependency_value("${INPUT_VALUE}" _arpg_tokens)
    foreach(_arpg_candidate IN LISTS _arpg_tokens)
        if(TARGET "${_arpg_candidate}")
            list(APPEND _arpg_targets "${_arpg_candidate}")
        endif()
    endforeach()

    list(REMOVE_DUPLICATES _arpg_targets)
    set("${OUT_TARGETS}" "${_arpg_targets}" PARENT_SCOPE)
endfunction()

function(arpg_walk_target_dependency_closure
        TARGET_NAME TARGET_PATH OUT_FOUND OUT_REASON)
    if(NOT TARGET "${TARGET_NAME}")
        set("${OUT_FOUND}" FALSE PARENT_SCOPE)
        set("${OUT_REASON}" "" PARENT_SCOPE)
        return()
    endif()

    get_target_property(_arpg_aliased_target "${TARGET_NAME}" ALIASED_TARGET)
    if(_arpg_aliased_target)
        set(_arpg_inspected_target "${_arpg_aliased_target}")
    else()
        set(_arpg_inspected_target "${TARGET_NAME}")
    endif()

    get_property(_arpg_visited GLOBAL PROPERTY ARPG_BOUNDARY_VISITED_TARGETS)
    list(FIND _arpg_visited "${_arpg_inspected_target}" _arpg_visited_index)
    if(NOT _arpg_visited_index EQUAL -1)
        set("${OUT_FOUND}" FALSE PARENT_SCOPE)
        set("${OUT_REASON}" "" PARENT_SCOPE)
        return()
    endif()
    set_property(
        GLOBAL APPEND PROPERTY
        ARPG_BOUNDARY_VISITED_TARGETS "${_arpg_inspected_target}")

    foreach(_arpg_property_name IN ITEMS
            LINK_LIBRARIES
            INTERFACE_LINK_LIBRARIES)
        get_target_property(
            _arpg_property_value
            "${_arpg_inspected_target}"
            "${_arpg_property_name}")
        if("${_arpg_property_value}" STREQUAL ""
                OR "${_arpg_property_value}" MATCHES "-NOTFOUND$")
            continue()
        endif()

        arpg_value_has_forbidden_dependency(
            "${_arpg_property_value}" _arpg_found _arpg_match)
        if(_arpg_found)
            set("${OUT_FOUND}" TRUE PARENT_SCOPE)
            set("${OUT_REASON}"
                "${TARGET_PATH} ${_arpg_property_name} contains '${_arpg_match}' in '${_arpg_property_value}'"
                PARENT_SCOPE)
            return()
        endif()

        arpg_extract_target_candidates(
            "${_arpg_property_value}" _arpg_dependencies)
        foreach(_arpg_dependency IN LISTS _arpg_dependencies)
            arpg_walk_target_dependency_closure(
                "${_arpg_dependency}"
                "${TARGET_PATH} -> ${_arpg_dependency}"
                _arpg_nested_found
                _arpg_nested_reason)
            if(_arpg_nested_found)
                set("${OUT_FOUND}" TRUE PARENT_SCOPE)
                set("${OUT_REASON}" "${_arpg_nested_reason}" PARENT_SCOPE)
                return()
            endif()
        endforeach()
    endforeach()

    foreach(_arpg_property_name IN ITEMS
            INCLUDE_DIRECTORIES
            INTERFACE_INCLUDE_DIRECTORIES)
        get_target_property(
            _arpg_property_value
            "${_arpg_inspected_target}"
            "${_arpg_property_name}")
        if("${_arpg_property_value}" STREQUAL ""
                OR "${_arpg_property_value}" MATCHES "-NOTFOUND$")
            continue()
        endif()

        arpg_value_has_forbidden_dependency(
            "${_arpg_property_value}" _arpg_found _arpg_match)
        if(_arpg_found)
            set("${OUT_FOUND}" TRUE PARENT_SCOPE)
            set("${OUT_REASON}"
                "${TARGET_PATH} ${_arpg_property_name} contains '${_arpg_match}' in '${_arpg_property_value}'"
                PARENT_SCOPE)
            return()
        endif()
    endforeach()

    set("${OUT_FOUND}" FALSE PARENT_SCOPE)
    set("${OUT_REASON}" "" PARENT_SCOPE)
endfunction()

function(arpg_find_target_dependency_leak ROOT_TARGET OUT_FOUND OUT_REASON)
    set_property(GLOBAL PROPERTY ARPG_BOUNDARY_VISITED_TARGETS "")
    arpg_walk_target_dependency_closure(
        "${ROOT_TARGET}" "${ROOT_TARGET}" _arpg_found _arpg_reason)
    set("${OUT_FOUND}" "${_arpg_found}" PARENT_SCOPE)
    set("${OUT_REASON}" "${_arpg_reason}" PARENT_SCOPE)
endfunction()

function(arpg_assert_target_dependency_boundary ROOT_TARGET)
    arpg_find_target_dependency_leak(
        "${ROOT_TARGET}" _arpg_found _arpg_reason)
    if(_arpg_found)
        message(FATAL_ERROR
            "${ROOT_TARGET} dependency closure leaks raylib: ${_arpg_reason}")
    endif()
endfunction()

foreach(_arpg_positive_value IN ITEMS
        "raylib"
        "raylib::raylib"
        [=[$<LINK_ONLY:raylib>]=]
        [=[$<IF:$<BOOL:1>,clean,raylib>]=]
        "rlgl"
        "raymath"
        "raylib-cpp"
        "E:/vendor/raylib.lib"
        "E:/vendor/libraylib.a"
        "E:/build/_deps/raylib-src/include")
    arpg_value_has_forbidden_dependency(
        "${_arpg_positive_value}" _arpg_found _arpg_match)
    if(NOT _arpg_found)
        message(FATAL_ERROR
            "Boundary detector self-test missed: ${_arpg_positive_value}")
    endif()
endforeach()

arpg_value_has_forbidden_dependency(
    "E:/game/.worktrees/m00-raylib-host/src" _arpg_found _arpg_match)
if(_arpg_found)
    message(FATAL_ERROR
        "Boundary detector self-test falsely matched m00-raylib-host")
endif()

add_library(arpg_boundary_probe_leaf INTERFACE)
target_link_libraries(arpg_boundary_probe_leaf INTERFACE raylib)

add_library(arpg_boundary_probe_mid INTERFACE)
target_link_libraries(
    arpg_boundary_probe_mid
    INTERFACE arpg_boundary_probe_leaf)

foreach(_arpg_candidate_value IN ITEMS
        "arpg_boundary_probe_leaf"
        [=[$<LINK_ONLY:arpg_boundary_probe_leaf>]=]
        [=[$<IF:$<BOOL:1>,arpg_boundary_probe_leaf,clean>]=])
    arpg_extract_target_candidates(
        "${_arpg_candidate_value}" _arpg_candidates)
    list(FIND
        _arpg_candidates arpg_boundary_probe_leaf _arpg_candidate_index)
    if(_arpg_candidate_index EQUAL -1)
        message(FATAL_ERROR
            "Target parser self-test missed: ${_arpg_candidate_value}")
    endif()
endforeach()

arpg_find_target_dependency_leak(
    arpg_boundary_probe_mid _arpg_found _arpg_reason)
if(NOT _arpg_found)
    message(FATAL_ERROR
        "Recursive boundary self-test missed leaf -> mid -> raylib")
endif()

add_library(arpg_boundary_include_probe_leaf INTERFACE)
target_include_directories(
    arpg_boundary_include_probe_leaf
    INTERFACE "E:/build/_deps/raylib-src/include")

add_library(arpg_boundary_include_probe_mid INTERFACE)
target_link_libraries(
    arpg_boundary_include_probe_mid
    INTERFACE arpg_boundary_include_probe_leaf)

arpg_find_target_dependency_leak(
    arpg_boundary_include_probe_mid _arpg_found _arpg_reason)
if(NOT _arpg_found)
    message(FATAL_ERROR
        "Recursive boundary self-test missed _deps/raylib include path")
endif()

arpg_assert_target_dependency_boundary(arpg_core)

add_test(
    NAME architecture.core_no_raylib
    COMMAND "${CMAKE_COMMAND}"
        "-DCORE_DIR=${PROJECT_SOURCE_DIR}/src/core"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/core_boundary_test.cmake")
set_tests_properties(
    architecture.core_no_raylib
    PROPERTIES LABELS "headless;architecture")
