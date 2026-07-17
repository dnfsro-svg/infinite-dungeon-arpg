#include "dungeon/dungeon_progression.hpp"
#include "dungeon/room_generation.hpp"
#include "persistence/save_store.hpp"
#include "platform/raylib/raylib_host.hpp"

#include <raylib.h>

#include <array>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>

namespace {

namespace dungeon = arpg::dungeon;
namespace platform = arpg::platform;
namespace persistence = arpg::persistence;

struct SeedChoice final {
    std::uint64_t seed{};
    dungeon::ExitDirection direction{dungeon::ExitDirection::none};
};

std::optional<std::uint64_t> find_hole_seed() {
    for (std::uint64_t seed = 1U; seed < 200000U; ++seed) {
        const auto initial = dungeon::make_initial_run_state(seed, {});
        if (initial.fault == dungeon::DungeonFault::none
                && initial.state.current_room.has_hole) return seed;
    }
    return std::nullopt;
}

std::optional<SeedChoice> find_abyss_seed() {
    constexpr std::array<dungeon::ExitDirection, 4> directions{{
        dungeon::ExitDirection::up, dungeon::ExitDirection::down,
        dungeon::ExitDirection::left, dungeon::ExitDirection::right}};
    for (std::uint64_t seed = 1U; seed < 200000U; ++seed) {
        const auto initial = dungeon::make_initial_run_state(seed, {});
        if (initial.fault != dungeon::DungeonFault::none) continue;
        const auto doors = dungeon::preview_abyss_doors(initial.state.current_room);
        for (std::size_t index = 0U; index < doors.size(); ++index) {
            if (doors[index]) return SeedChoice{seed, directions[index]};
        }
    }
    return std::nullopt;
}

bool run_path(const std::filesystem::path& save_directory,
    std::uint64_t seed, platform::Stage11ValidationScenario scenario,
    const std::filesystem::path& capture,
    dungeon::ExitDirection direction = dungeon::ExitDirection::none,
    std::uint32_t frames = 2400U) {
    platform::RaylibHostConfig config{};
    config.window_title = "Infinite Dungeon - Stage 11-A Formal Validation";
    config.save_directory = save_directory;
    config.new_run_seed = seed;
    config.stage11_validation = scenario;
    config.validation_abyss_direction = static_cast<std::uint8_t>(direction);
    config.validation_steps_per_frame = 64U;
    config.validation_exit_after_presented_frames = frames;
    config.validation_capture_file = capture;
    return platform::run_raylib_host(config) == platform::HostExitCode::success
        && std::filesystem::exists(capture)
        && std::filesystem::file_size(capture) != 0U;
}

std::optional<dungeon::checkpoint::DungeonRunState> load_state(
    const std::filesystem::path& directory) {
    persistence::SaveStore store({directory});
    auto loaded = store.load();
    if (loaded.state != persistence::SaveLoadState::ready) return std::nullopt;
    return std::move(loaded.checkpoint);
}

std::uint64_t death_hash(
    const dungeon::checkpoint::DeathCheckpoint& death) noexcept {
    std::uint64_t hash = 1469598103934665603ULL;
    const auto fold = [&hash](std::uint64_t value) noexcept {
        hash ^= value;
        hash *= 1099511628211ULL;
    };
    fold(death.death_depth);
    fold(death.death_floor_room_index);
    fold(death.target_room.seed);
    fold(death.target_room.depth);
    fold(static_cast<std::uint64_t>(death.source_kind));
    fold(death.source_monster_id);
    fold(death.source_detail_id);
    fold(death.final_damage);
    for (const auto value : death.recent_damage) fold(value);
    return hash;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 1 || argv == nullptr || argv[0] == nullptr) return 2;
    SetTraceLogLevel(LOG_WARNING);
    const auto evidence = std::filesystem::absolute(argv[0]).parent_path()
        / "stage11-death-formal-validation";
    std::error_code error;
    std::filesystem::remove_all(evidence, error);
    if (error) return 3;
    std::filesystem::create_directories(evidence, error);
    if (error) return 4;
    const auto hole = find_hole_seed();
    const auto abyss = find_abyss_seed();
    if (!hole.has_value() || !abyss.has_value()) return 5;

    const auto normal_dir = evidence / "normal-save";
    if (!run_path(normal_dir, 0x11A100U,
            platform::Stage11ValidationScenario::normal_death_recap,
            evidence / "01-normal-death.png")) return 10;
    const auto normal = load_state(normal_dir);
    if (!normal.has_value() || normal->death.lifecycle
            != dungeon::checkpoint::DeathLifecycle::pending_continue) return 11;
    const std::uint64_t normal_hash = death_hash(normal->death);
    const std::uint64_t normal_target = normal->death.target_room.seed;

    if (!run_path(normal_dir, 0x11A100U,
            platform::Stage11ValidationScenario::restart_same_recap,
            evidence / "02-restarted-death.png")) return 12;
    const auto restarted = load_state(normal_dir);
    if (!restarted.has_value() || death_hash(restarted->death) != normal_hash
            || restarted->death.target_room.seed != normal_target) return 13;

    const auto deep_dir = evidence / "deep-save";
    if (!run_path(deep_dir, *hole,
            platform::Stage11ValidationScenario::deep_continue,
            evidence / "03-deep-continued.png",
            dungeon::ExitDirection::none, 4000U)) return 14;
    const auto deep = load_state(deep_dir);
    if (!deep.has_value() || deep->current_room.depth != 1U
            || deep->death.lifecycle
                != dungeon::checkpoint::DeathLifecycle::none) return 15;

    const auto floor_dir = evidence / "floor-one-save";
    if (!run_path(floor_dir, 0x11A200U,
            platform::Stage11ValidationScenario::floor_one_continue,
            evidence / "04-floor-one-continued.png")) return 16;
    const auto floor = load_state(floor_dir);
    if (!floor.has_value() || floor->current_room.depth != 1U
            || floor->death.lifecycle
                != dungeon::checkpoint::DeathLifecycle::none) return 17;

    const auto abyss_dir = evidence / "abyss-save";
    if (!run_path(abyss_dir, abyss->seed,
            platform::Stage11ValidationScenario::abyss_death_recap,
            evidence / "05-abyss-death.png", abyss->direction, 4000U)) return 18;
    const auto abyss_state = load_state(abyss_dir);
    if (!abyss_state.has_value() || !abyss_state->death.death_was_abyss
            || !abyss_state->last_abyss_resolution.valid) return 19;

    std::ofstream summary(evidence / "formal-path-summary.txt", std::ios::trunc);
    if (!summary) return 20;
    summary << "normal_death_recap=PASS target=" << normal_target
            << " hash=" << normal_hash << '\n';
    summary << "restart_same_recap=PASS target="
            << restarted->death.target_room.seed << " hash="
            << death_hash(restarted->death) << '\n';
    summary << "deep_continue=PASS death_depth=2 target_depth=1 final_depth="
            << deep->current_room.depth << '\n';
    summary << "floor_one_continue=PASS target_depth=1 final_depth="
            << floor->current_room.depth << " clamp=1\n";
    summary << "abyss_death_recap=PASS failed_resolution="
            << abyss_state->last_abyss_resolution.valid
            << " abandoned=" << static_cast<unsigned>(
                abyss_state->last_abyss_resolution.abandoned) << '\n';
    return summary ? 0 : 21;
}
