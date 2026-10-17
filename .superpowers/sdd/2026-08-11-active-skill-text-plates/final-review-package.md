# Active Skill Text Plates Final Review Package

## Range

- Plan baseline: `7fa00a3`
- Feature head: `6853a7ab3fceb1b97f1813bd528083e16ac71ee8`
- Review Task 1 and Task 2 as one feature.

## Required sources

- Approved design: `docs/superpowers/specs/2026-08-11-active-skill-text-plates-design.md`
- Plan: `docs/superpowers/plans/2026-08-11-active-skill-text-plates.md`
- Ledger: `.superpowers/sdd/2026-08-11-active-skill-text-plates/progress.md`
- Task reports and earlier review packages are in the same SDD directory.

## Known deferred observation

- The downward room world prompt `水流` can overlap the third empty skill name region in screen space.
- Static production and tests prove the empty skill plan is key-only; this is a separate world-prompt/HUD collision outside the approved seven-file Task 2 scope.
- Treat it as a follow-up concern unless it reveals a defect in the active-skill implementation itself.

## Verification already performed

- Debug target build exit 0.
- MENU/HUD selector unique `183 cases, 0 failures`.
- Structured renderer direct guard 1/1; self-test 1/1.
- Release `arpg_game` target build exit 0.
- F12 screenshots prove ready and cooling plates; clean empty screenshot remained visually confounded by the separate `水流` world prompt.
- No full platform/formal/Test.ps1/deployment.

## Commit list, stat, and full implementation diff

