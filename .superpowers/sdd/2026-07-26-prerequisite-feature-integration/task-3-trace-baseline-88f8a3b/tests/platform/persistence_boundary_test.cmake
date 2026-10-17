if(NOT DEFINED SOURCE_DIR)
    message(FATAL_ERROR "SOURCE_DIR is required")
endif()
if(NOT DEFINED SOURCE_LABEL)
    message(FATAL_ERROR "SOURCE_LABEL is required")
endif()

file(GLOB_RECURSE SOURCE_FILES
    LIST_DIRECTORIES FALSE
    "${SOURCE_DIR}/*.h"
    "${SOURCE_DIR}/*.hh"
    "${SOURCE_DIR}/*.hpp"
    "${SOURCE_DIR}/*.hxx"
    "${SOURCE_DIR}/*.inl"
    "${SOURCE_DIR}/*.inc"
    "${SOURCE_DIR}/*.ipp"
    "${SOURCE_DIR}/*.tpp"
    "${SOURCE_DIR}/*.tcc"
    "${SOURCE_DIR}/*.c"
    "${SOURCE_DIR}/*.cc"
    "${SOURCE_DIR}/*.cpp"
    "${SOURCE_DIR}/*.cxx"
    "${SOURCE_DIR}/*.ixx"
    "${SOURCE_DIR}/*.cppm"
    "${SOURCE_DIR}/*.mpp")

foreach(SOURCE_FILE IN LISTS SOURCE_FILES)
    file(STRINGS "${SOURCE_FILE}" SOURCE_LINES)
    foreach(SOURCE_LINE IN LISTS SOURCE_LINES)
        string(TOLOWER "${SOURCE_LINE}" SOURCE_LINE_LOWER)
        if(SOURCE_LINE_LOWER MATCHES
                "^[ \\t]*#[ \\t]*include[ \\t]*[<\"]dungeon[/\\\\][^>\"]+[>\"]")
            if(NOT SOURCE_LINE_LOWER MATCHES
                    "^[ \\t]*#[ \\t]*include[ \\t]*[<\"]dungeon[/\\\\]dungeon_checkpoint\\.hpp[>\"]")
                message(FATAL_ERROR
                    "${SOURCE_LABEL} file includes a forbidden Dungeon header: ${SOURCE_FILE}: ${SOURCE_LINE}")
            endif()
        endif()
        if(SOURCE_LINE_LOWER MATCHES
                "^[ \\t]*#[ \\t]*include[ \\t]*[<\"]combat[/\\\\][^>\"]+[>\"]")
            message(FATAL_ERROR
                "${SOURCE_LABEL} file depends on Combat: ${SOURCE_FILE}: ${SOURCE_LINE}")
        endif()
    endforeach()
endforeach()
