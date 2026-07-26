#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace arpg::launcher {

enum class InstallationState : std::uint8_t {
    ready,
    invalid_launcher_path,
    missing_game_executable,
    missing_asset_directory,
};

struct InstallationStatus {
    InstallationState state;
    std::filesystem::path application_directory;
    std::filesystem::path game_executable;
    std::filesystem::path missing_path;

    bool ready() const noexcept;
};

InstallationStatus inspect_installation(const std::filesystem::path&) noexcept;

struct LaunchRequest {
    std::filesystem::path application;
    std::filesystem::path working_directory;
    std::wstring command_line;
};

std::optional<LaunchRequest> make_launch_request(const InstallationStatus&) noexcept;
std::optional<std::filesystem::path> default_save_directory(std::wstring_view) noexcept;
std::wstring installation_message(const InstallationStatus&);

}  // namespace arpg::launcher
