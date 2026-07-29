#include "raylib_host.hpp"

#include "combat_feedback.hpp"
#include "combat_renderer.hpp"
#include "combat/active_skill_runtime.hpp"
#include "combat/fire_room_obstacle.hpp"
#include "combat/room_bounds.hpp"
#include "control_hints.hpp"
#include "death_overlay_font.hpp"
#include "core/fixed_step.hpp"
#include "dungeon_runtime.hpp"
#include "dungeon_view_math.hpp"
#include "game_audio.hpp"
#include "host_input.hpp"
#include "host_launch_options.hpp"
#include "host_validation_input.hpp"
#include "host_validation_navigation.hpp"
#include "host_validation_stage10_11.hpp"
#include "host_validation_stage11b.hpp"
#include "host_validation_stage11c.hpp"
#include "host_validation_stage11d.hpp"
#include "inventory_renderer.hpp"
#include "passive_tree_renderer.hpp"
#include "passive_tree_view_math.hpp"
#include "pause_menu_renderer.hpp"
#include "pause_menu_state.hpp"
#include "pause_menu_view.hpp"
#include "ui_material.hpp"
#include "ui_text_bounds_audit.hpp"
#include "persistence/save_paths.hpp"
#include "platform/settings/settings_store.hpp"
#include "platform/settings/settings_types.hpp"
#include "window_settings.hpp"

#include <raylib.h>

#include <algorithm>
#include <cstdint>
#include <cmath>
#include <cstddef>
#include <array>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <memory>
#include <new>
#include <limits>
#include <optional>
#include <string>
#include <utility>

#ifdef _WIN32
extern "C" __declspec(dllimport) int __stdcall SetForegroundWindow(void*);
#endif

#include "direct_input_poison.hpp"

static_assert(arpg::platform::direct_input_poison::active,
    "direct input poison must be active in raylib_host.cpp");

#if !defined(RAYLIB_VERSION_MAJOR) || !defined(RAYLIB_VERSION_MINOR) \
    || !defined(RAYLIB_VERSION_PATCH)
#error "raylib version macros are unavailable"
#endif

static_assert(RAYLIB_VERSION_MAJOR == 6, "raylib 6.0.0 is required");
static_assert(RAYLIB_VERSION_MINOR == 0, "raylib 6.0.0 is required");
static_assert(RAYLIB_VERSION_PATCH == 0, "raylib 6.0.0 is required");
static_assert(FLAG_VSYNC_HINT != 0, "raylib VSync flag must remain available");

