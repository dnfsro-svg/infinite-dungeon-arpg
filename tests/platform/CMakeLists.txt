foreach(PROPERTY_NAME IN ITEMS
        LINK_LIBRARIES
        INTERFACE_LINK_LIBRARIES
        INCLUDE_DIRECTORIES
        INTERFACE_INCLUDE_DIRECTORIES)
    get_target_property(PROPERTY_VALUE arpg_core "${PROPERTY_NAME}")
    if(PROPERTY_VALUE)
        string(TOLOWER "${PROPERTY_VALUE}" PROPERTY_VALUE_LOWER)
        if(PROPERTY_VALUE_LOWER MATCHES "raylib|_deps[/\\\\]raylib")
            message(FATAL_ERROR
                "arpg_core ${PROPERTY_NAME} leaks raylib: ${PROPERTY_VALUE}")
        endif()
    endif()
endforeach()

add_test(
    NAME architecture.core_no_raylib
    COMMAND "${CMAKE_COMMAND}"
        "-DCORE_DIR=${PROJECT_SOURCE_DIR}/src/core"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/core_boundary_test.cmake")
set_tests_properties(
    architecture.core_no_raylib
    PROPERTIES LABELS "headless;architecture")
