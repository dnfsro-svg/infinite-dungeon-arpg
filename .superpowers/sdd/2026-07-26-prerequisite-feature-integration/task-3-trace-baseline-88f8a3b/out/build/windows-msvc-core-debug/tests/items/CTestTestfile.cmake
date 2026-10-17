# CMake generated Testfile for 
# Source directory: Q:/tests/items
# Build directory: Q:/out/build/windows-msvc-core-debug/tests/items
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test([=[items.units]=] "Q:/out/build/windows-msvc-core-debug/bin/arpg_item_tests.exe")
set_tests_properties([=[items.units]=] PROPERTIES  LABELS "headless;items" _BACKTRACE_TRIPLES "Q:/tests/items/CMakeLists.txt;16;add_test;Q:/tests/items/CMakeLists.txt;0;")
add_test([=[platform.module_boundary]=] "C:/Program Files/CMake/bin/cmake.exe" "-DSOURCE_DIR=Q://src/items" "-DSOURCE_LABEL=Items" "-DFORBIDDEN_MODULE=raylib" "-DFORBIDDEN_LABEL=raylib" "-P" "Q://tests/platform/module_boundary_test.cmake")
set_tests_properties([=[platform.module_boundary]=] PROPERTIES  LABELS "headless;platform;architecture;items" _BACKTRACE_TRIPLES "Q:/tests/items/CMakeLists.txt;19;add_test;Q:/tests/items/CMakeLists.txt;0;")
