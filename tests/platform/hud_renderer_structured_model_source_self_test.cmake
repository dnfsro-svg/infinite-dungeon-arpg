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
set(MISSED_MUTATIONS)

set(DUNGEON_FIXTURE_TEXT
    "void fixture(){ platform::ControlHints hints{}; platform::refresh_control_hints(hints, arpg::settings::default_settings()); renderer.observe_hud(previous, current, status, hints, 0.0F, false); }\n")
set(LOOT_FIXTURE_TEXT
    "platform::ControlHints control_hints(){ platform::ControlHints hints{}; platform::refresh_control_hints(hints, arpg::settings::default_settings()); return hints; }\nvoid fixture(){ renderer.observe_hud(previous, current, status, control_hints(), 0.0F, false); }\n")
set(STAGE12_FIXTURE_TEXT
    "void fixture(){ platform::ControlHints control_hints{}; platform::refresh_control_hints(control_hints, arpg::settings::default_settings()); renderer.observe_hud(previous, current, status, control_hints, 0.0F, false); }\n")
set(STAGE16_FIXTURE_TEXT
    "${DUNGEON_FIXTURE_TEXT}${DUNGEON_FIXTURE_TEXT}")

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
    set(CURRENT_DUNGEON_FIXTURE_TEXT "${DUNGEON_FIXTURE_TEXT}")
    set(CURRENT_LOOT_FIXTURE_TEXT "${LOOT_FIXTURE_TEXT}")
    set(CURRENT_STAGE12_FIXTURE_TEXT "${STAGE12_FIXTURE_TEXT}")
    set(CURRENT_STAGE16_FIXTURE_TEXT "${STAGE16_FIXTURE_TEXT}")
    if(DEFINED FIXTURE_OVERRIDE)
        set(CURRENT_DUNGEON_FIXTURE_TEXT "${FIXTURE_OVERRIDE}")
        set(CURRENT_LOOT_FIXTURE_TEXT "${FIXTURE_OVERRIDE}")
        set(CURRENT_STAGE12_FIXTURE_TEXT "${FIXTURE_OVERRIDE}")
    endif()
    if(DEFINED STAGE16_FIXTURE_OVERRIDE)
        set(CURRENT_STAGE16_FIXTURE_TEXT "${STAGE16_FIXTURE_OVERRIDE}")
    endif()
    file(WRITE "${DUNGEON_FIXTURE}" "${CURRENT_DUNGEON_FIXTURE_TEXT}")
    file(WRITE "${LOOT_FIXTURE}" "${CURRENT_LOOT_FIXTURE_TEXT}")
    file(WRITE "${STAGE12_FIXTURE}" "${CURRENT_STAGE12_FIXTURE_TEXT}")
    file(WRITE "${STAGE16_FIXTURE}" "${CURRENT_STAGE16_FIXTURE_TEXT}")
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
        list(APPEND MISSED_MUTATIONS "${LABEL}")
        set(MISSED_MUTATIONS "${MISSED_MUTATIONS}" PARENT_SCOPE)
    endif()
endfunction()

run_guard(clean "${HUD_ORIGINAL}" "${ACTIVE_ORIGINAL}" TRUE)

string(REPLACE "bar.label.bytes.data()" "u8\"生命\""
    HUD_MUTATED "${HUD_ORIGINAL}")
set(HUD_DECOY
    "if (false) {\n        static_cast<void>(bar.label.bytes.data());\n        static_cast<void>(bar.label.bytes.data());\n    }\n    ")
string(REPLACE
    "const HudLayout& layout, const MaterialPack& assets) const noexcept {\n    if (!IsWindowReady()) return;"
    "const HudLayout& layout, const MaterialPack& assets) const noexcept {\n    ${HUD_DECOY}if (!IsWindowReady()) return;"
    HUD_MUTATED "${HUD_MUTATED}")
run_guard(hud_dead_correct_decoy_with_real_hardcode
    "${HUD_MUTATED}" "${ACTIVE_ORIGINAL}" FALSE)

string(REPLACE "slot.key_label.data()" "legacy_key_label(slot)"
    ACTIVE_MUTATED "${ACTIVE_ORIGINAL}")
string(REPLACE "slot.cooldown_label.data()" "legacy_cooldown_label(slot)"
    ACTIVE_MUTATED "${ACTIVE_MUTATED}")
set(ACTIVE_DECOY
    "if (false) {\n        draw_skill_text(hud_font, slot.key_label.data(), 0.0F, 0.0F, 1.0F, WHITE);\n        draw_skill_text(hud_font, slot.cooldown_label.data(), 0.0F, 0.0F, 1.0F, WHITE);\n    }\n    ")
string(REPLACE
    "const MaterialPack& material_pack) const noexcept {\n    const float scale"
    "const MaterialPack& material_pack) const noexcept {\n    ${ACTIVE_DECOY}const float scale"
    ACTIVE_MUTATED "${ACTIVE_MUTATED}")
run_guard(active_dead_correct_decoy_with_real_legacy_text
    "${HUD_ORIGINAL}" "${ACTIVE_MUTATED}" FALSE)

set(FIXTURE_OVERRIDE
    "void fixture(){ platform::ControlHints initialized{}; platform::refresh_control_hints(initialized, arpg::settings::default_settings()); platform::ControlHints hints{}; renderer.observe_hud(previous, current, status, hints, 0.0F, false); }\n")
set(STAGE16_FIXTURE_OVERRIDE "${FIXTURE_OVERRIDE}${FIXTURE_OVERRIDE}")
run_guard(refreshes_unused_hints_but_passes_uninitialized
    "${HUD_ORIGINAL}" "${ACTIVE_ORIGINAL}" FALSE)
unset(FIXTURE_OVERRIDE)
unset(STAGE16_FIXTURE_OVERRIDE)

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

if(MISSED_MUTATIONS)
    list(JOIN MISSED_MUTATIONS ", " MISSED_LABELS)
    message(FATAL_ERROR
        "HUD renderer guard missed mutations: ${MISSED_LABELS}")
endif()
