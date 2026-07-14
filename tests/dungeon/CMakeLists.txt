add_executable(arpg_dungeon_tests
    ../core/allocation_probe.cpp
    dungeon_test_main.cpp
    dungeon_lifecycle_tests.cpp
    dungeon_navigation_tests.cpp
    dungeon_stress_tests.cpp
    room_generation_tests.cpp
    dungeon_rules_tests.cpp
    dungeon_progression_tests.cpp
    dungeon_transaction_tests.cpp
    encounter_director_tests.cpp
    dungeon_wave_tests.cpp)
target_sources(arpg_dungeon_tests PRIVATE dungeon_progression_reward_tests.cpp)

target_compile_features(arpg_dungeon_tests PRIVATE cxx_std_17)
target_include_directories(arpg_dungeon_tests
    PRIVATE "${PROJECT_SOURCE_DIR}/tests/core")
target_link_libraries(arpg_dungeon_tests PRIVATE arpg_dungeon)
arpg_enable_project_warnings(arpg_dungeon_tests)

add_test(NAME dungeon.units COMMAND arpg_dungeon_tests)
set_tests_properties(dungeon.units PROPERTIES LABELS "headless;dungeon")
