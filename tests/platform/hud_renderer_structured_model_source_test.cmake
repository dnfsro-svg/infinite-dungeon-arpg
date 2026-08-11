cmake_minimum_required(VERSION 3.25)

foreach(REQUIRED_INPUT IN ITEMS HUD_RENDERER_SOURCE ACTIVE_SKILL_RENDERER_SOURCE
        UI_TEXT_RENDERER_SOURCE UI_TEXT_BOUNDS_AUDIT_SOURCE
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

function(require_real_code BODY LABEL)
    string(REGEX MATCH "if[ ]*\\([ ]*(false|0)[ ]*\\)"
        DEAD_BRANCH "${BODY}")
    if(DEAD_BRANCH)
        message(FATAL_ERROR "${LABEL} must not hide required calls in constant-false code")
    endif()
    string(REGEX MATCH "auto[ ]+[A-Za-z_][A-Za-z0-9_]*[ ]*=[ ]*\\["
        UNCALLED_LAMBDA "${BODY}")
    if(UNCALLED_LAMBDA)
        message(FATAL_ERROR "${LABEL} must not hide required calls in an uncalled lambda")
    endif()
endfunction()

file(READ "${HUD_RENDERER_SOURCE}" HUD_SOURCE)
set(HUD_DRAW_MARKER
    "void HudRenderer::draw(const HudViewModel& view,\n    const HudLayout& layout, const MaterialPack& assets) const noexcept")
extract_cpp_function_body("${HUD_SOURCE}"
    "${HUD_DRAW_MARKER}"
    HUD_DRAW_BODY HUD_DRAW_CODE)
set(REQUIRED_BAR_AUDIT
    "record_ui_text_bounds(UiTextAuditPage::hud, role, draw_font, bar.label.bytes.data(), label_position, style.player_bar_font_size * layout.scale, 1.0F, label_container, style.player_bar_font_size * layout.scale, &blocker, 1U);")
set(REQUIRED_BAR_DRAW
    "draw_hud_text(draw_font, bar.label.bytes.data(), label_position, style.player_bar_font_size * layout.scale, palette.text, style.outline_pixels);")
foreach(REQUIRED_CALL IN ITEMS REQUIRED_BAR_AUDIT REQUIRED_BAR_DRAW)
    count_substring("${HUD_DRAW_BODY}" "${${REQUIRED_CALL}}"
        REQUIRED_CALL_COUNT)
    if(NOT REQUIRED_CALL_COUNT EQUAL 1)
        message(FATAL_ERROR
            "HudRenderer::draw must contain exactly one required audit/draw call")
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
    ACTIVE_DRAW_BODY ACTIVE_DRAW_CODE)
require_real_code("${ACTIVE_DRAW_CODE}" "ActiveSkillRenderer::draw_hud")
set(REQUIRED_INVALID_LAYOUT_GUARD "if (!layout.valid) return;")
string(FIND "${ACTIVE_DRAW_CODE}" "${REQUIRED_INVALID_LAYOUT_GUARD}"
    INVALID_LAYOUT_GUARD_INDEX)
if(INVALID_LAYOUT_GUARD_INDEX EQUAL -1)
    message(FATAL_ERROR
        "ActiveSkillRenderer::draw_hud must reject invalid layout before drawing")
endif()
if(NOT INVALID_LAYOUT_GUARD_INDEX EQUAL 0)
    message(FATAL_ERROR
        "ActiveSkillRenderer::draw_hud must begin with the invalid-layout guard")
endif()
foreach(FIRST_DRAW_TOKEN IN ITEMS
        "material_pack.draw_to(" "DrawRectangleRounded(" "DrawRectangleRec(")
    string(FIND "${ACTIVE_DRAW_CODE}" "${FIRST_DRAW_TOKEN}" FIRST_DRAW_INDEX)
    if(NOT FIRST_DRAW_INDEX EQUAL -1
            AND NOT INVALID_LAYOUT_GUARD_INDEX LESS FIRST_DRAW_INDEX)
        message(FATAL_ERROR
            "ActiveSkillRenderer::draw_hud must reject invalid layout before its first draw")
    endif()
endforeach()

set(REQUIRED_TEXT_PLAN
    "const ActiveSkillHudTextPlan text_plan = make_active_skill_hud_text_plan( slot, layout.text_regions[index], layout.scale, hud_font_ready && IsFontValid(hud_font), &measure_active_skill_text, &measure_context);")
count_substring("${ACTIVE_DRAW_BODY}" "${REQUIRED_TEXT_PLAN}" TEXT_PLAN_COUNT)
if(NOT TEXT_PLAN_COUNT EQUAL 1)
    message(FATAL_ERROR
        "ActiveSkillRenderer::draw_hud must build one shared-font active-skill text plan")
