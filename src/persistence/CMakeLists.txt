add_library(arpg_persistence STATIC
    crc32.cpp
    checkpoint_codec.cpp
    save_paths.cpp
    save_store.cpp)

target_include_directories(arpg_persistence PUBLIC "${PROJECT_SOURCE_DIR}/src")
target_compile_features(arpg_persistence PUBLIC cxx_std_17)
arpg_enable_project_warnings(arpg_persistence)
target_link_libraries(arpg_persistence PUBLIC arpg_progression)
