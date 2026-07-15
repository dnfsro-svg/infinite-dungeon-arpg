add_executable(arpg_item_tests
    item_test_main.cpp
    item_catalog_tests.cpp)

target_include_directories(arpg_item_tests PRIVATE
    "${PROJECT_SOURCE_DIR}/tests/core"
    "${PROJECT_SOURCE_DIR}/src")
target_compile_features(arpg_item_tests PRIVATE cxx_std_17)
target_link_libraries(arpg_item_tests PRIVATE arpg_items)
arpg_enable_project_warnings(arpg_item_tests)

add_test(NAME items.units COMMAND arpg_item_tests)
set_tests_properties(items.units PROPERTIES LABELS "headless;items")

add_test(
    NAME platform.module_boundary
    COMMAND "${CMAKE_COMMAND}"
        "-DSOURCE_DIR=${PROJECT_SOURCE_DIR}/src/items"
        "-DSOURCE_LABEL=Items"
        "-DFORBIDDEN_MODULE=raylib"
        "-DFORBIDDEN_LABEL=raylib"
        -P "${PROJECT_SOURCE_DIR}/tests/platform/module_boundary_test.cmake")
set_tests_properties(
    platform.module_boundary
    PROPERTIES LABELS "headless;platform;architecture;items")
