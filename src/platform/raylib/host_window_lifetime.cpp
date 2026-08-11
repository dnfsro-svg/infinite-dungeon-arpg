#include "host_window_lifetime.hpp"

#include "raylib_host.hpp"
#include "window_settings.hpp"

#undef SetConfigFlags
#undef InitWindow
#undef IsWindowReady
#undef CloseWindow

#include <raylib.h>

#undef SetConfigFlags
#undef InitWindow
#undef IsWindowReady
#undef CloseWindow

#ifdef _WIN32
extern "C" __declspec(dllimport) void* __stdcall ImmAssociateContext(
    void*, void*);
#endif

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

void* detach_ime_context_noexcept() noexcept {
#ifdef _WIN32
    void* const window = GetWindowHandle();
    return window != nullptr ? ImmAssociateContext(window, nullptr) : nullptr;
#else
    return nullptr;
#endif
}

void restore_ime_context_noexcept(void* context) noexcept {
#ifdef _WIN32
    void* const window = GetWindowHandle();
    if (window != nullptr) {
        static_cast<void>(ImmAssociateContext(window, context));
    }
#else
    static_cast<void>(context);
#endif
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
            || !backend_.is_window_ready || !backend_.detach_ime_context
            || !backend_.restore_ime_context || !backend_.close_window) {
        ready_ = false;
        return false;
    }
    backend_.set_config_flags(initial_window_flags(settings));
    backend_.init_window(config.window_width, config.window_height,
        config.window_title);
    ready_ = backend_.is_window_ready();
    if (ready_) {
        detached_ime_context_ = backend_.detach_ime_context();
        ime_context_detached_ = true;
    }
    return ready_;
}

void HostWindowLifetime::close() noexcept {
    if (!ready_) return;
    ready_ = false;
    if (ime_context_detached_) {
        backend_.restore_ime_context(detached_ime_context_);
        detached_ime_context_ = nullptr;
        ime_context_detached_ = false;
    }
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
        &is_window_ready_noexcept, &detach_ime_context_noexcept,
        &restore_ime_context_noexcept, &close_window_noexcept};
}

}  // namespace arpg::platform
