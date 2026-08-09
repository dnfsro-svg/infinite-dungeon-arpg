cmake_minimum_required(VERSION 3.25)

foreach(REQUIRED_INPUT IN ITEMS HUD_RENDERER_SOURCE ACTIVE_SKILL_RENDERER_SOURCE
        DUNGEON_RUNTIME_SOURCE LOOT_SUCTION_SOURCE STAGE12_SOURCE STAGE16_SOURCE)
    if(NOT DEFINED ${REQUIRED_INPUT} OR NOT EXISTS "${${REQUIRED_INPUT}}")
        message(FATAL_ERROR "HUD renderer structured-model source input missing: ${REQUIRED_INPUT}")
    endif()
endforeach()

function(read_normalized_source PATH OUTPUT)
    file(READ "${PATH}" VALUE)
    string(REGEX REPLACE "/\\*([^*]|\\*+[^*/])*\\*+/" "" VALUE "${VALUE}")
    string(REGEX REPLACE "//[^\r\n]*" "" VALUE "${VALUE}")
    string(REGEX REPLACE "[ \t\r\n]+" " " VALUE "${VALUE}")
    set(${OUTPUT} "${VALUE}" PARENT_SCOPE)
endfunction()

function(extract_between SOURCE_VARIABLE START_MARKER END_MARKER OUTPUT)
    set(SOURCE_TEXT "${${SOURCE_VARIABLE}}")
    string(FIND "${SOURCE_TEXT}" "${START_MARKER}" START)
    if(START EQUAL -1)
        message(FATAL_ERROR "Unable to isolate renderer function body")
    endif()
    string(SUBSTRING "${SOURCE_TEXT}" ${START} -1 SOURCE_TAIL)
    string(FIND "${SOURCE_TAIL}" "${END_MARKER}" LENGTH)
    if(LENGTH EQUAL -1)
        message(FATAL_ERROR "Unable to isolate renderer function body")
    endif()
    string(SUBSTRING "${SOURCE_TEXT}" ${START} ${LENGTH} BODY)
    set(${OUTPUT} "${BODY}" PARENT_SCOPE)
endfunction()

read_normalized_source("${HUD_RENDERER_SOURCE}" HUD_SOURCE)
extract_between(HUD_SOURCE
    "void HudRenderer::draw(const HudViewModel& view, const HudLayout& layout, const MaterialPack& assets) const noexcept {"
    "void CombatRenderer::draw_abyss_hud" HUD_DRAW_BODY)
string(REGEX MATCHALL "bar\\.label\\.bytes\\.data\\(\\)" BAR_LABEL_USES
    "${HUD_DRAW_BODY}")
list(LENGTH BAR_LABEL_USES BAR_LABEL_USE_COUNT)
if(BAR_LABEL_USE_COUNT LESS 2)
    message(FATAL_ERROR
        "HudRenderer::draw must consume each HudBarPlan label for audit and draw")
endif()

read_normalized_source("${ACTIVE_SKILL_RENDERER_SOURCE}" ACTIVE_SOURCE)
extract_between(ACTIVE_SOURCE
    "void ActiveSkillRenderer::draw_hud(const ActiveSkillHudModel& model,"
    "} }" ACTIVE_DRAW_BODY)
foreach(REQUIRED_USE IN ITEMS
        "draw_skill_text(hud_font, slot.key_label.data()"
        "draw_skill_text(hud_font, slot.cooldown_label.data()")
    string(FIND "${ACTIVE_DRAW_BODY}" "${REQUIRED_USE}" FOUND)
    if(FOUND EQUAL -1)
        message(FATAL_ERROR
            "ActiveSkillRenderer::draw_hud must consume model text: ${REQUIRED_USE}")
    endif()
endforeach()
foreach(FORBIDDEN IN ITEMS "\"Num" "\"seconds" "\"second" "%.0fs")
    string(FIND "${ACTIVE_DRAW_BODY}" "${FORBIDDEN}" FOUND)
    if(NOT FOUND EQUAL -1)
        message(FATAL_ERROR
            "ActiveSkillRenderer::draw_hud hard-coded fallback text is forbidden: ${FORBIDDEN}")
    endif()
endforeach()

read_normalized_source("${DUNGEON_RUNTIME_SOURCE}" DUNGEON_FIXTURE)
read_normalized_source("${LOOT_SUCTION_SOURCE}" LOOT_FIXTURE)
read_normalized_source("${STAGE12_SOURCE}" STAGE12_FIXTURE)
read_normalized_source("${STAGE16_SOURCE}" STAGE16_FIXTURE)

foreach(FIXTURE IN ITEMS DUNGEON_FIXTURE LOOT_FIXTURE STAGE12_FIXTURE)
    string(REGEX MATCHALL
        "refresh_control_hints\\([^;]*arpg::settings::default_settings\\(\\)\\);"
        INITIALIZERS "${${FIXTURE}}")
    list(LENGTH INITIALIZERS INITIALIZER_COUNT)
    if(INITIALIZER_COUNT LESS 1)
        message(FATAL_ERROR
            "Renderer fixture must initialize ControlHints from SettingsData: ${FIXTURE}")
    endif()
endforeach()
string(REGEX MATCHALL
    "refresh_control_hints\\([^;]*arpg::settings::default_settings\\(\\)\\);"
    STAGE16_INITIALIZERS "${STAGE16_FIXTURE}")
list(LENGTH STAGE16_INITIALIZERS STAGE16_INITIALIZER_COUNT)
if(STAGE16_INITIALIZER_COUNT LESS 2)
    message(FATAL_ERROR
        "Both Stage 16 renderer fixtures must initialize ControlHints from SettingsData")
endif()
string(FIND "${LOOT_FIXTURE}" "hints.revision =" LEGACY_REVISION_WRITE)
if(NOT LEGACY_REVISION_WRITE EQUAL -1)
    message(FATAL_ERROR
        "Loot suction fixture must not fake ControlHints initialization by revision")
endif()
