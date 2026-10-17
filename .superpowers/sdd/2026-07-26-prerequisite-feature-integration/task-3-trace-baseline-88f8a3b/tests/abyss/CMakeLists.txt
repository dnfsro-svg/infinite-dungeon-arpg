add_executable(arpg_abyss_tests
    abyss_test_main.cpp
    abyss_rules_tests.cpp
    abyss_rewards_tests.cpp)

target_include_directories(arpg_abyss_tests PRIVATE
    "${PROJECT_SOURCE_DIR}/tests/core"
    "${PROJECT_SOURCE_DIR}/src")
target_compile_features(arpg_abyss_tests PRIVATE cxx_std_17)
target_link_libraries(arpg_abyss_tests PRIVATE arpg_abyss)
arpg_enable_project_warnings(arpg_abyss_tests)

add_test(NAME abyss.units COMMAND arpg_abyss_tests)
set_tests_properties(abyss.units PROPERTIES LABELS "headless;abyss")
