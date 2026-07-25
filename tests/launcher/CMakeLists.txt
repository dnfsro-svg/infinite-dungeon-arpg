add_executable(arpg_launcher_tests
    launcher_test_main.cpp
    launcher_core_tests.cpp
    launcher_layout_tests.cpp)

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

find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_test(
    NAME launcher.icon_asset
    COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/launcher_icon_tests.py")
set_tests_properties(launcher.icon_asset PROPERTIES
    LABELS "headless;launcher;assets"
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}")
