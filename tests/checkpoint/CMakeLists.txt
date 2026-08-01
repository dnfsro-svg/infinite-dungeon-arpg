add_executable(arpg_checkpoint_tests
    checkpoint_test_main.cpp
    checkpoint_schema_tests.cpp)

target_compile_features(arpg_checkpoint_tests PRIVATE cxx_std_17)
target_include_directories(arpg_checkpoint_tests
    PRIVATE "${PROJECT_SOURCE_DIR}/tests/core")
target_link_libraries(arpg_checkpoint_tests PRIVATE arpg_checkpoint)
arpg_enable_project_warnings(arpg_checkpoint_tests)

add_test(NAME checkpoint.units COMMAND arpg_checkpoint_tests)
set_tests_properties(checkpoint.units PROPERTIES
    LABELS "headless;checkpoint")

add_test(
    NAME architecture.checkpoint_no_runtime_headers
    COMMAND "${CMAKE_COMMAND}"
        "-DCHECKPOINT_SOURCE_DIR=${PROJECT_SOURCE_DIR}/src/checkpoint"
        "-DCHECKPOINT_CMAKE_FILE=${PROJECT_SOURCE_DIR}/src/checkpoint/CMakeLists.txt"
        "-DASSERT_TARGET_BOUNDARY_MODULE=${PROJECT_SOURCE_DIR}/cmake/AssertTargetDependencyBoundary.cmake"
        "-DCHECKPOINT_BOUNDARY_SELF_TEST_ROOT=${CMAKE_CURRENT_BINARY_DIR}/checkpoint-boundary-self-test"
        "-DCHECKPOINT_BOUNDARY_SELF_TEST_GENERATOR=${CMAKE_GENERATOR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/checkpoint_boundary_test.cmake")
set_tests_properties(architecture.checkpoint_no_runtime_headers PROPERTIES
    LABELS "headless;architecture;checkpoint")
