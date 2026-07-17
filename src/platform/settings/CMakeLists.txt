add_library(arpg_settings STATIC
    settings_types.cpp
    settings_codec.cpp
    settings_store.cpp)

target_include_directories(arpg_settings PUBLIC "${PROJECT_SOURCE_DIR}/src")
target_compile_features(arpg_settings PUBLIC cxx_std_17)
arpg_enable_project_warnings(arpg_settings)
target_link_libraries(arpg_settings PRIVATE arpg_core)
