# Review package: 9a6f955..19b04af

## Commits
19b04af feat: define stage 11b settings schema

## Files changed
 CMakeLists.txt                           |   2 +
 src/platform/settings/CMakeLists.txt     |   6 ++
 src/platform/settings/settings_types.cpp | 103 +++++++++++++++++++++
 src/platform/settings/settings_types.hpp | 103 +++++++++++++++++++++
 tests/settings/CMakeLists.txt            |  13 +++
 tests/settings/settings_test_main.cpp    |   8 ++
 tests/settings/settings_types_tests.cpp  | 148 +++++++++++++++++++++++++++++++
 7 files changed, 383 insertions(+)

## Diff
diff --git a/CMakeLists.txt b/CMakeLists.txt
index 7240a5e..e41ca02 100644
--- a/CMakeLists.txt
+++ b/CMakeLists.txt
@@ -42,29 +42,31 @@ endfunction()
 include(CTest)
 add_subdirectory(src/core)
 add_subdirectory(src/abyss)
 add_subdirectory(src/combat)
 add_subdirectory(src/progression)
 add_subdirectory(src/modifiers)
 add_subdirectory(src/items)
 add_subdirectory(src/passives)
 add_subdirectory(src/dungeon)
 add_subdirectory(src/persistence)
+add_subdirectory(src/platform/settings)
 
 if(ARPG_BUILD_GRAPHICS)
     include(cmake/Dependencies.cmake)
     add_subdirectory(src/platform/raylib)
     add_subdirectory(src/app)
 endif()
 
 if(BUILD_TESTING)
     add_subdirectory(tests/core)
     add_subdirectory(tests/abyss)
     add_subdirectory(tests/combat)
     add_subdirectory(tests/dungeon)
     add_subdirectory(tests/persistence)
     add_subdirectory(tests/platform)
     add_subdirectory(tests/progression)
     add_subdirectory(tests/modifiers)
     add_subdirectory(tests/passives)
     add_subdirectory(tests/items)
+    add_subdirectory(tests/settings)
 endif()
