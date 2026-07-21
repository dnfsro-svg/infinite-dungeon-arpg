add_library(arpg_skills STATIC
    active_skill_catalog.cpp
    skill_loadout.cpp)

target_include_directories(arpg_skills PUBLIC "${PROJECT_SOURCE_DIR}/src")
target_compile_features(arpg_skills PUBLIC cxx_std_17)
arpg_enable_project_warnings(arpg_skills)
