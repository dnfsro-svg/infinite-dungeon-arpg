#include "abyss/abyss_rules.hpp"
#include "dungeon/dungeon_progression.hpp"
#include "dungeon/room_generation.hpp"
#include "raylib_host.hpp"

#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>

namespace {

struct RootAndDirection final {
    std::uint64_t root{};
    arpg::dungeon::ExitDirection direction{arpg::dungeon::ExitDirection::none};
};

std::optional<RootAndDirection> find_root() noexcept {
    constexpr std::array<arpg::dungeon::ExitDirection, 4> directions{{
        arpg::dungeon::ExitDirection::up,
        arpg::dungeon::ExitDirection::down,
        arpg::dungeon::ExitDirection::left,
        arpg::dungeon::ExitDirection::right,
    }};
    const arpg::dungeon::DungeonRules rules{};
    for (std::uint64_t root = 1U; root < 200000U; ++root) {
        const auto initial = arpg::dungeon::make_initial_run_state(root, rules);
        if (initial.fault != arpg::dungeon::DungeonFault::none) continue;
        const auto preview = arpg::dungeon::preview_abyss_doors(
            initial.state.current_room);
        for (std::size_t index = 0U; index < directions.size(); ++index) {
            if (!preview[index]) continue;
            const auto next = arpg::dungeon::make_door_transition(
                initial.state, directions[index], rules);
            if (next.fault == arpg::dungeon::DungeonFault::none
                    && next.state.abyss.rule
                        == arpg::abyss::AbyssRuleId::thunderstorm) {
                return RootAndDirection{root, directions[index]};
            }
        }
    }
    return std::nullopt;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 1 || argv == nullptr || argv[0] == nullptr) return 2;
    const auto selected = find_root();
    if (!selected.has_value()) return 3;
    const auto executable = std::filesystem::absolute(argv[0]);
    const auto save_directory = executable.parent_path()
        / "stage10-validation-game";
    std::error_code error;
    std::filesystem::remove_all(save_directory, error);
    if (error) return 4;
    std::filesystem::create_directories(save_directory, error);
    if (error) return 5;

    arpg::platform::RaylibHostConfig config{};
    config.window_title = "Infinite Dungeon - Stage 10 Door Validation";
    config.save_directory = save_directory;
    config.new_run_seed = selected->root;
    config.stage10_validation =
        arpg::platform::Stage10ValidationScenario::abyss_door;
    config.validation_abyss_direction = static_cast<std::uint8_t>(
        selected->direction);
    config.validation_steps_per_frame = 24U;
    config.validation_exit_after_presented_frames = 1200U;
    config.validation_capture_file = executable.parent_path()
        / "stage10-validation-capture.png";
    return static_cast<int>(arpg::platform::run_raylib_host(config));
}
