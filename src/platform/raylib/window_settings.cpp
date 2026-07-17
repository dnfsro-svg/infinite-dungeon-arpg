#include "window_settings.hpp"

#include <raylib.h>

namespace arpg::platform {
namespace {

constexpr int kWindowedWidth = 1280;
constexpr int kWindowedHeight = 720;

struct AppliedFields final {
    bool volume{};
    bool window_mode{};
    bool vsync{};
};

void restore_applied_fields(
    const settings::SettingsData& current,
    WindowSettingsBackend backend,
    AppliedFields applied) noexcept {
    if (applied.vsync) {
        static_cast<void>(backend.set_vsync(
            backend.context, current.vsync_enabled));
    }
    if (applied.window_mode) {
        static_cast<void>(backend.set_window_mode(
            backend.context, current.window_mode));
    }
    if (applied.volume) {
        static_cast<void>(backend.set_volume(
            backend.context, current.master_sfx_percent));
    }
}

[[nodiscard]] bool raylib_set_volume(
    void*,
    std::uint8_t percent) noexcept {
    if (!IsAudioDeviceReady()) return false;
    SetMasterVolume(master_volume_fraction(percent));
    return true;
}

[[nodiscard]] settings::WindowMode raylib_window_mode(void*) noexcept {
    return IsWindowState(FLAG_BORDERLESS_WINDOWED_MODE)
            || IsWindowState(FLAG_FULLSCREEN_MODE)
        ? settings::WindowMode::fullscreen
        : settings::WindowMode::windowed;
}

[[nodiscard]] bool raylib_set_window_mode(
    void*,
    settings::WindowMode mode) noexcept {
    if (!IsWindowReady()) return false;
    if (mode == settings::WindowMode::fullscreen) {
        SetWindowState(FLAG_BORDERLESS_WINDOWED_MODE);
        return true;
    }

    ClearWindowState(
        FLAG_BORDERLESS_WINDOWED_MODE | FLAG_FULLSCREEN_MODE);
    SetWindowSize(kWindowedWidth, kWindowedHeight);
    const int monitor = GetCurrentMonitor();
    if (monitor < 0 || monitor >= GetMonitorCount()) return false;
    const Vector2 origin = GetMonitorPosition(monitor);
    const int x = static_cast<int>(origin.x)
        + (GetMonitorWidth(monitor) - kWindowedWidth) / 2;
    const int y = static_cast<int>(origin.y)
        + (GetMonitorHeight(monitor) - kWindowedHeight) / 2;
    SetWindowPosition(x, y);
    return true;
}

[[nodiscard]] bool raylib_set_vsync(void*, bool enabled) noexcept {
    if (!IsWindowReady()) return false;
    if (enabled) SetWindowState(FLAG_VSYNC_HINT);
    else ClearWindowState(FLAG_VSYNC_HINT);
    return true;
}

[[nodiscard]] bool raylib_vsync(void*) noexcept {
    return IsWindowState(FLAG_VSYNC_HINT);
}

}  // namespace

static_assert(RAYLIB_VERSION_MAJOR == 6, "raylib 6.x required");
static_assert(RAYLIB_VERSION_MINOR == 0, "raylib 6.0 required");
static_assert(RAYLIB_VERSION_PATCH == 0, "raylib 6.0.0 required");

LiveSettingsResult apply_live_settings(
    const settings::SettingsData& current,
    const settings::SettingsData& desired,
    WindowSettingsBackend backend) noexcept {
    if (settings::validate_settings(current)
            != settings::SettingsValidationError::none
        || settings::validate_settings(desired)
            != settings::SettingsValidationError::none) {
        return LiveSettingsResult::invalid;
    }

    const bool volume_changed = current.master_sfx_percent
        != desired.master_sfx_percent;
    const bool mode_changed = current.window_mode != desired.window_mode;
    const bool vsync_changed = current.vsync_enabled != desired.vsync_enabled;
    if ((volume_changed && backend.set_volume == nullptr)
        || (mode_changed
            && (backend.set_window_mode == nullptr
                || backend.window_mode == nullptr))
        || (vsync_changed
            && (backend.set_vsync == nullptr || backend.vsync == nullptr))) {
        return LiveSettingsResult::backend_failed;
    }

    AppliedFields applied{};
    if (volume_changed) {
        applied.volume = true;
        if (!backend.set_volume(
                backend.context, desired.master_sfx_percent)) {
            restore_applied_fields(current, backend, applied);
            return LiveSettingsResult::backend_failed;
        }
    }

    if (mode_changed) {
        applied.window_mode = true;
        if (!backend.set_window_mode(backend.context, desired.window_mode)) {
            restore_applied_fields(current, backend, applied);
            return LiveSettingsResult::backend_failed;
        }
        if (backend.window_mode(backend.context) != desired.window_mode) {
            restore_applied_fields(current, backend, applied);
            return LiveSettingsResult::readback_failed;
        }
    }

    if (vsync_changed) {
        applied.vsync = true;
        if (!backend.set_vsync(backend.context, desired.vsync_enabled)) {
            restore_applied_fields(current, backend, applied);
            return LiveSettingsResult::backend_failed;
        }
        if (backend.vsync(backend.context) != desired.vsync_enabled) {
            restore_applied_fields(current, backend, applied);
            return LiveSettingsResult::readback_failed;
        }
    }

    return LiveSettingsResult::applied;
}

LiveSettingsResult rollback_live_settings(
    const settings::SettingsData& previewed,
    const settings::SettingsData& committed,
    WindowSettingsBackend backend) noexcept {
    return apply_live_settings(previewed, committed, backend);
}

WindowSettingsBackend raylib_window_settings_backend() noexcept {
    return {nullptr, &raylib_set_volume, &raylib_set_window_mode,
        &raylib_set_vsync, &raylib_window_mode, &raylib_vsync};
}

float master_volume_fraction(std::uint8_t percent) noexcept {
    return static_cast<float>(percent) / 100.0F;
}

unsigned int initial_window_flags(
    const settings::SettingsData& committed) noexcept {
    unsigned int flags = FLAG_WINDOW_RESIZABLE;
    if (committed.window_mode == settings::WindowMode::fullscreen) {
        flags |= FLAG_FULLSCREEN_MODE;
    }
    if (committed.vsync_enabled) flags |= FLAG_VSYNC_HINT;
    return flags;
}

}  // namespace arpg::platform
