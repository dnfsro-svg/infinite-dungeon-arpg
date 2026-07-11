add_executable(arpg_dungeon_tests
    dungeon_test_main.cpp
    dungeon_lifecycle_tests.cpp
    room_generation_tests.cpp)

target_compile_features(arpg_dungeon_tests PRIVATE cxx_std_17)
target_include_directories(arpg_dungeon_tests
    PRIVATE "${PROJECT_SOURCE_DIR}/tests/core")
target_link_libraries(arpg_dungeon_tests PRIVATE arpg_dungeon)
arpg_enable_project_warnings(arpg_dungeon_tests)

add_test(NAME dungeon.units COMMAND arpg_dungeon_tests)
set_tests_properties(dungeon.units PROPERTIES LABELS "headless;dungeon")
