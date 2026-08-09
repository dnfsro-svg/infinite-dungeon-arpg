add_executable(arpg_game main.cpp)

if(WIN32)
    target_sources(arpg_game PRIVATE arpg_game.manifest)
endif()

target_compile_features(arpg_game PRIVATE cxx_std_17)
target_link_libraries(arpg_game PRIVATE arpg_raylib)

arpg_enable_project_warnings(arpg_game)

include("${PROJECT_SOURCE_DIR}/cmake/ArpgRuntimeAssets.cmake")
arpg_add_runtime_asset_sync(
    TARGET arpg_game
    SOURCE_ROOT "${PROJECT_SOURCE_DIR}/assets"
    DESTINATION_ROOT "$<TARGET_FILE_DIR:arpg_game>/assets"
    DIRECTORIES
        fonts
        player
        skills
        stage12
        stage14/audio
        stage15/audio)
