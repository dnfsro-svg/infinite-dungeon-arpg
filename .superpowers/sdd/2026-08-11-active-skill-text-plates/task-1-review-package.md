# Task 1 Review Package

## Range

- Base: `7fa00a3`
- Head: `c33317a11c6b264dd0f7021985c1570ad1783940`
- Commit: `c33317a feat(ui): plan centered active skill labels`

## Stat

```text
src/platform/raylib/active_skill_view.cpp    | 119 ++++++++++++++++++++++
src/platform/raylib/active_skill_view.hpp    |  43 ++++++++
src/platform/raylib/ui_text_bounds_audit.hpp |   2 +
tests/platform/active_skill_view_tests.cpp   | 145 ++++++++++++++++++++++++++-
4 files changed, 308 insertions(+), 1 deletion(-)
```

## Full diff

```diff
diff --git a/src/platform/raylib/active_skill_view.cpp b/src/platform/raylib/active_skill_view.cpp
index d57401b..e2e2c1c 100644
--- a/src/platform/raylib/active_skill_view.cpp
+++ b/src/platform/raylib/active_skill_view.cpp
@@ -1,27 +1,79 @@
 #include "active_skill_view.hpp"
 
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
 
 }  // namespace
 
 MaterialSpriteId active_skill_icon_sprite(skills::ActiveSkillId id) noexcept {
@@ -76,28 +128,95 @@ ActiveSkillHudModel make_active_skill_hud_model(
             }
         }
     }
     return result;
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
+    if (!result.name.visible) return {};
+    if (slot.cooldown_ratio <= 0.0F || slot.cooldown_label[0U] == '\0') {
+        return result;
+    }
+
+    result.cooldown = make_text_plate_plan(slot.cooldown_label.data(),
+        regions.cooldown, scale, UiTextAuditRole::hud_skill_cooldown,
+        measure, measure_context);
+    if (!result.cooldown.visible) return {};
     return result;
 }
 
 }  // namespace arpg::platform
diff --git a/src/platform/raylib/active_skill_view.hpp b/src/platform/raylib/active_skill_view.hpp
index 2b350eb..c3639e3 100644
--- a/src/platform/raylib/active_skill_view.hpp
+++ b/src/platform/raylib/active_skill_view.hpp
@@ -1,14 +1,16 @@
 #pragma once
 
 #include "material_asset_types.hpp"
 #include "skills/active_skill_types.hpp"
+#include "ui_text_bounds_audit.hpp"
+#include "ui_text_layout.hpp"
 
 #include <raylib.h>
 
 #include <array>
 #include <cstdint>
 
 namespace arpg::platform {
 
 struct ActiveSkillHudSlot final {
     std::uint8_t key_number{};
@@ -22,24 +24,65 @@ struct ActiveSkillHudSlot final {
 };
 
 [[nodiscard]] MaterialSpriteId active_skill_icon_sprite(
     skills::ActiveSkillId id) noexcept;
 
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
diff --git a/src/platform/raylib/ui_text_bounds_audit.hpp b/src/platform/raylib/ui_text_bounds_audit.hpp
index 8713254..16813d5 100644
--- a/src/platform/raylib/ui_text_bounds_audit.hpp
+++ b/src/platform/raylib/ui_text_bounds_audit.hpp
@@ -31,20 +31,22 @@ enum class UiTextAuditRole : std::uint8_t {
     inventory_material_entry,
     inventory_detail,
     inventory_status,
     skill_page_title,
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
     std::uint64_t observed_roles{};
     std::uint64_t failed_bounds_roles{};
     std::uint64_t failed_size_roles{};
     std::uint32_t measured_text_count{};
diff --git a/tests/platform/active_skill_view_tests.cpp b/tests/platform/active_skill_view_tests.cpp
index 39125ed..902b376 100644
--- a/tests/platform/active_skill_view_tests.cpp
+++ b/tests/platform/active_skill_view_tests.cpp
@@ -1,33 +1,55 @@
 #include "test_framework.hpp"
 #include "allocation_probe.hpp"
 
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
 template <typename Slot, typename = void>
 struct SkillKeyLabelContract final {
     [[nodiscard]] static bool matches(
         const Slot&, const char*) noexcept {
         return false;
     }
 };
 
 template <typename Slot>
 struct SkillKeyLabelContract<Slot,
@@ -231,53 +253,174 @@ arpg::test::Failure hud_cooldown_ratios_are_clamped_and_empty_slots_stay_zero()
         == 0);
     ARPG_REQUIRE(platform::ui_utf8_is_valid(
         view.slots[0U].cooldown_label.data()));
     ARPG_REQUIRE(std::strstr(view.slots[0U].cooldown_label.data(), "s")
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
+    static_cast<void>(platform::make_active_skill_hud_text_plan(view.slots[2U],
+        regions, layout.scale, true, &measure_skill_text, nullptr));
+    static_cast<void>(platform::make_active_skill_hud_text_plan(view.slots[1U],
+        regions, layout.scale, true, &measure_skill_text, nullptr));
+    static_cast<void>(platform::make_active_skill_hud_text_plan(view.slots[0U],
+        regions, layout.scale, true, &measure_skill_text, nullptr));
+    const std::uint64_t allocations_before = arpg::test::allocation_count();
+    for (std::size_t iteration = 0U; iteration < 10'000U; ++iteration) {
+        const platform::ActiveSkillHudLayout repeated_layout =
+            platform::active_skill_hud_layout(800, 450);
+        static_cast<void>(platform::make_active_skill_hud_text_plan(
+            view.slots[2U], repeated_layout.text_regions[0U],
+            repeated_layout.scale, true, &measure_skill_text, nullptr));
+        static_cast<void>(platform::make_active_skill_hud_text_plan(
+            view.slots[1U], repeated_layout.text_regions[0U],
+            repeated_layout.scale, true, &measure_skill_text, nullptr));
+        static_cast<void>(platform::make_active_skill_hud_text_plan(
+            view.slots[0U], repeated_layout.text_regions[0U],
+            repeated_layout.scale, true, &measure_skill_text, nullptr));
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
         const float scale = platform::ui_viewport_scale(
             viewport[0], viewport[1]);
         const float expected_width = (5.0F * 76.0F + 4.0F * 8.0F) * scale;
         const platform::ActiveSkillHudLayout layout =
             platform::active_skill_hud_layout(viewport[0], viewport[1]);
+        ARPG_REQUIRE(layout.valid);
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
     combat::CombatSnapshot snapshot{};
     snapshot.player.facing = combat::Facing::left;
     snapshot.active_skill.id = skills::ActiveSkillId::draw_slash;
     snapshot.active_skill.phase = combat::ActiveSkillPhase::strikes;
```
