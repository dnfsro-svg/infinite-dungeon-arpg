# Task 2 Review Fix Round 1 Package

## Range

- Previous reviewed head: `f280b3afa5d369962c63a3dd4e160a19a1311a4a`
- Fix head: `6853a7ab3fceb1b97f1813bd528083e16ac71ee8`
- Cumulative task range: `34eed403e459e490110f200fb2886d9a9c3bffdc..6853a7ab3fceb1b97f1813bd528083e16ac71ee8`

## Review findings to verify

1. Reject C++17 constant-false and uncalled-lambda decoys in real parsed code.
2. Prove all slot/fallback/icon/cooldown artwork precedes text-plan creation and key -> cooldown -> name drawing.
3. Treat the blue `水流` under the third slot as a separate world-prompt/HUD collision; Task 2 production empty-slot behavior remains key-only and is covered by pure tests. No clean screenshot is claimed.

## RED/GREEN evidence

- Old self-test RED: 113.95s; missed exactly `if constexpr(false)`, `auto&&` uncalled lambda, and complete text block moved before icon/cooldown artwork.
- Updated direct guard: 1/1 in 52.66s.
- Updated exact self-test: 1/1 in 108.15s.
- Production files were unchanged in this round.

## Full fix diff

```diff
diff --git a/tests/platform/hud_renderer_structured_model_source_self_test.cmake b/tests/platform/hud_renderer_structured_model_source_self_test.cmake
index 2f23502..7ec5099 100644
--- a/tests/platform/hud_renderer_structured_model_source_self_test.cmake
+++ b/tests/platform/hud_renderer_structured_model_source_self_test.cmake
@@ -240,15 +240,50 @@ replace_required("${ACTIVE_ORIGINAL}"
     "legacy_draw(text_plan.key);" ACTIVE_MUTATED
     "replace live key plate draw for dead-code decoy")
 set(ACTIVE_FALSE_DECOY
     "if (false) { draw_active_skill_text_plate(hud_font, text_plan.key); }\n        ")
 replace_required("${ACTIVE_MUTATED}" "legacy_draw(text_plan.key);"
     "${ACTIVE_FALSE_DECOY}legacy_draw(text_plan.key);" ACTIVE_MUTATED
     "insert constant-false active-skill key plate decoy")
 run_guard(active_skill_key_call_hidden_in_constant_false
     "${HUD_ORIGINAL}" "${ACTIVE_MUTATED}" FALSE)
 
+replace_required("${ACTIVE_ORIGINAL}"
+    "draw_active_skill_text_plate(hud_font, text_plan.key);"
+    "legacy_draw(text_plan.key);" ACTIVE_MUTATED
+    "replace live key plate draw for if constexpr decoy")
+set(ACTIVE_CONSTEXPR_FALSE_DECOY
+    "if constexpr (false) { draw_active_skill_text_plate(hud_font, text_plan.key); }\n        ")
+replace_required("${ACTIVE_MUTATED}" "legacy_draw(text_plan.key);"
+    "${ACTIVE_CONSTEXPR_FALSE_DECOY}legacy_draw(text_plan.key);"
+    ACTIVE_MUTATED "insert if constexpr false active-skill key plate decoy")
+run_guard(active_skill_key_call_hidden_in_if_constexpr_false
+    "${HUD_ORIGINAL}" "${ACTIVE_MUTATED}" FALSE)
+
+replace_required("${ACTIVE_ORIGINAL}"
+    "draw_active_skill_text_plate(hud_font, text_plan.key);"
+    "legacy_draw(text_plan.key);" ACTIVE_MUTATED
+    "replace live key plate draw for auto reference lambda decoy")
+set(ACTIVE_AUTO_REF_LAMBDA_DECOY
+    "auto&& hidden = [&]() { draw_active_skill_text_plate(hud_font, text_plan.key); };\n        ")
+replace_required("${ACTIVE_MUTATED}" "legacy_draw(text_plan.key);"
+    "${ACTIVE_AUTO_REF_LAMBDA_DECOY}legacy_draw(text_plan.key);"
+    ACTIVE_MUTATED "insert auto reference uncalled active-skill lambda decoy")
+run_guard(active_skill_key_call_hidden_in_auto_ref_lambda
+    "${HUD_ORIGINAL}" "${ACTIVE_MUTATED}" FALSE)
+
+set(TEXT_PLAN_DRAW_BLOCK
+    "        const ActiveSkillHudTextPlan text_plan = make_active_skill_hud_text_plan(\n            slot, layout.text_regions[index], layout.scale,\n            hud_font_ready && IsFontValid(hud_font),\n            &measure_active_skill_text, &measure_context);\n        draw_active_skill_text_plate(hud_font, text_plan.key);\n        draw_active_skill_text_plate(hud_font, text_plan.cooldown);\n        draw_active_skill_text_plate(hud_font, text_plan.name);\n")
+replace_required("${ACTIVE_ORIGINAL}" "${TEXT_PLAN_DRAW_BLOCK}" ""
+    ACTIVE_MUTATED "remove live text plan and plate draw block")
+replace_required("${ACTIVE_MUTATED}"
+    "        if (!slot.empty) {\n            const float inset ="
+    "${TEXT_PLAN_DRAW_BLOCK}        if (!slot.empty) {\n            const float inset ="
+    ACTIVE_MUTATED "move text plan and plates before icon and cooldown artwork")
+run_guard(active_skill_plan_before_icon_and_cooldown_artwork
+    "${HUD_ORIGINAL}" "${ACTIVE_MUTATED}" FALSE)
+
 if(MISSED_MUTATIONS)
     list(JOIN MISSED_MUTATIONS ", " MISSED_LABELS)
     message(FATAL_ERROR
         "HUD renderer guard missed mutations: ${MISSED_LABELS}")
 endif()
diff --git a/tests/platform/hud_renderer_structured_model_source_test.cmake b/tests/platform/hud_renderer_structured_model_source_test.cmake
index a74d478..a7bd8b8 100644
--- a/tests/platform/hud_renderer_structured_model_source_test.cmake
+++ b/tests/platform/hud_renderer_structured_model_source_test.cmake
@@ -21,38 +21,63 @@ function(count_substring INPUT NEEDLE OUTPUT)
             break()
         endif()
         math(EXPR COUNT "${COUNT} + 1")
         math(EXPR NEXT "${FOUND} + ${NEEDLE_LENGTH}")
         string(SUBSTRING "${REMAINDER}" ${NEXT} -1 REMAINDER)
     endwhile()
     set(${OUTPUT} ${COUNT} PARENT_SCOPE)
 endfunction()
 
 function(require_real_code BODY LABEL)
-    string(REGEX MATCH "if[ ]*\\([ ]*(false|0)[ ]*\\)"
+    string(REGEX MATCH "if([ ]+constexpr)?[ ]*\\([ ]*(false|0)[ ]*\\)"
         DEAD_BRANCH "${BODY}")
     if(DEAD_BRANCH)
         message(FATAL_ERROR "${LABEL} must not hide required calls in constant-false code")
     endif()
-    string(REGEX MATCH "auto[ ]+[A-Za-z_][A-Za-z0-9_]*[ ]*=[ ]*\\["
-        UNCALLED_LAMBDA "${BODY}")
-    if(UNCALLED_LAMBDA)
-        message(FATAL_ERROR "${LABEL} must not hide required calls in an uncalled lambda")
-    endif()
+endfunction()
+
+function(require_no_uncalled_lambda BODY LABEL)
+    set(REMAINDER "${BODY}")
+    while(TRUE)
+        string(REGEX MATCH
+            "(const[ ]+)?auto([ ]*&&|[ ]*&)?[ ]+([A-Za-z_][A-Za-z0-9_]*)[ ]*=[ ]*\\["
+            LAMBDA_DECLARATION "${REMAINDER}")
+        if(LAMBDA_DECLARATION STREQUAL "")
+            break()
+        endif()
+        set(LAMBDA_NAME "${CMAKE_MATCH_3}")
+        string(LENGTH "${LAMBDA_DECLARATION}" DECLARATION_LENGTH)
+        string(FIND "${REMAINDER}" "${LAMBDA_DECLARATION}"
+            DECLARATION_INDEX)
+        math(EXPR AFTER_DECLARATION
+            "${DECLARATION_INDEX} + ${DECLARATION_LENGTH}")
+        string(SUBSTRING "${REMAINDER}" ${AFTER_DECLARATION} -1
+            AFTER_DECLARATION_TEXT)
+        string(REGEX MATCH
+            "(${LAMBDA_NAME}[ ]*\\(|std::invoke[ ]*\\([ ]*${LAMBDA_NAME})"
+            LAMBDA_INVOKED "${AFTER_DECLARATION_TEXT}")
+        if(LAMBDA_INVOKED STREQUAL "")
+            message(FATAL_ERROR
+                "${LABEL} must not hide required calls in an uncalled lambda")
+        endif()
+        set(REMAINDER "${AFTER_DECLARATION_TEXT}")
+    endwhile()
 endfunction()
 
 file(READ "${HUD_RENDERER_SOURCE}" HUD_SOURCE)
 set(HUD_DRAW_MARKER
     "void HudRenderer::draw(const HudViewModel& view,\n    const HudLayout& layout, const MaterialPack& assets) const noexcept")
 extract_cpp_function_body("${HUD_SOURCE}"
     "${HUD_DRAW_MARKER}"
     HUD_DRAW_BODY HUD_DRAW_CODE)
+require_real_code("${HUD_DRAW_CODE}" "HudRenderer::draw")
+require_no_uncalled_lambda("${HUD_DRAW_CODE}" "HudRenderer::draw")
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
@@ -64,20 +89,21 @@ list(LENGTH BAR_LABEL_USES BAR_LABEL_USE_COUNT)
 if(NOT BAR_LABEL_USE_COUNT EQUAL 2)
     message(FATAL_ERROR
         "HudRenderer::draw must contain exactly the audit and draw label uses")
 endif()
 
 file(READ "${ACTIVE_SKILL_RENDERER_SOURCE}" ACTIVE_SOURCE)
 extract_cpp_function_body("${ACTIVE_SOURCE}"
     "void ActiveSkillRenderer::draw_hud(const ActiveSkillHudModel& model,"
     ACTIVE_DRAW_BODY ACTIVE_DRAW_CODE)
 require_real_code("${ACTIVE_DRAW_CODE}" "ActiveSkillRenderer::draw_hud")
+require_no_uncalled_lambda("${ACTIVE_DRAW_CODE}" "ActiveSkillRenderer::draw_hud")
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
@@ -97,24 +123,47 @@ set(REQUIRED_TEXT_PLAN
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
-string(FIND "${ACTIVE_DRAW_CODE}" "material_pack.draw_to(" ARTWORK_DRAW_INDEX)
-if(ARTWORK_DRAW_INDEX EQUAL -1 OR NOT ARTWORK_DRAW_INDEX LESS TEXT_PLAN_INDEX)
+set(LAST_ARTWORK_DRAW_INDEX -1)
+foreach(ARTWORK_DRAW_TOKEN IN ITEMS
+        "material_pack.draw_to( ui_material_sprite(slot_material), bounds);"
+        "DrawRectangleRounded("
+        "DrawRectangleRoundedLinesEx("
+        "DrawCircleLines("
+        "DrawLineEx({center.x"
+        "static_cast<void>(material_pack.draw_to(slot.icon, icon_bounds));"
+        "DrawRectangleRec(cooldown_overlay.bounds, Color{3, 7, 14, 190});"
+        "DrawLineEx({cooldown_overlay.bounds.x")
+    string(FIND "${ACTIVE_DRAW_CODE}" "${ARTWORK_DRAW_TOKEN}"
+        ARTWORK_DRAW_INDEX)
+    if(ARTWORK_DRAW_INDEX EQUAL -1)
+        message(FATAL_ERROR
+            "ActiveSkillRenderer::draw_hud must retain its ${ARTWORK_DRAW_TOKEN} artwork path")
+    endif()
+    if(NOT ARTWORK_DRAW_INDEX LESS TEXT_PLAN_INDEX)
+        message(FATAL_ERROR
+            "Active-skill artwork must finish before the text plan is made")
+    endif()
+    if(ARTWORK_DRAW_INDEX GREATER LAST_ARTWORK_DRAW_INDEX)
+        set(LAST_ARTWORK_DRAW_INDEX ${ARTWORK_DRAW_INDEX})
+    endif()
+endforeach()
+if(NOT LAST_ARTWORK_DRAW_INDEX LESS TEXT_PLAN_INDEX)
     message(FATAL_ERROR
-        "Active-skill slot artwork must be drawn before the text plan is made")
+        "The last active-skill artwork draw must precede the text plan")
 endif()
 
 set(LAST_TEXT_DRAW_INDEX ${TEXT_PLAN_INDEX})
 foreach(REQUIRED_PLATE_DRAW IN ITEMS
         "draw_active_skill_text_plate(hud_font, text_plan.key);"
         "draw_active_skill_text_plate(hud_font, text_plan.cooldown);"
         "draw_active_skill_text_plate(hud_font, text_plan.name);")
     count_substring("${ACTIVE_DRAW_BODY}" "${REQUIRED_PLATE_DRAW}"
         PLATE_DRAW_COUNT)
     if(NOT PLATE_DRAW_COUNT EQUAL 1)
@@ -126,20 +175,21 @@ foreach(REQUIRED_PLATE_DRAW IN ITEMS
         message(FATAL_ERROR
             "Active-skill key, cooldown, and name plans must draw in order")
     endif()
     set(LAST_TEXT_DRAW_INDEX ${PLATE_DRAW_INDEX})
 endforeach()
 
 extract_cpp_function_body("${ACTIVE_SOURCE}"
     "void draw_active_skill_text_plate(Font font,"
     ACTIVE_PLATE_BODY ACTIVE_PLATE_CODE)
 require_real_code("${ACTIVE_PLATE_CODE}" "Active-skill text plate helper")
+require_no_uncalled_lambda("${ACTIVE_PLATE_CODE}" "Active-skill text plate helper")
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
@@ -161,38 +211,41 @@ foreach(REQUIRED_PLATE_CALL IN ITEMS REQUIRED_PLATE_BACKING REQUIRED_PLATE_AUDIT
             "Active-skill text plate must draw backing, then audit, then text")
     endif()
     set(LAST_PLATE_INDEX ${PLATE_CALL_INDEX})
 endforeach()
 
 file(READ "${UI_TEXT_RENDERER_SOURCE}" TEXT_RENDERER_SOURCE)
 extract_cpp_function_body("${TEXT_RENDERER_SOURCE}"
     "void draw_crisp_ui_text(Font font, const char* text,"
     TEXT_RENDERER TEXT_RENDERER_CODE)
 require_real_code("${TEXT_RENDERER_CODE}" "draw_crisp_ui_text")
+require_no_uncalled_lambda("${TEXT_RENDERER_CODE}" "draw_crisp_ui_text")
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
+require_no_uncalled_lambda("${TEXT_AUDIT_MEASURED_CODE}" "UI text measured bounds")
+require_no_uncalled_lambda("${TEXT_AUDIT_RECORD_CODE}" "record_ui_text_bounds")
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
@@ -206,20 +259,21 @@ count_substring("${TEXT_AUDIT_RECORD}" "${REQUIRED_AUDIT_MEASURE_CALL}"
     AUDIT_MEASURE_CALL_COUNT)
 if(NOT AUDIT_MEASURE_CALL_COUNT EQUAL 1)
     message(FATAL_ERROR
         "record_ui_text_bounds must pass the explicit shadow to measured bounds")
 endif()
 
 extract_cpp_function_body("${ACTIVE_SOURCE}"
     "[[nodiscard]] UiTextExtent measure_active_skill_text(const char* text,"
     ACTIVE_MEASURE_BODY ACTIVE_MEASURE_CODE)
 require_real_code("${ACTIVE_MEASURE_CODE}" "Active-skill MeasureTextEx adapter")
+require_no_uncalled_lambda("${ACTIVE_MEASURE_CODE}" "Active-skill MeasureTextEx adapter")
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
@@ -249,26 +303,22 @@ foreach(DRAW_BODY IN ITEMS HUD_DRAW_BODY ACTIVE_DRAW_BODY)
             "\"seconds" "\"second" "%.0fs")
         string(FIND "${${DRAW_BODY}}" "${FORBIDDEN}" FOUND)
         if(NOT FOUND EQUAL -1)
             message(FATAL_ERROR
                 "HUD draw function must not recalculate model text: ${FORBIDDEN}")
         endif()
     endforeach()
 endforeach()
 
 foreach(DRAW_CODE IN ITEMS HUD_DRAW_CODE ACTIVE_DRAW_CODE)
-    string(REGEX MATCH "if[ ]*\\([ ]*(false|0)[ ]*\\)"
-        DEAD_BRANCH "${${DRAW_CODE}}")
-    if(DEAD_BRANCH)
-        message(FATAL_ERROR
-            "HUD draw function must not hide required calls in a compile-time dead branch")
-    endif()
+    require_real_code("${${DRAW_CODE}}" "HUD draw function")
+    require_no_uncalled_lambda("${${DRAW_CODE}}" "HUD draw function")
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
```