diff --git a/src/platform/settings/CMakeLists.txt b/src/platform/settings/CMakeLists.txt
new file mode 100644
index 0000000..5799650
--- /dev/null
+++ b/src/platform/settings/CMakeLists.txt
@@ -0,0 +1,6 @@
+add_library(arpg_settings STATIC
+    settings_types.cpp)
+
+target_include_directories(arpg_settings PUBLIC "${PROJECT_SOURCE_DIR}/src")
+target_compile_features(arpg_settings PUBLIC cxx_std_17)
+arpg_enable_project_warnings(arpg_settings)
diff --git a/src/platform/settings/settings_types.cpp b/src/platform/settings/settings_types.cpp
new file mode 100644
index 0000000..121bf23
--- /dev/null
+++ b/src/platform/settings/settings_types.cpp
@@ -0,0 +1,103 @@
+#include "platform/settings/settings_types.hpp"
+
+#include <array>
+#include <cstddef>
+
+namespace arpg::settings {
+namespace {
+
+constexpr std::size_t action_count = static_cast<std::size_t>(SettingAction::count);
+
+constexpr std::array<StableKey, action_count> default_bindings{
+    StableKey::w,
+    StableKey::s,
+    StableKey::a,
+    StableKey::d,
+    StableKey::j,
+    StableKey::k,
+    StableKey::l,
+    StableKey::e,
+    StableKey::i,
+    StableKey::p};
+
+constexpr std::array<const char*, action_count> action_labels{
+    "Move Up",
+    "Move Down",
+    "Move Left",
+    "Move Right",
+    "Light Attack",
+    "Jump",
+    "Launcher",
+    "Interact",
+    "Inventory",
+    "Passive Tree"};
+
+[[nodiscard]] constexpr std::size_t action_index(SettingAction action) noexcept {
+    return static_cast<std::size_t>(action);
+}
+
+[[nodiscard]] constexpr bool valid_key(StableKey key) noexcept {
+    return static_cast<std::size_t>(key) < static_cast<std::size_t>(StableKey::count);
+}
+
+}  // namespace
+
+SettingsData default_settings() noexcept {
+    SettingsData settings{};
+    settings.bindings = default_bindings;
+    return settings;
+}
+
+SettingsValidationError validate_settings(const SettingsData& settings) noexcept {
+    if (settings.master_sfx_percent > 100U) {
+        return SettingsValidationError::volume_range;
+    }
+    if (settings.master_sfx_percent % 5U != 0U) {
+        return SettingsValidationError::volume_step;
+    }
+    if (settings.window_mode != WindowMode::windowed &&
+        settings.window_mode != WindowMode::fullscreen) {
+        return SettingsValidationError::window_mode;
+    }
+
+    for (std::size_t index = 0; index < settings.bindings.size(); ++index) {
+        if (!valid_key(settings.bindings[index])) {
+            return SettingsValidationError::key_range;
+        }
+        for (std::size_t other = index + 1U; other < settings.bindings.size(); ++other) {
+            if (settings.bindings[index] == settings.bindings[other]) {
+                return SettingsValidationError::duplicate_key;
+            }
+        }
+    }
+    return SettingsValidationError::none;
+}
+
+StableKey binding_for(const SettingsData& settings, SettingAction action) noexcept {
+    const std::size_t index = action_index(action);
+    return index < settings.bindings.size() ? settings.bindings[index] : StableKey::count;
+}
+
+bool assign_or_swap(SettingsData& settings, SettingAction action, StableKey key) noexcept {
+    const std::size_t target = action_index(action);
+    if (target >= settings.bindings.size() || !valid_key(key)) {
+        return false;
+    }
+
+    const StableKey old_key = settings.bindings[target];
+    for (std::size_t owner = 0; owner < settings.bindings.size(); ++owner) {
+        if (owner != target && settings.bindings[owner] == key) {
+            settings.bindings[owner] = old_key;
+            break;
+        }
+    }
+    settings.bindings[target] = key;
+    return true;
+}
+
+const char* action_label(SettingAction action) noexcept {
+    const std::size_t index = action_index(action);
+    return index < action_labels.size() ? action_labels[index] : "Unknown";
+}
+
+}  // namespace arpg::settings
diff --git a/src/platform/settings/settings_types.hpp b/src/platform/settings/settings_types.hpp
new file mode 100644
index 0000000..e5b649b
--- /dev/null
+++ b/src/platform/settings/settings_types.hpp
@@ -0,0 +1,103 @@
+#pragma once
+
+#include <array>
+#include <cstddef>
+#include <cstdint>
+
+namespace arpg::settings {
+
+enum class SettingAction : std::uint8_t {
+    move_up,
+    move_down,
+    move_left,
+    move_right,
+    light_attack,
+    jump,
+    launcher,
+    interact,
+    inventory,
+    passive_tree,
+    count
+};
+
+enum class StableKey : std::uint8_t {
+    a,
+    b,
+    c,
+    d,
+    e,
+    f,
+    g,
+    h,
+    i,
+    j,
+    k,
+    l,
+    m,
+    n,
+    o,
+    p,
+    q,
+    r,
+    s,
+    t,
+    u,
+    v,
+    w,
+    x,
+    y,
+    z,
+    digit_0,
+    digit_1,
+    digit_2,
+    digit_3,
+    digit_4,
+    digit_5,
+    digit_6,
+    digit_7,
+    digit_8,
+    digit_9,
+    arrow_up,
+    arrow_down,
+    arrow_left,
+    arrow_right,
+    space,
+    left_shift,
+    right_shift,
+    left_control,
+    right_control,
+    count
+};
+
+enum class WindowMode : std::uint8_t {
+    windowed,
+    fullscreen
+};
+
+struct SettingsData final {
+    std::uint8_t master_sfx_percent{100};
+    WindowMode window_mode{WindowMode::windowed};
+    bool vsync_enabled{true};
+    std::array<StableKey, static_cast<std::size_t>(SettingAction::count)> bindings{};
+    std::uint64_t revision{};
+};
+
+enum class SettingsValidationError : std::uint8_t {
+    none,
+    volume_range,
+    volume_step,
+    window_mode,
+    key_range,
+    duplicate_key
+};
+
+[[nodiscard]] SettingsData default_settings() noexcept;
+[[nodiscard]] SettingsValidationError validate_settings(const SettingsData& settings) noexcept;
+[[nodiscard]] StableKey binding_for(const SettingsData& settings, SettingAction action) noexcept;
+[[nodiscard]] bool assign_or_swap(
+    SettingsData& settings,
+    SettingAction action,
+    StableKey key) noexcept;
+[[nodiscard]] const char* action_label(SettingAction action) noexcept;
+
+}  // namespace arpg::settings
diff --git a/tests/settings/CMakeLists.txt b/tests/settings/CMakeLists.txt
new file mode 100644
index 0000000..4b60560
--- /dev/null
+++ b/tests/settings/CMakeLists.txt
@@ -0,0 +1,13 @@
+add_executable(arpg_settings_tests
+    settings_test_main.cpp
+    settings_types_tests.cpp)
+
+target_include_directories(arpg_settings_tests PRIVATE
+    "${PROJECT_SOURCE_DIR}/tests/core"
+    "${PROJECT_SOURCE_DIR}/src")
+target_compile_features(arpg_settings_tests PRIVATE cxx_std_17)
+target_link_libraries(arpg_settings_tests PRIVATE arpg_settings)
+arpg_enable_project_warnings(arpg_settings_tests)
+
+add_test(NAME settings.units COMMAND arpg_settings_tests)
+set_tests_properties(settings.units PROPERTIES LABELS "headless;settings")
diff --git a/tests/settings/settings_test_main.cpp b/tests/settings/settings_test_main.cpp
new file mode 100644
index 0000000..e5f11ef
--- /dev/null
+++ b/tests/settings/settings_test_main.cpp
@@ -0,0 +1,8 @@
+#include "test_framework.hpp"
+
+arpg::test::TestSuite settings_types_suite() noexcept;
+
+int main() {
+    const arpg::test::TestSuite suites[] = {settings_types_suite()};
+    return arpg::test::run_suites(suites, 9, "stage 11b task 1 settings");
+}
diff --git a/tests/settings/settings_types_tests.cpp b/tests/settings/settings_types_tests.cpp
new file mode 100644
index 0000000..8d26cb3
--- /dev/null
+++ b/tests/settings/settings_types_tests.cpp
@@ -0,0 +1,148 @@
+#include "test_framework.hpp"
+
+#include "platform/settings/settings_types.hpp"
+
+#include <array>
+#include <cstddef>
+#include <cstdint>
+#include <cstring>
+
+namespace {
+
+using arpg::settings::SettingAction;
+using arpg::settings::SettingsData;
+using arpg::settings::SettingsValidationError;
+using arpg::settings::StableKey;
+using arpg::settings::WindowMode;
+
+constexpr std::array<SettingAction, 10> actions{
+    SettingAction::move_up,
+    SettingAction::move_down,
+    SettingAction::move_left,
+    SettingAction::move_right,
+    SettingAction::light_attack,
+    SettingAction::jump,
+    SettingAction::launcher,
+    SettingAction::interact,
+    SettingAction::inventory,
+    SettingAction::passive_tree};
+
+arpg::test::Failure defaults_are_stable() noexcept {
+    const SettingsData settings = arpg::settings::default_settings();
+    const std::array<StableKey, 10> expected{
+        StableKey::w, StableKey::s, StableKey::a, StableKey::d, StableKey::j,
+        StableKey::k, StableKey::l, StableKey::e, StableKey::i, StableKey::p};
+
+    ARPG_REQUIRE(settings.master_sfx_percent == 100U);
+    ARPG_REQUIRE(settings.window_mode == WindowMode::windowed);
+    ARPG_REQUIRE(settings.vsync_enabled);
+    ARPG_REQUIRE(settings.revision == 0U);
+    ARPG_REQUIRE(settings.bindings == expected);
+    for (std::size_t index = 0; index < actions.size(); ++index) {
+        ARPG_REQUIRE(arpg::settings::binding_for(settings, actions[index]) == expected[index]);
+    }
+    ARPG_REQUIRE(arpg::settings::validate_settings(settings) == SettingsValidationError::none);
+    return {};
+}
+
+arpg::test::Failure action_labels_are_present_and_distinct() noexcept {
+    for (std::size_t lhs = 0; lhs < actions.size(); ++lhs) {
+        const char* label = arpg::settings::action_label(actions[lhs]);
+        ARPG_REQUIRE(label != nullptr);
+        ARPG_REQUIRE(label[0] != '\0');
+        for (std::size_t rhs = lhs + 1U; rhs < actions.size(); ++rhs) {
+            ARPG_REQUIRE(std::strcmp(label, arpg::settings::action_label(actions[rhs])) != 0);
+        }
+    }
+    return {};
+}
+
+arpg::test::Failure rejects_volume_out_of_range() noexcept {
+    SettingsData settings = arpg::settings::default_settings();
+    settings.master_sfx_percent = 101U;
+    ARPG_REQUIRE(arpg::settings::validate_settings(settings) ==
+        SettingsValidationError::volume_range);
+    return {};
+}
+
+arpg::test::Failure rejects_volume_off_step() noexcept {
+    SettingsData settings = arpg::settings::default_settings();
+    settings.master_sfx_percent = 99U;
+    ARPG_REQUIRE(arpg::settings::validate_settings(settings) ==
+        SettingsValidationError::volume_step);
+    return {};
+}
+
+arpg::test::Failure rejects_invalid_window_mode() noexcept {
+    SettingsData settings = arpg::settings::default_settings();
+    settings.window_mode = static_cast<WindowMode>(2U);
+    ARPG_REQUIRE(arpg::settings::validate_settings(settings) ==
+        SettingsValidationError::window_mode);
+    return {};
+}
+
+arpg::test::Failure rejects_invalid_and_duplicate_keys() noexcept {
+    SettingsData settings = arpg::settings::default_settings();
+    settings.bindings[0] = StableKey::count;
+    ARPG_REQUIRE(arpg::settings::validate_settings(settings) ==
+        SettingsValidationError::key_range);
+
+    settings = arpg::settings::default_settings();
+    settings.bindings[0] = settings.bindings[1];
+    ARPG_REQUIRE(arpg::settings::validate_settings(settings) ==
+        SettingsValidationError::duplicate_key);
+    return {};
+}
+
+arpg::test::Failure assigns_an_unoccupied_key() noexcept {
+    SettingsData settings = arpg::settings::default_settings();
+    ARPG_REQUIRE(arpg::settings::assign_or_swap(
+        settings, SettingAction::move_up, StableKey::z));
+    ARPG_REQUIRE(arpg::settings::binding_for(settings, SettingAction::move_up) == StableKey::z);
+    ARPG_REQUIRE(arpg::settings::validate_settings(settings) == SettingsValidationError::none);
+    return {};
+}
+
+arpg::test::Failure swaps_an_occupied_key() noexcept {
+    SettingsData settings = arpg::settings::default_settings();
+    ARPG_REQUIRE(arpg::settings::assign_or_swap(
+        settings, SettingAction::move_up, StableKey::s));
+    ARPG_REQUIRE(arpg::settings::binding_for(settings, SettingAction::move_up) == StableKey::s);
+    ARPG_REQUIRE(arpg::settings::binding_for(settings, SettingAction::move_down) == StableKey::w);
+    ARPG_REQUIRE(arpg::settings::validate_settings(settings) == SettingsValidationError::none);
+    return {};
+}
+
+arpg::test::Failure repeated_swaps_preserve_uniqueness() noexcept {
+    SettingsData settings = arpg::settings::default_settings();
+    for (std::size_t iteration = 0; iteration < 1000U; ++iteration) {
+        const std::size_t target = (iteration * 7U + 3U) % actions.size();
+        const std::size_t owner = (iteration * 3U + 1U) % actions.size();
+        const StableKey occupied = arpg::settings::binding_for(settings, actions[owner]);
+        ARPG_REQUIRE(arpg::settings::assign_or_swap(settings, actions[target], occupied));
+        ARPG_REQUIRE(arpg::settings::validate_settings(settings) == SettingsValidationError::none);
+        for (std::size_t lhs = 0; lhs < actions.size(); ++lhs) {
+            for (std::size_t rhs = lhs + 1U; rhs < actions.size(); ++rhs) {
+                ARPG_REQUIRE(settings.bindings[lhs] != settings.bindings[rhs]);
+            }
+        }
+    }
+    return {};
+}
+
+constexpr arpg::test::TestCase cases[] = {
+    {"defaults are stable", defaults_are_stable},
+    {"action labels are present and distinct", action_labels_are_present_and_distinct},
+    {"volume outside range is rejected", rejects_volume_out_of_range},
+    {"volume outside five-percent step is rejected", rejects_volume_off_step},
+    {"invalid window mode is rejected", rejects_invalid_window_mode},
+    {"invalid and duplicate keys are rejected", rejects_invalid_and_duplicate_keys},
+    {"unoccupied key assignment succeeds", assigns_an_unoccupied_key},
+    {"occupied key assignment swaps owners", swaps_an_occupied_key},
+    {"one thousand swaps preserve uniqueness", repeated_swaps_preserve_uniqueness}};
+
+}  // namespace
+
+arpg::test::TestSuite settings_types_suite() noexcept {
+    return arpg::test::make_suite("settings.types", cases);
+}
