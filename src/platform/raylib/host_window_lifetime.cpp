#include "host_window_lifetime.hpp"

#include "raylib_host.hpp"
#include "window_settings.hpp"

#include <raylib.h>

namespace {

void set_config_flags_noexcept(unsigned int flags) noexcept {
    SetConfigFlags(flags);
}

void init_window_noexcept(int width, int height, const char* title) noexcept {
    InitWindow(width, height, title);
}

bool is_window_ready_noexcept() noexcept {
    return IsWindowReady();
}

void close_window_noexcept() noexcept {
    CloseWindow();
}

}  // namespace

namespace arpg::platform {

HostWindowLifetime::HostWindowLifetime(HostWindowBackend backend) noexcept
    : backend_(backend) {}

bool HostWindowLifetime::initialize(
    const RaylibHostConfig& config,
    const settings::SettingsData& settings) noexcept {
    if (!backend_.set_config_flags || !backend_.init_window
            || !backend_.is_window_ready || !backend_.close_window) {
        ready_ = false;
        return false;
    }
    backend_.set_config_flags(initial_window_flags(settings));
    backend_.init_window(config.window_width, config.window_height,
        config.window_title);
    ready_ = backend_.is_window_ready();
    return ready_;
}

void HostWindowLifetime::close() noexcept {
    if (!ready_) return;
    ready_ = false;
    if (backend_.close_window != nullptr) backend_.close_window();
}

bool HostWindowLifetime::ready() const noexcept {
    return ready_;
}

HostWindowLifetime::~HostWindowLifetime() {
    close();
}

HostWindowBackend raylib_host_window_backend() noexcept {
    return {&set_config_flags_noexcept, &init_window_noexcept,
        &is_window_ready_noexcept, &close_window_noexcept};
}

}  // namespace arpg::platform
