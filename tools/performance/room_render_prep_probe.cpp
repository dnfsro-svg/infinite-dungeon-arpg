#include "combat_renderer.hpp"
#include "dungeon/dungeon_session.hpp"
#include "environment_prop_layout.hpp"
#include "material_residency.hpp"
#include "room_background_render_plan.hpp"

#if ARPG_TASK11_CURRENT_RENDER_PREP
#include "abyss/abyss_rules.hpp"
#include "checkpoint/room_progress_checkpoint.hpp"
#include "combat_view_math.hpp"
#include "dungeon/dungeon_progression.hpp"
#include "dungeon/dungeon_render_snapshot.hpp"
#include "dungeon/room_affix.hpp"
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#define CloseWindow CloseWindowWin32
#define ShowCursor ShowCursorWin32
#include <windows.h>
#undef ShowCursor
#undef CloseWindow

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <memory>
#include <new>
#include <string>
#include <vector>

#if !defined(_WIN32) || !defined(NDEBUG)
#error "Task 11 room render preparation probe requires Windows Release"
#endif

#ifndef ARPG_TASK11_BENCHMARK_REVISION
#define ARPG_TASK11_BENCHMARK_REVISION "unknown"
#endif

namespace {

constexpr float kViewportWidth = 1920.0F;
constexpr float kViewportHeight = 1080.0F;
constexpr std::size_t kDefaultFrames = 10'240U;
constexpr std::size_t kDefaultWarmup = 2'048U;
constexpr std::size_t kTimerCalibrationSamples = 10'000U;
constexpr std::size_t kVisibleMonsterCount = 8U;
constexpr std::size_t kVisibleEquipmentCount = 3U;
constexpr std::size_t kVisibleMaterialCount = 1U;
constexpr std::size_t kVisiblePotionCount = 2U;
constexpr std::size_t kVisiblePropCount = 5U;

struct Options final {
    std::filesystem::path output_directory{};
    std::string label{};
    std::string revision{ARPG_TASK11_BENCHMARK_REVISION};
    std::size_t frames{kDefaultFrames};
    std::size_t warmup{kDefaultWarmup};
    unsigned int cpu{};
};

struct PreparedFrame final {
    std::uint64_t checksum{};
    std::uint64_t visible_signature{};
    std::uint64_t visible_density_signature{};
    std::uint64_t variant_output_signature{};
    std::uint64_t production_signature{};
    std::uint16_t background_tiles{};
    std::uint16_t props{};
    std::uint16_t monsters{};
    std::uint16_t equipment{};
    std::uint16_t materials{};
    std::uint16_t potions{};
    std::uint16_t production_population{};
    std::uint16_t production_visible_monsters{};
    std::uint16_t production_environment_candidates{};
    std::uint16_t production_drop_candidates{};
    bool valid{};
};

struct Sample final {
    std::uint64_t cycles{};
    PreparedFrame frame{};
};

struct Fixture final {
    std::unique_ptr<arpg::dungeon::DungeonSnapshot> previous{};
    std::unique_ptr<arpg::dungeon::DungeonSnapshot> current{};
    std::unique_ptr<arpg::dungeon::DungeonSession> production_session{};
#if ARPG_TASK11_CURRENT_RENDER_PREP
    std::unique_ptr<arpg::dungeon::DungeonRenderSnapshot> world{};
    std::unique_ptr<arpg::dungeon::DungeonRenderSnapshot> production_world{};
#endif
};

[[nodiscard]] std::uint64_t mix(std::uint64_t hash,
    std::uint64_t value) noexcept {
    hash ^= value;
    hash *= 1099511628211ULL;
    return hash;
}

[[nodiscard]] std::uint64_t float_bits(float value) noexcept {
    std::uint32_t bits{};
    std::memcpy(&bits, &value, sizeof(bits));
    return bits;
}

[[nodiscard]] std::uint64_t mix_float(
    std::uint64_t hash, float value) noexcept {
    return mix(hash, float_bits(value));
}

[[nodiscard]] std::uint64_t mix_vec3(std::uint64_t hash,
    arpg::combat::Vec3 value) noexcept {
    hash = mix_float(hash, value.x);
    hash = mix_float(hash, value.y);
    return mix_float(hash, value.z);
}

[[nodiscard]] std::uint64_t mix_rect(std::uint64_t hash,
    const arpg::platform::LootLabelRect& value) noexcept {
    hash = mix_float(hash, value.x);
    hash = mix_float(hash, value.y);
    hash = mix_float(hash, value.width);
    return mix_float(hash, value.height);
}

[[nodiscard]] std::uint64_t mix_color(std::uint64_t hash,
    arpg::platform::Rgba8 value) noexcept {
    hash = mix(hash, value.r);
    hash = mix(hash, value.g);
    hash = mix(hash, value.b);
    return mix(hash, value.a);
}

template <typename Range>
[[nodiscard]] std::uint64_t mix_bytes(
    std::uint64_t hash, const Range& bytes) noexcept {
    hash = mix(hash, bytes.size());
    for (const auto value : bytes) {
        hash = mix(hash, static_cast<unsigned char>(value));
    }
    return hash;
}

[[nodiscard]] std::uint16_t active_monster_count(
    const arpg::combat::CombatSnapshot& combat) noexcept {
    const std::size_t limit = (std::min)(
        static_cast<std::size_t>(combat.monster_count),
        combat.monsters.size());
    std::uint16_t count{};
    for (std::size_t index{}; index < limit; ++index) {
        if (combat.monsters[index].active) ++count;
    }
    return count;
}

[[nodiscard]] std::uint64_t density_signature(
    const PreparedFrame& frame) noexcept {
    std::uint64_t hash = 1469598103934665603ULL;
    hash = mix(hash, frame.monsters);
    hash = mix(hash, frame.props);
    hash = mix(hash, frame.equipment);
    hash = mix(hash, frame.materials);
    hash = mix(hash, frame.potions);
    return hash;
}

template <typename PropLayout>
[[nodiscard]] std::uint64_t actual_prop_signature(
    const PropLayout& props) noexcept {
    std::uint64_t hash = 1469598103934665603ULL;
    hash = mix(hash, props.count);
    const std::size_t limit = (std::min)(props.count, props.props.size());
    for (std::size_t index{}; index < limit; ++index) {
        const auto& prop = props.props[index];
        hash = mix(hash, index);
        hash = mix(hash, static_cast<std::uint64_t>(prop.sprite));
        hash = mix(hash, prop.flip_x ? 1U : 0U);
        hash = mix_float(hash, prop.scale);
#if ARPG_TASK11_CURRENT_RENDER_PREP
        hash = mix(hash, prop.ordinal);
        hash = mix_vec3(hash, prop.world_foot_position);
        hash = mix(hash, prop.quarter_turns);
        hash = mix(hash, static_cast<std::uint64_t>(prop.obstacle_kind));
        hash = mix_vec3(hash, prop.world_obstacle_bounds.minimum);
        hash = mix_vec3(hash, prop.world_obstacle_bounds.maximum);
        hash = mix(hash, static_cast<std::uint64_t>(prop.visual_state));
#else
        hash = mix_float(hash, prop.normalized_foot_position.x);
        hash = mix_float(hash, prop.normalized_foot_position.y);
#endif
    }
    return hash;
}

template <typename PropLayout>
[[nodiscard]] std::uint64_t variant_layout_signature(
    const PropLayout& props,
    const arpg::platform::CombatRenderPlan& combat) noexcept {
    std::uint64_t hash = actual_prop_signature(props);
    const std::size_t equipment_limit = (std::min)(
        combat.ground_loot.count, combat.ground_loot.labels.size());
    for (std::size_t index{}; index < equipment_limit; ++index) {
        const auto& label = combat.ground_loot.labels[index];
        hash = mix_float(hash, label.anchor_x);
        hash = mix_float(hash, label.anchor_y);
        hash = mix_rect(hash, label.rect);
        hash = mix_color(hash, label.text_color);
        hash = mix_color(hash, label.border_color);
    }
    const std::size_t secondary_limit = (std::min)(
        combat.material_loot.count, combat.material_loot.labels.size());
    for (std::size_t index{}; index < secondary_limit; ++index) {
        const auto& label = combat.material_loot.labels[index];
        hash = mix_float(hash, label.anchor_x);
        hash = mix_float(hash, label.anchor_y);
        hash = mix_rect(hash, label.rect);
        hash = mix_color(hash, label.text_color);
    }
    return hash;
}

[[nodiscard]] std::uint64_t stable_output_signature(
    const arpg::combat::CombatSnapshot& visible_combat,
    const arpg::platform::CombatRenderPlan& combat,
    std::size_t prop_count) noexcept {
    std::uint64_t hash = 1469598103934665603ULL;
    hash = mix(hash, active_monster_count(visible_combat));
    hash = mix(hash, prop_count);
    hash = mix(hash, combat.ground_loot.count);
    hash = mix(hash, combat.material_loot.count);

    const std::size_t monster_limit = (std::min)(
        static_cast<std::size_t>(visible_combat.monster_count),
        visible_combat.monsters.size());
    for (std::size_t index{}; index < monster_limit; ++index) {
        const auto& monster = visible_combat.monsters[index];
        if (!monster.active) continue;
        hash = mix(hash, index);
        hash = mix(hash, monster.generation);
        hash = mix(hash, static_cast<std::uint64_t>(monster.id));
        hash = mix(hash, monster.spawn_ordinal);
        hash = mix_vec3(hash, monster.position);
        hash = mix(hash, static_cast<std::uint64_t>(monster.hp));
        hash = mix(hash, static_cast<std::uint64_t>(monster.max_hp));
    }

    const std::size_t stage_limit = (std::min)(
        combat.stage_count, combat.stages.size());
    hash = mix(hash, stage_limit);
    for (std::size_t index{}; index < stage_limit; ++index) {
        hash = mix(hash, static_cast<std::uint64_t>(combat.stages[index]));
    }

    const std::size_t equipment_limit = (std::min)(
        combat.ground_loot.count, combat.ground_loot.labels.size());
    for (std::size_t index{}; index < equipment_limit; ++index) {
        const auto& label = combat.ground_loot.labels[index];
        hash = mix(hash, label.ordinal);
        hash = mix(hash, static_cast<std::uint64_t>(label.item_sprite));
        hash = mix(hash, static_cast<std::uint64_t>(label.rarity_sprite));
        hash = mix(hash, label.abyss ? 1U : 0U);
        hash = mix_bytes(hash, label.text);
    }

    const std::size_t secondary_limit = (std::min)(
        combat.material_loot.count, combat.material_loot.labels.size());
    for (std::size_t index{}; index < secondary_limit; ++index) {
        const auto& label = combat.material_loot.labels[index];
        hash = mix(hash, static_cast<std::uint64_t>(label.kind));
        hash = mix(hash, label.ordinal);
        hash = mix(hash, static_cast<std::uint64_t>(label.sprite));
        hash = mix(hash, label.emphasized ? 1U : 0U);
        hash = mix_bytes(hash, label.text);
    }
    return hash;
}

[[nodiscard]] bool has_expected_visible_density(
    const PreparedFrame& frame) noexcept {
    return frame.monsters == kVisibleMonsterCount
        && frame.props == kVisiblePropCount
        && frame.equipment == kVisibleEquipmentCount
        && frame.materials == kVisibleMaterialCount
        && frame.potions == kVisiblePotionCount;
}

[[nodiscard]] bool parse_size(const char* text, std::size_t& value) noexcept {
    if (text == nullptr || *text == '\0') return false;
    char* end{};
    const unsigned long long parsed = std::strtoull(text, &end, 10);
    if (end == text || *end != '\0' || parsed == 0U) return false;
    value = static_cast<std::size_t>(parsed);
    return static_cast<unsigned long long>(value) == parsed;
}

[[nodiscard]] bool parse_cpu(const char* text, unsigned int& value) noexcept {
    if (text == nullptr || *text == '\0') return false;
    char* end{};
    const unsigned long parsed = std::strtoul(text, &end, 10);
    if (end == text || *end != '\0' || parsed > 63U) return false;
    value = static_cast<unsigned int>(parsed);
    return true;
}

[[nodiscard]] bool parse_options(int argc, char** argv,
    Options& options) noexcept {
    for (int index = 1; index < argc; index += 2) {
        if (index + 1 >= argc) return false;
        const std::string key{argv[index]};
        const char* const value = argv[index + 1];
        if (key == "--output") {
            options.output_directory = value;
        } else if (key == "--label") {
            options.label = value;
        } else if (key == "--revision") {
            options.revision = value;
        } else if (key == "--frames") {
            if (!parse_size(value, options.frames)) return false;
        } else if (key == "--warmup") {
            if (!parse_size(value, options.warmup)) return false;
        } else if (key == "--cpu") {
            if (!parse_cpu(value, options.cpu)) return false;
        } else {
            return false;
        }
    }
    return !options.output_directory.empty() && !options.label.empty()
        && options.frames >= 10'000U;
}

void initialize_snapshot(arpg::dungeon::DungeonSnapshot& snapshot) noexcept {
    snapshot = {};
    snapshot.has_active_room = true;
    snapshot.ecology = arpg::dungeon::DungeonElement::water;
    snapshot.combat.emplace();
    snapshot.combat->player.position = {};
    snapshot.combat->monster_count = kVisibleMonsterCount;
    for (std::size_t index{}; index < kVisibleMonsterCount; ++index) {
        auto& monster = snapshot.combat->monsters[index];
        monster.active = true;
        monster.generation = static_cast<std::uint16_t>(index + 1U);
        monster.spawn_ordinal = static_cast<std::uint16_t>(index);
        monster.id = index % 2U == 0U
            ? arpg::combat::MonsterId::water_bulwark
            : arpg::combat::MonsterId::water_support;
        monster.position = {
            -7.0F + static_cast<float>(index % 4U) * 4.0F,
            -3.0F + static_cast<float>(index / 4U) * 5.0F,
            0.0F};
        monster.hp = 100;
        monster.max_hp = 100;
    }

    snapshot.ground_item_count = kVisibleEquipmentCount;
    for (std::size_t index{}; index < kVisibleEquipmentCount; ++index) {
        auto& item = snapshot.ground_items[index];
        item.ordinal = static_cast<std::uint16_t>(10U + index * 10U);
        item.position = {-4.0F + static_cast<float>(index) * 4.0F,
            1.0F, 0.0F};
        item.item_id = 1000U + index;
        item.base_id = 1U;
        item.item_level = 40U;
        item.rarity = index == 0U ? arpg::items::ItemRarity::normal
            : index == 1U ? arpg::items::ItemRarity::magic
                : arpg::items::ItemRarity::rare;
    }
    snapshot.ground_material_count = kVisibleMaterialCount;
    snapshot.ground_materials[0] = {40U,
        arpg::dungeon::GroundMaterialSource::monster_common,
        {5.0F, 2.0F, 0.0F}, arpg::items::MaterialId::chaos};
    snapshot.ground_health_potion_count = kVisiblePotionCount;
    for (std::size_t index{}; index < kVisiblePotionCount; ++index) {
        snapshot.ground_health_potions[index] = {
            static_cast<std::uint16_t>(index),
            static_cast<std::uint16_t>(index * 2U + 1U),
            {-6.0F + static_cast<float>(index) * 12.0F, 3.0F, 0.0F}};
    }
}

[[nodiscard]] bool enter_production_combat(
    arpg::dungeon::DungeonSession& session) noexcept {
    for (std::size_t step{}; step < 8U; ++step) {
        if (session.phase() == arpg::dungeon::RoomPhase::combat) return true;
        const arpg::dungeon::PendingSave* const pending =
            session.pending_save_view();
        if (pending != nullptr) {
            session.resolve_pending_save({
                arpg::dungeon::SaveDisposition::committed,
                pending->expected_generation, pending->next_state,
                pending->kind});
        } else {
            session.tick({});
        }
    }
    return session.phase() == arpg::dungeon::RoomPhase::combat;
}

#if ARPG_TASK11_CURRENT_RENDER_PREP
[[nodiscard]] std::uint64_t maximum_abyss_seed() noexcept {
    for (std::uint64_t seed = 1U; seed < 2'000'000U; ++seed) {
        if (arpg::dungeon::roll_room_density(seed, true).total_count == 1125U
                && arpg::abyss::is_abyss_roll(seed)) {
            return seed;
        }
    }
    return 0U;
}

[[nodiscard]] std::unique_ptr<arpg::dungeon::DungeonSession>
make_maximum_production_session() noexcept {
    const arpg::dungeon::DungeonRules rules{};
    const std::uint64_t seed = maximum_abyss_seed();
    if (seed == 0U) return {};
    auto built = arpg::dungeon::make_initial_run_state(
        0x4C41524745524F4FULL, rules);
    if (built.fault != arpg::dungeon::DungeonFault::none) return {};
    auto& state = built.state;
    state.current_room.seed = seed;
    state.current_room.depth = 40U;
    state.current_room.entry = arpg::dungeon::EntrySide::left;
    state.current_room.ecology = arpg::dungeon::DungeonElement::chaos;
    state.current_room.has_hole = true;
    state.current_room.is_abyss = true;
    state.last_transition = arpg::dungeon::TransitionKind::door;
    state.last_direction = arpg::dungeon::ExitDirection::right;
    const auto selection = arpg::abyss::select_abyss_rule(seed, 40U);
    if (!selection.has_value()) return {};
    state.abyss.lifecycle = arpg::abyss::AbyssLifecycle::available;
    state.abyss.danger = selection->danger;
    state.abyss.rule = selection->rule;
    state.abyss.rules_version = selection->rules_version;
    auto session = std::unique_ptr<arpg::dungeon::DungeonSession>{
        new (std::nothrow) arpg::dungeon::DungeonSession{rules, state}};
    if (session == nullptr || !enter_production_combat(*session)
            || session->snapshot().initial_monster_count != 1125U) {
        return {};
    }

    auto checkpoint = std::unique_ptr<arpg::checkpoint::SaveCheckpointSlot>{
        new (std::nothrow) arpg::checkpoint::SaveCheckpointSlot{}};
    if (checkpoint == nullptr
            || !session->capture_save_checkpoint(*checkpoint, 1U)) {
        return {};
    }
    auto& room = checkpoint->room_progress;
    if (room.secondary_ground_count != 0U
            || room.generated_monsters != 1125U
            || room.generated_monsters > room.secondary_ground.size()) {
        return {};
    }
    room.secondary_ground_count = static_cast<std::uint16_t>(
        room.generated_monsters);
    for (std::uint16_t spawn{}; spawn < room.secondary_ground_count; ++spawn) {
        auto& ground = room.secondary_ground[spawn];
        ground.tag = arpg::checkpoint::SecondaryGroundTag::material;
        ground.ordinal = static_cast<std::uint16_t>(spawn * 2U);
        ground.source = 0U;
        ground.position = {};
        ground.material = arpg::items::MaterialId::reinforcement_stone;
    }
    auto restored = std::unique_ptr<arpg::dungeon::DungeonSession>{
        new (std::nothrow) arpg::dungeon::DungeonSession{
            rules, checkpoint->state}};
    if (restored == nullptr
            || !restored->restore_room_progress_checkpoint(*checkpoint)) {
        return {};
    }
    return restored;
}
#endif

[[nodiscard]] std::unique_ptr<Fixture> make_fixture() noexcept {
    auto fixture = std::unique_ptr<Fixture>{new (std::nothrow) Fixture{}};
    if (fixture == nullptr) return {};
    fixture->previous.reset(
        new (std::nothrow) arpg::dungeon::DungeonSnapshot{});
    fixture->current.reset(
        new (std::nothrow) arpg::dungeon::DungeonSnapshot{});
    if (fixture->previous == nullptr || fixture->current == nullptr) return {};
    initialize_snapshot(*fixture->previous);
    initialize_snapshot(*fixture->current);
    fixture->previous->combat->player.position = {-0.25F, 0.0F, 0.0F};
#if ARPG_TASK11_CURRENT_RENDER_PREP
    fixture->production_session = make_maximum_production_session();
    fixture->world.reset(
        new (std::nothrow) arpg::dungeon::DungeonRenderSnapshot{});
    fixture->production_world.reset(
        new (std::nothrow) arpg::dungeon::DungeonRenderSnapshot{});
    if (fixture->production_session == nullptr || fixture->world == nullptr
            || fixture->production_world == nullptr) {
        return {};
    }
    fixture->world->has_active_room = true;
    fixture->world->has_combat = true;
    fixture->world->ecology = fixture->current->ecology;
    fixture->world->combat = *fixture->current->combat;
    fixture->world->equipment_count = kVisibleEquipmentCount;
    for (std::size_t index{}; index < kVisibleEquipmentCount; ++index) {
        fixture->world->equipment[index] = fixture->current->ground_items[index];
    }
    fixture->world->material_count = kVisibleMaterialCount;
    fixture->world->materials[0] = fixture->current->ground_materials[0];
    fixture->world->health_potion_count = kVisiblePotionCount;
    for (std::size_t index{}; index < kVisiblePotionCount; ++index) {
        fixture->world->health_potions[index] =
            fixture->current->ground_health_potions[index];
    }
    constexpr std::array<arpg::combat::RoomPropKind, kVisiblePropCount>
        kKinds{{arpg::combat::RoomPropKind::lantern,
            arpg::combat::RoomPropKind::coral,
            arpg::combat::RoomPropKind::coral,
            arpg::combat::RoomPropKind::coral,
            arpg::combat::RoomPropKind::grate}};
    constexpr std::array<arpg::combat::Vec3, kVisiblePropCount> kAnchors{{
        {-8.5F, -3.4F, 0.0F}, {8.5F, -3.4F, 0.0F},
        {-8.1F, 3.0F, 0.0F}, {7.0F, 3.0F, 0.0F},
        {-5.0F, 4.2F, 0.0F}}};
    fixture->world->environment.count =
        static_cast<std::uint16_t>(kVisiblePropCount);
    for (std::size_t index{}; index < kVisiblePropCount; ++index) {
        auto& record = fixture->world->environment.records[index];
        record.ordinal = static_cast<std::uint16_t>(index);
        record.home_cell = static_cast<std::uint16_t>(index);
        record.prop = kKinds[index];
        record.anchor = kAnchors[index];
        record.mirror_x = index == 3U;
        record.scale_bp = 10000U;
    }
#else
    fixture->production_session.reset(
        new (std::nothrow) arpg::dungeon::DungeonSession{});
    if (fixture->production_session == nullptr
            || !enter_production_combat(*fixture->production_session)) {
        return {};
    }
#endif
    return fixture;
}

[[nodiscard]] PreparedFrame prepare_frame(
    const Fixture& fixture, std::size_t frame_index) noexcept {
    PreparedFrame result{};
    const float alpha = static_cast<float>(frame_index % 61U) / 60.0F;
    constexpr arpg::platform::CameraOffset kCameraOffset{};
#if ARPG_TASK11_CURRENT_RENDER_PREP
    const auto camera = arpg::platform::make_combat_camera_view(
        {0.0F, 0.0F, 0.0F}, kViewportWidth, kViewportHeight);
    const auto query = arpg::platform::make_world_view_query(
        camera, kViewportWidth, kViewportHeight, frame_index + 1U);
    const bool production_ready = fixture.production_session
        ->write_render_snapshot(query, *fixture.production_world);
    const auto combat = arpg::platform::make_combat_render_plan(
        *fixture.previous, *fixture.world, true, alpha, camera,
        kCameraOffset, arpg::settings::LootFilterMode::show_all,
        kViewportWidth, kViewportHeight);
    const auto background = arpg::platform::room_background_world_tile_plan(
        fixture.world->ecology, camera);
    const auto props = arpg::platform::environment_prop_layout(*fixture.world);
    result.valid = production_ready && background.valid
        && props.count == kVisiblePropCount;
    result.background_tiles = static_cast<std::uint16_t>(background.count);
    result.checksum = query.camera_version;
    result.production_population = 1125U;
    if (production_ready) {
        const auto& production = *fixture.production_world;
        result.production_visible_monsters =
            active_monster_count(production.combat);
        result.production_environment_candidates =
            production.environment.candidates_examined;
        result.production_drop_candidates = production.drop_candidates_examined;
        result.production_signature = mix(1469598103934665603ULL,
            result.production_population);
        result.production_signature = mix(result.production_signature,
            result.production_visible_monsters);
        result.production_signature = mix(result.production_signature,
            result.production_environment_candidates);
        result.production_signature = mix(result.production_signature,
            result.production_drop_candidates);
        const std::size_t monster_limit = (std::min)(
            static_cast<std::size_t>(production.combat.monster_count),
            production.combat.monsters.size());
        for (std::size_t index{}; index < monster_limit; ++index) {
            const auto& monster = production.combat.monsters[index];
            if (!monster.active) continue;
            result.production_signature = mix(result.production_signature,
                monster.monster_ordinal);
            result.production_signature = mix(result.production_signature,
                static_cast<std::uint64_t>(monster.id));
        }
        for (std::size_t index{}; index < production.environment.count;
                ++index) {
            result.production_signature = mix(result.production_signature,
                production.environment.records[index].ordinal);
        }
    }
    for (std::size_t index{}; index < background.count; ++index) {
        const auto projected =
            arpg::platform::project_room_background_world_tile(
                background.tiles[index], camera,
                kViewportWidth, kViewportHeight);
        result.valid = result.valid && projected.valid;
        result.checksum = mix(result.checksum,
            float_bits(projected.destination.top_left.x));
        result.checksum = mix(result.checksum,
            float_bits(projected.destination.bottom_right.y));
    }
    for (std::size_t index{}; index < props.count; ++index) {
        const auto projected = arpg::platform::project_environment_prop(
            props.props[index], camera, kViewportWidth, kViewportHeight);
        result.valid = result.valid && projected.scale > 0.0F;
        result.checksum = mix(result.checksum,
            float_bits(projected.foot_position.x));
        result.checksum = mix(result.checksum,
            float_bits(projected.foot_position.y));
    }
#else
    const arpg::dungeon::DungeonSnapshot production =
        fixture.production_session->snapshot();
    const auto combat = arpg::platform::make_combat_render_plan(
        *fixture.previous, *fixture.current, alpha, kCameraOffset,
        arpg::settings::LootFilterMode::show_all,
        kViewportWidth, kViewportHeight);
    const auto background = arpg::platform::room_background_render_plan(
        fixture.current->ecology);
    const float background_scale = arpg::platform::room_background_scale(
        kViewportWidth, kViewportHeight);
    const auto props = arpg::platform::environment_prop_layout(
        fixture.current->ecology, kViewportWidth, kViewportHeight);
    result.valid = background_scale > 0.0F
        && props.count == kVisiblePropCount && production.combat.has_value();
    result.background_tiles = 1U;
    result.checksum = static_cast<std::uint64_t>(background.atlas);
    if (production.combat.has_value()) {
        result.production_population = active_monster_count(*production.combat);
        result.production_visible_monsters = result.production_population;
        result.production_signature = mix(1469598103934665603ULL,
            result.production_population);
        const std::size_t monster_limit = (std::min)(
            static_cast<std::size_t>(production.combat->monster_count),
            production.combat->monsters.size());
        for (std::size_t index{}; index < monster_limit; ++index) {
            const auto& monster = production.combat->monsters[index];
            if (!monster.active) continue;
            result.production_signature = mix(result.production_signature,
                monster.spawn_ordinal);
            result.production_signature = mix(result.production_signature,
                static_cast<std::uint64_t>(monster.id));
        }
    }
    result.checksum = mix(result.checksum, float_bits(background_scale));
    for (std::size_t index{}; index < props.count; ++index) {
        const auto* const definition =
            arpg::platform::environment_prop_definition(props.props[index].sprite);
        result.valid = result.valid && definition != nullptr;
        if (definition == nullptr) continue;
        const auto projected =
            arpg::platform::project_environment_prop_bounds(
                *definition, props.props[index],
                kViewportWidth, kViewportHeight);
        result.checksum = mix(result.checksum, float_bits(projected.x));
        result.checksum = mix(result.checksum, float_bits(projected.y));
    }
#endif
#if ARPG_TASK11_CURRENT_RENDER_PREP
    const auto residency = arpg::platform::world_material_residency_request(
        *fixture.world, fixture.current->skill_loadout);
    const auto& visible_combat = fixture.world->combat;
#else
    const auto residency = arpg::platform::make_material_residency_request(
        *fixture.current);
    const auto& visible_combat = *fixture.current->combat;
#endif
    result.checksum = mix(result.checksum, residency.atlases);
    result.checksum = mix(result.checksum, combat.stage_count);
    result.checksum = mix(result.checksum, combat.ground_loot.count);
    result.checksum = mix(result.checksum, combat.material_loot.count);

    result.props = static_cast<std::uint16_t>(props.count);
    result.monsters = active_monster_count(visible_combat);
    result.equipment = static_cast<std::uint16_t>(combat.ground_loot.count);

    result.valid = result.valid
        && combat.stage_count <= combat.stages.size()
        && combat.ground_loot.count <= combat.ground_loot.labels.size()
        && combat.material_loot.count <= combat.material_loot.labels.size();
#if ARPG_TASK11_CURRENT_RENDER_PREP
    result.valid = result.valid && props.status
        == arpg::platform::EnvironmentPropLayoutStatus::ok;
#endif
    const std::size_t secondary_count = (std::min)(
        combat.material_loot.count, combat.material_loot.labels.size());
    for (std::size_t index{}; index < secondary_count; ++index) {
        const auto& label = combat.material_loot.labels[index];
        if (label.kind == arpg::platform::SecondaryLootKind::material) {
            ++result.materials;
        } else {
            ++result.potions;
        }
    }
    result.visible_density_signature = density_signature(result);
    result.visible_signature = stable_output_signature(
        visible_combat, combat, props.count);
    result.variant_output_signature = variant_layout_signature(props, combat);
    result.checksum = mix(result.checksum, result.visible_signature);
    result.checksum = mix(result.checksum, result.variant_output_signature);
    result.checksum = mix(result.checksum, result.production_signature);
    result.valid = result.valid && has_expected_visible_density(result);
    return result;
}

[[nodiscard]] bool read_thread_cycles(std::uint64_t& value) noexcept {
    ULONG64 cycles{};
    if (!QueryThreadCycleTime(GetCurrentThread(), &cycles)) return false;
    value = static_cast<std::uint64_t>(cycles);
    return true;
}

[[nodiscard]] std::uint64_t file_time_value(FILETIME value) noexcept {
    ULARGE_INTEGER converted{};
    converted.LowPart = value.dwLowDateTime;
    converted.HighPart = value.dwHighDateTime;
    return converted.QuadPart;
}

[[nodiscard]] std::uint64_t percentile(
    std::vector<std::uint64_t> values, std::size_t numerator) {
    std::sort(values.begin(), values.end());
    const std::size_t rank = (values.size() * numerator + 99U) / 100U;
    return values[(std::max)(std::size_t{1U}, rank) - 1U];
}

[[nodiscard]] bool write_outputs(const Options& options,
    const std::vector<Sample>& samples,
    const std::vector<std::uint64_t>& overhead,
    std::uint64_t checksum,
    std::uint64_t active_100ns,
    std::uint64_t wall_qpc,
    std::uint64_t qpc_frequency,
    unsigned int selected_cpu,
    bool affinity_set,
    bool priority_set) {
    std::error_code error{};
    std::filesystem::create_directories(options.output_directory, error);
    if (error) return false;
    const std::filesystem::path raw_path =
        options.output_directory / "samples.csv";
    std::ofstream raw{raw_path, std::ios::binary | std::ios::trunc};
    if (!raw) return false;
    raw << "schema,label,revision,frame,thread_cycles,visible_signature,"
           "visible_density_signature,variant_output_signature,"
           "production_signature,background_tiles,props,monsters,equipment,"
           "materials,potions,production_population,"
           "production_visible_monsters,production_environment_candidates,"
           "production_drop_candidates,frame_checksum\n";
    std::vector<std::uint64_t> cycles{};
    cycles.reserve(samples.size());
    if (samples.empty()) return false;
    const PreparedFrame& visible = samples.front().frame;
    const std::uint64_t reference_signature = visible.visible_signature;
    const std::uint64_t reference_density_signature =
        visible.visible_density_signature;
    const std::uint64_t reference_variant_signature =
        visible.variant_output_signature;
    const std::uint64_t reference_production_signature =
        visible.production_signature;
    bool all_valid = true;
    for (std::size_t index{}; index < samples.size(); ++index) {
        const Sample& sample = samples[index];
        cycles.push_back(sample.cycles);
        all_valid = all_valid && sample.cycles != 0U && sample.frame.valid
            && sample.frame.visible_density_signature
                == density_signature(sample.frame)
            && sample.frame.visible_signature == reference_signature
            && sample.frame.visible_density_signature
                == reference_density_signature
            && sample.frame.variant_output_signature
                == reference_variant_signature
            && sample.frame.production_signature
                == reference_production_signature
            && sample.frame.production_signature != 0U
            && sample.frame.production_population != 0U;
#if ARPG_TASK11_CURRENT_RENDER_PREP
        all_valid = all_valid
            && sample.frame.production_population == 1125U
            && sample.frame.production_visible_monsters != 0U
            && sample.frame.production_environment_candidates != 0U
            && sample.frame.production_drop_candidates != 0U;
#endif
        raw << "2," << options.label << ',' << options.revision << ','
            << index << ',' << sample.cycles << ','
            << sample.frame.visible_signature << ','
            << sample.frame.visible_density_signature << ','
            << sample.frame.variant_output_signature << ','
            << sample.frame.production_signature << ','
            << sample.frame.background_tiles << ',' << sample.frame.props
            << ',' << sample.frame.monsters << ',' << sample.frame.equipment
            << ',' << sample.frame.materials << ',' << sample.frame.potions
            << ',' << sample.frame.production_population
            << ',' << sample.frame.production_visible_monsters
            << ',' << sample.frame.production_environment_candidates
            << ',' << sample.frame.production_drop_candidates
            << ',' << sample.frame.checksum << '\n';
    }
    raw.close();
    if (!raw) return false;

    const std::uint64_t p50 = percentile(cycles, 50U);
    const std::uint64_t p95 = percentile(cycles, 95U);
    const std::uint64_t p99 = percentile(cycles, 99U);
    const std::uint64_t overhead_p50 = percentile(overhead, 50U);
    const std::uint64_t overhead_p99 = percentile(overhead, 99U);
    const bool timer_resolution_valid = overhead_p50 != 0U
        && p50 >= overhead_p50 * 20U;
    const bool valid = all_valid && affinity_set && priority_set
        && timer_resolution_valid;

    const std::filesystem::path summary_path =
        options.output_directory / "summary.json";
    std::ofstream summary{summary_path, std::ios::binary | std::ios::trunc};
    if (!summary) return false;
    summary << "{\n"
        << "  \"schema\": 2,\n"
        << "  \"label\": \"" << options.label << "\",\n"
        << "  \"revision\": \"" << options.revision << "\",\n"
        << "  \"variant\": \""
#if ARPG_TASK11_CURRENT_RENDER_PREP
        << "current"
#else
        << "baseline"
#endif
        << "\",\n"
        << "  \"timer\": \"QueryThreadCycleTime\",\n"
        << "  \"frames\": " << samples.size() << ",\n"
        << "  \"warmup_frames\": " << options.warmup << ",\n"
        << "  \"viewport_width\": 1920,\n"
        << "  \"viewport_height\": 1080,\n"
        << "  \"visible_signature\": "
        << reference_signature << ",\n"
        << "  \"visible_density_signature\": "
        << reference_density_signature << ",\n"
        << "  \"variant_output_signature\": "
        << reference_variant_signature << ",\n"
        << "  \"production_signature\": "
        << reference_production_signature << ",\n"
        << "  \"production_path\": \""
#if ARPG_TASK11_CURRENT_RENDER_PREP
        << "DungeonSession::write_render_snapshot"
#else
        << "DungeonSession::snapshot"
#endif
        << "\",\n"
        << "  \"visible_monsters\": " << visible.monsters << ",\n"
        << "  \"visible_props\": " << visible.props << ",\n"
        << "  \"visible_equipment\": " << visible.equipment << ",\n"
        << "  \"visible_materials\": " << visible.materials << ",\n"
        << "  \"visible_potions\": " << visible.potions << ",\n"
        << "  \"production_population\": "
        << visible.production_population << ",\n"
        << "  \"production_visible_monsters\": "
        << visible.production_visible_monsters << ",\n"
        << "  \"production_environment_candidates\": "
        << visible.production_environment_candidates << ",\n"
        << "  \"production_drop_candidates\": "
        << visible.production_drop_candidates << ",\n"
        << "  \"thread_cycles_p50\": " << p50 << ",\n"
        << "  \"thread_cycles_p95\": " << p95 << ",\n"
        << "  \"thread_cycles_p99\": " << p99 << ",\n"
        << "  \"timer_overhead_p50\": " << overhead_p50 << ",\n"
        << "  \"timer_overhead_p99\": " << overhead_p99 << ",\n"
        << "  \"thread_active_100ns_total\": " << active_100ns << ",\n"
        << "  \"wall_qpc_ticks_total\": " << wall_qpc << ",\n"
        << "  \"qpc_frequency\": " << qpc_frequency << ",\n"
        << "  \"logical_cpu\": " << selected_cpu << ",\n"
        << "  \"affinity_set\": " << (affinity_set ? "true" : "false")
        << ",\n"
        << "  \"priority_set\": " << (priority_set ? "true" : "false")
        << ",\n"
        << "  \"timer_resolution_valid\": "
        << (timer_resolution_valid ? "true" : "false") << ",\n"
        << "  \"checksum\": " << checksum << ",\n"
        << "  \"raw_csv\": \"samples.csv\",\n"
        << "  \"valid\": " << (valid ? "true" : "false") << "\n"
        << "}\n";
    summary.close();
    return summary.good() && valid;
}

}  // namespace

