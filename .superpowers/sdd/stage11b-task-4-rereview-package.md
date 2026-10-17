# Review package: bb80cf7..ffea5e7

## Commits
ffea5e7 fix: keep inventory input on frame snapshot

## Files changed
 src/platform/raylib/host_input.cpp          | 22 ++++++++-
 src/platform/raylib/host_input.hpp          |  8 +++
 src/platform/raylib/inventory_renderer.cpp  | 17 +++----
 src/platform/raylib/inventory_renderer.hpp  |  4 +-
 src/platform/raylib/raylib_host.cpp         |  2 +-
 tests/platform/host_input_source_test.cmake | 59 ++++++++++++++++------
 tests/platform/host_input_tests.cpp         | 77 ++++++++++++++++++++++++++---
 tests/platform/platform_test_main.cpp       |  2 +-
 8 files changed, 156 insertions(+), 35 deletions(-)

## Diff
diff --git a/src/platform/raylib/host_input.cpp b/src/platform/raylib/host_input.cpp
index adae3d4..923d67d 100644
--- a/src/platform/raylib/host_input.cpp
+++ b/src/platform/raylib/host_input.cpp
@@ -23,24 +23,32 @@ constexpr std::size_t kStableKeyCount =
 }
 
 [[nodiscard]] bool default_down(void*, int key) noexcept {
     return platform_key_down(key);
 }
 
 [[nodiscard]] bool default_mouse_left_pressed(void*) noexcept {
     return IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
 }
 
+[[nodiscard]] bool default_mouse_right_pressed(void*) noexcept {
+    return IsMouseButtonPressed(MOUSE_BUTTON_RIGHT);
+}
+
 [[nodiscard]] Vector2 default_mouse_position(void*) noexcept {
     return GetMousePosition();
 }
 
+[[nodiscard]] float default_mouse_wheel(void*) noexcept {
+    return GetMouseWheelMove();
+}
+
 [[nodiscard]] bool default_focus_lost(void*) noexcept {
     return !IsWindowFocused();
 }
 
 [[nodiscard]] bool pressed(
     const PhysicalKeySnapshot& snapshot,
     settings::StableKey key) noexcept {
     const std::size_t index = stable_index(key);
     return index < snapshot.pressed.size() && snapshot.pressed[index];
 }
