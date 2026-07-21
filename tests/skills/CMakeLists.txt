add_executable(arpg_skills_tests
    skills_test_main.cpp
    skill_loadout_tests.cpp)

target_include_directories(arpg_skills_tests PRIVATE
    "${PROJECT_SOURCE_DIR}/tests/core"
    "${PROJECT_SOURCE_DIR}/src")
target_compile_features(arpg_skills_tests PRIVATE cxx_std_17)
target_link_libraries(arpg_skills_tests PRIVATE arpg_skills)
arpg_enable_project_warnings(arpg_skills_tests)

add_test(NAME skills.units COMMAND arpg_skills_tests)
set_tests_properties(skills.units PROPERTIES LABELS "headless;skills")
