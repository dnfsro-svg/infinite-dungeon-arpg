#include "host_window_lifetime.hpp"

#include "raylib_host.hpp"
#include "window_settings.hpp"

#include <raylib.h>

namespace arpg::platform {

HostWindowLifetime::HostWindowLifetime(HostWindowBackend backend) noexcept
    : backend_(backend) {}

bool HostWindowLifetime::initialize(
    const RaylibHostConfig& config,
    const settings::SettingsData& settings) noexcept {
    backend_.set_config_flags(initial_window_flags(settings));
    backend_.init_window(config.window_width, config.window_height,
        config.window_title);
    ready_ = backend_.is_window_ready();
    return ready_;
}

void HostWindowLifetime::close() noexcept {
    if (!ready_) return;
    ready_ = false;
    backend_.close_window();
}

bool HostWindowLifetime::ready() const noexcept {
    return ready_;
}

HostWindowLifetime::~HostWindowLifetime() {
    close();
}

HostWindowBackend raylib_host_window_backend() noexcept {
    return {&SetConfigFlags, &InitWindow, &IsWindowReady, &CloseWindow};
}

}  // namespace arpg::platform
