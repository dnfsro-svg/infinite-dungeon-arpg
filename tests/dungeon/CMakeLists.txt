add_executable(arpg_dungeon_tests
    ../core/allocation_probe.cpp
    dungeon_test_main.cpp
    death_checkpoint_tests.cpp
    dungeon_death_lifecycle_tests.cpp
    dungeon_death_stress_tests.cpp
    dungeon_lifecycle_tests.cpp
    dungeon_navigation_tests.cpp
    dungeon_stress_tests.cpp
    room_generation_tests.cpp
    dungeon_rules_tests.cpp
    dungeon_progression_tests.cpp
    dungeon_passive_tree_tests.cpp
    dungeon_item_transaction_tests.cpp
    dungeon_crafting_transaction_tests.cpp
    dungeon_skill_loadout_transaction_tests.cpp
    dungeon_equipment_stress_tests.cpp
    dungeon_loot_drop_tests.cpp
    dungeon_material_loot_tests.cpp
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
if(MSVC)
    # Several lifecycle fixtures intentionally keep multiple full fixed-capacity
    # sessions and snapshots alive at once. The Stage 16 material ground pool is
    # also fixed-capacity, so reserve enough stack for those debug-only fixtures.
    target_link_options(arpg_dungeon_tests PRIVATE /STACK:4194304)
endif()

add_test(NAME dungeon.units COMMAND arpg_dungeon_tests)

add_test(NAME stage16.material_loot.units COMMAND arpg_dungeon_tests)
set_tests_properties(stage16.material_loot.units PROPERTIES
    ENVIRONMENT "ARPG_STAGE16_MATERIAL_LOOT_ONLY=1"
    LABELS "headless;dungeon;stage16")

add_test(NAME stage16.crafting_transaction.units COMMAND arpg_dungeon_tests)
set_tests_properties(stage16.crafting_transaction.units PROPERTIES
    ENVIRONMENT "ARPG_STAGE16_CRAFTING_TRANSACTION_ONLY=1"
    LABELS "headless;dungeon;stage16")

add_executable(arpg_stage16_loot_reinforcement_simulation
    stage16_loot_reinforcement_simulation.cpp)
target_compile_features(arpg_stage16_loot_reinforcement_simulation PRIVATE cxx_std_17)
target_include_directories(arpg_stage16_loot_reinforcement_simulation
    PRIVATE "${PROJECT_SOURCE_DIR}/tests/dungeon")
target_link_libraries(arpg_stage16_loot_reinforcement_simulation
    PRIVATE arpg_dungeon arpg_persistence)
arpg_enable_project_warnings(arpg_stage16_loot_reinforcement_simulation)
add_test(NAME stage16.loot_reinforcement.simulation
    COMMAND arpg_stage16_loot_reinforcement_simulation)
set_tests_properties(stage16.loot_reinforcement.simulation PROPERTIES
    LABELS "headless;dungeon;stage16;validation" TIMEOUT 60)

add_test(NAME stage11.death_stress.determinism_zero_alloc
    COMMAND arpg_dungeon_tests)
set_tests_properties(stage11.death_stress.determinism_zero_alloc PROPERTIES
    ENVIRONMENT "ARPG_STAGE11_DEATH_STRESS_ONLY=1"
    LABELS "headless;dungeon;stage11;stress"
    TIMEOUT 300)

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

    add_test(NAME stage9.formal_game.capture_validator_self_test
        COMMAND powershell -NoProfile -ExecutionPolicy Bypass
            -File "${CMAKE_CURRENT_SOURCE_DIR}/stage9_formal_game_capture_validator_self_test.ps1"
            -Validator "${CMAKE_CURRENT_SOURCE_DIR}/stage9_formal_game_capture_test.ps1"
            -ReferenceImage "$<TARGET_FILE_DIR:arpg_stage9_formal_game_validation>/stage9-formal-game-validation/stage8-validation-001.png"
            -MutationRoot "${CMAKE_CURRENT_BINARY_DIR}/stage9-formal-capture-validator-mutations"
            -EmptyFailureValidator "${CMAKE_CURRENT_SOURCE_DIR}/stage9_validator_empty_failure_fixture.ps1")
    set_tests_properties(stage9.formal_game.capture_validator_self_test PROPERTIES
        DEPENDS stage9.formal_game.capture_after_present
        LABELS "graphics;stage9;validation;formal-game;negative"
        TIMEOUT 120)

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

    add_test(NAME stage10.formal_game.capture_content_validator
        COMMAND powershell -NoProfile -ExecutionPolicy Bypass
            -File "${CMAKE_CURRENT_SOURCE_DIR}/stage10_formal_capture_content_test.ps1"
            -ValidatorScript "${CMAKE_CURRENT_SOURCE_DIR}/stage10_formal_game_capture_test.ps1"
            -ScratchDirectory "$<TARGET_FILE_DIR:arpg_stage10_formal_game_validation>/stage10-capture-content-test")
    set_tests_properties(stage10.formal_game.capture_content_validator PROPERTIES
        LABELS "headless;stage10;validation;formal-game;evidence"
        TIMEOUT 60)
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

add_test(NAME stage10.evidence.rejects_pre_capture_with_post_dummy
    COMMAND ${CMAKE_COMMAND}
        -DFIXTURE_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/stage10_validation_fixture.cpp
        -DVALIDATION_GAME_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/stage10_validation_game.cpp
        -DFORMAL_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/stage10_formal_game_validation.cpp
        -DCAPTURE_SCRIPT=${CMAKE_CURRENT_SOURCE_DIR}/stage10_validation_capture_test.ps1
        -DFORMAL_CAPTURE_SCRIPT=${CMAKE_CURRENT_SOURCE_DIR}/stage10_formal_game_capture_test.ps1
        -DSTRESS_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/dungeon_abyss_stress_tests.cpp
        -DHOST_HEADER=${PROJECT_SOURCE_DIR}/src/platform/raylib/raylib_host.hpp
        -DHOST_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/stage10_evidence_bad_pre_capture_with_dummy.txt
        -P ${CMAKE_CURRENT_SOURCE_DIR}/stage10_evidence_guard_test.cmake)
set_tests_properties(stage10.evidence.rejects_pre_capture_with_post_dummy PROPERTIES
    LABELS "headless;dungeon;stage10;evidence;negative"
    WILL_FAIL TRUE)

add_executable(arpg_stage11_death_validation_fixture
    stage11_death_validation_fixture.cpp)
target_compile_features(arpg_stage11_death_validation_fixture PRIVATE cxx_std_17)
target_link_libraries(arpg_stage11_death_validation_fixture
    PRIVATE arpg_dungeon arpg_persistence)
arpg_enable_project_warnings(arpg_stage11_death_validation_fixture)
add_test(NAME stage11.death_fixture.transactions
    COMMAND arpg_stage11_death_validation_fixture)
set_tests_properties(stage11.death_fixture.transactions PROPERTIES
    LABELS "headless;dungeon;stage11;validation" TIMEOUT 120)

if(TARGET arpg_raylib)
    add_executable(arpg_stage11_death_formal_game_validation
        stage11_death_formal_game_validation.cpp)
    target_compile_features(arpg_stage11_death_formal_game_validation PRIVATE cxx_std_17)
    target_link_libraries(arpg_stage11_death_formal_game_validation
        PRIVATE arpg_raylib arpg_persistence)
    arpg_enable_project_warnings(arpg_stage11_death_formal_game_validation)
    add_test(NAME stage11.death_formal.five_paths
        COMMAND powershell -NoProfile -ExecutionPolicy Bypass
            -File "${CMAKE_CURRENT_SOURCE_DIR}/stage11_death_formal_capture_test.ps1"
            -Executable "$<TARGET_FILE:arpg_stage11_death_formal_game_validation>"
            -EvidenceDirectory "$<TARGET_FILE_DIR:arpg_stage11_death_formal_game_validation>/stage11-death-formal-validation")
    set_tests_properties(stage11.death_formal.five_paths PROPERTIES
        LABELS "graphics;stage11;validation;formal-game;evidence" TIMEOUT 300)
endif()

add_test(NAME stage11.death_evidence.production_paths
    COMMAND ${CMAKE_COMMAND}
        -DFIXTURE_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/stage11_death_validation_fixture.cpp
        -DFORMAL_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/stage11_death_formal_game_validation.cpp
        -DCAPTURE_SCRIPT=${CMAKE_CURRENT_SOURCE_DIR}/stage11_death_formal_capture_test.ps1
        -DHOST_HEADER=${PROJECT_SOURCE_DIR}/src/platform/raylib/raylib_host.hpp
        -DHOST_SOURCE=${PROJECT_SOURCE_DIR}/src/platform/raylib/raylib_host.cpp
        -P ${CMAKE_CURRENT_SOURCE_DIR}/stage11_death_evidence_guard_test.cmake)
set_tests_properties(stage11.death_evidence.production_paths PROPERTIES
    LABELS "headless;dungeon;stage11;evidence")

add_test(NAME stage11.death_evidence.rejects_private_injection
    COMMAND ${CMAKE_COMMAND}
        -DFIXTURE_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/stage11_evidence_bad_injection.txt
        -DFORMAL_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/stage11_death_formal_game_validation.cpp
        -DCAPTURE_SCRIPT=${CMAKE_CURRENT_SOURCE_DIR}/stage11_death_formal_capture_test.ps1
        -DHOST_HEADER=${PROJECT_SOURCE_DIR}/src/platform/raylib/raylib_host.hpp
        -DHOST_SOURCE=${PROJECT_SOURCE_DIR}/src/platform/raylib/raylib_host.cpp
        -P ${CMAKE_CURRENT_SOURCE_DIR}/stage11_death_evidence_guard_test.cmake)
set_tests_properties(stage11.death_evidence.rejects_private_injection PROPERTIES
    LABELS "headless;dungeon;stage11;evidence;negative" WILL_FAIL TRUE)

add_test(NAME stage11.death_evidence.rejects_pre_present_capture
    COMMAND ${CMAKE_COMMAND}
        -DFIXTURE_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/stage11_death_validation_fixture.cpp
        -DFORMAL_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/stage11_death_formal_game_validation.cpp
        -DCAPTURE_SCRIPT=${CMAKE_CURRENT_SOURCE_DIR}/stage11_death_formal_capture_test.ps1
        -DHOST_HEADER=${PROJECT_SOURCE_DIR}/src/platform/raylib/raylib_host.hpp
        -DHOST_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/stage11_evidence_bad_capture_order.txt
        -P ${CMAKE_CURRENT_SOURCE_DIR}/stage11_death_evidence_guard_test.cmake)
set_tests_properties(stage11.death_evidence.rejects_pre_present_capture PROPERTIES
    LABELS "headless;dungeon;stage11;evidence;negative" WILL_FAIL TRUE)

add_test(NAME stage11.death_evidence.rejects_public_death_injection
    COMMAND ${CMAKE_COMMAND}
        -DFIXTURE_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/stage11_evidence_bad_public_death_injection.txt
        -DFORMAL_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/stage11_death_formal_game_validation.cpp
        -DCAPTURE_SCRIPT=${CMAKE_CURRENT_SOURCE_DIR}/stage11_death_formal_capture_test.ps1
        -DHOST_HEADER=${PROJECT_SOURCE_DIR}/src/platform/raylib/raylib_host.hpp
        -DHOST_SOURCE=${PROJECT_SOURCE_DIR}/src/platform/raylib/raylib_host.cpp
        -P ${CMAKE_CURRENT_SOURCE_DIR}/stage11_death_evidence_guard_test.cmake)
set_tests_properties(stage11.death_evidence.rejects_public_death_injection PROPERTIES
    LABELS "headless;dungeon;stage11;evidence;negative" WILL_FAIL TRUE)

add_test(NAME stage11.death_evidence.mutation_self_test
    COMMAND ${CMAKE_COMMAND}
        -DGUARD_SCRIPT=${CMAKE_CURRENT_SOURCE_DIR}/stage11_death_evidence_guard_test.cmake
        -DVALID_FIXTURE=${CMAKE_CURRENT_SOURCE_DIR}/stage11_death_validation_fixture.cpp
        -DVALID_FORMAL=${CMAKE_CURRENT_SOURCE_DIR}/stage11_death_formal_game_validation.cpp
        -DVALID_CAPTURE=${CMAKE_CURRENT_SOURCE_DIR}/stage11_death_formal_capture_test.ps1
        -DVALID_HOST_HEADER=${PROJECT_SOURCE_DIR}/src/platform/raylib/raylib_host.hpp
        -DVALID_HOST_SOURCE=${PROJECT_SOURCE_DIR}/src/platform/raylib/raylib_host.cpp
        -DBAD_TEST_ACCESS=${CMAKE_CURRENT_SOURCE_DIR}/stage11_evidence_bad_injection.txt
        -DBAD_CAPTURE_ORDER=${CMAKE_CURRENT_SOURCE_DIR}/stage11_evidence_bad_capture_order.txt
        -P ${CMAKE_CURRENT_SOURCE_DIR}/stage11_death_evidence_guard_self_test.cmake)
set_tests_properties(stage11.death_evidence.mutation_self_test PROPERTIES
    LABELS "headless;dungeon;stage11;evidence")