endif()
string(FIND "${ACTIVE_DRAW_CODE}" "${REQUIRED_TEXT_PLAN}" TEXT_PLAN_INDEX)
if(NOT INVALID_LAYOUT_GUARD_INDEX LESS TEXT_PLAN_INDEX)
    message(FATAL_ERROR
        "ActiveSkillRenderer::draw_hud must reject invalid layout before making a text plan")
endif()
string(FIND "${ACTIVE_DRAW_CODE}" "material_pack.draw_to(" ARTWORK_DRAW_INDEX)
if(ARTWORK_DRAW_INDEX EQUAL -1 OR NOT ARTWORK_DRAW_INDEX LESS TEXT_PLAN_INDEX)
    message(FATAL_ERROR
        "Active-skill slot artwork must be drawn before the text plan is made")
endif()

set(LAST_TEXT_DRAW_INDEX ${TEXT_PLAN_INDEX})
foreach(REQUIRED_PLATE_DRAW IN ITEMS
        "draw_active_skill_text_plate(hud_font, text_plan.key);"
        "draw_active_skill_text_plate(hud_font, text_plan.cooldown);"
        "draw_active_skill_text_plate(hud_font, text_plan.name);")
    count_substring("${ACTIVE_DRAW_BODY}" "${REQUIRED_PLATE_DRAW}"
        PLATE_DRAW_COUNT)
    if(NOT PLATE_DRAW_COUNT EQUAL 1)
        message(FATAL_ERROR
            "ActiveSkillRenderer::draw_hud must draw each real active-skill text plan")
    endif()
    string(FIND "${ACTIVE_DRAW_CODE}" "${REQUIRED_PLATE_DRAW}" PLATE_DRAW_INDEX)
    if(NOT PLATE_DRAW_INDEX GREATER LAST_TEXT_DRAW_INDEX)
        message(FATAL_ERROR
            "Active-skill key, cooldown, and name plans must draw in order")
    endif()
    set(LAST_TEXT_DRAW_INDEX ${PLATE_DRAW_INDEX})
endforeach()

extract_cpp_function_body("${ACTIVE_SOURCE}"
    "void draw_active_skill_text_plate(Font font,"
    ACTIVE_PLATE_BODY ACTIVE_PLATE_CODE)
require_real_code("${ACTIVE_PLATE_CODE}" "Active-skill text plate helper")
set(REQUIRED_PLATE_EARLY_RETURN "if (!plan.visible) return;")
string(FIND "${ACTIVE_PLATE_CODE}" "${REQUIRED_PLATE_EARLY_RETURN}"
    PLATE_EARLY_RETURN_INDEX)
if(NOT PLATE_EARLY_RETURN_INDEX EQUAL 0)
    message(FATAL_ERROR
        "Active-skill text plate helper must begin by rejecting invisible plans")
endif()
set(REQUIRED_PLATE_BACKING
    "DrawRectangleRec(plan.backing, active_skill_text_plate_backing());")
set(REQUIRED_PLATE_AUDIT
    "record_ui_text_bounds(UiTextAuditPage::hud, plan.role, font, plan.text.data(), plan.position, plan.font_size, plan.spacing, plan.backing, plan.font_size, nullptr, 0U, 0);")
set(REQUIRED_PLATE_TEXT
    "draw_crisp_ui_text(font, plan.text.data(), plan.position, plan.font_size, plan.spacing, ui_text_contrast_style().primary, 0);")
set(LAST_PLATE_INDEX -1)
foreach(REQUIRED_PLATE_CALL IN ITEMS REQUIRED_PLATE_BACKING REQUIRED_PLATE_AUDIT REQUIRED_PLATE_TEXT)
    count_substring("${ACTIVE_PLATE_BODY}" "${${REQUIRED_PLATE_CALL}}"
        PLATE_CALL_COUNT)
    if(NOT PLATE_CALL_COUNT EQUAL 1)
        message(FATAL_ERROR
            "Active-skill text plate helper must use exact backing, audit, and zero-shadow text")
    endif()
    string(FIND "${ACTIVE_PLATE_CODE}" "${${REQUIRED_PLATE_CALL}}"
        PLATE_CALL_INDEX)
    if(NOT PLATE_CALL_INDEX GREATER LAST_PLATE_INDEX)
        message(FATAL_ERROR
            "Active-skill text plate must draw backing, then audit, then text")
    endif()
    set(LAST_PLATE_INDEX ${PLATE_CALL_INDEX})
endforeach()

