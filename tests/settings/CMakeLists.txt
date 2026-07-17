add_executable(arpg_settings_tests
    settings_test_main.cpp
    settings_types_tests.cpp
    settings_codec_tests.cpp)

target_include_directories(arpg_settings_tests PRIVATE
    "${PROJECT_SOURCE_DIR}/tests/core"
    "${PROJECT_SOURCE_DIR}/src")
target_compile_features(arpg_settings_tests PRIVATE cxx_std_17)
target_link_libraries(arpg_settings_tests PRIVATE arpg_settings)
arpg_enable_project_warnings(arpg_settings_tests)

add_test(NAME settings.units COMMAND arpg_settings_tests)
set_tests_properties(settings.units PROPERTIES LABELS "headless;settings")
