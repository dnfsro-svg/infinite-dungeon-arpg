add_library(arpg_modifiers STATIC
    modifier_math.cpp
    effect_set.cpp
    player_modifier_values.cpp)

target_include_directories(arpg_modifiers PUBLIC "${PROJECT_SOURCE_DIR}/src")
target_compile_features(arpg_modifiers PUBLIC cxx_std_17)
arpg_enable_project_warnings(arpg_modifiers)