file(READ "${UI_TEXT_RENDERER_SOURCE}" TEXT_RENDERER_SOURCE)
extract_cpp_function_body("${TEXT_RENDERER_SOURCE}"
    "void draw_crisp_ui_text(Font font, const char* text,"
    TEXT_RENDERER TEXT_RENDERER_CODE)
require_real_code("${TEXT_RENDERER_CODE}" "draw_crisp_ui_text")
set(REQUIRED_CONDITIONAL_SHADOW
    "if (shadow_pixels > 0) { DrawTextEx(font, text, shadow, font_size, spacing, style.shadow); }")
count_substring("${TEXT_RENDERER}" "${REQUIRED_CONDITIONAL_SHADOW}"
    CONDITIONAL_SHADOW_COUNT)
if(NOT CONDITIONAL_SHADOW_COUNT EQUAL 1)
    message(FATAL_ERROR
        "draw_crisp_ui_text must draw a shadow only when requested")
endif()

file(READ "${UI_TEXT_BOUNDS_AUDIT_SOURCE}" TEXT_AUDIT_SOURCE)
extract_cpp_function_body("${TEXT_AUDIT_SOURCE}"
    "[[nodiscard]] Rectangle measured_bounds(Font font, const char* text,"
    TEXT_AUDIT_MEASURED TEXT_AUDIT_MEASURED_CODE)
extract_cpp_function_body("${TEXT_AUDIT_SOURCE}"
    "void record_ui_text_bounds(UiTextAuditPage page, UiTextAuditRole role,"
    TEXT_AUDIT_RECORD TEXT_AUDIT_RECORD_CODE)
require_real_code("${TEXT_AUDIT_MEASURED_CODE}" "UI text measured bounds")
require_real_code("${TEXT_AUDIT_RECORD_CODE}" "record_ui_text_bounds")
set(TEXT_AUDIT "${TEXT_AUDIT_MEASURED}")
set(REQUIRED_EFFECTIVE_SHADOW
    "const int effective_shadow = shadow_pixels > 0 ? shadow_pixels : 0;")
set(REQUIRED_AUDIT_SHADOW_USE
    "style.outline_pixels, effective_shadow")
foreach(REQUIRED_AUDIT_TOKEN IN ITEMS
        REQUIRED_EFFECTIVE_SHADOW REQUIRED_AUDIT_SHADOW_USE)
    count_substring("${TEXT_AUDIT}" "${${REQUIRED_AUDIT_TOKEN}}"
        AUDIT_TOKEN_COUNT)
    if(NOT AUDIT_TOKEN_COUNT EQUAL 1)
        message(FATAL_ERROR
            "record_ui_text_bounds must measure its explicit shadow size")
    endif()
endforeach()
set(REQUIRED_AUDIT_MEASURE_CALL
    "font, text, position, font_size, spacing, shadow_pixels);")
count_substring("${TEXT_AUDIT_RECORD}" "${REQUIRED_AUDIT_MEASURE_CALL}"
    AUDIT_MEASURE_CALL_COUNT)
if(NOT AUDIT_MEASURE_CALL_COUNT EQUAL 1)
    message(FATAL_ERROR
        "record_ui_text_bounds must pass the explicit shadow to measured bounds")
endif()

extract_cpp_function_body("${ACTIVE_SOURCE}"
    "[[nodiscard]] UiTextExtent measure_active_skill_text(const char* text,"
    ACTIVE_MEASURE_BODY ACTIVE_MEASURE_CODE)
require_real_code("${ACTIVE_MEASURE_CODE}" "Active-skill MeasureTextEx adapter")
foreach(REQUIRED_MEASURE_CALL IN ITEMS
        "static_cast<const ActiveSkillTextMeasureContext*>(context);"
        "!IsFontValid(measure_context->font)"
        "MeasureTextEx(measure_context->font, text, font_size, spacing);")
    count_substring("${ACTIVE_MEASURE_BODY}" "${REQUIRED_MEASURE_CALL}"
        MEASURE_CALL_COUNT)
    if(NOT MEASURE_CALL_COUNT EQUAL 1)
        message(FATAL_ERROR
            "Active-skill MeasureTextEx adapter must use its real shared-font context")
    endif()
endforeach()
count_substring("${ACTIVE_DRAW_BODY}"
    "ActiveSkillTextMeasureContext measure_context{hud_font};"
    MEASURE_CONTEXT_COUNT)
if(NOT MEASURE_CONTEXT_COUNT EQUAL 1)
    message(FATAL_ERROR
        "Active-skill HUD must create one shared-font measure context")
