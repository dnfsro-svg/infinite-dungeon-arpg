add_library(arpg_combat STATIC
    active_skill_runtime.cpp
    active_skill_timeline.cpp
    abyss_environment.cpp
    attack_catalog.cpp
    combat_collision.cpp
    combat_effects.cpp
    combat_snapshot.cpp
    combat_world.cpp
    hit_resolution.cpp
    input_buffer.cpp
    monster_ai.cpp
    monster_ai_melee.cpp
    monster_ai_ranged.cpp
    monster_ai_special.cpp
    monster_affix_catalog.cpp
    monster_affix_generation.cpp
    monster_affix_runtime.cpp
    monster_catalog.cpp
    monster_pool.cpp
    player_damage_history.cpp
    player_simulation.cpp
    room_spatial_grid.cpp
    target_simulation.cpp)

target_include_directories(arpg_combat
    PUBLIC "${PROJECT_SOURCE_DIR}/src")
target_compile_features(arpg_combat PUBLIC cxx_std_17)
target_link_libraries(arpg_combat
    PUBLIC arpg_abyss
    PRIVATE arpg_core arpg_modifiers)

arpg_enable_project_warnings(arpg_combat)
