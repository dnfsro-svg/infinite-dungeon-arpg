add_executable(arpg_dungeon_tests
    ../core/allocation_probe.cpp
    dungeon_test_main.cpp
    dungeon_lifecycle_tests.cpp
    dungeon_navigation_tests.cpp
    dungeon_stress_tests.cpp
    room_generation_tests.cpp
    dungeon_rules_tests.cpp
    dungeon_progression_tests.cpp
    dungeon_passive_tree_tests.cpp
    dungeon_item_transaction_tests.cpp
    dungeon_equipment_stress_tests.cpp
    dungeon_loot_drop_tests.cpp
    dungeon_transaction_tests.cpp
    dungeon_abyss_reward_tests.cpp
    dungeon_abyss_stress_tests.cpp
    encounter_director_tests.cpp
    dungeon_wave_tests.cpp)
target_sources(arpg_dungeon_tests PRIVATE
    dungeon_progression_reward_tests.cpp
    dungeon_affix_reward_tests.cpp
    dungeon_affix_stress_tests.cpp)

target_compile_features(arpg_dungeon_tests PRIVATE cxx_std_17)
target_include_directories(arpg_dungeon_tests
    PRIVATE
        "${PROJECT_SOURCE_DIR}/tests/core"
        "${PROJECT_SOURCE_DIR}/tests/combat")
target_link_libraries(arpg_dungeon_tests PRIVATE arpg_dungeon arpg_persistence)
arpg_enable_project_warnings(arpg_dungeon_tests)

add_test(NAME dungeon.units COMMAND arpg_dungeon_tests)

add_executable(arpg_stage8_validation_fixture
    stage8_validation_fixture.cpp)
target_compile_features(arpg_stage8_validation_fixture PRIVATE cxx_std_17)
target_link_libraries(arpg_stage8_validation_fixture
    PRIVATE arpg_dungeon arpg_persistence)
arpg_enable_project_warnings(arpg_stage8_validation_fixture)

add_executable(arpg_stage9_validation_fixture
    stage9_validation_fixture.cpp)
target_compile_features(arpg_stage9_validation_fixture PRIVATE cxx_std_17)
target_include_directories(arpg_stage9_validation_fixture
    PRIVATE "${PROJECT_SOURCE_DIR}/tests/dungeon")
target_link_libraries(arpg_stage9_validation_fixture PRIVATE arpg_dungeon arpg_persistence)
arpg_enable_project_warnings(arpg_stage9_validation_fixture)

add_test(NAME stage9.validation_fixture.real_v4_reward_reload
    COMMAND arpg_stage9_validation_fixture)
set_tests_properties(stage9.validation_fixture.real_v4_reward_reload PROPERTIES
    LABELS "headless;dungeon;stage9;validation")

add_executable(arpg_stage10_validation_fixture
    stage10_validation_fixture.cpp)
target_compile_features(arpg_stage10_validation_fixture PRIVATE cxx_std_17)
target_link_libraries(arpg_stage10_validation_fixture
    PRIVATE arpg_dungeon arpg_persistence)
arpg_enable_project_warnings(arpg_stage10_validation_fixture)

add_test(NAME stage10.validation_fixture.real_abyss_transactions
    COMMAND arpg_stage10_validation_fixture)
set_tests_properties(stage10.validation_fixture.real_abyss_transactions PROPERTIES
    LABELS "headless;dungeon;stage10;validation"
    TIMEOUT 30)

add_test(NAME stage9.evidence.no_injected_defeat_trace
    COMMAND ${CMAKE_COMMAND}
        -DTRACE_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/dungeon_affix_stress_tests.cpp
        -DFIXTURE_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/stage9_validation_fixture.cpp
        -DHOST_HEADER=${PROJECT_SOURCE_DIR}/src/platform/raylib/raylib_host.hpp
        -DHOST_SOURCE=${PROJECT_SOURCE_DIR}/src/platform/raylib/raylib_host.cpp
        -P ${CMAKE_CURRENT_SOURCE_DIR}/stage9_evidence_guard_test.cmake)
set_tests_properties(stage9.evidence.no_injected_defeat_trace PROPERTIES
    LABELS "headless;dungeon;stage9;evidence")

