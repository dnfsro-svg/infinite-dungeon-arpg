#include "abyss/abyss_rules.hpp"
#include "combat/monster_catalog.hpp"
#include "dungeon/dungeon_progression.hpp"
#include "dungeon/encounter_director.hpp"
#include "dungeon/room_generation.hpp"
#include "items/item_generation.hpp"
#include "persistence/save_store.hpp"
#include "raylib_host.hpp"

#include <raylib.h>

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>

namespace {

struct SelectedRun final {
    std::uint64_t root{};
    std::uint64_t target_seed{};
    arpg::dungeon::ExitDirection direction{arpg::dungeon::ExitDirection::none};
};

std::optional<SelectedRun> find_run(
    arpg::abyss::AbyssRuleId required_rule,
    bool require_hole = false,
    bool require_all_melee = false) noexcept {
    constexpr std::array<arpg::dungeon::ExitDirection, 4> directions{{
        arpg::dungeon::ExitDirection::up,
        arpg::dungeon::ExitDirection::down,
        arpg::dungeon::ExitDirection::left,
        arpg::dungeon::ExitDirection::right,
    }};
    const arpg::dungeon::DungeonRules rules{};
    for (std::uint64_t root = 1U; root < 500000U; ++root) {
        const auto initial = arpg::dungeon::make_initial_run_state(root, rules);
        if (initial.fault != arpg::dungeon::DungeonFault::none) continue;
        const auto preview = arpg::dungeon::preview_abyss_doors(
            initial.state.current_room);
        for (std::size_t index = 0U; index < directions.size(); ++index) {
            if (!preview[index]) continue;
            const auto next = arpg::dungeon::make_door_transition(
                initial.state, directions[index], rules);
            if (next.fault == arpg::dungeon::DungeonFault::none
                    && next.state.current_room.is_abyss
                    && next.state.abyss.rule == required_rule
                    && (!require_hole || next.state.current_room.has_hole)) {
                if (require_all_melee) {
                    const arpg::dungeon::EncounterBuildRequest request{
                        next.state.current_room.seed,
                        next.state.current_room.depth,
                        next.state.current_room.ecology,
                        next.state.current_room.entry,
                        next.state.current_room.has_hole,
                        18U,
                    };
                    const auto plan = arpg::dungeon::build_abyss_encounter_plan(
                        request, rules.encounter);
                    bool all_melee = plan.fault
                        == arpg::dungeon::DungeonFault::none;
                    for (std::size_t wave = 0U;
                         all_melee && wave < plan.plan.wave_count; ++wave) {
                        for (std::size_t spawn = 0U;
                             spawn < plan.plan.waves[wave].spawn_count; ++spawn) {
                            const auto* definition =
                                arpg::combat::monster_definition(
                                    plan.plan.waves[wave].spawns[spawn].id);
                            all_melee = definition != nullptr
                                && (definition->tags & static_cast<std::uint16_t>(
                                    arpg::combat::MonsterTag::melee)) != 0U;
                            if (!all_melee) break;
                        }
                    }
                    if (!all_melee) continue;
                }
                return SelectedRun{root, next.state.current_room.seed,
                    directions[index]};
            }
        }
    }
    return std::nullopt;
}

bool install_validation_build(arpg::dungeon::DungeonRunState& state) {
    state.progression = {100U, 0U, 99U, 99U};
    state.item_ownership = {};
    state.item_ownership.items.reserve(6U);
    for (std::uint8_t index = 0U; index < 6U; ++index) {
        const std::uint64_t id = static_cast<std::uint64_t>(index) + 1U;
        const auto item = arpg::items::generate_item({
            0xA8100000ULL + index,
            static_cast<arpg::items::ItemSlot>(index),
            100U,
            id,
            arpg::items::ItemRarity::rare,
        });
        if (!item.has_value()) return false;
        state.item_ownership.items.push_back(*item);
        state.item_ownership.equipment.equipped_ids[index] = id;
    }
    state.item_ownership.next_item_sequence = 7U;
    return true;
}

bool prepare_save(const std::filesystem::path& directory,
    std::uint64_t root, bool geared) {
    std::error_code error;
    std::filesystem::remove_all(directory, error);
    if (error) return false;
    std::filesystem::create_directories(directory, error);
    if (error) return false;
    auto initial = arpg::dungeon::make_initial_run_state(
        root, arpg::dungeon::DungeonRules{});
    if (initial.fault != arpg::dungeon::DungeonFault::none
            || (geared && !install_validation_build(initial.state))) {
        return false;
    }
    arpg::persistence::SaveStore store({directory});
    return store.commit(initial.state).state
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

    const auto thunder = find_run(arpg::abyss::AbyssRuleId::thunderstorm);
    const auto flames = find_run(arpg::abyss::AbyssRuleId::hunting_flames);
    const auto chaos = find_run(arpg::abyss::AbyssRuleId::chaos_expansion);
    const auto rewards = find_run(arpg::abyss::AbyssRuleId::life_sacrifice);
    const auto hole = find_run(
        arpg::abyss::AbyssRuleId::hunting_flames, true, true);
    if (!thunder || !flames || !chaos || !rewards || !hole) return 5;

    struct CaptureRun final {
        const char* name{};
        const SelectedRun* selected{};
        arpg::platform::Stage10ValidationScenario scenario{};
    };
    const std::array<CaptureRun, 7> captures{{
        {"01-abyss-door.png", &thunder.value(),
            arpg::platform::Stage10ValidationScenario::abyss_door},
        {"02-thunderstorm-warning.png", &thunder.value(),
            arpg::platform::Stage10ValidationScenario::thunderstorm_warning},
        {"03-hunting-flames-warning.png", &flames.value(),
            arpg::platform::Stage10ValidationScenario::hunting_flames_warning},
        {"04-chaos-expansion.png", &chaos.value(),
            arpg::platform::Stage10ValidationScenario::chaos_expansion},
        {"05-reward-chest.png", &rewards.value(),
            arpg::platform::Stage10ValidationScenario::reward_chest},
        {"06-pending-reward.png", &rewards.value(),
            arpg::platform::Stage10ValidationScenario::pending_reward},
        {"07-exit-confirmation.png", &rewards.value(),
            arpg::platform::Stage10ValidationScenario::exit_confirmation},
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

    const auto death_directory = root_directory / "path-death";
    if (!prepare_save(death_directory, thunder->root, false)
            || !run_scenario(death_directory, *thunder,
                arpg::platform::Stage10ValidationScenario::player_death)
            || !failed_same_room(death_directory, thunder->target_seed)) {
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
            || !failed_same_room(restart_directory, rewards->target_seed)) {
        return 23;
    }

    const auto hole_directory = root_directory / "path-hole";
    if (!prepare_save(hole_directory, hole->root, true)
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
