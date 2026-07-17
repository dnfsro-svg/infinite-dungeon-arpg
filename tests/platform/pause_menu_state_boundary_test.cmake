cmake_minimum_required(VERSION 3.25)

if(NOT DEFINED PAUSE_STATE_HEADER OR NOT DEFINED PAUSE_STATE_SOURCE)
    message(FATAL_ERROR "Pause state source paths are required")
endif()

set(_arpg_forbidden_fragments
    "raylib"
    "settings_store"
    "settingsstore"
    "combat"
    "dungeon"
    "<filesystem>"
    "std::filesystem"
    "fopen("
    "ifstream"
    "ofstream")

function(arpg_pause_source_has_forbidden_dependency SOURCE_TEXT OUT_FOUND OUT_FRAGMENT)
    string(TOLOWER "${SOURCE_TEXT}" _arpg_source_lower)
    foreach(_arpg_fragment IN LISTS _arpg_forbidden_fragments)
        string(FIND "${_arpg_source_lower}" "${_arpg_fragment}" _arpg_position)
        if(NOT _arpg_position EQUAL -1)
            set("${OUT_FOUND}" TRUE PARENT_SCOPE)
            set("${OUT_FRAGMENT}" "${_arpg_fragment}" PARENT_SCOPE)
            return()
        endif()
    endforeach()
    set("${OUT_FOUND}" FALSE PARENT_SCOPE)
    set("${OUT_FRAGMENT}" "" PARENT_SCOPE)
endfunction()

foreach(_arpg_mutation IN ITEMS
        "#include <raylib.h>"
        "SettingsStore* store;"
        "Combat* combat;"
        "Dungeon* dungeon;"
        "#include <filesystem>"
        "std::ifstream input;")
    arpg_pause_source_has_forbidden_dependency(
        "${_arpg_mutation}" _arpg_found _arpg_fragment)
    if(NOT _arpg_found)
        message(FATAL_ERROR
            "Pause state boundary self-test missed mutation: ${_arpg_mutation}")
    endif()
endforeach()

foreach(_arpg_source IN ITEMS "${PAUSE_STATE_HEADER}" "${PAUSE_STATE_SOURCE}")
    if(NOT EXISTS "${_arpg_source}")
        message(FATAL_ERROR "Pause state source missing: ${_arpg_source}")
    endif()
    file(READ "${_arpg_source}" _arpg_source_text)
    arpg_pause_source_has_forbidden_dependency(
        "${_arpg_source_text}" _arpg_found _arpg_fragment)
    if(_arpg_found)
        message(FATAL_ERROR
            "Pause state source contains forbidden dependency '${_arpg_fragment}': ${_arpg_source}")
    endif()
endforeach()
