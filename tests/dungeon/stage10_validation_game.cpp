#include "abyss/abyss_rules.hpp"
#include "dungeon/dungeon_progression.hpp"
#include "dungeon/room_affix.hpp"
#include "dungeon/room_generation.hpp"
#include "persistence/save_store.hpp"
#include "raylib_host.hpp"
#include "stage10_validation_build.hpp"

#include <raylib.h>

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
    std::optional<RootAndDirection> best{};
    std::uint16_t best_population = 0xFFFFU;
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
                const std::uint16_t population =
                    arpg::dungeon::roll_room_density(
                        initial.state.current_room.seed, false).total_count;
                if (population < best_population) {
                    best_population = population;
                    best = RootAndDirection{root, directions[index]};
                    if (population == 300U) return best;
                }
            }
        }
    }
    return best;
}

bool prepare_geared_save(const std::filesystem::path& directory,
    std::uint64_t root) {
    auto initial = arpg::dungeon::make_initial_run_state(
        root, arpg::dungeon::DungeonRules{});
    if (initial.fault != arpg::dungeon::DungeonFault::none) return false;
    if (!arpg::test::install_stage10_validation_build(initial.state)) {
        return false;
    }
    arpg::persistence::SaveStore store({directory});
    return store.commit(initial.state).state
        == arpg::persistence::SaveCommitState::committed;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 1 || argv == nullptr || argv[0] == nullptr) return 2;
    SetTraceLogLevel(LOG_WARNING);
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
    if (!prepare_geared_save(save_directory, selected->root)) return 6;

    arpg::platform::RaylibHostConfig config{};
    config.window_title = "Infinite Dungeon - Stage 10 Door Validation";
    config.save_directory = save_directory;
    config.new_run_seed = selected->root;
    config.stage10_validation =
        arpg::platform::Stage10ValidationScenario::abyss_door;
    config.validation_abyss_direction = static_cast<std::uint8_t>(
        selected->direction);
    config.validation_steps_per_frame = 64U;
    config.validation_exit_after_presented_frames = 1200U;
    config.validation_capture_file = executable.parent_path()
        / "stage10-validation-capture.png";
    return static_cast<int>(arpg::platform::run_raylib_host(config));
}
