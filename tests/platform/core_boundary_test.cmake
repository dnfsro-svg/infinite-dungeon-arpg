if(NOT DEFINED CORE_DIR)
    message(FATAL_ERROR "CORE_DIR is required")
endif()

file(GLOB_RECURSE CORE_FILES
    LIST_DIRECTORIES FALSE
    "${CORE_DIR}/*.h"
    "${CORE_DIR}/*.hpp"
    "${CORE_DIR}/*.inl"
    "${CORE_DIR}/*.c"
    "${CORE_DIR}/*.cc"
    "${CORE_DIR}/*.cpp")

foreach(CORE_FILE IN LISTS CORE_FILES)
    file(READ "${CORE_FILE}" CORE_CONTENT)
    string(TOLOWER "${CORE_CONTENT}" CORE_CONTENT_LOWER)
    if(CORE_CONTENT_LOWER MATCHES
            "raylib\\.h|raymath\\.h|rlgl\\.h|raylib-cpp")
        message(FATAL_ERROR
            "Core file depends on raylib: ${CORE_FILE}")
    endif()
endforeach()