endif()
foreach(FORBIDDEN_ACTIVE_TOKEN IN ITEMS
        "GetFontDefault(" "DrawText(" "TextFormat(" "snprintf(" "std::string")
    set(ACTIVE_REAL_CODE
        "${ACTIVE_DRAW_CODE} ${ACTIVE_PLATE_CODE} ${ACTIVE_MEASURE_CODE}")
    string(FIND "${ACTIVE_REAL_CODE}" "${FORBIDDEN_ACTIVE_TOKEN}"
        FORBIDDEN_ACTIVE_INDEX)
    if(NOT FORBIDDEN_ACTIVE_INDEX EQUAL -1)
        message(FATAL_ERROR
            "ActiveSkillRenderer::draw_hud must not use ${FORBIDDEN_ACTIVE_TOKEN}")
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

foreach(DRAW_CODE IN ITEMS HUD_DRAW_CODE ACTIVE_DRAW_CODE)
    string(REGEX MATCH "if[ ]*\\([ ]*(false|0)[ ]*\\)"
        DEAD_BRANCH "${${DRAW_CODE}}")
    if(DEAD_BRANCH)
        message(FATAL_ERROR
            "HUD draw function must not hide required calls in a compile-time dead branch")
    endif()
    foreach(FORBIDDEN_CODE_REGEX IN ITEMS
            "TextFormat[ ]*\\("
            "(std::)?snprintf[ ]*\\(")
        string(REGEX MATCH "${FORBIDDEN_CODE_REGEX}" FOUND "${${DRAW_CODE}}")
        if(FOUND)
            message(FATAL_ERROR
                "HUD draw function must not format legacy model text")
        endif()
    endforeach()
endforeach()

function(require_initialized_fixture PATH EXPECTED_COUNT OWNER
        CONSUMER_TOKEN MIN_CONSUMERS LABEL)
    file(READ "${PATH}" FIXTURE_SOURCE)
    normalize_cpp_source("${FIXTURE_SOURCE}" FIXTURE FIXTURE_CODE)
    string(REGEX REPLACE "\\( +" "(" FIXTURE "${FIXTURE}")
    string(REGEX REPLACE " +\\)" ")" FIXTURE "${FIXTURE}")
    string(REGEX REPLACE "\\( +" "(" FIXTURE_CODE "${FIXTURE_CODE}")
    string(REGEX REPLACE " +\\)" ")" FIXTURE_CODE "${FIXTURE_CODE}")
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

    set(INITIALIZER
        "platform::ControlHints ${OWNER}{}; platform::refresh_control_hints(${OWNER}, arpg::settings::default_settings());")
    set(REMAINDER "${FIXTURE_CODE}")
    foreach(SEQUENCE_INDEX RANGE 1 ${EXPECTED_COUNT})
        string(FIND "${REMAINDER}" "${INITIALIZER}" INITIALIZER_INDEX)
        if(INITIALIZER_INDEX EQUAL -1)
            message(FATAL_ERROR
                "${LABEL} must initialize each consumed ${OWNER} owner")
        endif()
        string(LENGTH "${INITIALIZER}" INITIALIZER_LENGTH)
        math(EXPR AFTER_INITIALIZER
            "${INITIALIZER_INDEX} + ${INITIALIZER_LENGTH}")
        string(SUBSTRING "${REMAINDER}" ${AFTER_INITIALIZER} -1
            AFTER_REFRESH)
        string(FIND "${AFTER_REFRESH}" "${CONSUMER_TOKEN}" CONSUMER_INDEX)
        if(CONSUMER_INDEX EQUAL -1)
            message(FATAL_ERROR
                "${LABEL} must consume ${OWNER} after refresh")
        endif()
        string(SUBSTRING "${AFTER_REFRESH}" 0 ${CONSUMER_INDEX}
            REFRESH_TO_CONSUMER)
        foreach(INVALIDATION_REGEX IN ITEMS
                "${OWNER}[ ]*=[^=]"
                "(std::)?memset[ ]*\\([ ]*&[ ]*${OWNER}"
                "${OWNER}[ ]*\\.[ ]*(reset|clear)[ ]*\\(")
            string(REGEX MATCH "${INVALIDATION_REGEX}" INVALIDATION
                "${REFRESH_TO_CONSUMER}")
            if(INVALIDATION)
                message(FATAL_ERROR
                    "${LABEL} invalidates ${OWNER} after refresh and before consumption")
            endif()
        endforeach()
        string(LENGTH "${CONSUMER_TOKEN}" CONSUMER_LENGTH)
        math(EXPR AFTER_CONSUMER "${CONSUMER_INDEX} + ${CONSUMER_LENGTH}")
        string(SUBSTRING "${AFTER_REFRESH}" ${AFTER_CONSUMER} -1 REMAINDER)
    endforeach()
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
