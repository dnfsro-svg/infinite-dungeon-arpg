#include "raylib_host.hpp"

#include "combat/monster_affix_generation.hpp"
#include "combat/monster_catalog.hpp"
#include "core/deterministic_rng.hpp"
#include "dungeon/abyss_reward.hpp"
#include "dungeon/dungeon_progression.hpp"
#include "dungeon/dungeon_session.hpp"
#include "dungeon/encounter_director.hpp"
#include "items/item_generation.hpp"
#include "persistence/save_store.hpp"
#include "platform/settings/settings_store.hpp"

#include <array>
#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <optional>
#include <string>
#include <string_view>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace {

namespace combat = arpg::combat;
namespace core = arpg::core;
namespace dungeon = arpg::dungeon;
namespace items = arpg::items;
namespace persistence = arpg::persistence;
namespace platform = arpg::platform;
namespace settings = arpg::settings;

constexpr std::uint64_t kDropChanceDomain = 0x44524F505F43484EULL;
constexpr std::uint64_t kDropSlotDomain = 0x44524F505F534C54ULL;
constexpr std::uint64_t kDropContentDomain = 0x44524F505F49544DULL;
constexpr std::uint64_t kDropItemIdDomain = 0x44524F505F49445FULL;

struct ScenarioSpec final {
    const char* argument{};
    const char* image{};
    const char* summary{};
    platform::Stage11DLootValidationScenario scenario{
        platform::Stage11DLootValidationScenario::none};
    settings::LootFilterMode mode{settings::LootFilterMode::show_all};
    bool abyss{};
};

constexpr std::array<ScenarioSpec, 6> kScenarios{{
    {"show-all", "show-all.png", "show-all.txt",
        platform::Stage11DLootValidationScenario::show_all,
        settings::LootFilterMode::show_all, false},
    {"magic-plus", "magic-plus.png", "magic-plus.txt",
        platform::Stage11DLootValidationScenario::magic_or_better,
        settings::LootFilterMode::magic_or_better, false},
    {"rare-only", "rare-only.png", "rare-only.txt",
        platform::Stage11DLootValidationScenario::rare_only,
        settings::LootFilterMode::rare_only, false},
    {"rare-abyss", "rare-abyss.png", "rare-abyss.txt",
        platform::Stage11DLootValidationScenario::rare_only_abyss,
        settings::LootFilterMode::rare_only, true},
    {"preview-cancel", "preview-cancel.png", "preview-cancel.txt",
        platform::Stage11DLootValidationScenario::preview_cancel,
        settings::LootFilterMode::show_all, false},
    {"pickup-feedback", "pickup-feedback.png", "pickup-feedback.txt",
        platform::Stage11DLootValidationScenario::pickup_feedback,
        settings::LootFilterMode::show_all, false},
}};

std::filesystem::path g_executable{};

[[nodiscard]] core::DeterministicRng drop_stream(std::uint64_t seed,
    std::uint16_t ordinal, std::uint64_t domain) noexcept {
    auto ordinal_stream = core::DeterministicRng::derive_stream(seed, ordinal);
    return core::DeterministicRng::derive_stream(
        ordinal_stream.next_u64(), domain);
}

[[nodiscard]] std::optional<items::ItemInstance> expected_drop(
    const dungeon::checkpoint::DungeonRunState& state,
    const combat::MonsterSpawnSpec& spawn) noexcept {
    const std::uint16_t score = combat::monster_affix_danger_score(spawn.affixes);
    auto chance = drop_stream(state.current_room.seed,
        spawn.spawn_ordinal, kDropChanceDomain);
    if ((score == 0U && chance.next_bounded(100U).value_or(1U) != 0U)
            || (score != 0U && chance.next_bounded(10000U).value_or(10000U)
                >= dungeon::affix_drop_chance_bp(score))) return std::nullopt;
    auto slot = drop_stream(state.current_room.seed,
        spawn.spawn_ordinal, kDropSlotDomain);
    auto content = drop_stream(state.current_room.seed,
        spawn.spawn_ordinal, kDropContentDomain);
    auto room = core::DeterministicRng::derive_stream(
        state.root_seed, state.current_room.index);
    auto id_stream = drop_stream(room.next_u64(),
        spawn.spawn_ordinal, kDropItemIdDomain);
    std::uint64_t id = id_stream.next_u64();
    if (id == 0U) id = 1U;
    return items::generate_item({content.next_u64(),
        static_cast<items::ItemSlot>(slot.next_bounded(6U).value_or(0U)),
        dungeon::affix_item_level(state.current_room.depth, score), id,
        std::nullopt});
}

