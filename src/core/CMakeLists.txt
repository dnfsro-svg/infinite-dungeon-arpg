add_library(arpg_core STATIC
    fixed_step.cpp
    deterministic_rng.cpp)

target_include_directories(arpg_core
    PUBLIC "${PROJECT_SOURCE_DIR}/src")
target_compile_features(arpg_core PUBLIC cxx_std_17)

arpg_enable_project_warnings(arpg_core)
