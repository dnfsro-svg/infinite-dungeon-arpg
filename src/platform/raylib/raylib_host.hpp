#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>

namespace arpg::platform {

enum class Stage10ValidationScenario : std::uint8_t {
    none,
    abyss_door,
    thunderstorm_warning,
    hunting_flames_warning,
    chaos_expansion,
    reward_chest,
    pending_reward,
    exit_confirmation,
    player_death,
    room_reset,
    leave_started,
    restarted_failed,
    abyss_hole_descent,
};

enum class Stage11ValidationScenario : std::uint8_t {
    none,
    normal_death_recap,
    restart_same_recap,
    deep_continue,
    floor_one_continue,
    abyss_death_recap,
};

struct RaylibHostConfig final {
    int window_width{1280};
    int window_height{720};
    const char* window_title{"Infinite Dungeon - Stage 3 Dungeon Rules"};
    std::optional<std::filesystem::path> save_directory{};
    std::optional<std::uint64_t> new_run_seed{};
    bool validation_capture{};
    std::uint32_t validation_exit_after_presented_frames{};
    Stage10ValidationScenario stage10_validation{
        Stage10ValidationScenario::none};
    Stage11ValidationScenario stage11_validation{
        Stage11ValidationScenario::none};
    std::uint8_t validation_abyss_direction{0xFFU};
    std::uint32_t validation_steps_per_frame{};
    std::optional<std::filesystem::path> validation_capture_file{};
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