if(TARGET arpg_raylib)
    add_executable(arpg_stage8_validation_game
        stage8_validation_game.cpp)
    target_compile_features(arpg_stage8_validation_game PRIVATE cxx_std_17)
    target_link_libraries(arpg_stage8_validation_game PRIVATE arpg_raylib)
    arpg_enable_project_warnings(arpg_stage8_validation_game)

    add_executable(arpg_stage9_validation_game
        stage9_validation_game.cpp)
    target_compile_features(arpg_stage9_validation_game PRIVATE cxx_std_17)
    target_link_libraries(arpg_stage9_validation_game PRIVATE arpg_raylib)
    arpg_enable_project_warnings(arpg_stage9_validation_game)

    add_executable(arpg_stage9_formal_game_validation
        stage9_formal_game_validation.cpp)
    target_compile_features(arpg_stage9_formal_game_validation PRIVATE cxx_std_17)
    target_link_libraries(arpg_stage9_formal_game_validation PRIVATE arpg_raylib)
    arpg_enable_project_warnings(arpg_stage9_formal_game_validation)

    add_executable(arpg_stage10_validation_game
        stage10_validation_game.cpp)
    target_compile_features(arpg_stage10_validation_game PRIVATE cxx_std_17)
    target_link_libraries(arpg_stage10_validation_game PRIVATE arpg_raylib)
    arpg_enable_project_warnings(arpg_stage10_validation_game)

    add_executable(arpg_stage10_formal_game_validation
        stage10_formal_game_validation.cpp)
    target_compile_features(arpg_stage10_formal_game_validation PRIVATE cxx_std_17)
    target_link_libraries(arpg_stage10_formal_game_validation
        PRIVATE arpg_raylib arpg_persistence)
    arpg_enable_project_warnings(arpg_stage10_formal_game_validation)

    add_test(NAME stage9.validation_game.capture_after_present
        COMMAND powershell -NoProfile -ExecutionPolicy Bypass
            -File "${CMAKE_CURRENT_SOURCE_DIR}/stage9_validation_capture_test.ps1"
            -Executable "$<TARGET_FILE:arpg_stage9_validation_game>"
            -CapturePath "$<TARGET_FILE_DIR:arpg_stage9_validation_game>/stage9-validation-capture.png")
    set_tests_properties(stage9.validation_game.capture_after_present PROPERTIES
        LABELS "graphics;stage9;validation"
        TIMEOUT 5)

    add_test(NAME stage9.formal_game.capture_after_present
        COMMAND powershell -NoProfile -ExecutionPolicy Bypass
            -File "${CMAKE_CURRENT_SOURCE_DIR}/stage9_formal_game_capture_test.ps1"
            -Executable "$<TARGET_FILE:arpg_stage9_formal_game_validation>"
            -CapturePath "$<TARGET_FILE_DIR:arpg_stage9_formal_game_validation>/stage9-formal-game-validation/stage8-validation-001.png")
    set_tests_properties(stage9.formal_game.capture_after_present PROPERTIES
        LABELS "graphics;stage9;validation;formal-game"
        TIMEOUT 15)

    add_test(NAME stage10.validation_game.capture_after_present
        COMMAND powershell -NoProfile -ExecutionPolicy Bypass
            -File "${CMAKE_CURRENT_SOURCE_DIR}/stage10_validation_capture_test.ps1"
            -Executable "$<TARGET_FILE:arpg_stage10_validation_game>"
            -CapturePath "$<TARGET_FILE_DIR:arpg_stage10_validation_game>/stage10-validation-capture.png")
    set_tests_properties(stage10.validation_game.capture_after_present PROPERTIES
        LABELS "graphics;stage10;validation"
        TIMEOUT 60)

    add_test(NAME stage10.formal_game.capture_after_present
        COMMAND powershell -NoProfile -ExecutionPolicy Bypass
            -File "${CMAKE_CURRENT_SOURCE_DIR}/stage10_formal_game_capture_test.ps1"
            -Executable "$<TARGET_FILE:arpg_stage10_formal_game_validation>"
            -EvidenceDirectory "$<TARGET_FILE_DIR:arpg_stage10_formal_game_validation>/stage10-formal-game-validation")
    set_tests_properties(stage10.formal_game.capture_after_present PROPERTIES
        LABELS "graphics;stage10;validation;formal-game"
        TIMEOUT 180)
endif()

