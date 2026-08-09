cmake_minimum_required(VERSION 3.25)

if(NOT DEFINED HUD_NOTICE_SOURCE OR NOT EXISTS "${HUD_NOTICE_SOURCE}")
    message(FATAL_ERROR "HUD notice source is required")
endif()

file(READ "${HUD_NOTICE_SOURCE}" SOURCE)

foreach(FORBIDDEN IN ITEMS
        "hints.secondary"
        "std::memcmp"
        "memcmp("
        "\"Interact\""
        "\"Inventory\""
        "\"Passive Tree\"")
    string(FIND "${SOURCE}" "${FORBIDDEN}" FOUND)
    if(NOT FOUND EQUAL -1)
        message(FATAL_ERROR
            "HUD notice display-text parsing is forbidden: ${FORBIDDEN}")
    endif()
endforeach()

foreach(REQUIRED IN ITEMS
        "control_hint_key_label(hints, action)"
        "settings::SettingAction::interact"
        "settings::SettingAction::inventory"
        "settings::SettingAction::passive_tree")
    string(FIND "${SOURCE}" "${REQUIRED}" FOUND)
    if(FOUND EQUAL -1)
        message(FATAL_ERROR
            "HUD notice structured binding token missing: ${REQUIRED}")
    endif()
endforeach()
