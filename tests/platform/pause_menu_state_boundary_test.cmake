cmake_minimum_required(VERSION 3.25)

if(NOT DEFINED PAUSE_STATE_HEADER OR NOT DEFINED PAUSE_STATE_SOURCE)
    message(FATAL_ERROR "Pause state source paths are required")
endif()

function(arpg_pause_includes_are_allowed SOURCE_TEXT OUT_ALLOWED OUT_BAD_INCLUDE)
    string(REPLACE "\r\n" "\n" _arpg_source "${SOURCE_TEXT}")
    string(REPLACE "\r" "\n" _arpg_source "${_arpg_source}")
    string(REPLACE "\n" ";" _arpg_lines "${_arpg_source}")
    foreach(_arpg_line IN LISTS _arpg_lines)
        if(_arpg_line MATCHES "^[ \t]*#[ \t]*include[ \t]+[<\"][^>\"]+[>\"]")
            if(NOT _arpg_line MATCHES
                    "^[ \t]*#[ \t]*include[ \t]+(\"pause_menu_state\\.hpp\"|\"platform/settings/settings_types\\.hpp\"|<cstddef>|<cstdint>|<optional>|<limits>)[ \t]*$")
                set("${OUT_ALLOWED}" FALSE PARENT_SCOPE)
                set("${OUT_BAD_INCLUDE}" "${_arpg_line}" PARENT_SCOPE)
                return()
            endif()
        endif()
    endforeach()
    set("${OUT_ALLOWED}" TRUE PARENT_SCOPE)
    set("${OUT_BAD_INCLUDE}" "" PARENT_SCOPE)
endfunction()

function(arpg_pause_source_has_forbidden_api SOURCE_TEXT OUT_FOUND OUT_LABEL)
    set(_arpg_api_checks
        "raylib device API|(^|[^A-Za-z0-9_])(IsKeyPressed|IsKeyPressedRepeat|IsKeyDown|IsKeyReleased|IsKeyUp|GetKeyPressed|GetCharPressed|IsMouseButtonPressed|IsMouseButtonDown|IsMouseButtonReleased|IsMouseButtonUp|GetMouseX|GetMouseY|GetMousePosition|GetMouseDelta|GetMouseWheelMove|GetMouseWheelMoveV)[ \t\r\n]*\\("
        "raylib window API|(^|[^A-Za-z0-9_])(IsWindowFocused|IsWindowState|SetWindowState|ClearWindowState|ToggleFullscreen|ToggleBorderlessWindowed|SetWindowSize|SetWindowPosition|SetWindowMinSize|SetWindowMaxSize|SetWindowTitle|SetWindowMonitor|SetTargetFPS|SetMasterVolume|CloseWindow|WindowShouldClose)[ \t\r\n]*\\("
        "SettingsStore type|(^|[^A-Za-z0-9_])SettingsStore([^A-Za-z0-9_]|$)"
        "settings_store token|(^|[^A-Za-z0-9_])settings_store([^A-Za-z0-9_]|$)"
        "combat namespace|(^|[^A-Za-z0-9_])combat[ \t\r\n]*::"
        "dungeon namespace|(^|[^A-Za-z0-9_])dungeon[ \t\r\n]*::"
        "filesystem namespace|(^|[^A-Za-z0-9_])(std[ \t\r\n]*::[ \t\r\n]*)?filesystem[ \t\r\n]*::"
        "C++ stream type|(^|[^A-Za-z0-9_])(std[ \t\r\n]*::[ \t\r\n]*)?(fstream|ifstream|ofstream)([^A-Za-z0-9_]|$)"
        "C FILE type|(^|[^A-Za-z0-9_])FILE([^A-Za-z0-9_]|$)"
        "C file API|(^|[^A-Za-z0-9_])(fopen|fread|fwrite|fclose)[ \t\r\n]*\\("
        "Windows file API|(^|[^A-Za-z0-9_])(CreateFile|ReadFile|WriteFile|MoveFile|MoveFileEx|ReplaceFile|DeleteFile|FlushFileBuffers)(A|W)?[ \t\r\n]*\\(")
    foreach(_arpg_check IN LISTS _arpg_api_checks)
        string(FIND "${_arpg_check}" "|" _arpg_separator)
        string(SUBSTRING "${_arpg_check}" 0 ${_arpg_separator} _arpg_label)
        math(EXPR _arpg_pattern_start "${_arpg_separator} + 1")
        string(SUBSTRING "${_arpg_check}" ${_arpg_pattern_start} -1 _arpg_pattern)
        if(SOURCE_TEXT MATCHES "${_arpg_pattern}")
            set("${OUT_FOUND}" TRUE PARENT_SCOPE)
            set("${OUT_LABEL}" "${_arpg_label}" PARENT_SCOPE)
            return()
        endif()
    endforeach()
    set("${OUT_FOUND}" FALSE PARENT_SCOPE)
    set("${OUT_LABEL}" "" PARENT_SCOPE)
