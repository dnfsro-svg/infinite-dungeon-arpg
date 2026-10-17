add_executable(arpg_passive_tests
    passive_test_main.cpp
    passive_tree_catalog_tests.cpp
    passive_tree_rules_tests.cpp)

target_include_directories(arpg_passive_tests PRIVATE
    "${PROJECT_SOURCE_DIR}/tests/core"
    "${PROJECT_SOURCE_DIR}/src")
target_compile_features(arpg_passive_tests PRIVATE cxx_std_17)
target_link_libraries(arpg_passive_tests PRIVATE arpg_passives)
arpg_enable_project_warnings(arpg_passive_tests)

add_test(NAME passives.units COMMAND arpg_passive_tests)
set_tests_properties(passives.units PROPERTIES LABELS "headless;passives")
