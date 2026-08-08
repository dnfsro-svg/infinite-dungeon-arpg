#include "raylib_host.hpp"

#include "combat/monster_affix_generation.hpp"
#include "combat/monster_catalog.hpp"
#include "combat/room_spatial_grid.hpp"
#include "core/deterministic_rng.hpp"
#include "core/gameplay_limits.hpp"
#include "dungeon/abyss_reward.hpp"
#include "dungeon/dungeon_progression.hpp"
#include "dungeon/dungeon_session.hpp"
#include "dungeon/room_combat_template.hpp"
#include "dungeon/room_environment.hpp"
#include "dungeon/room_monster_plan_builder.hpp"
#include "items/item_generation.hpp"
#include "persistence/save_store.hpp"
#include "platform/settings/settings_store.hpp"
#include "stage10_validation_build.hpp"

#include <array>
#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <optional>
#include <string>
#include <string_view>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define CloseWindow CloseWindowWin32
#define ShowCursor ShowCursorWin32
#include <windows.h>
#undef ShowCursor
#undef CloseWindow
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
constexpr std::uint32_t kStage11DDefaultPresentedFrameLimit = 4000U;
constexpr std::uint64_t kStage11DRareAbyssBaseFrameBudget = 12000U;
constexpr std::uint64_t kStage11DRareAbyssFramesPerMonster = 96U;
constexpr bool kStage11DFormalVsyncEnabled = false;

[[nodiscard]] constexpr std::optional<std::uint32_t>
stage11d_rare_abyss_presented_frame_budget(
    std::uint32_t monster_count) noexcept {
    if (monster_count == 0U
            || static_cast<std::uint64_t>(monster_count)
                > static_cast<std::uint64_t>(
                    arpg::limits::kRoomMonsterCapacity)) {
        return std::nullopt;
    }
    const std::uint64_t budget = kStage11DRareAbyssBaseFrameBudget
        + static_cast<std::uint64_t>(monster_count)
            * kStage11DRareAbyssFramesPerMonster;
    if (budget > (std::numeric_limits<std::uint32_t>::max)()) {
        return std::nullopt;
    }
    return static_cast<std::uint32_t>(budget);
}

static_assert(!stage11d_rare_abyss_presented_frame_budget(0U).has_value());
static_assert(stage11d_rare_abyss_presented_frame_budget(1U).value_or(0U)
    == 12096U);
static_assert(stage11d_rare_abyss_presented_frame_budget(
    static_cast<std::uint32_t>(arpg::limits::kRoomMonsterCapacity))
        .value_or(0U) == 122592U);