endfunction()

foreach(_arpg_allowed_include IN ITEMS
        "#include \"pause_menu_state.hpp\""
        "  # include \"platform/settings/settings_types.hpp\"  "
        "#include <cstddef>"
        "#include <cstdint>"
        "#include <optional>"
        "#include <limits>"
        "// #include <raylib.h>")
    arpg_pause_includes_are_allowed(
        "${_arpg_allowed_include}" _arpg_allowed _arpg_bad_include)
    if(NOT _arpg_allowed)
        message(FATAL_ERROR
            "Pause state include guard rejected clean control: ${_arpg_allowed_include}")
    endif()
endforeach()

foreach(_arpg_forbidden_include IN ITEMS
        "#include <raylib.h>"
        "#include \"combat/combat.hpp\""
        "#include \"dungeon/dungeon.hpp\""
        "#include <filesystem>"
        "#include <fstream>"
        "#include <cstdio>"
        "#include <windows.h>")
    arpg_pause_includes_are_allowed(
        "${_arpg_forbidden_include}" _arpg_allowed _arpg_bad_include)
    if(_arpg_allowed)
        message(FATAL_ERROR
            "Pause state include guard missed mutation: ${_arpg_forbidden_include}")
    endif()
endforeach()

foreach(_arpg_mutation IN ITEMS
        "IsKeyPressed(1);"
        "SetWindowState(1);"
        "SettingsStore* store;"
        "auto settings_store = 0;"
        "combat::CombatState combat_state;"
        "dungeon::DungeonSession session;"
        "filesystem::path save_path;"
        "std::ifstream input;"
        "FILE* handle;"
        "fopen(path, mode);"
        "CreateFileW(path, 0, 0, 0, 0, 0, 0);"
        "ReadFile(handle, buffer, size, count, 0);")
    arpg_pause_source_has_forbidden_api(
        "${_arpg_mutation}" _arpg_found _arpg_label)
    if(NOT _arpg_found)
        message(FATAL_ERROR
            "Pause state API guard missed mutation: ${_arpg_mutation}")
    endif()
endforeach()

foreach(_arpg_clean_control IN ITEMS
        "// raylib combat dungeon file are architecture words"
        "constexpr char message[] = \"raylib combat dungeon file\";"
        "int raylibish_combatant_dungeoned_file_id = 0;"
        "SettingsStorefront settings_storefront;"
        "combative::State dungeoned::State;"
        "filesystemish::path profile;"
        "int fstreaming_ifstreamed_ofstreamed = 0;"
        "int FILESYSTEM_FILE_ID = 0;"
        "auto fopenly_freader_fwriter_fcloser = 0;"
        "auto CreateFileName_ReadFileName_WriteFileName = 0;"
        "auto IsKeyPressedMessage_SetWindowStateCache = 0;")
    arpg_pause_source_has_forbidden_api(
        "${_arpg_clean_control}" _arpg_found _arpg_label)
    if(_arpg_found)
        message(FATAL_ERROR
            "Pause state API guard rejected clean control '${_arpg_label}': ${_arpg_clean_control}")
    endif()
endforeach()

foreach(_arpg_source IN ITEMS "${PAUSE_STATE_HEADER}" "${PAUSE_STATE_SOURCE}")
    if(NOT EXISTS "${_arpg_source}")
        message(FATAL_ERROR "Pause state source missing: ${_arpg_source}")
    endif()
    file(READ "${_arpg_source}" _arpg_source_text)
    arpg_pause_includes_are_allowed(
        "${_arpg_source_text}" _arpg_allowed _arpg_bad_include)
    if(NOT _arpg_allowed)
        message(FATAL_ERROR
            "Pause state source has forbidden include '${_arpg_bad_include}': ${_arpg_source}")
    endif()
    arpg_pause_source_has_forbidden_api(
        "${_arpg_source_text}" _arpg_found _arpg_label)
    if(_arpg_found)
        message(FATAL_ERROR
            "Pause state source contains forbidden ${_arpg_label}: ${_arpg_source}")
    endif()
endforeach()