int main(int argc, char** argv) {
    Options options{};
    if (!parse_options(argc, argv, options)) {
        std::fprintf(stderr,
            "usage: arpg_room_render_prep_probe --output PATH --label LABEL "
            "[--revision REV] [--frames N>=10000] [--warmup N] [--cpu 0..63]\n");
        return 2;
    }
    auto fixture = make_fixture();
    if (fixture == nullptr) return 3;

    DWORD_PTR process_mask{};
    DWORD_PTR system_mask{};
    if (!GetProcessAffinityMask(
            GetCurrentProcess(), &process_mask, &system_mask)) return 4;
    DWORD_PTR selected_mask = DWORD_PTR{1U} << options.cpu;
    if ((selected_mask & process_mask) == 0U) {
        selected_mask = process_mask & (~process_mask + 1U);
    }
    unsigned int selected_cpu{};
    while (((DWORD_PTR{1U} << selected_cpu) & selected_mask) == 0U
            && selected_cpu < 63U) {
        ++selected_cpu;
    }
    const DWORD_PTR previous_affinity =
        SetThreadAffinityMask(GetCurrentThread(), selected_mask);
    const bool affinity_set = previous_affinity != 0U;
    const int previous_priority = GetThreadPriority(GetCurrentThread());
    const bool priority_set = previous_priority != THREAD_PRIORITY_ERROR_RETURN
        && SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST) != 0;

    std::vector<std::uint64_t> overhead(kTimerCalibrationSamples);
    for (std::uint64_t& sample : overhead) {
        std::uint64_t begin{};
        std::uint64_t end{};
        if (!read_thread_cycles(begin) || !read_thread_cycles(end)
                || end < begin) {
            return 5;
        }
        sample = end - begin;
    }

    std::uint64_t checksum = 1469598103934665603ULL;
    for (std::size_t frame{}; frame < options.warmup; ++frame) {
        const PreparedFrame prepared = prepare_frame(*fixture, frame);
        if (!prepared.valid || !has_expected_visible_density(prepared)
                || prepared.visible_density_signature
                    != density_signature(prepared)
                || prepared.visible_signature == 0U
                || prepared.production_signature == 0U
#if ARPG_TASK11_CURRENT_RENDER_PREP
                || prepared.production_population != 1125U
#endif
                ) {
            return 6;
        }
        checksum = mix(checksum, prepared.checksum);
    }

    std::vector<Sample> samples(options.frames);
    FILETIME created{};
    FILETIME exited{};
    FILETIME kernel_before{};
    FILETIME user_before{};
    FILETIME kernel_after{};
    FILETIME user_after{};
    LARGE_INTEGER qpc_before{};
    LARGE_INTEGER qpc_after{};
    LARGE_INTEGER qpc_frequency{};
    if (!GetThreadTimes(GetCurrentThread(), &created, &exited,
            &kernel_before, &user_before)
            || !QueryPerformanceFrequency(&qpc_frequency)
            || !QueryPerformanceCounter(&qpc_before)) {
        return 7;
    }
    for (std::size_t frame{}; frame < samples.size(); ++frame) {
        std::uint64_t begin{};
        std::uint64_t end{};
        if (!read_thread_cycles(begin)) return 8;
        const PreparedFrame prepared = prepare_frame(*fixture, frame);
        if (!read_thread_cycles(end) || end <= begin) return 9;
        samples[frame] = {end - begin, prepared};
        checksum = mix(checksum, prepared.checksum);
    }
    if (!QueryPerformanceCounter(&qpc_after)
            || !GetThreadTimes(GetCurrentThread(), &created, &exited,
                &kernel_after, &user_after)) {
        return 10;
    }
    const std::uint64_t active_100ns =
        file_time_value(kernel_after) - file_time_value(kernel_before)
        + file_time_value(user_after) - file_time_value(user_before);
    const std::uint64_t wall_qpc = static_cast<std::uint64_t>(
        qpc_after.QuadPart - qpc_before.QuadPart);

    const bool written = write_outputs(options, samples, overhead, checksum,
        active_100ns, wall_qpc,
        static_cast<std::uint64_t>(qpc_frequency.QuadPart),
        selected_cpu, affinity_set, priority_set);
    if (priority_set) {
        static_cast<void>(SetThreadPriority(
            GetCurrentThread(), previous_priority));
    }
    if (affinity_set) {
        static_cast<void>(SetThreadAffinityMask(
            GetCurrentThread(), previous_affinity));
    }
    if (!written) return 11;
    std::printf("task11_render_prep label=%s revision=%s frames=%zu "
        "output=%s checksum=%llu\n", options.label.c_str(),
        options.revision.c_str(), options.frames,
        options.output_directory.string().c_str(),
        static_cast<unsigned long long>(checksum));
    return 0;
}