@@ -69,21 +77,23 @@ constexpr std::size_t kStableKeyCount =
 }
 
 }  // namespace
 
 PhysicalKeySnapshot sample_physical_keys() noexcept {
     const PhysicalKeySource source{
         nullptr,
         &default_pressed,
         &default_down,
         &default_mouse_left_pressed,
+        &default_mouse_right_pressed,
         &default_mouse_position,
+        &default_mouse_wheel,
         &default_focus_lost,
     };
     return sample_physical_keys(source);
 }
 
 PhysicalKeySnapshot sample_physical_keys(
     const PhysicalKeySource& source) noexcept {
     PhysicalKeySnapshot snapshot{};
     if (source.pressed == nullptr || source.down == nullptr) return snapshot;
 
@@ -93,23 +103,28 @@ PhysicalKeySnapshot sample_physical_keys(
         snapshot.pressed[index] = source.pressed(source.context, code);
         snapshot.down[index] = source.down(source.context, code);
     }
     snapshot.escape = source.pressed(source.context, KEY_ESCAPE);
     snapshot.enter = source.pressed(source.context, KEY_ENTER);
     snapshot.f1 = source.pressed(source.context, KEY_F1);
     snapshot.f12 = source.pressed(source.context, KEY_F12);
     snapshot.v = snapshot.pressed[stable_index(settings::StableKey::v)];
     snapshot.mouse_left = source.mouse_left_pressed != nullptr
         && source.mouse_left_pressed(source.context);
-    if (snapshot.mouse_left && source.mouse_position != nullptr) {
+    snapshot.mouse_right = source.mouse_right_pressed != nullptr
+        && source.mouse_right_pressed(source.context);
+    if (source.mouse_position != nullptr) {
         snapshot.mouse_position = source.mouse_position(source.context);
     }
+    if (source.mouse_wheel != nullptr) {
+        snapshot.mouse_wheel = source.mouse_wheel(source.context);
+    }
     snapshot.focus_lost = source.focus_lost != nullptr
         && source.focus_lost(source.context);
     return snapshot;
 }
 
 HostFrameInput map_host_frame_input(
     const settings::SettingsData& settings_data,
     const PhysicalKeySnapshot& snapshot) noexcept {
     HostFrameInput input{};
     const int left = down(snapshot, settings::binding_for(
@@ -146,20 +161,25 @@ HostFrameInput map_host_frame_input(
     input.keys.reset = pressed(snapshot, settings::StableKey::r)
         && !binding_owns(settings_data, settings::StableKey::r);
     input.keys.recovery = pressed(snapshot, settings::StableKey::n);
     input.keys.escape = snapshot.escape;
     input.keys.enter = snapshot.enter;
     input.keys.f1 = snapshot.f1;
     input.keys.f12 = snapshot.f12;
     input.keys.v = snapshot.v;
     input.keys.mouse_gameplay = snapshot.mouse_left;
     input.keys.focus_lost = snapshot.focus_lost;
+    input.mouse_left_pressed = snapshot.mouse_left;
+    input.mouse_right_pressed = snapshot.mouse_right;
+    input.control_down = down(snapshot, settings::StableKey::left_control)
+        || down(snapshot, settings::StableKey::right_control);
+    input.mouse_wheel = snapshot.mouse_wheel;
     input.mouse_position = snapshot.mouse_position;
     return input;
 }
 
 void submit_frame_actions(
     dungeon::DungeonSession& session,
     const HostFrameInput& input) noexcept {
     constexpr std::array<combat::Action, 3> actions{{
         combat::Action::light,
         combat::Action::jump,
diff --git a/src/platform/raylib/host_input.hpp b/src/platform/raylib/host_input.hpp
index 82086ba..5cbc13e 100644
--- a/src/platform/raylib/host_input.hpp
+++ b/src/platform/raylib/host_input.hpp
@@ -19,37 +19,45 @@ struct PhysicalKeySnapshot final {
     std::array<bool,
         static_cast<std::size_t>(settings::StableKey::count)> down{};
     std::array<bool,
         static_cast<std::size_t>(settings::StableKey::count)> pressed{};
     bool escape{};
     bool enter{};
     bool f1{};
     bool f12{};
     bool v{};
     bool mouse_left{};
+    bool mouse_right{};
     bool focus_lost{};
+    float mouse_wheel{};
     Vector2 mouse_position{};
 };
 
 struct HostFrameInput final {
     FrameKeyState keys{};
     combat::MovementInput movement{};
     std::array<bool, 3> combat_actions{};
+    bool mouse_left_pressed{};
+    bool mouse_right_pressed{};
+    bool control_down{};
+    float mouse_wheel{};
     Vector2 mouse_position{};
 };
 
 struct PhysicalKeySource final {
     void* context{};
     bool (*pressed)(void*, int) noexcept{};
     bool (*down)(void*, int) noexcept{};
     bool (*mouse_left_pressed)(void*) noexcept{};
+    bool (*mouse_right_pressed)(void*) noexcept{};
     Vector2 (*mouse_position)(void*) noexcept{};
+    float (*mouse_wheel)(void*) noexcept{};
     bool (*focus_lost)(void*) noexcept{};
 };
 
 [[nodiscard]] PhysicalKeySnapshot sample_physical_keys() noexcept;
 [[nodiscard]] PhysicalKeySnapshot sample_physical_keys(
     const PhysicalKeySource& source) noexcept;
 [[nodiscard]] HostFrameInput map_host_frame_input(
     const settings::SettingsData& settings,
     const PhysicalKeySnapshot& snapshot) noexcept;
 void submit_frame_actions(
diff --git a/src/platform/raylib/inventory_renderer.cpp b/src/platform/raylib/inventory_renderer.cpp
index 97d981e..d13b990 100644
--- a/src/platform/raylib/inventory_renderer.cpp
+++ b/src/platform/raylib/inventory_renderer.cpp
@@ -1,13 +1,13 @@
 #include "inventory_renderer.hpp"
 
-#include "raylib_input.hpp"
+#include "host_input.hpp"
 
 #include "dungeon/dungeon_session.hpp"
 #include "dungeon_runtime.hpp"
 #include "items/item_catalog.hpp"
 
 #include <raylib.h>
 
 #include <algorithm>
 #include <array>
 #include <cmath>
@@ -190,37 +190,38 @@ void InventoryRenderer::sync(const dungeon::DungeonSession& session,
         view_cache_.requested_recipe = recipe_;
     }
     refresh_comparison(session, snapshot);
 }
 
 bool InventoryRenderer::recipe_ready() const noexcept {
     return cached_recipe_ready(view_cache_);
 }
 
 bool InventoryRenderer::process_input(DungeonRuntime& runtime,
-    const dungeon::DungeonSnapshot& snapshot) {
+    const dungeon::DungeonSnapshot& snapshot,
+    const HostFrameInput& input) {
     if (!open_ || runtime.session() == nullptr) return false;
     dungeon::DungeonSession& session = *runtime.session();
     sync(session, snapshot);
     const items::ItemOwnershipState& state = session.item_state();
     const InventoryLayout layout = inventory_layout(GetScreenWidth(), GetScreenHeight());
     const Rectangle viewport = grid_viewport(layout.grid);
     const int columns = grid_columns(layout.grid);
-    const float wheel = GetMouseWheelMove();
+    const float wheel = input.mouse_wheel;
     if (wheel != 0.0F) {
         scroll_rows_ = clamp_inventory_scroll_rows(view_cache_.filtered_indices.size(), columns,
             scroll_rows_ - wheel, viewport.height, kCellHeight);
     }
-    const bool left_pressed = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
-    const bool right_pressed = IsMouseButtonPressed(MOUSE_BUTTON_RIGHT);
+    const bool left_pressed = input.mouse_left_pressed;
+    const bool right_pressed = input.mouse_right_pressed;
     if (!left_pressed && !right_pressed) return false;
-    const Vector2 mouse = GetMousePosition();
+    const Vector2 mouse = input.mouse_position;
     if (left_pressed && contains(slot_filter_button(layout.grid), mouse)) {
         if (!filter_.slot.has_value()) filter_.slot = items::ItemSlot::weapon;
         else if (*filter_.slot == items::ItemSlot::accessory) filter_.slot.reset();
         else filter_.slot = static_cast<items::ItemSlot>(
             static_cast<std::uint8_t>(*filter_.slot) + 1U);
         sync(session, snapshot);
         scroll_rows_ = 0.0F;
         return false;
     }
     if (left_pressed && contains(rarity_filter_button(layout.grid), mouse)) {
@@ -257,23 +258,21 @@ bool InventoryRenderer::process_input(DungeonRuntime& runtime,
     const VisibleGridRange visible = visible_grid_range(view_cache_.filtered_indices.size(),
         columns, scroll_rows_, viewport.height, kCellHeight);
     for (std::size_t offset = 0U; offset < visible.count; ++offset) {
         const std::size_t filtered_position = visible.first + offset;
         const Rectangle cell = grid_item_rectangle(viewport, columns,
             filtered_position, scroll_rows_);
         if (!contains(cell, mouse)) continue;
         const items::ItemInstance& item = state.items[
             view_cache_.filtered_indices[filtered_position]];
         selected_item_id_ = item.id;
-        const bool recipe_toggle = right_pressed
-            || platform_key_down(KEY_LEFT_CONTROL)
-            || platform_key_down(KEY_RIGHT_CONTROL);
+        const bool recipe_toggle = right_pressed || input.control_down;
         if (recipe_toggle) {
             static_cast<void>(toggle_recipe_selection(recipe_, item.id));
         }
         comparison_generation_ = ~std::uint64_t{0U};
         sync(session, snapshot);
         if (recipe_toggle) {
             click_tracker_ = {};
             return false;
         }
         if (register_inventory_click(click_tracker_, item.id, GetTime())
diff --git a/src/platform/raylib/inventory_renderer.hpp b/src/platform/raylib/inventory_renderer.hpp
index e9f1ec4..2f39aad 100644
--- a/src/platform/raylib/inventory_renderer.hpp
+++ b/src/platform/raylib/inventory_renderer.hpp
@@ -7,29 +7,31 @@
 
 namespace arpg::dungeon {
 class DungeonSession;
 struct DungeonSnapshot;
 }
 
 namespace arpg::platform {
 
 class DungeonRuntime;
 struct DungeonRenderStatus;
+struct HostFrameInput;
 
 class InventoryRenderer final {
 public:
     void open(const dungeon::DungeonSession& session,
         const dungeon::DungeonSnapshot& snapshot);
     void close() noexcept;
     [[nodiscard]] bool is_open() const noexcept;
     [[nodiscard]] bool process_input(DungeonRuntime& runtime,
-        const dungeon::DungeonSnapshot& snapshot);
+        const dungeon::DungeonSnapshot& snapshot,
+        const HostFrameInput& input);
     void draw(const dungeon::DungeonSession& session,
         const dungeon::DungeonSnapshot& snapshot,
         const DungeonRenderStatus& status);
 
 private:
     void sync(const dungeon::DungeonSession& session,
         const dungeon::DungeonSnapshot& snapshot);
     void refresh_comparison(const dungeon::DungeonSession& session,
         const dungeon::DungeonSnapshot& snapshot);
     [[nodiscard]] const items::ItemInstance* selected_item(
diff --git a/src/platform/raylib/raylib_host.cpp b/src/platform/raylib/raylib_host.cpp
index 9fa88db..b692ea3 100644
--- a/src/platform/raylib/raylib_host.cpp
+++ b/src/platform/raylib/raylib_host.cpp
@@ -557,21 +557,21 @@ HostExitCode run_raylib_host(const RaylibHostConfig& config) noexcept {
                 && frame_input.keys.passives
                 && passive_overlay_can_toggle(inventory.is_open())
                 && passive_tree_can_open(current)) {
                 passive_overlay_open = !passive_overlay_open;
             }
             if ((!inventory.is_open() || !death_gate.forward_gameplay)
                     && death_gate.debug_toggle) {
                 draw_debug = !draw_debug;
             }
             if (death_gate.forward_gameplay && inventory.is_open()
-                && inventory.process_input(runtime, current)) {
+                && inventory.process_input(runtime, current, frame_input)) {
                 current = session->snapshot();
                 previous = current;
                 drain_events(*session, renderer, feedback, audio);
                 if (runtime.state() != DungeonRuntimeState::running) {
                     inventory.close();
                     fixed_step.clear_accumulator();
                     inventory_toggled_this_frame = true;
                 }
             }
             const PassiveOverlayInputGate passive_input_gate = passive_overlay_input_gate(
diff --git a/tests/platform/host_input_source_test.cmake b/tests/platform/host_input_source_test.cmake
index 4b9a8e7..a5cbe73 100644
--- a/tests/platform/host_input_source_test.cmake
+++ b/tests/platform/host_input_source_test.cmake
@@ -1,63 +1,90 @@
 if(NOT DEFINED RAYLIB_SOURCE_DIR)
     message(FATAL_ERROR "RAYLIB_SOURCE_DIR is required")
 endif()
 
 file(READ "${RAYLIB_SOURCE_DIR}/host_input.cpp" HOST_INPUT_SOURCE)
 file(READ "${RAYLIB_SOURCE_DIR}/raylib_host.cpp" HOST_SOURCE)
+file(READ "${RAYLIB_SOURCE_DIR}/inventory_renderer.cpp" INVENTORY_SOURCE)
+
+function(strip_non_code SOURCE OUT_SOURCE)
+    set(CODE "${SOURCE}")
+    string(REGEX REPLACE "/\\*([^*]|\\*+[^*/])*\\*+/" "" CODE "${CODE}")
+    string(REGEX REPLACE "//[^\r\n]*" "" CODE "${CODE}")
+    string(REGEX REPLACE "\"([^\"\\\\]|\\\\.)*\"" "\"\"" CODE "${CODE}")
+    set("${OUT_SOURCE}" "${CODE}" PARENT_SCOPE)
+endfunction()
+
+strip_non_code("${HOST_INPUT_SOURCE}" HOST_INPUT_CODE)
+strip_non_code("${HOST_SOURCE}" HOST_CODE)
+strip_non_code("${INVENTORY_SOURCE}" INVENTORY_CODE)
 
 function(require_match_count SOURCE PATTERN EXPECTED LABEL)
     string(REGEX MATCHALL "${PATTERN}" MATCHES "${SOURCE}")
     list(LENGTH MATCHES ACTUAL)
     if(NOT ACTUAL EQUAL EXPECTED)
         message(FATAL_ERROR
             "${LABEL}: expected ${EXPECTED} source matches, found ${ACTUAL}")
     endif()
 endfunction()
 
 require_match_count(
-    "${HOST_INPUT_SOURCE}"
+    "${HOST_INPUT_CODE}"
     "platform_key_pressed[ \t\r\n]*\\("
     1
     "pressed sampling sites")
 require_match_count(
-    "${HOST_INPUT_SOURCE}"
+    "${HOST_INPUT_CODE}"
     "platform_key_down[ \t\r\n]*\\("
     1
     "down sampling sites")
 require_match_count(
-    "${HOST_INPUT_SOURCE}"
+    "${HOST_INPUT_CODE}"
     "IsMouseButtonPressed[ \t\r\n]*\\("
-    1
+    2
     "mouse pressed sampling sites")
 require_match_count(
-    "${HOST_INPUT_SOURCE}"
+    "${HOST_INPUT_CODE}"
     "GetMousePosition[ \t\r\n]*\\("
     1
     "mouse position sampling sites")
 require_match_count(
-    "${HOST_INPUT_SOURCE}"
+    "${HOST_INPUT_CODE}"
+    "GetMouseWheelMove[ \t\r\n]*\\("
+    1
+    "mouse wheel sampling sites")
+require_match_count(
+    "${HOST_INPUT_CODE}"
     "IsWindowFocused[ \t\r\n]*\\("
     1
     "focus sampling sites")
 
-if(HOST_INPUT_SOURCE MATCHES
+if(HOST_INPUT_CODE MATCHES
         "platform_key_pressed[ \t\r\n]*\\([ \t\r\n]*KEY_(V|R|N)")
     message(FATAL_ERROR "V/R/N must be derived from the stable-key loop")
 endif()
-if(HOST_INPUT_SOURCE MATCHES "(new[ \t\r\n]|malloc[ \t\r\n]*\\()")
-    message(FATAL_ERROR "host input mapping must not allocate")
-endif()
-if(HOST_SOURCE MATCHES
+if(HOST_CODE MATCHES
         "(platform_key_pressed|platform_key_down|IsKeyPressed|IsKeyDown)[ \t\r\n]*\\(")
     message(FATAL_ERROR "raylib_host.cpp must consume the one-sample input module")
 endif()
-if(HOST_SOURCE MATCHES
+if(HOST_CODE MATCHES
         "KEY_(W|A|S|D|J|K|L|E|I|P|R|N|V)([^A-Z0-9_]|$)")
     message(FATAL_ERROR "raylib_host.cpp retains a hard-coded gameplay/global stable key")
 endif()
-if(NOT HOST_SOURCE MATCHES "sample_physical_keys[ \t\r\n]*\\(")
-    message(FATAL_ERROR "raylib host does not sample a physical snapshot")
+require_match_count(
+    "${HOST_CODE}"
+    "sample_physical_keys[ \t\r\n]*\\("
+    1
+    "host physical snapshot calls")
+require_match_count(
+    "${HOST_CODE}"
+    "map_host_frame_input[ \t\r\n]*\\("
+    1
+    "host logical mapping calls")
+if(INVENTORY_CODE MATCHES
+        "(IsMouseButtonPressed|GetMousePosition|GetMouseWheelMove|platform_key_down)[ \t\r\n]*\\(")
+    message(FATAL_ERROR "inventory renderer must consume mapped frame input")
 endif()
-if(NOT HOST_SOURCE MATCHES "map_host_frame_input[ \t\r\n]*\\(")
-    message(FATAL_ERROR "raylib host does not map the sampled snapshot")
+if(NOT HOST_CODE MATCHES
+        "inventory\\.process_input[ \t\r\n]*\\([^;]*frame_input")
+    message(FATAL_ERROR "raylib host must pass mapped frame input to inventory")
 endif()
diff --git a/tests/platform/host_input_tests.cpp b/tests/platform/host_input_tests.cpp
index 9154580..341cc01 100644
--- a/tests/platform/host_input_tests.cpp
+++ b/tests/platform/host_input_tests.cpp
@@ -78,82 +78,101 @@ test::Failure adapter_rejects_unknown_values() noexcept {
     ARPG_REQUIRE(!platform::stable_key_from_raylib(KEY_NULL).has_value());
     ARPG_REQUIRE(!platform::stable_key_from_raylib(KEY_ESCAPE).has_value());
     ARPG_REQUIRE(std::strcmp(
         platform::stable_key_label(settings::StableKey::count), "Unknown") == 0);
     return {};
 }
 
 struct QueryLog final {
     std::array<unsigned, 512> pressed{};
     std::array<unsigned, 512> down{};
-    unsigned mouse_pressed{};
+    unsigned mouse_left_pressed{};
+    unsigned mouse_right_pressed{};
     unsigned mouse_position{};
+    unsigned mouse_wheel{};
     unsigned focus_lost{};
 };
 
 bool query_pressed(void* context, int key) noexcept {
     auto& log = *static_cast<QueryLog*>(context);
     ++log.pressed[static_cast<std::size_t>(key)];
     return key == KEY_V || key == KEY_R || key == KEY_N;
 }
 
 bool query_down(void* context, int key) noexcept {
     auto& log = *static_cast<QueryLog*>(context);
     ++log.down[static_cast<std::size_t>(key)];
-    return key == KEY_W;
+    return key == KEY_W || key == KEY_LEFT_CONTROL;
 }
 
-bool query_mouse_pressed(void* context) noexcept {
-    ++static_cast<QueryLog*>(context)->mouse_pressed;
+bool query_mouse_left_pressed(void* context) noexcept {
+    ++static_cast<QueryLog*>(context)->mouse_left_pressed;
+    return true;
+}
+
+bool query_mouse_right_pressed(void* context) noexcept {
+    ++static_cast<QueryLog*>(context)->mouse_right_pressed;
     return true;
 }
 
 Vector2 query_mouse_position(void* context) noexcept {
     ++static_cast<QueryLog*>(context)->mouse_position;
     return {123.0F, 456.0F};
 }
 
+float query_mouse_wheel(void* context) noexcept {
+    ++static_cast<QueryLog*>(context)->mouse_wheel;
+    return 2.5F;
+}
+
 bool query_focus_lost(void* context) noexcept {
     ++static_cast<QueryLog*>(context)->focus_lost;
     return true;
 }
 
 test::Failure sampler_queries_every_stable_key_once_per_state() noexcept {
     QueryLog log{};
     const platform::PhysicalKeySource source{
         &log,
         &query_pressed,
         &query_down,
-        &query_mouse_pressed,
+        &query_mouse_left_pressed,
+        &query_mouse_right_pressed,
         &query_mouse_position,
+        &query_mouse_wheel,
         &query_focus_lost,
     };
     const platform::PhysicalKeySnapshot snapshot =
         platform::sample_physical_keys(source);
 
     for (std::size_t index = 0U; index < kStableKeyCount; ++index) {
         const int code = kExpectedRaylibKeys[index];
         ARPG_REQUIRE(log.pressed[static_cast<std::size_t>(code)] == 1U);
         ARPG_REQUIRE(log.down[static_cast<std::size_t>(code)] == 1U);
     }
     ARPG_REQUIRE(log.pressed[KEY_ESCAPE] == 1U);
     ARPG_REQUIRE(log.pressed[KEY_ENTER] == 1U);
     ARPG_REQUIRE(log.pressed[KEY_F1] == 1U);
     ARPG_REQUIRE(log.pressed[KEY_F12] == 1U);
     ARPG_REQUIRE(log.pressed[KEY_V] == 1U);
     ARPG_REQUIRE(log.pressed[KEY_R] == 1U);
     ARPG_REQUIRE(log.pressed[KEY_N] == 1U);
-    ARPG_REQUIRE(log.mouse_pressed == 1U);
+    ARPG_REQUIRE(log.mouse_left_pressed == 1U);
+    ARPG_REQUIRE(log.mouse_right_pressed == 1U);
     ARPG_REQUIRE(log.mouse_position == 1U);
+    ARPG_REQUIRE(log.mouse_wheel == 1U);
     ARPG_REQUIRE(log.focus_lost == 1U);
     ARPG_REQUIRE(snapshot.v);
     ARPG_REQUIRE(snapshot.mouse_left);
+    ARPG_REQUIRE(snapshot.mouse_right);
+    ARPG_REQUIRE(snapshot.mouse_wheel == 2.5F);
+    ARPG_REQUIRE(snapshot.down[key_index(settings::StableKey::left_control)]);
     ARPG_REQUIRE(snapshot.focus_lost);
     ARPG_REQUIRE(snapshot.mouse_position.x == 123.0F);
     ARPG_REQUIRE(snapshot.mouse_position.y == 456.0F);
     return {};
 }
 
 test::Failure default_bindings_map_movement_actions_and_overlays() noexcept {
     const settings::SettingsData settings = settings::default_settings();
     platform::PhysicalKeySnapshot snapshot{};
     hold(snapshot, settings::StableKey::w);
@@ -173,20 +192,57 @@ test::Failure default_bindings_map_movement_actions_and_overlays() noexcept {
     ARPG_REQUIRE(input.combat_actions[0]);
     ARPG_REQUIRE(input.combat_actions[1]);
     ARPG_REQUIRE(input.combat_actions[2]);
     ARPG_REQUIRE(input.keys.attack);
     ARPG_REQUIRE(input.keys.e);
     ARPG_REQUIRE(input.keys.inventory);
     ARPG_REQUIRE(input.keys.passives);
     return {};
 }
 
+test::Failure every_setting_action_maps_only_to_its_expected_output() noexcept {
+    const settings::SettingsData settings = settings::default_settings();
+    constexpr std::size_t kActionCount =
+        static_cast<std::size_t>(settings::SettingAction::count);
+    for (std::size_t action_index = 0U; action_index < kActionCount;
+            ++action_index) {
+        const auto action = static_cast<settings::SettingAction>(action_index);
+        const settings::StableKey binding = settings::binding_for(settings, action);
+        platform::PhysicalKeySnapshot snapshot{};
+        if (action_index < 4U) hold(snapshot, binding);
+        else press(snapshot, binding);
+
+        const platform::HostFrameInput input =
+            platform::map_host_frame_input(settings, snapshot);
+        constexpr std::array<std::int8_t, 4> kExpectedX{{0, 0, -1, 1}};
+        constexpr std::array<std::int8_t, 4> kExpectedY{{-1, 1, 0, 0}};
+        const std::int8_t expected_x = action_index < 4U
+            ? kExpectedX[action_index] : 0;
+        const std::int8_t expected_y = action_index < 4U
+            ? kExpectedY[action_index] : 0;
+        ARPG_REQUIRE(input.movement.x == expected_x);
+        ARPG_REQUIRE(input.movement.y == expected_y);
+        ARPG_REQUIRE(input.keys.movement == (action_index < 4U));
+        for (std::size_t combat_index = 0U; combat_index < 3U;
+                ++combat_index) {
+            ARPG_REQUIRE(input.combat_actions[combat_index]
+                == (action_index == combat_index + 4U));
+        }
+        ARPG_REQUIRE(input.keys.attack
+            == (action_index >= 4U && action_index <= 6U));
+        ARPG_REQUIRE(input.keys.e == (action_index == 7U));
+        ARPG_REQUIRE(input.keys.inventory == (action_index == 8U));
+        ARPG_REQUIRE(input.keys.passives == (action_index == 9U));
+    }
+    return {};
+}
+
 test::Failure swapped_bindings_drive_the_new_logical_owners() noexcept {
     settings::SettingsData settings = settings::default_settings();
     ARPG_REQUIRE(settings::assign_or_swap(
         settings, settings::SettingAction::light_attack, settings::StableKey::d));
     ARPG_REQUIRE(settings::binding_for(
         settings, settings::SettingAction::move_right) == settings::StableKey::j);
     ARPG_REQUIRE(settings::validate_settings(settings)
         == settings::SettingsValidationError::none);
 
     platform::PhysicalKeySnapshot snapshot{};
@@ -255,31 +311,38 @@ test::Failure global_keys_and_mouse_are_binding_independent() noexcept {
     settings::SettingsData settings = settings::default_settings();
     ARPG_REQUIRE(settings::assign_or_swap(
         settings, settings::SettingAction::interact, settings::StableKey::q));
     platform::PhysicalKeySnapshot snapshot{};
     snapshot.escape = true;
     snapshot.enter = true;
     snapshot.f1 = true;
     snapshot.f12 = true;
     snapshot.v = true;
     snapshot.mouse_left = true;
+    snapshot.mouse_right = true;
+    snapshot.mouse_wheel = -3.0F;
+    snapshot.down[key_index(settings::StableKey::right_control)] = true;
     snapshot.focus_lost = true;
     snapshot.mouse_position = {17.0F, 29.0F};
 
     const platform::HostFrameInput input =
         platform::map_host_frame_input(settings, snapshot);
     ARPG_REQUIRE(input.keys.escape);
     ARPG_REQUIRE(input.keys.enter);
     ARPG_REQUIRE(input.keys.f1);
     ARPG_REQUIRE(input.keys.f12);
     ARPG_REQUIRE(input.keys.v);
     ARPG_REQUIRE(input.keys.mouse_gameplay);
+    ARPG_REQUIRE(input.mouse_left_pressed);
+    ARPG_REQUIRE(input.mouse_right_pressed);
+    ARPG_REQUIRE(input.mouse_wheel == -3.0F);
+    ARPG_REQUIRE(input.control_down);
     ARPG_REQUIRE(input.keys.focus_lost);
     ARPG_REQUIRE(input.mouse_position.x == 17.0F);
     ARPG_REQUIRE(input.mouse_position.y == 29.0F);
     ARPG_REQUIRE(!input.keys.e);
     return {};
 }
 
 test::Failure valid_r_binding_wins_over_legacy_room_reset() noexcept {
     platform::PhysicalKeySnapshot snapshot{};
     press(snapshot, settings::StableKey::r);
@@ -378,20 +441,22 @@ test::Failure submit_forwards_each_combat_action_to_the_session() noexcept {
             == combat::AttackId::launcher);
     }
     return {};
 }
 
 constexpr test::TestCase kCases[] = {
     {"45 stable keys round-trip with labels", &adapter_round_trips_all_45_keys_with_unique_codes_and_labels},
     {"unknown adapter values rejected", &adapter_rejects_unknown_values},
     {"stable keys sampled once per state", &sampler_queries_every_stable_key_once_per_state},
     {"default logical bindings", &default_bindings_map_movement_actions_and_overlays},
+    {"all setting actions map exhaustively",
+        &every_setting_action_maps_only_to_its_expected_output},
     {"swapped logical bindings", &swapped_bindings_drive_the_new_logical_owners},
     {"movement down and action pressed", &movement_uses_down_and_actions_use_pressed},
     {"global input binding independent", &global_keys_and_mouse_are_binding_independent},
     {"R binding wins over reset", &valid_r_binding_wins_over_legacy_room_reset},
     {"N recovery from stable edge", &recovery_n_uses_the_stable_snapshot_edge},
     {"validated binding emits one combat action", &validated_swap_never_emits_two_combat_actions},
     {"mapper has zero allocations", &mapper_performs_no_heap_allocations},
     {"submit forwards combat actions", &submit_forwards_each_combat_action_to_the_session},
 };
 
diff --git a/tests/platform/platform_test_main.cpp b/tests/platform/platform_test_main.cpp
index 53c5937..e4a853a 100644
--- a/tests/platform/platform_test_main.cpp
+++ b/tests/platform/platform_test_main.cpp
@@ -34,12 +34,12 @@ int main() {
         host_input_suite(),
         dungeon_view_math_suite(),
         passive_tree_view_suite(),
         host_launch_options_suite(),
         dungeon_runtime_suite(),
         inventory_view_math_suite(),
         death_input_gate_suite(),
         death_overlay_view_suite(),
     };
 
-    return arpg::test::run_suites(suites, 114, "stage 11b task 4 host input");
+    return arpg::test::run_suites(suites, 115, "stage 11b task 4 host input");
 }
