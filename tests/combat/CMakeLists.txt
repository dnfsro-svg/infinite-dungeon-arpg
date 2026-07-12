add_executable(arpg_combat_tests
    ../core/allocation_probe.cpp
    combat_test_main.cpp
    attack_catalog_tests.cpp
    attack_state_tests.cpp
    break_stress_tests.cpp
    combat_config_tests.cpp
    dummy_reaction_tests.cpp
    hit_resolution_tests.cpp
    input_buffer_tests.cpp
    monster_catalog_tests.cpp
    monster_pool_tests.cpp
    movement_jump_tests.cpp)

target_compile_features(arpg_combat_tests PRIVATE cxx_std_17)
target_include_directories(arpg_combat_tests
    PRIVATE "${PROJECT_SOURCE_DIR}/tests/core")
target_link_libraries(arpg_combat_tests PRIVATE arpg_combat)
arpg_enable_project_warnings(arpg_combat_tests)

add_test(NAME combat.units COMMAND arpg_combat_tests)
set_tests_properties(combat.units PROPERTIES LABELS "headless;combat")