static_assert(!stage11d_rare_abyss_presented_frame_budget(
    static_cast<std::uint32_t>(arpg::limits::kRoomMonsterCapacity + 1U))
        .has_value());

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
    const combat::RoomMonsterBlueprint& monster) noexcept {
    const std::uint16_t score =
        combat::monster_affix_danger_score(monster.affixes);
    auto chance = drop_stream(state.current_room.seed,
        monster.spawn_ordinal, kDropChanceDomain);
    if ((score == 0U && chance.next_bounded(100U).value_or(1U) != 0U)
            || (score != 0U && chance.next_bounded(10000U).value_or(10000U)
                >= dungeon::affix_drop_chance_bp(score))) return std::nullopt;
    auto slot = drop_stream(state.current_room.seed,
        monster.spawn_ordinal, kDropSlotDomain);
    auto content = drop_stream(state.current_room.seed,
        monster.spawn_ordinal, kDropContentDomain);
    auto room = core::DeterministicRng::derive_stream(
        state.root_seed, state.current_room.index);
    auto id_stream = drop_stream(room.next_u64(),
        monster.spawn_ordinal, kDropItemIdDomain);
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

[[nodiscard]] std::optional<dungeon::checkpoint::DungeonRunState>
next_non_fire_state(const dungeon::checkpoint::DungeonRunState& state,
    const dungeon::DungeonRules& rules) noexcept {
    for (const auto direction : {dungeon::ExitDirection::left,
        dungeon::ExitDirection::down, dungeon::ExitDirection::right}) {
        const auto next = dungeon::make_door_transition(state, direction, rules);
        if (next.fault == dungeon::DungeonFault::none
                && !next.state.current_room.is_abyss) return next.state;
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<dungeon::checkpoint::DungeonRunState>
next_descending_state(const dungeon::checkpoint::DungeonRunState& state,
    const dungeon::DungeonRules& rules) noexcept {
    if (state.current_room.has_hole && !state.current_room.is_abyss) {
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
    std::uint64_t ordinary_blueprint_hash{};
    std::uint16_t ordinary_monster_count{};
    std::uint16_t ordinary_initial_resident_count{};
    std::uint16_t ordinary_required_kills{};
    std::array<std::uint16_t, 3> ordinary_drop_ordinals{{
        combat::kInvalidMonsterOrdinal,
        combat::kInvalidMonsterOrdinal,
        combat::kInvalidMonsterOrdinal}};
    std::array<std::uint64_t, 3> ordinary_drop_item_ids{};
    std::array<items::ItemRarity, 3> ordinary_drop_rarities{{
        items::ItemRarity::normal,
        items::ItemRarity::normal,
        items::ItemRarity::normal}};
    std::uint64_t abyss_blueprint_hash{};
    std::uint16_t abyss_monster_count{};
    std::uint16_t abyss_initial_owned_item_count{};
    std::uint32_t abyss_presented_frame_budget{};
    bool ordinary_ready{};
    bool abyss_ready{};
};

struct OrdinaryRoomProfile final {
    std::uint64_t blueprint_hash{};
    std::uint16_t monster_count{};
    std::uint16_t initial_resident_count{};
    std::uint16_t required_kills{};
    std::uint32_t prefix_hp{};
    std::uint32_t prefix_affix_danger{};
    std::uint16_t prefix_ranged{};
    std::uint16_t prefix_ground_hazards{};
    std::array<std::uint16_t, 3> drop_ordinals{{
        combat::kInvalidMonsterOrdinal,
        combat::kInvalidMonsterOrdinal,
        combat::kInvalidMonsterOrdinal}};
    std::array<std::uint64_t, 3> drop_item_ids{};
    std::array<items::ItemRarity, 3> drop_rarities{{
        items::ItemRarity::normal,
        items::ItemRarity::normal,
        items::ItemRarity::normal}};
};

struct AbyssRoomProfile final {
    std::uint64_t blueprint_hash{};
    std::uint16_t monster_count{};
    std::uint32_t presented_frame_budget{};
};

[[nodiscard]] std::optional<AbyssRoomProfile> abyss_room_profile(
    const dungeon::checkpoint::DungeonRunState& state,
    const dungeon::DungeonRules& rules) noexcept {
    if (!state.current_room.is_abyss) return std::nullopt;
    combat::RoomMonsterPlan plan{};
    const auto built = dungeon::build_room_monster_plan(state.current_room,
        rules, dungeon::kRoomMonsterGeneratorVersion, plan);
    const auto budget = stage11d_rare_abyss_presented_frame_budget(
        plan.monster_count);
    if (built.fault != dungeon::DungeonFault::none
            || built.density.total_count != plan.monster_count
            || !dungeon::room_monster_plan_legal(state.current_room, plan)
            || !budget.has_value()) {
        return std::nullopt;
    }
    return AbyssRoomProfile{
        plan.blueprint_hash, plan.monster_count, *budget};
}

[[nodiscard]] std::uint8_t rarity_bit(items::ItemRarity rarity) noexcept {
    switch (rarity) {
    case items::ItemRarity::normal: return 1U;
    case items::ItemRarity::magic: return 2U;
    case items::ItemRarity::rare: return 4U;
    }
    return 0U;
}

[[nodiscard]] std::optional<OrdinaryRoomProfile> ordinary_room_profile(
    const dungeon::checkpoint::DungeonRunState& state,
    const dungeon::DungeonRules& rules) noexcept {
    constexpr std::uint16_t kMaximumPrefixKills = 10U;
    if (state.current_room.is_abyss || state.current_room.depth != 4U
            || state.current_room.ecology == dungeon::DungeonElement::fire
            || state.current_room.has_hole) {
        return std::nullopt;
    }
    const dungeon::RoomDensityRoll density = dungeon::roll_room_density(
        state.current_room.seed, false);
    if (density.affix != dungeon::RoomDensityAffix::crowded
            || density.total_count > 350U) return std::nullopt;

    combat::RoomMonsterPlan plan{};
    const auto built = dungeon::build_room_monster_plan(state.current_room,
        rules, dungeon::kRoomMonsterGeneratorVersion, plan);
    if (built.fault != dungeon::DungeonFault::none
            || built.density.total_count != plan.monster_count
            || !dungeon::room_monster_plan_legal(state.current_room, plan)) {
        return std::nullopt;
    }
    const auto combat_config = dungeon::make_combat_lab_config(
        state.current_room.entry, rules.rules_version);
    if (!combat_config.has_value()) return std::nullopt;
    const combat::RoomStreamingRegion region =
        combat::make_room_streaming_region(combat_config->player_spawn);
    std::array<std::uint16_t, combat::kMonsterCapacity> resident_ordinals{};
    std::size_t resident_count = 0U;
    for (std::size_t row = region.first_row;
         row < static_cast<std::size_t>(region.first_row) + region.row_count;
         ++row) {
        for (std::size_t column = region.first_column;
             column < static_cast<std::size_t>(region.first_column)
                    + region.column_count; ++column) {
            const std::size_t cell = row * combat::room_spatial::columns
                + column;
            for (std::uint16_t ordinal = plan.cell_offsets[cell];
                 ordinal < plan.cell_offsets[cell + 1U]; ++ordinal) {
                if (resident_count >= resident_ordinals.size()) {
                    return std::nullopt;
                }
                resident_ordinals[resident_count++] = ordinal;
            }
        }
    }
    std::sort(resident_ordinals.begin(),
        resident_ordinals.begin() + resident_count);
    if (resident_count < kMaximumPrefixKills) return std::nullopt;

    OrdinaryRoomProfile profile{};
    profile.blueprint_hash = plan.blueprint_hash;
    profile.monster_count = plan.monster_count;
    profile.initial_resident_count = static_cast<std::uint16_t>(resident_count);
    std::size_t drop_count = 0U;
    std::uint8_t rarity_bits = 0U;
    for (std::size_t index = 0U; index < kMaximumPrefixKills; ++index) {
        const auto& monster = plan.monsters[resident_ordinals[index]];
        const auto* definition = combat::monster_definition(monster.id);
        if (definition == nullptr
                || combat::has_tag(
                    *definition, combat::MonsterTag::ground_hazard)) {
            return std::nullopt;
        }
        profile.prefix_hp += static_cast<std::uint32_t>(definition->max_hp);
        profile.prefix_affix_danger +=
            combat::monster_affix_danger_score(monster.affixes);
        profile.prefix_ranged += combat::has_tag(
            *definition, combat::MonsterTag::ranged) ? 1U : 0U;
        profile.prefix_ground_hazards += combat::has_tag(
            *definition, combat::MonsterTag::ground_hazard) ? 1U : 0U;
        const auto item = expected_drop(state, monster);
        if (!item.has_value()) continue;
        if (drop_count >= profile.drop_ordinals.size()) return std::nullopt;
        profile.drop_ordinals[drop_count] = monster.spawn_ordinal;
        profile.drop_item_ids[drop_count] = item->id;
        profile.drop_rarities[drop_count] = item->rarity;
        rarity_bits = static_cast<std::uint8_t>(
            rarity_bits | rarity_bit(item->rarity));
        ++drop_count;
        if (drop_count == profile.drop_ordinals.size()) {
            if (rarity_bits != 7U) return std::nullopt;
            profile.required_kills = static_cast<std::uint16_t>(index + 1U);
            for (std::size_t tail = index + 1U;
                 tail < resident_count; ++tail) {
                if (expected_drop(state,
                        plan.monsters[resident_ordinals[tail]]).has_value()) {
                    return std::nullopt;
                }
            }
            return profile;
        }
    }
    return std::nullopt;
}

[[nodiscard]] bool prepare_stage11d_live_damage_build(
    dungeon::checkpoint::DungeonRunState& state) noexcept {
    constexpr std::uint16_t kBarrierAffix = 12U;
    std::size_t non_weapon_count = 0U;
    std::size_t removed_count = 0U;
    for (auto& item : state.item_ownership.items) {
        const items::BaseDefinition* base = items::base_definition(item.base_id);
        if (base == nullptr) return false;
        if (base->slot == items::ItemSlot::weapon) continue;
        ++non_weapon_count;
        std::array<items::AffixRoll, 6> retained{};
        std::uint8_t retained_count = 0U;
        for (std::uint8_t index = 0U; index < item.affix_count; ++index) {
            const auto affix = item.affixes[index];
            if (affix.affix_id == kBarrierAffix) {
                ++removed_count;
                continue;
            }
            if (retained_count >= retained.size()) return false;
            retained[retained_count++] = affix;
        }
        item.affixes = retained;
        item.affix_count = retained_count;
        if (retained_count != 5U || !items::validate_item(item)) return false;
    }
    return state.item_ownership.items.size() == 6U
        && non_weapon_count == 5U && removed_count == 5U
        && items::validate_ownership(state.item_ownership);
}

[[nodiscard]] SelectedStates select_states() noexcept {
    const dungeon::DungeonRules rules{};
    SelectedStates selected{};
    constexpr std::uint64_t kOrdinaryRootSeed = 12U;
    constexpr std::size_t kOrdinaryFixtureTransitions = 4571U;
    auto ordinary_built = dungeon::make_initial_run_state(
        kOrdinaryRootSeed, rules);
    if (ordinary_built.fault == dungeon::DungeonFault::none) {
        auto state = ordinary_built.state;
        bool reached = true;
        for (std::size_t step = 0U;
             step < kOrdinaryFixtureTransitions; ++step) {
            const auto next = state.current_room.depth < 4U
                ? next_descending_state(state, rules)
                : next_non_fire_state(state, rules);
            if (!next.has_value()) {
                reached = false;
                break;
            }
            state = *next;
        }
        const auto profile = reached
            ? ordinary_room_profile(state, rules) : std::nullopt;
        constexpr std::array<std::uint16_t, 3> kDropOrdinals{{
            124U, 125U, 137U}};
        constexpr std::array<std::uint64_t, 3> kDropItemIds{{
            16727938678320169397ULL,
            17561380846184487225ULL,
            18282590332386757687ULL}};
        if (profile.has_value()
                && state.current_room.index == 4571U
                && state.current_room.seed == 16445546368581026750ULL
                && state.current_room.depth == 4U
                && state.current_room.ecology
                    == dungeon::DungeonElement::lightning
                && !state.current_room.has_hole
                && profile->blueprint_hash == 2903734950153057739ULL
                && profile->monster_count == 300U
                && profile->initial_resident_count == 15U
                && profile->required_kills == 4U
                && profile->prefix_hp == 445U
                && profile->prefix_affix_danger == 13U
                && profile->prefix_ranged == 2U
                && profile->prefix_ground_hazards == 0U
                && profile->drop_ordinals == kDropOrdinals
                && profile->drop_item_ids == kDropItemIds
                && profile->drop_rarities[0] == items::ItemRarity::magic
                && profile->drop_rarities[1] == items::ItemRarity::rare
                && profile->drop_rarities[2] == items::ItemRarity::normal) {
            selected.ordinary = state;
            selected.ordinary_blueprint_hash = profile->blueprint_hash;
            selected.ordinary_monster_count = profile->monster_count;
            selected.ordinary_initial_resident_count =
                profile->initial_resident_count;
            selected.ordinary_required_kills = profile->required_kills;
            selected.ordinary_drop_ordinals = profile->drop_ordinals;
            selected.ordinary_drop_item_ids = profile->drop_item_ids;
            selected.ordinary_drop_rarities = profile->drop_rarities;
            selected.ordinary_ready = true;
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
                const auto profile = abyss_room_profile(next.state, rules);
                auto validation_state = next.state;
                if (reward.has_value()
                        && reward->rarity != items::ItemRarity::rare
                        && profile.has_value()
                        && arpg::test::install_stage10_validation_build(
                            validation_state)
                        && arpg::test::install_stage10_validation_survival_passives(
                            validation_state)
                        && prepare_stage11d_live_damage_build(
                            validation_state)) {
                    selected.abyss = validation_state;
                    selected.abyss_blueprint_hash = profile->blueprint_hash;
                    selected.abyss_monster_count = profile->monster_count;
                    selected.abyss_initial_owned_item_count =
                        static_cast<std::uint16_t>(validation_state
                            .item_ownership.items.size());
                    selected.abyss_presented_frame_budget =
                        profile->presented_frame_budget;
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
    draft.vsync_enabled = kStage11DFormalVsyncEnabled;
    const auto settings_saved = settings_store.save(loaded.settings, draft);
    const bool abyss_build_round_tripped = !spec.abyss
        || (saved.verified_state.item_ownership.items.size()
                == selected.abyss.item_ownership.items.size()
            && saved.verified_state.item_ownership.equipment.equipped_ids
                == selected.abyss.item_ownership.equipment.equipped_ids
            && saved.verified_state.passive_tree.allocated_bits
                == selected.abyss.passive_tree.allocated_bits
            && saved.verified_state.progression.level
                == selected.abyss.progression.level
            && saved.verified_state.progression.earned_passive_points
                == selected.abyss.progression.earned_passive_points
            && saved.verified_state.progression.unspent_passive_points
                == selected.abyss.progression.unspent_passive_points);
    return saved.state == persistence::SaveCommitState::committed
        && abyss_build_round_tripped
        && settings_saved.status == settings::SettingsSaveStatus::committed
        && settings_saved.settings.vsync_enabled
            == kStage11DFormalVsyncEnabled;
}

[[nodiscard]] bool fresh_file(const std::filesystem::path& path,
    std::filesystem::file_time_type started, std::uintmax_t minimum) {
    std::error_code error{};
    const auto size = std::filesystem::file_size(path, error);
    if (error || size <= minimum) return false;
    return std::filesystem::last_write_time(path, error) >= started && !error;
}

[[nodiscard]] bool run_host(const std::filesystem::path& root,
    const ScenarioSpec& spec, const SelectedStates& selected) {
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
    config.validation_exit_after_presented_frames = spec.abyss
        ? selected.abyss_presented_frame_budget
        : kStage11DDefaultPresentedFrameLimit;
    if (config.validation_exit_after_presented_frames == 0U) return false;
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
        const auto profile = ordinary_room_profile(
            selected.ordinary, dungeon::DungeonRules{});
        std::cout << "ordinary root=" << selected.ordinary.root_seed
            << " room=" << selected.ordinary.current_room.index
            << " seed=" << selected.ordinary.current_room.seed
            << " depth=" << selected.ordinary.current_room.depth
            << " entry=" << static_cast<unsigned>(
                selected.ordinary.current_room.entry)
            << " ecology=" << static_cast<unsigned>(
                selected.ordinary.current_room.ecology)
            << " monsters=" << profile->monster_count
            << " residents=" << profile->initial_resident_count
            << " prefix_kills=" << profile->required_kills
            << " prefix_hp=" << profile->prefix_hp
            << " prefix_danger=" << profile->prefix_affix_danger
            << " ranged=" << profile->prefix_ranged
            << " hazards=" << profile->prefix_ground_hazards
            << " blueprint=" << profile->blueprint_hash
            << " drops=";
        for (std::size_t index = 0U;
             index < profile->drop_ordinals.size(); ++index) {
            if (index != 0U) std::cout << ',';
            std::cout << profile->drop_ordinals[index] << ':'
                << static_cast<unsigned>(profile->drop_rarities[index]) << ':'
                << profile->drop_item_ids[index];
        }
        std::cout << '\n';
        std::cout << "abyss root=" << selected.abyss.root_seed
            << " room=" << selected.abyss.current_room.index
            << " seed=" << selected.abyss.current_room.seed
            << " rule=" << static_cast<unsigned>(selected.abyss.abyss.rule)
            << " monsters=" << selected.abyss_monster_count
            << " blueprint=" << selected.abyss_blueprint_hash
            << " frame_budget=" << selected.abyss_presented_frame_budget
            << " owned_items=" << selected.abyss_initial_owned_item_count
            << " passive_bits="
                << selected.abyss.passive_tree.allocated_bits
            << " vsync=" << (kStage11DFormalVsyncEnabled ? 1 : 0)
            << '\n';
        return 0;
    }
    if (argc == 4 && std::string{argv[1]} == "--scenario") {
        const auto* spec = find_scenario(argv[2]);
        const SelectedStates selected = select_states();
        return spec != nullptr && selected.ordinary_ready
                && selected.abyss_ready
                && run_host(argv[3], *spec, selected) ? 0 : 1;
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
        << "ordinary_seed=" << selected.ordinary.current_room.seed << '\n'
        << "ordinary_depth=" << selected.ordinary.current_room.depth << '\n'
        << "ordinary_monsters=" << selected.ordinary_monster_count << '\n'
        << "ordinary_initial_residents="
            << selected.ordinary_initial_resident_count << '\n'
        << "ordinary_generator_version="
            << dungeon::kRoomMonsterGeneratorVersion << '\n'
        << "ordinary_blueprint_hash="
            << selected.ordinary_blueprint_hash << '\n'
        << "ordinary_prefix_kills=" << selected.ordinary_required_kills << '\n'
        << "ordinary_drop_ordinals="
            << selected.ordinary_drop_ordinals[0] << ','
            << selected.ordinary_drop_ordinals[1] << ','
            << selected.ordinary_drop_ordinals[2] << '\n'
        << "ordinary_drop_item_ids="
            << selected.ordinary_drop_item_ids[0] << ','
            << selected.ordinary_drop_item_ids[1] << ','
            << selected.ordinary_drop_item_ids[2] << '\n'
        << "abyss_root=" << selected.abyss.root_seed << '\n'
        << "abyss_room=" << selected.abyss.current_room.index << '\n'
        << "abyss_seed=" << selected.abyss.current_room.seed << '\n'
        << "abyss_rule=" << static_cast<unsigned>(
            selected.abyss.abyss.rule) << '\n'
        << "abyss_monsters=" << selected.abyss_monster_count << '\n'
        << "abyss_generator_version="
            << dungeon::kRoomMonsterGeneratorVersion << '\n'
        << "abyss_blueprint_hash=" << selected.abyss_blueprint_hash << '\n'
        << "abyss_initial_owned_items="
            << selected.abyss_initial_owned_item_count << '\n'
        << "abyss_initial_equipped_items="
            << std::count_if(selected.abyss.item_ownership.equipment
                    .equipped_ids.begin(),
                selected.abyss.item_ownership.equipment.equipped_ids.end(),
                [](std::uint64_t id) { return id != 0U; }) << '\n'
        << "abyss_validation_level="
            << static_cast<unsigned>(selected.abyss.progression.level) << '\n'
        << "abyss_validation_earned_passives="
            << static_cast<unsigned>(
                selected.abyss.progression.earned_passive_points) << '\n'
        << "abyss_validation_unspent_passives="
            << static_cast<unsigned>(
                selected.abyss.progression.unspent_passive_points) << '\n'
        << "abyss_validation_passive_bits="
            << selected.abyss.passive_tree.allocated_bits << '\n'
        << "abyss_presented_frame_budget="
            << selected.abyss_presented_frame_budget << '\n'
        << "formal_vsync_enabled="
            << (kStage11DFormalVsyncEnabled ? 1 : 0) << '\n';
    manifest << "abyss_validation_item_ids=";
    for (std::size_t index = 0U;
         index < selected.abyss.item_ownership.items.size(); ++index) {
        if (index != 0U) manifest << ',';
        manifest << selected.abyss.item_ownership.items[index].id;
    }
    manifest << '\n' << "abyss_validation_equipped_ids=";
    bool first_equipped = true;
    for (const std::uint64_t id :
            selected.abyss.item_ownership.equipment.equipped_ids) {
        if (id == 0U) continue;
        if (!first_equipped) manifest << ',';
        manifest << id;
        first_equipped = false;
    }
    manifest << '\n';
    const auto write_ordinary_tuple = [&](const char* name,
                                          items::ItemRarity rarity) {
        for (std::size_t index = 0U;
             index < selected.ordinary_drop_ordinals.size(); ++index) {
            if (selected.ordinary_drop_rarities[index] != rarity) continue;
            manifest << name << '=' << selected.ordinary_drop_item_ids[index]
                << ',' << selected.ordinary_drop_ordinals[index] << '\n';
            return;
        }
        manifest << name << "=0,65535\n";
    };
    write_ordinary_tuple("ordinary_normal_tuple", items::ItemRarity::normal);
    write_ordinary_tuple("ordinary_magic_tuple", items::ItemRarity::magic);
    write_ordinary_tuple("ordinary_rare_tuple", items::ItemRarity::rare);
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
