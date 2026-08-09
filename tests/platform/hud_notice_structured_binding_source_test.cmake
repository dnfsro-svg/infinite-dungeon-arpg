cmake_minimum_required(VERSION 3.25)

if(NOT DEFINED HUD_NOTICE_SOURCE OR NOT EXISTS "${HUD_NOTICE_SOURCE}")
    message(FATAL_ERROR "HUD notice source is required")
endif()

file(READ "${HUD_NOTICE_SOURCE}" SOURCE)

string(REGEX REPLACE "/\\*([^*]|\\*+[^*/])*\\*+/" "" CODE "${SOURCE}")
string(REGEX REPLACE "//[^\r\n]*" "" CODE "${CODE}")
string(REGEX REPLACE "[ \t\r\n]+" " " NORMALIZED_CODE "${CODE}")

foreach(FORBIDDEN_REGEX IN ITEMS
        "hints[ \t\r\n]*\\.[ \t\r\n]*(primary|secondary)"
        "(std::)?strstr[ \t\r\n]*\\("
        "(std::)?strchr[ \t\r\n]*\\("
        "(std::)?memcmp[ \t\r\n]*\\("
        "(std::)?string_view"
        "\\.[ \t\r\n]*find[ \t\r\n]*\\("
        "\"Interact\""
        "\"Inventory\""
        "\"Passive Tree\"")
    string(REGEX MATCH "${FORBIDDEN_REGEX}" FOUND "${CODE}")
    if(FOUND)
        message(FATAL_ERROR
            "HUD notice display-text parsing is forbidden: ${FORBIDDEN_REGEX}")
    endif()
endforeach()

set(REQUIRED_FORMAT_CALL
    "format_notice(notice, \"%s %s\", control_hint_key_label(hints, action), suffix);")
string(FIND "${NORMALIZED_CODE}" "${REQUIRED_FORMAT_CALL}" FOUND)
if(FOUND EQUAL -1)
    message(FATAL_ERROR
        "HUD notice action formatter must consume control_hint_key_label")
endif()

foreach(REQUIRED_ACTION IN ITEMS interact inventory passive_tree)
    set(REQUIRED_CALL_REGEX
        "add_action_notice\\([^;]*hints,[ \t\r\n]*settings::SettingAction::${REQUIRED_ACTION},")
    string(REGEX MATCH "${REQUIRED_CALL_REGEX}" FOUND "${CODE}")
    if(NOT FOUND)
        message(FATAL_ERROR
            "HUD notice structured action call missing: ${REQUIRED_ACTION}")
    endif()
endforeach()