if(WIN32)
    add_executable(arpg_stage8_validation_input
        stage8_validation_input.cpp)
    target_compile_features(arpg_stage8_validation_input PRIVATE cxx_std_17)
    target_link_libraries(arpg_stage8_validation_input PRIVATE user32)
    arpg_enable_project_warnings(arpg_stage8_validation_input)
endif()
set_tests_properties(dungeon.units PROPERTIES LABELS "headless;dungeon")

add_test(NAME stage10.abyss_stress.determinism_and_zero_alloc
    COMMAND arpg_dungeon_tests)
set_tests_properties(stage10.abyss_stress.determinism_and_zero_alloc PROPERTIES
    LABELS "headless;dungeon;stage10;stress"
    ENVIRONMENT "ARPG_STAGE10_ABYSS_STRESS_ONLY=1"
    TIMEOUT 240)

add_test(NAME stage10.evidence.no_private_injection
    COMMAND ${CMAKE_COMMAND}
        -DFIXTURE_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/stage10_validation_fixture.cpp
        -DVALIDATION_GAME_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/stage10_validation_game.cpp
        -DFORMAL_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/stage10_formal_game_validation.cpp
        -DCAPTURE_SCRIPT=${CMAKE_CURRENT_SOURCE_DIR}/stage10_validation_capture_test.ps1
        -DFORMAL_CAPTURE_SCRIPT=${CMAKE_CURRENT_SOURCE_DIR}/stage10_formal_game_capture_test.ps1
        -DSTRESS_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/dungeon_abyss_stress_tests.cpp
        -DHOST_HEADER=${PROJECT_SOURCE_DIR}/src/platform/raylib/raylib_host.hpp
        -DHOST_SOURCE=${PROJECT_SOURCE_DIR}/src/platform/raylib/raylib_host.cpp
        -P ${CMAKE_CURRENT_SOURCE_DIR}/stage10_evidence_guard_test.cmake)
set_tests_properties(stage10.evidence.no_private_injection PROPERTIES
    LABELS "headless;dungeon;stage10;evidence")

add_test(NAME stage10.evidence.rejects_private_injection
    COMMAND ${CMAKE_COMMAND}
        -DFIXTURE_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/stage10_evidence_bad_injection.txt
        -DVALIDATION_GAME_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/stage10_validation_game.cpp
        -DFORMAL_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/stage10_formal_game_validation.cpp
        -DCAPTURE_SCRIPT=${CMAKE_CURRENT_SOURCE_DIR}/stage10_validation_capture_test.ps1
        -DFORMAL_CAPTURE_SCRIPT=${CMAKE_CURRENT_SOURCE_DIR}/stage10_formal_game_capture_test.ps1
        -DSTRESS_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/dungeon_abyss_stress_tests.cpp
        -DHOST_HEADER=${PROJECT_SOURCE_DIR}/src/platform/raylib/raylib_host.hpp
        -DHOST_SOURCE=${PROJECT_SOURCE_DIR}/src/platform/raylib/raylib_host.cpp
        -P ${CMAKE_CURRENT_SOURCE_DIR}/stage10_evidence_guard_test.cmake)
set_tests_properties(stage10.evidence.rejects_private_injection PROPERTIES
    LABELS "headless;dungeon;stage10;evidence;negative"
    WILL_FAIL TRUE)

add_test(NAME stage10.evidence.rejects_pre_present_capture
    COMMAND ${CMAKE_COMMAND}
        -DFIXTURE_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/stage10_validation_fixture.cpp
        -DVALIDATION_GAME_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/stage10_validation_game.cpp
        -DFORMAL_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/stage10_formal_game_validation.cpp
        -DCAPTURE_SCRIPT=${CMAKE_CURRENT_SOURCE_DIR}/stage10_validation_capture_test.ps1
        -DFORMAL_CAPTURE_SCRIPT=${CMAKE_CURRENT_SOURCE_DIR}/stage10_formal_game_capture_test.ps1
        -DSTRESS_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/dungeon_abyss_stress_tests.cpp
        -DHOST_HEADER=${PROJECT_SOURCE_DIR}/src/platform/raylib/raylib_host.hpp
        -DHOST_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/stage10_evidence_bad_capture_order.txt
        -P ${CMAKE_CURRENT_SOURCE_DIR}/stage10_evidence_guard_test.cmake)
set_tests_properties(stage10.evidence.rejects_pre_present_capture PROPERTIES
    LABELS "headless;dungeon;stage10;evidence;negative"
    WILL_FAIL TRUE)
