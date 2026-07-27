add_executable(arpg_combat_tests
    ../core/allocation_probe.cpp
    combat_test_main.cpp
    active_skill_timeline_tests.cpp
    abyss_environment_tests.cpp
    attack_catalog_tests.cpp
    attack_state_tests.cpp
    break_stress_tests.cpp
    combat_config_tests.cpp
    combat_query_tests.cpp
    draw_slash_skill_tests.cpp
    dummy_reaction_tests.cpp
    hit_resolution_tests.cpp
    input_buffer_tests.cpp
    monster_affix_catalog_tests.cpp
    monster_affix_generation_tests.cpp
    monster_affix_runtime_tests.cpp
    monster_affix_trigger_tests.cpp
    monster_catalog_tests.cpp
    monster_melee_tests.cpp
    monster_ranged_tests.cpp
    monster_special_tests.cpp
    monster_pool_tests.cpp
    movement_jump_tests.cpp
    player_health_tests.cpp
    player_build_tests.cpp
    player_damage_history_tests.cpp
    player_death_snapshot_tests.cpp
    player_defense_tests.cpp
    storm_swords_skill_tests.cpp)

target_compile_features(arpg_combat_tests PRIVATE cxx_std_17)
target_include_directories(arpg_combat_tests
    PRIVATE "${PROJECT_SOURCE_DIR}/tests/core")
target_link_libraries(arpg_combat_tests PRIVATE arpg_combat)
arpg_enable_project_warnings(arpg_combat_tests)
if(MSVC)
    # Capacity tests intentionally keep several complete combat worlds and
    # snapshots alive in one debug stack frame.
    target_link_options(arpg_combat_tests PRIVATE /STACK:4194304)
endif()

add_test(NAME combat.units COMMAND arpg_combat_tests)
set_tests_properties(combat.units PROPERTIES LABELS "headless;combat")
