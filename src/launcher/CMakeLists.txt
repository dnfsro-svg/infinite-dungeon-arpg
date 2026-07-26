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

if(TARGET arpg_game)
    add_executable(arpg_launcher WIN32
        launcher_main.cpp
        launcher_platform_win32.cpp
        launcher_renderer.cpp
        arpg_launcher.manifest
        arpg_launcher.rc)
    set_target_properties(arpg_launcher PROPERTIES
        OUTPUT_NAME "无限地下城启动器"
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    target_compile_features(arpg_launcher PRIVATE cxx_std_17)
    target_compile_definitions(arpg_launcher PRIVATE
        UNICODE
        _UNICODE
        WIN32_LEAN_AND_MEAN
        NOMINMAX)
    target_compile_options(arpg_launcher PRIVATE /utf-8)
    arpg_enable_project_warnings(arpg_launcher)
    target_link_libraries(arpg_launcher PRIVATE
        arpg_launcher_core
        d2d1
        dwrite
        shell32
        ole32
        user32
        gdi32)
    add_dependencies(arpg_launcher arpg_game)
endif()
