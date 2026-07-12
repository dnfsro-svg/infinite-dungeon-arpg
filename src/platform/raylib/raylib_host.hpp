#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>

namespace arpg::platform {

struct RaylibHostConfig final {
    int window_width{1280};
    int window_height{720};
    const char* window_title{"Infinite Dungeon - Stage 3 Dungeon Rules"};
    std::optional<std::filesystem::path> save_directory{};
    std::optional<std::uint64_t> new_run_seed{};
};

enum class HostExitCode : int {
    success = 0,
    window_initialization_failed = 1,
    invalid_arguments = 2,
    save_initialization_failed = 3,
};

[[nodiscard]] HostExitCode run_raylib_host(
    const RaylibHostConfig& config) noexcept;

}  // namespace arpg::platform