namespace arpg::platform {
namespace {

using host_validation::inject_validation_action;
using host_validation::inject_validation_movement;
using host_validation::inject_validation_pressed;
using host_validation::inject_stage11d_physical_edges;
using host_validation::nearest_living_monster;
using host_validation::stage11d_has_three_ordinary_rarities;
using host_validation::stage11d_record_semantics;
using host_validation::stage11d_target_visible;
using host_validation::Stage11DLootValidationState;
using host_validation::validation_attack_lane;
using host_validation::validation_direction;
using host_validation::validation_door_position;
using host_validation::validation_exit_movement;
using host_validation::validation_movement_toward;
using host_validation::validation_route_fire_movement;
using host_validation::write_stage11d_loot_validation_summary;

constexpr char kSettingsPreviewFailed[] = "Live preview failed";
constexpr char kSettingsSaveFailed[] = "Settings save failed; retry";
constexpr char kSettingsRollbackFailed[] = "Settings rollback failed";
constexpr char kSettingsSaved[] = "Settings saved";
constexpr char kSettingsRecoveredDefaults[] = u8"设置已恢复默认值";

struct Stage17SkillStonesValidationState;
void observe_stage17_combat_event(
    Stage17SkillStonesValidationState* state,
    const combat::CombatEvent& event) noexcept;

[[nodiscard]] bool stable_pressed(
    const PhysicalKeySnapshot& snapshot,
    settings::StableKey key) noexcept {
    const std::size_t index = static_cast<std::size_t>(key);
    return index < snapshot.pressed.size() && snapshot.pressed[index];
}

[[nodiscard]] std::optional<settings::StableKey> captured_stable_key(
    const PhysicalKeySnapshot& snapshot) noexcept {
    for (std::size_t index = 0U; index < snapshot.pressed.size(); ++index) {
        if (snapshot.pressed[index]) {
            return static_cast<settings::StableKey>(index);
        }
    }
    return std::nullopt;
}

[[nodiscard]] PauseInput pause_input_from_snapshot(
    const PhysicalKeySnapshot& snapshot,
    bool escape_consumed,
    bool capture_binding) noexcept {
    PauseInput input{};
    input.escape = snapshot.escape && !escape_consumed;
    input.enter = snapshot.enter;
    input.up = stable_pressed(snapshot, settings::StableKey::arrow_up);
    input.down = stable_pressed(snapshot, settings::StableKey::arrow_down);
    input.left = stable_pressed(snapshot, settings::StableKey::arrow_left);
    input.right = stable_pressed(snapshot, settings::StableKey::arrow_right);
    input.activate = snapshot.mouse_left;
    input.focus_lost = snapshot.focus_lost;
    if (capture_binding) input.captured_key = captured_stable_key(snapshot);
    return input;
}

void drain_events(dungeon::DungeonSession& session, CombatRenderer& renderer,
    CombatFeedback& feedback, GameAudio& audio,
    Stage17SkillStonesValidationState* stage17 = nullptr) noexcept {
    while (const auto event = session.try_pop_combat_event()) {
        observe_stage17_combat_event(stage17, *event);
        renderer.consume_event(*event);
        feedback.consume(*event);
        audio.consume_event(*event);
    }
    while (const auto event = session.try_pop_event()) {
        renderer.consume_dungeon_event(*event);
        if (dungeon_event_clears_transients(event->kind)) {
            renderer.clear_combat_transients();
            feedback.clear();
            audio.clear_combat_transients();
        }
    }
}

[[nodiscard]] AudioBusLevels audio_bus_levels(
    const settings::SettingsData& value) noexcept {
    return {value.master_sfx_percent, value.sfx_percent, value.music_percent,
        value.ambience_percent, value.ui_percent};
}

[[nodiscard]] UiAudioCueMask pause_audio_cues(PauseScreen before,
    PauseScreen after, PauseCommand command) noexcept {
    UiAudioCueMask cues{};
    if (before == PauseScreen::closed && after != PauseScreen::closed) {
        cues = static_cast<UiAudioCueMask>(
            cues | ui_audio_cue_mask(UiAudioCue::open));
    } else if (before != PauseScreen::closed && after == PauseScreen::closed) {
        cues = static_cast<UiAudioCueMask>(
            cues | ui_audio_cue_mask(UiAudioCue::close));
    }
    switch (command) {
    case PauseCommand::preview:
        cues = static_cast<UiAudioCueMask>(
            cues | ui_audio_cue_mask(UiAudioCue::navigate));
        break;
    case PauseCommand::apply:
    case PauseCommand::quit:
        cues = static_cast<UiAudioCueMask>(
            cues | ui_audio_cue_mask(UiAudioCue::confirm));
        break;
    case PauseCommand::rollback:
        cues = static_cast<UiAudioCueMask>(
            cues | ui_audio_cue_mask(UiAudioCue::cancel));
        break;
    case PauseCommand::none:
    case PauseCommand::resume:
        break;
    }
    return cues;
}

void draw_recovery_screen(const DungeonRenderStatus& status) noexcept {
    BeginDrawing();
    ClearBackground(Color{21, 11, 27, 255});
    DrawText("SAVE RECOVERY REQUIRED", 48, 70, 32, Color{255, 169, 194, 255});
    DrawText("Both save slots are invalid. Press N to archive them and create a new run.",
        48, 122, 18, RAYWHITE);
    DrawText(TextFormat("Save error %u", static_cast<unsigned>(status.error)),
        48, 156, 16, Color{255, 202, 126, 255});
}

// Production screenshots intentionally read after EndDrawing presents the frame.
// With raylib 6 on Windows, reading the default framebuffer before that point
// produced black 1920x1080 captures despite correct render/framebuffer dimensions.
[[nodiscard]] bool present_frame_and_maybe_capture(const char* path) noexcept {
    EndDrawing();
    if (path == nullptr) return true;
    Image image = LoadImageFromScreen();
    if (image.data == nullptr) return false;
    const bool exported = ExportImage(image, path);
    UnloadImage(image);
    return exported;
}

void draw_stage12_ui_material_gallery(const MaterialPack& assets) noexcept {
    constexpr float kCell = 104.0F;
    constexpr float kGap = 8.0F;
    constexpr std::size_t kColumns = 8U;
    constexpr std::size_t kRows = 5U;
    const float width = kCell * static_cast<float>(kColumns)
        + kGap * static_cast<float>(kColumns - 1U);
    const float height = kCell * static_cast<float>(kRows)
        + kGap * static_cast<float>(kRows - 1U);
    const float left = (static_cast<float>(GetScreenWidth()) - width) * 0.5F;
    const float top = (static_cast<float>(GetScreenHeight()) - height) * 0.5F;
    DrawRectangleRounded({left - 16.0F, top - 16.0F,
        width + 32.0F, height + 32.0F}, 0.025F, 6,
        Color{3, 6, 11, 244});
    for (std::size_t index{}; index < kUiMaterialSprites.size(); ++index) {
        const std::size_t row = index / kColumns;
        const std::size_t column = index % kColumns;
        static_cast<void>(assets.draw_to(kUiMaterialSprites[index],
            {left + static_cast<float>(column) * (kCell + kGap),
             top + static_cast<float>(row) * (kCell + kGap), kCell, kCell}));
    }
}

std::optional<std::string> host_screenshot_path(
    const RaylibHostConfig& config) noexcept {
    try {
        const std::filesystem::path directory = config.screenshot_directory
            .value_or(std::filesystem::path{GetApplicationDirectory()});
        std::error_code error;
        std::filesystem::create_directories(directory, error);
        if (error) {
            TraceLog(LOG_WARNING, "failed to create screenshot directory");
            return std::nullopt;
        }
        return (directory / "stage8-equipment-loot.png").string();
    } catch (...) {
        TraceLog(LOG_WARNING, "failed to construct screenshot path");
        return std::nullopt;
    }
}

void apply_stage12_material_showcase(dungeon::DungeonSnapshot& snapshot,
    std::optional<dungeon::DungeonElement> ecology,
    bool hide_monsters, bool hide_items) noexcept {
    if (!snapshot.combat.has_value()) return;
    if (ecology.has_value()) snapshot.ecology = *ecology;
    if (hide_items) {
        snapshot.ground_item_count = 0U;
        snapshot.ground_material_count = 0U;
        snapshot.ground_health_potion_count = 0U;
    } else {
        constexpr std::array<items::ItemSlot, 6U> item_slots{{
            items::ItemSlot::weapon, items::ItemSlot::helmet,
            items::ItemSlot::chest, items::ItemSlot::gloves,
            items::ItemSlot::boots, items::ItemSlot::accessory,
        }};
        constexpr std::array<items::ItemRarity, 6U> item_rarities{{
            items::ItemRarity::rare, items::ItemRarity::magic,
            items::ItemRarity::normal, items::ItemRarity::normal,
            items::ItemRarity::magic, items::ItemRarity::rare,
        }};
        snapshot.ground_item_count = static_cast<std::uint16_t>(
            item_slots.size());
        for (std::size_t index{}; index < item_slots.size(); ++index) {
            dungeon::GroundItemSnapshot& item = snapshot.ground_items[index];
            item = {};
            item.ordinal = static_cast<std::uint16_t>(index + 1U);
            item.source = index == 3U
                ? dungeon::GroundItemSource::abyss_chest
                : dungeon::GroundItemSource::monster_drop;
            item.abyss_reward_ordinal = index == 3U ? 0U : 0xFFU;
            item.position = {-5.0F + static_cast<float>(index) * 2.0F,
                -3.2F, 0.0F};
            item.item_id = index + 1U;
            item.base_id = static_cast<std::uint8_t>(index + 1U);
            item.item_level = 60U;
            item.slot = item_slots[index];
            item.rarity = item_rarities[index];
        }
        snapshot.ground_material_count = static_cast<std::uint16_t>(
            items::kMaterialCount);
        for (std::size_t index{}; index < items::kMaterialCount; ++index) {
            dungeon::GroundMaterialSnapshot& material =
                snapshot.ground_materials[index];
            material = {};
            material.ordinal = static_cast<std::uint16_t>(index + 1U);
            material.source = items::material_is_coupon(
                    static_cast<items::MaterialId>(index))
                ? dungeon::GroundMaterialSource::monster_coupon
                : dungeon::GroundMaterialSource::monster_common;
            material.position = {
                -4.5F + static_cast<float>(index % 7U) * 1.5F,
                2.0F + static_cast<float>(index / 7U) * 1.6F, 0.0F};
            material.material = static_cast<items::MaterialId>(index);
        }
    }
    constexpr std::array<combat::MonsterId, 8> ids{{
        combat::MonsterId::fire_bomber, combat::MonsterId::fire_charger,
        combat::MonsterId::water_bulwark, combat::MonsterId::water_support,
        combat::MonsterId::lightning_shooter, combat::MonsterId::lightning_dasher,
        combat::MonsterId::chaos_chaser, combat::MonsterId::chaos_hazard,
    }};
    constexpr std::array<combat::Vec3, 8> positions{{
        {-4.0F, -2.0F, 0.0F}, {-1.3F, -2.0F, 0.0F}, {1.3F, -2.0F, 0.0F},
        {4.0F, -2.0F, 0.0F}, {-4.0F, 1.5F, 0.0F}, {-1.3F, 1.5F, 0.0F},
        {1.3F, 1.5F, 0.0F}, {4.0F, 1.5F, 0.0F},
    }};
    auto& combat_snapshot = *snapshot.combat;
    if (hide_monsters) {
        combat_snapshot.monster_count = 0U;
        snapshot.remaining_targets = 0U;
        for (combat::MonsterSnapshot& monster : combat_snapshot.monsters) {
            monster = {};
        }
        return;
    }
    combat_snapshot.monster_count = static_cast<std::uint16_t>(ids.size());
    snapshot.remaining_targets = static_cast<std::uint32_t>(ids.size());
    for (std::size_t index = 0U; index < ids.size(); ++index) {
        combat::MonsterSnapshot& monster = combat_snapshot.monsters[index];
        monster = {};
        monster.active = true;
        monster.generation = 1U;
        monster.monster_ordinal = static_cast<combat::MonsterOrdinal>(index);
        monster.id = ids[index];
        monster.position = positions[index];
        monster.spawn = positions[index];
        monster.facing = combat::Facing::right;
        monster.hp = 100;
        monster.max_hp = 100;
        monster.ai_phase = combat::MonsterAiPhase::active;
    }
}


struct HostValidationStates final {
    host_validation::Stage10ValidationState stage10{};
    host_validation::Stage11ValidationState stage11{};
    host_validation::Stage11BValidationState stage11b{};
    host_validation::Stage11CHudValidationState stage11c{};
    Stage11DLootValidationState stage11d{};
};

enum class Stage17ValidationStep : std::uint8_t {
    initial,
    empty_slot_3,
    empty_slot_4,
    empty_slot_5,
    approach_draw,
    draw_active,
    approach_storm,
    storm_active,
    open_inventory,
    open_skill_page,
    remove_slot_1,
    select_draw_inventory,
    equip_slot_5,
    swap_slots_2_5,
    close_inventory,
    cooldown_drain,
    restart_open_inventory,
    restart_open_skill_page,
    restart_capture,
    complete,
};

enum class Stage17Capture : std::uint8_t {
    none,
    initial,
    draw_windup,
    draw_hit,
    storm_array,
    storm_aerial,
    storm_finisher,
    restarted,
};

struct Stage17SkillDrawRuntimeEvidence final {
    std::uint32_t samples{};
    bool material_frame_drawn{};
    bool base_player_drawn{};
    std::size_t procedural_main_visual_peak{};
    bool valid{true};
};

struct Stage17SkillStonesValidationState final {
    Stage17ValidationStep step{Stage17ValidationStep::initial};
    Stage17Capture capture_pending{Stage17Capture::none};
    skills::SkillLoadoutState initial_loadout{};
    skills::SkillLoadoutState final_loadout{};
    combat::Vec3 storm_center{};
    combat::Vec3 storm_player_start{};
    combat::Vec3 storm_player_last{};
    combat::Vec3 storm_approach_center{};
    combat::Vec3 storm_approach_target_start{};
    combat::Vec3 storm_approach_player_start{};
    combat::Vec3 storm_approach_player_last{};
    std::array<combat::Vec3, 4U> storm_approach_samples{};
    std::array<bool, 4U> storm_approach_sampled{};
    std::array<std::uint16_t, combat::kStormStrikeCount>
        storm_targets_per_strike{};
    combat::MovementInput storm_threat_pull_movement{};
    std::uint16_t storm_approach_target_ordinal{
        (std::numeric_limits<std::uint16_t>::max)()};
    bool initial_recorded{};
    bool initial_captured{};
    bool draw_accepted{};
    bool draw_windup_captured{};
    bool draw_captured{};
    bool storm_accepted{};
    bool storm_approach_locked{};
    bool storm_isolation_invalidated{};
    bool storm_lock_recorded{};
    bool storm_threat_pull_locked{};
    bool storm_center_locked{true};
    bool storm_player_moved{};
    bool storm_array_captured{};
    bool storm_aerial_captured{};
    bool storm_finisher_captured{};
    bool restart_captured{};
    bool restart_persisted{};
    bool restart_cooldowns_zero{};
    bool public_input_path{};
    bool production_transactions{};
    bool production_inventory_closed{};
    bool production_cooldown_wait_started{};
    bool production_cooldowns_zero_before_shutdown{};
    bool clean_shutdown_exact_ready{};
    bool active_skill_atlases_ready{};
    bool storm_invulnerable_seen{};
    bool storm_finisher_phase_seen{};
    bool aborted_by_death{};
    bool renderer_status_failure_latched{};
    bool suspend_injection{};
    bool suppress_draw_captures{};
    Stage17SkillDrawRuntimeEvidence draw_runtime{};
    Stage17SkillDrawRuntimeEvidence storm_runtime{};
    std::array<bool, 3> empty_slots_none{};
    std::uint32_t draw_hit_count{};
    std::uint32_t storm_strike_hit_count{};
    std::uint32_t storm_finisher_hit_count{};
    std::uint8_t storm_strike_count{};
    std::uint8_t storm_sword_peak{};
    std::uint16_t production_cooldown_start_ticks{};
    std::uint64_t production_cooldown_wait_start_tick{};
    std::uint64_t production_cooldown_wait_end_tick{};
    std::uint8_t storm_sampled_strike_count{};
    std::uint16_t draw_frame_peak{};
    std::uint32_t presented_frames{};
    std::uint32_t storm_approach_frames{};
    std::uint32_t storm_approach_movement_injections{};
    std::uint32_t storm_isolation_invalidated_frame{};
    std::size_t storm_isolation_invalidated_target_count{};
    float storm_approach_initial_clearance{};
};

void observe_stage17_draw_runtime(const RaylibHostConfig& config,
    Stage17SkillStonesValidationState& state,
    const dungeon::DungeonSnapshot& presented,
    const ActiveSkillDrawRuntimeStatus& status) noexcept {
    if (config.stage17_skill_stones_validation
            == Stage17SkillStonesValidationScenario::none
        || config.stage17_skill_stones_validation
            == Stage17SkillStonesValidationScenario::restarted_loadout
        || !presented.combat.has_value()) {
        return;
    }
    const combat::ActiveSkillSnapshot& skill =
        presented.combat->active_skill;
    Stage17SkillDrawRuntimeEvidence* evidence = nullptr;
    if (skill.id == skills::ActiveSkillId::draw_slash) {
        evidence = &state.draw_runtime;
    } else if (skill.id == skills::ActiveSkillId::storm_swords) {
        evidence = &state.storm_runtime;
    } else {
        return;
    }
    ++evidence->samples;
    evidence->material_frame_drawn = evidence->material_frame_drawn
        || status.material_frame_drawn;
    evidence->base_player_drawn = evidence->base_player_drawn
        || status.base_player_drawn;
    evidence->procedural_main_visual_peak = (std::max)(
        evidence->procedural_main_visual_peak,
        status.procedural_main_visual_count);
    const bool sample_valid = status.mode == ActiveSkillVisualMode::material
        && status.atlas == active_skill_material_atlas(skill.id)
        && status.atlas_frame == active_skill_visual_frame_index(
            skill.id, skill.elapsed_ticks)
        && status.material_frame_drawn
        && !status.base_player_drawn
        && status.procedural_main_visual_count == 0U;
    evidence->valid = evidence->valid && sample_valid;
    state.renderer_status_failure_latched =
        state.renderer_status_failure_latched || !sample_valid;
}

[[nodiscard]] bool stage17_draw_runtime_valid(
    const Stage17SkillDrawRuntimeEvidence& evidence) noexcept {
    return evidence.samples > 0U && evidence.valid
        && evidence.material_frame_drawn && !evidence.base_player_drawn
        && evidence.procedural_main_visual_peak == 0U;
}

void observe_stage17_combat_event(
    Stage17SkillStonesValidationState* const state,
    const combat::CombatEvent& event) noexcept {
    if (state == nullptr || event.kind != combat::CombatEventKind::hit) return;
    if (event.skill == skills::ActiveSkillId::draw_slash) {
        ++state->draw_hit_count;
        if (!state->suppress_draw_captures && !state->draw_captured) {
            state->capture_pending = Stage17Capture::draw_hit;
        }
    } else if (event.skill == skills::ActiveSkillId::storm_swords) {
        if (event.finisher) {
            ++state->storm_finisher_hit_count;
            if (!state->storm_finisher_captured) {
                state->capture_pending = Stage17Capture::storm_finisher;
            }
        } else {
            ++state->storm_strike_hit_count;
            state->storm_strike_count = std::max(
                state->storm_strike_count, event.strike_index);
            if (!state->storm_array_captured) {
                state->capture_pending = Stage17Capture::storm_array;
            }
        }
    }
}


void stage17_press_action(PhysicalKeySnapshot& snapshot,
    const settings::SettingsData& input_settings,
    const settings::SettingAction action) noexcept {
    inject_validation_pressed(snapshot,
        settings::binding_for(input_settings, action));
}

void stage17_apply_movement(PhysicalKeySnapshot& snapshot,
    const settings::SettingsData& input_settings,
    const combat::MovementInput movement) noexcept {
    inject_validation_movement(snapshot, input_settings, movement);
}

struct Stage17IsolatedStormTarget final {
    const combat::MonsterSnapshot* monster{};
    combat::Vec3 projected_center{};
    float route_distance_squared{};
    float clearance_squared{};
};

[[nodiscard]] Stage17IsolatedStormTarget stage17_isolated_storm_target(
    const combat::CombatSnapshot& combat_state) noexcept {
    Stage17IsolatedStormTarget best{};
    combat::Vec3 projected_center = combat_state.player.position;
    projected_center.x += combat::kStormCenterForward;
    const float finisher_radius_squared =
        combat::kStormFinisherRadius * combat::kStormFinisherRadius;
    for (const combat::MonsterSnapshot& candidate : combat_state.monsters) {
        if (!candidate.active || candidate.hp <= 0
                || candidate.id != combat::MonsterId::fire_bomber) {
            continue;
        }
        const float route_x = candidate.position.x - projected_center.x;
        const float route_y = candidate.position.y - projected_center.y;
        const float route_distance_squared =
            route_x * route_x + route_y * route_y;
        if (route_distance_squared > finisher_radius_squared) continue;
        float clearance_squared = (std::numeric_limits<float>::max)();
        std::size_t targets_in_finisher = 0U;
        for (const combat::MonsterSnapshot& other : combat_state.monsters) {
            if (!other.active || other.hp <= 0) continue;
            const float x = other.position.x - projected_center.x;
            const float y = other.position.y - projected_center.y;
            const float distance_squared = x * x + y * y;
            targets_in_finisher += distance_squared <= finisher_radius_squared;
            if (other.monster_ordinal != candidate.monster_ordinal) {
                clearance_squared = (std::min)(
                    clearance_squared, distance_squared);
            }
        }
        if (targets_in_finisher != 1U) continue;
        if (best.monster == nullptr
                || clearance_squared > best.clearance_squared
                || (clearance_squared == best.clearance_squared
                    && route_distance_squared < best.route_distance_squared)) {
            best = {&candidate, projected_center, route_distance_squared,
                clearance_squared};
        }
    }
    return best;
}

[[nodiscard]] bool stage17_prepare_isolated_storm(
    PhysicalKeySnapshot& snapshot,
    const settings::SettingsData& input_settings,
    const combat::CombatSnapshot& combat_state,
    const bool fire_room,
    Stage17SkillStonesValidationState& state) noexcept {
    if (state.storm_isolation_invalidated) return false;
    ++state.storm_approach_frames;
    if (!state.storm_approach_locked) {
        const Stage17IsolatedStormTarget target =
            stage17_isolated_storm_target(combat_state);
        if (target.monster == nullptr) return false;
        state.storm_approach_center = target.projected_center;
        state.storm_approach_target_start = target.monster->position;
        state.storm_approach_player_start = combat_state.player.position;
        state.storm_approach_initial_clearance =
            std::sqrt(target.clearance_squared);
        state.storm_approach_target_ordinal = target.monster->monster_ordinal;
        state.storm_approach_locked = true;
    }
    state.storm_approach_player_last = combat_state.player.position;
    if (state.storm_approach_frames % 300U == 0U) {
        const std::size_t sample = state.storm_approach_frames / 300U - 1U;
        if (sample < state.storm_approach_samples.size()) {
            state.storm_approach_samples[sample] = combat_state.player.position;
            state.storm_approach_sampled[sample] = true;
        }
    }

    std::size_t targets_in_finisher = 0U;
    bool locked_target_in_finisher = false;
    const combat::MonsterSnapshot* locked_target = nullptr;
    combat::Vec3 projected_center = combat_state.player.position;
    projected_center.x += combat::kStormCenterForward;
    const float finisher_radius_squared =
        combat::kStormFinisherRadius * combat::kStormFinisherRadius;
    for (const combat::MonsterSnapshot& monster : combat_state.monsters) {
        if (!monster.active || monster.hp <= 0) continue;
        const float x = monster.position.x - projected_center.x;
        const float y = monster.position.y - projected_center.y;
        if (x * x + y * y > finisher_radius_squared) continue;
        ++targets_in_finisher;
        if (monster.monster_ordinal == state.storm_approach_target_ordinal) {
            locked_target_in_finisher = true;
            locked_target = &monster;
        }
    }
    if (targets_in_finisher != 1U || !locked_target_in_finisher) {
        state.storm_isolation_invalidated = true;
        state.storm_isolation_invalidated_frame = state.storm_approach_frames;
        state.storm_isolation_invalidated_target_count = targets_in_finisher;
        return false;
    }

    const combat::Vec3& player = combat_state.player.position;
    combat::MovementInput movement{};
    const float target_x = locked_target->position.x - projected_center.x;
    const float target_y = locked_target->position.y - projected_center.y;
    constexpr float kRouteTargetRadius = 2.65F;
    if (target_x * target_x + target_y * target_y
            > kRouteTargetRadius * kRouteTargetRadius) {
        movement.x = static_cast<std::int8_t>(target_x > 0.0F ? 1 : -1);
        movement.y = static_cast<std::int8_t>(target_y > 0.0F ? 1 : -1);
    } else if (combat_state.player.facing != combat::Facing::right) {
        movement.x = 1;
    }
    if (fire_room && (movement.x != 0 || movement.y != 0)) {
        movement = validation_route_fire_movement(
            player, locked_target->position, movement);
    }
    if (movement.x != 0 || movement.y != 0) {
        ++state.storm_approach_movement_injections;
        stage17_apply_movement(snapshot, input_settings, movement);
        return false;
    }

    return true;
}

[[nodiscard]] Vector2 stage17_center(const Rectangle rectangle) noexcept {
    return {rectangle.x + rectangle.width * 0.5F,
        rectangle.y + rectangle.height * 0.5F};
}

void stage17_click(PhysicalKeySnapshot& snapshot,
    const Rectangle rectangle) noexcept {
    snapshot.mouse_left = true;
    snapshot.mouse_position = stage17_center(rectangle);
}

[[nodiscard]] PhysicalKeySnapshot inject_stage17_physical_edges(
    PhysicalKeySnapshot snapshot, const RaylibHostConfig& config,
    const settings::SettingsData& input_settings,
    const dungeon::DungeonSnapshot& current,
    Stage17SkillStonesValidationState& state) noexcept {
    if (config.stage17_skill_stones_validation
            == Stage17SkillStonesValidationScenario::none
        || snapshot.focus_lost || state.suspend_injection) {
        return snapshot;
    }
    const ActiveSkillLoadoutLayout layout = active_skill_loadout_layout(
        config.window_width, config.window_height);
    switch (state.step) {
    case Stage17ValidationStep::initial:
    case Stage17ValidationStep::draw_active:
    case Stage17ValidationStep::complete:
    case Stage17ValidationStep::restart_capture:
    case Stage17ValidationStep::cooldown_drain:
        break;
    case Stage17ValidationStep::empty_slot_3:
        snapshot.active_skill_slots[2] = true;
        break;
    case Stage17ValidationStep::empty_slot_4:
        snapshot.active_skill_slots[3] = true;
        break;
    case Stage17ValidationStep::empty_slot_5:
        snapshot.active_skill_slots[4] = true;
        break;
    case Stage17ValidationStep::approach_draw:
        // Seed 753 starts with the locked two-target draw-slash geometry.
        // Exercise the production binding directly before AI movement changes it.
        snapshot.active_skill_slots[0] = true;
        break;
    case Stage17ValidationStep::approach_storm:
        if (current.combat.has_value()
                && stage17_prepare_isolated_storm(
                    snapshot, input_settings, *current.combat,
                    current.ecology
                        == dungeon::checkpoint::DungeonElement::fire,
                    state)) {
            snapshot.active_skill_slots[1] = true;
        }
        break;
    case Stage17ValidationStep::storm_active:
        if (state.storm_threat_pull_locked) {
            stage17_apply_movement(
                snapshot, input_settings, state.storm_threat_pull_movement);
        }
        break;
    case Stage17ValidationStep::open_inventory:
    case Stage17ValidationStep::restart_open_inventory:
        stage17_press_action(snapshot, input_settings,
            settings::SettingAction::inventory);
        break;
    case Stage17ValidationStep::open_skill_page:
        stage17_click(snapshot, layout.skill_stones_page_button);
        state.step = Stage17ValidationStep::remove_slot_1;
        break;
    case Stage17ValidationStep::remove_slot_1:
        stage17_click(snapshot, layout.remove_button);
        break;
    case Stage17ValidationStep::select_draw_inventory:
        stage17_click(snapshot, layout.inventory_slots[0]);
        state.step = Stage17ValidationStep::equip_slot_5;
        break;
    case Stage17ValidationStep::equip_slot_5:
        stage17_click(snapshot, layout.main_slots[4]);
        break;
    case Stage17ValidationStep::swap_slots_2_5:
        stage17_click(snapshot, layout.main_slots[1]);
        break;
    case Stage17ValidationStep::close_inventory:
        stage17_press_action(snapshot, input_settings,
            settings::SettingAction::inventory);
        break;
    case Stage17ValidationStep::restart_open_skill_page:
        stage17_click(snapshot, layout.skill_stones_page_button);
        state.step = Stage17ValidationStep::restart_capture;
        state.capture_pending = Stage17Capture::restarted;
        break;
    }
    return snapshot;
}

void observe_stage17_submitted_actions(
    const RaylibHostConfig& config,
    Stage17SkillStonesValidationState& state,
    const SubmittedFrameActions& submitted) noexcept {
    if (config.stage17_skill_stones_validation
            == Stage17SkillStonesValidationScenario::none) {
        return;
    }
    switch (state.step) {
    case Stage17ValidationStep::empty_slot_3:
    case Stage17ValidationStep::empty_slot_4:
    case Stage17ValidationStep::empty_slot_5: {
        const std::size_t index = state.step
                == Stage17ValidationStep::empty_slot_3 ? 2U
            : state.step == Stage17ValidationStep::empty_slot_4 ? 3U : 4U;
        state.empty_slots_none[index - 2U] =
            submitted.skills[index] == combat::SkillCastResult::none;
        state.public_input_path = true;
        state.step = index == 2U ? Stage17ValidationStep::empty_slot_4
            : index == 3U ? Stage17ValidationStep::empty_slot_5
                          : Stage17ValidationStep::approach_draw;
        break;
    }
    case Stage17ValidationStep::approach_draw:
        if (submitted.skills[0] == combat::SkillCastResult::accepted) {
            state.draw_accepted = true;
            state.public_input_path = true;
            state.step = Stage17ValidationStep::draw_active;
        }
        break;
    case Stage17ValidationStep::approach_storm:
        if (submitted.skills[1] == combat::SkillCastResult::accepted) {
            state.storm_accepted = true;
            state.public_input_path = true;
            state.step = Stage17ValidationStep::storm_active;
        }
        break;
    default:
        break;
    }
}

[[nodiscard]] bool stage17_same_point(
    combat::Vec3 left, combat::Vec3 right) noexcept {
    constexpr float kTolerance = 0.0001F;
    return std::fabs(left.x - right.x) <= kTolerance
        && std::fabs(left.y - right.y) <= kTolerance
        && std::fabs(left.z - right.z) <= kTolerance;
}

[[nodiscard]] bool stage17_all_cooldowns_zero(
    const combat::CombatSnapshot& combat_state) noexcept {
    return std::all_of(combat_state.skill_cooldowns.begin(),
        combat_state.skill_cooldowns.end(),
        [](const std::uint16_t ticks) noexcept { return ticks == 0U; });
}

[[nodiscard]] bool stage17_final_loadout(
    const skills::SkillLoadoutState& loadout) noexcept {
    return loadout.slots[0].active == skills::ActiveSkillId::none
        && loadout.slots[1].active == skills::ActiveSkillId::draw_slash
        && loadout.slots[2].active == skills::ActiveSkillId::none
        && loadout.slots[3].active == skills::ActiveSkillId::none
        && loadout.slots[4].active == skills::ActiveSkillId::storm_swords;
}

void observe_stage17_snapshot(const RaylibHostConfig& config,
    Stage17SkillStonesValidationState& state,
    const dungeon::DungeonSnapshot& current) noexcept {
    if (config.stage17_skill_stones_validation
            == Stage17SkillStonesValidationScenario::none) {
        return;
    }
    if (!state.initial_recorded) {
        state.initial_loadout = current.skill_loadout;
        state.initial_recorded = true;
        if (config.stage17_skill_stones_validation
                == Stage17SkillStonesValidationScenario::storm_sequence) {
            state.suppress_draw_captures = true;
            state.step = Stage17ValidationStep::approach_draw;
        } else if (config.stage17_skill_stones_validation
                == Stage17SkillStonesValidationScenario::restarted_loadout) {
            state.restart_persisted = stage17_final_loadout(current.skill_loadout);
            state.restart_cooldowns_zero = current.combat.has_value()
                && stage17_all_cooldowns_zero(*current.combat);
            state.step = Stage17ValidationStep::restart_open_inventory;
        }
    }
    if (current.death.has_value()) {
        state.aborted_by_death = true;
        return;
    }
    if (!current.combat.has_value()) return;
    const combat::CombatSnapshot& combat_state = *current.combat;
    if (state.step == Stage17ValidationStep::draw_active
            && combat_state.active_skill.id == skills::ActiveSkillId::draw_slash) {
        state.draw_frame_peak = std::max(state.draw_frame_peak,
            combat_state.active_skill.frame_index);
        if (config.stage17_skill_stones_validation
                != Stage17SkillStonesValidationScenario::storm_sequence
                && !state.draw_windup_captured
                && combat_state.active_skill.frame_index >= 6U
                && combat_state.active_skill.frame_index < 18U) {
            state.capture_pending = Stage17Capture::draw_windup;
        }
    } else if (state.step == Stage17ValidationStep::draw_active
            && combat_state.active_skill.id == skills::ActiveSkillId::none) {
        if (config.stage17_skill_stones_validation
                == Stage17SkillStonesValidationScenario::storm_sequence) {
            if (state.draw_accepted && state.draw_hit_count == 2U) {
                state.step = Stage17ValidationStep::approach_storm;
            }
        } else if (state.draw_windup_captured && state.draw_captured) {
            state.step = Stage17ValidationStep::open_inventory;
        }
    }
    if (state.step == Stage17ValidationStep::cooldown_drain
            && state.production_cooldown_wait_started
            && stage17_all_cooldowns_zero(combat_state)) {
        state.production_cooldown_wait_end_tick = combat_state.tick;
        state.production_cooldowns_zero_before_shutdown =
            state.production_cooldown_wait_end_tick
                > state.production_cooldown_wait_start_tick;
        state.step = Stage17ValidationStep::complete;
    }
    if (state.step != Stage17ValidationStep::storm_active) return;
    const combat::ActiveSkillSnapshot& skill = combat_state.active_skill;
    if (skill.id == skills::ActiveSkillId::storm_swords) {
        if (!state.storm_lock_recorded) {
            state.storm_lock_recorded = true;
            state.storm_center = skill.locked_center;
            state.storm_player_start = combat_state.player.position;
            state.storm_player_last = combat_state.player.position;
            const combat::MonsterSnapshot* threat = nullptr;
            float threat_distance_squared =
                (std::numeric_limits<float>::max)();
            for (const combat::MonsterSnapshot& monster :
                    combat_state.monsters) {
                if (!monster.active || monster.hp <= 0
                        || monster.monster_ordinal
                            == state.storm_approach_target_ordinal) {
                    continue;
                }
                const float x = monster.position.x - state.storm_center.x;
                const float y = monster.position.y - state.storm_center.y;
                const float distance_squared = x * x + y * y;
                if (threat == nullptr
                        || distance_squared < threat_distance_squared) {
                    threat = &monster;
                    threat_distance_squared = distance_squared;
                }
            }
            if (threat != nullptr) {
                const float x = threat->position.x - state.storm_center.x;
                const float y = threat->position.y - state.storm_center.y;
                state.storm_threat_pull_movement = {
                    static_cast<std::int8_t>(x > 0.0F ? 1
                        : (x < 0.0F ? -1 : 0)),
                    static_cast<std::int8_t>(y > 0.0F ? 1
                        : (y < 0.0F ? -1 : 0)),
                };
                state.storm_threat_pull_locked =
                    state.storm_threat_pull_movement.x != 0
                    || state.storm_threat_pull_movement.y != 0;
            }
        } else {
            state.storm_center_locked = state.storm_center_locked
                && stage17_same_point(state.storm_center, skill.locked_center);
            state.storm_player_moved = state.storm_player_moved
                || !stage17_same_point(
                    state.storm_player_start, combat_state.player.position);
        }
        state.storm_player_last = combat_state.player.position;
        state.storm_strike_count = std::max(
            state.storm_strike_count, skill.strike_index);
        state.storm_sword_peak = std::max(state.storm_sword_peak,
            skill.spawned_sword_count);
        if (skill.strike_index > state.storm_sampled_strike_count
                && skill.strike_index <= combat::kStormStrikeCount) {
            std::uint16_t targets = 0U;
            const float strike_radius_squared =
                combat::kStormStrikeRadius * combat::kStormStrikeRadius;
            for (const combat::MonsterSnapshot& monster :
                    combat_state.monsters) {
                if (!monster.active || monster.hp <= 0) continue;
                const float x = monster.position.x - skill.locked_center.x;
                const float y = monster.position.y - skill.locked_center.y;
                targets += x * x + y * y <= strike_radius_squared;
            }
            state.storm_targets_per_strike[skill.strike_index - 1U] = targets;
            state.storm_sampled_strike_count = skill.strike_index;
        }
        state.storm_invulnerable_seen = state.storm_invulnerable_seen
            || skill.player_invulnerable;
        if (skill.phase == combat::ActiveSkillPhase::strikes
                && !state.storm_array_captured) {
            state.capture_pending = Stage17Capture::storm_array;
        }
        if (skill.spawned_sword_count > 12U && !state.storm_aerial_captured) {
            state.capture_pending = Stage17Capture::storm_aerial;
        }
        if (skill.phase == combat::ActiveSkillPhase::finisher
                && !state.storm_finisher_captured) {
            state.capture_pending = Stage17Capture::storm_finisher;
        }
        state.storm_finisher_phase_seen = state.storm_finisher_phase_seen
            || skill.phase == combat::ActiveSkillPhase::finisher;
    } else if (state.storm_array_captured && state.storm_aerial_captured
            && state.storm_finisher_captured) {
        state.step = config.stage17_skill_stones_validation
                == Stage17SkillStonesValidationScenario::storm_sequence
            ? Stage17ValidationStep::complete
            : Stage17ValidationStep::open_inventory;
    }
}

void observe_stage17_inventory(const RaylibHostConfig& config,
    Stage17SkillStonesValidationState& state,
    const InventoryRenderer& inventory,
    const dungeon::DungeonSnapshot& current) noexcept {
    if (config.stage17_skill_stones_validation
            == Stage17SkillStonesValidationScenario::none) {
        return;
    }
    if (state.step == Stage17ValidationStep::open_inventory
            && inventory.is_open()) {
        state.step = Stage17ValidationStep::open_skill_page;
    } else if (state.step == Stage17ValidationStep::remove_slot_1
            && current.skill_loadout.slots[0].active
                == skills::ActiveSkillId::none) {
        state.step = Stage17ValidationStep::select_draw_inventory;
    } else if (state.step == Stage17ValidationStep::equip_slot_5
            && current.skill_loadout.slots[4].active
                == skills::ActiveSkillId::draw_slash) {
        state.step = Stage17ValidationStep::swap_slots_2_5;
    } else if (state.step == Stage17ValidationStep::swap_slots_2_5
            && stage17_final_loadout(current.skill_loadout)) {
        state.final_loadout = current.skill_loadout;
        state.production_transactions =
            !current.pending_save_kind.has_value();
        if (state.production_transactions) {
            state.step = Stage17ValidationStep::close_inventory;
        }
    } else if (state.step == Stage17ValidationStep::close_inventory
            && !inventory.is_open() && current.combat.has_value()) {
        state.production_inventory_closed = true;
        state.production_cooldown_start_ticks =
            current.combat->skill_cooldowns[0U];
        state.production_cooldown_wait_start_tick = current.combat->tick;
        state.production_cooldown_wait_started =
            state.production_cooldown_start_ticks != 0U;
        state.step = Stage17ValidationStep::cooldown_drain;
    } else if (state.step == Stage17ValidationStep::restart_open_inventory
            && inventory.is_open()) {
        state.step = Stage17ValidationStep::restart_open_skill_page;
    }
}

[[nodiscard]] const char* stage17_capture_name(
    const Stage17Capture capture) noexcept {
    switch (capture) {
    case Stage17Capture::initial: return "01-new-default-1280x720.png";
    case Stage17Capture::draw_windup: return "02-draw-slash-windup-1280x720.png";
    case Stage17Capture::draw_hit: return "03-draw-slash-hit-1280x720.png";
    case Stage17Capture::storm_array: return "04-storm-ground-array-1280x720.png";
    case Stage17Capture::storm_aerial: return "05-storm-aerial-array-1280x720.png";
    case Stage17Capture::storm_finisher: return "06-storm-finisher-1280x720.png";
    case Stage17Capture::restarted: return "07-restarted-loadout-1280x720.png";
    case Stage17Capture::none: break;
    }
    return nullptr;
}

[[nodiscard]] std::optional<std::string> stage17_capture_path(
    const RaylibHostConfig& config,
    Stage17SkillStonesValidationState& state) noexcept {
    if (config.stage17_skill_stones_validation
            == Stage17SkillStonesValidationScenario::none
        || !config.screenshot_directory.has_value()) {
        return std::nullopt;
    }
    ++state.presented_frames;
    if (state.step == Stage17ValidationStep::initial
            && !state.initial_captured && state.presented_frames >= 2U) {
        state.capture_pending = Stage17Capture::initial;
    }
    const char* const name = stage17_capture_name(state.capture_pending);
    if (name == nullptr) return std::nullopt;
    return (*config.screenshot_directory / name).string();
}

void mark_stage17_capture_complete(
    Stage17SkillStonesValidationState& state) noexcept {
    switch (state.capture_pending) {
    case Stage17Capture::initial:
        state.initial_captured = true;
        state.step = Stage17ValidationStep::empty_slot_3;
        break;
    case Stage17Capture::draw_windup:
        state.draw_windup_captured = true;
        break;
    case Stage17Capture::draw_hit:
        state.draw_captured = true;
        break;
    case Stage17Capture::storm_array:
        state.storm_array_captured = true;
        break;
    case Stage17Capture::storm_aerial:
        state.storm_aerial_captured = true;
        break;
    case Stage17Capture::storm_finisher:
        state.storm_finisher_captured = true;
        break;
    case Stage17Capture::restarted:
        state.restart_captured = true;
        state.step = Stage17ValidationStep::complete;
        break;
    case Stage17Capture::none:
        break;
    }
    state.capture_pending = Stage17Capture::none;
}

[[nodiscard]] const char* stage17_skill_name(
    const skills::ActiveSkillId id) noexcept {
    switch (id) {
    case skills::ActiveSkillId::draw_slash: return "draw_slash";
    case skills::ActiveSkillId::storm_swords: return "storm_swords";
    case skills::ActiveSkillId::none: return "none";
    case skills::ActiveSkillId::count: break;
    }
    return "invalid";
}

void write_stage17_loadout(std::ostream& stream,
    const skills::SkillLoadoutState& loadout) {
    for (std::size_t index = 0U; index < loadout.slots.size(); ++index) {
        if (index != 0U) stream << ',';
        stream << stage17_skill_name(loadout.slots[index].active);
    }
}

[[nodiscard]] std::size_t stage17_support_none_count(
    const skills::SkillLoadoutState& loadout) noexcept {
    std::size_t count{};
    for (const skills::ActiveSkillSlot& slot : loadout.slots) {
        count += static_cast<std::size_t>(std::count(slot.supports.begin(),
            slot.supports.end(), skills::SupportSkillId::none));
    }
    return count;
}

[[nodiscard]] bool stage17_validation_complete(
    const RaylibHostConfig& config,
    const Stage17SkillStonesValidationState& state) noexcept {
    return config.stage17_skill_stones_validation
            != Stage17SkillStonesValidationScenario::none
        && (state.step == Stage17ValidationStep::complete
            || state.aborted_by_death);
}

void write_stage17_validation_summary(const RaylibHostConfig& config,
    const Stage17SkillStonesValidationState& state) noexcept {
    if (!config.validation_summary_file.has_value()
        || config.stage17_skill_stones_validation
            == Stage17SkillStonesValidationScenario::none) {
        return;
    }
    try {
        std::ofstream stream(*config.validation_summary_file,
            std::ios::out | std::ios::trunc);
        if (!stream) return;
        const bool production = config.stage17_skill_stones_validation
            == Stage17SkillStonesValidationScenario::production_sequence;
        const bool storm = config.stage17_skill_stones_validation
            == Stage17SkillStonesValidationScenario::storm_sequence;
        const bool empty_slots = std::all_of(state.empty_slots_none.begin(),
            state.empty_slots_none.end(), [](const bool value) noexcept {
                return value;
            });
        const float approach_x = state.storm_approach_center.x
            - state.storm_approach_player_last.x;
        const float approach_y = state.storm_approach_center.y
            - state.storm_approach_player_last.y;
        const float approach_distance =
            std::sqrt(approach_x * approach_x + approach_y * approach_y);
        const float storm_player_x = state.storm_player_last.x
            - state.storm_player_start.x;
        const float storm_player_y = state.storm_player_last.y
            - state.storm_player_start.y;
        const float storm_player_displacement = std::sqrt(
            storm_player_x * storm_player_x + storm_player_y * storm_player_y);
        const bool passed = production
            ? state.step == Stage17ValidationStep::complete
                && state.initial_captured && state.draw_accepted
                && state.draw_windup_captured && state.draw_captured
                && stage17_draw_runtime_valid(state.draw_runtime)
                && !state.renderer_status_failure_latched
                && state.public_input_path
                && state.production_transactions
                && state.production_inventory_closed
                && state.production_cooldown_wait_started
                && state.production_cooldowns_zero_before_shutdown
                && state.clean_shutdown_exact_ready
                && empty_slots
            : storm
                ? state.step == Stage17ValidationStep::complete
                    && state.draw_accepted && state.draw_hit_count == 2U
                    && state.storm_accepted && state.storm_array_captured
                    && state.storm_aerial_captured
                    && state.storm_finisher_captured
                    && state.storm_center_locked && state.storm_player_moved
                    && !state.storm_isolation_invalidated
                    && state.active_skill_atlases_ready
                    && state.storm_invulnerable_seen
                    && state.storm_finisher_phase_seen
                    && stage17_draw_runtime_valid(state.storm_runtime)
                    && !state.renderer_status_failure_latched
                    && state.clean_shutdown_exact_ready
                    && state.public_input_path
                : state.step == Stage17ValidationStep::complete
                && state.restart_captured && state.restart_persisted
                && state.restart_cooldowns_zero
                && state.clean_shutdown_exact_ready;
        stream << "scenario=" << (production ? "production_sequence"
                : storm ? "storm_sequence" : "restarted_loadout") << '\n'
               << "result=" << (passed ? "pass" : "fail") << '\n'
               << "final_step=" << static_cast<unsigned>(state.step) << '\n'
               << "aborted_by_death=" << (state.aborted_by_death ? 1 : 0) << '\n'
               << "initial_slots=";
        write_stage17_loadout(stream, state.initial_loadout);
        stream << '\n' << "final_slots=";
        write_stage17_loadout(stream, state.final_loadout);
        stream << '\n'
               << "owned_active_bits=" << state.initial_loadout.owned_active_bits << '\n'
               << "support_none_count="
               << stage17_support_none_count(state.initial_loadout) << '\n'
               << "empty_slots_none=" << (empty_slots ? 1 : 0) << '\n'
               << "draw_accepted=" << (state.draw_accepted ? 1 : 0) << '\n'
               << "draw_windup_captured="
               << (state.draw_windup_captured ? 1 : 0) << '\n'
               << "draw_frame_peak=" << state.draw_frame_peak << '\n'
               << "draw_hit_count=" << state.draw_hit_count << '\n'
               << "storm_prelude_draw_hit_count="
               << (storm ? state.draw_hit_count : 0U) << '\n'
               << "storm_accepted=" << (state.storm_accepted ? 1 : 0) << '\n'
               << "storm_strike_hit_count=" << state.storm_strike_hit_count << '\n'
               << "storm_finisher_hit_count=" << state.storm_finisher_hit_count << '\n'
               << "storm_strike_count="
               << static_cast<unsigned>(state.storm_strike_count) << '\n'
               << "storm_sword_peak="
               << static_cast<unsigned>(state.storm_sword_peak) << '\n'
               << "storm_invulnerable_seen="
               << (state.storm_invulnerable_seen ? 1 : 0) << '\n'
               << "storm_finisher_phase_seen="
               << (state.storm_finisher_phase_seen ? 1 : 0) << '\n'
               << "storm_aerial_captured="
               << (state.storm_aerial_captured ? 1 : 0) << '\n'
               << "active_skill_atlases_ready="
               << (state.active_skill_atlases_ready ? 1 : 0) << '\n'
               << "draw_renderer_samples=" << state.draw_runtime.samples << '\n'
               << "draw_material_frame_drawn="
               << (state.draw_runtime.material_frame_drawn ? 1 : 0) << '\n'
               << "draw_base_player_drawn="
               << (state.draw_runtime.base_player_drawn ? 1 : 0) << '\n'
               << "draw_procedural_main_visual_peak="
               << state.draw_runtime.procedural_main_visual_peak << '\n'
               << "draw_renderer_status_valid="
               << (stage17_draw_runtime_valid(state.draw_runtime) ? 1 : 0)
               << '\n'
               << "storm_renderer_samples=" << state.storm_runtime.samples
               << '\n'
               << "storm_material_frame_drawn="
               << (state.storm_runtime.material_frame_drawn ? 1 : 0) << '\n'
               << "storm_base_player_drawn="
               << (state.storm_runtime.base_player_drawn ? 1 : 0) << '\n'
               << "storm_procedural_main_visual_peak="
               << state.storm_runtime.procedural_main_visual_peak << '\n'
               << "storm_renderer_status_valid="
               << (stage17_draw_runtime_valid(state.storm_runtime) ? 1 : 0)
               << '\n'
               << "renderer_status_failure_latched="
               << (state.renderer_status_failure_latched ? 1 : 0) << '\n'
               << "storm_center_locked=" << (state.storm_center_locked ? 1 : 0) << '\n'
               << "storm_isolation_invalidated="
               << (state.storm_isolation_invalidated ? 1 : 0) << '\n'
               << "storm_approach_center=" << state.storm_approach_center.x
               << ',' << state.storm_approach_center.y << '\n'
               << "storm_approach_target_start="
               << state.storm_approach_target_start.x << ','
               << state.storm_approach_target_start.y << '\n'
               << "storm_approach_player_start="
               << state.storm_approach_player_start.x << ','
               << state.storm_approach_player_start.y << '\n'
               << "storm_approach_player_last="
               << state.storm_approach_player_last.x << ','
               << state.storm_approach_player_last.y << '\n'
               << "storm_approach_distance=" << approach_distance << '\n'
               << "storm_approach_initial_clearance="
               << state.storm_approach_initial_clearance << '\n'
               << "storm_approach_frames=" << state.storm_approach_frames << '\n'
               << "storm_approach_movement_injections="
               << state.storm_approach_movement_injections << '\n'
               << "storm_isolation_invalidated_frame="
               << state.storm_isolation_invalidated_frame << '\n'
               << "storm_isolation_invalidated_target_count="
               << state.storm_isolation_invalidated_target_count << '\n'
               << "storm_player_moved=" << (state.storm_player_moved ? 1 : 0) << '\n'
               << "storm_player_displacement="
               << storm_player_displacement << '\n'
               << "storm_threat_pull_movement="
               << static_cast<int>(state.storm_threat_pull_movement.x) << ','
               << static_cast<int>(state.storm_threat_pull_movement.y) << '\n'
               << "public_input_path=" << (state.public_input_path ? 1 : 0) << '\n'
               << "production_transactions="
               << (state.production_transactions ? 1 : 0) << '\n'
               << "production_inventory_closed="
               << (state.production_inventory_closed ? 1 : 0) << '\n'
               << "production_cooldown_wait_started="
               << (state.production_cooldown_wait_started ? 1 : 0) << '\n'
               << "production_cooldown_start_ticks="
               << state.production_cooldown_start_ticks << '\n'
               << "production_cooldown_wait_ticks="
               << (state.production_cooldown_wait_end_tick
                       >= state.production_cooldown_wait_start_tick
                   ? state.production_cooldown_wait_end_tick
                       - state.production_cooldown_wait_start_tick
                   : 0U) << '\n'
               << "production_cooldowns_zero_before_shutdown="
               << (state.production_cooldowns_zero_before_shutdown ? 1 : 0)
               << '\n'
               << "clean_shutdown_exact_ready="
               << (state.clean_shutdown_exact_ready ? 1 : 0) << '\n'
               << "restart_persisted=" << (state.restart_persisted ? 1 : 0) << '\n'
               << "restart_cooldowns_zero="
               << (state.restart_cooldowns_zero ? 1 : 0) << '\n';
        for (std::size_t index = 0U;
                index < state.storm_approach_samples.size(); ++index) {
            stream << "storm_approach_sample_" << (index + 1U) * 300U << '=';
            if (state.storm_approach_sampled[index]) {
                stream << state.storm_approach_samples[index].x << ','
                       << state.storm_approach_samples[index].y;
            } else {
                stream << "none";
            }
            stream << '\n';
        }
        stream << "storm_targets_per_strike=";
        for (std::size_t index = 0U;
                index < state.storm_targets_per_strike.size(); ++index) {
            if (index != 0U) stream << ',';
            stream << state.storm_targets_per_strike[index];
        }
        stream << '\n';
    } catch (...) {
        TraceLog(LOG_WARNING, "failed to write stage17 validation summary");
    }
}




}  // namespace

HostFrameGateResult gate_host_frame(
    core::FixedStepRunner& fixed_step,
    bool& pause_latched,
    bool paused,
    double frame_seconds) noexcept {
    if (paused) {
        if (!pause_latched) fixed_step.clear_accumulator();
        pause_latched = true;
        return {};
    }
    pause_latched = false;
    return {true, fixed_step.advance(frame_seconds)};
}

dungeon::AutoPickupPolicy loot_pickup_policy(
    settings::LootFilterMode mode) noexcept {
    switch (mode) {
    case settings::LootFilterMode::show_all:
        return {items::ItemRarity::normal};
    case settings::LootFilterMode::magic_or_better:
        return {items::ItemRarity::magic};
    case settings::LootFilterMode::rare_only:
        return {items::ItemRarity::rare};
    }
    return {};
}

DeathInputGate host_death_input_gate(
    bool death_saving,
    bool death_pending,
    FrameKeyState keys,
    const PhysicalKeySnapshot& physical_keys) noexcept {
    keys.e = stable_pressed(physical_keys, settings::StableKey::e);
    return death_input_gate(death_saving, death_pending, keys);
}

HostSettingsNotice make_host_settings_notice(
    settings::SettingsLoadStatus status) noexcept {
    return {status == settings::SettingsLoadStatus::defaults_corrupt};
}

settings::LootFilterMode renderer_loot_filter_mode(
    PauseScreen screen,
    const settings::SettingsData& live_settings,
    const settings::SettingsData& draft_settings) noexcept {
    return screen == PauseScreen::settings
        ? draft_settings.loot_filter_mode
        : live_settings.loot_filter_mode;
}

void consume_host_settings_notice(
    HostSettingsNotice& notice,
    PauseScreen previous_screen,
    PauseMenuState& pause_menu) noexcept {
    if (!notice.recovered_defaults_pending
            || previous_screen != PauseScreen::root
            || pause_menu.screen != PauseScreen::settings) {
        return;
    }
    pause_menu.message = kSettingsRecoveredDefaults;
    notice.recovered_defaults_pending = false;
}

bool settle_host_pause_command(
    PauseCommand command,
    bool window_close_requested,
    PauseMenuState& pause_menu,
    settings::SettingsData& live_settings,
    settings::SettingsData& input_settings,
    const settings::SettingsStore& settings_store,
    WindowSettingsBackend settings_backend) {
    switch (command) {
    case PauseCommand::none:
        break;
    case PauseCommand::preview: {
        settings::SettingsData preview_settings = pause_menu.draft;
        preview_settings.loot_filter_mode =
            pause_menu.committed.loot_filter_mode;
        const LiveSettingsResult result = apply_live_settings(
            live_settings, preview_settings, settings_backend);
        if (result == LiveSettingsResult::applied) {
            live_settings = preview_settings;
            pause_menu.message = nullptr;
        } else {
            pause_menu.message = kSettingsPreviewFailed;
        }
        break;
    }
    case PauseCommand::apply: {
        settings::SettingsData preview_settings = pause_menu.draft;
        preview_settings.loot_filter_mode =
            pause_menu.committed.loot_filter_mode;
        const LiveSettingsResult preview = apply_live_settings(
            live_settings, preview_settings, settings_backend);
        if (preview != LiveSettingsResult::applied) {
            const LiveSettingsResult rollback = rollback_live_settings(
                live_settings, pause_menu.committed, settings_backend);
            live_settings.loot_filter_mode =
                pause_menu.committed.loot_filter_mode;
            pause_menu.draft.loot_filter_mode =
                pause_menu.committed.loot_filter_mode;
            if (rollback == LiveSettingsResult::applied) {
                live_settings = pause_menu.committed;
                pause_menu.message = kSettingsPreviewFailed;
            } else {
                pause_menu.message = kSettingsRollbackFailed;
            }
            break;
        }
        const settings::SettingsData previewed = preview_settings;
        live_settings = previewed;
        settings::SettingsData save_draft = pause_menu.draft;
        save_draft.revision = pause_menu.committed.revision;
        const settings::SettingsSaveResult saved = settings_store.save(
            pause_menu.committed, save_draft);
        if (saved.status == settings::SettingsSaveStatus::committed) {
            pause_menu.committed = saved.settings;
            pause_menu.draft = saved.settings;
            live_settings = saved.settings;
            input_settings = saved.settings;
            pause_menu.message = kSettingsSaved;
            break;
        }
        const LiveSettingsResult rollback = rollback_live_settings(
            previewed, pause_menu.committed, settings_backend);
        live_settings.loot_filter_mode =
            pause_menu.committed.loot_filter_mode;
        pause_menu.draft.loot_filter_mode =
            pause_menu.committed.loot_filter_mode;
        if (rollback == LiveSettingsResult::applied) {
            live_settings = pause_menu.committed;
            pause_menu.message = kSettingsSaveFailed;
        } else {
            pause_menu.message = kSettingsRollbackFailed;
        }
        break;
    }
    case PauseCommand::rollback: {
        const LiveSettingsResult result = rollback_live_settings(
            live_settings, pause_menu.committed, settings_backend);
        if (result == LiveSettingsResult::applied) {
            live_settings = pause_menu.committed;
        } else {
            pause_menu.message = kSettingsRollbackFailed;
        }
        break;
    }
    case PauseCommand::resume:
        break;
    case PauseCommand::quit:
        return true;
    }
    return window_close_requested;
}

RaylibHostConfig make_production_host_config(HostLaunchOptions options) {
    RaylibHostConfig config{};
    config.save_directory = std::move(options.save_directory);
    config.settings_directory = std::move(options.settings_directory);
    config.screenshot_directory = std::move(options.screenshot_directory);
    config.new_run_seed = options.new_run_seed;
    config.continue_pending_death_on_launch = true;
    return config;
}

HostExitCode run_raylib_host(const RaylibHostConfig& config) noexcept {
    bool window_ready = false;
    try {
        const auto save_directory = config.save_directory.has_value()
            ? config.save_directory : persistence::default_save_directory();
        if (!save_directory.has_value()) {
            return HostExitCode::save_initialization_failed;
        }
        const auto settings_directory = config.settings_directory.has_value()
            ? config.settings_directory : save_directory;
        if (!settings_directory.has_value()) {
            return HostExitCode::save_initialization_failed;
        }
        settings::SettingsStore settings_store(*settings_directory);
        const settings::SettingsLoadResult loaded = settings_store.load();
        HostSettingsNotice settings_notice =
            make_host_settings_notice(loaded.status);
        settings::SettingsData committed_settings = loaded.settings;
        if (settings::validate_settings(committed_settings)
                != settings::SettingsValidationError::none) {
            committed_settings = settings::default_settings();
        }
        DungeonRuntimeConfig runtime_config{};
        runtime_config.save.directory = *save_directory;
        runtime_config.new_run_seed = config.new_run_seed;
        runtime_config.continue_pending_death_on_initialize =
            config.continue_pending_death_on_launch;
        const auto runtime_storage =
            std::make_unique<DungeonRuntime>(runtime_config);
        DungeonRuntime& runtime = *runtime_storage;
        const bool initialized = runtime.initialize();
        if (!initialized && runtime.state() != DungeonRuntimeState::recovery_required) {
            return HostExitCode::save_initialization_failed;
        }
        if ((config.stage12_material_background_only
                || config.stage12_material_icons_only)
                && runtime.state() == DungeonRuntimeState::recovery_required) {
            TraceLog(LOG_ERROR,
                "Stage 12 background-only capture refused a recovery save");
            return HostExitCode::save_initialization_failed;
        }

        const std::unique_ptr<HostValidationStates> validation_states{
            new (std::nothrow) HostValidationStates{}};
        if (validation_states == nullptr) {
            return HostExitCode::save_initialization_failed;
        }
        SetConfigFlags(initial_window_flags(committed_settings));
        InitWindow(config.window_width, config.window_height,
            config.window_title);
        window_ready = IsWindowReady();
        if (!window_ready) {
            TraceLog(LOG_ERROR, "raylib window initialization failed");
            return HostExitCode::window_initialization_failed;
        }
        const bool directory_changed = config.validation_capture
            ? ChangeDirectory(save_directory->string().c_str())
            : ChangeDirectory(GetApplicationDirectory());
        if (!directory_changed) {
            TraceLog(LOG_WARNING, "failed to use screenshot working directory");
        }
        SetWindowMinSize(800, 450);
        SetExitKey(KEY_NULL);
#ifdef _WIN32
        if (config.stage11b_validation != Stage11BValidationScenario::none
                || config.stage11c_hud_validation
                    != Stage11CHudValidationScenario::none
// STAGE11D_LOOT_VALIDATION_SEAM_BEGIN foreground
                || config.stage11d_loot_validation
                    != Stage11DLootValidationScenario::none
// STAGE11D_LOOT_VALIDATION_SEAM_END foreground
                || config.stage17_skill_stones_validation
                    != Stage17SkillStonesValidationScenario::none
                ) {
            static_cast<void>(SetForegroundWindow(GetWindowHandle()));
        }
#endif
        core::FixedStepRunner fixed_step;
        const auto renderer_storage = std::make_unique<CombatRenderer>();
        CombatRenderer& renderer = *renderer_storage;
        const bool hud_resources_ready = renderer.initialize_resources();
        const auto pause_menu_renderer_storage =
            std::make_unique<PauseMenuRenderer>();
        PauseMenuRenderer& pause_menu_renderer = *pause_menu_renderer_storage;
        static_cast<void>(pause_menu_renderer.initialize());
        CombatFeedback feedback;
        const auto audio_storage = std::make_unique<GameAudio>();
        GameAudio& audio = *audio_storage;
        const auto inventory_storage = std::make_unique<InventoryRenderer>();
        InventoryRenderer& inventory = *inventory_storage;
        const bool audio_ready = audio.initialize();
        if (audio_ready) {
            SetMasterVolume(1.0F);
        }
        const WindowSettingsBackend settings_backend =
            raylib_window_settings_backend();
        PauseMenuState pause_menu{};
        pause_menu.committed = committed_settings;
        pause_menu.draft = committed_settings;
        settings::SettingsData live_settings = committed_settings;
        settings::SettingsData input_settings = committed_settings;
        ControlHints control_hints{};
        refresh_control_hints(control_hints, input_settings);
        bool pause_latched = false;
        bool draw_debug = false;
        bool passive_overlay_open = false;
        bool exit_requested = false;
        bool window_close_latched = false;
        const auto begin_clean_exit = [&]() noexcept {
            if (runtime.state() == DungeonRuntimeState::running
                    && runtime.session() != nullptr) {
                if (runtime.clean_shutdown_state()
                        == CleanShutdownState::ready) {
                    exit_requested = true;
                } else {
                    static_cast<void>(runtime.request_clean_shutdown());
                }
                return;
            }
            exit_requested = true;
        };
        std::uint32_t presented_frame_count = 0U;
        unsigned validation_capture_tick = 0U;
        unsigned validation_capture_count = 0U;
        host_validation::Stage10ValidationState& stage10_validation_state =
            validation_states->stage10;
        host_validation::Stage11ValidationState& stage11_validation_state =
            validation_states->stage11;
        host_validation::Stage11BValidationState& stage11b_validation_state =
            validation_states->stage11b;
        stage11b_validation_state.load_status = loaded.status;
        host_validation::Stage11CHudValidationState& stage11c_validation_state =
            validation_states->stage11c;
        stage11c_validation_state.cjk_font_ready = hud_resources_ready;
// STAGE11D_LOOT_VALIDATION_SEAM_BEGIN runtime_state
        Stage11DLootValidationState& stage11d_validation_state =
            validation_states->stage11d;
// STAGE11D_LOOT_VALIDATION_SEAM_END runtime_state
        const auto stage17_validation_state =
            std::make_unique<Stage17SkillStonesValidationState>();
        stage17_validation_state->active_skill_atlases_ready =
            renderer.active_skill_assets_ready();
        bool stage10_validation_captured = false;
        const std::string validation_capture_prefix = config.validation_capture
            ? (*save_directory / "stage8-validation-").string()
            : std::string{};
        const auto validation_capture_path = [&]() noexcept
                -> std::optional<std::string> {
            if (!config.validation_capture
                    || validation_capture_count >= 2000U
                    || ++validation_capture_tick < 60U) {
                return std::nullopt;
            }
            validation_capture_tick = 0U;
            ++validation_capture_count;
            return std::string{TextFormat("%s%03u.png",
                validation_capture_prefix.c_str(), validation_capture_count)};
        };
        const auto current_storage =
            std::make_unique<dungeon::DungeonSnapshot>();
        const auto previous_storage =
            std::make_unique<dungeon::DungeonSnapshot>();
        const auto presented_snapshot_storage =
            std::make_unique<dungeon::DungeonSnapshot>();
        dungeon::DungeonSnapshot& current = *current_storage;
        dungeon::DungeonSnapshot& previous = *previous_storage;
        dungeon::DungeonSnapshot& presented_snapshot =
            *presented_snapshot_storage;
        if (runtime.session() != nullptr) {
            runtime.session()->snapshot(current);
            previous = current;
            observe_stage17_snapshot(
                config, *stage17_validation_state, current);
            drain_events(*runtime.session(), renderer, feedback, audio,
                stage17_validation_state.get());
            if (config.stage12_ui_showcase == Stage12UiShowcase::inventory
                    || config.stage12_ui_showcase
                        == Stage12UiShowcase::skill_stones) {
                inventory.open(*runtime.session(), current);
                if (config.stage12_ui_showcase
                        == Stage12UiShowcase::skill_stones) {
                    inventory.show_skill_stones_page();
                }
            } else if (config.stage12_ui_showcase
                    == Stage12UiShowcase::pause) {
                pause_menu.screen = PauseScreen::root;
                pause_menu.selected_row = 1U;
            }
        }

        while (!exit_requested) {
            runtime.pump_persistence_frame();
            if (runtime.clean_shutdown_state()
                    == CleanShutdownState::canceled) {
                // PollInputEvents clears raylib's native GLFW close flag after
                // each close event. Re-arm only our edge latch here so a later
                // genuine close request can start a new exact-save attempt.
                window_close_latched = false;
            }
            if (runtime.clean_shutdown_state() == CleanShutdownState::ready
                    || runtime.clean_shutdown_state()
                        == CleanShutdownState::faulted) {
                exit_requested = true;
                continue;
            }
            if (runtime.session() != nullptr) {
                previous = current;
                runtime.session()->snapshot(current);
                drain_events(*runtime.session(), renderer, feedback, audio,
                    stage17_validation_state.get());
            }
            observe_stage17_snapshot(
                config, *stage17_validation_state, current);
            const bool window_close_requested = WindowShouldClose();
            if (!window_close_requested) window_close_latched = false;
            const PhysicalKeySnapshot sampled_physical_keys = sample_physical_keys();
            const PhysicalKeySnapshot stage11b_physical_keys =
                host_validation::inject_stage11b_physical_edges(
                sampled_physical_keys, config, stage11b_validation_state);
            const PhysicalKeySnapshot stage11c_physical_keys = host_validation::inject_stage11c_physical_edges(
                stage11b_physical_keys, config, input_settings, current,
                stage11c_validation_state);
// STAGE11D_LOOT_VALIDATION_SEAM_BEGIN runtime_input
            const PhysicalKeySnapshot physical_keys = inject_stage11d_physical_edges(
                stage11c_physical_keys, config, input_settings, current,
                stage11d_validation_state);
// STAGE11D_LOOT_VALIDATION_SEAM_END runtime_input
            const bool gameplay_rearm_was_required =
                runtime.gameplay_rearm_required();
            stage17_validation_state->suspend_injection =
                gameplay_rearm_was_required;
            const PhysicalKeySnapshot stage17_physical_keys =
                inject_stage17_physical_edges(physical_keys,
                    config, input_settings, current,
                    *stage17_validation_state);
            if (gameplay_rearm_was_required
                    && gameplay_controls_physically_released(
                        stage17_physical_keys)) {
                runtime.acknowledge_gameplay_rearmed();
            }
            HostFrameInput frame_input = map_host_frame_input(
                input_settings, stage17_physical_keys);
            if (recovery_requested(runtime.state() == DungeonRuntimeState::recovery_required,
                    frame_input.keys.recovery)) {
                if (runtime.recover_with_new_run() && runtime.session() != nullptr) {
                    runtime.session()->snapshot(current);
                    previous = current;
                    drain_events(*runtime.session(), renderer, feedback, audio,
                        stage17_validation_state.get());
                }
            }
            if (runtime.state() == DungeonRuntimeState::recovery_required) {
                if (config.stage12_material_background_only
                    || config.stage12_material_icons_only) {
                    TraceLog(LOG_ERROR,
                        "Stage 12 background-only capture entered recovery state");
                    exit_requested = true;
                    continue;
                }
                if (inventory.is_open()) {
                    inventory.close();
                    fixed_step.clear_accumulator();
                }
                if (frame_input.keys.escape
                        || (window_close_requested
                            && !window_close_latched)) {
                    if (window_close_requested) window_close_latched = true;
                    begin_clean_exit();
                    continue;
                }
                const float recovery_frame_seconds = GetFrameTime();
                audio.update({false, current.is_abyss, true,
                        current.death.has_value()},
                    audio_bus_levels(live_settings), 0U,
                    recovery_frame_seconds);
                // A recovery screen owns presentation, not simulation.  It still
                // receives exactly one HUD observation before its presented frame.
                renderer.observe_presented_hud_frame(HudPresentedFrame::recovery,
                    previous, current, runtime.render_status(), control_hints,
                    GetFrameTime(), true);
                draw_recovery_screen(runtime.render_status());
                std::optional<std::string> capture_path =
                    validation_capture_path();
                if (frame_input.keys.f12 || frame_input.keys.v) {
                    capture_path = host_screenshot_path(config);
                }
                static_cast<void>(present_frame_and_maybe_capture(
                    capture_path.has_value() ? capture_path->c_str() : nullptr));
                ++presented_frame_count;
                continue;
            }
            dungeon::DungeonSession* const session = runtime.session();
            if (session == nullptr) {
                break;
            }
            const bool inventory_open_before = inventory.is_open();
            const bool passive_open_before = passive_overlay_open;
            const std::uint32_t inventory_count_before = current.inventory_count;
            bool inventory_toggled_this_frame = false;
            if (runtime.state() != DungeonRuntimeState::running
                && inventory.is_open()) {
                inventory.close();
                fixed_step.clear_accumulator();
                inventory_toggled_this_frame = true;
            }
            const bool death_saving = current.death.has_value()
                && current.death->saving;
            const bool death_pending = current.death.has_value()
                && current.death->can_continue;
            const bool validation_continue = death_pending
                && (config.stage11_validation
                        == Stage11ValidationScenario::deep_continue
                    || config.stage11_validation
                        == Stage11ValidationScenario::floor_one_continue);
            DeathInputGate death_gate = host_death_input_gate(
                death_saving, death_pending, frame_input.keys, physical_keys);
            if (validation_continue && !death_saving
                    && !stage11_validation_state.continue_requested) {
                death_gate.continue_death = true;
            }
            if (!death_gate.forward_gameplay) {
                if (inventory.is_open()) inventory.close();
                passive_overlay_open = false;
                inventory_toggled_this_frame = false;
            }
            if (!passive_tree_can_open(current)) {
                passive_overlay_open = false;
            }
            if (death_gate.exit && !death_gate.forward_gameplay) {
                begin_clean_exit();
            }
            dungeon::RequestResult death_continue_result =
                dungeon::RequestResult::rejected;
            if (death_gate.continue_death) {
                death_continue_result = runtime.request_death_continue();
                if (death_continue_result
                        != dungeon::RequestResult::rejected) {
                    previous = current;
                    session->snapshot(current);
                }
            }
            if (validation_continue
                    && death_continue_result
                        != dungeon::RequestResult::rejected) {
                stage11_validation_state.continue_requested = true;
            }
            if (!death_gate.forward_gameplay && window_close_requested
                    && !window_close_latched) {
                window_close_latched = true;
                begin_clean_exit();
            }
            bool escape_consumed = false;
            if (death_gate.forward_gameplay && frame_input.keys.escape) {
                if (inventory.is_open()) {
                    inventory.close();
                    fixed_step.clear_accumulator();
                    inventory_toggled_this_frame = true;
                    escape_consumed = true;
                } else if (passive_overlay_open) {
                    passive_overlay_open = false;
                    escape_consumed = true;
                }
            } else if (death_gate.forward_gameplay
                    && pause_menu.screen == PauseScreen::closed
                    && frame_input.keys.inventory) {
                if (inventory.is_open()) {
                    inventory.close();
                    fixed_step.clear_accumulator();
                    inventory_toggled_this_frame = true;
                } else if (inventory_can_open(
                        runtime.state() == DungeonRuntimeState::running,
                        passive_overlay_open)) {
                    inventory.open(*session, current);
                    fixed_step.clear_accumulator();
                    previous = current;
                    inventory_toggled_this_frame = true;
                }
            }
            if (death_gate.forward_gameplay
                && pause_menu.screen == PauseScreen::closed
                && !inventory.is_open() && !inventory_toggled_this_frame
                && frame_input.keys.passives
                && passive_overlay_can_toggle(inventory.is_open())
                && passive_tree_can_open(current)) {
                passive_overlay_open = !passive_overlay_open;
            }
            if ((!inventory.is_open() || !death_gate.forward_gameplay)
                    && death_gate.debug_toggle) {
                draw_debug = !draw_debug;
                stage11c_validation_state.debug_visible = draw_debug;
            }
            if (death_gate.forward_gameplay
                && pause_menu.screen == PauseScreen::closed
                && inventory.is_open()
                && inventory.process_input(runtime, current, frame_input)) {
                session->snapshot(current);
                previous = current;
                drain_events(*session, renderer, feedback, audio,
                    stage17_validation_state.get());
                if (runtime.state() != DungeonRuntimeState::running) {
                    inventory.close();
                    fixed_step.clear_accumulator();
                    inventory_toggled_this_frame = true;
                }
            }
            observe_stage17_inventory(config, *stage17_validation_state,
                inventory, current);
            const PauseScreen pause_screen_before = pause_menu.screen;
            const bool pause_was_open =
                pause_screen_before != PauseScreen::closed;
            if (pause_was_open && physical_keys.mouse_left) {
                const PauseMenuLayout layout = pause_menu_layout(
                    GetScreenWidth(), GetScreenHeight());
                const auto selected = hit_test_pause_row(
                    layout, physical_keys.mouse_position);
                if (selected.has_value()) pause_menu.selected_row = *selected;
            }
            const PauseContext pause_context{
                death_saving || death_pending,
                false,
                inventory.is_open(),
                passive_overlay_open,
                current.pending_save_kind.has_value(),
                !physical_keys.focus_lost,
            };
            PauseInput pause_input = pause_input_from_snapshot(
                physical_keys, escape_consumed,
                pause_menu.screen == PauseScreen::capture_binding);
            const PauseCommand pause_command = update_pause_menu(
                pause_menu, pause_context, pause_input);
            UiAudioCueMask ui_audio_cues = pause_audio_cues(
                pause_screen_before, pause_menu.screen, pause_command);
            consume_host_settings_notice(
                settings_notice, pause_screen_before, pause_menu);
            const std::uint64_t input_revision_before = input_settings.revision;
            if (settle_host_pause_command(
                    pause_command, window_close_requested,
                    pause_menu, live_settings, input_settings,
                    settings_store, settings_backend)) {
                if (!window_close_requested || !window_close_latched) {
                    if (window_close_requested) window_close_latched = true;
                    begin_clean_exit();
                }
            }
            if (input_settings.revision != input_revision_before) {
                refresh_control_hints(control_hints, input_settings);
            }
            const bool pause_open = pause_menu.screen != PauseScreen::closed;
            const bool pause_blocks_gameplay = pause_open || pause_was_open;
            const bool gameplay_armed = runtime.authority_requests_enabled()
                && !gameplay_rearm_was_required;
            if (config.stage11b_validation
                    == Stage11BValidationScenario::paused_freeze
                && pause_was_open && !pause_open
                && stage11b_validation_state.resume_input_injected) {
                stage11b_validation_state.resume_observed = true;
                stage11b_validation_state.resume_ticks_before =
                    stage11b_validation_state.fixed_ticks;
            }
            const float frame_seconds = GetFrameTime();
            HostFrameGateResult host_gate{};
            if (pause_open) {
                host_gate = gate_host_frame(fixed_step, pause_latched, true,
                    static_cast<double>(frame_seconds));
                previous = current;
            } else if (!inventory.is_open()
                    && !inventory_toggled_this_frame) {
                host_gate = gate_host_frame(fixed_step, pause_latched, false,
                    static_cast<double>(frame_seconds));
            } else {
                previous = current;
            }
            const PassiveOverlayInputGate passive_input_gate = passive_overlay_input_gate(
                passive_overlay_open);
            const InventoryInputGate inventory_gate = inventory_input_gate(
                inventory.is_open() || inventory_toggled_this_frame);
            const bool forward_actions = passive_input_gate.forward_actions
                && inventory_gate.forward_actions
                && death_gate.forward_gameplay
                && host_gate.forward_gameplay && !pause_blocks_gameplay
                && gameplay_armed;
            const bool forward_movement = passive_input_gate.forward_movement
                && inventory_gate.forward_movement
                && death_gate.forward_gameplay
                && host_gate.forward_gameplay && !pause_blocks_gameplay
                && gameplay_armed;
            const bool forward_descent = passive_input_gate.forward_descent
                && inventory_gate.forward_descent
                && death_gate.forward_gameplay
                && host_gate.forward_gameplay && !pause_blocks_gameplay
                && gameplay_armed;
            if (forward_actions && inventory_gate.forward_room_reset
                && frame_input.keys.reset) {
                const dungeon::RequestResult reset =
                    session->reset_current_room();
                if (reset != dungeon::RequestResult::rejected) {
                    session->snapshot(current);
                    previous = current;
                    drain_events(*session, renderer, feedback, audio,
                        stage17_validation_state.get());
                }
            }
            if (gameplay_armed && !pause_blocks_gameplay
                    && death_gate.forward_gameplay
                    && passive_overlay_open
                    && frame_input.keys.mouse_gameplay) {
                const auto selected = hit_test_passive_node(
                    {frame_input.mouse_position.x, frame_input.mouse_position.y},
                    static_cast<float>(GetScreenWidth()), static_cast<float>(GetScreenHeight()));
                if (selected.has_value()) {
                    const bool allocated = (current.passive_tree.allocated_bits
                        & (1ULL << *selected)) != 0U;
                    static_cast<void>(allocated
                        ? session->request_passive_refund(*selected)
                        : session->request_passive_allocation(*selected));
                    session->snapshot(current);
                }
            }
            if (forward_actions) {
                const SubmittedFrameActions submitted_actions =
                    submit_frame_actions(*session, frame_input);
                observe_stage17_submitted_actions(
                    config, *stage17_validation_state, submitted_actions);
                if (config.stage11b_validation
                        == Stage11BValidationScenario::rebound_attack) {
                    if (stage11b_validation_state.injected_frame == 28U) {
                        stage11b_validation_state.old_attack_checked = true;
                        stage11b_validation_state.old_attack_count +=
                            submitted_actions.combat[0] ? 1U : 0U;
                    } else if (stage11b_validation_state.injected_frame == 29U) {
                        stage11b_validation_state.new_attack_count +=
                            submitted_actions.combat[0] ? 1U : 0U;
                    }
                }
            }
            if (forward_descent && frame_input.keys.e) {
                session->snapshot(current);
                const bool in_range = current.combat.has_value()
                    && can_prompt_descent(
                        current, current.combat->player.position);
                static_cast<void>(session->request_descent(in_range));
            }

            const combat::MovementInput movement = forward_movement
                ? frame_input.movement : combat::MovementInput{};
            feedback.update(frame_seconds);
            renderer.update(frame_seconds);
            core::FixedStepFrame frame = host_gate.fixed_step;
            if (host_gate.forward_gameplay) {
                if (config.stage10_validation
                            != Stage10ValidationScenario::none
                        || config.stage11_validation
                            != Stage11ValidationScenario::none
                        || config.stage11b_validation
                            != Stage11BValidationScenario::none
                        || config.stage11c_hud_validation
                            != Stage11CHudValidationScenario::none
// STAGE11D_LOOT_VALIDATION_SEAM_BEGIN fixed_step
                        || host_validation::stage11d_validation_active(config)
// STAGE11D_LOOT_VALIDATION_SEAM_END fixed_step
                        || config.stage17_skill_stones_validation
                            != Stage17SkillStonesValidationScenario::none
                        || (config.stage12_material_showcase
                            && config.validation_capture_file.has_value())
                        ) {
                    if (config.validation_steps_per_frame != 0U) {
                        frame.steps = config.validation_steps_per_frame;
                        frame.interpolation_alpha = 0.0;
                    }
                }
            }
            for (std::uint32_t step = 0; step < frame.steps; ++step) {
                previous = current;
                const bool step_death = current.death.has_value();
                combat::MovementInput step_movement{};
                if (!step_death) {
                    if (config.stage11_validation
                            != Stage11ValidationScenario::none) {
                        step_movement = host_validation::stage11_validation_input(*session,
                            current, config, stage11_validation_state);
                    } else if (config.stage10_validation
                            != Stage10ValidationScenario::none) {
                        step_movement = host_validation::stage10_validation_input(*session,
                            current, config, stage10_validation_state);
                    } else {
                        step_movement = movement;
                    }
                }
                runtime.fixed_tick(step_movement,
                    loot_pickup_policy(live_settings.loot_filter_mode));
                ++stage11b_validation_state.fixed_ticks;
                session->snapshot(current);
                observe_stage17_snapshot(
                    config, *stage17_validation_state, current);
// STAGE11D_LOOT_VALIDATION_SEAM_BEGIN abyss_claim
                host_validation::observe_stage11d_abyss_claim(
                    stage11d_validation_state, current, session->item_state());
// STAGE11D_LOOT_VALIDATION_SEAM_END abyss_claim
                if (current.death.has_value()) {
                    if (inventory.is_open()) inventory.close();
                    passive_overlay_open = false;
                }
                if (!passive_tree_can_open(current)) {
                    passive_overlay_open = false;
                }
                if (runtime.state() != DungeonRuntimeState::running
                    && inventory.is_open()) {
                    inventory.close();
                    fixed_step.clear_accumulator();
                }
                drain_events(*session, renderer, feedback, audio,
                    stage17_validation_state.get());
                if (host_validation::stage10_validation_reached(
                        current, config, stage10_validation_state)) {
                    break;
                }
                if (host_validation::stage11_validation_reached(
                        current, config, stage11_validation_state)) {
                    break;
                }
            }

            if (inventory.is_open() != inventory_open_before
                    || passive_overlay_open != passive_open_before) {
                const bool opened = inventory.is_open() || passive_overlay_open;
                ui_audio_cues = static_cast<UiAudioCueMask>(ui_audio_cues
                    | ui_audio_cue_mask(opened
                        ? UiAudioCue::open : UiAudioCue::close));
            }
            if (current.inventory_count > inventory_count_before) {
                ui_audio_cues = static_cast<UiAudioCueMask>(ui_audio_cues
                    | ui_audio_cue_mask(UiAudioCue::reward));
            }
            const settings::SettingsData& presented_audio_settings =
                pause_menu.screen == PauseScreen::settings
                    || pause_menu.screen == PauseScreen::capture_binding
                ? pause_menu.draft : live_settings;
            const bool combat_audio_active = current.combat.has_value()
                && (current.phase == dungeon::RoomPhase::combat
                    || current.phase == dungeon::RoomPhase::wave_delay)
                && current.remaining_targets != 0U;
            audio.update({combat_audio_active, current.is_abyss,
                    pause_blocks_gameplay || inventory.is_open()
                        || passive_overlay_open,
                    current.death.has_value()},
                audio_bus_levels(presented_audio_settings), ui_audio_cues,
                frame_seconds);

            if (config.stage11c_hud_validation
                    == Stage11CHudValidationScenario::low_health_status
                    && current.combat.has_value()) {
                current.combat->player.max_barrier = 1000;
                current.combat->player.barrier = 625;
            }

            const bool stage12_item_baseline_frame =
                config.stage12_material_baseline_capture_file.has_value()
                && config.validation_exit_after_presented_frames >= 2U
                && presented_frame_count + 2U
                    == config.validation_exit_after_presented_frames;
            presented_snapshot = current;
            if (config.stage12_material_showcase) {
                apply_stage12_material_showcase(presented_snapshot,
                    config.stage12_material_showcase_ecology,
                    config.stage12_material_showcase_hide_monsters,
                    config.stage12_material_showcase_hide_items
                        || stage12_item_baseline_frame);
            }
            const dungeon::DungeonSnapshot& presented_hud_previous =
                config.stage12_material_showcase ? presented_snapshot : previous;
            const dungeon::DungeonSnapshot& presented_hud_current =
                config.stage12_material_showcase ? presented_snapshot : current;

            // Observe after all possible fixed-step changes and before every
            // presented frame, including death/recovery-owned overlay frames.
            const HudPresentedFrame hud_presented_frame = current.death.has_value()
                ? HudPresentedFrame::death_overlay : HudPresentedFrame::normal;
            renderer.observe_presented_hud_frame(hud_presented_frame,
                presented_hud_previous, presented_hud_current,
                runtime.render_status(), control_hints,
                frame_seconds, pause_blocks_gameplay);
// STAGE11C_HUD_VALIDATION_SEAM_BEGIN observation
            const bool stage11c_target_visible = host_validation::stage11c_hud_validation_reached(
                current, config.stage11c_hud_validation,
                stage11c_validation_state, draw_debug);
            if (stage11c_target_visible) {
                ++stage11c_validation_state.target_presented_frames;
                stage11c_validation_state.production_snapshot_hash =
                    host_validation::stage11c_production_snapshot_hash(current);
                stage11c_validation_state.model = renderer.hud_model();
                stage11c_validation_state.notices = renderer.hud_notice_view();
                // Evidence records every formal HUD slot, including the F1 slot
                // while hidden; f1 remains the authoritative visibility flag.
                stage11c_validation_state.layout = make_hud_layout(
                    GetScreenWidth(), GetScreenHeight(), true);
                stage11c_validation_state.debug_visible = draw_debug;
            } else {
                stage11c_validation_state.target_presented_frames = 0U;
            }
// STAGE11C_HUD_VALIDATION_SEAM_END observation
            const settings::LootFilterMode presented_loot_filter =
                renderer_loot_filter_mode(
                    pause_menu.screen, live_settings, pause_menu.draft);
            renderer.set_loot_filter_mode(presented_loot_filter);
            BeginDrawing();
            ClearBackground(Color{13, 17, 27, 255});
            reset_ui_text_bounds_audit();
            const GroundLootView ground_loot_view = [&]() noexcept {
                if (config.stage12_material_background_only) {
                    static_cast<void>(renderer.draw_room_background_only(
                        presented_snapshot.ecology));
                    return GroundLootView{};
                }
                if (config.stage12_material_icons_only) {
                    return renderer.draw_ground_loot_icons_only(
                        presented_snapshot);
                }
                return renderer.draw(
                    previous, presented_snapshot, runtime.render_status(),
                    static_cast<float>(frame.interpolation_alpha), draw_debug,
                    feedback, audio_ready);
            }();
            observe_stage17_draw_runtime(config, *stage17_validation_state,
                presented_snapshot, renderer.active_skill_draw_status());
            if (config.stage12_material_runtime_status != nullptr) {
                const MonsterMaterialDrawRuntimeStatus shooter_draw =
                    renderer.monster_material_draw_status(
                        combat::MonsterId::lightning_shooter);
                const MonsterMaterialDrawRuntimeStatus dasher_draw =
                    renderer.monster_material_draw_status(
                        combat::MonsterId::lightning_dasher);
                const MonsterMaterialDrawRuntimeStatus chaser_draw =
                    renderer.monster_material_draw_status(
                        combat::MonsterId::chaos_chaser);
                const MonsterMaterialDrawRuntimeStatus hazard_draw =
                    renderer.monster_material_draw_status(
                        combat::MonsterId::chaos_hazard);
                *config.stage12_material_runtime_status = {
                    renderer.material_pipeline_ready(),
                    renderer.material_ecology_ready(MaterialEcology::water),
                    renderer.material_atlas_available(
                        MaterialAtlasId::water_environment),
                    renderer.material_atlas_available(
                        MaterialAtlasId::water_bulwark),
                    renderer.material_atlas_available(
                        MaterialAtlasId::water_support),
                    renderer.material_ecology_ready(MaterialEcology::lightning),
                    renderer.material_atlas_available(
                        MaterialAtlasId::lightning_environment),
                    renderer.material_atlas_available(
                        MaterialAtlasId::lightning_shooter),
                    renderer.material_atlas_available(
                        MaterialAtlasId::lightning_dasher),
                    {shooter_draw.presenter_visible,
                        shooter_draw.use_material_frame, shooter_draw.atlas,
                        shooter_draw.frame_index, shooter_draw.drawn},
                    {dasher_draw.presenter_visible,
                        dasher_draw.use_material_frame, dasher_draw.atlas,
                        dasher_draw.frame_index, dasher_draw.drawn},
                    renderer.material_ecology_ready(MaterialEcology::chaos),
                    renderer.material_atlas_available(
                        MaterialAtlasId::chaos_environment),
                    renderer.material_atlas_available(
                        MaterialAtlasId::chaos_chaser),
                    renderer.material_atlas_available(
                        MaterialAtlasId::chaos_hazard),
                    {chaser_draw.presenter_visible,
                        chaser_draw.use_material_frame, chaser_draw.atlas,
                        chaser_draw.frame_index, chaser_draw.drawn},
                    {hazard_draw.presenter_visible,
                        hazard_draw.use_material_frame, hazard_draw.atlas,
                        hazard_draw.frame_index, hazard_draw.drawn},
                };
                auto& material_status =
                    *config.stage12_material_runtime_status;
                material_status.hud_ecology = renderer.hud_model().navigation.ecology;
                const RoomBackgroundDrawRuntimeStatus background_draw =
                    renderer.room_background_draw_status();
                material_status.room_background_ecology = background_draw.ecology;
                material_status.room_background_atlas = background_draw.atlas;
                material_status.room_background_resident = background_draw.resident;
                material_status.room_background_drawn = background_draw.drawn;
                material_status.room_background_source_width =
                    background_draw.source_width;
                material_status.room_background_source_height =
                    background_draw.source_height;
                material_status.room_background_scale = background_draw.scale;
                material_status.items_ui_resident =
                    renderer.material_atlas_available(MaterialAtlasId::items_ui);
                constexpr std::array<MaterialSpriteId, 6U> equipment_sprites{{
                    MaterialSpriteId::item_weapon, MaterialSpriteId::item_helmet,
                    MaterialSpriteId::item_chest, MaterialSpriteId::item_gloves,
                    MaterialSpriteId::item_boots, MaterialSpriteId::item_accessory,
                }};
                constexpr std::array<MaterialSpriteId, 4U> rarity_sprites{{
                    MaterialSpriteId::loot_rarity_normal,
                    MaterialSpriteId::loot_rarity_magic,
                    MaterialSpriteId::loot_rarity_rare,
                    MaterialSpriteId::loot_rarity_abyss,
                }};
                for (std::size_t index{}; index < equipment_sprites.size(); ++index) {
                    material_status.equipment_slot_draws[index] =
                        renderer.material_sprite_draw_count(equipment_sprites[index]);
                }
                for (std::size_t index{}; index < rarity_sprites.size(); ++index) {
                    material_status.rarity_draws[index] =
                        renderer.material_sprite_draw_count(rarity_sprites[index]);
                }
                for (std::size_t index{}; index < items::kMaterialCount; ++index) {
                    material_status.material_draws[index] =
                        renderer.material_sprite_draw_count(material_loot_sprite(
                            static_cast<items::MaterialId>(index)));
                }
                material_status.ui_material_resident =
                    renderer.material_atlas_available(
                        MaterialAtlasId::ui_material);
                for (std::size_t index{}; index < kUiMaterialSprites.size();
                     ++index) {
                    material_status.ui_material_draws[index] =
                        renderer.material_sprite_draw_count(
                            kUiMaterialSprites[index]);
                    material_status.ui_direct_stretch_draws[index] =
                        renderer.material_direct_stretch_draw_count(
                            kUiMaterialSprites[index]);
                }
                material_status.bundled_font_ready =
                    renderer.hud_font_ready()
                    && pause_menu_renderer.has_cjk_font();
                material_status.bundled_font_glyph_count =
                    renderer.hud_font_ready()
                    ? static_cast<std::uint16_t>((std::min)(
                        renderer.hud_font().glyphCount, 65535))
                    : 0U;
                const Font runtime_font = renderer.hud_font();
                material_status.bundled_font_source_base_size =
                    renderer.hud_font_ready()
                    ? static_cast<std::uint16_t>((std::max)(
                        runtime_font.baseSize, 0))
                    : 0U;
                material_status.bundled_font_atlas_bytes =
                    renderer.hud_font_ready()
                    ? ui_font_atlas_bytes(runtime_font.texture.width,
                        runtime_font.texture.height)
                    : 0U;
                material_status.bundled_font_total_atlas_bytes =
                    material_status.bundled_font_atlas_bytes
                        * kUiFontAtlasInstanceCount;
                material_status.bundled_font_total_atlas_byte_budget =
                    kUiFontTotalAtlasByteBudget;
            }
// STAGE11D_LOOT_VALIDATION_SEAM_BEGIN presented_semantics
            stage11d_validation_state.target_visible = stage11d_target_visible(
                config, current, pause_menu, runtime.render_status(),
                presented_loot_filter, ground_loot_view,
                renderer.hud_notice_view(), stage11d_validation_state);
            if (stage11d_validation_state.target_visible
                    && !stage11d_validation_state.captured) {
                stage11d_record_semantics(stage11d_validation_state, current,
                    runtime.item_state(), ground_loot_view,
                    renderer.hud_notice_view());
            }
// STAGE11D_LOOT_VALIDATION_SEAM_END presented_semantics
            if (!config.stage12_material_background_only
                && !config.stage12_material_icons_only
                && passive_overlay_open) {
                draw_passive_tree_overlay(current, runtime.render_status());
            }
            if (!config.stage12_material_background_only
                && !config.stage12_material_icons_only
                && inventory.is_open()) {
                inventory.draw(*session, current, runtime.render_status(),
                    renderer.material_pack(), renderer.hud_font(),
                    renderer.hud_font_ready());
            }
            if (stage11b_validation_state.resume_observed) {
                stage11b_validation_state.resume_ticks_after =
                    stage11b_validation_state.fixed_ticks;
            }
            if (!config.stage12_material_background_only
                && !config.stage12_material_icons_only
                && pause_menu.screen != PauseScreen::closed) {
                pause_menu_renderer.draw(pause_menu, renderer.material_pack());
                if (config.stage11b_validation
                        == Stage11BValidationScenario::corrupt_defaults
                    && pause_menu.message == kSettingsRecoveredDefaults
                    && pause_menu_renderer.has_cjk_font()) {
                    stage11b_validation_state.recovery_notice_visible = true;
                }
            }
            if (config.stage11b_validation
                    == Stage11BValidationScenario::paused_freeze
                && pause_menu.screen != PauseScreen::closed) {
                if (stage11b_validation_state.paused_presented == 0U) {
                    stage11b_validation_state.paused_ticks_before =
                        stage11b_validation_state.fixed_ticks;
                    stage11b_validation_state.player_monster_hash_before =
                        host_validation::stage11b_snapshot_hash(current);
                }
                ++stage11b_validation_state.paused_presented;
                stage11b_validation_state.paused_ticks_after =
                    stage11b_validation_state.fixed_ticks;
                stage11b_validation_state.player_monster_hash_after =
                    host_validation::stage11b_snapshot_hash(current);
            }
            const bool stage10_target_visible = host_validation::stage10_validation_reached(
                current, config, stage10_validation_state);
            const bool stage11_target_visible = host_validation::stage11_validation_reached(
                current, config, stage11_validation_state);
            if (stage11_target_visible) {
                ++stage11_validation_state.target_presented_frames;
            } else {
                stage11_validation_state.target_presented_frames = 0U;
            }
            if (config.stage10_validation
                    == Stage10ValidationScenario::chaos_expansion
                    && stage10_target_visible) {
                ++stage10_validation_state.chaos_presented_frames;
            }
            const bool stage10_reached = stage10_target_visible
                && (config.stage10_validation
                        != Stage10ValidationScenario::chaos_expansion
                    || stage10_validation_state.chaos_presented_frames >= 16U);
            const bool stage11_reached = stage11_target_visible
                && stage11_validation_state.target_presented_frames >= 4U;
            const bool stage11b_reached = host_validation::stage11b_validation_complete(
                config, stage11b_validation_state, pause_menu);
// STAGE11C_HUD_VALIDATION_SEAM_BEGIN reached
            const bool stage11c_reached = stage11c_target_visible
                && stage11c_validation_state.target_presented_frames >= 4U;
// STAGE11C_HUD_VALIDATION_SEAM_END reached
// STAGE11D_LOOT_VALIDATION_SEAM_BEGIN reached
            const bool stage11d_reached = stage11d_validation_state.captured
                && (config.stage11d_loot_validation
                        != Stage11DLootValidationScenario::rare_only_abyss
                    || stage11d_validation_state.abyss_claimed);
// STAGE11D_LOOT_VALIDATION_SEAM_END reached
            const bool stage17_reached = stage17_validation_complete(
                config, *stage17_validation_state);
            const bool stage11b_visible_capture =
                (config.stage11b_validation == Stage11BValidationScenario::rebound_attack
                    || config.stage11b_validation == Stage11BValidationScenario::conflict_swap)
                && stage11b_validation_state.injected_frame == 21U;
            const bool stage11b_paused_visible_capture =
                config.stage11b_validation == Stage11BValidationScenario::paused_freeze
                && pause_menu.screen != PauseScreen::closed
                && stage11b_validation_state.paused_presented >= 120U
                && !stage11b_validation_state.pause_capture_while_paused;
            const bool prior_validation_reached = stage10_reached || stage11_reached
                || stage11b_reached || stage11c_reached;
// STAGE11D_LOOT_VALIDATION_SEAM_BEGIN reached_merge
            const bool validation_reached = prior_validation_reached
                || stage11d_reached || stage17_reached;
// STAGE11D_LOOT_VALIDATION_SEAM_END reached_merge
// STAGE11D_LOOT_VALIDATION_SEAM_BEGIN visible_capture
            const bool loot_validation_visible_capture =
                stage11d_validation_state.target_visible;
            const bool no_loot_validation_active =
                config.stage11d_loot_validation
                    == Stage11DLootValidationScenario::none;
// STAGE11D_LOOT_VALIDATION_SEAM_END visible_capture
            std::optional<std::string> capture_path = stage17_capture_path(
                config, *stage17_validation_state);
            const bool stage17_capture_requested = capture_path.has_value();
            bool captured_stage10_target = false;
            if (!capture_path.has_value() && stage12_item_baseline_frame) {
                capture_path =
                    config.stage12_material_baseline_capture_file->string();
            }
            if (config.stage12_material_runtime_status != nullptr) {
                const UiTextBoundsAuditStatus audit =
                    ui_text_bounds_audit_status();
                auto& material_status =
                    *config.stage12_material_runtime_status;
                for (std::size_t page{}; page < audit.pages.size(); ++page) {
                    material_status.ui_text_bounds_safe[page] =
                        audit.pages[page].bounds_safe;
                    material_status.ui_text_sizes_readable[page] =
                        audit.pages[page].sizes_readable;
                    material_status.ui_text_observed_roles[page] =
                        audit.pages[page].observed_roles;
                    material_status.ui_text_failed_bounds_roles[page] =
                        audit.pages[page].failed_bounds_roles;
                    material_status.ui_text_failed_size_roles[page] =
                        audit.pages[page].failed_size_roles;
                    material_status.ui_text_measured_counts[page] =
                        audit.pages[page].measured_text_count;
                    material_status.ui_text_minimum_display_sizes[page] =
                        audit.pages[page].minimum_display_font_size;
                }
            }
            if (!config.stage12_material_background_only
                && !config.stage12_material_icons_only
                && config.stage12_ui_showcase
                    == Stage12UiShowcase::material_gallery) {
                draw_stage12_ui_material_gallery(renderer.material_pack());
            }
            if (!capture_path.has_value()
                    && (validation_reached || loot_validation_visible_capture
                    || stage11b_visible_capture
                    || stage11b_paused_visible_capture)
                    && !stage10_validation_captured
                    && config.validation_capture_file.has_value()) {
                capture_path = config.validation_capture_file->string();
                captured_stage10_target = true;
                stage11b_validation_state.pause_capture_while_paused =
                    stage11b_paused_visible_capture;
            }
            if (death_gate.screenshot || (config.validation_request_screenshot
                    && presented_frame_count == 1U)) {
                capture_path = host_screenshot_path(config);
                captured_stage10_target = false;
            } else if (!capture_path.has_value()
                    && config.validation_capture_file.has_value()
                    && config.validation_exit_after_presented_frames != 0U
                    && config.stage10_validation == Stage10ValidationScenario::none
                    && config.stage11_validation == Stage11ValidationScenario::none
                    && config.stage11b_validation == Stage11BValidationScenario::none
                    && config.stage11c_hud_validation == Stage11CHudValidationScenario::none
                    && no_loot_validation_active
                    && presented_frame_count + 1U
                        >= config.validation_exit_after_presented_frames) {
                // Formal material validation captures an ordinary gameplay frame
                // at a caller-owned path without sharing the legacy F12 file.
                capture_path = config.validation_capture_file->string();
            } else if (!capture_path.has_value()) {
                capture_path = validation_capture_path();
            }
// STAGE11C_HUD_VALIDATION_SEAM_BEGIN presented_capture
            const bool capture_succeeded =
                present_frame_and_maybe_capture(capture_path.has_value()
                    ? capture_path->c_str() : nullptr);
            if (stage17_capture_requested && capture_succeeded) {
                mark_stage17_capture_complete(*stage17_validation_state);
            }
            ++presented_frame_count;
            const bool captured_stage10_frame = captured_stage10_target
                && capture_succeeded;
            if (captured_stage10_frame && stage11c_reached) {
                stage11c_validation_state.captured = true;
            }
// STAGE11C_HUD_VALIDATION_SEAM_END presented_capture
// STAGE11D_LOOT_VALIDATION_SEAM_BEGIN captured
            if (captured_stage10_frame
                    && stage11d_validation_state.target_visible) {
                stage11d_validation_state.captured = true;
            }
// STAGE11D_LOOT_VALIDATION_SEAM_END captured
            stage10_validation_captured = stage10_validation_captured
                || captured_stage10_frame;
            if (config.validation_exit_after_presented_frames != 0U
                    && presented_frame_count
                        >= config.validation_exit_after_presented_frames) {
                begin_clean_exit();
            }
            if (validation_reached
                    && (!config.validation_capture_file.has_value()
                        || stage10_validation_captured)) {
                begin_clean_exit();
            }
        }
        stage17_validation_state->clean_shutdown_exact_ready =
            runtime.clean_shutdown_state() == CleanShutdownState::ready;
        host_validation::write_stage11b_validation_summary(config, stage11b_validation_state,
            pause_menu);
        host_validation::write_stage11c_hud_validation_summary(config,
            stage11c_validation_state);
// STAGE11D_LOOT_VALIDATION_SEAM_BEGIN summary
        write_stage11d_loot_validation_summary(config,
            stage11d_validation_state, pause_menu);
// STAGE11D_LOOT_VALIDATION_SEAM_END summary
        write_stage17_validation_summary(config, *stage17_validation_state);
        audio.shutdown();
        renderer.shutdown_resources();
        pause_menu_renderer.shutdown();
        CloseWindow();
        return HostExitCode::success;
    } catch (const std::exception& exception) {
        TraceLog(LOG_ERROR, "raylib host failed: %s", exception.what());
        if (window_ready) {
            CloseWindow();
        }
        return HostExitCode::save_initialization_failed;
    } catch (...) {
        TraceLog(LOG_ERROR, "raylib host failed with an unknown exception");
        if (window_ready) {
            CloseWindow();
        }
        return HostExitCode::save_initialization_failed;
    }
}

}  // namespace arpg::platform
