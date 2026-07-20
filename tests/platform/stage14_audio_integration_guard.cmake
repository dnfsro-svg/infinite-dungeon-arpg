get_filename_component(_stage14_guard_directory
    "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
get_filename_component(_stage14_repository_root
    "${_stage14_guard_directory}/../.." ABSOLUTE)

if(DEFINED SOURCE_ROOT)
    set(_stage14_repository_root "${SOURCE_ROOT}")
endif()

set(_stage14_app_cmake
    "${_stage14_repository_root}/src/app/CMakeLists.txt")
set(_stage14_combat_audio
    "${_stage14_repository_root}/src/platform/raylib/combat_audio.cpp")
foreach(_stage14_required_path IN ITEMS
        "${_stage14_app_cmake}"
        "${_stage14_combat_audio}")
    if(NOT EXISTS "${_stage14_required_path}")
        message(FATAL_ERROR
            "Stage14 audio integration guard input missing: ${_stage14_required_path}")
    endif()
endforeach()

file(GLOB_RECURSE _stage14_production_sources LIST_DIRECTORIES false
    "${_stage14_repository_root}/src/*.c"
    "${_stage14_repository_root}/src/*.cc"
    "${_stage14_repository_root}/src/*.cpp"
    "${_stage14_repository_root}/src/*.cxx"
    "${_stage14_repository_root}/src/*.h"
    "${_stage14_repository_root}/src/*.hpp")
file(GLOB_RECURSE _stage14_production_cmake LIST_DIRECTORIES false
    "${_stage14_repository_root}/src/CMakeLists.txt"
    "${_stage14_repository_root}/src/*/CMakeLists.txt")
set(_stage14_production_files
    ${_stage14_production_sources} ${_stage14_production_cmake})

foreach(_stage14_source IN LISTS _stage14_production_files)
    file(READ "${_stage14_source}" _stage14_source_text)
    string(TOLOWER "${_stage14_source_text}" _stage14_source_lower)
    if(_stage14_source_lower MATCHES "(https?|ftp)://")
        message(FATAL_ERROR
            "Stage14 production source must not contain a download URL: ${_stage14_source}")
    endif()
    if(_stage14_source_lower MATCHES "ffmpeg")
        message(FATAL_ERROR
            "Stage14 production source must not invoke or reference ffmpeg: ${_stage14_source}")
    endif()
    if(_stage14_source_text MATCHES
            "(std::)?(system|popen|_popen|CreateProcess[A-Za-z]*|ShellExecute[A-Za-z]*|WinExec)[ \\t\\r\\n]*\\(")
        message(FATAL_ERROR
            "Stage14 production source must not launch a process: ${_stage14_source}")
    endif()
endforeach()

file(GLOB_RECURSE _stage14_core_boundary_sources LIST_DIRECTORIES false
    "${_stage14_repository_root}/src/core/*.c"
    "${_stage14_repository_root}/src/core/*.cc"
    "${_stage14_repository_root}/src/core/*.cpp"
    "${_stage14_repository_root}/src/core/*.cxx"
    "${_stage14_repository_root}/src/core/*.h"
    "${_stage14_repository_root}/src/core/*.hpp"
    "${_stage14_repository_root}/src/combat/*.c"
    "${_stage14_repository_root}/src/combat/*.cc"
    "${_stage14_repository_root}/src/combat/*.cpp"
    "${_stage14_repository_root}/src/combat/*.cxx"
    "${_stage14_repository_root}/src/combat/*.h"
    "${_stage14_repository_root}/src/combat/*.hpp"
    "${_stage14_repository_root}/src/dungeon/*.c"
    "${_stage14_repository_root}/src/dungeon/*.cc"
    "${_stage14_repository_root}/src/dungeon/*.cpp"
    "${_stage14_repository_root}/src/dungeon/*.cxx"
    "${_stage14_repository_root}/src/dungeon/*.h"
    "${_stage14_repository_root}/src/dungeon/*.hpp")

foreach(_stage14_source IN LISTS _stage14_core_boundary_sources)
    file(READ "${_stage14_source}" _stage14_source_text)
    if(_stage14_source_text MATCHES "assets/stage14"
            OR _stage14_source_text MATCHES "AudioAssetId"
            OR _stage14_source_text MATCHES
                "(Load|Unload|Is|Play|Stop|Pause|Resume|Update|Set)(Wave|Sound|Music|AudioDevice)[ \\t\\r\\n]*\\(")
        message(FATAL_ERROR
            "Stage14 core boundary violation in ${_stage14_source}")
    endif()
endforeach()

function(stage14_extract_function_body SOURCE_TEXT SIGNATURE OUT_BODY)
    string(FIND "${SOURCE_TEXT}" "${SIGNATURE}" _stage14_signature_index)
    if(_stage14_signature_index EQUAL -1)
        message(FATAL_ERROR
            "Stage14 audio integration guard cannot find ${SIGNATURE}")
    endif()

    string(SUBSTRING "${SOURCE_TEXT}" ${_stage14_signature_index} -1
        _stage14_function_tail)
    string(FIND "${_stage14_function_tail}" "{" _stage14_opening_index)
    if(_stage14_opening_index EQUAL -1)
        message(FATAL_ERROR
            "Stage14 audio integration guard cannot isolate ${SIGNATURE}")
    endif()

    string(SUBSTRING "${_stage14_function_tail}" ${_stage14_opening_index} -1
        _stage14_body_and_rest)
    string(LENGTH "${_stage14_body_and_rest}" _stage14_body_length)
    set(_stage14_depth 0)
    set(_stage14_cursor 0)
    while(_stage14_cursor LESS _stage14_body_length)
        string(SUBSTRING "${_stage14_body_and_rest}" ${_stage14_cursor} 1
            _stage14_character)
        if(_stage14_character STREQUAL "{")
            math(EXPR _stage14_depth "${_stage14_depth} + 1")
        elseif(_stage14_character STREQUAL "}")
            math(EXPR _stage14_depth "${_stage14_depth} - 1")
            if(_stage14_depth EQUAL 0)
                math(EXPR _stage14_function_length "${_stage14_cursor} + 1")
                string(SUBSTRING "${_stage14_body_and_rest}" 0
                    ${_stage14_function_length} _stage14_function_body)
                set("${OUT_BODY}" "${_stage14_function_body}" PARENT_SCOPE)
                return()
            endif()
        endif()
        math(EXPR _stage14_cursor "${_stage14_cursor} + 1")
    endwhile()

    message(FATAL_ERROR
        "Stage14 audio integration guard cannot close ${SIGNATURE}")
endfunction()

file(READ "${_stage14_combat_audio}" _stage14_combat_audio_text)
stage14_extract_function_body("${_stage14_combat_audio_text}"
    "void CombatAudio::consume_event(" _stage14_consume_event_body)
if(_stage14_consume_event_body MATCHES
        "Load(Wave|Sound)[ \\t\\r\\n]*\\(")
    message(FATAL_ERROR
        "Stage14 CombatAudio::consume_event hot path must not load audio")
endif()

file(READ "${_stage14_app_cmake}" _stage14_app_cmake_text)
string(FIND "${_stage14_app_cmake_text}" "assets/stage14/audio"
    _stage14_copy_source_index)
string(FIND "${_stage14_app_cmake_text}"
    "$<TARGET_FILE_DIR:arpg_game>/assets/stage14/audio"
    _stage14_copy_destination_index)
if(_stage14_copy_source_index EQUAL -1
        OR _stage14_copy_destination_index EQUAL -1
        OR _stage14_copy_source_index GREATER _stage14_copy_destination_index)
    message(FATAL_ERROR
        "Stage14 audio integration guard missing Stage14 copy rule in src/app/CMakeLists.txt")
endif()

string(SUBSTRING "${_stage14_app_cmake_text}" 0
    ${_stage14_copy_source_index} _stage14_copy_prefix)
string(REPLACE "add_custom_command(TARGET arpg_game POST_BUILD" ";"
    _stage14_post_build_sections "${_stage14_copy_prefix}")
list(LENGTH _stage14_post_build_sections _stage14_post_build_count)
if(_stage14_post_build_count LESS 2)
    message(FATAL_ERROR
        "Stage14 audio integration guard missing Stage14 POST_BUILD copy command")
endif()
math(EXPR _stage14_last_post_build_index "${_stage14_post_build_count} - 1")
list(GET _stage14_post_build_sections ${_stage14_last_post_build_index}
    _stage14_copy_command)
if(NOT _stage14_copy_command MATCHES "copy_directory")
    message(FATAL_ERROR
        "Stage14 audio integration guard missing Stage14 copy rule in src/app/CMakeLists.txt")
endif()

message(STATUS "[stage14-audio-integration] source and packaging boundaries passed")
