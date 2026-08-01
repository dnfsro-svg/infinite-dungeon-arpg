cmake_minimum_required(VERSION 3.21)

project(arpg_keyboard_driver LANGUAGES CXX)

add_executable(arpg_keyboard_driver keyboard_driver.cpp)
target_compile_features(arpg_keyboard_driver PRIVATE cxx_std_20)
target_compile_definitions(arpg_keyboard_driver PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN)
target_link_libraries(arpg_keyboard_driver PRIVATE user32)

include(CTest)
if(BUILD_TESTING)
    add_test(NAME keyboard_driver.accepts_active_skill_key_1
        COMMAND arpg_keyboard_driver --key 1 --hold-ms 1 --dry-run)
    set_tests_properties(keyboard_driver.accepts_active_skill_key_1 PROPERTIES
        PASS_REGULAR_EXPRESSION "1 for 1 ms")

    add_test(NAME keyboard_driver.accepts_active_skill_key_5
        COMMAND arpg_keyboard_driver --key 5 --hold-ms 1 --dry-run)
    set_tests_properties(keyboard_driver.accepts_active_skill_key_5 PROPERTIES
        PASS_REGULAR_EXPRESSION "5 for 1 ms")
endif()
