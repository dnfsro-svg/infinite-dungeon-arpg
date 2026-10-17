# Review package: 38db8aa..786168d

## Commits
786168d feat: preview and rollback live settings

## Files changed
 src/platform/raylib/CMakeLists.txt       |   3 +-
 src/platform/raylib/window_settings.cpp  | 177 ++++++++++++++
 src/platform/raylib/window_settings.hpp  |  39 ++++
 tests/platform/CMakeLists.txt            |   1 +
 tests/platform/platform_test_main.cpp    |   4 +-
 tests/platform/window_settings_tests.cpp | 388 +++++++++++++++++++++++++++++++
 6 files changed, 610 insertions(+), 2 deletions(-)

## Diff
diff --git a/src/platform/raylib/CMakeLists.txt b/src/platform/raylib/CMakeLists.txt
index 81ce1e4..49c3f32 100644
--- a/src/platform/raylib/CMakeLists.txt
+++ b/src/platform/raylib/CMakeLists.txt
@@ -16,21 +16,22 @@ add_library(arpg_raylib STATIC
     inventory_renderer.cpp
     inventory_view_math.cpp
     passive_tree_renderer.cpp
     passive_tree_view_math.cpp
     pause_menu_state.cpp
     pause_menu_view.cpp
     pause_menu_renderer.cpp
     raylib_input.cpp
     raylib_host.cpp
     room_renderer.cpp
-    stable_key_raylib.cpp)
+    stable_key_raylib.cpp
+    window_settings.cpp)
 
 target_include_directories(arpg_raylib
     PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
 target_compile_features(arpg_raylib PUBLIC cxx_std_17)
 target_link_libraries(arpg_raylib
     PUBLIC arpg_dungeon arpg_persistence raylib
     PRIVATE arpg_core arpg_settings)
 if(WIN32)
     target_link_libraries(arpg_raylib PRIVATE user32)
 endif()
diff --git a/src/platform/raylib/window_settings.cpp b/src/platform/raylib/window_settings.cpp
new file mode 100644
index 0000000..3b8f529
--- /dev/null
+++ b/src/platform/raylib/window_settings.cpp
@@ -0,0 +1,177 @@
+#include "window_settings.hpp"
+
+#include <raylib.h>
+
+namespace arpg::platform {
+namespace {
+
+constexpr int kWindowedWidth = 1280;
+constexpr int kWindowedHeight = 720;
+
+struct AppliedFields final {
+    bool volume{};
+    bool window_mode{};
+    bool vsync{};
+};
+
+void restore_applied_fields(
+    const settings::SettingsData& current,
+    WindowSettingsBackend backend,
+    AppliedFields applied) noexcept {
+    if (applied.vsync) {
+        static_cast<void>(backend.set_vsync(
+            backend.context, current.vsync_enabled));
+    }
+    if (applied.window_mode) {
+        static_cast<void>(backend.set_window_mode(
+            backend.context, current.window_mode));
+    }
+    if (applied.volume) {
+        static_cast<void>(backend.set_volume(
+            backend.context, current.master_sfx_percent));
+    }
+}
+
+[[nodiscard]] bool raylib_set_volume(
+    void*,
+    std::uint8_t percent) noexcept {
+    if (!IsAudioDeviceReady()) return false;
+    SetMasterVolume(master_volume_fraction(percent));
+    return true;
+}
+
+[[nodiscard]] settings::WindowMode raylib_window_mode(void*) noexcept {
+    return IsWindowState(FLAG_BORDERLESS_WINDOWED_MODE)
+            || IsWindowState(FLAG_FULLSCREEN_MODE)
+        ? settings::WindowMode::fullscreen
+        : settings::WindowMode::windowed;
+}
+
+[[nodiscard]] bool raylib_set_window_mode(
+    void*,
+    settings::WindowMode mode) noexcept {
+    if (!IsWindowReady()) return false;
+    if (mode == settings::WindowMode::fullscreen) {
+        SetWindowState(FLAG_BORDERLESS_WINDOWED_MODE);
+        return true;
+    }
+
+    ClearWindowState(
+        FLAG_BORDERLESS_WINDOWED_MODE | FLAG_FULLSCREEN_MODE);
+    SetWindowSize(kWindowedWidth, kWindowedHeight);
+    const int monitor = GetCurrentMonitor();
+    if (monitor < 0 || monitor >= GetMonitorCount()) return false;
+    const Vector2 origin = GetMonitorPosition(monitor);
+    const int x = static_cast<int>(origin.x)
+        + (GetMonitorWidth(monitor) - kWindowedWidth) / 2;
+    const int y = static_cast<int>(origin.y)
+        + (GetMonitorHeight(monitor) - kWindowedHeight) / 2;
+    SetWindowPosition(x, y);
+    return true;
+}
+
+[[nodiscard]] bool raylib_set_vsync(void*, bool enabled) noexcept {
+    if (!IsWindowReady()) return false;
+    if (enabled) SetWindowState(FLAG_VSYNC_HINT);
+    else ClearWindowState(FLAG_VSYNC_HINT);
+    return true;
+}
+
+[[nodiscard]] bool raylib_vsync(void*) noexcept {
+    return IsWindowState(FLAG_VSYNC_HINT);
+}
+
+}  // namespace
+
+static_assert(RAYLIB_VERSION_MAJOR == 6, "raylib 6.x required");
+static_assert(RAYLIB_VERSION_MINOR == 0, "raylib 6.0 required");
+static_assert(RAYLIB_VERSION_PATCH == 0, "raylib 6.0.0 required");
+
+LiveSettingsResult apply_live_settings(
+    const settings::SettingsData& current,
+    const settings::SettingsData& desired,
+    WindowSettingsBackend backend) noexcept {
+    if (settings::validate_settings(current)
+            != settings::SettingsValidationError::none
+        || settings::validate_settings(desired)
+            != settings::SettingsValidationError::none) {
+        return LiveSettingsResult::invalid;
+    }
+
+    const bool volume_changed = current.master_sfx_percent
+        != desired.master_sfx_percent;
+    const bool mode_changed = current.window_mode != desired.window_mode;
+    const bool vsync_changed = current.vsync_enabled != desired.vsync_enabled;
+    if ((volume_changed && backend.set_volume == nullptr)
+        || (mode_changed
+            && (backend.set_window_mode == nullptr
+                || backend.window_mode == nullptr))
+        || (vsync_changed
+            && (backend.set_vsync == nullptr || backend.vsync == nullptr))) {
+        return LiveSettingsResult::backend_failed;
+    }
+
+    AppliedFields applied{};
+    if (volume_changed) {
+        applied.volume = true;
+        if (!backend.set_volume(
+                backend.context, desired.master_sfx_percent)) {
+            restore_applied_fields(current, backend, applied);
+            return LiveSettingsResult::backend_failed;
+        }
+    }
+
+    if (mode_changed) {
+        applied.window_mode = true;
+        if (!backend.set_window_mode(backend.context, desired.window_mode)) {
+            restore_applied_fields(current, backend, applied);
+            return LiveSettingsResult::backend_failed;
+        }
+        if (backend.window_mode(backend.context) != desired.window_mode) {
+            restore_applied_fields(current, backend, applied);
+            return LiveSettingsResult::readback_failed;
+        }
+    }
+
+    if (vsync_changed) {
+        applied.vsync = true;
+        if (!backend.set_vsync(backend.context, desired.vsync_enabled)) {
+            restore_applied_fields(current, backend, applied);
+            return LiveSettingsResult::backend_failed;
+        }
+        if (backend.vsync(backend.context) != desired.vsync_enabled) {
+            restore_applied_fields(current, backend, applied);
+            return LiveSettingsResult::readback_failed;
+        }
+    }
+
+    return LiveSettingsResult::applied;
+}
+
+LiveSettingsResult rollback_live_settings(
+    const settings::SettingsData& previewed,
+    const settings::SettingsData& committed,
+    WindowSettingsBackend backend) noexcept {
+    return apply_live_settings(previewed, committed, backend);
+}
+
+WindowSettingsBackend raylib_window_settings_backend() noexcept {
+    return {nullptr, &raylib_set_volume, &raylib_set_window_mode,
+        &raylib_set_vsync, &raylib_window_mode, &raylib_vsync};
+}
+
+float master_volume_fraction(std::uint8_t percent) noexcept {
+    return static_cast<float>(percent) / 100.0F;
+}
+
+unsigned int initial_window_flags(
+    const settings::SettingsData& committed) noexcept {
+    unsigned int flags = FLAG_WINDOW_RESIZABLE;
+    if (committed.window_mode == settings::WindowMode::fullscreen) {
+        flags |= FLAG_FULLSCREEN_MODE;
+    }
+    if (committed.vsync_enabled) flags |= FLAG_VSYNC_HINT;
+    return flags;
+}
+
+}  // namespace arpg::platform
diff --git a/src/platform/raylib/window_settings.hpp b/src/platform/raylib/window_settings.hpp
new file mode 100644
index 0000000..a4ed938
--- /dev/null
+++ b/src/platform/raylib/window_settings.hpp
@@ -0,0 +1,39 @@
+#pragma once
+
+#include "platform/settings/settings_types.hpp"
+
+#include <cstdint>
+
+namespace arpg::platform {
+
+struct WindowSettingsBackend final {
+    void* context{};
+    bool (*set_volume)(void*, std::uint8_t){};
+    bool (*set_window_mode)(void*, settings::WindowMode){};
+    bool (*set_vsync)(void*, bool){};
+    settings::WindowMode (*window_mode)(void*){};
+    bool (*vsync)(void*){};
+};
+
+enum class LiveSettingsResult : std::uint8_t {
+    applied,
+    invalid,
+    backend_failed,
+    readback_failed
+};
+
+[[nodiscard]] LiveSettingsResult apply_live_settings(
+    const settings::SettingsData& current,
+    const settings::SettingsData& desired,
+    WindowSettingsBackend backend) noexcept;
+[[nodiscard]] LiveSettingsResult rollback_live_settings(
+    const settings::SettingsData& previewed,
+    const settings::SettingsData& committed,
+    WindowSettingsBackend backend) noexcept;
+[[nodiscard]] WindowSettingsBackend raylib_window_settings_backend() noexcept;
+
+[[nodiscard]] float master_volume_fraction(std::uint8_t percent) noexcept;
+[[nodiscard]] unsigned int initial_window_flags(
+    const settings::SettingsData& committed) noexcept;
+
+}  // namespace arpg::platform
diff --git a/tests/platform/CMakeLists.txt b/tests/platform/CMakeLists.txt
index 295af4f..5ef36a7 100644
--- a/tests/platform/CMakeLists.txt
+++ b/tests/platform/CMakeLists.txt
@@ -5,20 +5,21 @@ if(TARGET arpg_raylib)
         host_input_tests.cpp
         combat_feedback_tests.cpp
         combat_view_math_tests.cpp
         monster_view_tests.cpp
         dungeon_view_math_tests.cpp
         passive_tree_view_tests.cpp
         host_launch_options_tests.cpp
         dungeon_runtime_tests.cpp
         pause_menu_state_tests.cpp
         pause_menu_view_tests.cpp
+        window_settings_tests.cpp
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
diff --git a/tests/platform/platform_test_main.cpp b/tests/platform/platform_test_main.cpp
index 8b767ae..d7389fe 100644
--- a/tests/platform/platform_test_main.cpp
+++ b/tests/platform/platform_test_main.cpp
@@ -11,20 +11,21 @@ arpg::test::TestSuite combat_feedback_suite() noexcept;
 arpg::test::TestSuite host_input_suite() noexcept;
 arpg::test::TestSuite dungeon_view_math_suite() noexcept;
 arpg::test::TestSuite passive_tree_view_suite() noexcept;
 arpg::test::TestSuite host_launch_options_suite() noexcept;
 arpg::test::TestSuite dungeon_runtime_suite() noexcept;
 arpg::test::TestSuite inventory_view_math_suite() noexcept;
 arpg::test::TestSuite death_input_gate_suite() noexcept;
 arpg::test::TestSuite death_overlay_view_suite() noexcept;
 arpg::test::TestSuite pause_menu_state_suite() noexcept;
 arpg::test::TestSuite pause_menu_view_suite() noexcept;
+arpg::test::TestSuite window_settings_suite() noexcept;
 
 int main() {
 #if defined(_WIN32) && defined(_DEBUG)
     _set_error_mode(_OUT_TO_STDERR);
     _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
     _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
     _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE);
     _CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
     _set_abort_behavior(_WRITE_ABORT_MSG,
         _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
@@ -36,14 +37,15 @@ int main() {
         host_input_suite(),
         dungeon_view_math_suite(),
         passive_tree_view_suite(),
         host_launch_options_suite(),
         dungeon_runtime_suite(),
         inventory_view_math_suite(),
         death_input_gate_suite(),
         death_overlay_view_suite(),
         pause_menu_state_suite(),
         pause_menu_view_suite(),
+        window_settings_suite(),
     };
 
-    return arpg::test::run_suites(suites, 154, "stage 11b task 6 pause view");
+    return arpg::test::run_suites(suites, 166, "stage 11b task 7 live settings");
 }
diff --git a/tests/platform/window_settings_tests.cpp b/tests/platform/window_settings_tests.cpp
new file mode 100644
index 0000000..4182833
--- /dev/null
+++ b/tests/platform/window_settings_tests.cpp
@@ -0,0 +1,388 @@
+#include "test_framework.hpp"
+
+#include "allocation_probe.hpp"
+#include "window_settings.hpp"
+
+#include <raylib.h>
+
+#include <array>
+#include <cstddef>
+#include <cstdint>
+
+namespace {
+
+using arpg::platform::LiveSettingsResult;
+using arpg::platform::WindowSettingsBackend;
+using arpg::settings::SettingsData;
+using arpg::settings::WindowMode;
+
+enum class BackendEventKind : std::uint8_t {
+    set_volume,
+    set_window_mode,
+    read_window_mode,
+    set_vsync,
+    read_vsync
+};
+
+struct BackendEvent final {
+    BackendEventKind kind{};
+    std::uint8_t volume{};
+    WindowMode mode{WindowMode::windowed};
+    bool enabled{};
+};
+
+struct FakeBackend final {
+    std::uint8_t volume{100U};
+    WindowMode mode{WindowMode::windowed};
+    bool vsync{true};
+    std::array<BackendEvent, 32U> events{};
+    std::array<bool, 32U> fail_on_event{};
+    std::size_t event_count{};
+    bool mismatch_mode_readback{};
+    bool mismatch_vsync_readback{};
+
+    [[nodiscard]] bool record(BackendEvent event) noexcept {
+        const std::size_t index = event_count;
+        if (index < events.size()) events[index] = event;
+        ++event_count;
+        return index >= fail_on_event.size() || !fail_on_event[index];
+    }
+};
+
+bool set_volume(void* context, std::uint8_t percent) noexcept {
+    auto& fake = *static_cast<FakeBackend*>(context);
+    fake.volume = percent;
+    return fake.record({BackendEventKind::set_volume, percent});
+}
+
+bool set_window_mode(void* context, WindowMode mode) noexcept {
+    auto& fake = *static_cast<FakeBackend*>(context);
+    BackendEvent event{};
+    event.kind = BackendEventKind::set_window_mode;
+    event.mode = mode;
+    fake.mode = mode;
+    return fake.record(event);
+}
+
+bool set_vsync(void* context, bool enabled) noexcept {
+    auto& fake = *static_cast<FakeBackend*>(context);
+    BackendEvent event{};
+    event.kind = BackendEventKind::set_vsync;
+    event.enabled = enabled;
+    fake.vsync = enabled;
+    return fake.record(event);
+}
+
+WindowMode window_mode(void* context) noexcept {
+    auto& fake = *static_cast<FakeBackend*>(context);
+    BackendEvent event{};
+    event.kind = BackendEventKind::read_window_mode;
+    static_cast<void>(fake.record(event));
+    if (!fake.mismatch_mode_readback) return fake.mode;
+    return fake.mode == WindowMode::windowed
+        ? WindowMode::fullscreen
+        : WindowMode::windowed;
+}
+
+bool vsync(void* context) noexcept {
+    auto& fake = *static_cast<FakeBackend*>(context);
+    BackendEvent event{};
+    event.kind = BackendEventKind::read_vsync;
+    static_cast<void>(fake.record(event));
+    return fake.mismatch_vsync_readback ? !fake.vsync : fake.vsync;
+}
+
+[[nodiscard]] WindowSettingsBackend backend_for(FakeBackend& fake) noexcept {
+    return {&fake, &set_volume, &set_window_mode, &set_vsync,
+        &window_mode, &vsync};
+}
+
+[[nodiscard]] SettingsData live_settings(
+    std::uint8_t volume,
+    WindowMode mode,
+    bool vsync_enabled) noexcept {
+    SettingsData settings = arpg::settings::default_settings();
+    settings.master_sfx_percent = volume;
+    settings.window_mode = mode;
+    settings.vsync_enabled = vsync_enabled;
+    return settings;
+}
+
+arpg::test::Failure invalid_snapshots_never_call_backend() noexcept {
+    FakeBackend fake{};
+    SettingsData valid = arpg::settings::default_settings();
+    SettingsData invalid = valid;
+    invalid.master_sfx_percent = 101U;
+
+    ARPG_REQUIRE(arpg::platform::apply_live_settings(
+        invalid, valid, backend_for(fake)) == LiveSettingsResult::invalid);
+    ARPG_REQUIRE(fake.event_count == 0U);
+    ARPG_REQUIRE(arpg::platform::apply_live_settings(
+        valid, invalid, backend_for(fake)) == LiveSettingsResult::invalid);
+    ARPG_REQUIRE(fake.event_count == 0U);
+    return {};
+}
+
+arpg::test::Failure valid_noop_accepts_empty_backend() noexcept {
+    SettingsData current = arpg::settings::default_settings();
+    SettingsData desired = current;
+    desired.revision = 19U;
+    desired.bindings[0] = current.bindings[1];
+    desired.bindings[1] = current.bindings[0];
+
+    ARPG_REQUIRE(arpg::platform::apply_live_settings(
+        current, desired, {}) == LiveSettingsResult::applied);
+    return {};
+}
+
+arpg::test::Failure changed_fields_apply_in_stable_order() noexcept {
+    FakeBackend fake{};
+    const SettingsData current = live_settings(100U, WindowMode::windowed, true);
+    const SettingsData desired = live_settings(0U, WindowMode::fullscreen, false);
+
+    ARPG_REQUIRE(arpg::platform::apply_live_settings(
+        current, desired, backend_for(fake)) == LiveSettingsResult::applied);
+    ARPG_REQUIRE(fake.event_count == 5U);
+    ARPG_REQUIRE(fake.events[0].kind == BackendEventKind::set_volume);
+    ARPG_REQUIRE(fake.events[0].volume == 0U);
+    ARPG_REQUIRE(fake.events[1].kind == BackendEventKind::set_window_mode);
+    ARPG_REQUIRE(fake.events[1].mode == WindowMode::fullscreen);
+    ARPG_REQUIRE(fake.events[2].kind == BackendEventKind::read_window_mode);
+    ARPG_REQUIRE(fake.events[3].kind == BackendEventKind::set_vsync);
+    ARPG_REQUIRE(!fake.events[3].enabled);
+    ARPG_REQUIRE(fake.events[4].kind == BackendEventKind::read_vsync);
+    ARPG_REQUIRE(fake.volume == 0U);
+    ARPG_REQUIRE(fake.mode == WindowMode::fullscreen);
+    ARPG_REQUIRE(!fake.vsync);
+    return {};
+}
+
+arpg::test::Failure volume_endpoints_and_fraction_are_exact() noexcept {
+    ARPG_REQUIRE(arpg::platform::master_volume_fraction(0U) == 0.0F);
+    ARPG_REQUIRE(arpg::platform::master_volume_fraction(55U)
+        == 55.0F / 100.0F);
+    ARPG_REQUIRE(arpg::platform::master_volume_fraction(100U) == 1.0F);
+
+    for (const std::uint8_t endpoint : {std::uint8_t{0U}, std::uint8_t{100U}}) {
+        FakeBackend fake{};
+        const std::uint8_t current_volume = endpoint == 0U ? 100U : 0U;
+        fake.volume = current_volume;
+        const SettingsData current = live_settings(
+            current_volume, WindowMode::windowed, true);
+        const SettingsData desired = live_settings(
+            endpoint, WindowMode::windowed, true);
+        ARPG_REQUIRE(arpg::platform::apply_live_settings(
+            current, desired, backend_for(fake)) == LiveSettingsResult::applied);
+        ARPG_REQUIRE(fake.event_count == 1U);
+        ARPG_REQUIRE(fake.events[0].kind == BackendEventKind::set_volume);
+        ARPG_REQUIRE(fake.events[0].volume == endpoint);
+    }
+    return {};
+}
+
+arpg::test::Failure setter_failures_restore_completed_fields() noexcept {
+    const SettingsData current = live_settings(100U, WindowMode::windowed, true);
+    const SettingsData desired = live_settings(0U, WindowMode::fullscreen, false);
+
+    for (std::size_t failed_event : {0U, 1U, 3U}) {
+        FakeBackend fake{};
+        fake.fail_on_event[failed_event] = true;
+        ARPG_REQUIRE(arpg::platform::apply_live_settings(
+            current, desired, backend_for(fake))
+            == LiveSettingsResult::backend_failed);
+        if (failed_event == 0U) {
+            ARPG_REQUIRE(fake.event_count == 2U);
+            ARPG_REQUIRE(fake.events[1].kind == BackendEventKind::set_volume);
+            ARPG_REQUIRE(fake.events[1].volume == 100U);
+        } else if (failed_event == 1U) {
+            ARPG_REQUIRE(fake.event_count == 4U);
+            ARPG_REQUIRE(fake.events[2].kind == BackendEventKind::set_window_mode);
+            ARPG_REQUIRE(fake.events[2].mode == WindowMode::windowed);
+            ARPG_REQUIRE(fake.events[3].kind == BackendEventKind::set_volume);
+            ARPG_REQUIRE(fake.events[3].volume == 100U);
+        } else {
+            ARPG_REQUIRE(fake.event_count == 7U);
+            ARPG_REQUIRE(fake.events[4].kind == BackendEventKind::set_vsync);
+            ARPG_REQUIRE(fake.events[4].enabled);
+            ARPG_REQUIRE(fake.events[5].kind == BackendEventKind::set_window_mode);
+            ARPG_REQUIRE(fake.events[5].mode == WindowMode::windowed);
+            ARPG_REQUIRE(fake.events[6].kind == BackendEventKind::set_volume);
+            ARPG_REQUIRE(fake.events[6].volume == 100U);
+        }
+        ARPG_REQUIRE(fake.volume == 100U);
+        ARPG_REQUIRE(fake.mode == WindowMode::windowed);
+        ARPG_REQUIRE(fake.vsync);
+    }
+    return {};
+}
+
+arpg::test::Failure missing_callbacks_fail_before_backend_calls() noexcept {
+    FakeBackend fake{};
+    const SettingsData current = live_settings(100U, WindowMode::windowed, true);
+    const SettingsData desired = live_settings(0U, WindowMode::fullscreen, false);
+    WindowSettingsBackend backend = backend_for(fake);
+
+    backend.set_volume = nullptr;
+    ARPG_REQUIRE(arpg::platform::apply_live_settings(
+        current, desired, backend) == LiveSettingsResult::backend_failed);
+    ARPG_REQUIRE(fake.event_count == 0U);
+
+    backend = backend_for(fake);
+    backend.set_window_mode = nullptr;
+    ARPG_REQUIRE(arpg::platform::apply_live_settings(
+        current, desired, backend) == LiveSettingsResult::backend_failed);
+    ARPG_REQUIRE(fake.event_count == 0U);
+
+    backend = backend_for(fake);
+    backend.window_mode = nullptr;
+    ARPG_REQUIRE(arpg::platform::apply_live_settings(
+        current, desired, backend) == LiveSettingsResult::backend_failed);
+    ARPG_REQUIRE(fake.event_count == 0U);
+
+    backend = backend_for(fake);
+    backend.set_vsync = nullptr;
+    ARPG_REQUIRE(arpg::platform::apply_live_settings(
+        current, desired, backend) == LiveSettingsResult::backend_failed);
+    ARPG_REQUIRE(fake.event_count == 0U);
+
+    backend = backend_for(fake);
+    backend.vsync = nullptr;
+    ARPG_REQUIRE(arpg::platform::apply_live_settings(
+        current, desired, backend) == LiveSettingsResult::backend_failed);
+    ARPG_REQUIRE(fake.event_count == 0U);
+    return {};
+}
+
+arpg::test::Failure mode_readback_mismatch_restores_mode_then_volume() noexcept {
+    FakeBackend fake{};
+    fake.mismatch_mode_readback = true;
+    const SettingsData current = live_settings(100U, WindowMode::windowed, true);
+    const SettingsData desired = live_settings(0U, WindowMode::fullscreen, true);
+
+    ARPG_REQUIRE(arpg::platform::apply_live_settings(
+        current, desired, backend_for(fake))
+        == LiveSettingsResult::readback_failed);
+    ARPG_REQUIRE(fake.event_count == 5U);
+    ARPG_REQUIRE(fake.events[2].kind == BackendEventKind::read_window_mode);
+    ARPG_REQUIRE(fake.events[3].kind == BackendEventKind::set_window_mode);
+    ARPG_REQUIRE(fake.events[3].mode == WindowMode::windowed);
+    ARPG_REQUIRE(fake.events[4].kind == BackendEventKind::set_volume);
+    ARPG_REQUIRE(fake.events[4].volume == 100U);
+    ARPG_REQUIRE(fake.volume == 100U);
+    ARPG_REQUIRE(fake.mode == WindowMode::windowed);
+    return {};
+}
+
+arpg::test::Failure vsync_readback_mismatch_restores_reverse_order() noexcept {
+    FakeBackend fake{};
+    fake.mismatch_vsync_readback = true;
+    const SettingsData current = live_settings(100U, WindowMode::windowed, true);
+    const SettingsData desired = live_settings(0U, WindowMode::fullscreen, false);
+
+    ARPG_REQUIRE(arpg::platform::apply_live_settings(
+        current, desired, backend_for(fake))
+        == LiveSettingsResult::readback_failed);
+    ARPG_REQUIRE(fake.event_count == 8U);
+    ARPG_REQUIRE(fake.events[4].kind == BackendEventKind::read_vsync);
+    ARPG_REQUIRE(fake.events[5].kind == BackendEventKind::set_vsync);
+    ARPG_REQUIRE(fake.events[5].enabled);
+    ARPG_REQUIRE(fake.events[6].kind == BackendEventKind::set_window_mode);
+    ARPG_REQUIRE(fake.events[6].mode == WindowMode::windowed);
+    ARPG_REQUIRE(fake.events[7].kind == BackendEventKind::set_volume);
+    ARPG_REQUIRE(fake.events[7].volume == 100U);
+    ARPG_REQUIRE(fake.volume == 100U);
+    ARPG_REQUIRE(fake.mode == WindowMode::windowed);
+    ARPG_REQUIRE(fake.vsync);
+    return {};
+}
+
+arpg::test::Failure rollback_uses_the_same_transaction_path() noexcept {
+    FakeBackend fake{};
+    fake.volume = 0U;
+    fake.mode = WindowMode::fullscreen;
+    fake.vsync = false;
+    const SettingsData previewed = live_settings(0U, WindowMode::fullscreen, false);
+    const SettingsData committed = live_settings(100U, WindowMode::windowed, true);
+
+    ARPG_REQUIRE(arpg::platform::rollback_live_settings(
+        previewed, committed, backend_for(fake)) == LiveSettingsResult::applied);
+    ARPG_REQUIRE(fake.event_count == 5U);
+    ARPG_REQUIRE(fake.events[0].kind == BackendEventKind::set_volume);
+    ARPG_REQUIRE(fake.events[0].volume == 100U);
+    ARPG_REQUIRE(fake.events[1].kind == BackendEventKind::set_window_mode);
+    ARPG_REQUIRE(fake.events[1].mode == WindowMode::windowed);
+    ARPG_REQUIRE(fake.events[2].kind == BackendEventKind::read_window_mode);
+    ARPG_REQUIRE(fake.events[3].kind == BackendEventKind::set_vsync);
+    ARPG_REQUIRE(fake.events[3].enabled);
+    ARPG_REQUIRE(fake.events[4].kind == BackendEventKind::read_vsync);
+    return {};
+}
+
+arpg::test::Failure rollback_failures_do_not_replace_original_error() noexcept {
+    FakeBackend fake{};
+    const SettingsData current = live_settings(100U, WindowMode::windowed, true);
+    const SettingsData desired = live_settings(0U, WindowMode::fullscreen, false);
+    fake.fail_on_event[3] = true;
+    fake.fail_on_event[4] = true;
+
+    ARPG_REQUIRE(arpg::platform::apply_live_settings(
+        current, desired, backend_for(fake))
+        == LiveSettingsResult::backend_failed);
+    ARPG_REQUIRE(fake.event_count == 7U);
+    ARPG_REQUIRE(fake.events[4].kind == BackendEventKind::set_vsync);
+    ARPG_REQUIRE(fake.events[5].kind == BackendEventKind::set_window_mode);
+    ARPG_REQUIRE(fake.events[6].kind == BackendEventKind::set_volume);
+    ARPG_REQUIRE(fake.volume == 100U);
+    ARPG_REQUIRE(fake.mode == WindowMode::windowed);
+    ARPG_REQUIRE(fake.vsync);
+    return {};
+}
+
+arpg::test::Failure initial_flags_cover_all_mode_vsync_combinations() noexcept {
+    for (const WindowMode mode : {WindowMode::windowed, WindowMode::fullscreen}) {
+        for (const bool vsync_enabled : {false, true}) {
+            const SettingsData settings = live_settings(100U, mode, vsync_enabled);
+            const unsigned int flags = arpg::platform::initial_window_flags(settings);
+            unsigned int expected = FLAG_WINDOW_RESIZABLE;
+            if (mode == WindowMode::fullscreen) expected |= FLAG_FULLSCREEN_MODE;
+            if (vsync_enabled) expected |= FLAG_VSYNC_HINT;
+            ARPG_REQUIRE(flags == expected);
+        }
+    }
+    return {};
+}
+
+arpg::test::Failure orchestration_does_not_allocate() noexcept {
+    FakeBackend fake{};
+    const SettingsData current = live_settings(100U, WindowMode::windowed, true);
+    const SettingsData desired = live_settings(0U, WindowMode::fullscreen, false);
+    const std::uint64_t before = arpg::test::allocation_count();
+    for (std::size_t iteration = 0U; iteration < 1000U; ++iteration) {
+        fake.event_count = 0U;
+        ARPG_REQUIRE(arpg::platform::apply_live_settings(
+            current, desired, backend_for(fake)) == LiveSettingsResult::applied);
+    }
+    ARPG_REQUIRE(arpg::test::allocation_count() == before);
+    return {};
+}
+
+constexpr arpg::test::TestCase kCases[] = {
+    {"invalid snapshots", &invalid_snapshots_never_call_backend},
+    {"valid noop", &valid_noop_accepts_empty_backend},
+    {"stable apply order", &changed_fields_apply_in_stable_order},
+    {"volume endpoints and fraction", &volume_endpoints_and_fraction_are_exact},
+    {"setter failures rollback", &setter_failures_restore_completed_fields},
+    {"missing callbacks", &missing_callbacks_fail_before_backend_calls},
+    {"mode readback mismatch", &mode_readback_mismatch_restores_mode_then_volume},
+    {"vsync readback mismatch", &vsync_readback_mismatch_restores_reverse_order},
+    {"rollback transaction", &rollback_uses_the_same_transaction_path},
+    {"rollback failure tolerance", &rollback_failures_do_not_replace_original_error},
+    {"initial flags", &initial_flags_cover_all_mode_vsync_combinations},
+    {"no allocation", &orchestration_does_not_allocate},
+};
+
+}  // namespace
+
+arpg::test::TestSuite window_settings_suite() noexcept {
+    return arpg::test::make_suite("window_settings", kCases);
+}
