add_executable(arpg_persistence_tests
    persistence_test_main.cpp
    checkpoint_codec_tests.cpp
    checkpoint_v8_skill_loadout_tests.cpp
    checkpoint_v9_tests.cpp
    death_checkpoint_codec_tests.cpp
    passive_tree_checkpoint_tests.cpp
    save_store_tests.cpp
    save_store_fault_tests.cpp
    save_commit_worker_tests.cpp
    dungeon_save_integration_tests.cpp)

target_compile_features(arpg_persistence_tests PRIVATE cxx_std_17)
target_include_directories(arpg_persistence_tests
    PRIVATE
        "${PROJECT_SOURCE_DIR}/tests/core"
        "${PROJECT_SOURCE_DIR}/tests/dungeon")
target_link_libraries(arpg_persistence_tests PRIVATE arpg_persistence arpg_dungeon)
arpg_enable_project_warnings(arpg_persistence_tests)
if(MSVC)
    # Existing dungeon integration fixtures keep a complete session and
    # snapshot in one test frame. Large V9 checkpoint/job objects remain
    # heap-only and are covered by persistence.save_commit_stack_guard.
    target_link_options(arpg_persistence_tests PRIVATE /STACK:4194304)
endif()

add_test(NAME persistence.units COMMAND arpg_persistence_tests)
set_tests_properties(
    persistence.units PROPERTIES LABELS "headless;persistence")

add_test(NAME persistence.save_commit_stack_guard
    COMMAND "${CMAKE_COMMAND}"
        -DROOT=${PROJECT_SOURCE_DIR}
        -P "${CMAKE_CURRENT_SOURCE_DIR}/save_commit_stack_guard_test.cmake")
set_tests_properties(persistence.save_commit_stack_guard PROPERTIES
    LABELS "headless;persistence")

add_executable(arpg_checkpoint_v9_wire_baseline
    checkpoint_v9_wire_baseline.cpp)
target_compile_features(arpg_checkpoint_v9_wire_baseline PRIVATE cxx_std_17)
target_include_directories(arpg_checkpoint_v9_wire_baseline PRIVATE
    "${PROJECT_SOURCE_DIR}/tests/combat")
target_link_libraries(arpg_checkpoint_v9_wire_baseline PRIVATE
    arpg_persistence arpg_dungeon)
arpg_enable_project_warnings(arpg_checkpoint_v9_wire_baseline)

add_test(NAME persistence.v9_wire_baseline
    COMMAND "${CMAKE_COMMAND}"
        -DPROBE=$<TARGET_FILE:arpg_checkpoint_v9_wire_baseline>
        -DOUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/v9_wire_baseline
        -P "${CMAKE_CURRENT_SOURCE_DIR}/checkpoint_v9_wire_baseline_test.cmake")
set_tests_properties(persistence.v9_wire_baseline PROPERTIES
    LABELS "headless;persistence;checkpoint;wire")
