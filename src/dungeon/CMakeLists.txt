add_library(arpg_dungeon STATIC
    dungeon_rules.cpp
    encounter_director.cpp
    encounter_budget.cpp
    dungeon_session.cpp
    dungeon_transition.cpp
    dungeon_snapshot.cpp
    dungeon_progression.cpp
    room_generation.cpp
    room_combat_template.cpp
    room_navigation.cpp)

target_include_directories(arpg_dungeon PUBLIC "${PROJECT_SOURCE_DIR}/src")
target_compile_features(arpg_dungeon PUBLIC cxx_std_17)
target_link_libraries(arpg_dungeon PUBLIC arpg_combat arpg_progression PRIVATE arpg_core)
arpg_enable_project_warnings(arpg_dungeon)
