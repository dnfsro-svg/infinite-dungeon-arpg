include_guard(GLOBAL)

function(_arpg_dependency_target_candidates INPUT_VALUE OUT_TARGETS)
    set(_arpg_candidates "")
    if(TARGET "${INPUT_VALUE}")
        list(APPEND _arpg_candidates "${INPUT_VALUE}")
    endif()

    string(REGEX MATCHALL
        "[A-Za-z0-9_.+-]+::[A-Za-z0-9_.+-]+"
        _arpg_namespaced_candidates
        "${INPUT_VALUE}")
    foreach(_arpg_candidate IN LISTS _arpg_namespaced_candidates)
        if(TARGET "${_arpg_candidate}")
            list(APPEND _arpg_candidates "${_arpg_candidate}")
        endif()
    endforeach()

    string(REGEX MATCHALL
        "[A-Za-z_][A-Za-z0-9_.+-]*"
        _arpg_plain_candidates
        "${INPUT_VALUE}")
    foreach(_arpg_candidate IN LISTS _arpg_plain_candidates)
        if(TARGET "${_arpg_candidate}")
            list(APPEND _arpg_candidates "${_arpg_candidate}")
        endif()
    endforeach()

    list(REMOVE_DUPLICATES _arpg_candidates)
    set("${OUT_TARGETS}" "${_arpg_candidates}" PARENT_SCOPE)
endfunction()

function(_arpg_canonical_target_name TARGET_NAME OUT_TARGET)
    get_target_property(_arpg_aliased_target "${TARGET_NAME}" ALIASED_TARGET)
    if(_arpg_aliased_target)
        set("${OUT_TARGET}" "${_arpg_aliased_target}" PARENT_SCOPE)
    else()
        set("${OUT_TARGET}" "${TARGET_NAME}" PARENT_SCOPE)
    endif()
endfunction()

function(_arpg_find_target_dependency_path
        TARGET_NAME FORBIDDEN_TARGET TARGET_PATH OUT_FOUND OUT_PATH)
    _arpg_canonical_target_name("${TARGET_NAME}" _arpg_target)
    _arpg_canonical_target_name("${FORBIDDEN_TARGET}" _arpg_forbidden)
    if(_arpg_target STREQUAL _arpg_forbidden)
        set("${OUT_FOUND}" TRUE PARENT_SCOPE)
        set("${OUT_PATH}" "${TARGET_PATH}" PARENT_SCOPE)
        return()
    endif()

    get_property(_arpg_visited GLOBAL
        PROPERTY ARPG_TARGET_DEPENDENCY_BOUNDARY_VISITED)
    list(FIND _arpg_visited "${_arpg_target}" _arpg_visited_index)
    if(NOT _arpg_visited_index EQUAL -1)
        set("${OUT_FOUND}" FALSE PARENT_SCOPE)
        set("${OUT_PATH}" "" PARENT_SCOPE)
        return()
    endif()
    set_property(GLOBAL APPEND
        PROPERTY ARPG_TARGET_DEPENDENCY_BOUNDARY_VISITED "${_arpg_target}")

    foreach(_arpg_property IN ITEMS
            LINK_LIBRARIES INTERFACE_LINK_LIBRARIES)
        get_target_property(
            _arpg_link_value "${_arpg_target}" "${_arpg_property}")
        if("${_arpg_link_value}" STREQUAL ""
                OR "${_arpg_link_value}" MATCHES "-NOTFOUND$")
            continue()
        endif()

        _arpg_dependency_target_candidates(
            "${_arpg_link_value}" _arpg_dependencies)
        foreach(_arpg_dependency IN LISTS _arpg_dependencies)
            _arpg_find_target_dependency_path(
                "${_arpg_dependency}"
                "${_arpg_forbidden}"
                "${TARGET_PATH} -> ${_arpg_dependency}"
                _arpg_found
                _arpg_path)
            if(_arpg_found)
                set("${OUT_FOUND}" TRUE PARENT_SCOPE)
                set("${OUT_PATH}" "${_arpg_path}" PARENT_SCOPE)
                return()
            endif()
        endforeach()
    endforeach()

    set("${OUT_FOUND}" FALSE PARENT_SCOPE)
    set("${OUT_PATH}" "" PARENT_SCOPE)
endfunction()

function(arpg_assert_target_dependency_boundary ROOT_TARGET)
    if(NOT TARGET "${ROOT_TARGET}")
        message(FATAL_ERROR
            "Target dependency boundary root does not exist: '${ROOT_TARGET}'")
    endif()
    if(ARGC LESS 2)
        message(FATAL_ERROR
            "Target dependency boundary requires at least one forbidden target")
    endif()

    foreach(_arpg_forbidden IN LISTS ARGN)
        if(NOT TARGET "${_arpg_forbidden}")
            message(FATAL_ERROR
                "Target dependency boundary forbidden target does not exist: '${_arpg_forbidden}'")
        endif()
        set_property(GLOBAL
            PROPERTY ARPG_TARGET_DEPENDENCY_BOUNDARY_VISITED "")
        _arpg_find_target_dependency_path(
            "${ROOT_TARGET}"
            "${_arpg_forbidden}"
            "${ROOT_TARGET}"
            _arpg_found
            _arpg_path)
        if(_arpg_found)
            message(FATAL_ERROR
                "Target dependency boundary violation: '${ROOT_TARGET}' reaches forbidden target '${_arpg_forbidden}' via ${_arpg_path}")
        endif()
    endforeach()
endfunction()
