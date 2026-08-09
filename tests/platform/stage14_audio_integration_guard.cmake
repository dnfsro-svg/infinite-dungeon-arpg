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
include("${CMAKE_CURRENT_LIST_DIR}/audio_process_guard_common.cmake")
set(_stage14_process_function_family
    "(system|_wsystem|popen|_popen|_wpopen|createprocess[a-z]*|shellexecute[a-z]*|winexec|posix_spawn[a-z]*|_spawn(l|le|lp|lpe|v|ve|vp|vpe)?|_wspawn(l|le|lp|lpe|v|ve|vp|vpe)?|exec(l|le|lp|lpe|v|ve|vp|vpe)?|_exec(l|le|lp|lpe|v|ve|vp|vpe)?)")
set(_stage14_bare_process_call_pattern
    "(^|[^A-Za-z0-9_:>.])${_stage14_process_function_family}[ \\t\\r\\n]*\\(")
set(_stage14_std_process_call_pattern
    "(^|[^A-Za-z0-9_:>.])std::system[ \\t\\r\\n]*\\(")
set(_stage14_global_process_call_pattern
    "(^|[^A-Za-z0-9_:>.])::${_stage14_process_function_family}[ \\t\\r\\n]*\\(")

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
    arpg_audio_process_guard_mask_launcher_calls(
        "${_stage14_repository_root}" "${_stage14_source}"
        "${_stage14_source_text}" _stage14_process_text)
    string(TOLOWER "${_stage14_process_text}" _stage14_process_lower)
    if(_stage14_process_lower MATCHES "${_stage14_bare_process_call_pattern}"
            OR _stage14_process_lower MATCHES "${_stage14_std_process_call_pattern}"
            OR _stage14_process_lower MATCHES "${_stage14_global_process_call_pattern}")
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
    string(FIND "${_stage14_source_text}" [=[assets\stage14]=]
        _stage14_single_backslash_path_index)
    string(FIND "${_stage14_source_text}" [=[assets\\stage14]=]
        _stage14_double_backslash_path_index)
    if(_stage14_source_text MATCHES "assets/stage14"
            OR NOT _stage14_single_backslash_path_index EQUAL -1
            OR NOT _stage14_double_backslash_path_index EQUAL -1
            OR _stage14_source_text MATCHES "AudioAssetId")
        message(FATAL_ERROR
            "Stage14 core boundary violation in ${_stage14_source}")
    endif()

    foreach(_stage14_audio_api IN ITEMS
            InitAudioDevice CloseAudioDevice IsAudioDeviceReady
            SetMasterVolume GetMasterVolume
            LoadWave LoadWaveFromMemory IsWaveValid
            LoadSound LoadSoundFromWave LoadSoundAlias IsSoundValid
            UpdateSound UnloadWave UnloadSound UnloadSoundAlias
            ExportWave ExportWaveAsCode WaveCopy WaveCrop WaveFormat
            LoadWaveSamples UnloadWaveSamples
            PlaySound StopSound PauseSound ResumeSound IsSoundPlaying
            SetSoundVolume SetSoundPitch SetSoundPan
            LoadMusicStream LoadMusicStreamFromMemory IsMusicValid
            UnloadMusicStream PlayMusicStream StopMusicStream PauseMusicStream
            ResumeMusicStream IsMusicStreamPlaying UpdateMusicStream
            SeekMusicStream SetMusicVolume SetMusicPitch SetMusicPan
            GetMusicTimeLength GetMusicTimePlayed
            LoadAudioStream IsAudioStreamValid UnloadAudioStream
            PlayAudioStream StopAudioStream PauseAudioStream ResumeAudioStream
            IsAudioStreamPlaying UpdateAudioStream IsAudioStreamProcessed
            SetAudioStreamVolume SetAudioStreamPitch SetAudioStreamPan
            SetAudioStreamBufferSizeDefault SetAudioStreamCallback
            AttachAudioStreamProcessor DetachAudioStreamProcessor
            AttachAudioMixedProcessor DetachAudioMixedProcessor)
        if(_stage14_source_text MATCHES
                "(^|[^A-Za-z0-9_])${_stage14_audio_api}[ \\t\\r\\n]*\\(")
            message(FATAL_ERROR
                "Stage14 core boundary rejects raylib audio API ${_stage14_audio_api} in ${_stage14_source}")
        endif()
    endforeach()
endforeach()

file(READ "${_stage14_combat_audio}" _stage14_combat_audio_text)
if(_stage14_combat_audio_text MATCHES
        "(^|[^A-Za-z0-9_])Load(Wave|Sound)[A-Za-z0-9_]*[ \\t\\r\\n]*\\(")
    message(FATAL_ERROR
        "Stage14 combat_audio.cpp must not directly load audio on its event path")
endif()

file(READ "${_stage14_app_cmake}" _stage14_app_cmake_text)
string(REGEX REPLACE "#[^\\r\\n]*" "" _stage14_app_cmake_without_comments
    "${_stage14_app_cmake_text}")
foreach(_stage14_sync_token IN ITEMS
        "ArpgRuntimeAssets.cmake"
        "arpg_add_runtime_asset_sync("
        "TARGET arpg_game"
        [=[${PROJECT_SOURCE_DIR}/assets]=]
        [=[$<TARGET_FILE_DIR:arpg_game>/assets]=]
        "stage14/audio")
    string(FIND "${_stage14_app_cmake_without_comments}"
        "${_stage14_sync_token}" _stage14_sync_token_index)
    if(_stage14_sync_token_index EQUAL -1)
        message(FATAL_ERROR
            "Stage14 runtime asset sync rule missing: ${_stage14_sync_token}")
    endif()
endforeach()

message(STATUS "[stage14-audio-integration] source and packaging boundaries passed")
