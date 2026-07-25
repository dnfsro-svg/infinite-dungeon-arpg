add_executable(arpg_launcher_tests
    launcher_test_main.cpp
    launcher_core_tests.cpp)

target_compile_features(arpg_launcher_tests PRIVATE cxx_std_17)
target_include_directories(arpg_launcher_tests
    PRIVATE "${PROJECT_SOURCE_DIR}/tests/core")
target_link_libraries(arpg_launcher_tests PRIVATE arpg_launcher_core)
arpg_enable_project_warnings(arpg_launcher_tests)
if(MSVC)
    target_compile_options(arpg_launcher_tests PRIVATE /utf-8)
endif()
set_target_properties(arpg_launcher_tests PROPERTIES
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

add_test(NAME launcher.units COMMAND arpg_launcher_tests)
set_tests_properties(launcher.units PROPERTIES LABELS "headless;launcher")