```text
6853a7a test(ui): harden active skill text plate guard
f280b3a fix(ui): back active skill labels with contrast
34eed40 test(ui): warm active skill ready plan
31da080 fix(ui): harden active skill text plan
c33317a feat(ui): plan centered active skill labels
 src/platform/raylib/active_skill_renderer.cpp      |  98 +++----
 src/platform/raylib/active_skill_view.cpp          | 118 ++++++++
 src/platform/raylib/active_skill_view.hpp          |  43 +++
 src/platform/raylib/ui_text_bounds_audit.cpp       |  11 +-
 src/platform/raylib/ui_text_bounds_audit.hpp       |   5 +-
 src/platform/raylib/ui_text_renderer.cpp           |   4 +-
 tests/platform/CMakeLists.txt                      |   4 +
 tests/platform/active_skill_view_tests.cpp         | 309 ++++++++++++++++++++-
 ...enderer_structured_model_source_self_test.cmake | 191 +++++++++----
 ...hud_renderer_structured_model_source_test.cmake | 259 +++++++++++++++--
 10 files changed, 894 insertions(+), 148 deletions(-)
diff --git a/src/platform/raylib/active_skill_renderer.cpp b/src/platform/raylib/active_skill_renderer.cpp
index 76be473..7cf51b0 100644
--- a/src/platform/raylib/active_skill_renderer.cpp
+++ b/src/platform/raylib/active_skill_renderer.cpp
@@ -1,24 +1,21 @@
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
@@ -36,21 +33,38 @@ constexpr std::size_t kStormGroundSwordCount = 12U;
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
@@ -80,34 +94,12 @@ void draw_skill_text(Font font, const char* text,
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
@@ -374,13 +366,15 @@ ActiveSkillCooldownOverlayPlan make_active_skill_cooldown_overlay(
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
@@ -418,39 +412,17 @@ void ActiveSkillRenderer::draw_hud(const ActiveSkillHudModel& model,
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
diff --git a/src/platform/raylib/active_skill_view.cpp b/src/platform/raylib/active_skill_view.cpp
index d57401b..9df6f7f 100644
--- a/src/platform/raylib/active_skill_view.cpp
+++ b/src/platform/raylib/active_skill_view.cpp
@@ -2,22 +2,74 @@
 
 #include "skills/active_skill_catalog.hpp"
 #include "ui_typography.hpp"
 #include "ui_utf8.hpp"
 
 #include <algorithm>
+#include <array>
 #include <cmath>
+#include <cstring>
 #include <cstdio>
 
 namespace arpg::platform {
 namespace {
 
 constexpr float kSlotSize = 76.0F;
 constexpr float kSlotGap = 8.0F;
 constexpr float kBottomMargin = 20.0F;
 
+[[nodiscard]] bool finite(float value) noexcept {
+    return std::isfinite(value);
+}
+
+[[nodiscard]] bool valid_rectangle(Rectangle bounds) noexcept {
+    return finite(bounds.x) && finite(bounds.y)
+        && finite(bounds.width) && finite(bounds.height)
+        && bounds.width > 0.0F && bounds.height > 0.0F
+        && finite(bounds.x + bounds.width)
+        && finite(bounds.y + bounds.height);
+}
+
+[[nodiscard]] bool c_string_is_valid_utf8(
+    const char* text, std::size_t capacity) noexcept {
+    return text != nullptr && std::memchr(text, '\0', capacity) != nullptr
+        && ui_utf8_is_valid(text);
+}
+
+[[nodiscard]] Rectangle rectangle_union(
+    Rectangle first, Rectangle second) noexcept {
+    const float left = std::min(first.x, second.x);
+    const float top = std::min(first.y, second.y);
+    const float right = std::max(first.x + first.width,
+        second.x + second.width);
+    const float bottom = std::max(first.y + first.height,
+        second.y + second.height);
+    return {left, top, right - left, bottom - top};
+}
+
+[[nodiscard]] ActiveSkillTextPlatePlan make_text_plate_plan(
+    const char* source, Rectangle backing, float scale, UiTextAuditRole role,
+    UiTextMeasureFn measure, void* measure_context) noexcept {
+    ActiveSkillTextPlatePlan result{};
+    result.backing = backing;
+    result.role = role;
+    const UiCenteredTextLayout fitted = fit_centered_ui_text(
+        result.text.data(), result.text.size(), source, backing,
+        16.0F * scale, 9.0F * scale, 0.5F * scale, measure,
+        measure_context);
+    if (!fitted.drawable) return {};
+    result.position = fitted.position;
+    result.extent = fitted.size;
+    result.font_size = fitted.font_size;
+    result.spacing = fitted.spacing;
+    result.role = role;
+    result.visible = true;
+    result.truncated = fitted.truncated;
+    return result;
+}
+
 [[nodiscard]] const char* active_skill_hud_name(
     skills::ActiveSkillId id,
     const skills::ActiveSkillDefinition& definition) noexcept {
     if (id == skills::ActiveSkillId::storm_swords) return u8"暴风式";
     return definition.display_name;
 }
@@ -80,24 +132,90 @@ ActiveSkillHudModel make_active_skill_hud_model(
 }
 
 ActiveSkillHudLayout active_skill_hud_layout(
     int width, int height) noexcept {
     if (width <= 0 || height <= 0) return {};
     const float scale = ui_viewport_scale(width, height);
+    if (!finite(scale) || scale <= 0.0F) return {};
     const float total_width = scale * (
         static_cast<float>(skills::kActiveSkillSlotCount) * kSlotSize
         + static_cast<float>(skills::kActiveSkillSlotCount - 1U) * kSlotGap);
     const float x = (static_cast<float>(width) - total_width) * 0.5F;
     const float y = static_cast<float>(height)
         - (kSlotSize + kBottomMargin) * scale;
+    const Rectangle viewport{0.0F, 0.0F, static_cast<float>(width),
+        static_cast<float>(height)};
     ActiveSkillHudLayout result{};
     result.bounds = {x, y, total_width, kSlotSize * scale};
+    result.scale = scale;
+    if (!valid_rectangle(viewport) || !valid_rectangle(result.bounds)) {
+        return {};
+    }
     for (std::size_t index = 0U; index < result.slots.size(); ++index) {
         result.slots[index] = {
             x + static_cast<float>(index) * (kSlotSize + kSlotGap) * scale,
             y, kSlotSize * scale, kSlotSize * scale,
         };
+        ActiveSkillHudTextRegions& regions = result.text_regions[index];
+        const Rectangle& slot = result.slots[index];
+        regions.key = {slot.x + 4.0F * scale, slot.y - 20.0F * scale,
+            slot.width - 8.0F * scale, 18.0F * scale};
+        regions.cooldown = {slot.x + 16.0F * scale,
+            slot.y + 28.0F * scale, slot.width - 32.0F * scale,
+            20.0F * scale};
+        regions.name = {slot.x + 7.0F * scale, slot.y + 77.0F * scale,
+            slot.width - 14.0F * scale, 18.0F * scale};
+        regions.visual_bounds = rectangle_union(slot, regions.key);
+        regions.visual_bounds = rectangle_union(
+            regions.visual_bounds, regions.cooldown);
+        regions.visual_bounds = rectangle_union(
+            regions.visual_bounds, regions.name);
+        if (!valid_rectangle(slot) || !valid_rectangle(regions.key)
+                || !valid_rectangle(regions.cooldown)
+                || !valid_rectangle(regions.name)
+                || !valid_rectangle(regions.visual_bounds)
+                || !ui_text_bounds_inside(regions.visual_bounds, viewport)) {
+            return {};
+        }
+    }
+    result.valid = true;
+    return result;
+}
+
+ActiveSkillHudTextPlan make_active_skill_hud_text_plan(
+    const ActiveSkillHudSlot& slot,
+    const ActiveSkillHudTextRegions& regions,
+    float scale, bool font_ready,
+    UiTextMeasureFn measure, void* measure_context) noexcept {
+    if (!font_ready || !finite(scale) || scale <= 0.0F || measure == nullptr
+            || !valid_rectangle(regions.key)
+            || !valid_rectangle(regions.cooldown)
+            || !valid_rectangle(regions.name)
+            || !valid_rectangle(regions.visual_bounds)
+            || !c_string_is_valid_utf8(slot.key_label.data(),
+                slot.key_label.size())
+            || !c_string_is_valid_utf8(slot.name.data(), slot.name.size())
+            || !c_string_is_valid_utf8(slot.cooldown_label.data(),
+                slot.cooldown_label.size())) {
+        return {};
     }
+
+    ActiveSkillHudTextPlan result{};
+    result.key = make_text_plate_plan(slot.key_label.data(), regions.key,
+        scale, UiTextAuditRole::hud_skill_key, measure, measure_context);
+    if (!result.key.visible) return {};
+    if (slot.empty) return result;
+
+    result.name = make_text_plate_plan(slot.name.data(), regions.name, scale,
+        UiTextAuditRole::hud_skill_name, measure, measure_context);
+    if (!finite(slot.cooldown_ratio) || slot.cooldown_ratio <= 0.0F
+            || slot.cooldown_label[0U] == '\0') {
+        return result;
+    }
+
+    result.cooldown = make_text_plate_plan(slot.cooldown_label.data(),
+        regions.cooldown, scale, UiTextAuditRole::hud_skill_cooldown,
+        measure, measure_context);
     return result;
 }
 
 }  // namespace arpg::platform
diff --git a/src/platform/raylib/active_skill_view.hpp b/src/platform/raylib/active_skill_view.hpp
index 2b350eb..c3639e3 100644
--- a/src/platform/raylib/active_skill_view.hpp
+++ b/src/platform/raylib/active_skill_view.hpp
@@ -1,10 +1,12 @@
 #pragma once
 
 #include "material_asset_types.hpp"
 #include "skills/active_skill_types.hpp"
+#include "ui_text_bounds_audit.hpp"
+#include "ui_text_layout.hpp"
 
 #include <raylib.h>
 
 #include <array>
 #include <cstdint>
 
@@ -26,20 +28,61 @@ struct ActiveSkillHudSlot final {
 
 struct ActiveSkillHudModel final {
     std::array<ActiveSkillHudSlot,
         skills::kActiveSkillSlotCount> slots{};
 };
 
+inline constexpr std::size_t kActiveSkillTextCapacity = 48U;
+
+struct ActiveSkillHudTextRegions final {
+    Rectangle key{};
+    Rectangle cooldown{};
+    Rectangle name{};
+    Rectangle visual_bounds{};
+};
+
+struct ActiveSkillTextPlatePlan final {
+    Rectangle backing{};
+    Vector2 position{};
+    Vector2 extent{};
+    std::array<char, kActiveSkillTextCapacity> text{};
+    float font_size{};
+    float spacing{};
+    UiTextAuditRole role{UiTextAuditRole::count};
+    bool visible{};
+    bool truncated{};
+};
+
+struct ActiveSkillHudTextPlan final {
+    ActiveSkillTextPlatePlan key{};
+    ActiveSkillTextPlatePlan cooldown{};
+    ActiveSkillTextPlatePlan name{};
+};
+
 struct ActiveSkillHudLayout final {
     Rectangle bounds{};
     std::array<Rectangle, skills::kActiveSkillSlotCount> slots{};
+    float scale{};
+    bool valid{};
+    std::array<ActiveSkillHudTextRegions,
+        skills::kActiveSkillSlotCount> text_regions{};
 };
 
+[[nodiscard]] constexpr Color active_skill_text_plate_backing() noexcept {
+    return {5, 9, 16, 255};
+}
+
 [[nodiscard]] ActiveSkillHudModel make_active_skill_hud_model(
     const skills::SkillLoadoutState& loadout,
     const std::array<std::uint16_t, skills::kActiveSkillCount>& cooldowns)
     noexcept;
 
 [[nodiscard]] ActiveSkillHudLayout active_skill_hud_layout(
     int width, int height) noexcept;
 
+[[nodiscard]] ActiveSkillHudTextPlan make_active_skill_hud_text_plan(
+    const ActiveSkillHudSlot& slot,
+    const ActiveSkillHudTextRegions& regions,
+    float scale, bool font_ready,
+    UiTextMeasureFn measure, void* measure_context) noexcept;
+
 }  // namespace arpg::platform
diff --git a/src/platform/raylib/ui_text_bounds_audit.cpp b/src/platform/raylib/ui_text_bounds_audit.cpp
index 3d34573..6e8f48e 100644
--- a/src/platform/raylib/ui_text_bounds_audit.cpp
+++ b/src/platform/raylib/ui_text_bounds_audit.cpp
@@ -10,20 +10,22 @@
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
@@ -53,13 +55,14 @@ bool ui_text_bounds_separated(Rectangle first, Rectangle second) noexcept {
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
@@ -71,13 +74,13 @@ void record_ui_text_bounds(UiTextAuditPage page, UiTextAuditRole role,
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
diff --git a/src/platform/raylib/ui_text_bounds_audit.hpp b/src/platform/raylib/ui_text_bounds_audit.hpp
index 8713254..46190be 100644
--- a/src/platform/raylib/ui_text_bounds_audit.hpp
+++ b/src/platform/raylib/ui_text_bounds_audit.hpp
@@ -35,12 +35,14 @@ enum class UiTextAuditRole : std::uint8_t {
     skill_main_slot,
     skill_description,
     skill_inventory_entry,
     pause_title,
     pause_row,
     pause_footer,
+    hud_skill_key,
+    hud_skill_cooldown,
     count,
 };
 
 struct UiTextAuditPageStatus final {
     bool bounds_safe{true};
     bool sizes_readable{true};
@@ -61,11 +63,12 @@ void reset_ui_text_bounds_audit() noexcept;
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
@@ -26,11 +26,13 @@ void draw_crisp_ui_text(Font font, const char* text, Vector2 position,
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
@@ -2084,12 +2084,14 @@ set_tests_properties(
 
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
@@ -2098,12 +2100,14 @@ set_tests_properties(
 
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
diff --git a/tests/platform/active_skill_view_tests.cpp b/tests/platform/active_skill_view_tests.cpp
index 39125ed..05698a5 100644
--- a/tests/platform/active_skill_view_tests.cpp
+++ b/tests/platform/active_skill_view_tests.cpp
@@ -3,27 +3,80 @@
 
 #include "active_skill_renderer.hpp"
 #include "active_skill_view.hpp"
 #include "combat/active_skill_runtime.hpp"
 #include "skills/active_skill_catalog.hpp"
 #include "skills/skill_loadout.hpp"
+#include "ui_text_bounds_audit.hpp"
+#include "ui_text_contrast.hpp"
+#include "ui_text_layout.hpp"
 #include "ui_typography.hpp"
 #include "ui_utf8.hpp"
 
 #include <array>
 #include <cstddef>
 #include <cstring>
+#include <limits>
 #include <type_traits>
 #include <utility>
 
 namespace {
 
 namespace combat = arpg::combat;
 namespace platform = arpg::platform;
 namespace skills = arpg::skills;
 
+platform::UiTextExtent measure_skill_text(const char* text, float size,
+    float spacing, void*) noexcept {
+    std::size_t count{};
+    for (const unsigned char* cursor =
+             reinterpret_cast<const unsigned char*>(text);
+         cursor != nullptr && *cursor != 0U; ++cursor) {
+        if ((*cursor & 0xC0U) != 0x80U) ++count;
+    }
+    return {count == 0U ? 0.0F
+                        : static_cast<float>(count) * size
+                            + static_cast<float>(count - 1U) * spacing,
+        size};
+}
+
+platform::UiTextExtent fail_skill_text_measure(
+    const char*, float, float, void*) noexcept {
+    return {};
+}
+
+enum class SkillMeasureFailure : std::uint8_t {
+    none,
+    key,
+    name,
+    cooldown,
+};
+
+struct SkillMeasureContext final {
+    SkillMeasureFailure failure{SkillMeasureFailure::none};
+    std::size_t key_calls{};
+    std::size_t name_calls{};
+    std::size_t cooldown_calls{};
+};
+
+platform::UiTextExtent measure_skill_text_by_stage(const char* text,
+    float size, float spacing, void* context) noexcept {
+    SkillMeasureContext& state = *static_cast<SkillMeasureContext*>(context);
+    if (std::strcmp(text, u8"小键盘1") == 0) {
+        ++state.key_calls;
+        if (state.failure == SkillMeasureFailure::key) return {};
+    } else if (std::strcmp(text, u8"拔刀斩") == 0) {
+        ++state.name_calls;
+        if (state.failure == SkillMeasureFailure::name) return {};
+    } else if (std::strcmp(text, u8"2秒") == 0) {
+        ++state.cooldown_calls;
+        if (state.failure == SkillMeasureFailure::cooldown) return {};
+    }
+    return measure_skill_text(text, size, spacing, nullptr);
+}
+
 template <typename Slot, typename = void>
 struct SkillKeyLabelContract final {
     [[nodiscard]] static bool matches(
         const Slot&, const char*) noexcept {
         return false;
     }
@@ -42,12 +95,19 @@ bool overlaps(Rectangle left, Rectangle right) noexcept {
     return left.x < right.x + right.width
         && right.x < left.x + left.width
         && left.y < right.y + right.height
         && right.y < left.y + left.height;
 }
 
+bool same_rectangle(Rectangle left, Rectangle right) noexcept {
+    return arpg::test::near(left.x, right.x)
+        && arpg::test::near(left.y, right.y)
+        && arpg::test::near(left.width, right.width)
+        && arpg::test::near(left.height, right.height);
+}
+
 arpg::test::Failure active_skill_visual_frames_reach_complete_material_timelines()
     noexcept {
     std::array<bool, 36U> draw_slash_frames{};
     for (std::uint16_t tick = 0U; tick < 90U; ++tick) {
         const std::size_t frame = platform::active_skill_visual_frame_index(
             skills::ActiveSkillId::draw_slash, tick);
@@ -235,45 +295,288 @@ arpg::test::Failure hud_cooldown_ratios_are_clamped_and_empty_slots_stay_zero()
         == nullptr);
     for (std::size_t index = 2U; index < view.slots.size(); ++index) {
         ARPG_REQUIRE(arpg::test::near(
             view.slots[index].cooldown_ratio, 0.0F));
         ARPG_REQUIRE(view.slots[index].cooldown_label[0] == '\0');
     }
+
+    const platform::ActiveSkillHudLayout layout =
+        platform::active_skill_hud_layout(800, 450);
+    const platform::ActiveSkillHudTextRegions& regions =
+        layout.text_regions[0U];
+    platform::ActiveSkillHudSlot ready_slot = view.slots[1U];
+    ready_slot.cooldown_ratio = 0.0F;
+    ready_slot.cooldown_label[0U] = '\0';
+    const platform::ActiveSkillHudTextPlan empty =
+        platform::make_active_skill_hud_text_plan(view.slots[2U], regions,
+            layout.scale, true, &measure_skill_text, nullptr);
+    const platform::ActiveSkillHudTextPlan ready =
+        platform::make_active_skill_hud_text_plan(ready_slot, regions,
+            layout.scale, true, &measure_skill_text, nullptr);
+    const platform::ActiveSkillHudTextPlan cooling =
+        platform::make_active_skill_hud_text_plan(view.slots[0U], regions,
+            layout.scale, true, &measure_skill_text, nullptr);
+
+    ARPG_REQUIRE(empty.key.visible);
+    ARPG_REQUIRE(!empty.name.visible);
+    ARPG_REQUIRE(!empty.cooldown.visible);
+    ARPG_REQUIRE(ready.key.visible);
+    ARPG_REQUIRE(ready.name.visible);
+    ARPG_REQUIRE(!ready.cooldown.visible);
+    ARPG_REQUIRE(cooling.key.visible);
+    ARPG_REQUIRE(cooling.name.visible);
+    ARPG_REQUIRE(cooling.cooldown.visible);
+    ARPG_REQUIRE(cooling.key.role == platform::UiTextAuditRole::hud_skill_key);
+    ARPG_REQUIRE(cooling.name.role == platform::UiTextAuditRole::hud_skill_name);
+    ARPG_REQUIRE(cooling.cooldown.role
+        == platform::UiTextAuditRole::hud_skill_cooldown);
+
+    constexpr std::array<const char*, skills::kActiveSkillSlotCount>
+        kExpectedKeyLabels{{u8"小键盘1", u8"小键盘2", u8"小键盘3",
+            u8"小键盘4", u8"小键盘5"}};
+    for (std::size_t index = 0U; index < view.slots.size(); ++index) {
+        const platform::ActiveSkillHudTextPlan plan =
+            platform::make_active_skill_hud_text_plan(view.slots[index],
+                layout.text_regions[index], layout.scale, true,
+                &measure_skill_text, nullptr);
+        ARPG_REQUIRE(plan.key.visible);
+        ARPG_REQUIRE(std::strcmp(plan.key.text.data(),
+            view.slots[index].key_label.data()) == 0);
+        ARPG_REQUIRE(std::strcmp(plan.key.text.data(),
+            kExpectedKeyLabels[index]) == 0);
+        ARPG_REQUIRE(!plan.key.truncated);
+        ARPG_REQUIRE(same_rectangle(plan.key.backing,
+            layout.text_regions[index].key));
+    }
+    const platform::ActiveSkillHudTextPlan cooling_thirty =
+        platform::make_active_skill_hud_text_plan(view.slots[1U],
+            layout.text_regions[1U], layout.scale, true, &measure_skill_text,
+            nullptr);
+    for (const platform::ActiveSkillTextPlatePlan* plate :
+         std::array<const platform::ActiveSkillTextPlatePlan*, 2U>{
+             &cooling.name, &cooling_thirty.name}) {
+        ARPG_REQUIRE(plate->visible);
+        ARPG_REQUIRE(!plate->truncated);
+    }
+    ARPG_REQUIRE(std::strcmp(cooling.name.text.data(),
+        view.slots[0U].name.data()) == 0);
+    ARPG_REQUIRE(std::strcmp(cooling.name.text.data(), u8"拔刀斩") == 0);
+    ARPG_REQUIRE(same_rectangle(cooling.name.backing, regions.name));
+    ARPG_REQUIRE(std::strcmp(cooling_thirty.name.text.data(),
+        view.slots[1U].name.data()) == 0);
+    ARPG_REQUIRE(std::strcmp(cooling_thirty.name.text.data(), u8"暴风式") == 0);
+    ARPG_REQUIRE(same_rectangle(cooling_thirty.name.backing,
+        layout.text_regions[1U].name));
+    ARPG_REQUIRE(cooling_thirty.cooldown.visible);
+    ARPG_REQUIRE(std::strcmp(cooling_thirty.cooldown.text.data(),
+        view.slots[1U].cooldown_label.data()) == 0);
+    ARPG_REQUIRE(std::strcmp(cooling_thirty.cooldown.text.data(),
+        u8"30秒") == 0);
+    ARPG_REQUIRE(!cooling_thirty.cooldown.truncated);
+    ARPG_REQUIRE(same_rectangle(cooling_thirty.cooldown.backing,
+        layout.text_regions[1U].cooldown));
+
+    for (const platform::ActiveSkillTextPlatePlan* plate :
+         std::array<const platform::ActiveSkillTextPlatePlan*, 3U>{
+             &cooling.key, &cooling.name, &cooling.cooldown}) {
+        ARPG_REQUIRE(platform::ui_utf8_is_valid(plate->text.data()));
+        ARPG_REQUIRE(platform::ui_text_bounds_inside(
+            {plate->position.x, plate->position.y, plate->extent.x,
+                plate->extent.y}, plate->backing));
+        ARPG_REQUIRE(!plate->truncated);
+    }
+    ARPG_REQUIRE(platform::active_skill_text_plate_backing().a == 255U);
+    ARPG_REQUIRE(platform::ui_luma_contrast_ratio(
+        platform::ui_text_contrast_style().primary,
+        platform::active_skill_text_plate_backing()) >= 4.5F);
+
+    const auto plan_is_empty = [](const platform::ActiveSkillHudTextPlan& plan)
+        noexcept {
+        return !plan.key.visible && !plan.name.visible && !plan.cooldown.visible;
+    };
+    ARPG_REQUIRE(plan_is_empty(platform::make_active_skill_hud_text_plan(
+        view.slots[0U], regions, layout.scale, false, &measure_skill_text,
+        nullptr)));
+    platform::ActiveSkillHudSlot invalid_utf8 = view.slots[0U];
+    invalid_utf8.name[0U] = static_cast<char>(0xFFU);
+    ARPG_REQUIRE(plan_is_empty(platform::make_active_skill_hud_text_plan(
+        invalid_utf8, regions, layout.scale, true, &measure_skill_text,
+        nullptr)));
+    platform::ActiveSkillHudTextRegions invalid_regions = regions;
+    invalid_regions.name.width = 0.0F;
+    ARPG_REQUIRE(plan_is_empty(platform::make_active_skill_hud_text_plan(
+        view.slots[0U], invalid_regions, layout.scale, true,
+        &measure_skill_text, nullptr)));
+    ARPG_REQUIRE(plan_is_empty(platform::make_active_skill_hud_text_plan(
+        view.slots[0U], regions, layout.scale, true,
+        &fail_skill_text_measure, nullptr)));
+
+    for (const float ratio : std::array<float, 3U>{
+             std::numeric_limits<float>::quiet_NaN(),
+             std::numeric_limits<float>::infinity(),
+             -std::numeric_limits<float>::infinity()}) {
+        platform::ActiveSkillHudSlot non_finite_ratio = view.slots[0U];
+        non_finite_ratio.cooldown_ratio = ratio;
+        const platform::ActiveSkillHudTextPlan non_finite_plan =
+            platform::make_active_skill_hud_text_plan(non_finite_ratio,
+                regions, layout.scale, true, &measure_skill_text, nullptr);
+        ARPG_REQUIRE(non_finite_plan.key.visible);
+        ARPG_REQUIRE(non_finite_plan.name.visible);
+        ARPG_REQUIRE(!non_finite_plan.cooldown.visible);
+    }
+
+    SkillMeasureContext name_failure{SkillMeasureFailure::name};
+    const platform::ActiveSkillHudTextPlan failed_name =
+        platform::make_active_skill_hud_text_plan(view.slots[0U], regions,
+            layout.scale, true, &measure_skill_text_by_stage, &name_failure);
+    ARPG_REQUIRE(name_failure.key_calls > 0U);
+    ARPG_REQUIRE(name_failure.name_calls > 0U);
+    ARPG_REQUIRE(name_failure.cooldown_calls > 0U);
+    ARPG_REQUIRE(failed_name.key.visible);
+    ARPG_REQUIRE(!failed_name.name.visible);
+    ARPG_REQUIRE(failed_name.cooldown.visible);
+
+    SkillMeasureContext cooldown_failure{SkillMeasureFailure::cooldown};
+    const platform::ActiveSkillHudTextPlan failed_cooldown =
+        platform::make_active_skill_hud_text_plan(view.slots[0U], regions,
+            layout.scale, true, &measure_skill_text_by_stage,
+            &cooldown_failure);
+    ARPG_REQUIRE(cooldown_failure.key_calls > 0U);
+    ARPG_REQUIRE(cooldown_failure.name_calls > 0U);
+    ARPG_REQUIRE(cooldown_failure.cooldown_calls > 0U);
+    ARPG_REQUIRE(failed_cooldown.key.visible);
+    ARPG_REQUIRE(failed_cooldown.name.visible);
+    ARPG_REQUIRE(!failed_cooldown.cooldown.visible);
+
+    SkillMeasureContext key_failure{SkillMeasureFailure::key};
+    const platform::ActiveSkillHudTextPlan failed_key =
+        platform::make_active_skill_hud_text_plan(view.slots[0U], regions,
+            layout.scale, true, &measure_skill_text_by_stage, &key_failure);
+    ARPG_REQUIRE(key_failure.key_calls > 0U);
+    ARPG_REQUIRE(key_failure.name_calls == 0U);
+    ARPG_REQUIRE(key_failure.cooldown_calls == 0U);
+    ARPG_REQUIRE(plan_is_empty(failed_key));
+
+    static_cast<void>(platform::make_active_skill_hud_text_plan(view.slots[2U],
+        regions, layout.scale, true, &measure_skill_text, nullptr));
+    static_cast<void>(platform::make_active_skill_hud_text_plan(ready_slot,
+        regions, layout.scale, true, &measure_skill_text, nullptr));
+    static_cast<void>(platform::make_active_skill_hud_text_plan(view.slots[0U],
+        regions, layout.scale, true, &measure_skill_text, nullptr));
+    const std::uint64_t allocations_before = arpg::test::allocation_count();
+    for (std::size_t iteration = 0U; iteration < 10'000U; ++iteration) {
+        const platform::ActiveSkillHudLayout repeated_layout =
+            platform::active_skill_hud_layout(800, 450);
+        const platform::ActiveSkillHudTextPlan repeated_empty =
+            platform::make_active_skill_hud_text_plan(
+            view.slots[2U], repeated_layout.text_regions[0U],
+            repeated_layout.scale, true, &measure_skill_text, nullptr);
+        const platform::ActiveSkillHudTextPlan repeated_ready =
+            platform::make_active_skill_hud_text_plan(
+            ready_slot, repeated_layout.text_regions[0U],
+            repeated_layout.scale, true, &measure_skill_text, nullptr);
+        const platform::ActiveSkillHudTextPlan repeated_cooling =
+            platform::make_active_skill_hud_text_plan(
+            view.slots[0U], repeated_layout.text_regions[0U],
+            repeated_layout.scale, true, &measure_skill_text, nullptr);
+        ARPG_REQUIRE(repeated_empty.key.visible
+            && !repeated_empty.name.visible && !repeated_empty.cooldown.visible);
+        ARPG_REQUIRE(repeated_ready.key.visible
+            && repeated_ready.name.visible && !repeated_ready.cooldown.visible);
+        ARPG_REQUIRE(repeated_cooling.key.visible
+            && repeated_cooling.name.visible && repeated_cooling.cooldown.visible);
+    }
+    ARPG_REQUIRE(arpg::test::allocation_count() == allocations_before);
     return {};
 }
 
 arpg::test::Failure hud_layout_is_bottom_centered_with_scaled_slot_geometry()
     noexcept {
     constexpr std::array<std::array<int, 2>, 3> kViewports{{
-        {{1280, 720}}, {{1600, 900}}, {{1920, 1080}},
+        {{800, 450}}, {{1280, 720}}, {{1920, 1080}},
     }};
     for (const auto viewport : kViewports) {
-        const float scale = platform::ui_viewport_scale(
-            viewport[0], viewport[1]);
+        const float scale = viewport[0] == 1920 ? 1.5F : 1.0F;
         const float expected_width = (5.0F * 76.0F + 4.0F * 8.0F) * scale;
         const platform::ActiveSkillHudLayout layout =
             platform::active_skill_hud_layout(viewport[0], viewport[1]);
+        ARPG_REQUIRE(layout.valid);
+        ARPG_REQUIRE(arpg::test::near(layout.scale, scale));
         ARPG_REQUIRE(arpg::test::near(layout.bounds.width, expected_width));
         ARPG_REQUIRE(arpg::test::near(layout.bounds.x
             + layout.bounds.width * 0.5F,
             static_cast<float>(viewport[0]) * 0.5F));
         ARPG_REQUIRE(layout.bounds.y + layout.bounds.height
             <= static_cast<float>(viewport[1]));
         for (std::size_t index = 0U; index < layout.slots.size(); ++index) {
             ARPG_REQUIRE(arpg::test::near(
                 layout.slots[index].width, 76.0F * scale));
             ARPG_REQUIRE(arpg::test::near(
                 layout.slots[index].height, 76.0F * scale));
+            const platform::ActiveSkillHudTextRegions& regions =
+                layout.text_regions[index];
+            ARPG_REQUIRE(arpg::test::near(regions.key.x,
+                layout.slots[index].x + 4.0F * scale));
+            ARPG_REQUIRE(arpg::test::near(regions.key.y,
+                layout.slots[index].y - 20.0F * scale));
+            ARPG_REQUIRE(arpg::test::near(regions.key.width,
+                layout.slots[index].width - 8.0F * scale));
+            ARPG_REQUIRE(arpg::test::near(regions.key.height, 18.0F * scale));
+            ARPG_REQUIRE(arpg::test::near(regions.cooldown.x,
+                layout.slots[index].x + 16.0F * scale));
+            ARPG_REQUIRE(arpg::test::near(regions.cooldown.y,
+                layout.slots[index].y + 28.0F * scale));
+            ARPG_REQUIRE(arpg::test::near(regions.cooldown.width,
+                layout.slots[index].width - 32.0F * scale));
+            ARPG_REQUIRE(arpg::test::near(
+                regions.cooldown.height, 20.0F * scale));
+            ARPG_REQUIRE(arpg::test::near(regions.name.x,
+                layout.slots[index].x + 7.0F * scale));
+            ARPG_REQUIRE(arpg::test::near(regions.name.y,
+                layout.slots[index].y + 77.0F * scale));
+            ARPG_REQUIRE(arpg::test::near(regions.name.width,
+                layout.slots[index].width - 14.0F * scale));
+            ARPG_REQUIRE(arpg::test::near(regions.name.height, 18.0F * scale));
+            ARPG_REQUIRE(platform::ui_text_bounds_inside(regions.visual_bounds,
+                {0.0F, 0.0F, static_cast<float>(viewport[0]),
+                    static_cast<float>(viewport[1])}));
+            ARPG_REQUIRE(platform::ui_text_bounds_separated(
+                regions.key, regions.cooldown));
+            ARPG_REQUIRE(platform::ui_text_bounds_separated(
+                regions.key, regions.name));
+            ARPG_REQUIRE(platform::ui_text_bounds_separated(
+                regions.cooldown, regions.name));
             if (index != 0U) {
                 ARPG_REQUIRE(arpg::test::near(layout.slots[index].x
                     - (layout.slots[index - 1U].x
                         + layout.slots[index - 1U].width), 8.0F * scale));
                 ARPG_REQUIRE(!overlaps(
                     layout.slots[index - 1U], layout.slots[index]));
             }
         }
+        if (viewport[0] == 800) {
+            ARPG_REQUIRE(arpg::test::near(layout.bounds.x, 194.0F));
+            ARPG_REQUIRE(arpg::test::near(layout.bounds.y, 354.0F));
+            ARPG_REQUIRE(arpg::test::near(layout.bounds.width, 412.0F));
+            ARPG_REQUIRE(arpg::test::near(layout.bounds.height, 76.0F));
+            const platform::ActiveSkillHudTextRegions& regions =
+                layout.text_regions[0U];
+            ARPG_REQUIRE(arpg::test::near(regions.key.y, 334.0F));
+            ARPG_REQUIRE(arpg::test::near(
+                regions.key.y + regions.key.height, 352.0F));
+            ARPG_REQUIRE(arpg::test::near(regions.cooldown.y, 382.0F));
+            ARPG_REQUIRE(arpg::test::near(
+                regions.cooldown.y + regions.cooldown.height, 402.0F));
+            ARPG_REQUIRE(arpg::test::near(regions.name.y, 431.0F));
+            ARPG_REQUIRE(arpg::test::near(
+                regions.name.y + regions.name.height, 449.0F));
+            const Rectangle icon{layout.slots[0U].x + 8.0F,
+                layout.slots[0U].y + 8.0F, 60.0F, 60.0F};
+            ARPG_REQUIRE((regions.cooldown.width * regions.cooldown.height)
+                / (icon.width * icon.height) <= 0.25F);
+        }
     }
     return {};
 }
 
 arpg::test::Failure native_effect_plan_uses_snapshot_timing_and_twelve_swords()
     noexcept {
diff --git a/tests/platform/hud_renderer_structured_model_source_self_test.cmake b/tests/platform/hud_renderer_structured_model_source_self_test.cmake
index 365900e..7ec5099 100644
--- a/tests/platform/hud_renderer_structured_model_source_self_test.cmake
+++ b/tests/platform/hud_renderer_structured_model_source_self_test.cmake
@@ -1,9 +1,10 @@
 cmake_minimum_required(VERSION 3.25)
 
 foreach(REQUIRED_INPUT IN ITEMS HUD_RENDERER_SOURCE ACTIVE_SKILL_RENDERER_SOURCE
+        UI_TEXT_RENDERER_SOURCE UI_TEXT_BOUNDS_AUDIT_SOURCE
         GUARD_SCRIPT MUTATION_ROOT)
     if(NOT DEFINED ${REQUIRED_INPUT}
             OR ("${${REQUIRED_INPUT}}" STREQUAL "")
             OR (NOT REQUIRED_INPUT STREQUAL "MUTATION_ROOT"
                 AND NOT EXISTS "${${REQUIRED_INPUT}}"))
         message(FATAL_ERROR "HUD renderer guard self-test input missing: ${REQUIRED_INPUT}")
@@ -11,12 +12,14 @@ foreach(REQUIRED_INPUT IN ITEMS HUD_RENDERER_SOURCE ACTIVE_SKILL_RENDERER_SOURCE
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
@@ -27,18 +30,30 @@ set(STAGE16_FIXTURE_TEXT
 
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
@@ -62,12 +77,14 @@ function(run_guard LABEL HUD_TEXT ACTIVE_TEXT EXPECT_SUCCESS)
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
@@ -78,36 +95,37 @@ function(run_guard LABEL HUD_TEXT ACTIVE_TEXT EXPECT_SUCCESS)
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
@@ -128,76 +146,143 @@ run_fixture_reset_mutant(refresh_then_typed_reassignment
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
+    "${HUD_ORIGINAL}" "${ACTIVE_MUTATED}" FALSE)
+
+replace_required("${ACTIVE_ORIGINAL}" "ui_text_contrast_style().primary, 0);"
+    "ui_text_contrast_style().primary, 2);" ACTIVE_MUTATED
+    "replace draw zero shadow")
+run_guard(active_skill_plate_draw_uses_shadow
+    "${HUD_ORIGINAL}" "${ACTIVE_MUTATED}" FALSE)
+
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
+    "${HUD_ORIGINAL}" "${ACTIVE_MUTATED}" FALSE)
+
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
 
-string(REPLACE "slot.key_label.data()" "legacy_key_label(slot)"
-    ACTIVE_MUTATED "${ACTIVE_ORIGINAL}")
-set(ACTIVE_MUTATED
-    "${ACTIVE_MUTATED}\nvoid decoy_key_label_use(){ slot.key_label.data(); }\n")
-run_guard(active_ignores_key_label
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
     "${HUD_ORIGINAL}" "${ACTIVE_MUTATED}" FALSE)
 
-string(REPLACE "slot.cooldown_label.data()" "\"2 seconds\""
-    ACTIVE_MUTATED "${ACTIVE_ORIGINAL}")
-set(ACTIVE_MUTATED
-    "${ACTIVE_MUTATED}\nvoid decoy_cooldown_use(){ slot.cooldown_label.data(); }\n")
-run_guard(active_hardcodes_seconds
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
     "${HUD_ORIGINAL}" "${ACTIVE_MUTATED}" FALSE)
 
-string(REPLACE "slot.cooldown_label.data()" "legacy_cooldown_label(slot)"
-    ACTIVE_MUTATED "${ACTIVE_ORIGINAL}")
-set(ACTIVE_MUTATED
-    "${ACTIVE_MUTATED}\nvoid decoy_cooldown_use(){ slot.cooldown_label.data(); }\n")
-run_guard(active_ignores_cooldown_label
+set(TEXT_PLAN_DRAW_BLOCK
+    "        const ActiveSkillHudTextPlan text_plan = make_active_skill_hud_text_plan(\n            slot, layout.text_regions[index], layout.scale,\n            hud_font_ready && IsFontValid(hud_font),\n            &measure_active_skill_text, &measure_context);\n        draw_active_skill_text_plate(hud_font, text_plan.key);\n        draw_active_skill_text_plate(hud_font, text_plan.cooldown);\n        draw_active_skill_text_plate(hud_font, text_plan.name);\n")
+replace_required("${ACTIVE_ORIGINAL}" "${TEXT_PLAN_DRAW_BLOCK}" ""
+    ACTIVE_MUTATED "remove live text plan and plate draw block")
+replace_required("${ACTIVE_MUTATED}"
+    "        if (!slot.empty) {\n            const float inset ="
+    "${TEXT_PLAN_DRAW_BLOCK}        if (!slot.empty) {\n            const float inset ="
+    ACTIVE_MUTATED "move text plan and plates before icon and cooldown artwork")
+run_guard(active_skill_plan_before_icon_and_cooldown_artwork
     "${HUD_ORIGINAL}" "${ACTIVE_MUTATED}" FALSE)
 
 if(MISSED_MUTATIONS)
     list(JOIN MISSED_MUTATIONS ", " MISSED_LABELS)
     message(FATAL_ERROR
         "HUD renderer guard missed mutations: ${MISSED_LABELS}")
diff --git a/tests/platform/hud_renderer_structured_model_source_test.cmake b/tests/platform/hud_renderer_structured_model_source_test.cmake
index 8f4cfff..a7bd8b8 100644
--- a/tests/platform/hud_renderer_structured_model_source_test.cmake
+++ b/tests/platform/hud_renderer_structured_model_source_test.cmake
@@ -1,9 +1,10 @@
 cmake_minimum_required(VERSION 3.25)
 
 foreach(REQUIRED_INPUT IN ITEMS HUD_RENDERER_SOURCE ACTIVE_SKILL_RENDERER_SOURCE
+        UI_TEXT_RENDERER_SOURCE UI_TEXT_BOUNDS_AUDIT_SOURCE
         DUNGEON_RUNTIME_SOURCE LOOT_SUCTION_SOURCE STAGE12_SOURCE STAGE16_SOURCE)
     if(NOT DEFINED ${REQUIRED_INPUT} OR NOT EXISTS "${${REQUIRED_INPUT}}")
         message(FATAL_ERROR
             "HUD renderer structured-model source input missing: ${REQUIRED_INPUT}")
     endif()
 endforeach()
@@ -23,18 +24,56 @@ function(count_substring INPUT NEEDLE OUTPUT)
         math(EXPR NEXT "${FOUND} + ${NEEDLE_LENGTH}")
         string(SUBSTRING "${REMAINDER}" ${NEXT} -1 REMAINDER)
     endwhile()
     set(${OUTPUT} ${COUNT} PARENT_SCOPE)
 endfunction()
 
+function(require_real_code BODY LABEL)
+    string(REGEX MATCH "if([ ]+constexpr)?[ ]*\\([ ]*(false|0)[ ]*\\)"
+        DEAD_BRANCH "${BODY}")
+    if(DEAD_BRANCH)
+        message(FATAL_ERROR "${LABEL} must not hide required calls in constant-false code")
+    endif()
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
+endfunction()
+
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
@@ -53,33 +92,211 @@ if(NOT BAR_LABEL_USE_COUNT EQUAL 2)
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
+require_no_uncalled_lambda("${ACTIVE_DRAW_CODE}" "ActiveSkillRenderer::draw_hud")
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
         message(FATAL_ERROR
-            "ActiveSkillRenderer::draw_hud must contain exactly one required model draw call")
+            "ActiveSkillRenderer::draw_hud must reject invalid layout before its first draw")
     endif()
 endforeach()
-foreach(MODEL_TOKEN IN ITEMS
-        "slot.key_label.data()" "slot.cooldown_label.data()")
-    string(REGEX REPLACE "([][+.*()^$?\\\\|])" "\\\\\\1"
-        TOKEN_REGEX "${MODEL_TOKEN}")
-    string(REGEX MATCHALL "${TOKEN_REGEX}" MODEL_USES "${ACTIVE_DRAW_BODY}")
-    list(LENGTH MODEL_USES MODEL_USE_COUNT)
-    if(NOT MODEL_USE_COUNT EQUAL 1)
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
         message(FATAL_ERROR
-            "ActiveSkillRenderer::draw_hud model text must have one real use")
+            "Active-skill artwork must finish before the text plan is made")
+    endif()
+    if(ARTWORK_DRAW_INDEX GREATER LAST_ARTWORK_DRAW_INDEX)
+        set(LAST_ARTWORK_DRAW_INDEX ${ARTWORK_DRAW_INDEX})
+    endif()
+endforeach()
+if(NOT LAST_ARTWORK_DRAW_INDEX LESS TEXT_PLAN_INDEX)
+    message(FATAL_ERROR
+        "The last active-skill artwork draw must precede the text plan")
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
+        message(FATAL_ERROR
+            "Active-skill key, cooldown, and name plans must draw in order")
+    endif()
+    set(LAST_TEXT_DRAW_INDEX ${PLATE_DRAW_INDEX})
+endforeach()
+
+extract_cpp_function_body("${ACTIVE_SOURCE}"
+    "void draw_active_skill_text_plate(Font font,"
+    ACTIVE_PLATE_BODY ACTIVE_PLATE_CODE)
+require_real_code("${ACTIVE_PLATE_CODE}" "Active-skill text plate helper")
+require_no_uncalled_lambda("${ACTIVE_PLATE_CODE}" "Active-skill text plate helper")
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
+require_no_uncalled_lambda("${TEXT_RENDERER_CODE}" "draw_crisp_ui_text")
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
+require_no_uncalled_lambda("${TEXT_AUDIT_MEASURED_CODE}" "UI text measured bounds")
+require_no_uncalled_lambda("${TEXT_AUDIT_RECORD_CODE}" "record_ui_text_bounds")
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
+require_no_uncalled_lambda("${ACTIVE_MEASURE_CODE}" "Active-skill MeasureTextEx adapter")
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
+        message(FATAL_ERROR
+            "ActiveSkillRenderer::draw_hud must not use ${FORBIDDEN_ACTIVE_TOKEN}")
     endif()
 endforeach()
 
 foreach(DRAW_BODY IN ITEMS HUD_DRAW_BODY ACTIVE_DRAW_BODY)
     foreach(FORBIDDEN IN ITEMS
             "TextFormat(" "snprintf(" "\"Num" "\"HP" "\"XP" "\"LV"
@@ -90,18 +307,14 @@ foreach(DRAW_BODY IN ITEMS HUD_DRAW_BODY ACTIVE_DRAW_BODY)
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
```

