#include "abyss/abyss_rules.hpp"
#include "combat/monster_catalog.hpp"
#include "dungeon/dungeon_progression.hpp"
#include "dungeon/room_affix.hpp"
#include "dungeon/room_environment.hpp"
#include "dungeon/room_generation.hpp"
#include "dungeon/room_monster_plan_builder.hpp"
#include "persistence/save_store.hpp"
#include "raylib_host.hpp"
#include "stage10_validation_build.hpp"

#include <raylib.h>

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <new>
#include <optional>
#include <string>
#include <utility>

namespace {

// The pending-reward path clears the 25% ordinary-room gate and all 450
// monsters in the 100x room before it can capture the first reward commit.
// Keep headroom for render/simulation scheduling; the scenario exits as soon
// as the required reward state is presented, so this is only a safety ceiling.
constexpr std::uint32_t kPendingRewardMaximumFrames = 4096U;

struct SelectedRun final {
    std::uint64_t root{};
    std::uint64_t target_seed{};
    arpg::dungeon::ExitDirection direction{arpg::dungeon::ExitDirection::none};
    std::uint16_t population{};
    std::uint16_t abyss_population{};
};

std::optional<SelectedRun> find_run(
    arpg::abyss::AbyssRuleId required_rule,
    bool require_hole = false,
    bool require_exact_abyss_population = false,
    bool require_lateral_exit = false,
    std::optional<std::uint64_t> exact_root = std::nullopt) noexcept {
    constexpr std::array<arpg::dungeon::ExitDirection, 4> directions{{
        arpg::dungeon::ExitDirection::up,
        arpg::dungeon::ExitDirection::down,
        arpg::dungeon::ExitDirection::left,
        arpg::dungeon::ExitDirection::right,
    }};
    const arpg::dungeon::DungeonRules rules{};
    const std::uint64_t first_root = exact_root.value_or(1U);
    const std::uint64_t root_limit = exact_root.has_value()
        ? first_root + 1U : 500000U;
    for (std::uint64_t root = first_root; root < root_limit; ++root) {
        const auto initial_result =
            arpg::dungeon::make_initial_run_state(root, rules);
        if (initial_result.fault != arpg::dungeon::DungeonFault::none) continue;
        const auto& initial = initial_result.state;
        const auto preview = arpg::dungeon::preview_abyss_doors(
            initial.current_room);
        for (std::size_t index = 0U; index < directions.size(); ++index) {
            if (!preview[index]) continue;
            if (require_lateral_exit
                    && directions[index] != arpg::dungeon::ExitDirection::left
                    && directions[index] != arpg::dungeon::ExitDirection::right) {
                continue;
            }
            const auto next = arpg::dungeon::make_door_transition(
                initial, directions[index], rules);
            if (next.fault == arpg::dungeon::DungeonFault::none
                    && next.state.current_room.is_abyss
                    && next.state.abyss.rule == required_rule
                    && (!require_hole || next.state.current_room.has_hole)) {
                const std::uint16_t population =
                    arpg::dungeon::roll_room_density(
                        initial.current_room.seed, false).total_count;
                const std::uint16_t abyss_population =
                    arpg::dungeon::roll_room_density(
                        next.state.current_room.seed, true).total_count;
                if (population != 300U
                        || (require_exact_abyss_population
                            && abyss_population != 450U)) {
                    continue;
                }
                return SelectedRun{root, next.state.current_room.seed,
                    directions[index], population, abyss_population};
            }
        }
    }
    return std::nullopt;
}

bool valid_mixed_hole_fixture(const SelectedRun& selected) noexcept {
    const arpg::dungeon::DungeonRules rules{};
    const auto initial = arpg::dungeon::make_initial_run_state(
        selected.root, rules);
    if (initial.fault != arpg::dungeon::DungeonFault::none) return false;
    const auto next = arpg::dungeon::make_door_transition(
        initial.state, selected.direction, rules);
    if (next.fault != arpg::dungeon::DungeonFault::none
            || !next.state.current_room.is_abyss
            || !next.state.current_room.has_hole) {
        return false;
    }
    std::unique_ptr<arpg::combat::RoomMonsterPlan> plan{
        new (std::nothrow) arpg::combat::RoomMonsterPlan{}};
    if (!plan) return false;
    const auto built = arpg::dungeon::build_room_monster_plan(
        next.state.current_room, rules,
        arpg::dungeon::kRoomMonsterGeneratorVersion, *plan);
    if (built.fault != arpg::dungeon::DungeonFault::none
            || plan->monster_count != 450U
            || !arpg::dungeon::room_monster_plan_legal(
                next.state.current_room, *plan)) {
        return false;
    }
    bool has_melee = false;
    bool has_ranged = false;
    bool has_ordinal_19_shooter = false;
    for (std::uint16_t index = 0U; index < plan->monster_count; ++index) {
        const auto& monster = plan->monsters[index];
        const auto* definition = arpg::combat::monster_definition(monster.id);
        if (definition == nullptr) return false;
        has_melee = has_melee || arpg::combat::has_tag(
            *definition, arpg::combat::MonsterTag::melee);
        has_ranged = has_ranged || arpg::combat::has_tag(
            *definition, arpg::combat::MonsterTag::ranged);
        has_ordinal_19_shooter = has_ordinal_19_shooter
            || (monster.spawn_ordinal == 19U
                && monster.id == arpg::combat::MonsterId::lightning_shooter);
    }
    return has_melee && has_ranged && has_ordinal_19_shooter;
}

bool prepare_save(const std::filesystem::path& directory,
    std::uint64_t root, bool geared, bool survival_passives = false,
    bool offense_only = false) {
    std::error_code error;
    std::filesystem::remove_all(directory, error);
    if (error) return false;
    std::filesystem::create_directories(directory, error);
    if (error) return false;
    auto initial_result = arpg::dungeon::make_initial_run_state(
        root, arpg::dungeon::DungeonRules{});
    if (initial_result.fault != arpg::dungeon::DungeonFault::none) {
        return false;
    }
    auto initial = std::move(initial_result.state);
    if ((geared
                && !arpg::test::install_stage10_validation_build(
                    initial))
            || (survival_passives
                && !arpg::test::install_stage10_validation_survival_passives(
                    initial))
            || (offense_only
                && !arpg::test::install_stage10_validation_offense_build(
                    initial))) {
        return false;
    }
    arpg::persistence::SaveStore store({directory});
    return store.commit(initial).state
        == arpg::persistence::SaveCommitState::committed;
}

bool run_scenario(const std::filesystem::path& directory,
    const SelectedRun& selected,
    arpg::platform::Stage10ValidationScenario scenario,
    const std::optional<std::filesystem::path>& capture = std::nullopt,
    std::uint32_t maximum_frames = 1600U) noexcept {
    arpg::platform::RaylibHostConfig config{};
    config.window_title = "Infinite Dungeon - Stage 10 Formal Validation";
    config.save_directory = directory;
    config.new_run_seed = selected.root;
    config.stage10_validation = scenario;
    config.validation_abyss_direction = static_cast<std::uint8_t>(
        selected.direction);
    config.validation_steps_per_frame = 64U;
    config.validation_exit_after_presented_frames = maximum_frames;
    config.validation_capture_file = capture;
    const auto result = arpg::platform::run_raylib_host(config);
    return result == arpg::platform::HostExitCode::success
        && (!capture.has_value()
            || (std::filesystem::exists(*capture)
                && std::filesystem::file_size(*capture) != 0U));
}

std::optional<arpg::dungeon::DungeonRunState> load_state(
    const std::filesystem::path& directory) noexcept {
    arpg::persistence::SaveStore store({directory});
    auto loaded = store.load();
    return loaded.state == arpg::persistence::SaveLoadState::ready
        ? std::optional<arpg::dungeon::DungeonRunState>{
            std::move(loaded.checkpoint)}
        : std::nullopt;
}

bool failed_same_room(const std::filesystem::path& directory,
    std::uint64_t target_seed) noexcept {
    const auto state = load_state(directory);
    return state.has_value()
        && state->current_room.seed == target_seed
        && !state->current_room.is_abyss
        && state->abyss.lifecycle == arpg::abyss::AbyssLifecycle::failed;
}

bool resumed_started_same_room(const std::filesystem::path& directory,
    std::uint64_t target_seed,
    const arpg::dungeon::DungeonRunState& before) noexcept {
    const auto after = load_state(directory);
    return after.has_value()
        && after->current_room.seed == target_seed
        && after->current_room.is_abyss
        && after->abyss.lifecycle == arpg::abyss::AbyssLifecycle::started
        && arpg::dungeon::same_run_state(before, *after);
}

bool continued_from_failed_same_room(
    const std::filesystem::path& directory,
    std::uint64_t failed_seed,
    arpg::abyss::AbyssRuleId expected_rule) noexcept {
    const auto state = load_state(directory);
    if (!state.has_value()) return false;
    const auto& active = state->abyss;
    const auto& history = state->last_abyss_resolution;
    return !state->current_room.is_abyss
        && state->current_room.seed != 0U
        && active.lifecycle == arpg::abyss::AbyssLifecycle::none
        && active.danger == arpg::abyss::AbyssDanger::low
        && active.rule == arpg::abyss::AbyssRuleId::none
        && active.rules_version == 0U
        && active.reward_total == 0U
        && active.generated_mask == 0U
        && active.claimed_mask == 0U
        && active.abandoned_mask == 0U
        && active.reward_revision == 0U
        && state->death.lifecycle
            == arpg::checkpoint::DeathLifecycle::none
        && arpg::checkpoint::valid_death_checkpoint_structural(state->death)
        && history.valid
        && history.lifecycle == arpg::abyss::AbyssLifecycle::failed
        && history.room_seed == failed_seed
        && history.rule == expected_rule
        && history.total > 0U
        && history.generated == 0U
        && history.claimed == 0U
        && history.abandoned == history.total
        && state->last_transition
            == arpg::checkpoint::TransitionKind::death_retreat;
}

bool valid_cleared_reward_save(const std::filesystem::path& directory,
    std::uint64_t target_seed) noexcept {
    const auto state = load_state(directory);
    if (!state.has_value()
            || !state->current_room.is_abyss
            || state->current_room.seed != target_seed
            || state->abyss.lifecycle
                != arpg::abyss::AbyssLifecycle::cleared
            || state->abyss.reward_total == 0U
            || state->abyss.reward_total > 3U) {
        return false;
    }
    const auto valid_mask = static_cast<std::uint8_t>(
        (std::uint8_t{1U} << state->abyss.reward_total) - 1U);
    return state->abyss.generated_mask != 0U
        && (state->abyss.generated_mask
            & static_cast<std::uint8_t>(~valid_mask)) == 0U
        && (state->abyss.claimed_mask
            & static_cast<std::uint8_t>(~state->abyss.generated_mask)) == 0U
        && (state->abyss.abandoned_mask
            & static_cast<std::uint8_t>(~valid_mask)) == 0U
        && (state->abyss.abandoned_mask
            & state->abyss.generated_mask) == 0U;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 1 || argv == nullptr || argv[0] == nullptr) return 2;
    SetTraceLogLevel(LOG_WARNING);
    const auto executable = std::filesystem::absolute(argv[0]);
    const auto root_directory = executable.parent_path()
        / "stage10-formal-game-validation";
    std::error_code error;
    std::filesystem::remove_all(root_directory, error);
    if (error) return 3;
    std::filesystem::create_directories(root_directory, error);
    if (error) return 4;

