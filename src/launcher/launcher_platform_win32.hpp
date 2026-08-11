#pragma once

#include "launcher_core.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace arpg::launcher {

std::optional<std::filesystem::path> current_executable_path() noexcept;

struct PlatformResult {
    bool ok;
    unsigned long error_code;
    std::wstring message;
};

[[nodiscard]] std::wstring format_platform_failure_message(
    std::wstring_view operation,
    unsigned long error_code,
    std::wstring_view system_detail);

PlatformResult launch_game(const LaunchRequest&) noexcept;
PlatformResult open_save_directory(const std::filesystem::path&) noexcept;

}  // namespace arpg::launcher
