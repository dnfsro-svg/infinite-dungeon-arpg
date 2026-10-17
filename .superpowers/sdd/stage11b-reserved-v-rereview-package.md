# Review package: 8ef1438..a03bf2e

## Commits
a03bf2e test: restore binding swap stress

## Files changed
 tests/settings/settings_types_tests.cpp | 35 ++++++++++++++-------------------
 1 file changed, 15 insertions(+), 20 deletions(-)

## Diff
diff --git a/tests/settings/settings_types_tests.cpp b/tests/settings/settings_types_tests.cpp
index a059c65..277f4b8 100644
--- a/tests/settings/settings_types_tests.cpp
+++ b/tests/settings/settings_types_tests.cpp
@@ -139,58 +139,53 @@ arpg::test::Failure reserved_v_assignment_is_rejected_without_mutation() noexcep
     settings.vsync_enabled = false;
     settings.revision = 42U;
     const SettingsData before = settings;
 
     ARPG_REQUIRE(!arpg::settings::assign_or_swap(
         settings, SettingAction::light_attack, StableKey::v));
     ARPG_REQUIRE(same_settings(settings, before));
     return {};
 }
 
-arpg::test::Failure repeated_swaps_preserve_uniqueness() noexcept {
-    constexpr std::array<StableKey, 44> assignable_keys{
-        StableKey::a, StableKey::b, StableKey::c, StableKey::d, StableKey::e,
-        StableKey::f, StableKey::g, StableKey::h, StableKey::i, StableKey::j,
-        StableKey::k, StableKey::l, StableKey::m, StableKey::n, StableKey::o,
-        StableKey::p, StableKey::q, StableKey::r, StableKey::s, StableKey::t,
-        StableKey::u, StableKey::w, StableKey::x, StableKey::y, StableKey::z,
-        StableKey::digit_0, StableKey::digit_1, StableKey::digit_2,
-        StableKey::digit_3, StableKey::digit_4, StableKey::digit_5,
-        StableKey::digit_6, StableKey::digit_7, StableKey::digit_8,
-        StableKey::digit_9, StableKey::arrow_up, StableKey::arrow_down,
-        StableKey::arrow_left, StableKey::arrow_right, StableKey::space,
-        StableKey::left_shift, StableKey::right_shift,
-        StableKey::left_control, StableKey::right_control};
+arpg::test::Failure repeated_swaps_preserve_exact_permutation() noexcept {
     SettingsData settings = arpg::settings::default_settings();
     for (std::size_t iteration = 0; iteration < 1000U; ++iteration) {
         const std::size_t target = (iteration * 7U + 3U) % actions.size();
-        const StableKey candidate = assignable_keys[(iteration * 3U + 1U) % assignable_keys.size()];
+        const std::size_t owner =
+            (target + 1U + (iteration * 3U) % (actions.size() - 1U)) % actions.size();
+        ARPG_REQUIRE(owner != target);
+
+        const SettingsData before = settings;
+        const StableKey target_old_key = before.bindings[target];
+        const StableKey candidate = before.bindings[owner];
         ARPG_REQUIRE(candidate != StableKey::v);
         ARPG_REQUIRE(arpg::settings::assign_or_swap(settings, actions[target], candidate));
-        ARPG_REQUIRE(arpg::settings::validate_settings(settings) == SettingsValidationError::none);
-        for (std::size_t lhs = 0; lhs < actions.size(); ++lhs) {
-            for (std::size_t rhs = lhs + 1U; rhs < actions.size(); ++rhs) {
-                ARPG_REQUIRE(settings.bindings[lhs] != settings.bindings[rhs]);
+        ARPG_REQUIRE(settings.bindings[target] == candidate);
+        ARPG_REQUIRE(settings.bindings[owner] == target_old_key);
+        for (std::size_t index = 0; index < actions.size(); ++index) {
+            if (index != target && index != owner) {
+                ARPG_REQUIRE(settings.bindings[index] == before.bindings[index]);
             }
         }
+        ARPG_REQUIRE(arpg::settings::validate_settings(settings) == SettingsValidationError::none);
     }
     return {};
 }
 
 constexpr arpg::test::TestCase cases[] = {
     {"defaults are stable", defaults_are_stable},
     {"action labels are present and distinct", action_labels_are_present_and_distinct},
     {"volume outside range is rejected", rejects_volume_out_of_range},
     {"volume outside five-percent step is rejected", rejects_volume_off_step},
     {"invalid window mode is rejected", rejects_invalid_window_mode},
     {"invalid and duplicate keys are rejected", rejects_invalid_and_duplicate_keys},
     {"reserved V is rejected in every binding", rejects_reserved_v_in_every_binding},
     {"unoccupied key assignment succeeds", assigns_an_unoccupied_key},
     {"occupied key assignment swaps owners", swaps_an_occupied_key},
     {"reserved V assignment leaves settings unchanged", reserved_v_assignment_is_rejected_without_mutation},
-    {"one thousand swaps preserve uniqueness", repeated_swaps_preserve_uniqueness}};
+    {"one thousand occupied-key swaps preserve exact permutation", repeated_swaps_preserve_exact_permutation}};
 
 }  // namespace
 
 arpg::test::TestSuite settings_types_suite() noexcept {
     return arpg::test::make_suite("settings.types", cases);
 }
