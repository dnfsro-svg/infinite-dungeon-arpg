# Review package: a03bf2e..bb80cf7

## Commits
bb80cf7 feat: map configurable keys to frame input

## Files changed
 src/platform/raylib/CMakeLists.txt           |   6 +-
 src/platform/raylib/combat_key_bindings.hpp  |  25 --
 src/platform/raylib/host_input.cpp           | 175 ++++++++++++
 src/platform/raylib/host_input.hpp           |  59 ++++
 src/platform/raylib/raylib_host.cpp          |  59 +---
 src/platform/raylib/raylib_input.hpp         |   3 +
 src/platform/raylib/stable_key_raylib.cpp    |  85 ++++++
 src/platform/raylib/stable_key_raylib.hpp    |  14 +
 tests/platform/CMakeLists.txt                |  11 +-
 tests/platform/combat_key_bindings_tests.cpp |  49 ----
 tests/platform/host_input_source_test.cmake  |  63 +++++
 tests/platform/host_input_tests.cpp          | 402 +++++++++++++++++++++++++++
 tests/platform/platform_test_main.cpp        |   6 +-
 13 files changed, 825 insertions(+), 132 deletions(-)

## Diff
diff --git a/src/platform/raylib/CMakeLists.txt b/src/platform/raylib/CMakeLists.txt
index 6fe62f4..cbae86a 100644
--- a/src/platform/raylib/CMakeLists.txt
+++ b/src/platform/raylib/CMakeLists.txt
@@ -3,34 +3,36 @@ add_library(arpg_raylib STATIC
     combat_audio.cpp
     combat_feedback.cpp
     combat_renderer.cpp
     combat_view_math.cpp
     death_overlay_font.cpp
     death_overlay_renderer.cpp
     death_overlay_view.cpp
     debug_renderer.cpp
     dungeon_view_math.cpp
     dungeon_runtime.cpp
+    host_input.cpp
     host_launch_options.cpp
     hud_renderer.cpp
     inventory_renderer.cpp
     inventory_view_math.cpp
     passive_tree_renderer.cpp
     passive_tree_view_math.cpp
     raylib_input.cpp
     raylib_host.cpp
-    room_renderer.cpp)
+    room_renderer.cpp
+    stable_key_raylib.cpp)
 
 target_include_directories(arpg_raylib
     PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
 target_compile_features(arpg_raylib PUBLIC cxx_std_17)
 target_link_libraries(arpg_raylib
     PUBLIC arpg_dungeon arpg_persistence raylib
-    PRIVATE arpg_core)
+    PRIVATE arpg_core arpg_settings)
 if(WIN32)
     target_link_libraries(arpg_raylib PRIVATE user32)
 endif()
 
 arpg_enable_project_warnings(arpg_raylib)
 if(MSVC)
     target_compile_options(arpg_raylib PRIVATE /utf-8)
 endif()
