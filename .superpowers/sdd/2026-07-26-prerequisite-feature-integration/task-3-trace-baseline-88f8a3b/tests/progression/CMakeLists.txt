add_executable(arpg_progression_tests
    progression_test_main.cpp
    progression_rules_tests.cpp)

target_include_directories(arpg_progression_tests PRIVATE
    "${PROJECT_SOURCE_DIR}/tests/core"
    "${PROJECT_SOURCE_DIR}/src")
target_compile_features(arpg_progression_tests PRIVATE cxx_std_17)
target_link_libraries(arpg_progression_tests PRIVATE arpg_progression)
arpg_enable_project_warnings(arpg_progression_tests)

add_test(NAME progression.units COMMAND arpg_progression_tests)
set_tests_properties(progression.units PROPERTIES LABELS "headless;progression")
