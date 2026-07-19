add_executable(arpg_game main.cpp)

target_compile_features(arpg_game PRIVATE cxx_std_17)
target_link_libraries(arpg_game PRIVATE arpg_raylib)

arpg_enable_project_warnings(arpg_game)

add_custom_command(TARGET arpg_game POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E make_directory
        "$<TARGET_FILE_DIR:arpg_game>/assets/stage12"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
        "${PROJECT_SOURCE_DIR}/assets/stage12"
        "$<TARGET_FILE_DIR:arpg_game>/assets/stage12"
    COMMENT "Copying Stage 12 material-pack assets"
    VERBATIM)
