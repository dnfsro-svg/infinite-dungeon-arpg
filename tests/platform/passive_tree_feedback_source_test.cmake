if(NOT DEFINED RAYLIB_SOURCE_DIR)
    message(FATAL_ERROR "RAYLIB_SOURCE_DIR is required")
endif()

include("${CMAKE_CURRENT_LIST_DIR}/cpp_source_lexer.cmake")

file(READ "${RAYLIB_SOURCE_DIR}/passive_tree_renderer.cpp" RENDERER_SOURCE)
arpg_sanitize_cpp_source("${RENDERER_SOURCE}" RENDERER_CODE)

foreach(REQUIRED_CALL IN ITEMS
        "passive_node_action_text(snapshot, node)"
        "passive_tree_status_view({"
        "DrawText(action_text,"
        "DrawText(status.text,")
    string(FIND "${RENDERER_CODE}" "${REQUIRED_CALL}" REQUIRED_CALL_INDEX)
    if(REQUIRED_CALL_INDEX EQUAL -1)
        message(FATAL_ERROR
            "passive-tree renderer must consume the tested node-action and status feedback helpers: ${REQUIRED_CALL}")
    endif()
endforeach()

foreach(FORBIDDEN_LITERAL IN ITEMS
        "Click to allocate (autosaves)"
        "Autosave ERROR")
    string(FIND "${RENDERER_SOURCE}" "\"${FORBIDDEN_LITERAL}\""
        FORBIDDEN_LITERAL_INDEX)
    if(NOT FORBIDDEN_LITERAL_INDEX EQUAL -1)
        message(FATAL_ERROR
            "passive-tree renderer must not bypass tested feedback mapping: ${FORBIDDEN_LITERAL}")
    endif()
endforeach()

message(STATUS
    "[CPLAY-034] passive-tree renderer consumes precise node and status feedback")
