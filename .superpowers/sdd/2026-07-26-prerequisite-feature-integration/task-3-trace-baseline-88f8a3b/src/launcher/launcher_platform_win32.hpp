#pragma once

#include "launcher_core.hpp"

#include <filesystem>
#include <optional>
#include <string>

namespace arpg::launcher {

std::optional<std::filesystem::path> current_executable_path() noexcept;

struct PlatformResult {
    bool ok;
    unsigned long error_code;
    std::wstring message;
};

PlatformResult launch_game(const LaunchRequest&) noexcept;
PlatformResult open_save_directory(const std::filesystem::path&) noexcept;

}  // namespace arpg::launcher
