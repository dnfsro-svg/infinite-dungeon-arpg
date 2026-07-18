add_executable(arpg_settings_tests
    settings_test_main.cpp
    settings_types_tests.cpp
    settings_codec_tests.cpp
    settings_store_tests.cpp)

target_include_directories(arpg_settings_tests PRIVATE
    "${PROJECT_SOURCE_DIR}/tests/core"
    "${PROJECT_SOURCE_DIR}/src")
target_compile_features(arpg_settings_tests PRIVATE cxx_std_17)
target_link_libraries(arpg_settings_tests PRIVATE arpg_settings)
arpg_enable_project_warnings(arpg_settings_tests)

add_test(NAME settings.units COMMAND arpg_settings_tests)
set_tests_properties(settings.units PROPERTIES LABELS "headless;settings")

if(TARGET arpg_raylib)
    add_executable(arpg_settings_stress_tests
        ../core/allocation_probe.cpp
        settings_stress_tests.cpp)
    target_include_directories(arpg_settings_stress_tests PRIVATE
        "${PROJECT_SOURCE_DIR}/tests/core"
        "${PROJECT_SOURCE_DIR}/src"
        "${PROJECT_SOURCE_DIR}/src/platform/raylib")
    target_compile_features(arpg_settings_stress_tests PRIVATE cxx_std_17)
    target_link_libraries(arpg_settings_stress_tests PRIVATE arpg_raylib)
    arpg_enable_project_warnings(arpg_settings_stress_tests)
    if(MSVC)
        target_compile_options(arpg_settings_stress_tests PRIVATE /utf-8)
    endif()

    add_test(
        NAME stage11b.settings_stress.atomic_reload_zero_alloc
        COMMAND arpg_settings_stress_tests)
    set_tests_properties(
        stage11b.settings_stress.atomic_reload_zero_alloc
        PROPERTIES
            ENVIRONMENT "ARPG_TEST_FILTER=stage11b.settings_stress"
            LABELS "headless;settings;stress;stage11b"
            TIMEOUT 300)
endif()
