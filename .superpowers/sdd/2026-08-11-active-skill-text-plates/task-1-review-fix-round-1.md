# Task 1 Review Fix Round 1 Package

## Range

- Previous head: `c33317a11c6b264dd0f7021985c1570ad1783940`
- Fix head: `31da080ec9f78d4333961c3cd6a8e45d79cedda5`
- Commit: `31da080 fix(ui): harden active skill text plan`

## Stat

```text
src/platform/raylib/active_skill_view.cpp  |   5 +-
tests/platform/active_skill_view_tests.cpp | 178 +++++++++++++++++++++++++++--
2 files changed, 171 insertions(+), 12 deletions(-)
```

## Production diff

```diff
diff --git a/src/platform/raylib/active_skill_view.cpp b/src/platform/raylib/active_skill_view.cpp
index e2e2c1c..9df6f7f 100644
--- a/src/platform/raylib/active_skill_view.cpp
+++ b/src/platform/raylib/active_skill_view.cpp
@@ -202,21 +202,20 @@ ActiveSkillHudTextPlan make_active_skill_hud_text_plan(
     ActiveSkillHudTextPlan result{};
     result.key = make_text_plate_plan(slot.key_label.data(), regions.key,
         scale, UiTextAuditRole::hud_skill_key, measure, measure_context);
     if (!result.key.visible) return {};
     if (slot.empty) return result;
 
     result.name = make_text_plate_plan(slot.name.data(), regions.name, scale,
         UiTextAuditRole::hud_skill_name, measure, measure_context);
-    if (!result.name.visible) return {};
-    if (slot.cooldown_ratio <= 0.0F || slot.cooldown_label[0U] == '\0') {
+    if (!finite(slot.cooldown_ratio) || slot.cooldown_ratio <= 0.0F
+            || slot.cooldown_label[0U] == '\0') {
         return result;
     }
 
     result.cooldown = make_text_plate_plan(slot.cooldown_label.data(),
         regions.cooldown, scale, UiTextAuditRole::hud_skill_cooldown,
         measure, measure_context);
-    if (!result.cooldown.visible) return {};
     return result;
 }
```

## Test coverage added

- NaN, positive infinity, and negative infinity keep key/name and suppress cooldown.
- Stage-aware measurement independently fails key, name, or cooldown and asserts call order/partial preservation.
- All five key labels, both equipped names, and `30秒` are copied exactly without truncation and use the matching backing region.
- The 10,000-iteration allocation loop now constructs and asserts real empty, ready, and cooling plans.
- 800x450, 1280x720, and 1920x1080 assert literal scale plus every region x/y/width/height formula for every slot.

The complete test diff is the committed range `c33317a..31da080`; the full test body and exact RED/GREEN evidence are recorded in `task-1-report.md`.
