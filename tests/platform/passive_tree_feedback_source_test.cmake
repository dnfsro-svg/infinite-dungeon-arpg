if(NOT DEFINED RAYLIB_SOURCE_DIR)
    message(FATAL_ERROR "RAYLIB_SOURCE_DIR is required")
endif()

include("${CMAKE_CURRENT_LIST_DIR}/cpp_source_lexer.cmake")

file(READ "${RAYLIB_SOURCE_DIR}/passive_tree_renderer.cpp" RENDERER_SOURCE)
file(READ "${RAYLIB_SOURCE_DIR}/passive_tree_view_math.cpp" VIEW_SOURCE)
arpg_sanitize_cpp_source("${RENDERER_SOURCE}" RENDERER_CODE)
arpg_sanitize_cpp_source("${VIEW_SOURCE}" VIEW_CODE)

foreach(REQUIRED_CALL IN ITEMS
        "passive_node_text_view(snapshot, node)"
        "passive_tree_status_view({"
        "text.action.data(), layout.action"
        "status.text, layout.status")
    string(FIND "${RENDERER_CODE}" "${REQUIRED_CALL}" REQUIRED_CALL_INDEX)
    if(REQUIRED_CALL_INDEX EQUAL -1)
        message(FATAL_ERROR
            "passive-tree renderer must consume the tested node-action and status feedback helpers: ${REQUIRED_CALL}")
    endif()
endforeach()

foreach(REQUIRED_FEEDBACK IN ITEMS
        "u8\"点击分配并保存\""
        "u8\"点击退款并保存\""
        "u8\"请分配相邻节点\""
        "u8\"退款错误：将断链\""
        "u8\"保存状态：保存中\""
        "u8\"保存状态：就绪\""
        "u8\"保存状态：错误\"")
    string(FIND "${VIEW_SOURCE}" "${REQUIRED_FEEDBACK}"
        REQUIRED_FEEDBACK_INDEX)
    if(REQUIRED_FEEDBACK_INDEX EQUAL -1)
        message(FATAL_ERROR
            "passive-tree precise Chinese feedback is missing: ${REQUIRED_FEEDBACK}")
    endif()
endforeach()

foreach(FORBIDDEN_LITERAL IN ITEMS
        "Click to allocate" "Click to refund" "Autosave"
        "Unknown passive" "No passive points"
        "Allocate an adjacent" "Refund would disconnect")
    string(FIND "${RENDERER_SOURCE}${VIEW_SOURCE}" "${FORBIDDEN_LITERAL}"
        FORBIDDEN_LITERAL_INDEX)
    if(NOT FORBIDDEN_LITERAL_INDEX EQUAL -1)
        message(FATAL_ERROR
            "passive-tree renderer must not bypass tested feedback mapping: ${FORBIDDEN_LITERAL}")
    endif()
endforeach()

string(FIND "${VIEW_CODE}" "if (input.save_error)" SAVE_ERROR_INDEX)
string(FIND "${VIEW_CODE}" "if (input.saving)" SAVING_INDEX)
string(FIND "${VIEW_CODE}" "switch (input.rule_error)" RULE_ERROR_INDEX)
if(SAVE_ERROR_INDEX EQUAL -1 OR SAVING_INDEX EQUAL -1
        OR RULE_ERROR_INDEX EQUAL -1
        OR NOT SAVE_ERROR_INDEX LESS SAVING_INDEX
        OR NOT SAVING_INDEX LESS RULE_ERROR_INDEX)
    message(FATAL_ERROR
        "save error must outrank saving, and saving must outrank rule feedback")
endif()

foreach(FORBIDDEN_DRAW IN ITEMS "DrawText(" "MeasureText(" "GetFontDefault(")
    string(FIND "${RENDERER_CODE}" "${FORBIDDEN_DRAW}" FORBIDDEN_DRAW_INDEX)
    if(NOT FORBIDDEN_DRAW_INDEX EQUAL -1)
        message(FATAL_ERROR
            "passive-tree feedback must use SharedUiFont without fallback: ${FORBIDDEN_DRAW}")
    endif()
endforeach()

message(STATUS
    "[CPLAY-034] passive-tree renderer consumes precise node and status feedback")