diff --git a/src/platform/raylib/combat_key_bindings.hpp b/src/platform/raylib/combat_key_bindings.hpp
deleted file mode 100644
index 9e45aae..0000000
--- a/src/platform/raylib/combat_key_bindings.hpp
+++ /dev/null
@@ -1,25 +0,0 @@
-#pragma once
-
-#include "combat/input_buffer.hpp"
-
-#include <raylib.h>
-
-#include <array>
-
-namespace arpg::platform {
-
-struct CombatKeyBinding final {
-    int key{};
-    combat::Action action{};
-};
-
-inline constexpr std::array<CombatKeyBinding, 3> kCombatKeyBindings{{
-    {KEY_J, combat::Action::light},
-    {KEY_K, combat::Action::jump},
-    {KEY_L, combat::Action::launcher},
-}};
-
-inline constexpr int kInventoryKey = KEY_I;
-inline constexpr int kPassiveOverlayKey = KEY_P;
-
-}  // namespace arpg::platform
diff --git a/src/platform/raylib/host_input.cpp b/src/platform/raylib/host_input.cpp
new file mode 100644
index 0000000..adae3d4
--- /dev/null
+++ b/src/platform/raylib/host_input.cpp
@@ -0,0 +1,175 @@
+#include "host_input.hpp"
+
+#include "dungeon/dungeon_session.hpp"
+#include "stable_key_raylib.hpp"
+
+#include <array>
+#include <cstddef>
+#include <cstdint>
+
+namespace arpg::platform {
+namespace {
+
+constexpr std::size_t kStableKeyCount =
+    static_cast<std::size_t>(settings::StableKey::count);
+
+[[nodiscard]] constexpr std::size_t stable_index(
+    settings::StableKey key) noexcept {
+    return static_cast<std::size_t>(key);
+}
+
+[[nodiscard]] bool default_pressed(void*, int key) noexcept {
+    return platform_key_pressed(key);
+}
+
+[[nodiscard]] bool default_down(void*, int key) noexcept {
+    return platform_key_down(key);
+}
+
+[[nodiscard]] bool default_mouse_left_pressed(void*) noexcept {
+    return IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
+}
+
+[[nodiscard]] Vector2 default_mouse_position(void*) noexcept {
+    return GetMousePosition();
+}
+
+[[nodiscard]] bool default_focus_lost(void*) noexcept {
+    return !IsWindowFocused();
+}
+
+[[nodiscard]] bool pressed(
+    const PhysicalKeySnapshot& snapshot,
+    settings::StableKey key) noexcept {
+    const std::size_t index = stable_index(key);
+    return index < snapshot.pressed.size() && snapshot.pressed[index];
+}
+
+[[nodiscard]] bool down(
+    const PhysicalKeySnapshot& snapshot,
+    settings::StableKey key) noexcept {
+    const std::size_t index = stable_index(key);
+    return index < snapshot.down.size() && snapshot.down[index];
+}
+
+[[nodiscard]] bool action_pressed(
+    const settings::SettingsData& settings_data,
+    const PhysicalKeySnapshot& snapshot,
+    settings::SettingAction action) noexcept {
+    return pressed(snapshot, settings::binding_for(settings_data, action));
+}
+
+[[nodiscard]] bool binding_owns(
+    const settings::SettingsData& settings_data,
+    settings::StableKey key) noexcept {
+    for (settings::StableKey binding : settings_data.bindings) {
+        if (binding == key) return true;
+    }
+    return false;
+}
+
+}  // namespace
+
+PhysicalKeySnapshot sample_physical_keys() noexcept {
+    const PhysicalKeySource source{
+        nullptr,
+        &default_pressed,
+        &default_down,
+        &default_mouse_left_pressed,
+        &default_mouse_position,
+        &default_focus_lost,
+    };
+    return sample_physical_keys(source);
+}
+
+PhysicalKeySnapshot sample_physical_keys(
+    const PhysicalKeySource& source) noexcept {
+    PhysicalKeySnapshot snapshot{};
+    if (source.pressed == nullptr || source.down == nullptr) return snapshot;
+
+    for (std::size_t index = 0U; index < kStableKeyCount; ++index) {
+        const auto key = static_cast<settings::StableKey>(index);
+        const int code = raylib_key(key);
+        snapshot.pressed[index] = source.pressed(source.context, code);
+        snapshot.down[index] = source.down(source.context, code);
+    }
+    snapshot.escape = source.pressed(source.context, KEY_ESCAPE);
+    snapshot.enter = source.pressed(source.context, KEY_ENTER);
+    snapshot.f1 = source.pressed(source.context, KEY_F1);
+    snapshot.f12 = source.pressed(source.context, KEY_F12);
+    snapshot.v = snapshot.pressed[stable_index(settings::StableKey::v)];
+    snapshot.mouse_left = source.mouse_left_pressed != nullptr
+        && source.mouse_left_pressed(source.context);
+    if (snapshot.mouse_left && source.mouse_position != nullptr) {
+        snapshot.mouse_position = source.mouse_position(source.context);
+    }
+    snapshot.focus_lost = source.focus_lost != nullptr
+        && source.focus_lost(source.context);
+    return snapshot;
+}
+
+HostFrameInput map_host_frame_input(
+    const settings::SettingsData& settings_data,
+    const PhysicalKeySnapshot& snapshot) noexcept {
+    HostFrameInput input{};
+    const int left = down(snapshot, settings::binding_for(
+        settings_data, settings::SettingAction::move_left)) ? 1 : 0;
+    const int right = down(snapshot, settings::binding_for(
+        settings_data, settings::SettingAction::move_right)) ? 1 : 0;
+    const int up = down(snapshot, settings::binding_for(
+        settings_data, settings::SettingAction::move_up)) ? 1 : 0;
+    const int down_value = down(snapshot, settings::binding_for(
+        settings_data, settings::SettingAction::move_down)) ? 1 : 0;
+    input.movement = {
+        static_cast<std::int8_t>(right - left),
+        static_cast<std::int8_t>(down_value - up),
+    };
+    input.keys.movement = input.movement.x != 0 || input.movement.y != 0;
+
+    constexpr std::array<settings::SettingAction, 3> combat_actions{{
+        settings::SettingAction::light_attack,
+        settings::SettingAction::jump,
+        settings::SettingAction::launcher,
+    }};
+    for (std::size_t index = 0U; index < combat_actions.size(); ++index) {
+        input.combat_actions[index] = action_pressed(
+            settings_data, snapshot, combat_actions[index]);
+        input.keys.attack = input.keys.attack || input.combat_actions[index];
+    }
+
+    input.keys.e = action_pressed(
+        settings_data, snapshot, settings::SettingAction::interact);
+    input.keys.inventory = action_pressed(
+        settings_data, snapshot, settings::SettingAction::inventory);
+    input.keys.passives = action_pressed(
+        settings_data, snapshot, settings::SettingAction::passive_tree);
+    input.keys.reset = pressed(snapshot, settings::StableKey::r)
+        && !binding_owns(settings_data, settings::StableKey::r);
+    input.keys.recovery = pressed(snapshot, settings::StableKey::n);
+    input.keys.escape = snapshot.escape;
+    input.keys.enter = snapshot.enter;
+    input.keys.f1 = snapshot.f1;
+    input.keys.f12 = snapshot.f12;
+    input.keys.v = snapshot.v;
+    input.keys.mouse_gameplay = snapshot.mouse_left;
+    input.keys.focus_lost = snapshot.focus_lost;
+    input.mouse_position = snapshot.mouse_position;
+    return input;
+}
+
+void submit_frame_actions(
+    dungeon::DungeonSession& session,
+    const HostFrameInput& input) noexcept {
+    constexpr std::array<combat::Action, 3> actions{{
+        combat::Action::light,
+        combat::Action::jump,
+        combat::Action::launcher,
+    }};
+    for (std::size_t index = 0U; index < actions.size(); ++index) {
+        if (input.combat_actions[index]) {
+            static_cast<void>(session.queue_action(actions[index]));
+        }
+    }
+}
+
+}  // namespace arpg::platform
diff --git a/src/platform/raylib/host_input.hpp b/src/platform/raylib/host_input.hpp
new file mode 100644
index 0000000..82086ba
--- /dev/null
+++ b/src/platform/raylib/host_input.hpp
@@ -0,0 +1,59 @@
+#pragma once
+
+#include "combat/combat_types.hpp"
+#include "platform/settings/settings_types.hpp"
+#include "raylib_input.hpp"
+
+#include <raylib.h>
+
+#include <array>
+#include <cstddef>
+
+namespace arpg::dungeon {
+class DungeonSession;
+}
+
+namespace arpg::platform {
+
+struct PhysicalKeySnapshot final {
+    std::array<bool,
+        static_cast<std::size_t>(settings::StableKey::count)> down{};
+    std::array<bool,
+        static_cast<std::size_t>(settings::StableKey::count)> pressed{};
+    bool escape{};
+    bool enter{};
+    bool f1{};
+    bool f12{};
+    bool v{};
+    bool mouse_left{};
+    bool focus_lost{};
+    Vector2 mouse_position{};
+};
+
+struct HostFrameInput final {
+    FrameKeyState keys{};
+    combat::MovementInput movement{};
+    std::array<bool, 3> combat_actions{};
+    Vector2 mouse_position{};
+};
+
+struct PhysicalKeySource final {
+    void* context{};
+    bool (*pressed)(void*, int) noexcept{};
+    bool (*down)(void*, int) noexcept{};
+    bool (*mouse_left_pressed)(void*) noexcept{};
+    Vector2 (*mouse_position)(void*) noexcept{};
+    bool (*focus_lost)(void*) noexcept{};
+};
+
+[[nodiscard]] PhysicalKeySnapshot sample_physical_keys() noexcept;
+[[nodiscard]] PhysicalKeySnapshot sample_physical_keys(
+    const PhysicalKeySource& source) noexcept;
+[[nodiscard]] HostFrameInput map_host_frame_input(
+    const settings::SettingsData& settings,
+    const PhysicalKeySnapshot& snapshot) noexcept;
+void submit_frame_actions(
+    dungeon::DungeonSession& session,
+    const HostFrameInput& input) noexcept;
+
+}  // namespace arpg::platform
diff --git a/src/platform/raylib/raylib_host.cpp b/src/platform/raylib/raylib_host.cpp
index a79fd46..9fa88db 100644
--- a/src/platform/raylib/raylib_host.cpp
+++ b/src/platform/raylib/raylib_host.cpp
@@ -1,24 +1,24 @@
 #include "raylib_host.hpp"
 
 #include "combat_audio.hpp"
 #include "combat_feedback.hpp"
-#include "combat_key_bindings.hpp"
 #include "combat_renderer.hpp"
 #include "core/fixed_step.hpp"
 #include "dungeon_runtime.hpp"
 #include "dungeon_view_math.hpp"
+#include "host_input.hpp"
 #include "inventory_renderer.hpp"
 #include "passive_tree_renderer.hpp"
 #include "passive_tree_view_math.hpp"
 #include "persistence/save_paths.hpp"
