add_executable(arpg_persistence_tests
    persistence_test_main.cpp
    checkpoint_codec_tests.cpp
    save_store_tests.cpp
    save_store_fault_tests.cpp
    dungeon_save_integration_tests.cpp)

target_compile_features(arpg_persistence_tests PRIVATE cxx_std_17)
target_include_directories(arpg_persistence_tests
    PRIVATE
        "${PROJECT_SOURCE_DIR}/tests/core"
        "${PROJECT_SOURCE_DIR}/tests/dungeon")
target_link_libraries(arpg_persistence_tests PRIVATE arpg_persistence arpg_dungeon)
arpg_enable_project_warnings(arpg_persistence_tests)

add_test(NAME persistence.units COMMAND arpg_persistence_tests)
set_tests_properties(
    persistence.units PROPERTIES LABELS "headless;persistence")
