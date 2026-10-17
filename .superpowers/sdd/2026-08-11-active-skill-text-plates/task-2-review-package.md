# Task 2 Review Package

## Range

- Base: `34eed403e459e490110f200fb2886d9a9c3bffdc`
- Head: `f280b3afa5d369962c63a3dd4e160a19a1311a4a`
- Commit: `f280b3a fix(ui): back active skill labels with contrast`

## Verification evidence

- Debug target build: exit 0.
- MENU/HUD selector: unique `183 cases, 0 failures`.
- Direct guard: 1/1, 53.73s.
- Guard self-test: 1/1, 98.14s.
- Release `arpg_game` target: exit 0.
- Isolated screenshots: `task2-ready-empty.png`, `task2-numpad2-retry.png`.
- No full/formal/Test.ps1/deployment.

## Full diff

```diff
diff --git a/src/platform/raylib/active_skill_renderer.cpp b/src/platform/raylib/active_skill_renderer.cpp
index 76be473..7cf51b0 100644
--- a/src/platform/raylib/active_skill_renderer.cpp
+++ b/src/platform/raylib/active_skill_renderer.cpp
@@ -1,28 +1,25 @@
 #include "active_skill_renderer.hpp"
 
 #include "ui_text_contrast.hpp"
 #include "ui_text_bounds_audit.hpp"
 #include "ui_text_renderer.hpp"
-#include "ui_typography.hpp"
 
 #include "combat/active_skill_runtime.hpp"
 #include "combat/room_bounds.hpp"
 #include "combat_view_math.hpp"
 #include "skills/active_skill_catalog.hpp"
 #include "ui_material.hpp"
 
 #include <algorithm>
 #include <array>
 #include <cmath>
-#include <cstdio>
-#include <string>
 
 namespace arpg::platform {
 namespace {
 
 constexpr float kPi = 3.14159265358979323846F;
 constexpr std::uint64_t kFlashTicks = 7U;
 constexpr std::uint16_t kDrawSlashImpactTick = 46U;
 constexpr std::uint16_t kStormFinisherTick = 324U;
 constexpr std::size_t kStormGroundSwordCount = 12U;
 
@@ -32,29 +29,46 @@ constexpr std::size_t kStormGroundSwordCount = 12U;
     std::uint64_t& age) noexcept {
     if (event == nullptr
         || event->skill != skills::ActiveSkillId::storm_swords
         || !event->finisher || snapshot.tick < event->tick) {
         return false;
     }
     age = snapshot.tick - event->tick;
     return age <= kFlashTicks;
 }
 
-void draw_skill_text(Font font, const char* text,
-    float x, float y, float size, Color color) noexcept {
-    if (text == nullptr || text[0] == '\0') return;
-    const UiTextContrastStyle style = ui_text_contrast_style();
-    color.a = 255U;
-    if (ui_luma_contrast_ratio(color, style.backing) < 4.5F) {
-        color = style.muted;
+struct ActiveSkillTextMeasureContext final {
+    Font font{};
+};
+
+[[nodiscard]] UiTextExtent measure_active_skill_text(const char* text,
+    float font_size, float spacing, void* context) noexcept {
+    const auto* const measure_context =
+        static_cast<const ActiveSkillTextMeasureContext*>(context);
+    if (text == nullptr || measure_context == nullptr
+            || !IsFontValid(measure_context->font)) {
+        return {};
     }
-    draw_crisp_ui_text(font, text, {x, y}, size, 0.5F, color);
+    const Vector2 extent = MeasureTextEx(measure_context->font, text,
+        font_size, spacing);
+    return {extent.x, extent.y};
+}
+
+void draw_active_skill_text_plate(Font font,
+    const ActiveSkillTextPlatePlan& plan) noexcept {
+    if (!plan.visible) return;
+    DrawRectangleRec(plan.backing, active_skill_text_plate_backing());
+    record_ui_text_bounds(UiTextAuditPage::hud, plan.role, font,
+        plan.text.data(), plan.position, plan.font_size, plan.spacing,
+        plan.backing, plan.font_size, nullptr, 0U, 0);
+    draw_crisp_ui_text(font, plan.text.data(), plan.position,
+        plan.font_size, plan.spacing, ui_text_contrast_style().primary, 0);
 }
 
 [[nodiscard]] bool draw_draw_slash(const DrawSlashVisualPlan& plan,
     const CombatCameraView& camera,
     float width, float height) noexcept {
     if (!plan.visible) return false;
     const ScreenProjection projected = project_combat_position(
         plan.center, camera, width, height);
     constexpr std::size_t kArcSegments = 14U;
     std::array<Vector2, kArcSegments + 2U> fan{};
@@ -76,42 +90,20 @@ void draw_skill_text(Font font, const char* text,
     DrawTriangleFan(fan.data(), static_cast<int>(fan.size()),
         Fade(Color{150, 224, 255, 255}, 0.24F * plan.opacity));
     for (std::size_t index = 1U; index + 1U < fan.size(); ++index) {
         DrawLineEx(fan[index], fan[index + 1U],
             3.0F * projected.scale,
             Fade(Color{236, 251, 255, 255}, plan.opacity));
     }
     return true;
 }
 
-void fit_skill_label(Font font, const char* source, char* output,
-    std::size_t capacity, float width, float size) noexcept {
-    if (output == nullptr || capacity == 0U) return;
-    static_cast<void>(std::snprintf(output, capacity, "%s",
-        source == nullptr ? "" : source));
-    output[capacity - 1U] = '\0';
-    if (MeasureTextEx(font, output, size, 0.5F).x <= width) return;
-    std::size_t length = std::char_traits<char>::length(output);
-    while (length > 0U) {
-        do {
-            --length;
-        } while (length > 0U
-            && (static_cast<unsigned char>(output[length]) & 0xC0U) == 0x80U);
-        output[length] = '\0';
-        if (length + 4U >= capacity) continue;
-        static_cast<void>(std::snprintf(output + length,
-            capacity - length, "..."));
-        if (MeasureTextEx(font, output, size, 0.5F).x <= width) return;
-        output[length] = '\0';
-    }
-}
-
 void draw_sword(Vector2 center, float angle, float scale,
     bool highlighted, float opacity) noexcept {
     const Vector2 direction{std::cos(angle), std::sin(angle)};
     const Vector2 normal{-direction.y, direction.x};
     const float length = (highlighted ? 54.0F : 42.0F) * scale;
     const float half_width = (highlighted ? 6.5F : 5.2F) * scale;
     const Vector2 tip{center.x + direction.x * length * 0.58F,
         center.y + direction.y * length * 0.58F};
     const Vector2 base{center.x - direction.x * length * 0.42F,
         center.y - direction.y * length * 0.42F};
@@ -370,21 +362,23 @@ ActiveSkillCooldownOverlayPlan make_active_skill_cooldown_overlay(
     const float ratio = std::clamp(cooldown_ratio, 0.0F, 1.0F);
     const float overlay_height = bounds.height * ratio;
     return {true, {bounds.x, bounds.y + bounds.height - overlay_height,
         bounds.width, overlay_height}};
 }
 
 void ActiveSkillRenderer::draw_hud(const ActiveSkillHudModel& model,
     const ActiveSkillHudLayout& layout,
     Font hud_font, bool hud_font_ready,
     const MaterialPack& material_pack) const noexcept {
-    const float scale = ui_viewport_scale(GetScreenWidth(), GetScreenHeight());
+    if (!layout.valid) return;
+    const float scale = layout.scale;
+    ActiveSkillTextMeasureContext measure_context{hud_font};
     for (std::size_t index = 0U; index < model.slots.size(); ++index) {
         const ActiveSkillHudSlot& slot = model.slots[index];
         const Rectangle bounds = layout.slots[index];
         const UiMaterialElement slot_material = slot.cooldown_ratio > 0.0F
             ? UiMaterialElement::hud_skill_cooldown
             : slot.empty ? UiMaterialElement::hud_skill_empty
                          : UiMaterialElement::hud_skill_ready;
         const bool material_drawn = material_pack.draw_to(
             ui_material_sprite(slot_material), bounds);
         if (!material_drawn) {
@@ -414,43 +408,21 @@ void ActiveSkillRenderer::draw_hud(const ActiveSkillHudModel& model,
             make_active_skill_cooldown_overlay(
                 bounds, slot.cooldown_ratio, slot.empty);
         if (cooldown_overlay.visible) {
             DrawRectangleRec(cooldown_overlay.bounds, Color{3, 7, 14, 190});
             DrawLineEx({cooldown_overlay.bounds.x,
                     cooldown_overlay.bounds.y},
                 {cooldown_overlay.bounds.x + cooldown_overlay.bounds.width,
                     cooldown_overlay.bounds.y},
                 1.5F, Color{112, 211, 255, 210});
         }
-        if (!hud_font_ready) continue;
-        draw_skill_text(hud_font, slot.key_label.data(),
-            bounds.x + 5.0F * scale,
-            bounds.y + 3.0F * scale, 16.0F * scale,
-            ui_text_contrast_style().primary);
-        if (!slot.empty) {
-            const float name_size =
-                ui_typography().kHudSkillNameFontSize * scale;
-            const Rectangle name_container{bounds.x + 3.0F * scale,
-                bounds.y + bounds.height - 24.0F * scale,
-                bounds.width - 6.0F * scale, 23.0F * scale};
-            char fitted_name[48]{};
-            fit_skill_label(hud_font, slot.name.data(), fitted_name,
-                sizeof(fitted_name),
-                name_container.width - 6.0F * scale, name_size);
-            const Vector2 name_position{name_container.x + 3.0F * scale,
-                name_container.y + 2.0F * scale};
-            record_ui_text_bounds(UiTextAuditPage::hud,
-                UiTextAuditRole::hud_skill_name, hud_font, fitted_name,
-                name_position, name_size, 0.5F, name_container, name_size);
-            draw_skill_text(hud_font, fitted_name, name_position.x,
-                name_position.y, name_size,
-                ui_text_contrast_style().primary);
-        }
-        if (slot.cooldown_ratio <= 0.0F || slot.empty) continue;
-        draw_skill_text(hud_font, slot.cooldown_label.data(),
-            bounds.x + 18.0F * scale,
-            bounds.y + 22.0F * scale, 16.0F * scale,
-            ui_text_contrast_style().primary);
+        const ActiveSkillHudTextPlan text_plan = make_active_skill_hud_text_plan(
+            slot, layout.text_regions[index], layout.scale,
+            hud_font_ready && IsFontValid(hud_font),
+            &measure_active_skill_text, &measure_context);
+        draw_active_skill_text_plate(hud_font, text_plan.key);
+        draw_active_skill_text_plate(hud_font, text_plan.cooldown);
+        draw_active_skill_text_plate(hud_font, text_plan.name);
     }
 }
 
 }  // namespace arpg::platform
diff --git a/src/platform/raylib/ui_text_bounds_audit.cpp b/src/platform/raylib/ui_text_bounds_audit.cpp
index 3d34573..6e8f48e 100644
--- a/src/platform/raylib/ui_text_bounds_audit.cpp
+++ b/src/platform/raylib/ui_text_bounds_audit.cpp
@@ -6,28 +6,30 @@
 #include <algorithm>
 #include <cmath>
 #include <limits>
 
 namespace arpg::platform {
 namespace {
 
 UiTextBoundsAuditStatus g_status{};
 
 [[nodiscard]] Rectangle measured_bounds(Font font, const char* text,
-    Vector2 position, float font_size, float spacing) noexcept {
+    Vector2 position, float font_size, float spacing,
+    int shadow_pixels) noexcept {
     position = snap_ui_text_position(position);
     const Vector2 measured = MeasureTextEx(font, text, font_size, spacing);
     const UiTextContrastStyle style = ui_text_contrast_style();
     const float left = static_cast<float>(style.outline_pixels);
     const float top = static_cast<float>(style.outline_pixels);
+    const int effective_shadow = shadow_pixels > 0 ? shadow_pixels : 0;
     const float right = static_cast<float>((std::max)(
-        style.outline_pixels, style.shadow_pixels));
+        style.outline_pixels, effective_shadow));
     const float bottom = right;
     return {position.x - left, position.y - top,
         measured.x + left + right, measured.y + top + bottom};
 }
 
 }  // namespace
 
 void reset_ui_text_bounds_audit() noexcept {
     g_status = {};
 }
@@ -49,39 +51,40 @@ bool ui_text_bounds_separated(Rectangle first, Rectangle second) noexcept {
     }
     return first.x + first.width <= second.x
         || second.x + second.width <= first.x
         || first.y + first.height <= second.y
         || second.y + second.height <= first.y;
 }
 
 void record_ui_text_bounds(UiTextAuditPage page, UiTextAuditRole role,
     Font font, const char* text, Vector2 position, float font_size,
     float spacing, Rectangle container, float minimum_font_size,
-    const Rectangle* blockers, std::size_t blocker_count) noexcept {
+    const Rectangle* blockers, std::size_t blocker_count,
+    int shadow_pixels) noexcept {
     const std::size_t page_index = static_cast<std::size_t>(page);
     const std::size_t role_index = static_cast<std::size_t>(role);
     if (page_index >= g_status.pages.size() || role_index >= 64U
             || text == nullptr || text[0] == '\0' || !IsFontValid(font)) {
         return;
     }
     UiTextAuditPageStatus& status = g_status.pages[page_index];
     status.observed_roles |= 1ULL << role_index;
     ++status.measured_text_count;
     status.minimum_display_font_size = status.measured_text_count == 1U
         ? font_size : (std::min)(status.minimum_display_font_size, font_size);
     const std::uint64_t role_bit = 1ULL << role_index;
     const bool size_readable = std::isfinite(font_size)
         && font_size >= minimum_font_size;
     status.sizes_readable = status.sizes_readable && size_readable;
     if (!size_readable) status.failed_size_roles |= role_bit;
     const Rectangle bounds = measured_bounds(
-        font, text, position, font_size, spacing);
+        font, text, position, font_size, spacing, shadow_pixels);
     const bool inside_container = ui_text_bounds_inside(bounds, container);
     bool bounds_safe = inside_container;
     for (std::size_t index{}; index < blocker_count; ++index) {
         bounds_safe = bounds_safe
             && ui_text_bounds_separated(bounds, blockers[index]);
     }
     if (!bounds_safe) {
         TraceLog(LOG_WARNING,
             "UI TEXT BOUNDS role=%u inside=%u bounds=%.1f,%.1f,%.1f,%.1f container=%.1f,%.1f,%.1f,%.1f blockers=%u",
             static_cast<unsigned>(role), inside_container ? 1U : 0U,
diff --git a/src/platform/raylib/ui_text_bounds_audit.hpp b/src/platform/raylib/ui_text_bounds_audit.hpp
index 16813d5..46190be 100644
--- a/src/platform/raylib/ui_text_bounds_audit.hpp
+++ b/src/platform/raylib/ui_text_bounds_audit.hpp
@@ -59,15 +59,16 @@ struct UiTextBoundsAuditStatus final {
 };
 
 void reset_ui_text_bounds_audit() noexcept;
 [[nodiscard]] bool ui_text_bounds_inside(
     Rectangle inner, Rectangle outer) noexcept;
 [[nodiscard]] bool ui_text_bounds_separated(
     Rectangle first, Rectangle second) noexcept;
 void record_ui_text_bounds(UiTextAuditPage page, UiTextAuditRole role,
     Font font, const char* text, Vector2 position, float font_size,
     float spacing, Rectangle container, float minimum_font_size,
-    const Rectangle* blockers = nullptr, std::size_t blocker_count = 0U)
+    const Rectangle* blockers = nullptr, std::size_t blocker_count = 0U,
+    int shadow_pixels = 2)
     noexcept;
 [[nodiscard]] UiTextBoundsAuditStatus ui_text_bounds_audit_status() noexcept;
 
 }  // namespace arpg::platform
diff --git a/src/platform/raylib/ui_text_renderer.cpp b/src/platform/raylib/ui_text_renderer.cpp
index f4c8ae2..01270b9 100644
--- a/src/platform/raylib/ui_text_renderer.cpp
+++ b/src/platform/raylib/ui_text_renderer.cpp
@@ -22,15 +22,17 @@ void draw_crisp_ui_text(Font font, const char* text, Vector2 position,
     if (ui_luma_contrast_ratio(color, style.backing) < 4.5F) {
         color = style.muted;
     }
     const Vector2 foreground = snap_ui_text_position(position);
     const float shadow_offset = static_cast<float>(
         shadow_pixels > 0 ? shadow_pixels : 0);
     const Vector2 shadow{
         foreground.x + shadow_offset,
         foreground.y + shadow_offset,
     };
-    DrawTextEx(font, text, shadow, font_size, spacing, style.shadow);
+    if (shadow_pixels > 0) {
+        DrawTextEx(font, text, shadow, font_size, spacing, style.shadow);
+    }
     DrawTextEx(font, text, foreground, font_size, spacing, color);
 }
 
 }  // namespace arpg::platform
diff --git a/tests/platform/CMakeLists.txt b/tests/platform/CMakeLists.txt
index 7198937..2f58cb1 100644
--- a/tests/platform/CMakeLists.txt
+++ b/tests/platform/CMakeLists.txt
@@ -2080,34 +2080,38 @@ add_test(
         -P "${CMAKE_CURRENT_SOURCE_DIR}/hud_notice_structured_binding_source_self_test.cmake")
 set_tests_properties(
     platform.hud_notice_structured_binding_source_self_test
     PROPERTIES LABELS "headless;platform;architecture;negative;full_chinese_ui")
 
 add_test(
     NAME platform.hud_renderer_structured_model_source
     COMMAND "${CMAKE_COMMAND}"
         "-DHUD_RENDERER_SOURCE=${PROJECT_SOURCE_DIR}/src/platform/raylib/hud_renderer.cpp"
         "-DACTIVE_SKILL_RENDERER_SOURCE=${PROJECT_SOURCE_DIR}/src/platform/raylib/active_skill_renderer.cpp"
+        "-DUI_TEXT_RENDERER_SOURCE=${PROJECT_SOURCE_DIR}/src/platform/raylib/ui_text_renderer.cpp"
+        "-DUI_TEXT_BOUNDS_AUDIT_SOURCE=${PROJECT_SOURCE_DIR}/src/platform/raylib/ui_text_bounds_audit.cpp"
         "-DDUNGEON_RUNTIME_SOURCE=${PROJECT_SOURCE_DIR}/tests/platform/dungeon_runtime_tests.cpp"
         "-DLOOT_SUCTION_SOURCE=${PROJECT_SOURCE_DIR}/tests/platform/loot_suction_animation_tests.cpp"
         "-DSTAGE12_SOURCE=${PROJECT_SOURCE_DIR}/tests/platform/stage12_material_formal_game_validation.cpp"
         "-DSTAGE16_SOURCE=${PROJECT_SOURCE_DIR}/tests/platform/stage16_loot_reinforcement_game_validation.cpp"
         -P "${CMAKE_CURRENT_SOURCE_DIR}/hud_renderer_structured_model_source_test.cmake")
 set_tests_properties(
     platform.hud_renderer_structured_model_source
     PROPERTIES LABELS "headless;platform;architecture;full_chinese_ui")
 
 add_test(
     NAME platform.hud_renderer_structured_model_source_self_test
     COMMAND "${CMAKE_COMMAND}"
         "-DHUD_RENDERER_SOURCE=${PROJECT_SOURCE_DIR}/src/platform/raylib/hud_renderer.cpp"
         "-DACTIVE_SKILL_RENDERER_SOURCE=${PROJECT_SOURCE_DIR}/src/platform/raylib/active_skill_renderer.cpp"
+        "-DUI_TEXT_RENDERER_SOURCE=${PROJECT_SOURCE_DIR}/src/platform/raylib/ui_text_renderer.cpp"
+        "-DUI_TEXT_BOUNDS_AUDIT_SOURCE=${PROJECT_SOURCE_DIR}/src/platform/raylib/ui_text_bounds_audit.cpp"
         "-DGUARD_SCRIPT=${CMAKE_CURRENT_SOURCE_DIR}/hud_renderer_structured_model_source_test.cmake"
         "-DMUTATION_ROOT=${CMAKE_CURRENT_BINARY_DIR}/hud-renderer-structured-model-mutations"
         -P "${CMAKE_CURRENT_SOURCE_DIR}/hud_renderer_structured_model_source_self_test.cmake")
 set_tests_properties(
     platform.hud_renderer_structured_model_source_self_test
     PROPERTIES LABELS "headless;platform;architecture;negative;full_chinese_ui")
 
 add_test(
     NAME platform.passive_tree_binding_label_source
     COMMAND "${CMAKE_COMMAND}"
diff --git a/tests/platform/hud_renderer_structured_model_source_self_test.cmake b/tests/platform/hud_renderer_structured_model_source_self_test.cmake
index 365900e..2f23502 100644
--- a/tests/platform/hud_renderer_structured_model_source_self_test.cmake
+++ b/tests/platform/hud_renderer_structured_model_source_self_test.cmake
@@ -1,48 +1,63 @@
 cmake_minimum_required(VERSION 3.25)
 
 foreach(REQUIRED_INPUT IN ITEMS HUD_RENDERER_SOURCE ACTIVE_SKILL_RENDERER_SOURCE
+        UI_TEXT_RENDERER_SOURCE UI_TEXT_BOUNDS_AUDIT_SOURCE
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
+file(READ "${UI_TEXT_RENDERER_SOURCE}" TEXT_RENDERER_ORIGINAL)
+file(READ "${UI_TEXT_BOUNDS_AUDIT_SOURCE}" TEXT_AUDIT_ORIGINAL)
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
+    set(TEXT_RENDERER_MUTANT "${ROOT}/ui_text_renderer.cpp")
+    set(TEXT_AUDIT_MUTANT "${ROOT}/ui_text_bounds_audit.cpp")
     set(DUNGEON_FIXTURE "${ROOT}/dungeon.cpp")
     set(LOOT_FIXTURE "${ROOT}/loot.cpp")
     set(STAGE12_FIXTURE "${ROOT}/stage12.cpp")
     set(STAGE16_FIXTURE "${ROOT}/stage16.cpp")
     file(WRITE "${HUD_MUTANT}" "${HUD_TEXT}")
     file(WRITE "${ACTIVE_MUTANT}" "${ACTIVE_TEXT}")
+    set(TEXT_RENDERER_TEXT "${TEXT_RENDERER_ORIGINAL}")
+    set(TEXT_AUDIT_TEXT "${TEXT_AUDIT_ORIGINAL}")
+    if(ARGC GREATER 4)
+        set(TEXT_RENDERER_TEXT "${ARGV4}")
+    endif()
+    if(ARGC GREATER 5)
+        set(TEXT_AUDIT_TEXT "${ARGV5}")
+    endif()
+    file(WRITE "${TEXT_RENDERER_MUTANT}" "${TEXT_RENDERER_TEXT}")
+    file(WRITE "${TEXT_AUDIT_MUTANT}" "${TEXT_AUDIT_TEXT}")
     set(CURRENT_DUNGEON_FIXTURE_TEXT "${DUNGEON_FIXTURE_TEXT}")
     set(CURRENT_LOOT_FIXTURE_TEXT "${LOOT_FIXTURE_TEXT}")
     set(CURRENT_STAGE12_FIXTURE_TEXT "${STAGE12_FIXTURE_TEXT}")
     set(CURRENT_STAGE16_FIXTURE_TEXT "${STAGE16_FIXTURE_TEXT}")
     if(DEFINED FIXTURE_OVERRIDE)
         set(CURRENT_DUNGEON_FIXTURE_TEXT "${FIXTURE_OVERRIDE}")
         set(CURRENT_LOOT_FIXTURE_TEXT "${FIXTURE_OVERRIDE}")
         set(CURRENT_STAGE12_FIXTURE_TEXT "${FIXTURE_OVERRIDE}")
     endif()
     if(DEFINED DUNGEON_FIXTURE_OVERRIDE)
@@ -58,60 +73,63 @@ function(run_guard LABEL HUD_TEXT ACTIVE_TEXT EXPECT_SUCCESS)
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
+            "-DUI_TEXT_RENDERER_SOURCE=${TEXT_RENDERER_MUTANT}"
+            "-DUI_TEXT_BOUNDS_AUDIT_SOURCE=${TEXT_AUDIT_MUTANT}"
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
 
+function(replace_required INPUT MATCH REPLACEMENT OUTPUT LABEL)
+    string(FIND "${INPUT}" "${MATCH}" MATCH_INDEX)
+    if(MATCH_INDEX EQUAL -1)
+        message(FATAL_ERROR "Self-test mutation did not match: ${LABEL}")
+    endif()
+    string(REPLACE "${MATCH}" "${REPLACEMENT}" RESULT "${INPUT}")
+    if(RESULT STREQUAL INPUT)
+        message(FATAL_ERROR "Self-test mutation did not change source: ${LABEL}")
+    endif()
+    set(${OUTPUT} "${RESULT}" PARENT_SCOPE)
+endfunction()
+
 run_guard(clean "${HUD_ORIGINAL}" "${ACTIVE_ORIGINAL}" TRUE)
 
-string(REPLACE "bar.label.bytes.data()" "legacy_bar_label(bar)"
-    HUD_MUTATED "${HUD_ORIGINAL}")
+replace_required("${HUD_ORIGINAL}" "bar.label.bytes.data()"
+    "legacy_bar_label(bar)" HUD_MUTATED "legacy HUD label replacement")
 set(HUD_COMPLETE_DECOY
     "if (false) {\n                record_ui_text_bounds(UiTextAuditPage::hud, role, draw_font,\n                    bar.label.bytes.data(), label_position,\n                    style.player_bar_font_size * layout.scale,\n                    1.0F, label_container,\n                    style.player_bar_font_size * layout.scale, &blocker, 1U);\n                draw_hud_text(draw_font, bar.label.bytes.data(), label_position,\n                    style.player_bar_font_size * layout.scale,\n                    palette.text, style.outline_pixels);\n            }\n            ")
-string(REPLACE
+replace_required("${HUD_MUTATED}"
     "record_ui_text_bounds(UiTextAuditPage::hud, role, draw_font,"
     "${HUD_COMPLETE_DECOY}record_ui_text_bounds(UiTextAuditPage::hud, role, draw_font,"
-    HUD_MUTATED "${HUD_MUTATED}")
+    HUD_MUTATED "HUD dead-branch insertion")
 run_guard(hud_complete_dead_calls_with_live_legacy_path
     "${HUD_MUTATED}" "${ACTIVE_ORIGINAL}" FALSE)
 
-string(REPLACE "slot.key_label.data()" "legacy_key_label(slot)"
-    ACTIVE_MUTATED "${ACTIVE_ORIGINAL}")
-string(REPLACE "slot.cooldown_label.data()" "legacy_cooldown_label(slot)"
-    ACTIVE_MUTATED "${ACTIVE_MUTATED}")
-set(ACTIVE_COMPLETE_DECOY
-    "if (false) {\n            if (!hud_font_ready) continue;\n            draw_skill_text(hud_font, slot.key_label.data(),\n                bounds.x + 5.0F * scale,\n                bounds.y + 3.0F * scale, 16.0F * scale,\n                ui_text_contrast_style().primary);\n            if (!slot.empty) { }\n            if (slot.cooldown_ratio <= 0.0F || slot.empty) continue;\n            draw_skill_text(hud_font, slot.cooldown_label.data(),\n                bounds.x + 18.0F * scale,\n                bounds.y + 22.0F * scale, 16.0F * scale,\n                ui_text_contrast_style().primary);\n        }\n        ")
-string(REPLACE "if (!hud_font_ready) continue;"
-    "${ACTIVE_COMPLETE_DECOY}if (!hud_font_ready) continue;"
-    ACTIVE_MUTATED "${ACTIVE_MUTATED}")
-run_guard(active_complete_dead_calls_with_live_legacy_path
-    "${HUD_ORIGINAL}" "${ACTIVE_MUTATED}" FALSE)
 
 macro(run_fixture_reset_mutant LABEL RESET_HINTS RESET_CONTROL_HINTS)
     set(DUNGEON_FIXTURE_OVERRIDE
         "void fixture(){ platform::ControlHints hints{}; platform::refresh_control_hints(hints, arpg::settings::default_settings()); ${RESET_HINTS} renderer.observe_hud(previous, current, status, hints, 0.0F, false); }")
     set(LOOT_FIXTURE_OVERRIDE
         "platform::ControlHints control_hints(){ platform::ControlHints hints{}; platform::refresh_control_hints(hints, arpg::settings::default_settings()); ${RESET_HINTS} return hints; } void fixture(){ renderer.observe_hud(previous, current, status, control_hints(), 0.0F, false); }")
     set(STAGE12_FIXTURE_OVERRIDE
         "void fixture(){ platform::ControlHints control_hints{}; platform::refresh_control_hints(control_hints, arpg::settings::default_settings()); ${RESET_CONTROL_HINTS} renderer.observe_hud(previous, current, status, control_hints, 0.0F, false); }")
     set(STAGE16_FIXTURE_OVERRIDE
         "${DUNGEON_FIXTURE_OVERRIDE}${DUNGEON_FIXTURE_OVERRIDE}")
@@ -124,81 +142,113 @@ endmacro()
 
 run_fixture_reset_mutant(refresh_then_brace_assignment
     "hints = {};" "control_hints = {};")
 run_fixture_reset_mutant(refresh_then_typed_reassignment
     "hints = platform::ControlHints{};"
     "control_hints = platform::ControlHints{};")
 run_fixture_reset_mutant(refresh_then_memset_clear
     "std::memset(&hints, 0, sizeof(hints));"
     "std::memset(&control_hints, 0, sizeof(control_hints));")
 
-string(REPLACE "bar.label.bytes.data()" "u8\"生命\""
-    HUD_MUTATED "${HUD_ORIGINAL}")
+replace_required("${HUD_ORIGINAL}" "bar.label.bytes.data()" "u8\"生命\""
+    HUD_MUTATED "HUD hardcoded label replacement")
 set(HUD_DECOY
     "if (false) {\n        static_cast<void>(bar.label.bytes.data());\n        static_cast<void>(bar.label.bytes.data());\n    }\n    ")
-string(REPLACE
+replace_required("${HUD_MUTATED}"
     "const HudLayout& layout, const MaterialPack& assets) const noexcept {\n    if (!IsWindowReady()) return;"
     "const HudLayout& layout, const MaterialPack& assets) const noexcept {\n    ${HUD_DECOY}if (!IsWindowReady()) return;"
-    HUD_MUTATED "${HUD_MUTATED}")
+    HUD_MUTATED "HUD dead-code decoy insertion")
 run_guard(hud_dead_correct_decoy_with_real_hardcode
     "${HUD_MUTATED}" "${ACTIVE_ORIGINAL}" FALSE)
 
-string(REPLACE "slot.key_label.data()" "legacy_key_label(slot)"
-    ACTIVE_MUTATED "${ACTIVE_ORIGINAL}")
-string(REPLACE "slot.cooldown_label.data()" "legacy_cooldown_label(slot)"
-    ACTIVE_MUTATED "${ACTIVE_MUTATED}")
-set(ACTIVE_DECOY
-    "if (false) {\n        draw_skill_text(hud_font, slot.key_label.data(), 0.0F, 0.0F, 1.0F, WHITE);\n        draw_skill_text(hud_font, slot.cooldown_label.data(), 0.0F, 0.0F, 1.0F, WHITE);\n    }\n    ")
-string(REPLACE
-    "const MaterialPack& material_pack) const noexcept {\n    const float scale"
-    "const MaterialPack& material_pack) const noexcept {\n    ${ACTIVE_DECOY}const float scale"
-    ACTIVE_MUTATED "${ACTIVE_MUTATED}")
-run_guard(active_dead_correct_decoy_with_real_legacy_text
-    "${HUD_ORIGINAL}" "${ACTIVE_MUTATED}" FALSE)
 
 set(FIXTURE_OVERRIDE
     "void fixture(){ platform::ControlHints initialized{}; platform::refresh_control_hints(initialized, arpg::settings::default_settings()); platform::ControlHints hints{}; renderer.observe_hud(previous, current, status, hints, 0.0F, false); }\n")
 set(STAGE16_FIXTURE_OVERRIDE "${FIXTURE_OVERRIDE}${FIXTURE_OVERRIDE}")
 run_guard(refreshes_unused_hints_but_passes_uninitialized
     "${HUD_ORIGINAL}" "${ACTIVE_ORIGINAL}" FALSE)
 unset(FIXTURE_OVERRIDE)
 unset(STAGE16_FIXTURE_OVERRIDE)
 
-string(REPLACE "bar.label.bytes.data()" "u8\"生命\""
-    HUD_MUTATED "${HUD_ORIGINAL}")
+replace_required("${HUD_ORIGINAL}" "bar.label.bytes.data()" "u8\"生命\""
+    HUD_MUTATED "HUD ignored-label replacement")
 set(HUD_MUTATED
     "${HUD_MUTATED}\nvoid decoy_bar_label_use(){ bar.label.bytes.data(); }\n")
 run_guard(hud_ignores_bar_label "${HUD_MUTATED}" "${ACTIVE_ORIGINAL}" FALSE)
 
-string(REPLACE "slot.key_label.data()" "\"Num1\""
-    ACTIVE_MUTATED "${ACTIVE_ORIGINAL}")
-set(ACTIVE_MUTATED
-    "${ACTIVE_MUTATED}\nvoid decoy_key_label_use(){ slot.key_label.data(); }\n")
-run_guard(active_hardcodes_num_keycap
+
+replace_required("${ACTIVE_ORIGINAL}" "if (!layout.valid) return;" ""
+    ACTIVE_MUTATED "remove early invalid-layout guard")
+replace_required("${ACTIVE_MUTATED}" "if (!material_drawn) {"
+    "if (!layout.valid) return;\n        if (!material_drawn) {"
+    ACTIVE_MUTATED "move invalid-layout guard after material draw")
+run_guard(invalid_layout_guard_moved_after_first_material_draw
+    "${HUD_ORIGINAL}" "${ACTIVE_MUTATED}" FALSE)
+
+foreach(PLATE_NAME IN ITEMS key cooldown name)
+    replace_required("${ACTIVE_ORIGINAL}"
+        "draw_active_skill_text_plate(hud_font, text_plan.${PLATE_NAME});"
+        "static_cast<void>(text_plan.${PLATE_NAME});"
+        ACTIVE_MUTATED "remove active-skill ${PLATE_NAME} plate draw")
+    run_guard(active_skill_${PLATE_NAME}_plate_removed
+        "${HUD_ORIGINAL}" "${ACTIVE_MUTATED}" FALSE)
+endforeach()
+
+replace_required("${ACTIVE_ORIGINAL}" "plan.position" "Vector2{0.0F, 0.0F}"
+    ACTIVE_MUTATED "replace plan position with fixed offset")
+run_guard(active_skill_plate_uses_fixed_text_offset
+    "${HUD_ORIGINAL}" "${ACTIVE_MUTATED}" FALSE)
+
+replace_required("${ACTIVE_ORIGINAL}" "nullptr, 0U, 0);" "nullptr, 0U, 2);"
+    ACTIVE_MUTATED "replace audit zero shadow")
+run_guard(active_skill_plate_audit_uses_shadow
     "${HUD_ORIGINAL}" "${ACTIVE_MUTATED}" FALSE)
 
-string(REPLACE "slot.key_label.data()" "legacy_key_label(slot)"
-    ACTIVE_MUTATED "${ACTIVE_ORIGINAL}")
-set(ACTIVE_MUTATED
-    "${ACTIVE_MUTATED}\nvoid decoy_key_label_use(){ slot.key_label.data(); }\n")
-run_guard(active_ignores_key_label
+replace_required("${ACTIVE_ORIGINAL}" "ui_text_contrast_style().primary, 0);"
+    "ui_text_contrast_style().primary, 2);" ACTIVE_MUTATED
+    "replace draw zero shadow")
+run_guard(active_skill_plate_draw_uses_shadow
     "${HUD_ORIGINAL}" "${ACTIVE_MUTATED}" FALSE)
 
-string(REPLACE "slot.cooldown_label.data()" "\"2 seconds\""
-    ACTIVE_MUTATED "${ACTIVE_ORIGINAL}")
-set(ACTIVE_MUTATED
-    "${ACTIVE_MUTATED}\nvoid decoy_cooldown_use(){ slot.cooldown_label.data(); }\n")
-run_guard(active_hardcodes_seconds
+replace_required("${TEXT_RENDERER_ORIGINAL}" "if (shadow_pixels > 0) {"
+    "if (true) {" TEXT_RENDERER_MUTATED "make shadow draw unconditional")
+run_guard(unconditional_crisp_text_shadow
+    "${HUD_ORIGINAL}" "${ACTIVE_ORIGINAL}" FALSE
+    "${TEXT_RENDERER_MUTATED}")
+
+replace_required("${TEXT_AUDIT_ORIGINAL}"
+    "style.outline_pixels, effective_shadow"
+    "style.outline_pixels, style.shadow_pixels" TEXT_AUDIT_MUTATED
+    "ignore explicit audit shadow")
+run_guard(audit_ignores_explicit_shadow
+    "${HUD_ORIGINAL}" "${ACTIVE_ORIGINAL}" FALSE
+    "${TEXT_RENDERER_ORIGINAL}" "${TEXT_AUDIT_MUTATED}")
+
+replace_required("${ACTIVE_ORIGINAL}"
+    "draw_active_skill_text_plate(hud_font, text_plan.key);"
+    "legacy_draw(text_plan.key);" ACTIVE_MUTATED
+    "replace live active-skill key plate draw")
+set(ACTIVE_LAMBDA_DECOY
+    "auto hidden = [&]() { draw_active_skill_text_plate(hud_font, text_plan.key); };\n        ")
+replace_required("${ACTIVE_MUTATED}" "legacy_draw(text_plan.key);"
+    "${ACTIVE_LAMBDA_DECOY}legacy_draw(text_plan.key);"
+    ACTIVE_MUTATED "insert uncalled active-skill lambda decoy")
+run_guard(active_skill_calls_hidden_in_uncalled_lambda
     "${HUD_ORIGINAL}" "${ACTIVE_MUTATED}" FALSE)
 
-string(REPLACE "slot.cooldown_label.data()" "legacy_cooldown_label(slot)"
-    ACTIVE_MUTATED "${ACTIVE_ORIGINAL}")
-set(ACTIVE_MUTATED
-    "${ACTIVE_MUTATED}\nvoid decoy_cooldown_use(){ slot.cooldown_label.data(); }\n")
-run_guard(active_ignores_cooldown_label
+replace_required("${ACTIVE_ORIGINAL}"
+    "draw_active_skill_text_plate(hud_font, text_plan.key);"
+    "legacy_draw(text_plan.key);" ACTIVE_MUTATED
+    "replace live key plate draw for dead-code decoy")
+set(ACTIVE_FALSE_DECOY
+    "if (false) { draw_active_skill_text_plate(hud_font, text_plan.key); }\n        ")
+replace_required("${ACTIVE_MUTATED}" "legacy_draw(text_plan.key);"
+    "${ACTIVE_FALSE_DECOY}legacy_draw(text_plan.key);" ACTIVE_MUTATED
+    "insert constant-false active-skill key plate decoy")
+run_guard(active_skill_key_call_hidden_in_constant_false
     "${HUD_ORIGINAL}" "${ACTIVE_MUTATED}" FALSE)
 
 if(MISSED_MUTATIONS)
     list(JOIN MISSED_MUTATIONS ", " MISSED_LABELS)
     message(FATAL_ERROR
         "HUD renderer guard missed mutations: ${MISSED_LABELS}")
 endif()
diff --git a/tests/platform/hud_renderer_structured_model_source_test.cmake b/tests/platform/hud_renderer_structured_model_source_test.cmake
index 8f4cfff..a74d478 100644
--- a/tests/platform/hud_renderer_structured_model_source_test.cmake
+++ b/tests/platform/hud_renderer_structured_model_source_test.cmake
@@ -1,13 +1,14 @@
 cmake_minimum_required(VERSION 3.25)
 
 foreach(REQUIRED_INPUT IN ITEMS HUD_RENDERER_SOURCE ACTIVE_SKILL_RENDERER_SOURCE
+        UI_TEXT_RENDERER_SOURCE UI_TEXT_BOUNDS_AUDIT_SOURCE
         DUNGEON_RUNTIME_SOURCE LOOT_SUCTION_SOURCE STAGE12_SOURCE STAGE16_SOURCE)
     if(NOT DEFINED ${REQUIRED_INPUT} OR NOT EXISTS "${${REQUIRED_INPUT}}")
         message(FATAL_ERROR
             "HUD renderer structured-model source input missing: ${REQUIRED_INPUT}")
     endif()
 endforeach()
 
 include("${CMAKE_CURRENT_LIST_DIR}/cpp_source_function_body.cmake")
 
 function(count_substring INPUT NEEDLE OUTPUT)
@@ -19,20 +20,33 @@ function(count_substring INPUT NEEDLE OUTPUT)
         if(FOUND EQUAL -1)
             break()
         endif()
         math(EXPR COUNT "${COUNT} + 1")
         math(EXPR NEXT "${FOUND} + ${NEEDLE_LENGTH}")
         string(SUBSTRING "${REMAINDER}" ${NEXT} -1 REMAINDER)
     endwhile()
     set(${OUTPUT} ${COUNT} PARENT_SCOPE)
 endfunction()
 
+function(require_real_code BODY LABEL)
+    string(REGEX MATCH "if[ ]*\\([ ]*(false|0)[ ]*\\)"
+        DEAD_BRANCH "${BODY}")
+    if(DEAD_BRANCH)
+        message(FATAL_ERROR "${LABEL} must not hide required calls in constant-false code")
+    endif()
+    string(REGEX MATCH "auto[ ]+[A-Za-z_][A-Za-z0-9_]*[ ]*=[ ]*\\["
+        UNCALLED_LAMBDA "${BODY}")
+    if(UNCALLED_LAMBDA)
+        message(FATAL_ERROR "${LABEL} must not hide required calls in an uncalled lambda")
+    endif()
+endfunction()
+
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
@@ -49,41 +63,190 @@ string(REGEX MATCHALL "bar\\.label\\.bytes\\.data\\(\\)" BAR_LABEL_USES
 list(LENGTH BAR_LABEL_USES BAR_LABEL_USE_COUNT)
 if(NOT BAR_LABEL_USE_COUNT EQUAL 2)
     message(FATAL_ERROR
         "HudRenderer::draw must contain exactly the audit and draw label uses")
 endif()
 
 file(READ "${ACTIVE_SKILL_RENDERER_SOURCE}" ACTIVE_SOURCE)
 extract_cpp_function_body("${ACTIVE_SOURCE}"
     "void ActiveSkillRenderer::draw_hud(const ActiveSkillHudModel& model,"
     ACTIVE_DRAW_BODY ACTIVE_DRAW_CODE)
-set(REQUIRED_KEY_DRAW
-    "if (!hud_font_ready) continue; draw_skill_text(hud_font, slot.key_label.data(), bounds.x + 5.0F * scale, bounds.y + 3.0F * scale, 16.0F * scale, ui_text_contrast_style().primary); if (!slot.empty) {")
-set(REQUIRED_COOLDOWN_DRAW
-    "if (slot.cooldown_ratio <= 0.0F || slot.empty) continue; draw_skill_text(hud_font, slot.cooldown_label.data(), bounds.x + 18.0F * scale, bounds.y + 22.0F * scale, 16.0F * scale, ui_text_contrast_style().primary);")
-foreach(REQUIRED_CALL IN ITEMS REQUIRED_KEY_DRAW REQUIRED_COOLDOWN_DRAW)
-    count_substring("${ACTIVE_DRAW_BODY}" "${${REQUIRED_CALL}}"
-        REQUIRED_CALL_COUNT)
-    if(NOT REQUIRED_CALL_COUNT EQUAL 1)
+require_real_code("${ACTIVE_DRAW_CODE}" "ActiveSkillRenderer::draw_hud")
+set(REQUIRED_INVALID_LAYOUT_GUARD "if (!layout.valid) return;")
+string(FIND "${ACTIVE_DRAW_CODE}" "${REQUIRED_INVALID_LAYOUT_GUARD}"
+    INVALID_LAYOUT_GUARD_INDEX)
+if(INVALID_LAYOUT_GUARD_INDEX EQUAL -1)
+    message(FATAL_ERROR
+        "ActiveSkillRenderer::draw_hud must reject invalid layout before drawing")
+endif()
+if(NOT INVALID_LAYOUT_GUARD_INDEX EQUAL 0)
+    message(FATAL_ERROR
+        "ActiveSkillRenderer::draw_hud must begin with the invalid-layout guard")
+endif()
+foreach(FIRST_DRAW_TOKEN IN ITEMS
+        "material_pack.draw_to(" "DrawRectangleRounded(" "DrawRectangleRec(")
+    string(FIND "${ACTIVE_DRAW_CODE}" "${FIRST_DRAW_TOKEN}" FIRST_DRAW_INDEX)
+    if(NOT FIRST_DRAW_INDEX EQUAL -1
+            AND NOT INVALID_LAYOUT_GUARD_INDEX LESS FIRST_DRAW_INDEX)
+        message(FATAL_ERROR
+            "ActiveSkillRenderer::draw_hud must reject invalid layout before its first draw")
+    endif()
+endforeach()
+
+set(REQUIRED_TEXT_PLAN
+    "const ActiveSkillHudTextPlan text_plan = make_active_skill_hud_text_plan( slot, layout.text_regions[index], layout.scale, hud_font_ready && IsFontValid(hud_font), &measure_active_skill_text, &measure_context);")
+count_substring("${ACTIVE_DRAW_BODY}" "${REQUIRED_TEXT_PLAN}" TEXT_PLAN_COUNT)
+if(NOT TEXT_PLAN_COUNT EQUAL 1)
+    message(FATAL_ERROR
+        "ActiveSkillRenderer::draw_hud must build one shared-font active-skill text plan")
+endif()
+string(FIND "${ACTIVE_DRAW_CODE}" "${REQUIRED_TEXT_PLAN}" TEXT_PLAN_INDEX)
+if(NOT INVALID_LAYOUT_GUARD_INDEX LESS TEXT_PLAN_INDEX)
+    message(FATAL_ERROR
+        "ActiveSkillRenderer::draw_hud must reject invalid layout before making a text plan")
+endif()
+string(FIND "${ACTIVE_DRAW_CODE}" "material_pack.draw_to(" ARTWORK_DRAW_INDEX)
+if(ARTWORK_DRAW_INDEX EQUAL -1 OR NOT ARTWORK_DRAW_INDEX LESS TEXT_PLAN_INDEX)
+    message(FATAL_ERROR
+        "Active-skill slot artwork must be drawn before the text plan is made")
+endif()
+
+set(LAST_TEXT_DRAW_INDEX ${TEXT_PLAN_INDEX})
+foreach(REQUIRED_PLATE_DRAW IN ITEMS
+        "draw_active_skill_text_plate(hud_font, text_plan.key);"
+        "draw_active_skill_text_plate(hud_font, text_plan.cooldown);"
+        "draw_active_skill_text_plate(hud_font, text_plan.name);")
+    count_substring("${ACTIVE_DRAW_BODY}" "${REQUIRED_PLATE_DRAW}"
+        PLATE_DRAW_COUNT)
+    if(NOT PLATE_DRAW_COUNT EQUAL 1)
+        message(FATAL_ERROR
+            "ActiveSkillRenderer::draw_hud must draw each real active-skill text plan")
+    endif()
+    string(FIND "${ACTIVE_DRAW_CODE}" "${REQUIRED_PLATE_DRAW}" PLATE_DRAW_INDEX)
+    if(NOT PLATE_DRAW_INDEX GREATER LAST_TEXT_DRAW_INDEX)
         message(FATAL_ERROR
-            "ActiveSkillRenderer::draw_hud must contain exactly one required model draw call")
+            "Active-skill key, cooldown, and name plans must draw in order")
     endif()
+    set(LAST_TEXT_DRAW_INDEX ${PLATE_DRAW_INDEX})
 endforeach()
-foreach(MODEL_TOKEN IN ITEMS
-        "slot.key_label.data()" "slot.cooldown_label.data()")
-    string(REGEX REPLACE "([][+.*()^$?\\\\|])" "\\\\\\1"
-        TOKEN_REGEX "${MODEL_TOKEN}")
-    string(REGEX MATCHALL "${TOKEN_REGEX}" MODEL_USES "${ACTIVE_DRAW_BODY}")
-    list(LENGTH MODEL_USES MODEL_USE_COUNT)
-    if(NOT MODEL_USE_COUNT EQUAL 1)
+
+extract_cpp_function_body("${ACTIVE_SOURCE}"
+    "void draw_active_skill_text_plate(Font font,"
+    ACTIVE_PLATE_BODY ACTIVE_PLATE_CODE)
+require_real_code("${ACTIVE_PLATE_CODE}" "Active-skill text plate helper")
+set(REQUIRED_PLATE_EARLY_RETURN "if (!plan.visible) return;")
+string(FIND "${ACTIVE_PLATE_CODE}" "${REQUIRED_PLATE_EARLY_RETURN}"
+    PLATE_EARLY_RETURN_INDEX)
+if(NOT PLATE_EARLY_RETURN_INDEX EQUAL 0)
+    message(FATAL_ERROR
+        "Active-skill text plate helper must begin by rejecting invisible plans")
+endif()
+set(REQUIRED_PLATE_BACKING
+    "DrawRectangleRec(plan.backing, active_skill_text_plate_backing());")
+set(REQUIRED_PLATE_AUDIT
+    "record_ui_text_bounds(UiTextAuditPage::hud, plan.role, font, plan.text.data(), plan.position, plan.font_size, plan.spacing, plan.backing, plan.font_size, nullptr, 0U, 0);")
+set(REQUIRED_PLATE_TEXT
+    "draw_crisp_ui_text(font, plan.text.data(), plan.position, plan.font_size, plan.spacing, ui_text_contrast_style().primary, 0);")
+set(LAST_PLATE_INDEX -1)
+foreach(REQUIRED_PLATE_CALL IN ITEMS REQUIRED_PLATE_BACKING REQUIRED_PLATE_AUDIT REQUIRED_PLATE_TEXT)
+    count_substring("${ACTIVE_PLATE_BODY}" "${${REQUIRED_PLATE_CALL}}"
+        PLATE_CALL_COUNT)
+    if(NOT PLATE_CALL_COUNT EQUAL 1)
+        message(FATAL_ERROR
+            "Active-skill text plate helper must use exact backing, audit, and zero-shadow text")
+    endif()
+    string(FIND "${ACTIVE_PLATE_CODE}" "${${REQUIRED_PLATE_CALL}}"
+        PLATE_CALL_INDEX)
+    if(NOT PLATE_CALL_INDEX GREATER LAST_PLATE_INDEX)
+        message(FATAL_ERROR
+            "Active-skill text plate must draw backing, then audit, then text")
+    endif()
+    set(LAST_PLATE_INDEX ${PLATE_CALL_INDEX})
+endforeach()
+
+file(READ "${UI_TEXT_RENDERER_SOURCE}" TEXT_RENDERER_SOURCE)
+extract_cpp_function_body("${TEXT_RENDERER_SOURCE}"
+    "void draw_crisp_ui_text(Font font, const char* text,"
+    TEXT_RENDERER TEXT_RENDERER_CODE)
+require_real_code("${TEXT_RENDERER_CODE}" "draw_crisp_ui_text")
+set(REQUIRED_CONDITIONAL_SHADOW
+    "if (shadow_pixels > 0) { DrawTextEx(font, text, shadow, font_size, spacing, style.shadow); }")
+count_substring("${TEXT_RENDERER}" "${REQUIRED_CONDITIONAL_SHADOW}"
+    CONDITIONAL_SHADOW_COUNT)
+if(NOT CONDITIONAL_SHADOW_COUNT EQUAL 1)
+    message(FATAL_ERROR
+        "draw_crisp_ui_text must draw a shadow only when requested")
+endif()
+
+file(READ "${UI_TEXT_BOUNDS_AUDIT_SOURCE}" TEXT_AUDIT_SOURCE)
+extract_cpp_function_body("${TEXT_AUDIT_SOURCE}"
+    "[[nodiscard]] Rectangle measured_bounds(Font font, const char* text,"
+    TEXT_AUDIT_MEASURED TEXT_AUDIT_MEASURED_CODE)
+extract_cpp_function_body("${TEXT_AUDIT_SOURCE}"
+    "void record_ui_text_bounds(UiTextAuditPage page, UiTextAuditRole role,"
+    TEXT_AUDIT_RECORD TEXT_AUDIT_RECORD_CODE)
+require_real_code("${TEXT_AUDIT_MEASURED_CODE}" "UI text measured bounds")
+require_real_code("${TEXT_AUDIT_RECORD_CODE}" "record_ui_text_bounds")
+set(TEXT_AUDIT "${TEXT_AUDIT_MEASURED}")
+set(REQUIRED_EFFECTIVE_SHADOW
+    "const int effective_shadow = shadow_pixels > 0 ? shadow_pixels : 0;")
+set(REQUIRED_AUDIT_SHADOW_USE
+    "style.outline_pixels, effective_shadow")
+foreach(REQUIRED_AUDIT_TOKEN IN ITEMS
+        REQUIRED_EFFECTIVE_SHADOW REQUIRED_AUDIT_SHADOW_USE)
+    count_substring("${TEXT_AUDIT}" "${${REQUIRED_AUDIT_TOKEN}}"
+        AUDIT_TOKEN_COUNT)
+    if(NOT AUDIT_TOKEN_COUNT EQUAL 1)
+        message(FATAL_ERROR
+            "record_ui_text_bounds must measure its explicit shadow size")
+    endif()
+endforeach()
+set(REQUIRED_AUDIT_MEASURE_CALL
+    "font, text, position, font_size, spacing, shadow_pixels);")
+count_substring("${TEXT_AUDIT_RECORD}" "${REQUIRED_AUDIT_MEASURE_CALL}"
+    AUDIT_MEASURE_CALL_COUNT)
+if(NOT AUDIT_MEASURE_CALL_COUNT EQUAL 1)
+    message(FATAL_ERROR
+        "record_ui_text_bounds must pass the explicit shadow to measured bounds")
+endif()
+
+extract_cpp_function_body("${ACTIVE_SOURCE}"
+    "[[nodiscard]] UiTextExtent measure_active_skill_text(const char* text,"
+    ACTIVE_MEASURE_BODY ACTIVE_MEASURE_CODE)
+require_real_code("${ACTIVE_MEASURE_CODE}" "Active-skill MeasureTextEx adapter")
+foreach(REQUIRED_MEASURE_CALL IN ITEMS
+        "static_cast<const ActiveSkillTextMeasureContext*>(context);"
+        "!IsFontValid(measure_context->font)"
+        "MeasureTextEx(measure_context->font, text, font_size, spacing);")
+    count_substring("${ACTIVE_MEASURE_BODY}" "${REQUIRED_MEASURE_CALL}"
+        MEASURE_CALL_COUNT)
+    if(NOT MEASURE_CALL_COUNT EQUAL 1)
+        message(FATAL_ERROR
+            "Active-skill MeasureTextEx adapter must use its real shared-font context")
+    endif()
+endforeach()
+count_substring("${ACTIVE_DRAW_BODY}"
+    "ActiveSkillTextMeasureContext measure_context{hud_font};"
+    MEASURE_CONTEXT_COUNT)
+if(NOT MEASURE_CONTEXT_COUNT EQUAL 1)
+    message(FATAL_ERROR
+        "Active-skill HUD must create one shared-font measure context")
+endif()
+foreach(FORBIDDEN_ACTIVE_TOKEN IN ITEMS
+        "GetFontDefault(" "DrawText(" "TextFormat(" "snprintf(" "std::string")
+    set(ACTIVE_REAL_CODE
+        "${ACTIVE_DRAW_CODE} ${ACTIVE_PLATE_CODE} ${ACTIVE_MEASURE_CODE}")
+    string(FIND "${ACTIVE_REAL_CODE}" "${FORBIDDEN_ACTIVE_TOKEN}"
+        FORBIDDEN_ACTIVE_INDEX)
+    if(NOT FORBIDDEN_ACTIVE_INDEX EQUAL -1)
         message(FATAL_ERROR
-            "ActiveSkillRenderer::draw_hud model text must have one real use")
+            "ActiveSkillRenderer::draw_hud must not use ${FORBIDDEN_ACTIVE_TOKEN}")
     endif()
 endforeach()
 
 foreach(DRAW_BODY IN ITEMS HUD_DRAW_BODY ACTIVE_DRAW_BODY)
     foreach(FORBIDDEN IN ITEMS
             "TextFormat(" "snprintf(" "\"Num" "\"HP" "\"XP" "\"LV"
             "\"seconds" "\"second" "%.0fs")
         string(FIND "${${DRAW_BODY}}" "${FORBIDDEN}" FOUND)
         if(NOT FOUND EQUAL -1)
             message(FATAL_ERROR
```