    const auto thunder = find_run(
        arpg::abyss::AbyssRuleId::thunderstorm,
        false, false, false, 2395U);
    const auto flames = find_run(
        arpg::abyss::AbyssRuleId::hunting_flames,
        false, false, false, 6695U);
    const auto chaos = find_run(
        arpg::abyss::AbyssRuleId::chaos_expansion,
        false, false, false, 31237U);
    const auto rewards = find_run(
        arpg::abyss::AbyssRuleId::life_sacrifice,
        false, true, false, 244541U);
    const auto hole = find_run(
        arpg::abyss::AbyssRuleId::hunting_flames,
        true, true, false, 408182U);
    if (!thunder || !flames || !chaos || !rewards || !hole
            || !valid_mixed_hole_fixture(*hole)) return 5;

    struct CaptureRun final {
        const char* name{};
        const SelectedRun* selected{};
        arpg::platform::Stage10ValidationScenario scenario{};
    };
    const std::array<CaptureRun, 4> captures{{
        {"01-abyss-door.png", &thunder.value(),
            arpg::platform::Stage10ValidationScenario::abyss_door},
        {"02-thunderstorm-warning.png", &thunder.value(),
            arpg::platform::Stage10ValidationScenario::thunderstorm_warning},
        {"03-hunting-flames-warning.png", &flames.value(),
            arpg::platform::Stage10ValidationScenario::hunting_flames_warning},
        {"04-chaos-expansion.png", &chaos.value(),
            arpg::platform::Stage10ValidationScenario::chaos_expansion},
    }};
    for (std::size_t index = 0U; index < captures.size(); ++index) {
        const auto directory = root_directory
            / ("capture-" + std::to_string(index + 1U));
        if (!prepare_save(directory, captures[index].selected->root, true)
                || !run_scenario(directory, *captures[index].selected,
                    captures[index].scenario,
                    root_directory / captures[index].name)) {
            return static_cast<int>(10U + index);
        }
    }
    const auto reward_directory = root_directory / "capture-reward-chain";
    if (!prepare_save(reward_directory, rewards->root, true, true)
            || !run_scenario(reward_directory, *rewards,
                arpg::platform::Stage10ValidationScenario::pending_reward,
                root_directory / "06-pending-reward.png",
                kPendingRewardMaximumFrames)
            || !valid_cleared_reward_save(
                reward_directory, rewards->target_seed)) {
        return 15;
    }
    if (!run_scenario(reward_directory, *rewards,
            arpg::platform::Stage10ValidationScenario::reward_chest,
            root_directory / "05-reward-chest.png")
            || !valid_cleared_reward_save(
                reward_directory, rewards->target_seed)) {
        return 14;
    }
    if (!run_scenario(reward_directory, *rewards,
            arpg::platform::Stage10ValidationScenario::exit_confirmation,
            root_directory / "07-exit-confirmation.png")) {
        return 16;
    }
    const auto death_directory = root_directory / "path-death";
    if (!prepare_save(death_directory, thunder->root, false, false, true)
            || !run_scenario(death_directory, *thunder,
                arpg::platform::Stage10ValidationScenario::player_death)
            || !continued_from_failed_same_room(
                death_directory, thunder->target_seed,
                arpg::abyss::AbyssRuleId::thunderstorm)) {
        return 20;
    }

