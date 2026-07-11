if(NOT DEFINED CORE_DIR)
    message(FATAL_ERROR "CORE_DIR is required")
endif()

set(ARPG_FORBIDDEN_SOURCE_INCLUDE_REGEX
    [=[^[ \t]*#[ \t]*include[ \t]*[<"]([^>"]*[/\\])?(raylib\.h|raymath\.h|rlgl\.h|raylib-cpp[^>"]*)[>"]]=])

foreach(POSITIVE_INCLUDE IN ITEMS
        "#include <raylib.h>"
        "  # include \"vendor/raymath.h\""
        "#include <rlgl.h>"
        "#include <raylib-cpp/raylib-cpp.hpp>")
    string(TOLOWER "${POSITIVE_INCLUDE}" POSITIVE_INCLUDE_LOWER)
    if(NOT POSITIVE_INCLUDE_LOWER MATCHES
            "${ARPG_FORBIDDEN_SOURCE_INCLUDE_REGEX}")
        message(FATAL_ERROR
            "Include regex self-test missed: ${POSITIVE_INCLUDE}")
    endif()
endforeach()

foreach(NEGATIVE_INCLUDE IN ITEMS
        "// #include <raylib.h>"
        "/* #include <raymath.h> */"
        "const char* text = \"#include <rlgl.h>\""
        "#include <array>"
        "#include <my_raylib.h>"
        "#include <raylib.hpp>")
    string(TOLOWER "${NEGATIVE_INCLUDE}" NEGATIVE_INCLUDE_LOWER)
    if(NEGATIVE_INCLUDE_LOWER MATCHES
            "${ARPG_FORBIDDEN_SOURCE_INCLUDE_REGEX}")
        message(FATAL_ERROR
            "Include regex self-test false positive: ${NEGATIVE_INCLUDE}")
    endif()
endforeach()

file(GLOB_RECURSE CORE_FILES
    LIST_DIRECTORIES FALSE
    "${CORE_DIR}/*.h"
    "${CORE_DIR}/*.hh"
    "${CORE_DIR}/*.hpp"
    "${CORE_DIR}/*.hxx"
    "${CORE_DIR}/*.inl"
    "${CORE_DIR}/*.ipp"
    "${CORE_DIR}/*.tpp"
    "${CORE_DIR}/*.c"
    "${CORE_DIR}/*.cc"
    "${CORE_DIR}/*.cpp"
    "${CORE_DIR}/*.cxx"
    "${CORE_DIR}/*.ixx"
    "${CORE_DIR}/*.cppm")

foreach(CORE_FILE IN LISTS CORE_FILES)
    file(STRINGS "${CORE_FILE}" CORE_LINES)
    foreach(CORE_LINE IN LISTS CORE_LINES)
        string(TOLOWER "${CORE_LINE}" CORE_LINE_LOWER)
        if(CORE_LINE_LOWER MATCHES
                "${ARPG_FORBIDDEN_SOURCE_INCLUDE_REGEX}")
            message(FATAL_ERROR
                "Core file depends on raylib: ${CORE_FILE}: ${CORE_LINE}")
        endif()
    endforeach()
endforeach()
