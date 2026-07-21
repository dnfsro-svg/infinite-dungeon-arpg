#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>

namespace arpg::platform {

struct HostLaunchOptions final {
    std::optional<std::filesystem::path> save_directory{};
    std::optional<std::filesystem::path> settings_directory{};
    std::optional<std::filesystem::path> screenshot_directory{};
    std::optional<std::uint64_t> new_run_seed{};
};

enum class HostArgumentError : std::uint8_t {
    none,
    unknown_option,
    missing_value,
    invalid_seed,
    duplicate_option,
};

struct HostArgumentResult final {
    HostArgumentError error{HostArgumentError::none};
    HostLaunchOptions options{};
};

[[nodiscard]] HostArgumentResult parse_host_arguments(
    int argc,
    const char* const* argv) noexcept;

}  // namespace arpg::platform