    const auto reset_directory = root_directory / "path-reset";
    if (!prepare_save(reset_directory, rewards->root, true)
            || !run_scenario(reset_directory, *rewards,
                arpg::platform::Stage10ValidationScenario::room_reset)
            || !failed_same_room(reset_directory, rewards->target_seed)) {
        return 21;
    }

    const auto restart_directory = root_directory / "path-restart";
    if (!prepare_save(restart_directory, rewards->root, true)
            || !run_scenario(restart_directory, *rewards,
                arpg::platform::Stage10ValidationScenario::leave_started)) {
        return 22;
    }
    const auto started = load_state(restart_directory);
    if (!started.has_value()
            || started->abyss.lifecycle
                != arpg::abyss::AbyssLifecycle::started
            || !run_scenario(restart_directory, *rewards,
                arpg::platform::Stage10ValidationScenario::restarted_failed)
            || !resumed_started_same_room(
                restart_directory, rewards->target_seed, *started)) {
        return 23;
    }

    const auto hole_directory = root_directory / "path-hole";
    if (!prepare_save(hole_directory, hole->root, true, true)
            || !run_scenario(hole_directory, *hole,
                arpg::platform::Stage10ValidationScenario::abyss_hole_descent,
                std::nullopt, 2400U)) {
        return 24;
    }
    const auto descended = load_state(hole_directory);
    if (!descended.has_value()) return 25;
    {
        std::ofstream summary(root_directory / "formal-path-summary.txt",
            std::ios::trunc);
        summary << "hole_room_seed=" << descended->current_room.seed
                << " target_seed=" << hole->target_seed
                << " depth=" << descended->current_room.depth
                << " is_abyss=" << descended->current_room.is_abyss
                << " lifecycle=" << static_cast<unsigned>(
                    descended->abyss.lifecycle)
                << " generated=" << static_cast<unsigned>(
                    descended->abyss.generated_mask)
                << " claimed=" << static_cast<unsigned>(
                    descended->abyss.claimed_mask)
                << " abandoned=" << static_cast<unsigned>(
                    descended->abyss.abandoned_mask)
                << " last_transition=" << static_cast<unsigned>(
                    descended->last_transition)
                << " resolution_valid="
                << descended->last_abyss_resolution.valid
                << " resolution_total=" << static_cast<unsigned>(
                    descended->last_abyss_resolution.total)
                << " resolution_generated=" << static_cast<unsigned>(
                    descended->last_abyss_resolution.generated)
                << " resolution_claimed=" << static_cast<unsigned>(
                    descended->last_abyss_resolution.claimed)
                << " resolution_abandoned=" << static_cast<unsigned>(
                    descended->last_abyss_resolution.abandoned) << '\n';
    }
    if (descended->current_room.depth != 2U) return 26;
    if (descended->last_transition
            != arpg::dungeon::TransitionKind::descent) return 27;
    if (!descended->last_abyss_resolution.valid
            || descended->last_abyss_resolution.room_seed
                != hole->target_seed
            || descended->last_abyss_resolution.total == 0U
            || descended->last_abyss_resolution.claimed
                >= descended->last_abyss_resolution.total) {
        return 28;
    }

    std::cout << "formal_captures=7 death=PASS reset=PASS restart=PASS "
                 "abyss_hole_descent=PASS\n";
    return 0;
}
