cmake_minimum_required(VERSION 3.25)

foreach(REQUIRED_INPUT IN ITEMS HUD_RENDERER_SOURCE ACTIVE_SKILL_RENDERER_SOURCE
        GUARD_SCRIPT MUTATION_ROOT)
    if(NOT DEFINED ${REQUIRED_INPUT}
            OR ("${${REQUIRED_INPUT}}" STREQUAL "")
            OR (NOT REQUIRED_INPUT STREQUAL "MUTATION_ROOT"
                AND NOT EXISTS "${${REQUIRED_INPUT}}"))
        message(FATAL_ERROR "HUD renderer guard self-test input missing: ${REQUIRED_INPUT}")
    endif()
endforeach()

file(REMOVE_RECURSE "${MUTATION_ROOT}")
file(MAKE_DIRECTORY "${MUTATION_ROOT}")
file(READ "${HUD_RENDERER_SOURCE}" HUD_ORIGINAL)
file(READ "${ACTIVE_SKILL_RENDERER_SOURCE}" ACTIVE_ORIGINAL)

set(FIXTURE_TEXT
    "void fixture(){ arpg::platform::ControlHints hints{}; arpg::platform::refresh_control_hints(hints, arpg::settings::default_settings()); }\n")
set(STAGE16_FIXTURE_TEXT "${FIXTURE_TEXT}${FIXTURE_TEXT}")

function(run_guard LABEL HUD_TEXT ACTIVE_TEXT EXPECT_SUCCESS)
    set(ROOT "${MUTATION_ROOT}/${LABEL}")
    file(MAKE_DIRECTORY "${ROOT}")
    set(HUD_MUTANT "${ROOT}/hud_renderer.cpp")
    set(ACTIVE_MUTANT "${ROOT}/active_skill_renderer.cpp")
    set(DUNGEON_FIXTURE "${ROOT}/dungeon.cpp")
    set(LOOT_FIXTURE "${ROOT}/loot.cpp")
    set(STAGE12_FIXTURE "${ROOT}/stage12.cpp")
    set(STAGE16_FIXTURE "${ROOT}/stage16.cpp")
    file(WRITE "${HUD_MUTANT}" "${HUD_TEXT}")
    file(WRITE "${ACTIVE_MUTANT}" "${ACTIVE_TEXT}")
    file(WRITE "${DUNGEON_FIXTURE}" "${FIXTURE_TEXT}")
    file(WRITE "${LOOT_FIXTURE}" "${FIXTURE_TEXT}")
    file(WRITE "${STAGE12_FIXTURE}" "${FIXTURE_TEXT}")
    file(WRITE "${STAGE16_FIXTURE}" "${STAGE16_FIXTURE_TEXT}")
    execute_process(
        COMMAND "${CMAKE_COMMAND}"
            "-DHUD_RENDERER_SOURCE=${HUD_MUTANT}"
            "-DACTIVE_SKILL_RENDERER_SOURCE=${ACTIVE_MUTANT}"
            "-DDUNGEON_RUNTIME_SOURCE=${DUNGEON_FIXTURE}"
            "-DLOOT_SUCTION_SOURCE=${LOOT_FIXTURE}"
            "-DSTAGE12_SOURCE=${STAGE12_FIXTURE}"
            "-DSTAGE16_SOURCE=${STAGE16_FIXTURE}"
            -P "${GUARD_SCRIPT}"
        RESULT_VARIABLE RESULT
        OUTPUT_QUIET ERROR_QUIET)
    if(EXPECT_SUCCESS AND NOT RESULT EQUAL 0)
        message(FATAL_ERROR "HUD renderer guard rejected clean sources: ${LABEL}")
    endif()
    if(NOT EXPECT_SUCCESS AND RESULT EQUAL 0)
        message(FATAL_ERROR "HUD renderer guard missed mutation: ${LABEL}")
    endif()
endfunction()

run_guard(clean "${HUD_ORIGINAL}" "${ACTIVE_ORIGINAL}" TRUE)

string(REPLACE "bar.label.bytes.data()" "u8\"生命\""
    HUD_MUTATED "${HUD_ORIGINAL}")
set(HUD_MUTATED
    "${HUD_MUTATED}\nvoid decoy_bar_label_use(){ bar.label.bytes.data(); }\n")
run_guard(hud_ignores_bar_label "${HUD_MUTATED}" "${ACTIVE_ORIGINAL}" FALSE)

string(REPLACE "slot.key_label.data()" "\"Num1\""
    ACTIVE_MUTATED "${ACTIVE_ORIGINAL}")
set(ACTIVE_MUTATED
    "${ACTIVE_MUTATED}\nvoid decoy_key_label_use(){ slot.key_label.data(); }\n")
run_guard(active_hardcodes_num_keycap
    "${HUD_ORIGINAL}" "${ACTIVE_MUTATED}" FALSE)

string(REPLACE "slot.key_label.data()" "legacy_key_label(slot)"
    ACTIVE_MUTATED "${ACTIVE_ORIGINAL}")
set(ACTIVE_MUTATED
    "${ACTIVE_MUTATED}\nvoid decoy_key_label_use(){ slot.key_label.data(); }\n")
run_guard(active_ignores_key_label
    "${HUD_ORIGINAL}" "${ACTIVE_MUTATED}" FALSE)

string(REPLACE "slot.cooldown_label.data()" "\"2 seconds\""
    ACTIVE_MUTATED "${ACTIVE_ORIGINAL}")
set(ACTIVE_MUTATED
    "${ACTIVE_MUTATED}\nvoid decoy_cooldown_use(){ slot.cooldown_label.data(); }\n")
run_guard(active_hardcodes_seconds
    "${HUD_ORIGINAL}" "${ACTIVE_MUTATED}" FALSE)

string(REPLACE "slot.cooldown_label.data()" "legacy_cooldown_label(slot)"
    ACTIVE_MUTATED "${ACTIVE_ORIGINAL}")
set(ACTIVE_MUTATED
    "${ACTIVE_MUTATED}\nvoid decoy_cooldown_use(){ slot.cooldown_label.data(); }\n")
run_guard(active_ignores_cooldown_label
    "${HUD_ORIGINAL}" "${ACTIVE_MUTATED}" FALSE)
