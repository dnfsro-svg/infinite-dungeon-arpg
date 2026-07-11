add_executable(arpg_core_tests
    test_main.cpp
    allocation_probe.cpp
    fixed_step_tests.cpp
    deterministic_rng_tests.cpp
    fixed_pool_tests.cpp
    bounded_queue_tests.cpp)

target_compile_features(arpg_core_tests PRIVATE cxx_std_17)
target_link_libraries(arpg_core_tests PRIVATE arpg_core)
arpg_enable_project_warnings(arpg_core_tests)

add_test(NAME core.units COMMAND arpg_core_tests)
set_tests_properties(core.units PROPERTIES LABELS "headless;core")