[[nodiscard]] std::optional<dungeon::checkpoint::DungeonRunState>
next_normal_state(const dungeon::checkpoint::DungeonRunState& state,
    const dungeon::DungeonRules& rules) noexcept {
    if (state.current_room.depth < 1U && state.current_room.has_hole
            && !state.current_room.is_abyss) {
        const auto next = dungeon::make_descent_transition(state, rules);
        if (next.fault == dungeon::DungeonFault::none) return next.state;
    }
    for (const auto direction : {dungeon::ExitDirection::up,
        dungeon::ExitDirection::down, dungeon::ExitDirection::left,
        dungeon::ExitDirection::right}) {
        const auto next = dungeon::make_door_transition(state, direction, rules);
        if (next.fault == dungeon::DungeonFault::none
                && !next.state.current_room.is_abyss) return next.state;
    }
    return std::nullopt;
}

struct SelectedStates final {
    dungeon::checkpoint::DungeonRunState ordinary{};
    dungeon::checkpoint::DungeonRunState abyss{};
    bool ordinary_ready{};
    bool abyss_ready{};
};

struct OrdinaryRoomScore final {
    std::uint8_t last_wave{0xFFU};
    std::uint16_t last_required_spawn{0xFFFFU};
    std::uint32_t prefix_affix_danger{0xFFFFFFFFU};
    std::uint32_t total_affix_danger{0xFFFFFFFFU};
    std::uint16_t spawn_count{0xFFFFU};
    std::uint64_t base_threat{0xFFFFFFFFFFFFFFFFULL};
    std::uint8_t total_budget{0xFFU};
};

[[nodiscard]] std::optional<OrdinaryRoomScore> ordinary_room_score(
    const dungeon::checkpoint::DungeonRunState& state,
    const dungeon::DungeonRules& rules) noexcept {
    if (state.current_room.is_abyss || state.current_room.depth != 1U) {
        return std::nullopt;
    }
    const auto built = dungeon::build_encounter_plan(state.current_room.seed,
        state.current_room.depth, state.current_room.ecology, rules.encounter);
    if (built.fault != dungeon::DungeonFault::none) return std::nullopt;
    bool normal = false;
    bool magic = false;
    bool rare = false;
    std::uint32_t danger = 0U;
    std::uint16_t spawn_count = 0U;
    std::uint64_t base_threat = 0U;
    std::optional<OrdinaryRoomScore> score{};
    for (std::size_t wave = 0U; wave < built.plan.wave_count; ++wave) {
        for (std::size_t index = 0U;
             index < built.plan.waves[wave].spawn_count; ++index) {
            const auto& spawn = built.plan.waves[wave].spawns[index];
            ++spawn_count;
            danger += combat::monster_affix_danger_score(spawn.affixes);
            const auto* definition = combat::monster_definition(spawn.id);
            if (definition == nullptr) return std::nullopt;
            std::uint64_t damage = 0U;
            for (const int amount : definition->contact_damage.amount) {
                damage += static_cast<std::uint64_t>((std::max)(0, amount));
            }
            base_threat += static_cast<std::uint64_t>(definition->max_hp)
                + damage * 20U
                + (combat::has_tag(*definition, combat::MonsterTag::ranged)
                    ? 500U : 0U)
                + (combat::has_tag(*definition,
                    combat::MonsterTag::ground_hazard) ? 1000U : 0U);
            const auto item = expected_drop(state, spawn);
            if (item.has_value()) {
                normal = normal || item->rarity == items::ItemRarity::normal;
                magic = magic || item->rarity == items::ItemRarity::magic;
                rare = rare || item->rarity == items::ItemRarity::rare;
            }
            if (!score.has_value() && normal && magic && rare) {
                score = OrdinaryRoomScore{
                    static_cast<std::uint8_t>(wave), spawn.spawn_ordinal,
                    danger, 0U, 0U, 0U, built.plan.total_budget};
            }
        }
    }
    if (score.has_value()) {
        score->total_affix_danger = danger;
        score->spawn_count = spawn_count;
        score->base_threat = base_threat;
    }
    return score;
}

