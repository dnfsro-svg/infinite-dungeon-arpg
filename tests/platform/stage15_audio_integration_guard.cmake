get_filename_component(_guard_directory "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
get_filename_component(_repository_root "${_guard_directory}/../.." ABSOLUTE)
if(DEFINED SOURCE_ROOT)
    set(_repository_root "${SOURCE_ROOT}")
endif()

set(_game_audio "${_repository_root}/src/platform/raylib/game_audio.cpp")
set(_game_audio_header "${_repository_root}/src/platform/raylib/game_audio.hpp")
set(_host "${_repository_root}/src/platform/raylib/raylib_host.cpp")
set(_raylib_cmake "${_repository_root}/src/platform/raylib/CMakeLists.txt")
set(_app_cmake "${_repository_root}/src/app/CMakeLists.txt")
foreach(_required IN ITEMS
        "${_game_audio}" "${_game_audio_header}" "${_host}" "${_raylib_cmake}"
        "${_app_cmake}")
    if(NOT EXISTS "${_required}")
        message(FATAL_ERROR "Stage15 integration input missing: ${_required}")
    endif()
endforeach()

file(GLOB_RECURSE _production_files LIST_DIRECTORIES false
    "${_repository_root}/src/*.c" "${_repository_root}/src/*.cc"
    "${_repository_root}/src/*.cpp" "${_repository_root}/src/*.cxx"
    "${_repository_root}/src/*.h" "${_repository_root}/src/*.hpp")
include("${CMAKE_CURRENT_LIST_DIR}/audio_process_guard_common.cmake")
foreach(_source IN LISTS _production_files)
    file(READ "${_source}" _text)
    string(TOLOWER "${_text}" _lower)
    if(_lower MATCHES "(https?|ftp)://")
        message(FATAL_ERROR "Stage15 production source contains a download URL: ${_source}")
    endif()
    arpg_audio_process_guard_process_call_detected(
        "${_repository_root}" "${_source}" "${_text}"
        _process_call_detected)
    if(_process_call_detected)
        message(FATAL_ERROR "Stage15 production source launches a process: ${_source}")
    endif()
endforeach()

file(GLOB_RECURSE _core_files LIST_DIRECTORIES false
    "${_repository_root}/src/core/*" "${_repository_root}/src/combat/*"
    "${_repository_root}/src/dungeon/*")
foreach(_source IN LISTS _core_files)
    if(IS_DIRECTORY "${_source}")
        continue()
    endif()
    file(READ "${_source}" _text)
    if(_text MATCHES "assets/stage15|GameAudio|StreamPack|UiAudioPack|AudioSceneInput")
        message(FATAL_ERROR "Stage15 core boundary violation: ${_source}")
    endif()
endforeach()

file(READ "${_game_audio_header}" _header_text)
foreach(_token IN ITEMS "AudioPack sfx_" "StreamPack streams_"
        "UiAudioPack ui_" "AudioSceneState scene_" "UiAudioCueGate ui_gate_")
    string(FIND "${_header_text}" "${_token}" _found)
    if(_found EQUAL -1)
        message(FATAL_ERROR "Stage15 GameAudio ownership missing: ${_token}")
    endif()
endforeach()

file(READ "${_game_audio}" _game_audio_text)
foreach(_token IN ITEMS "InitAudioDevice" "CloseAudioDevice"
        "streams_.update()" "sfx_.set_volume" "ui_.set_volume")
    string(FIND "${_game_audio_text}" "${_token}" _found)
    if(_found EQUAL -1)
        message(FATAL_ERROR "Stage15 GameAudio production wiring missing: ${_token}")
    endif()
endforeach()

file(READ "${_host}" _host_text)
foreach(_token IN ITEMS "#include \"game_audio.hpp\""
        "const auto audio_storage = std::make_unique<GameAudio>();"
        "GameAudio& audio = *audio_storage;"
        "audio.update({combat_audio_active" "audio_bus_levels(presented_audio_settings)")
    string(FIND "${_host_text}" "${_token}" _found)
    if(_found EQUAL -1)
        message(FATAL_ERROR "Stage15 host wiring missing: ${_token}")
    endif()
endforeach()
string(FIND "${_host_text}" "CombatAudio audio;" _legacy_audio)
if(NOT _legacy_audio EQUAL -1)
    message(FATAL_ERROR "Stage15 host still instantiates legacy CombatAudio")
endif()

file(READ "${_raylib_cmake}" _cmake_text)
string(FIND "${_cmake_text}" "game_audio.cpp" _cmake_found)
if(_cmake_found EQUAL -1)
    message(FATAL_ERROR "Stage15 arpg_raylib target omits game_audio.cpp")
endif()

file(READ "${_app_cmake}" _app_cmake_text)
foreach(_token IN ITEMS
        "assets/stage15/audio"
        "$<TARGET_FILE_DIR:arpg_game>/assets/stage15/audio")
    string(FIND "${_app_cmake_text}" "${_token}" _copy_found)
    if(_copy_found EQUAL -1)
        message(FATAL_ERROR "Stage15 release copy rule missing: ${_token}")
    endif()
endforeach()

message(STATUS "[stage15-audio-integration] ownership, boundary, and host wiring passed")
