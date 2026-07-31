add_library(arpg_checkpoint STATIC
    room_checkpoint_validation.cpp)

target_include_directories(arpg_checkpoint
    PUBLIC "${PROJECT_SOURCE_DIR}/src")
target_compile_features(arpg_checkpoint PUBLIC cxx_std_17)
target_link_libraries(arpg_checkpoint
    PUBLIC
        arpg_core
        arpg_abyss
        arpg_skills
        arpg_modifiers
        arpg_items
        arpg_progression
        arpg_passives)

arpg_enable_project_warnings(arpg_checkpoint)