-#include "raylib_input.hpp"
+#include "platform/settings/settings_types.hpp"
 
 #include <raylib.h>
 
 #include <cstdint>
 #include <cmath>
 #include <filesystem>
 #include <optional>
 #include <string>
 
 #if !defined(RAYLIB_VERSION_MAJOR) || !defined(RAYLIB_VERSION_MINOR) \
@@ -26,68 +26,20 @@
 #error "raylib version macros are unavailable"
 #endif
 
 static_assert(RAYLIB_VERSION_MAJOR == 6, "raylib 6.0.0 is required");
 static_assert(RAYLIB_VERSION_MINOR == 0, "raylib 6.0.0 is required");
 static_assert(RAYLIB_VERSION_PATCH == 0, "raylib 6.0.0 is required");
 
 namespace arpg::platform {
 namespace {
 
-std::int8_t key_direction(int negative_key, int positive_key) noexcept {
-    const int negative = platform_key_down(negative_key) ? 1 : 0;
-    const int positive = platform_key_down(positive_key) ? 1 : 0;
-    return static_cast<std::int8_t>(positive - negative);
-}
-
-struct HostFrameInput final {
-    FrameKeyState keys{};
-    combat::MovementInput movement{};
-    std::array<bool, kCombatKeyBindings.size()> actions{};
-    Vector2 mouse_position{};
-};
-
-combat::MovementInput sample_movement_input() noexcept {
-    return {key_direction(KEY_A, KEY_D), key_direction(KEY_W, KEY_S)};
-}
-
-HostFrameInput sample_host_frame_input() noexcept {
-    HostFrameInput input{};
-    input.keys.e = platform_key_pressed(KEY_E);
-    input.keys.f12 = platform_key_pressed(KEY_F12);
-    input.keys.v = platform_key_pressed(KEY_V);
-    input.keys.f1 = platform_key_pressed(KEY_F1);
-    input.keys.escape = platform_key_pressed(KEY_ESCAPE);
-    input.movement = sample_movement_input();
-    input.keys.movement = input.movement.x != 0 || input.movement.y != 0;
-    for (std::size_t index = 0U; index < kCombatKeyBindings.size(); ++index) {
-        input.actions[index] = platform_key_pressed(kCombatKeyBindings[index].key);
-        input.keys.attack = input.keys.attack || input.actions[index];
-    }
-    input.keys.reset = platform_key_pressed(KEY_R);
-    input.keys.inventory = platform_key_pressed(kInventoryKey);
-    input.keys.passives = platform_key_pressed(kPassiveOverlayKey);
-    input.keys.mouse_gameplay = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
-    if (input.keys.mouse_gameplay) input.mouse_position = GetMousePosition();
-    return input;
-}
-
-void submit_frame_actions(dungeon::DungeonSession& session,
-    const HostFrameInput& input) noexcept {
-    for (std::size_t index = 0U; index < kCombatKeyBindings.size(); ++index) {
-        if (input.actions[index]) {
-            static_cast<void>(session.queue_action(
-                kCombatKeyBindings[index].action));
-        }
-    }
-}
-
 void drain_events(dungeon::DungeonSession& session, CombatRenderer& renderer,
     CombatFeedback& feedback, CombatAudio& audio) noexcept {
     while (const auto event = session.try_pop_combat_event()) {
         renderer.consume_event(*event);
         feedback.consume(*event);
         audio.consume_event(*event);
     }
     while (const auto event = session.try_pop_event()) {
         renderer.consume_dungeon_event(*event);
         if (dungeon_event_clears_transients(event->kind)) {
@@ -457,20 +409,21 @@ HostExitCode run_raylib_host(const RaylibHostConfig& config) noexcept {
         InventoryRenderer inventory;
         const bool audio_ready = audio.initialize();
         bool draw_debug = false;
         bool passive_overlay_open = false;
         bool exit_requested = false;
         std::uint32_t presented_frame_count = 0U;
         unsigned validation_capture_tick = 0U;
         unsigned validation_capture_count = 0U;
         Stage10ValidationState stage10_validation_state{};
         Stage11ValidationState stage11_validation_state{};
+        const settings::SettingsData input_settings = settings::default_settings();
         bool stage10_validation_captured = false;
         const std::string validation_capture_prefix = config.validation_capture
             ? (*save_directory / "stage8-validation-").string()
             : std::string{};
         const auto validation_capture_path = [&]() noexcept
                 -> std::optional<std::string> {
             if (!config.validation_capture
                     || validation_capture_count >= 2000U
                     || ++validation_capture_tick < 60U) {
                 return std::nullopt;
@@ -482,23 +435,25 @@ HostExitCode run_raylib_host(const RaylibHostConfig& config) noexcept {
         };
         dungeon::DungeonSnapshot current{};
         dungeon::DungeonSnapshot previous{};
         if (runtime.session() != nullptr) {
             current = runtime.session()->snapshot();
             previous = current;
             drain_events(*runtime.session(), renderer, feedback, audio);
         }
 
         while (!WindowShouldClose() && !exit_requested) {
-            HostFrameInput frame_input = sample_host_frame_input();
+            const PhysicalKeySnapshot physical_keys = sample_physical_keys();
+            HostFrameInput frame_input = map_host_frame_input(
+                input_settings, physical_keys);
             if (recovery_requested(runtime.state() == DungeonRuntimeState::recovery_required,
-                    platform_key_pressed(KEY_N))) {
+                    frame_input.keys.recovery)) {
                 if (runtime.recover_with_new_run() && runtime.session() != nullptr) {
                     current = runtime.session()->snapshot();
                     previous = current;
                     drain_events(*runtime.session(), renderer, feedback, audio);
                 }
             }
             if (runtime.state() == DungeonRuntimeState::recovery_required) {
                 if (inventory.is_open()) {
                     inventory.close();
                     fixed_step.clear_accumulator();
diff --git a/src/platform/raylib/raylib_input.hpp b/src/platform/raylib/raylib_input.hpp
index 14e562c..34f5073 100644
--- a/src/platform/raylib/raylib_input.hpp
+++ b/src/platform/raylib/raylib_input.hpp
@@ -15,19 +15,22 @@ struct FrameKeyState final {
     bool f12{};
     bool v{};
     bool f1{};
     bool escape{};
     bool movement{};
     bool attack{};
     bool reset{};
     bool inventory{};
     bool passives{};
     bool mouse_gameplay{};
+    bool enter{};
+    bool recovery{};
+    bool focus_lost{};
 };
 
 [[nodiscard]] bool platform_key_pressed(int raylib_key) noexcept;
 [[nodiscard]] bool platform_key_down(int raylib_key) noexcept;
 [[nodiscard]] DeathInputGate death_input_gate(
     bool death_saving, bool death_pending,
     const FrameKeyState& keys) noexcept;
 
 }  // namespace arpg::platform
diff --git a/src/platform/raylib/stable_key_raylib.cpp b/src/platform/raylib/stable_key_raylib.cpp
new file mode 100644
index 0000000..9e84ab3
--- /dev/null
+++ b/src/platform/raylib/stable_key_raylib.cpp
@@ -0,0 +1,85 @@
+#include "stable_key_raylib.hpp"
+
+#include <raylib.h>
+
+#include <array>
+#include <cstddef>
+
+namespace arpg::platform {
+namespace {
+
+struct StableKeyEntry final {
+    settings::StableKey stable{};
+    int raylib{};
+    const char* label{};
+};
+
+constexpr std::array<StableKeyEntry,
+    static_cast<std::size_t>(settings::StableKey::count)> kStableKeys{{
+    {settings::StableKey::a, KEY_A, "A"},
+    {settings::StableKey::b, KEY_B, "B"},
+    {settings::StableKey::c, KEY_C, "C"},
+    {settings::StableKey::d, KEY_D, "D"},
+    {settings::StableKey::e, KEY_E, "E"},
+    {settings::StableKey::f, KEY_F, "F"},
+    {settings::StableKey::g, KEY_G, "G"},
+    {settings::StableKey::h, KEY_H, "H"},
+    {settings::StableKey::i, KEY_I, "I"},
+    {settings::StableKey::j, KEY_J, "J"},
+    {settings::StableKey::k, KEY_K, "K"},
+    {settings::StableKey::l, KEY_L, "L"},
+    {settings::StableKey::m, KEY_M, "M"},
+    {settings::StableKey::n, KEY_N, "N"},
+    {settings::StableKey::o, KEY_O, "O"},
+    {settings::StableKey::p, KEY_P, "P"},
+    {settings::StableKey::q, KEY_Q, "Q"},
+    {settings::StableKey::r, KEY_R, "R"},
+    {settings::StableKey::s, KEY_S, "S"},
+    {settings::StableKey::t, KEY_T, "T"},
+    {settings::StableKey::u, KEY_U, "U"},
+    {settings::StableKey::v, KEY_V, "V"},
+    {settings::StableKey::w, KEY_W, "W"},
+    {settings::StableKey::x, KEY_X, "X"},
+    {settings::StableKey::y, KEY_Y, "Y"},
+    {settings::StableKey::z, KEY_Z, "Z"},
+    {settings::StableKey::digit_0, KEY_ZERO, "0"},
+    {settings::StableKey::digit_1, KEY_ONE, "1"},
+    {settings::StableKey::digit_2, KEY_TWO, "2"},
+    {settings::StableKey::digit_3, KEY_THREE, "3"},
+    {settings::StableKey::digit_4, KEY_FOUR, "4"},
+    {settings::StableKey::digit_5, KEY_FIVE, "5"},
+    {settings::StableKey::digit_6, KEY_SIX, "6"},
+    {settings::StableKey::digit_7, KEY_SEVEN, "7"},
+    {settings::StableKey::digit_8, KEY_EIGHT, "8"},
+    {settings::StableKey::digit_9, KEY_NINE, "9"},
+    {settings::StableKey::arrow_up, KEY_UP, "Up"},
+    {settings::StableKey::arrow_down, KEY_DOWN, "Down"},
+    {settings::StableKey::arrow_left, KEY_LEFT, "Left"},
+    {settings::StableKey::arrow_right, KEY_RIGHT, "Right"},
+    {settings::StableKey::space, KEY_SPACE, "Space"},
+    {settings::StableKey::left_shift, KEY_LEFT_SHIFT, "Left Shift"},
+    {settings::StableKey::right_shift, KEY_RIGHT_SHIFT, "Right Shift"},
+    {settings::StableKey::left_control, KEY_LEFT_CONTROL, "Left Ctrl"},
+    {settings::StableKey::right_control, KEY_RIGHT_CONTROL, "Right Ctrl"},
+}};
+
+}  // namespace
+
+int raylib_key(settings::StableKey key) noexcept {
+    const std::size_t index = static_cast<std::size_t>(key);
+    return index < kStableKeys.size() ? kStableKeys[index].raylib : KEY_NULL;
+}
+
+std::optional<settings::StableKey> stable_key_from_raylib(int key) noexcept {
+    for (const StableKeyEntry& entry : kStableKeys) {
+        if (entry.raylib == key) return entry.stable;
+    }
+    return std::nullopt;
+}
+
+const char* stable_key_label(settings::StableKey key) noexcept {
+    const std::size_t index = static_cast<std::size_t>(key);
+    return index < kStableKeys.size() ? kStableKeys[index].label : "Unknown";
+}
+
+}  // namespace arpg::platform
diff --git a/src/platform/raylib/stable_key_raylib.hpp b/src/platform/raylib/stable_key_raylib.hpp
new file mode 100644
index 0000000..4e0b3f1
--- /dev/null
+++ b/src/platform/raylib/stable_key_raylib.hpp
@@ -0,0 +1,14 @@
+#pragma once
+
+#include "platform/settings/settings_types.hpp"
+
+#include <optional>
+
+namespace arpg::platform {
+
+[[nodiscard]] int raylib_key(settings::StableKey key) noexcept;
+[[nodiscard]] std::optional<settings::StableKey> stable_key_from_raylib(
+    int key) noexcept;
+[[nodiscard]] const char* stable_key_label(settings::StableKey key) noexcept;
+
+}  // namespace arpg::platform
diff --git a/tests/platform/CMakeLists.txt b/tests/platform/CMakeLists.txt
index cf52148..4a9ec6c 100644
--- a/tests/platform/CMakeLists.txt
+++ b/tests/platform/CMakeLists.txt
@@ -1,15 +1,15 @@
 if(TARGET arpg_raylib)
     add_executable(arpg_platform_tests
         ../core/allocation_probe.cpp
         platform_test_main.cpp
-        combat_key_bindings_tests.cpp
+        host_input_tests.cpp
         combat_feedback_tests.cpp
         combat_view_math_tests.cpp
         monster_view_tests.cpp
         dungeon_view_math_tests.cpp
         passive_tree_view_tests.cpp
         host_launch_options_tests.cpp
         dungeon_runtime_tests.cpp
         death_input_gate_tests.cpp
         death_overlay_view_tests.cpp)
     target_sources(arpg_platform_tests PRIVATE inventory_view_math_tests.cpp)
@@ -1012,10 +1012,19 @@ set_tests_properties(
     PROPERTIES LABELS "headless;architecture")
 
 add_test(
     NAME platform.input_latency_source
     COMMAND "${CMAKE_COMMAND}"
         "-DRAYLIB_SOURCE_DIR=${PROJECT_SOURCE_DIR}/src/platform/raylib"
         -P "${CMAKE_CURRENT_SOURCE_DIR}/input_latency_source_test.cmake")
 set_tests_properties(
     platform.input_latency_source
     PROPERTIES LABELS "headless;platform;architecture")
+
+add_test(
+    NAME platform.host_input_source
+    COMMAND "${CMAKE_COMMAND}"
+        "-DRAYLIB_SOURCE_DIR=${PROJECT_SOURCE_DIR}/src/platform/raylib"
+        -P "${CMAKE_CURRENT_SOURCE_DIR}/host_input_source_test.cmake")
+set_tests_properties(
+    platform.host_input_source
+    PROPERTIES LABELS "headless;platform;architecture")
diff --git a/tests/platform/combat_key_bindings_tests.cpp b/tests/platform/combat_key_bindings_tests.cpp
deleted file mode 100644
index bc3aca7..0000000
--- a/tests/platform/combat_key_bindings_tests.cpp
+++ /dev/null
@@ -1,49 +0,0 @@
-#include "test_framework.hpp"
-
-#include "combat_key_bindings.hpp"
-
-namespace {
-
-using namespace arpg;
-
-static_assert(platform::kInventoryKey == KEY_I);
-static_assert(platform::kPassiveOverlayKey == KEY_P);
-static_assert(platform::kInventoryKey != platform::kPassiveOverlayKey);
-
-test::Failure combat_keys_are_j_k_and_l_without_u() noexcept {
-    ARPG_REQUIRE(platform::kCombatKeyBindings.size() == 3);
-    ARPG_REQUIRE(platform::kCombatKeyBindings[0].key == KEY_J);
-    ARPG_REQUIRE(
-        platform::kCombatKeyBindings[0].action == combat::Action::light);
-    ARPG_REQUIRE(platform::kCombatKeyBindings[1].key == KEY_K);
-    ARPG_REQUIRE(
-        platform::kCombatKeyBindings[1].action == combat::Action::jump);
-    ARPG_REQUIRE(platform::kCombatKeyBindings[2].key == KEY_L);
-    ARPG_REQUIRE(
-        platform::kCombatKeyBindings[2].action == combat::Action::launcher);
-    for (const platform::CombatKeyBinding& binding
-         : platform::kCombatKeyBindings) {
-        ARPG_REQUIRE(binding.key != KEY_U);
-    }
-    return {};
-}
-
-test::Failure overlay_keys_are_i_and_p() noexcept {
-    volatile int inventory_key = platform::kInventoryKey;
-    volatile int passive_key = platform::kPassiveOverlayKey;
-    ARPG_REQUIRE(inventory_key == KEY_I);
-    ARPG_REQUIRE(passive_key == KEY_P);
-    ARPG_REQUIRE(inventory_key != passive_key);
-    return {};
-}
-
-constexpr test::TestCase kCases[] = {
-    {"J K L bindings without U", &combat_keys_are_j_k_and_l_without_u},
-    {"I P overlay bindings", &overlay_keys_are_i_and_p},
-};
-
-}  // namespace
-
-arpg::test::TestSuite combat_key_bindings_suite() noexcept {
-    return arpg::test::make_suite("combat_key_bindings", kCases);
-}
diff --git a/tests/platform/host_input_source_test.cmake b/tests/platform/host_input_source_test.cmake
new file mode 100644
index 0000000..4b9a8e7
--- /dev/null
+++ b/tests/platform/host_input_source_test.cmake
@@ -0,0 +1,63 @@
+if(NOT DEFINED RAYLIB_SOURCE_DIR)
+    message(FATAL_ERROR "RAYLIB_SOURCE_DIR is required")
+endif()
+
+file(READ "${RAYLIB_SOURCE_DIR}/host_input.cpp" HOST_INPUT_SOURCE)
+file(READ "${RAYLIB_SOURCE_DIR}/raylib_host.cpp" HOST_SOURCE)
+
+function(require_match_count SOURCE PATTERN EXPECTED LABEL)
+    string(REGEX MATCHALL "${PATTERN}" MATCHES "${SOURCE}")
+    list(LENGTH MATCHES ACTUAL)
+    if(NOT ACTUAL EQUAL EXPECTED)
+        message(FATAL_ERROR
+            "${LABEL}: expected ${EXPECTED} source matches, found ${ACTUAL}")
+    endif()
+endfunction()
+
+require_match_count(
+    "${HOST_INPUT_SOURCE}"
+    "platform_key_pressed[ \t\r\n]*\\("
+    1
+    "pressed sampling sites")
+require_match_count(
+    "${HOST_INPUT_SOURCE}"
+    "platform_key_down[ \t\r\n]*\\("
+    1
+    "down sampling sites")
+require_match_count(
+    "${HOST_INPUT_SOURCE}"
+    "IsMouseButtonPressed[ \t\r\n]*\\("
+    1
+    "mouse pressed sampling sites")
+require_match_count(
+    "${HOST_INPUT_SOURCE}"
+    "GetMousePosition[ \t\r\n]*\\("
+    1
+    "mouse position sampling sites")
+require_match_count(
+    "${HOST_INPUT_SOURCE}"
+    "IsWindowFocused[ \t\r\n]*\\("
+    1
+    "focus sampling sites")
+
+if(HOST_INPUT_SOURCE MATCHES
+        "platform_key_pressed[ \t\r\n]*\\([ \t\r\n]*KEY_(V|R|N)")
+    message(FATAL_ERROR "V/R/N must be derived from the stable-key loop")
+endif()
+if(HOST_INPUT_SOURCE MATCHES "(new[ \t\r\n]|malloc[ \t\r\n]*\\()")
+    message(FATAL_ERROR "host input mapping must not allocate")
+endif()
+if(HOST_SOURCE MATCHES
+        "(platform_key_pressed|platform_key_down|IsKeyPressed|IsKeyDown)[ \t\r\n]*\\(")
+    message(FATAL_ERROR "raylib_host.cpp must consume the one-sample input module")
+endif()
+if(HOST_SOURCE MATCHES
+        "KEY_(W|A|S|D|J|K|L|E|I|P|R|N|V)([^A-Z0-9_]|$)")
+    message(FATAL_ERROR "raylib_host.cpp retains a hard-coded gameplay/global stable key")
+endif()
+if(NOT HOST_SOURCE MATCHES "sample_physical_keys[ \t\r\n]*\\(")
+    message(FATAL_ERROR "raylib host does not sample a physical snapshot")
+endif()
+if(NOT HOST_SOURCE MATCHES "map_host_frame_input[ \t\r\n]*\\(")
+    message(FATAL_ERROR "raylib host does not map the sampled snapshot")
+endif()
diff --git a/tests/platform/host_input_tests.cpp b/tests/platform/host_input_tests.cpp
new file mode 100644
index 0000000..9154580
--- /dev/null
+++ b/tests/platform/host_input_tests.cpp
@@ -0,0 +1,402 @@
+#include "test_framework.hpp"
+
+#include "allocation_probe.hpp"
+#include "host_input.hpp"
+#include "stable_key_raylib.hpp"
+
+#include "dungeon/dungeon_session.hpp"
+#include "platform/settings/settings_types.hpp"
+
+#include <raylib.h>
+
+#include <array>
+#include <cstddef>
+#include <cstring>
+
+namespace {
+
+using namespace arpg;
+
+constexpr std::size_t kStableKeyCount =
+    static_cast<std::size_t>(settings::StableKey::count);
+
+constexpr std::array<int, kStableKeyCount> kExpectedRaylibKeys{{
+    KEY_A, KEY_B, KEY_C, KEY_D, KEY_E, KEY_F, KEY_G, KEY_H, KEY_I,
+    KEY_J, KEY_K, KEY_L, KEY_M, KEY_N, KEY_O, KEY_P, KEY_Q, KEY_R,
+    KEY_S, KEY_T, KEY_U, KEY_V, KEY_W, KEY_X, KEY_Y, KEY_Z,
+    KEY_ZERO, KEY_ONE, KEY_TWO, KEY_THREE, KEY_FOUR, KEY_FIVE,
+    KEY_SIX, KEY_SEVEN, KEY_EIGHT, KEY_NINE,
+    KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT, KEY_SPACE,
+    KEY_LEFT_SHIFT, KEY_RIGHT_SHIFT, KEY_LEFT_CONTROL, KEY_RIGHT_CONTROL,
+}};
+
+constexpr std::array<const char*, kStableKeyCount> kExpectedLabels{{
+    "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L",
+    "M", "N", "O", "P", "Q", "R", "S", "T", "U", "V", "W", "X",
+    "Y", "Z", "0", "1", "2", "3", "4", "5", "6", "7", "8", "9",
+    "Up", "Down", "Left", "Right", "Space", "Left Shift", "Right Shift",
+    "Left Ctrl", "Right Ctrl",
+}};
+
+[[nodiscard]] constexpr std::size_t key_index(settings::StableKey key) noexcept {
+    return static_cast<std::size_t>(key);
+}
+
+void press(platform::PhysicalKeySnapshot& snapshot,
+    settings::StableKey key) noexcept {
+    snapshot.pressed[key_index(key)] = true;
+}
+
+void hold(platform::PhysicalKeySnapshot& snapshot,
+    settings::StableKey key) noexcept {
+    snapshot.down[key_index(key)] = true;
+}
+
+test::Failure adapter_round_trips_all_45_keys_with_unique_codes_and_labels() noexcept {
+    for (std::size_t index = 0U; index < kStableKeyCount; ++index) {
+        const auto key = static_cast<settings::StableKey>(index);
+        const int code = platform::raylib_key(key);
+        ARPG_REQUIRE(code == kExpectedRaylibKeys[index]);
+        const auto round_trip = platform::stable_key_from_raylib(code);
+        ARPG_REQUIRE(round_trip.has_value());
+        ARPG_REQUIRE(*round_trip == key);
+        ARPG_REQUIRE(std::strcmp(
+            platform::stable_key_label(key), kExpectedLabels[index]) == 0);
+        for (std::size_t earlier = 0U; earlier < index; ++earlier) {
+            ARPG_REQUIRE(code != kExpectedRaylibKeys[earlier]);
+            ARPG_REQUIRE(std::strcmp(
+                platform::stable_key_label(key),
+                platform::stable_key_label(
+                    static_cast<settings::StableKey>(earlier))) != 0);
+        }
+    }
+    return {};
+}
+
+test::Failure adapter_rejects_unknown_values() noexcept {
+    ARPG_REQUIRE(platform::raylib_key(settings::StableKey::count) == KEY_NULL);
+    ARPG_REQUIRE(!platform::stable_key_from_raylib(KEY_NULL).has_value());
+    ARPG_REQUIRE(!platform::stable_key_from_raylib(KEY_ESCAPE).has_value());
+    ARPG_REQUIRE(std::strcmp(
+        platform::stable_key_label(settings::StableKey::count), "Unknown") == 0);
+    return {};
+}
+
+struct QueryLog final {
+    std::array<unsigned, 512> pressed{};
+    std::array<unsigned, 512> down{};
+    unsigned mouse_pressed{};
+    unsigned mouse_position{};
+    unsigned focus_lost{};
+};
+
+bool query_pressed(void* context, int key) noexcept {
+    auto& log = *static_cast<QueryLog*>(context);
+    ++log.pressed[static_cast<std::size_t>(key)];
+    return key == KEY_V || key == KEY_R || key == KEY_N;
+}
+
+bool query_down(void* context, int key) noexcept {
+    auto& log = *static_cast<QueryLog*>(context);
+    ++log.down[static_cast<std::size_t>(key)];
+    return key == KEY_W;
+}
+
+bool query_mouse_pressed(void* context) noexcept {
+    ++static_cast<QueryLog*>(context)->mouse_pressed;
+    return true;
+}
+
+Vector2 query_mouse_position(void* context) noexcept {
+    ++static_cast<QueryLog*>(context)->mouse_position;
+    return {123.0F, 456.0F};
+}
+
+bool query_focus_lost(void* context) noexcept {
+    ++static_cast<QueryLog*>(context)->focus_lost;
+    return true;
+}
+
+test::Failure sampler_queries_every_stable_key_once_per_state() noexcept {
+    QueryLog log{};
+    const platform::PhysicalKeySource source{
+        &log,
+        &query_pressed,
+        &query_down,
+        &query_mouse_pressed,
+        &query_mouse_position,
+        &query_focus_lost,
+    };
+    const platform::PhysicalKeySnapshot snapshot =
+        platform::sample_physical_keys(source);
+
+    for (std::size_t index = 0U; index < kStableKeyCount; ++index) {
+        const int code = kExpectedRaylibKeys[index];
+        ARPG_REQUIRE(log.pressed[static_cast<std::size_t>(code)] == 1U);
+        ARPG_REQUIRE(log.down[static_cast<std::size_t>(code)] == 1U);
+    }
+    ARPG_REQUIRE(log.pressed[KEY_ESCAPE] == 1U);
+    ARPG_REQUIRE(log.pressed[KEY_ENTER] == 1U);
+    ARPG_REQUIRE(log.pressed[KEY_F1] == 1U);
+    ARPG_REQUIRE(log.pressed[KEY_F12] == 1U);
+    ARPG_REQUIRE(log.pressed[KEY_V] == 1U);
+    ARPG_REQUIRE(log.pressed[KEY_R] == 1U);
+    ARPG_REQUIRE(log.pressed[KEY_N] == 1U);
+    ARPG_REQUIRE(log.mouse_pressed == 1U);
+    ARPG_REQUIRE(log.mouse_position == 1U);
+    ARPG_REQUIRE(log.focus_lost == 1U);
+    ARPG_REQUIRE(snapshot.v);
+    ARPG_REQUIRE(snapshot.mouse_left);
+    ARPG_REQUIRE(snapshot.focus_lost);
+    ARPG_REQUIRE(snapshot.mouse_position.x == 123.0F);
+    ARPG_REQUIRE(snapshot.mouse_position.y == 456.0F);
+    return {};
+}
+
+test::Failure default_bindings_map_movement_actions_and_overlays() noexcept {
+    const settings::SettingsData settings = settings::default_settings();
+    platform::PhysicalKeySnapshot snapshot{};
+    hold(snapshot, settings::StableKey::w);
+    hold(snapshot, settings::StableKey::d);
+    press(snapshot, settings::StableKey::j);
+    press(snapshot, settings::StableKey::k);
+    press(snapshot, settings::StableKey::l);
+    press(snapshot, settings::StableKey::e);
+    press(snapshot, settings::StableKey::i);
+    press(snapshot, settings::StableKey::p);
+
+    const platform::HostFrameInput input =
+        platform::map_host_frame_input(settings, snapshot);
+    ARPG_REQUIRE(input.movement.x == 1);
+    ARPG_REQUIRE(input.movement.y == -1);
+    ARPG_REQUIRE(input.keys.movement);
+    ARPG_REQUIRE(input.combat_actions[0]);
+    ARPG_REQUIRE(input.combat_actions[1]);
+    ARPG_REQUIRE(input.combat_actions[2]);
+    ARPG_REQUIRE(input.keys.attack);
+    ARPG_REQUIRE(input.keys.e);
+    ARPG_REQUIRE(input.keys.inventory);
+    ARPG_REQUIRE(input.keys.passives);
+    return {};
+}
+
+test::Failure swapped_bindings_drive_the_new_logical_owners() noexcept {
+    settings::SettingsData settings = settings::default_settings();
+    ARPG_REQUIRE(settings::assign_or_swap(
+        settings, settings::SettingAction::light_attack, settings::StableKey::d));
+    ARPG_REQUIRE(settings::binding_for(
+        settings, settings::SettingAction::move_right) == settings::StableKey::j);
+    ARPG_REQUIRE(settings::validate_settings(settings)
+        == settings::SettingsValidationError::none);
+
+    platform::PhysicalKeySnapshot snapshot{};
+    press(snapshot, settings::StableKey::d);
+    hold(snapshot, settings::StableKey::j);
+    const platform::HostFrameInput input =
+        platform::map_host_frame_input(settings, snapshot);
+    ARPG_REQUIRE(input.combat_actions[0]);
+    ARPG_REQUIRE(!input.combat_actions[1]);
+    ARPG_REQUIRE(!input.combat_actions[2]);
+    ARPG_REQUIRE(input.movement.x == 1);
+
+    settings = settings::default_settings();
+    ARPG_REQUIRE(settings::assign_or_swap(
+        settings, settings::SettingAction::interact, settings::StableKey::i));
+    snapshot = {};
+    press(snapshot, settings::StableKey::i);
+    auto overlay_input = platform::map_host_frame_input(settings, snapshot);
+    ARPG_REQUIRE(overlay_input.keys.e);
+    ARPG_REQUIRE(!overlay_input.keys.inventory);
+    snapshot = {};
+    press(snapshot, settings::StableKey::e);
+    overlay_input = platform::map_host_frame_input(settings, snapshot);
+    ARPG_REQUIRE(!overlay_input.keys.e);
+    ARPG_REQUIRE(overlay_input.keys.inventory);
+
+    settings = settings::default_settings();
+    ARPG_REQUIRE(settings::assign_or_swap(
+        settings, settings::SettingAction::inventory, settings::StableKey::p));
+    snapshot = {};
+    press(snapshot, settings::StableKey::p);
+    overlay_input = platform::map_host_frame_input(settings, snapshot);
+    ARPG_REQUIRE(overlay_input.keys.inventory);
+    ARPG_REQUIRE(!overlay_input.keys.passives);
+    snapshot = {};
+    press(snapshot, settings::StableKey::i);
+    overlay_input = platform::map_host_frame_input(settings, snapshot);
+    ARPG_REQUIRE(!overlay_input.keys.inventory);
+    ARPG_REQUIRE(overlay_input.keys.passives);
+    return {};
+}
+
+test::Failure movement_uses_down_and_actions_use_pressed() noexcept {
+    const settings::SettingsData settings = settings::default_settings();
+    platform::PhysicalKeySnapshot snapshot{};
+    press(snapshot, settings::StableKey::w);
+    hold(snapshot, settings::StableKey::j);
+    platform::HostFrameInput input =
+        platform::map_host_frame_input(settings, snapshot);
+    ARPG_REQUIRE(input.movement.x == 0);
+    ARPG_REQUIRE(input.movement.y == 0);
+    ARPG_REQUIRE(!input.keys.movement);
+    ARPG_REQUIRE(!input.combat_actions[0]);
+    ARPG_REQUIRE(!input.keys.attack);
+
+    snapshot = {};
+    hold(snapshot, settings::StableKey::w);
+    press(snapshot, settings::StableKey::j);
+    input = platform::map_host_frame_input(settings, snapshot);
+    ARPG_REQUIRE(input.movement.y == -1);
+    ARPG_REQUIRE(input.combat_actions[0]);
+    return {};
+}
+
+test::Failure global_keys_and_mouse_are_binding_independent() noexcept {
+    settings::SettingsData settings = settings::default_settings();
+    ARPG_REQUIRE(settings::assign_or_swap(
+        settings, settings::SettingAction::interact, settings::StableKey::q));
+    platform::PhysicalKeySnapshot snapshot{};
+    snapshot.escape = true;
+    snapshot.enter = true;
+    snapshot.f1 = true;
+    snapshot.f12 = true;
+    snapshot.v = true;
+    snapshot.mouse_left = true;
+    snapshot.focus_lost = true;
+    snapshot.mouse_position = {17.0F, 29.0F};
+
+    const platform::HostFrameInput input =
+        platform::map_host_frame_input(settings, snapshot);
+    ARPG_REQUIRE(input.keys.escape);
+    ARPG_REQUIRE(input.keys.enter);
+    ARPG_REQUIRE(input.keys.f1);
+    ARPG_REQUIRE(input.keys.f12);
+    ARPG_REQUIRE(input.keys.v);
+    ARPG_REQUIRE(input.keys.mouse_gameplay);
+    ARPG_REQUIRE(input.keys.focus_lost);
+    ARPG_REQUIRE(input.mouse_position.x == 17.0F);
+    ARPG_REQUIRE(input.mouse_position.y == 29.0F);
+    ARPG_REQUIRE(!input.keys.e);
+    return {};
+}
+
+test::Failure valid_r_binding_wins_over_legacy_room_reset() noexcept {
+    platform::PhysicalKeySnapshot snapshot{};
+    press(snapshot, settings::StableKey::r);
+    hold(snapshot, settings::StableKey::r);
+
+    const platform::HostFrameInput legacy = platform::map_host_frame_input(
+        settings::default_settings(), snapshot);
+    ARPG_REQUIRE(legacy.keys.reset);
+
+    settings::SettingsData rebound = settings::default_settings();
+    ARPG_REQUIRE(settings::assign_or_swap(
+        rebound, settings::SettingAction::move_up, settings::StableKey::r));
+    ARPG_REQUIRE(settings::validate_settings(rebound)
+        == settings::SettingsValidationError::none);
+    const platform::HostFrameInput bound =
+        platform::map_host_frame_input(rebound, snapshot);
+    ARPG_REQUIRE(bound.movement.y == -1);
+    ARPG_REQUIRE(!bound.keys.reset);
+    return {};
+}
+
+test::Failure recovery_n_uses_the_stable_snapshot_edge() noexcept {
+    platform::PhysicalKeySnapshot snapshot{};
+    press(snapshot, settings::StableKey::n);
+    const platform::HostFrameInput input = platform::map_host_frame_input(
+        settings::default_settings(), snapshot);
+    ARPG_REQUIRE(input.keys.recovery);
+    return {};
+}
+
+test::Failure validated_swap_never_emits_two_combat_actions() noexcept {
+    settings::SettingsData settings = settings::default_settings();
+    ARPG_REQUIRE(settings::assign_or_swap(
+        settings, settings::SettingAction::light_attack, settings::StableKey::k));
+    ARPG_REQUIRE(settings::validate_settings(settings)
+        == settings::SettingsValidationError::none);
+    platform::PhysicalKeySnapshot snapshot{};
+    press(snapshot, settings::StableKey::k);
+    const platform::HostFrameInput input =
+        platform::map_host_frame_input(settings, snapshot);
+    const unsigned action_count = static_cast<unsigned>(input.combat_actions[0])
+        + static_cast<unsigned>(input.combat_actions[1])
+        + static_cast<unsigned>(input.combat_actions[2]);
+    ARPG_REQUIRE(action_count == 1U);
+    ARPG_REQUIRE(input.combat_actions[0]);
+    return {};
+}
+
+test::Failure mapper_performs_no_heap_allocations() noexcept {
+    const settings::SettingsData settings = settings::default_settings();
+    platform::PhysicalKeySnapshot snapshot{};
+    press(snapshot, settings::StableKey::j);
+    hold(snapshot, settings::StableKey::w);
+    const std::uint64_t before = test::allocation_count();
+    for (int iteration = 0; iteration < 1000; ++iteration) {
+        const platform::HostFrameInput input =
+            platform::map_host_frame_input(settings, snapshot);
+        ARPG_REQUIRE(input.combat_actions[0]);
+    }
+    ARPG_REQUIRE(test::allocation_count() == before);
+    return {};
+}
+
+test::Failure submit_forwards_each_combat_action_to_the_session() noexcept {
+    {
+        dungeon::DungeonSession session{};
+        session.tick({});
+        platform::HostFrameInput input{};
+        input.combat_actions[0] = true;
+        platform::submit_frame_actions(session, input);
+        session.tick({});
+        ARPG_REQUIRE(session.snapshot().combat.has_value());
+        ARPG_REQUIRE(session.snapshot().combat->player.active_attack
+            == combat::AttackId::j1);
+    }
+    {
+        dungeon::DungeonSession session{};
+        session.tick({});
+        platform::HostFrameInput input{};
+        input.combat_actions[1] = true;
+        platform::submit_frame_actions(session, input);
+        session.tick({});
+        ARPG_REQUIRE(session.snapshot().combat.has_value());
+        ARPG_REQUIRE(session.snapshot().combat->player.state
+            == combat::PlayerState::jump_rise);
+    }
+    {
+        dungeon::DungeonSession session{};
+        session.tick({});
+        platform::HostFrameInput input{};
+        input.combat_actions[2] = true;
+        platform::submit_frame_actions(session, input);
+        session.tick({});
+        ARPG_REQUIRE(session.snapshot().combat.has_value());
+        ARPG_REQUIRE(session.snapshot().combat->player.active_attack
+            == combat::AttackId::launcher);
+    }
+    return {};
+}
+
+constexpr test::TestCase kCases[] = {
+    {"45 stable keys round-trip with labels", &adapter_round_trips_all_45_keys_with_unique_codes_and_labels},
+    {"unknown adapter values rejected", &adapter_rejects_unknown_values},
+    {"stable keys sampled once per state", &sampler_queries_every_stable_key_once_per_state},
+    {"default logical bindings", &default_bindings_map_movement_actions_and_overlays},
+    {"swapped logical bindings", &swapped_bindings_drive_the_new_logical_owners},
+    {"movement down and action pressed", &movement_uses_down_and_actions_use_pressed},
+    {"global input binding independent", &global_keys_and_mouse_are_binding_independent},
+    {"R binding wins over reset", &valid_r_binding_wins_over_legacy_room_reset},
+    {"N recovery from stable edge", &recovery_n_uses_the_stable_snapshot_edge},
+    {"validated binding emits one combat action", &validated_swap_never_emits_two_combat_actions},
+    {"mapper has zero allocations", &mapper_performs_no_heap_allocations},
+    {"submit forwards combat actions", &submit_forwards_each_combat_action_to_the_session},
+};
+
+}  // namespace
+
+arpg::test::TestSuite host_input_suite() noexcept {
+    return arpg::test::make_suite("host_input", kCases);
+}
diff --git a/tests/platform/platform_test_main.cpp b/tests/platform/platform_test_main.cpp
index 0b5d072..53c5937 100644
--- a/tests/platform/platform_test_main.cpp
+++ b/tests/platform/platform_test_main.cpp
@@ -1,21 +1,21 @@
 #include "test_framework.hpp"
 
 #if defined(_WIN32) && defined(_DEBUG)
 #include <crtdbg.h>
 #include <cstdlib>
 #endif
 
 arpg::test::TestSuite combat_view_math_suite() noexcept;
 arpg::test::TestSuite monster_view_suite() noexcept;
 arpg::test::TestSuite combat_feedback_suite() noexcept;
-arpg::test::TestSuite combat_key_bindings_suite() noexcept;
+arpg::test::TestSuite host_input_suite() noexcept;
 arpg::test::TestSuite dungeon_view_math_suite() noexcept;
 arpg::test::TestSuite passive_tree_view_suite() noexcept;
 arpg::test::TestSuite host_launch_options_suite() noexcept;
 arpg::test::TestSuite dungeon_runtime_suite() noexcept;
 arpg::test::TestSuite inventory_view_math_suite() noexcept;
 arpg::test::TestSuite death_input_gate_suite() noexcept;
 arpg::test::TestSuite death_overlay_view_suite() noexcept;
 
 int main() {
 #if defined(_WIN32) && defined(_DEBUG)
@@ -24,22 +24,22 @@ int main() {
     _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
     _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE);
     _CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
     _set_abort_behavior(_WRITE_ABORT_MSG,
         _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
 #endif
     const arpg::test::TestSuite suites[] = {
         combat_view_math_suite(),
         monster_view_suite(),
         combat_feedback_suite(),
-        combat_key_bindings_suite(),
+        host_input_suite(),
         dungeon_view_math_suite(),
         passive_tree_view_suite(),
         host_launch_options_suite(),
         dungeon_runtime_suite(),
         inventory_view_math_suite(),
         death_input_gate_suite(),
         death_overlay_view_suite(),
     };
 
-    return arpg::test::run_suites(suites, 104, "stage 11a task 10 death overlay");
+    return arpg::test::run_suites(suites, 114, "stage 11b task 4 host input");
 }
