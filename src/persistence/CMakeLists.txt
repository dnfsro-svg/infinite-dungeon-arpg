add_library(arpg_persistence STATIC
    crc32.cpp
    checkpoint_codec.cpp)

target_include_directories(arpg_persistence PUBLIC "${PROJECT_SOURCE_DIR}/src")
target_compile_features(arpg_persistence PUBLIC cxx_std_17)
arpg_enable_project_warnings(arpg_persistence)
