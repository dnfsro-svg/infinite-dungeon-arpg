add_library(arpg_items STATIC
    item_catalog.cpp
    item_generation.cpp
    item_modifiers.cpp)

target_include_directories(arpg_items PUBLIC "${PROJECT_SOURCE_DIR}/src")
target_compile_features(arpg_items PUBLIC cxx_std_17)
target_link_libraries(arpg_items PUBLIC arpg_core arpg_modifiers)
arpg_enable_project_warnings(arpg_items)