[[nodiscard]] SelectedStates select_states() noexcept {
    const dungeon::DungeonRules rules{};
    SelectedStates selected{};
    auto ordinary_built = dungeon::make_initial_run_state(62U, rules);
    if (ordinary_built.fault == dungeon::DungeonFault::none) {
        auto state = ordinary_built.state;
        for (std::size_t step = 0U; step < 1100U; ++step) {
            if (state.current_room.index == 979U) {
                const auto score = ordinary_room_score(state, rules);
                if (score.has_value() && score->last_wave == 0U
                        && score->last_required_spawn == 2U
                        && score->prefix_affix_danger == 4U
                        && score->total_affix_danger == 4U
                        && score->spawn_count == 3U
                        && score->base_threat == 5320U
                        && score->total_budget == 8U) {
                    selected.ordinary = state;
                    selected.ordinary_ready = true;
                }
                break;
            }
            const auto next = next_normal_state(state, rules);
            if (!next.has_value()) break;
            state = *next;
        }
    }
    auto abyss_built = dungeon::make_initial_run_state(1U, rules);
    if (abyss_built.fault == dungeon::DungeonFault::none) {
        auto state = abyss_built.state;
        for (std::size_t step = 0U; step < 32U; ++step) {
            const auto doors = dungeon::preview_abyss_doors(state.current_room);
            for (std::size_t index = 0U; index < doors.size(); ++index) {
                if (!doors[index]) continue;
                const auto next = dungeon::make_door_transition(state,
                    static_cast<dungeon::ExitDirection>(index), rules);
                if (next.fault != dungeon::DungeonFault::none
                        || next.state.current_room.index != 6U
                        || !next.state.current_room.is_abyss) continue;
                const auto reward = dungeon::derive_abyss_reward_slot(
                    next.state.current_room.seed, next.state.abyss.danger,
                    static_cast<std::uint8_t>((std::min<std::uint64_t>)(
                        next.state.current_room.depth, 100U)), 0U);
                if (reward.has_value()
                        && reward->rarity != items::ItemRarity::rare) {
                    selected.abyss = next.state;
                    selected.abyss_ready = true;
                }
            }
            if (selected.abyss_ready) break;
            const auto next = next_normal_state(state, rules);
            if (!next.has_value()) break;
            state = *next;
        }
    }
    return selected;
}

[[nodiscard]] const ScenarioSpec* find_scenario(const std::string& name) noexcept {
    for (const auto& spec : kScenarios) if (name == spec.argument) return &spec;
    return nullptr;
}

[[nodiscard]] std::string quote(const std::filesystem::path& path) {
    return std::string{"\""} + path.string() + "\"";
}

[[nodiscard]] bool path_is_within(const std::filesystem::path& child,
    const std::filesystem::path& parent) {
    const auto relative = child.lexically_relative(parent);
    if (relative.empty() || relative.is_absolute()) return false;
    const auto first = relative.begin();
    return first != relative.end() && *first != "..";
}

