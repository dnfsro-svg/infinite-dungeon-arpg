#pragma once

namespace arpg::settings {
struct SettingsData;
}

namespace arpg::platform {

struct RaylibHostConfig;

struct HostWindowBackend final {
    void (*set_config_flags)(unsigned int){};
    void (*init_window)(int, int, const char*){};
    bool (*is_window_ready)(){};
    void (*close_window)(){};
};

class HostWindowLifetime final {
public:
    explicit HostWindowLifetime(HostWindowBackend backend) noexcept;

    [[nodiscard]] bool initialize(
        const RaylibHostConfig& config,
        const settings::SettingsData& settings) noexcept;
    void close() noexcept;
    [[nodiscard]] bool ready() const noexcept;

    ~HostWindowLifetime();

    HostWindowLifetime(const HostWindowLifetime&) = delete;
    HostWindowLifetime& operator=(const HostWindowLifetime&) = delete;

private:
    HostWindowBackend backend_{};
    bool ready_{};
};

[[nodiscard]] HostWindowBackend raylib_host_window_backend() noexcept;

}  // namespace arpg::platform
