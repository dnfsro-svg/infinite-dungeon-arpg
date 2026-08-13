cmake_minimum_required(VERSION 3.21)

project(arpg_keyboard_driver LANGUAGES CXX)

add_executable(arpg_keyboard_driver keyboard_driver.cpp)
target_compile_features(arpg_keyboard_driver PRIVATE cxx_std_17)
target_compile_definitions(arpg_keyboard_driver PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN)
target_link_libraries(arpg_keyboard_driver PRIVATE user32)
if(MSVC)
    target_compile_options(arpg_keyboard_driver PRIVATE /EHsc /utf-8)
endif()

include(CTest)
if(BUILD_TESTING)
    function(add_active_skill_numpad_mapping_test token expected_label)
        add_test(NAME keyboard_driver.maps_active_skill_token_${token}_to_numpad
            COMMAND arpg_keyboard_driver --key ${token} --hold-ms 1 --dry-run)
        set_tests_properties(
            keyboard_driver.maps_active_skill_token_${token}_to_numpad
            PROPERTIES PASS_REGULAR_EXPRESSION "${expected_label} for 1 ms")
    endfunction()

    add_active_skill_numpad_mapping_test(1 Num1)
    add_active_skill_numpad_mapping_test(2 Num2)
    add_active_skill_numpad_mapping_test(3 Num3)
    add_active_skill_numpad_mapping_test(4 Num4)
    add_active_skill_numpad_mapping_test(5 Num5)

    function(add_allowed_key_label_test test_name token expected_label)
        add_test(NAME keyboard_driver.accepts_${test_name}
            COMMAND arpg_keyboard_driver --key ${token} --hold-ms 1 --dry-run)
        set_tests_properties(keyboard_driver.accepts_${test_name}
            PROPERTIES PASS_REGULAR_EXPRESSION "${expected_label} for 1 ms")
    endfunction()

    add_allowed_key_label_test(i_key i I)
    add_allowed_key_label_test(p_key p P)
    add_allowed_key_label_test(f1_key F1 F1)
    add_allowed_key_label_test(f12_key F12 F12)
    add_allowed_key_label_test(escape_key Esc Esc)
    add_allowed_key_label_test(enter_key eNtEr Enter)
    add_allowed_key_label_test(up_key uP Up)
    add_allowed_key_label_test(down_key DOWN Down)
    add_allowed_key_label_test(left_key lEfT Left)
    add_allowed_key_label_test(right_key RIGHT Right)

    add_executable(keyboard_driver_window_fixture WIN32
        tests/window_fixture.cpp)
    target_compile_features(keyboard_driver_window_fixture PRIVATE cxx_std_17)
    target_compile_definitions(keyboard_driver_window_fixture PRIVATE
        UNICODE _UNICODE WIN32_LEAN_AND_MEAN)
    target_link_libraries(keyboard_driver_window_fixture PRIVATE user32)
    if(MSVC)
        target_compile_options(keyboard_driver_window_fixture PRIVATE
            /EHsc /utf-8)
    endif()

    add_test(NAME keyboard_driver.rejects_ambiguous_default_target
        COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass
            -File ${CMAKE_CURRENT_SOURCE_DIR}/tests/ambiguous_default_target_test.ps1
            -Driver $<TARGET_FILE:arpg_keyboard_driver>
            -Fixture $<TARGET_FILE:keyboard_driver_window_fixture>)

    add_test(NAME keyboard_driver.explicit_pid_selects_requested_process
        COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass
            -File ${CMAKE_CURRENT_SOURCE_DIR}/tests/explicit_pid_target_test.ps1
            -Driver $<TARGET_FILE:arpg_keyboard_driver>
            -Fixture $<TARGET_FILE:keyboard_driver_window_fixture>)

    add_test(NAME keyboard_driver.explicit_pid_rejects_executable_mismatch
        COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass
            -File ${CMAKE_CURRENT_SOURCE_DIR}/tests/explicit_pid_rejection_test.ps1
            -Case executable-mismatch
            -Driver $<TARGET_FILE:arpg_keyboard_driver>
            -Fixture $<TARGET_FILE:keyboard_driver_window_fixture>)

    add_test(NAME keyboard_driver.explicit_pid_requires_unique_window
        COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass
            -File ${CMAKE_CURRENT_SOURCE_DIR}/tests/explicit_pid_rejection_test.ps1
            -Case multiple-windows
            -Driver $<TARGET_FILE:arpg_keyboard_driver>
            -Fixture $<TARGET_FILE:keyboard_driver_window_fixture>)

    add_test(NAME keyboard_driver.chord_dry_run_is_explicit
        COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass
            -File ${CMAKE_CURRENT_SOURCE_DIR}/tests/chord_behavior_test.ps1
            -Case dry-run
            -Driver $<TARGET_FILE:arpg_keyboard_driver>
            -Fixture $<TARGET_FILE:keyboard_driver_window_fixture>)

    add_test(NAME keyboard_driver.chord_releases_in_reverse_order
        COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass
            -File ${CMAKE_CURRENT_SOURCE_DIR}/tests/chord_behavior_test.ps1
            -Case key-order
            -Driver $<TARGET_FILE:arpg_keyboard_driver>
            -Fixture $<TARGET_FILE:keyboard_driver_window_fixture>)
endif()