[[nodiscard]] bool is_reparse_point(const std::filesystem::path& path) {
#ifdef _WIN32
    const DWORD attributes = GetFileAttributesW(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES
        && (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0U;
#else
    static_cast<void>(path);
    return false;
#endif
}

[[nodiscard]] bool has_link_or_reparse_component(
    const std::filesystem::path& path) {
    std::filesystem::path current = path.root_path();
    std::error_code error{};
    for (const auto& component : path.relative_path()) {
        current /= component;
        const auto status = std::filesystem::symlink_status(current, error);
        if (error) {
            error.clear();
            continue;
        }
        if (std::filesystem::is_symlink(status)
                || is_reparse_point(current)) return true;
    }
    return false;
}

[[nodiscard]] bool resolves_within(const std::filesystem::path& candidate,
    const std::filesystem::path& root) {
    std::error_code error{};
    const auto resolved_root = std::filesystem::weakly_canonical(root, error);
    if (error) return false;
    const auto resolved_candidate =
        std::filesystem::weakly_canonical(candidate, error);
    return !error && (resolved_candidate == resolved_root
        || path_is_within(resolved_candidate, resolved_root));
}

[[nodiscard]] bool allowed_evidence_root(
    const std::filesystem::path& candidate) {
    std::error_code error{};
    const auto absolute = std::filesystem::absolute(candidate, error)
        .lexically_normal();
    if (error || absolute.empty() || absolute == absolute.root_path()
            || absolute.filename() != "stage11d loot evidence"
            || has_link_or_reparse_component(absolute)) {
        return false;
    }
    auto generic = absolute.generic_string();
    std::transform(generic.begin(), generic.end(), generic.begin(),
        [](unsigned char value) {
            return static_cast<char>(std::tolower(value));
        });
    const bool build_child = generic.find("/out/build/") != std::string::npos;
    const auto temporary = std::filesystem::temp_directory_path(error)
        .lexically_normal();
    return build_child || (!error && path_is_within(absolute, temporary));
}

[[nodiscard]] bool remove_known_file(const std::filesystem::path& root,
    const std::filesystem::path& path) {
    if (has_link_or_reparse_component(path.parent_path())
            || !resolves_within(path.parent_path(), root)) {
        return false;
    }
    std::error_code error{};
    const auto status = std::filesystem::symlink_status(path, error);
    if (error == std::errc::no_such_file_or_directory) return true;
    if (error) return false;
    if (!std::filesystem::exists(status)) return true;
    if (std::filesystem::is_directory(status)) return false;
    return std::filesystem::remove(path, error) && !error;
}

[[nodiscard]] bool reset_evidence_root(
    const std::filesystem::path& candidate) {
    if (!allowed_evidence_root(candidate)) return false;
    const auto root = std::filesystem::absolute(candidate).lexically_normal();
    std::error_code error{};
    std::filesystem::create_directories(root, error);
    if (error || has_link_or_reparse_component(root)) return false;
    if (!remove_known_file(
            root, root / "stage11d-loot-evidence.txt")) return false;
    constexpr std::array<std::string_view, 4> kSaveFiles{{
        "run_a.sav", "run_b.sav", "run_a.tmp", "run_b.tmp"}};
    constexpr std::array<std::string_view, 4> kSettingsFiles{{
        "settings-a.bin", "settings-b.bin",
        "settings-a.bin.tmp", "settings-b.bin.tmp"}};
    for (const auto& spec : kScenarios) {
        if (!remove_known_file(root, root / spec.image)
                || !remove_known_file(root, root / spec.summary)
                || !remove_known_file(root,
                    root / (std::string{"stage11d-child-"}
                        + spec.argument + ".cmd"))) {
            return false;
        }
        for (const auto name : kSaveFiles) {
            if (!remove_known_file(
                    root, root / spec.argument / "saves" / name)) {
                return false;
            }
        }
        for (const auto name : kSettingsFiles) {
            if (!remove_known_file(
                    root, root / spec.argument / "settings" / name)) {
                return false;
            }
        }
    }
    return true;
}

[[nodiscard]] bool prepare_scenario(const std::filesystem::path& root,
    const ScenarioSpec& spec, const SelectedStates& selected) {
    const auto directory = root / spec.argument;
    if (has_link_or_reparse_component(directory)
            || !resolves_within(directory, root)
            || !resolves_within(directory / "saves", root)
            || !resolves_within(directory / "settings", root)) {
        return false;
    }
    std::error_code error{};
    std::filesystem::create_directories(directory / "saves", error);
    std::filesystem::create_directories(directory / "settings", error);
    if (error) return false;
    persistence::SaveStore save({directory / "saves"});
    const auto saved = save.commit(spec.abyss ? selected.abyss : selected.ordinary);
    settings::SettingsStore settings_store(directory / "settings");
    const auto loaded = settings_store.load();
    auto draft = loaded.settings;
    draft.loot_filter_mode = spec.mode;
    const auto settings_saved = settings_store.save(loaded.settings, draft);
    return saved.state == persistence::SaveCommitState::committed
        && settings_saved.status == settings::SettingsSaveStatus::committed;
}

[[nodiscard]] bool fresh_file(const std::filesystem::path& path,
    std::filesystem::file_time_type started, std::uintmax_t minimum) {
    std::error_code error{};
    const auto size = std::filesystem::file_size(path, error);
    if (error || size <= minimum) return false;
    return std::filesystem::last_write_time(path, error) >= started && !error;
}

[[nodiscard]] bool run_host(const std::filesystem::path& root,
    const ScenarioSpec& spec) {
    const auto absolute_root = std::filesystem::absolute(root);
    const auto started = std::filesystem::file_time_type::clock::now()
        - std::chrono::seconds(2);
    const auto directory = absolute_root / spec.argument;
    platform::RaylibHostConfig config{};
    config.window_width = 1280;
    config.window_height = 720;
    config.window_title = "Stage11D Production Loot Formal Validation";
    config.save_directory = directory / "saves";
    config.settings_directory = directory / "settings";
    config.stage11d_loot_validation = spec.scenario;
    config.validation_steps_per_frame = 1U;
    config.validation_exit_after_presented_frames = 4000U;
    config.validation_capture_file = absolute_root / spec.image;
    config.validation_summary_file = absolute_root / spec.summary;
    return platform::run_raylib_host(config) == platform::HostExitCode::success
        && fresh_file(*config.validation_capture_file, started, 1024U)
        && fresh_file(*config.validation_summary_file, started, 64U);
}

[[nodiscard]] bool run_child(const std::filesystem::path& root,
    const ScenarioSpec& spec) {
    const auto command_file = root / (std::string{"stage11d-child-"}
        + spec.argument + ".cmd");
    std::ofstream command(command_file, std::ios::trunc);
    command << "@echo off\r\n" << quote(g_executable) << " --scenario "
        << spec.argument << ' ' << quote(root) << "\r\n";
    command.close();
    const std::string invocation = "call " + quote(command_file);
    return std::system(invocation.c_str()) == 0;
}

[[nodiscard]] std::uint64_t hash_file(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    std::uint64_t hash = 1469598103934665603ULL;
    char value{};
    while (stream.get(value)) {
        hash ^= static_cast<unsigned char>(value);
        hash *= 1099511628211ULL;
    }
    return hash;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc == 4 && std::string{argv[1]} == "--root-safety-self-test") {
        const auto root = std::filesystem::absolute(argv[2]).lexically_normal();
        if (!reset_evidence_root(root)) return 5;
        const auto sentinel =
            root / "stage11d-root-safety-sentinel.unknown";
        const auto known_evidence = root / kScenarios.front().image;
        std::error_code error{};
        const auto sentinel_status =
            std::filesystem::symlink_status(sentinel, error);
        if (error != std::errc::no_such_file_or_directory
                && (error || std::filesystem::is_directory(sentinel_status)
                    || std::filesystem::is_symlink(sentinel_status)
                    || is_reparse_point(sentinel))) {
            return 5;
        }
        std::ofstream sentinel_stream(sentinel, std::ios::trunc);
        std::ofstream known_stream(known_evidence, std::ios::trunc);
        sentinel_stream << "preserve";
        known_stream << "remove";
        sentinel_stream.close();
        known_stream.close();
        if (!sentinel_stream || !known_stream) return 5;
        if (!reset_evidence_root(root)) return 5;
        error.clear();
        const bool sentinel_preserved =
            std::filesystem::is_regular_file(sentinel, error) && !error
            && std::filesystem::file_size(sentinel, error) == 8U && !error;
        error.clear();
        const bool known_removed =
            !std::filesystem::exists(known_evidence, error) && !error;
        return sentinel_preserved && known_removed
                && !allowed_evidence_root(argv[3]) ? 0 : 5;
    }
    if (argc == 4 && std::string{argv[1]} == "--prepare") {
        const auto* spec = find_scenario(argv[2]);
        const SelectedStates selected = select_states();
        if (spec == nullptr || !selected.ordinary_ready || !selected.abyss_ready) {
            return 4;
        }
        const auto root = std::filesystem::absolute(argv[3]);
        return reset_evidence_root(root)
                && prepare_scenario(root, *spec, selected) ? 0 : 3;
    }
    if (argc == 2 && std::string{argv[1]} == "--select-only") {
        const SelectedStates selected = select_states();
        if (!selected.ordinary_ready || !selected.abyss_ready) return 4;
        const auto score = ordinary_room_score(
            selected.ordinary, dungeon::DungeonRules{});
        std::cout << "ordinary root=" << selected.ordinary.root_seed
            << " room=" << selected.ordinary.current_room.index
            << " depth=" << selected.ordinary.current_room.depth
            << " score=" << static_cast<unsigned>(score->last_wave) << ','
            << score->last_required_spawn << ','
            << score->prefix_affix_danger << ','
            << score->total_affix_danger << ','
            << score->spawn_count << ','
            << score->base_threat << ','
            << static_cast<unsigned>(score->total_budget) << '\n';
        const auto plan = dungeon::build_encounter_plan(
            selected.ordinary.current_room.seed,
            selected.ordinary.current_room.depth,
            selected.ordinary.current_room.ecology,
            dungeon::DungeonRules{}.encounter);
        for (std::size_t wave = 0U; wave < plan.plan.wave_count; ++wave) {
            for (std::size_t index = 0U;
                 index < plan.plan.waves[wave].spawn_count; ++index) {
                const auto& spawn = plan.plan.waves[wave].spawns[index];
                const auto item = expected_drop(selected.ordinary, spawn);
                const auto* definition = combat::monster_definition(spawn.id);
                std::uint64_t damage = 0U;
                for (const int amount : definition->contact_damage.amount) {
                    damage += static_cast<std::uint64_t>(
                        (std::max)(0, amount));
                }
                std::cout << "spawn ordinal=" << spawn.spawn_ordinal
                    << " id=" << static_cast<unsigned>(spawn.id)
                    << " pos=" << spawn.position.x << ',' << spawn.position.y
                    << " hp=" << definition->max_hp
                    << " damage=" << damage
                    << " rarity=" << static_cast<unsigned>(item->rarity)
                    << '\n';
            }
        }
        return 0;
    }
    if (argc == 4 && std::string{argv[1]} == "--scenario") {
        const auto* spec = find_scenario(argv[2]);
        return spec != nullptr && run_host(argv[3], *spec) ? 0 : 1;
    }
    if (argc != 3) {
        std::cerr << "usage: arpg_stage11d_loot_formal <evidence> <committed>\n";
        return 2;
    }
    g_executable = std::filesystem::absolute(argv[0]);
    const auto root = std::filesystem::absolute(argv[1]);
    const auto committed = std::filesystem::absolute(argv[2]);
    std::error_code error{};
    if (!reset_evidence_root(root)) return 3;
    std::filesystem::create_directories(committed, error);
    if (error) return 3;
    const SelectedStates selected = select_states();
    if (!selected.ordinary_ready || !selected.abyss_ready) return 4;
    std::cout << "STAGE11D SELECT ordinary root=" << selected.ordinary.root_seed
        << " room=" << selected.ordinary.current_room.index
        << " depth=" << selected.ordinary.current_room.depth
        << " abyss root=" << selected.abyss.root_seed
        << " room=" << selected.abyss.current_room.index << std::endl;
    bool ok = true;
    for (const auto& spec : kScenarios) {
        const bool prepared = prepare_scenario(root, spec, selected);
        const bool ran = prepared && run_child(root, spec);
        std::cout << "STAGE11D LOOT " << (ran ? "PASS " : "FAIL ")
            << spec.argument << std::endl;
        bool copied = ran;
        if (ran) {
            error.clear();
            std::filesystem::copy_file(root / spec.image,
                committed / spec.image,
                std::filesystem::copy_options::overwrite_existing, error);
            copied = copied && !error;
            error.clear();
            std::filesystem::copy_file(root / spec.summary,
                committed / spec.summary,
                std::filesystem::copy_options::overwrite_existing, error);
            copied = copied && !error;
        }
        ok = ok && ran && copied;
    }
    std::ofstream manifest(root / "stage11d-loot-evidence.txt", std::ios::trunc);
    manifest << "ordinary_root=" << selected.ordinary.root_seed << '\n'
        << "ordinary_room=" << selected.ordinary.current_room.index << '\n'
        << "ordinary_depth=" << selected.ordinary.current_room.depth << '\n'
        << "abyss_root=" << selected.abyss.root_seed << '\n'
        << "abyss_room=" << selected.abyss.current_room.index << '\n';
    for (const auto& spec : kScenarios) {
        manifest << spec.argument << "_hash=" << hash_file(root / spec.image) << '\n';
    }
    manifest << "result=" << (ok ? "pass" : "fail") << '\n';
    manifest.flush();
    if (!manifest) return 1;
    manifest.close();
    error.clear();
    std::filesystem::copy_file(root / "stage11d-loot-evidence.txt",
        committed / "stage11d-loot-evidence.txt",
        std::filesystem::copy_options::overwrite_existing, error);
    return ok && !error ? 0 : 1;
}
