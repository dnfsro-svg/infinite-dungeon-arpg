#pragma once

#include <cstdint>

namespace arpg::platform {

struct RaylibHostConfig final {
    int window_width{1280};
    int window_height{720};
    const char* window_title{"Infinite Dungeon - Stage 2 Room Loop"};
    std::uint64_t root_seed{0x6D30305F5241594CULL};
};

enum class HostExitCode : int {
    success = 0,
    window_initialization_failed = 1
};

[[nodiscard]] HostExitCode run_raylib_host(
    const RaylibHostConfig& config) noexcept;

}  // namespace arpg::platform
