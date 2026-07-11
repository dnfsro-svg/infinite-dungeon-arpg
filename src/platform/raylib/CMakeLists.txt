add_library(arpg_raylib STATIC raylib_host.cpp)

target_include_directories(arpg_raylib
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_features(arpg_raylib PUBLIC cxx_std_17)
target_link_libraries(arpg_raylib PRIVATE arpg_core raylib)

arpg_enable_project_warnings(arpg_raylib)
