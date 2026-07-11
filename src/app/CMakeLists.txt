add_executable(arpg_game main.cpp)

target_compile_features(arpg_game PRIVATE cxx_std_17)
target_link_libraries(arpg_game PRIVATE arpg_raylib)

arpg_enable_project_warnings(arpg_game)
