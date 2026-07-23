add_executable(arpg_game main.cpp)

if(WIN32)
    target_sources(arpg_game PRIVATE arpg_game.manifest)
endif()

target_compile_features(arpg_game PRIVATE cxx_std_17)
target_link_libraries(arpg_game PRIVATE arpg_raylib)

arpg_enable_project_warnings(arpg_game)

add_custom_command(TARGET arpg_game POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
        "${PROJECT_SOURCE_DIR}/assets/fonts"
        "$<TARGET_FILE_DIR:arpg_game>/assets/fonts"
    COMMENT "Copying bundled Noto Sans SC font and OFL license"
    VERBATIM)

add_custom_command(TARGET arpg_game POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E make_directory
        "$<TARGET_FILE_DIR:arpg_game>/assets/stage12"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
        "${PROJECT_SOURCE_DIR}/assets/stage12"
        "$<TARGET_FILE_DIR:arpg_game>/assets/stage12"
    COMMENT "Copying Stage 12 material-pack assets"
    VERBATIM)

add_custom_command(TARGET arpg_game POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
        "${PROJECT_SOURCE_DIR}/assets/player"
        "$<TARGET_FILE_DIR:arpg_game>/assets/player"
    COMMENT "Copying player base-animation material atlases"
    VERBATIM)

add_custom_command(TARGET arpg_game POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
        "${PROJECT_SOURCE_DIR}/assets/skills"
        "$<TARGET_FILE_DIR:arpg_game>/assets/skills"
    COMMENT "Copying active-skill frame atlases"
    VERBATIM)

add_custom_command(TARGET arpg_game POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
        "${PROJECT_SOURCE_DIR}/assets/stage14/audio"
        "$<TARGET_FILE_DIR:arpg_game>/assets/stage14/audio"
    COMMENT "Copying Stage 14 combat audio assets"
    VERBATIM)

add_custom_command(TARGET arpg_game POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
        "${PROJECT_SOURCE_DIR}/assets/stage15/audio"
        "$<TARGET_FILE_DIR:arpg_game>/assets/stage15/audio"
    COMMENT "Copying Stage 15 music, ambience, and UI audio assets"
    VERBATIM)
