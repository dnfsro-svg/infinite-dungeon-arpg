add_library(arpg_abyss STATIC
    abyss_rules.cpp
    abyss_rewards.cpp)

target_include_directories(arpg_abyss
    PUBLIC "${PROJECT_SOURCE_DIR}/src")
target_compile_features(arpg_abyss PUBLIC cxx_std_17)
target_link_libraries(arpg_abyss PRIVATE arpg_core)

arpg_enable_project_warnings(arpg_abyss)
