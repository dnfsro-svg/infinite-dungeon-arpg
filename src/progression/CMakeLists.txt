add_library(arpg_progression STATIC progression_rules.cpp)

target_include_directories(arpg_progression PUBLIC "${PROJECT_SOURCE_DIR}/src")
target_compile_features(arpg_progression PUBLIC cxx_std_17)
arpg_enable_project_warnings(arpg_progression)
