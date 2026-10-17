add_library(arpg_passives STATIC
    passive_tree_catalog.cpp
    passive_tree_rules.cpp)

target_include_directories(arpg_passives PUBLIC "${PROJECT_SOURCE_DIR}/src")
target_compile_features(arpg_passives PUBLIC cxx_std_17)
target_link_libraries(arpg_passives PUBLIC arpg_modifiers arpg_progression)
arpg_enable_project_warnings(arpg_passives)
