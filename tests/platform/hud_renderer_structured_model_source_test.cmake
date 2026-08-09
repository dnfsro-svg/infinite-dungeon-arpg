cmake_minimum_required(VERSION 3.25)

foreach(REQUIRED_INPUT IN ITEMS HUD_RENDERER_SOURCE ACTIVE_SKILL_RENDERER_SOURCE
        DUNGEON_RUNTIME_SOURCE LOOT_SUCTION_SOURCE STAGE12_SOURCE STAGE16_SOURCE)
    if(NOT DEFINED ${REQUIRED_INPUT} OR NOT EXISTS "${${REQUIRED_INPUT}}")
        message(FATAL_ERROR
            "HUD renderer structured-model source input missing: ${REQUIRED_INPUT}")
    endif()
endforeach()

include("${CMAKE_CURRENT_LIST_DIR}/cpp_source_function_body.cmake")

function(count_substring INPUT NEEDLE OUTPUT)
    set(REMAINDER "${INPUT}")
    set(COUNT 0)
    string(LENGTH "${NEEDLE}" NEEDLE_LENGTH)
    while(TRUE)
        string(FIND "${REMAINDER}" "${NEEDLE}" FOUND)
        if(FOUND EQUAL -1)
            break()
        endif()
        math(EXPR COUNT "${COUNT} + 1")
        math(EXPR NEXT "${FOUND} + ${NEEDLE_LENGTH}")
        string(SUBSTRING "${REMAINDER}" ${NEXT} -1 REMAINDER)
    endwhile()
    set(${OUTPUT} ${COUNT} PARENT_SCOPE)
endfunction()

file(READ "${HUD_RENDERER_SOURCE}" HUD_SOURCE)
set(HUD_DRAW_MARKER
    "void HudRenderer::draw(const HudViewModel& view,\n    const HudLayout& layout, const MaterialPack& assets) const noexcept")
extract_cpp_function_body("${HUD_SOURCE}"
    "${HUD_DRAW_MARKER}"
    HUD_DRAW_BODY)
set(REQUIRED_BAR_AUDIT
    "record_ui_text_bounds(UiTextAuditPage::hud, role, draw_font, bar.label.bytes.data(), label_position, style.player_bar_font_size * layout.scale, 1.0F, label_container, style.player_bar_font_size * layout.scale, &blocker, 1U);")
set(REQUIRED_BAR_DRAW
    "draw_hud_text(draw_font, bar.label.bytes.data(), label_position, style.player_bar_font_size * layout.scale, palette.text, style.outline_pixels);")
foreach(REQUIRED_CALL IN ITEMS REQUIRED_BAR_AUDIT REQUIRED_BAR_DRAW)
    string(FIND "${HUD_DRAW_BODY}" "${${REQUIRED_CALL}}" FOUND)
    if(FOUND EQUAL -1)
        message(FATAL_ERROR
            "HudRenderer::draw must consume HudBarPlan::label in the real audit/draw call")
    endif()
endforeach()
string(REGEX MATCHALL "bar\\.label\\.bytes\\.data\\(\\)" BAR_LABEL_USES
    "${HUD_DRAW_BODY}")
list(LENGTH BAR_LABEL_USES BAR_LABEL_USE_COUNT)
if(NOT BAR_LABEL_USE_COUNT EQUAL 2)
    message(FATAL_ERROR
        "HudRenderer::draw must contain exactly the audit and draw label uses")
endif()

file(READ "${ACTIVE_SKILL_RENDERER_SOURCE}" ACTIVE_SOURCE)
extract_cpp_function_body("${ACTIVE_SOURCE}"
    "void ActiveSkillRenderer::draw_hud(const ActiveSkillHudModel& model,"
    ACTIVE_DRAW_BODY)
set(REQUIRED_KEY_DRAW
    "if (!hud_font_ready) continue; draw_skill_text(hud_font, slot.key_label.data(), bounds.x + 5.0F * scale, bounds.y + 3.0F * scale, 16.0F * scale, ui_text_contrast_style().primary); if (!slot.empty) {")
set(REQUIRED_COOLDOWN_DRAW
    "if (slot.cooldown_ratio <= 0.0F || slot.empty) continue; draw_skill_text(hud_font, slot.cooldown_label.data(), bounds.x + 18.0F * scale, bounds.y + 22.0F * scale, 16.0F * scale, ui_text_contrast_style().primary);")
foreach(REQUIRED_CALL IN ITEMS REQUIRED_KEY_DRAW REQUIRED_COOLDOWN_DRAW)
    string(FIND "${ACTIVE_DRAW_BODY}" "${${REQUIRED_CALL}}" FOUND)
    if(FOUND EQUAL -1)
        message(FATAL_ERROR
            "ActiveSkillRenderer::draw_hud must consume model text on the real draw path")
    endif()
