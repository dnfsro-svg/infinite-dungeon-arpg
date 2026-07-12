add_executable(arpg_modifier_tests
    modifier_test_main.cpp
    modifier_math_tests.cpp
    effect_set_tests.cpp)

target_include_directories(arpg_modifier_tests PRIVATE
    "${PROJECT_SOURCE_DIR}/tests/core"
    "${PROJECT_SOURCE_DIR}/src")
target_compile_features(arpg_modifier_tests PRIVATE cxx_std_17)
target_link_libraries(arpg_modifier_tests PRIVATE arpg_modifiers)
arpg_enable_project_warnings(arpg_modifier_tests)

add_test(NAME modifiers.units COMMAND arpg_modifier_tests)
set_tests_properties(modifiers.units PROPERTIES LABELS "headless;modifiers")
