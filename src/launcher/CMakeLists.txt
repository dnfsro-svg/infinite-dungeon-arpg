add_library(arpg_launcher_core STATIC
    launcher_core.cpp
    launcher_layout.cpp)

target_include_directories(arpg_launcher_core
    PUBLIC "${PROJECT_SOURCE_DIR}/src/launcher")
target_compile_features(arpg_launcher_core PUBLIC cxx_std_17)
arpg_enable_project_warnings(arpg_launcher_core)
if(MSVC)
    target_compile_options(arpg_launcher_core PRIVATE /utf-8)
endif()
set_target_properties(arpg_launcher_core PROPERTIES
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
