# Review package: 84f5334..33f544d

## Commits
33f544d feat: model pause and settings navigation

## Files changed
 src/platform/raylib/CMakeLists.txt                 |   1 +
 src/platform/raylib/pause_menu_state.cpp           | 251 ++++++++++
 src/platform/raylib/pause_menu_state.hpp           |  63 +++
 tests/platform/CMakeLists.txt                      |  11 +
 .../platform/pause_menu_state_boundary_test.cmake  |  59 +++
 tests/platform/pause_menu_state_tests.cpp          | 527 +++++++++++++++++++++
 tests/platform/platform_test_main.cpp              |   4 +-
 7 files changed, 915 insertions(+), 1 deletion(-)

## Diff
diff --git a/src/platform/raylib/CMakeLists.txt b/src/platform/raylib/CMakeLists.txt
index cbae86a..5a33051 100644
--- a/src/platform/raylib/CMakeLists.txt
+++ b/src/platform/raylib/CMakeLists.txt
@@ -10,20 +10,21 @@ add_library(arpg_raylib STATIC
     debug_renderer.cpp
     dungeon_view_math.cpp
     dungeon_runtime.cpp
     host_input.cpp
     host_launch_options.cpp
     hud_renderer.cpp
     inventory_renderer.cpp
     inventory_view_math.cpp
     passive_tree_renderer.cpp
     passive_tree_view_math.cpp
+    pause_menu_state.cpp
     raylib_input.cpp
     raylib_host.cpp
     room_renderer.cpp
     stable_key_raylib.cpp)
 
 target_include_directories(arpg_raylib
     PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
 target_compile_features(arpg_raylib PUBLIC cxx_std_17)
 target_link_libraries(arpg_raylib
     PUBLIC arpg_dungeon arpg_persistence raylib
diff --git a/src/platform/raylib/pause_menu_state.cpp b/src/platform/raylib/pause_menu_state.cpp
new file mode 100644
index 0000000..aaa07fa
--- /dev/null
+++ b/src/platform/raylib/pause_menu_state.cpp
@@ -0,0 +1,251 @@
+#include "pause_menu_state.hpp"
+
+#include <cstddef>
+#include <cstdint>
+#include <limits>
+
+namespace arpg::platform {
+namespace {
+
+constexpr std::size_t kRootRowCount = 3U;
+constexpr std::size_t kSettingsRowCount = 16U;
+constexpr std::size_t kQuitRowCount = 2U;
+constexpr std::size_t kFirstBindingRow = 3U;
+constexpr std::size_t kLastBindingRow = 12U;
+
+constexpr char kInvalidBindingMessage[] = "Key cannot be assigned";
+constexpr char kInvalidSettingsMessage[] = "Settings are invalid";
+constexpr char kRevisionOverflowMessage[] = "Settings revision limit reached";
+
+[[nodiscard]] bool confirmed(const PauseInput& input) noexcept {
+    return input.enter || input.activate;
+}
+
+[[nodiscard]] bool navigate(
+    std::size_t& selected_row,
+    std::size_t row_count,
+    const PauseInput& input) noexcept {
+    selected_row %= row_count;
+    if (input.up) {
+        selected_row = selected_row == 0U ? row_count - 1U : selected_row - 1U;
+        return true;
+    }
+    if (input.down) {
+        selected_row = (selected_row + 1U) % row_count;
+        return true;
+    }
+    return false;
+}
+
+void return_to_root(PauseMenuState& state) noexcept {
+    state.screen = PauseScreen::root;
+    state.selected_row = 0U;
+    state.capture_action.reset();
+}
+
+[[nodiscard]] PauseCommand rollback_to_root(PauseMenuState& state) noexcept {
+    state.draft = state.committed;
+    state.message = nullptr;
+    return_to_root(state);
+    return PauseCommand::rollback;
+}
+
+[[nodiscard]] PauseCommand update_closed(
+    PauseMenuState& state,
+    const PauseContext& context,
+    const PauseInput& input) noexcept {
+    if (!input.escape) {
+        return PauseCommand::none;
+    }
+    if (context.recovery_active || context.death_active || context.inventory_open ||
+        context.passive_open || context.pending_save) {
+        return PauseCommand::none;
+    }
+    state.screen = PauseScreen::root;
+    state.selected_row = 0U;
+    state.capture_action.reset();
+    return PauseCommand::none;
+}
+
+[[nodiscard]] PauseCommand update_root(
+    PauseMenuState& state,
+    const PauseInput& input) noexcept {
+    if (input.escape) {
+        state.screen = PauseScreen::closed;
+        state.selected_row = 0U;
+        state.capture_action.reset();
+        return PauseCommand::resume;
+    }
+    if (navigate(state.selected_row, kRootRowCount, input)) {
+        return PauseCommand::none;
+    }
+    if (!confirmed(input)) {
+        return PauseCommand::none;
+    }
+    if (state.selected_row == 0U) {
+        state.screen = PauseScreen::closed;
+        state.selected_row = 0U;
+        state.capture_action.reset();
+        return PauseCommand::resume;
+    }
+    if (state.selected_row == 1U) {
+        state.screen = PauseScreen::settings;
+        state.selected_row = 0U;
+        state.capture_action.reset();
+        state.draft = state.committed;
+        state.message = nullptr;
+        return PauseCommand::none;
+    }
+    state.screen = PauseScreen::quit_confirm;
+    state.selected_row = 1U;
+    state.capture_action.reset();
+    return PauseCommand::none;
+}
+
+[[nodiscard]] PauseCommand update_volume(
+    PauseMenuState& state,
+    const PauseInput& input) noexcept {
+    const std::uint8_t before = state.draft.master_sfx_percent;
+    if (input.left) {
+        state.draft.master_sfx_percent = before <= 5U
+            ? 0U
+            : static_cast<std::uint8_t>(before - 5U);
+    } else if (input.right) {
+        state.draft.master_sfx_percent = before >= 95U
+            ? 100U
+            : static_cast<std::uint8_t>(before + 5U);
+    }
+    return state.draft.master_sfx_percent == before
+        ? PauseCommand::none
+        : PauseCommand::preview;
+}
+
+[[nodiscard]] PauseCommand update_settings(
+    PauseMenuState& state,
+    const PauseInput& input) noexcept {
+    if (input.escape) {
+        return rollback_to_root(state);
+    }
+    if (navigate(state.selected_row, kSettingsRowCount, input)) {
+        return PauseCommand::none;
+    }
+    if (state.selected_row == 0U) {
+        return update_volume(state, input);
+    }
+    if (state.selected_row == 1U &&
+        (input.left || input.right || confirmed(input))) {
+        state.draft.window_mode =
+            state.draft.window_mode == settings::WindowMode::windowed
+            ? settings::WindowMode::fullscreen
+            : settings::WindowMode::windowed;
+        return PauseCommand::preview;
+    }
+    if (state.selected_row == 2U &&
+        (input.left || input.right || confirmed(input))) {
+        state.draft.vsync_enabled = !state.draft.vsync_enabled;
+        return PauseCommand::preview;
+    }
+    if (!confirmed(input)) {
+        return PauseCommand::none;
+    }
+    if (state.selected_row >= kFirstBindingRow &&
+        state.selected_row <= kLastBindingRow) {
+        state.capture_action = static_cast<settings::SettingAction>(
+            state.selected_row - kFirstBindingRow);
+        state.screen = PauseScreen::capture_binding;
+        state.message = nullptr;
+        return PauseCommand::none;
+    }
+    if (state.selected_row == 13U) {
+        const std::uint64_t revision = state.draft.revision;
+        state.draft = settings::default_settings();
+        state.draft.revision = revision;
+        return PauseCommand::preview;
+    }
+    if (state.selected_row == 14U) {
+        if (settings::validate_settings(state.draft) !=
+            settings::SettingsValidationError::none) {
+            state.message = kInvalidSettingsMessage;
+            return PauseCommand::none;
+        }
+        if (state.committed.revision ==
+            std::numeric_limits<std::uint64_t>::max()) {
+            state.message = kRevisionOverflowMessage;
+            return PauseCommand::none;
+        }
+        state.draft.revision = state.committed.revision + 1U;
+        return PauseCommand::apply;
+    }
+    return rollback_to_root(state);
+}
+
+[[nodiscard]] PauseCommand update_capture(
+    PauseMenuState& state,
+    const PauseContext& context,
+    const PauseInput& input) noexcept {
+    if (input.escape || input.focus_lost || !context.window_focused) {
+        state.screen = PauseScreen::settings;
+        state.capture_action.reset();
+        state.message = nullptr;
+        return PauseCommand::none;
+    }
+    if (!input.captured_key.has_value()) {
+        return PauseCommand::none;
+    }
+    if (!state.capture_action.has_value() ||
+        !settings::assign_or_swap(
+            state.draft, *state.capture_action, *input.captured_key)) {
+        state.message = kInvalidBindingMessage;
+        return PauseCommand::none;
+    }
+    state.screen = PauseScreen::settings;
+    state.capture_action.reset();
+    state.message = nullptr;
+    return PauseCommand::none;
+}
+
+[[nodiscard]] PauseCommand update_quit_confirm(
+    PauseMenuState& state,
+    const PauseInput& input) noexcept {
+    if (input.escape) {
+        return_to_root(state);
+        return PauseCommand::none;
+    }
+    if (navigate(state.selected_row, kQuitRowCount, input)) {
+        return PauseCommand::none;
+    }
+    if (!confirmed(input)) {
+        return PauseCommand::none;
+    }
+    if (state.selected_row == 0U) {
+        return PauseCommand::quit;
+    }
+    return_to_root(state);
+    return PauseCommand::none;
+}
+
+}  // namespace
+
+PauseCommand update_pause_menu(
+    PauseMenuState& state,
+    const PauseContext& context,
+    const PauseInput& input) noexcept {
+    switch (state.screen) {
+        case PauseScreen::closed:
+            return update_closed(state, context, input);
+        case PauseScreen::root:
+            return update_root(state, input);
+        case PauseScreen::settings:
+            return update_settings(state, input);
+        case PauseScreen::capture_binding:
+            return update_capture(state, context, input);
+        case PauseScreen::quit_confirm:
+            return update_quit_confirm(state, input);
+    }
+    state.screen = PauseScreen::closed;
+    state.selected_row = 0U;
+    state.capture_action.reset();
+    return PauseCommand::none;
+}
+
+}  // namespace arpg::platform
diff --git a/src/platform/raylib/pause_menu_state.hpp b/src/platform/raylib/pause_menu_state.hpp
new file mode 100644
index 0000000..9d0c1fc
--- /dev/null
+++ b/src/platform/raylib/pause_menu_state.hpp
@@ -0,0 +1,63 @@
+#pragma once
+
+#include "platform/settings/settings_types.hpp"
+
+#include <cstddef>
+#include <cstdint>
+#include <optional>
+
+namespace arpg::platform {
+
+enum class PauseScreen : std::uint8_t {
+    closed,
+    root,
+    settings,
+    capture_binding,
+    quit_confirm
+};
+
+enum class PauseCommand : std::uint8_t {
+    none,
+    preview,
+    apply,
+    rollback,
+    resume,
+    quit
+};
+
+struct PauseContext final {
+    bool death_active{};
+    bool recovery_active{};
+    bool inventory_open{};
+    bool passive_open{};
+    bool pending_save{};
+    bool window_focused{true};
+};
+
+struct PauseInput final {
+    bool escape{};
+    bool enter{};
+    bool up{};
+    bool down{};
+    bool left{};
+    bool right{};
+    bool activate{};
+    bool focus_lost{};
+    std::optional<settings::StableKey> captured_key{};
+};
+
+struct PauseMenuState final {
+    PauseScreen screen{PauseScreen::closed};
+    std::size_t selected_row{};
+    std::optional<settings::SettingAction> capture_action{};
+    settings::SettingsData committed{};
+    settings::SettingsData draft{};
+    const char* message{};
+};
+
+[[nodiscard]] PauseCommand update_pause_menu(
+    PauseMenuState& state,
+    const PauseContext& context,
+    const PauseInput& input) noexcept;
+
+}  // namespace arpg::platform
diff --git a/tests/platform/CMakeLists.txt b/tests/platform/CMakeLists.txt
index 7b55cb4..bd90b91 100644
--- a/tests/platform/CMakeLists.txt
+++ b/tests/platform/CMakeLists.txt
@@ -3,35 +3,46 @@ if(TARGET arpg_raylib)
         ../core/allocation_probe.cpp
         platform_test_main.cpp
         host_input_tests.cpp
         combat_feedback_tests.cpp
         combat_view_math_tests.cpp
         monster_view_tests.cpp
         dungeon_view_math_tests.cpp
         passive_tree_view_tests.cpp
         host_launch_options_tests.cpp
         dungeon_runtime_tests.cpp
+        pause_menu_state_tests.cpp
         death_input_gate_tests.cpp
         death_overlay_view_tests.cpp)
     target_sources(arpg_platform_tests PRIVATE inventory_view_math_tests.cpp)
     target_compile_features(arpg_platform_tests PRIVATE cxx_std_17)
     target_include_directories(arpg_platform_tests
         PRIVATE "${PROJECT_SOURCE_DIR}/tests/core")
     target_link_libraries(arpg_platform_tests PRIVATE arpg_raylib)
     if(MSVC)
         target_compile_options(arpg_platform_tests PRIVATE /utf-8)
     endif()
     arpg_enable_project_warnings(arpg_platform_tests)
 
     add_test(NAME platform.units COMMAND arpg_platform_tests)
     set_tests_properties(
         platform.units PROPERTIES LABELS "headless;platform")
+
+    add_test(
+        NAME platform.pause_menu_state_boundary
+        COMMAND "${CMAKE_COMMAND}"
+            "-DPAUSE_STATE_HEADER=${PROJECT_SOURCE_DIR}/src/platform/raylib/pause_menu_state.hpp"
+            "-DPAUSE_STATE_SOURCE=${PROJECT_SOURCE_DIR}/src/platform/raylib/pause_menu_state.cpp"
+            -P "${CMAKE_CURRENT_SOURCE_DIR}/pause_menu_state_boundary_test.cmake")
+    set_tests_properties(
+        platform.pause_menu_state_boundary
+        PROPERTIES LABELS "headless;platform;architecture;stage11b")
 endif()
 
 function(arpg_tokenize_dependency_value INPUT_VALUE OUT_TOKENS)
     set(_arpg_token_text "${INPUT_VALUE}")
     foreach(_arpg_delimiter IN ITEMS
             "$" "<" ">" "{" "}" "(" ")" "," ":" "/" "\\" "\"" "'")
         string(REPLACE
             "${_arpg_delimiter}" ";" _arpg_token_text "${_arpg_token_text}")
     endforeach()
 
diff --git a/tests/platform/pause_menu_state_boundary_test.cmake b/tests/platform/pause_menu_state_boundary_test.cmake
new file mode 100644
index 0000000..ff32e76
--- /dev/null
+++ b/tests/platform/pause_menu_state_boundary_test.cmake
@@ -0,0 +1,59 @@
+cmake_minimum_required(VERSION 3.25)
+
+if(NOT DEFINED PAUSE_STATE_HEADER OR NOT DEFINED PAUSE_STATE_SOURCE)
+    message(FATAL_ERROR "Pause state source paths are required")
+endif()
+
+set(_arpg_forbidden_fragments
+    "raylib"
+    "settings_store"
+    "settingsstore"
+    "combat"
+    "dungeon"
+    "<filesystem>"
+    "std::filesystem"
+    "fopen("
+    "ifstream"
+    "ofstream")
+
+function(arpg_pause_source_has_forbidden_dependency SOURCE_TEXT OUT_FOUND OUT_FRAGMENT)
+    string(TOLOWER "${SOURCE_TEXT}" _arpg_source_lower)
+    foreach(_arpg_fragment IN LISTS _arpg_forbidden_fragments)
+        string(FIND "${_arpg_source_lower}" "${_arpg_fragment}" _arpg_position)
+        if(NOT _arpg_position EQUAL -1)
+            set("${OUT_FOUND}" TRUE PARENT_SCOPE)
+            set("${OUT_FRAGMENT}" "${_arpg_fragment}" PARENT_SCOPE)
+            return()
+        endif()
+    endforeach()
+    set("${OUT_FOUND}" FALSE PARENT_SCOPE)
+    set("${OUT_FRAGMENT}" "" PARENT_SCOPE)
+endfunction()
+
+foreach(_arpg_mutation IN ITEMS
+        "#include <raylib.h>"
+        "SettingsStore* store;"
+        "Combat* combat;"
+        "Dungeon* dungeon;"
+        "#include <filesystem>"
+        "std::ifstream input;")
+    arpg_pause_source_has_forbidden_dependency(
+        "${_arpg_mutation}" _arpg_found _arpg_fragment)
+    if(NOT _arpg_found)
+        message(FATAL_ERROR
+            "Pause state boundary self-test missed mutation: ${_arpg_mutation}")
+    endif()
+endforeach()
+
+foreach(_arpg_source IN ITEMS "${PAUSE_STATE_HEADER}" "${PAUSE_STATE_SOURCE}")
+    if(NOT EXISTS "${_arpg_source}")
+        message(FATAL_ERROR "Pause state source missing: ${_arpg_source}")
+    endif()
+    file(READ "${_arpg_source}" _arpg_source_text)
+    arpg_pause_source_has_forbidden_dependency(
+        "${_arpg_source_text}" _arpg_found _arpg_fragment)
+    if(_arpg_found)
+        message(FATAL_ERROR
+            "Pause state source contains forbidden dependency '${_arpg_fragment}': ${_arpg_source}")
+    endif()
+endforeach()
diff --git a/tests/platform/pause_menu_state_tests.cpp b/tests/platform/pause_menu_state_tests.cpp
new file mode 100644
index 0000000..402c6dd
--- /dev/null
+++ b/tests/platform/pause_menu_state_tests.cpp
@@ -0,0 +1,527 @@
+#include "allocation_probe.hpp"
+#include "pause_menu_state.hpp"
+#include "test_framework.hpp"
+
+#include <cstddef>
+#include <cstdint>
+#include <limits>
+
+namespace {
+
+namespace platform = arpg::platform;
+namespace settings = arpg::settings;
+namespace test = arpg::test;
+
+platform::PauseMenuState make_state() noexcept {
+    platform::PauseMenuState state{};
+    state.committed = settings::default_settings();
+    state.draft = state.committed;
+    return state;
+}
+
+platform::PauseCommand update(
+    platform::PauseMenuState& state,
+    platform::PauseInput input = {},
+    platform::PauseContext context = {}) noexcept {
+    return platform::update_pause_menu(state, context, input);
+}
+
+enum class InputKind : std::uint8_t {
+    escape,
+    enter,
+    up,
+    down,
+    left,
+    right,
+    activate,
+    focus_lost
+};
+
+platform::PauseInput input_for(InputKind kind) noexcept {
+    platform::PauseInput input{};
+    switch (kind) {
+        case InputKind::escape: input.escape = true; break;
+        case InputKind::enter: input.enter = true; break;
+        case InputKind::up: input.up = true; break;
+        case InputKind::down: input.down = true; break;
+        case InputKind::left: input.left = true; break;
+        case InputKind::right: input.right = true; break;
+        case InputKind::activate: input.activate = true; break;
+        case InputKind::focus_lost: input.focus_lost = true; break;
+    }
+    return input;
+}
+
+platform::PauseInput capture_input(settings::StableKey key) noexcept {
+    platform::PauseInput input{};
+    input.captured_key = key;
+    return input;
+}
+
+enum class ContextOwner : std::uint8_t {
+    death,
+    recovery,
+    inventory,
+    passive,
+    pending_save
+};
+
+platform::PauseContext context_for(ContextOwner owner) noexcept {
+    platform::PauseContext context{};
+    switch (owner) {
+        case ContextOwner::death: context.death_active = true; break;
+        case ContextOwner::recovery: context.recovery_active = true; break;
+        case ContextOwner::inventory: context.inventory_open = true; break;
+        case ContextOwner::passive: context.passive_open = true; break;
+        case ContextOwner::pending_save: context.pending_save = true; break;
+    }
+    return context;
+}
+
+platform::PauseContext unfocused_context() noexcept {
+    platform::PauseContext context{};
+    context.window_focused = false;
+    return context;
+}
+
+test::Failure closed_is_idle_without_escape() noexcept {
+    auto state = make_state();
+    ARPG_REQUIRE(update(state) == platform::PauseCommand::none);
+    ARPG_REQUIRE(state.screen == platform::PauseScreen::closed);
+    return {};
+}
+
+test::Failure higher_priority_contexts_block_pause_opening() noexcept {
+    const platform::PauseContext contexts[] = {
+        context_for(ContextOwner::death),
+        context_for(ContextOwner::recovery),
+        context_for(ContextOwner::inventory),
+        context_for(ContextOwner::passive),
+        context_for(ContextOwner::pending_save),
+    };
+    for (const auto context : contexts) {
+        auto state = make_state();
+        ARPG_REQUIRE(update(state, input_for(InputKind::escape), context) ==
+            platform::PauseCommand::none);
+        ARPG_REQUIRE(state.screen == platform::PauseScreen::closed);
+    }
+    return {};
+}
+
+test::Failure opening_pause_drops_same_frame_inputs() noexcept {
+    auto state = make_state();
+    platform::PauseInput input{};
+    input.escape = true;
+    input.enter = true;
+    input.down = true;
+    input.activate = true;
+    input.captured_key = settings::StableKey::q;
+    ARPG_REQUIRE(update(state, input) == platform::PauseCommand::none);
+    ARPG_REQUIRE(state.screen == platform::PauseScreen::root);
+    ARPG_REQUIRE(state.selected_row == 0U);
+    return {};
+}
+
+test::Failure root_navigation_wraps_three_rows() noexcept {
+    auto state = make_state();
+    state.screen = platform::PauseScreen::root;
+    ARPG_REQUIRE(update(state, input_for(InputKind::up)) == platform::PauseCommand::none);
+    ARPG_REQUIRE(state.selected_row == 2U);
+    for (std::size_t expected = 0U; expected < 3U; ++expected) {
+        ARPG_REQUIRE(update(state, input_for(InputKind::down)) == platform::PauseCommand::none);
+        ARPG_REQUIRE(state.selected_row == expected);
+    }
+    return {};
+}
+
+test::Failure root_continue_supports_enter_and_escape() noexcept {
+    {
+        auto state = make_state();
+        state.screen = platform::PauseScreen::root;
+        ARPG_REQUIRE(update(state, input_for(InputKind::enter)) == platform::PauseCommand::resume);
+        ARPG_REQUIRE(state.screen == platform::PauseScreen::closed);
+    }
+    {
+        auto state = make_state();
+        state.screen = platform::PauseScreen::root;
+        state.selected_row = 2U;
+        ARPG_REQUIRE(update(state, input_for(InputKind::escape)) == platform::PauseCommand::resume);
+        ARPG_REQUIRE(state.screen == platform::PauseScreen::closed);
+    }
+    return {};
+}
+
+test::Failure opening_settings_copies_committed_and_clears_message() noexcept {
+    auto state = make_state();
+    state.screen = platform::PauseScreen::root;
+    state.selected_row = 1U;
+    state.committed.master_sfx_percent = 35U;
+    state.draft.master_sfx_percent = 80U;
+    state.message = "old error";
+    ARPG_REQUIRE(update(state, input_for(InputKind::activate)) == platform::PauseCommand::none);
+    ARPG_REQUIRE(state.screen == platform::PauseScreen::settings);
+    ARPG_REQUIRE(state.selected_row == 0U);
+    ARPG_REQUIRE(state.draft.master_sfx_percent == 35U);
+    ARPG_REQUIRE(state.message == nullptr);
+    return {};
+}
+
+test::Failure opening_quit_confirmation_defaults_to_back() noexcept {
+    auto state = make_state();
+    state.screen = platform::PauseScreen::root;
+    state.selected_row = 2U;
+    ARPG_REQUIRE(update(state, input_for(InputKind::enter)) == platform::PauseCommand::none);
+    ARPG_REQUIRE(state.screen == platform::PauseScreen::quit_confirm);
+    ARPG_REQUIRE(state.selected_row == 1U);
+    return {};
+}
+
+test::Failure settings_navigation_wraps_all_sixteen_rows() noexcept {
+    auto state = make_state();
+    state.screen = platform::PauseScreen::settings;
+    ARPG_REQUIRE(update(state, input_for(InputKind::up)) == platform::PauseCommand::none);
+    ARPG_REQUIRE(state.selected_row == 15U);
+    for (std::size_t expected = 0U; expected < 16U; ++expected) {
+        ARPG_REQUIRE(update(state, input_for(InputKind::down)) == platform::PauseCommand::none);
+        ARPG_REQUIRE(state.selected_row == expected);
+    }
+    return {};
+}
+
+test::Failure volume_preview_clamps_and_noop_returns_none() noexcept {
+    auto state = make_state();
+    state.screen = platform::PauseScreen::settings;
+    state.selected_row = 0U;
+    state.draft.master_sfx_percent = 5U;
+    ARPG_REQUIRE(update(state, input_for(InputKind::left)) == platform::PauseCommand::preview);
+    ARPG_REQUIRE(state.draft.master_sfx_percent == 0U);
+    ARPG_REQUIRE(update(state, input_for(InputKind::left)) == platform::PauseCommand::none);
+    state.draft.master_sfx_percent = 95U;
+    ARPG_REQUIRE(update(state, input_for(InputKind::right)) == platform::PauseCommand::preview);
+    ARPG_REQUIRE(state.draft.master_sfx_percent == 100U);
+    ARPG_REQUIRE(update(state, input_for(InputKind::right)) == platform::PauseCommand::none);
+    ARPG_REQUIRE(update(state, input_for(InputKind::activate)) == platform::PauseCommand::none);
+    return {};
+}
+
+test::Failure window_mode_preview_toggles_for_all_controls() noexcept {
+    auto state = make_state();
+    state.screen = platform::PauseScreen::settings;
+    state.selected_row = 1U;
+    const platform::PauseInput inputs[] = {
+        input_for(InputKind::left),
+        input_for(InputKind::right),
+        input_for(InputKind::activate),
+        input_for(InputKind::enter)};
+    auto expected = settings::WindowMode::fullscreen;
+    for (const auto input : inputs) {
+        ARPG_REQUIRE(update(state, input) == platform::PauseCommand::preview);
+        ARPG_REQUIRE(state.draft.window_mode == expected);
+        expected = expected == settings::WindowMode::windowed
+            ? settings::WindowMode::fullscreen
+            : settings::WindowMode::windowed;
+    }
+    return {};
+}
+
+test::Failure vsync_preview_toggles_for_all_controls() noexcept {
+    auto state = make_state();
+    state.screen = platform::PauseScreen::settings;
+    state.selected_row = 2U;
+    const platform::PauseInput inputs[] = {
+        input_for(InputKind::left),
+        input_for(InputKind::right),
+        input_for(InputKind::activate),
+        input_for(InputKind::enter)};
+    bool expected = false;
+    for (const auto input : inputs) {
+        ARPG_REQUIRE(update(state, input) == platform::PauseCommand::preview);
+        ARPG_REQUIRE(state.draft.vsync_enabled == expected);
+        expected = !expected;
+    }
+    return {};
+}
+
+test::Failure every_binding_row_enters_matching_capture() noexcept {
+    for (std::size_t action = 0U;
+         action < static_cast<std::size_t>(settings::SettingAction::count);
+         ++action) {
+        auto state = make_state();
+        state.screen = platform::PauseScreen::settings;
+        state.selected_row = action + 3U;
+        const InputKind confirm = action % 2U == 0U
+            ? InputKind::activate
+            : InputKind::enter;
+        ARPG_REQUIRE(update(state, input_for(confirm)) == platform::PauseCommand::none);
+        ARPG_REQUIRE(state.screen == platform::PauseScreen::capture_binding);
+        ARPG_REQUIRE(state.capture_action == static_cast<settings::SettingAction>(action));
+        ARPG_REQUIRE(state.message == nullptr);
+    }
+    return {};
+}
+
+test::Failure valid_capture_assigns_without_preview() noexcept {
+    auto state = make_state();
+    state.screen = platform::PauseScreen::capture_binding;
+    state.selected_row = 7U;
+    state.capture_action = settings::SettingAction::light_attack;
+    ARPG_REQUIRE(update(state, capture_input(settings::StableKey::q)) ==
+        platform::PauseCommand::none);
+    ARPG_REQUIRE(state.screen == platform::PauseScreen::settings);
+    ARPG_REQUIRE(!state.capture_action.has_value());
+    ARPG_REQUIRE(settings::binding_for(
+        state.draft, settings::SettingAction::light_attack) == settings::StableKey::q);
+    ARPG_REQUIRE(state.message == nullptr);
+    return {};
+}
+
+test::Failure occupied_capture_swaps_without_preview() noexcept {
+    auto state = make_state();
+    state.screen = platform::PauseScreen::capture_binding;
+    state.capture_action = settings::SettingAction::light_attack;
+    ARPG_REQUIRE(update(state, capture_input(settings::StableKey::k)) ==
+        platform::PauseCommand::none);
+    ARPG_REQUIRE(state.screen == platform::PauseScreen::settings);
+    ARPG_REQUIRE(settings::binding_for(
+        state.draft, settings::SettingAction::light_attack) == settings::StableKey::k);
+    ARPG_REQUIRE(settings::binding_for(
+        state.draft, settings::SettingAction::jump) == settings::StableKey::j);
+    return {};
+}
+
+test::Failure reserved_and_invalid_capture_remain_active() noexcept {
+    auto state = make_state();
+    state.screen = platform::PauseScreen::capture_binding;
+    state.capture_action = settings::SettingAction::light_attack;
+    const auto original = state.draft;
+    ARPG_REQUIRE(update(state, capture_input(settings::StableKey::v)) ==
+        platform::PauseCommand::none);
+    ARPG_REQUIRE(state.screen == platform::PauseScreen::capture_binding);
+    ARPG_REQUIRE(state.message != nullptr);
+    ARPG_REQUIRE(state.draft.bindings == original.bindings);
+    const char* reserved_message = state.message;
+    ARPG_REQUIRE(update(state, capture_input(settings::StableKey::count)) ==
+        platform::PauseCommand::none);
+    ARPG_REQUIRE(state.screen == platform::PauseScreen::capture_binding);
+    ARPG_REQUIRE(state.message != nullptr);
+    ARPG_REQUIRE(state.message == reserved_message);
+    ARPG_REQUIRE(state.draft.bindings == original.bindings);
+    return {};
+}
+
+test::Failure capture_cancellation_paths_preserve_draft() noexcept {
+    const platform::PauseInput inputs[] = {
+        input_for(InputKind::escape), input_for(InputKind::focus_lost), {}};
+    const platform::PauseContext contexts[] = {
+        {}, {}, unfocused_context()};
+    for (std::size_t index = 0U; index < 3U; ++index) {
+        auto state = make_state();
+        state.screen = platform::PauseScreen::capture_binding;
+        state.selected_row = 5U;
+        state.capture_action = settings::SettingAction::move_left;
+        state.message = "capture error";
+        const auto original = state.draft;
+        ARPG_REQUIRE(update(state, inputs[index], contexts[index]) ==
+            platform::PauseCommand::none);
+        ARPG_REQUIRE(state.screen == platform::PauseScreen::settings);
+        ARPG_REQUIRE(state.selected_row == 5U);
+        ARPG_REQUIRE(!state.capture_action.has_value());
+        ARPG_REQUIRE(state.message == nullptr);
+        ARPG_REQUIRE(state.draft.bindings == original.bindings);
+    }
+    return {};
+}
+
+test::Failure reset_defaults_preserves_revision_and_previews() noexcept {
+    auto state = make_state();
+    state.screen = platform::PauseScreen::settings;
+    state.selected_row = 13U;
+    state.draft.master_sfx_percent = 25U;
+    state.draft.window_mode = settings::WindowMode::fullscreen;
+    state.draft.vsync_enabled = false;
+    state.draft.revision = 42U;
+    ARPG_REQUIRE(update(state, input_for(InputKind::activate)) == platform::PauseCommand::preview);
+    const auto defaults = settings::default_settings();
+    ARPG_REQUIRE(state.draft.master_sfx_percent == defaults.master_sfx_percent);
+    ARPG_REQUIRE(state.draft.window_mode == defaults.window_mode);
+    ARPG_REQUIRE(state.draft.vsync_enabled == defaults.vsync_enabled);
+    ARPG_REQUIRE(state.draft.bindings == defaults.bindings);
+    ARPG_REQUIRE(state.draft.revision == 42U);
+    return {};
+}
+
+test::Failure apply_increments_draft_without_publishing_or_closing() noexcept {
+    auto state = make_state();
+    state.screen = platform::PauseScreen::settings;
+    state.selected_row = 14U;
+    state.committed.revision = 7U;
+    state.draft.revision = 2U;
+    state.draft.master_sfx_percent = 55U;
+    ARPG_REQUIRE(update(state, input_for(InputKind::enter)) == platform::PauseCommand::apply);
+    ARPG_REQUIRE(state.draft.revision == 8U);
+    ARPG_REQUIRE(state.committed.revision == 7U);
+    ARPG_REQUIRE(state.committed.master_sfx_percent == 100U);
+    ARPG_REQUIRE(state.screen == platform::PauseScreen::settings);
+    return {};
+}
+
+test::Failure apply_preserves_host_retry_message() noexcept {
+    auto state = make_state();
+    state.screen = platform::PauseScreen::settings;
+    state.selected_row = 14U;
+    state.message = "save failed; retry";
+    const char* message = state.message;
+    ARPG_REQUIRE(update(state, input_for(InputKind::activate)) == platform::PauseCommand::apply);
+    ARPG_REQUIRE(state.message == message);
+    return {};
+}
+
+test::Failure apply_rejects_invalid_draft() noexcept {
+    auto state = make_state();
+    state.screen = platform::PauseScreen::settings;
+    state.selected_row = 14U;
+    state.draft.master_sfx_percent = 53U;
+    ARPG_REQUIRE(update(state, input_for(InputKind::activate)) == platform::PauseCommand::none);
+    ARPG_REQUIRE(state.screen == platform::PauseScreen::settings);
+    ARPG_REQUIRE(state.draft.revision == 0U);
+    ARPG_REQUIRE(state.message != nullptr);
+    return {};
+}
+
+test::Failure apply_rejects_revision_overflow() noexcept {
+    auto state = make_state();
+    state.screen = platform::PauseScreen::settings;
+    state.selected_row = 14U;
+    state.committed.revision = std::numeric_limits<std::uint64_t>::max();
+    ARPG_REQUIRE(update(state, input_for(InputKind::activate)) == platform::PauseCommand::none);
+    ARPG_REQUIRE(state.draft.revision == 0U);
+    ARPG_REQUIRE(state.message != nullptr);
+    return {};
+}
+
+test::Failure settings_escape_rolls_back_to_committed() noexcept {
+    auto state = make_state();
+    state.screen = platform::PauseScreen::settings;
+    state.draft.master_sfx_percent = 20U;
+    state.message = "error";
+    ARPG_REQUIRE(update(state, input_for(InputKind::escape)) == platform::PauseCommand::rollback);
+    ARPG_REQUIRE(state.screen == platform::PauseScreen::root);
+    ARPG_REQUIRE(state.selected_row == 0U);
+    ARPG_REQUIRE(state.draft.master_sfx_percent == state.committed.master_sfx_percent);
+    ARPG_REQUIRE(state.message == nullptr);
+    return {};
+}
+
+test::Failure settings_cancel_row_rolls_back_to_committed() noexcept {
+    auto state = make_state();
+    state.screen = platform::PauseScreen::settings;
+    state.selected_row = 15U;
+    state.draft.vsync_enabled = false;
+    ARPG_REQUIRE(update(state, input_for(InputKind::enter)) == platform::PauseCommand::rollback);
+    ARPG_REQUIRE(state.screen == platform::PauseScreen::root);
+    ARPG_REQUIRE(state.draft.vsync_enabled == state.committed.vsync_enabled);
+    return {};
+}
+
+test::Failure quit_navigation_wraps_two_rows() noexcept {
+    auto state = make_state();
+    state.screen = platform::PauseScreen::quit_confirm;
+    state.selected_row = 1U;
+    ARPG_REQUIRE(update(state, input_for(InputKind::down)) == platform::PauseCommand::none);
+    ARPG_REQUIRE(state.selected_row == 0U);
+    ARPG_REQUIRE(update(state, input_for(InputKind::up)) == platform::PauseCommand::none);
+    ARPG_REQUIRE(state.selected_row == 1U);
+    return {};
+}
+
+test::Failure quit_back_and_escape_return_to_root() noexcept {
+    {
+        auto state = make_state();
+        state.screen = platform::PauseScreen::quit_confirm;
+        state.selected_row = 1U;
+        ARPG_REQUIRE(update(state, input_for(InputKind::activate)) == platform::PauseCommand::none);
+        ARPG_REQUIRE(state.screen == platform::PauseScreen::root);
+    }
+    {
+        auto state = make_state();
+        state.screen = platform::PauseScreen::quit_confirm;
+        state.selected_row = 0U;
+        ARPG_REQUIRE(update(state, input_for(InputKind::escape)) == platform::PauseCommand::none);
+        ARPG_REQUIRE(state.screen == platform::PauseScreen::root);
+    }
+    return {};
+}
+
+test::Failure quit_confirm_only_row_zero_quits() noexcept {
+    auto state = make_state();
+    state.screen = platform::PauseScreen::quit_confirm;
+    state.selected_row = 0U;
+    ARPG_REQUIRE(update(state, input_for(InputKind::enter)) == platform::PauseCommand::quit);
+    ARPG_REQUIRE(state.screen == platform::PauseScreen::quit_confirm);
+    return {};
+}
+
+test::Failure non_action_inputs_do_not_mutate_settings() noexcept {
+    auto state = make_state();
+    state.screen = platform::PauseScreen::settings;
+    state.selected_row = 8U;
+    const auto original = state.draft;
+    ARPG_REQUIRE(update(state, capture_input(settings::StableKey::q)) ==
+        platform::PauseCommand::none);
+    ARPG_REQUIRE(state.screen == platform::PauseScreen::settings);
+    ARPG_REQUIRE(state.draft.bindings == original.bindings);
+    return {};
+}
+
+test::Failure state_machine_performs_no_heap_allocations() noexcept {
+    auto state = make_state();
+    const std::uint64_t before = test::allocation_count();
+    for (int iteration = 0; iteration < 1000; ++iteration) {
+        state.screen = platform::PauseScreen::closed;
+        ARPG_REQUIRE(update(state, input_for(InputKind::escape)) == platform::PauseCommand::none);
+        state.selected_row = 1U;
+        ARPG_REQUIRE(update(state, input_for(InputKind::activate)) == platform::PauseCommand::none);
+        state.selected_row = 0U;
+        ARPG_REQUIRE(update(state, input_for(InputKind::left)) != platform::PauseCommand::apply);
+        ARPG_REQUIRE(update(state, input_for(InputKind::escape)) == platform::PauseCommand::rollback);
+    }
+    ARPG_REQUIRE(test::allocation_count() == before);
+    return {};
+}
+
+constexpr test::TestCase kCases[] = {
+    {"closed is idle without escape", &closed_is_idle_without_escape},
+    {"higher priority contexts block pause", &higher_priority_contexts_block_pause_opening},
+    {"opening drops same-frame inputs", &opening_pause_drops_same_frame_inputs},
+    {"root navigation wraps", &root_navigation_wraps_three_rows},
+    {"root continue uses enter and escape", &root_continue_supports_enter_and_escape},
+    {"settings copies committed", &opening_settings_copies_committed_and_clears_message},
+    {"quit defaults to back", &opening_quit_confirmation_defaults_to_back},
+    {"settings navigation wraps", &settings_navigation_wraps_all_sixteen_rows},
+    {"volume preview clamps and noops", &volume_preview_clamps_and_noop_returns_none},
+    {"window mode toggles", &window_mode_preview_toggles_for_all_controls},
+    {"vsync toggles", &vsync_preview_toggles_for_all_controls},
+    {"all binding rows capture", &every_binding_row_enters_matching_capture},
+    {"valid capture assigns", &valid_capture_assigns_without_preview},
+    {"occupied capture swaps", &occupied_capture_swaps_without_preview},
+    {"invalid capture remains active", &reserved_and_invalid_capture_remain_active},
+    {"capture cancellation preserves draft", &capture_cancellation_paths_preserve_draft},
+    {"defaults preserve revision", &reset_defaults_preserves_revision_and_previews},
+    {"apply increments without publish", &apply_increments_draft_without_publishing_or_closing},
+    {"apply preserves retry message", &apply_preserves_host_retry_message},
+    {"apply rejects invalid draft", &apply_rejects_invalid_draft},
+    {"apply rejects overflow", &apply_rejects_revision_overflow},
+    {"settings escape rolls back", &settings_escape_rolls_back_to_committed},
+    {"cancel row rolls back", &settings_cancel_row_rolls_back_to_committed},
+    {"quit navigation wraps", &quit_navigation_wraps_two_rows},
+    {"quit back and escape", &quit_back_and_escape_return_to_root},
+    {"quit row zero quits", &quit_confirm_only_row_zero_quits},
+    {"non-action input is ignored", &non_action_inputs_do_not_mutate_settings},
+    {"state machine has zero allocations", &state_machine_performs_no_heap_allocations},
+};
+
+}  // namespace
+
+arpg::test::TestSuite pause_menu_state_suite() noexcept {
+    return arpg::test::make_suite("pause_menu_state", kCases);
+}
diff --git a/tests/platform/platform_test_main.cpp b/tests/platform/platform_test_main.cpp
index e4a853a..35c1a1e 100644
--- a/tests/platform/platform_test_main.cpp
+++ b/tests/platform/platform_test_main.cpp
@@ -9,20 +9,21 @@ arpg::test::TestSuite combat_view_math_suite() noexcept;
 arpg::test::TestSuite monster_view_suite() noexcept;
 arpg::test::TestSuite combat_feedback_suite() noexcept;
 arpg::test::TestSuite host_input_suite() noexcept;
 arpg::test::TestSuite dungeon_view_math_suite() noexcept;
 arpg::test::TestSuite passive_tree_view_suite() noexcept;
 arpg::test::TestSuite host_launch_options_suite() noexcept;
 arpg::test::TestSuite dungeon_runtime_suite() noexcept;
 arpg::test::TestSuite inventory_view_math_suite() noexcept;
 arpg::test::TestSuite death_input_gate_suite() noexcept;
 arpg::test::TestSuite death_overlay_view_suite() noexcept;
+arpg::test::TestSuite pause_menu_state_suite() noexcept;
 
 int main() {
 #if defined(_WIN32) && defined(_DEBUG)
     _set_error_mode(_OUT_TO_STDERR);
     _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
     _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
     _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE);
     _CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
     _set_abort_behavior(_WRITE_ABORT_MSG,
         _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
@@ -32,14 +33,15 @@ int main() {
         monster_view_suite(),
         combat_feedback_suite(),
         host_input_suite(),
         dungeon_view_math_suite(),
         passive_tree_view_suite(),
         host_launch_options_suite(),
         dungeon_runtime_suite(),
         inventory_view_math_suite(),
         death_input_gate_suite(),
         death_overlay_view_suite(),
+        pause_menu_state_suite(),
     };
 
-    return arpg::test::run_suites(suites, 115, "stage 11b task 4 host input");
+    return arpg::test::run_suites(suites, 143, "stage 11b task 5 pause state");
 }