endforeach()
foreach(MODEL_TOKEN IN ITEMS
        "slot.key_label.data()" "slot.cooldown_label.data()")
    string(REGEX REPLACE "([][+.*()^$?\\\\|])" "\\\\\\1"
        TOKEN_REGEX "${MODEL_TOKEN}")
    string(REGEX MATCHALL "${TOKEN_REGEX}" MODEL_USES "${ACTIVE_DRAW_BODY}")
    list(LENGTH MODEL_USES MODEL_USE_COUNT)
    if(NOT MODEL_USE_COUNT EQUAL 1)
        message(FATAL_ERROR
            "ActiveSkillRenderer::draw_hud model text must have one real use")
    endif()
endforeach()

foreach(DRAW_BODY IN ITEMS HUD_DRAW_BODY ACTIVE_DRAW_BODY)
    foreach(FORBIDDEN IN ITEMS
            "TextFormat(" "snprintf(" "\"Num" "\"HP" "\"XP" "\"LV"
            "\"seconds" "\"second" "%.0fs")
        string(FIND "${${DRAW_BODY}}" "${FORBIDDEN}" FOUND)
        if(NOT FOUND EQUAL -1)
            message(FATAL_ERROR
                "HUD draw function must not recalculate model text: ${FORBIDDEN}")
        endif()
    endforeach()
endforeach()

function(require_initialized_fixture PATH EXPECTED_COUNT OWNER
        CONSUMER_TOKEN MIN_CONSUMERS LABEL)
    file(READ "${PATH}" FIXTURE_SOURCE)
    normalize_cpp_source("${FIXTURE_SOURCE}" FIXTURE)
    string(REGEX REPLACE "\\( +" "(" FIXTURE "${FIXTURE}")
    string(REGEX REPLACE " +\\)" ")" FIXTURE "${FIXTURE}")
    set(DECLARATION_COUNT 0)
    set(INITIALIZED_OWNER_COUNT 0)
    foreach(CANDIDATE_OWNER IN ITEMS hints control_hints initialized)
        count_substring("${FIXTURE}"
            "platform::ControlHints ${CANDIDATE_OWNER}{};" OWNER_DECLARATIONS)
        math(EXPR DECLARATION_COUNT
            "${DECLARATION_COUNT} + ${OWNER_DECLARATIONS}")
        count_substring("${FIXTURE}"
            "platform::ControlHints ${CANDIDATE_OWNER}{}; platform::refresh_control_hints(${CANDIDATE_OWNER}, arpg::settings::default_settings());"
            OWNER_INITIALIZERS)
        math(EXPR INITIALIZED_OWNER_COUNT
            "${INITIALIZED_OWNER_COUNT} + ${OWNER_INITIALIZERS}")
    endforeach()
    if(NOT DECLARATION_COUNT EQUAL EXPECTED_COUNT)
        message(FATAL_ERROR
            "${LABEL} must have exactly ${EXPECTED_COUNT} ControlHints owners")
    endif()
    if(NOT INITIALIZED_OWNER_COUNT EQUAL DECLARATION_COUNT)
        message(FATAL_ERROR
            "${LABEL} must refresh every ControlHints owner before it can be consumed")
    endif()
    count_substring("${FIXTURE}" "${CONSUMER_TOKEN}" CONSUMER_COUNT)
    if(CONSUMER_COUNT LESS MIN_CONSUMERS)
        message(FATAL_ERROR
            "${LABEL} must consume the refreshed ${OWNER} owner")
    endif()
endfunction()

require_initialized_fixture("${DUNGEON_RUNTIME_SOURCE}" 1 hints
    ", hints," 1 "Dungeon runtime fixture")
require_initialized_fixture("${LOOT_SUCTION_SOURCE}" 1 hints
    "return hints;" 1 "Loot suction fixture")
require_initialized_fixture("${STAGE12_SOURCE}" 1 control_hints
    ", control_hints," 1 "Stage 12 fixture")
require_initialized_fixture("${STAGE16_SOURCE}" 2 hints
    ", hints," 2 "Stage 16 fixtures")
file(READ "${LOOT_SUCTION_SOURCE}" LOOT_SOURCE)
normalize_cpp_source("${LOOT_SOURCE}" LOOT_SOURCE)
count_substring("${LOOT_SOURCE}" "control_hints()" LOOT_HELPER_USES)
if(LOOT_HELPER_USES LESS 2)
    message(FATAL_ERROR
        "Loot suction renderer paths must consume the initialized helper")
endif()
