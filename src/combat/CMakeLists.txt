add_library(arpg_combat STATIC
    attack_catalog.cpp
    combat_collision.cpp
    combat_world.cpp
    hit_resolution.cpp
    input_buffer.cpp
    monster_ai.cpp
    monster_catalog.cpp
    monster_pool.cpp
    player_simulation.cpp
    target_simulation.cpp)

target_include_directories(arpg_combat
    PUBLIC "${PROJECT_SOURCE_DIR}/src")
target_compile_features(arpg_combat PUBLIC cxx_std_17)
target_link_libraries(arpg_combat PRIVATE arpg_core arpg_modifiers)

arpg_enable_project_warnings(arpg_combat)
