#include "raylib_host.hpp"

#include "combat_feedback.hpp"
#include "combat_renderer.hpp"
#include "combat/monster_affix_generation.hpp"
#include "control_hints.hpp"
#include "core/fixed_step.hpp"
#include "dungeon_runtime.hpp"
#include "dungeon_view_math.hpp"
#include "game_audio.hpp"
#include "host_input.hpp"
#include "inventory_renderer.hpp"
#include "passive_tree_renderer.hpp"
#include "passive_tree_view_math.hpp"
#include "pause_menu_renderer.hpp"
#include "pause_menu_state.hpp"
#include "pause_menu_view.hpp"
#include "persistence/save_paths.hpp"
#include "platform/settings/settings_store.hpp"
#include "platform/settings/settings_types.hpp"
#include "stable_key_raylib.hpp"
#include "window_settings.hpp"

#include <raylib.h>

#include <cstdint>
#include <cmath>
#include <cstddef>
#include <array>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>

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

constexpr char kSettingsPreviewFailed[] = "Live preview failed";
constexpr char kSettingsSaveFailed[] = "Settings save failed; retry";
constexpr char kSettingsRollbackFailed[] = "Settings rollback failed";
constexpr char kSettingsSaved[] = "Settings saved";
constexpr char kSettingsRecoveredDefaults[] = u8"设置已恢复默认值";

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
    CombatFeedback& feedback, GameAudio& audio) noexcept {
    while (const auto event = session.try_pop_combat_event()) {
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

void present_frame_and_maybe_capture(const char* path) noexcept {
    EndDrawing();
    if (path == nullptr) return;
    Image image = LoadImageFromScreen();
    if (image.data == nullptr) return;
    static_cast<void>(ExportImage(image, path));
    UnloadImage(image);
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

void apply_stage12_material_showcase(dungeon::DungeonSnapshot& snapshot) noexcept {
    if (!snapshot.combat.has_value()) return;
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
    combat_snapshot.monster_count = ids.size();
    snapshot.remaining_targets = static_cast<std::uint8_t>(ids.size());
    for (std::size_t index = 0U; index < ids.size(); ++index) {
        combat::MonsterSnapshot& monster = combat_snapshot.monsters[index];
        monster = {};
        monster.active = true;
        monster.generation = 1U;
        monster.id = ids[index];
        monster.position = positions[index];
        monster.spawn = positions[index];
        monster.facing = combat::Facing::right;
        monster.hp = 100;
        monster.max_hp = 100;
        monster.ai_phase = combat::MonsterAiPhase::active;
    }
}

struct Stage10ValidationState final {
    bool entered_abyss{};
    bool reset_requested{};
    bool descent_warning_seen{};
    std::uint32_t chaos_presented_frames{};
};

struct Stage11ValidationState final {
    bool entered_abyss{};
    bool saw_depth_two{};
    bool continue_requested{};
    std::uint32_t target_presented_frames{};
};

struct Stage11BValidationState final {
    std::uint32_t injected_frame{};
    std::uint32_t paused_presented{};
    std::uint64_t fixed_ticks{};
    std::uint64_t paused_ticks_before{};
    std::uint64_t paused_ticks_after{};
    std::uint64_t resume_ticks_before{};
    std::uint64_t resume_ticks_after{};
    std::uint64_t player_monster_hash_before{};
    std::uint64_t player_monster_hash_after{};
    std::uint32_t old_attack_count{};
    std::uint32_t new_attack_count{};
    bool old_attack_checked{};
    bool pause_capture_while_paused{};
    bool resume_input_injected{};
    bool resume_observed{};
    bool recovery_notice_visible{};
    settings::SettingsLoadStatus load_status{
        settings::SettingsLoadStatus::defaults_missing};
};

struct Stage11CHudValidationState final {
    std::uint32_t injected_frames{};
    std::uint32_t target_presented_frames{};
    std::uint64_t production_snapshot_hash{};
    HudViewModel model{};
    HudNoticeView notices{};
    HudLayout layout{};
    bool cjk_font_ready{};
    bool debug_visible{};
    bool captured{};
};

// STAGE11D_LOOT_VALIDATION_SEAM_BEGIN state
struct Stage11DLootValidationState final {
    std::uint32_t injected_frames{};
    std::uint32_t target_presented_frames{};
    std::array<std::uint64_t, dungeon::kGroundDropCapacity> snapshot_item_ids{};
    std::array<std::uint64_t, dungeon::kGroundDropCapacity> inventory_item_ids{};
    GroundLootView view{};
    HudNoticeView notices{};
    std::uint16_t snapshot_item_count{};
    std::uint16_t inventory_item_count{};
    std::uint64_t normal_item_id{};
    std::uint64_t magic_item_id{};
    std::uint64_t rare_item_id{};
    std::uint64_t observed_normal_item_id{};
    std::uint64_t observed_magic_item_id{};
    std::uint64_t observed_rare_item_id{};
    std::uint16_t observed_normal_ordinal{0xFFFFU};
    std::uint16_t observed_magic_ordinal{0xFFFFU};
    std::uint16_t observed_rare_ordinal{0xFFFFU};
    std::uint64_t abyss_item_id{};
    items::ItemRarity abyss_rarity{items::ItemRarity::normal};
    HudRect pickup_notice_rect{};
    std::size_t preview_visible_count{};
    std::size_t restored_visible_count{};
    std::uint8_t preview_phase{};
    std::uint32_t ordinary_normal_count{};
    std::uint32_t ordinary_magic_count{};
    std::uint32_t ordinary_rare_count{};
    std::uint32_t max_ordinary_normal_count{};
    std::uint32_t max_ordinary_magic_count{};
    std::uint32_t max_ordinary_rare_count{};
    std::uint32_t max_ground_item_count{};
    std::uint32_t max_inventory_count{};
    std::array<std::int32_t, 3> last_monster_hp{};
    std::array<std::int32_t, 3> min_monster_hp{{
        0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF}};
    std::array<std::uint16_t, 3> monster_affix_danger{};
    std::array<std::uint8_t, 3> monster_ai_phase{};
    std::array<std::uint32_t, 3> defeat_distance_milli{};
    std::array<std::int32_t, 3> defeat_player_hp{};
    std::uint8_t seen_ordinal_bits{};
    std::uint8_t defeated_ordinal_bits{};
    std::uint16_t target_ordinal{0xFFFFU};
    std::uint8_t remaining_targets{};
    std::uint8_t min_remaining_targets{255U};
    std::uint32_t live_inventory_count{};
    dungeon::RoomPhase last_phase{dungeon::RoomPhase::locked};
    std::int32_t player_hp{};
    std::uint64_t pickup_item_id{};
    std::uint64_t pickup_commit_generation{};
    bool target_visible{};
    bool captured{};
    bool abyss_claim_requested{};
    bool abyss_claimed{};
};
// STAGE11D_LOOT_VALIDATION_SEAM_END state

// STAGE11D_LOOT_VALIDATION_SEAM_BEGIN selectors
[[nodiscard]] bool stage11d_has_three_ordinary_rarities(
    const dungeon::DungeonSnapshot& snapshot) noexcept {
    bool normal = false;
    bool magic = false;
    bool rare = false;
    for (std::size_t index = 0U; index < snapshot.ground_item_count; ++index) {
        const auto& item = snapshot.ground_items[index];
        if (item.source != dungeon::GroundItemSource::monster_drop) continue;
        normal = normal || item.rarity == items::ItemRarity::normal;
        magic = magic || item.rarity == items::ItemRarity::magic;
        rare = rare || item.rarity == items::ItemRarity::rare;
    }
    return normal && magic && rare;
}

[[nodiscard]] const dungeon::GroundItemSnapshot* stage11d_nearest_ground(
    const dungeon::DungeonSnapshot& snapshot) noexcept {
    if (!snapshot.combat.has_value()) return nullptr;
    const auto& player = snapshot.combat->player.position;
    const dungeon::GroundItemSnapshot* nearest = nullptr;
    float nearest_distance = 0.0F;
    for (std::size_t index = 0U; index < snapshot.ground_item_count; ++index) {
        const auto& item = snapshot.ground_items[index];
        const float x = item.position.x - player.x;
        const float y = item.position.y - player.y;
        const float distance = x * x + y * y;
        if (nearest == nullptr || distance < nearest_distance) {
            nearest = &item;
            nearest_distance = distance;
        }
    }
    return nearest;
}

[[nodiscard]] const combat::MonsterSnapshot* stage11d_priority_monster(
    const combat::CombatSnapshot& snapshot) noexcept {
    const combat::MonsterSnapshot* best = nullptr;
    for (std::size_t index = 0U; index < snapshot.monster_count; ++index) {
        const auto& monster = snapshot.monsters[index];
        if (!monster.active || monster.hp <= 0) continue;
        const bool bomber = monster.id == combat::MonsterId::fire_bomber;
        const bool best_bomber = best != nullptr
            && best->id == combat::MonsterId::fire_bomber;
        if (best == nullptr || (bomber && !best_bomber)
                || (bomber == best_bomber
                    && monster.spawn_ordinal < best->spawn_ordinal)) {
            best = &monster;
        }
    }
    return best;
}

[[nodiscard]] bool stage11d_attack_lane(
    const combat::CombatSnapshot& state,
    const combat::MonsterSnapshot& target,
    float minimum_x, float maximum_x) noexcept {
    const float x = target.position.x - state.player.position.x;
    const float y = target.position.y - state.player.position.y;
    const bool facing = std::fabs(x) <= 0.20F
        || (x > 0.0F && state.player.facing == combat::Facing::right)
        || (x < 0.0F && state.player.facing == combat::Facing::left);
    const float distance_x = std::fabs(x);
    return facing && distance_x >= minimum_x && distance_x <= maximum_x
        && std::fabs(y) <= 0.55F;
}
// STAGE11D_LOOT_VALIDATION_SEAM_END selectors

void inject_stage11b_pressed(PhysicalKeySnapshot& snapshot,
    settings::StableKey key) noexcept {
    const std::size_t index = static_cast<std::size_t>(key);
    if (index < snapshot.pressed.size()) {
        snapshot.pressed[index] = true;
        snapshot.down[index] = true;
    }
}

void inject_stage11b_open_settings(PhysicalKeySnapshot& snapshot,
    std::uint32_t frame) noexcept {
    if (frame == 1U) snapshot.escape = true;
    if (frame == 2U) inject_stage11b_pressed(snapshot, settings::StableKey::arrow_down);
    if (frame == 3U) snapshot.enter = true;
}

[[nodiscard]] PhysicalKeySnapshot inject_stage11b_physical_edges(
    PhysicalKeySnapshot snapshot, const RaylibHostConfig& config,
    Stage11BValidationState& state) noexcept {
    if (config.stage11b_validation == Stage11BValidationScenario::none) {
        return snapshot;
    }
    // Preserve every sampled production input.  A deterministic scenario only
    // adds physical key edges after the actual raylib window has focus.
    if (snapshot.focus_lost) return snapshot;
    const std::uint32_t frame = ++state.injected_frame;
    switch (config.stage11b_validation) {
    case Stage11BValidationScenario::paused_freeze:
        if (frame == 2U || (state.paused_presented >= 120U
                && !state.resume_input_injected)) {
            snapshot.escape = true;
            state.resume_input_injected = frame != 2U;
        }
        break;
    case Stage11BValidationScenario::settings_page:
    case Stage11BValidationScenario::restarted_settings:
    case Stage11BValidationScenario::single_slot_recovery:
    case Stage11BValidationScenario::corrupt_defaults:
        inject_stage11b_open_settings(snapshot, frame);
        break;
    case Stage11BValidationScenario::rebound_attack:
    case Stage11BValidationScenario::conflict_swap:
        if (frame == 1U) snapshot.escape = true;
        else if (frame == 2U || (frame >= 4U && frame <= 15U)
            || (frame >= 18U && frame <= 24U)) {
            inject_stage11b_pressed(snapshot, settings::StableKey::arrow_down);
        } else if (frame == 3U || frame == 16U || frame == 25U) {
            snapshot.enter = true;
        } else if (frame == 17U) {
            inject_stage11b_pressed(snapshot,
                config.stage11b_validation == Stage11BValidationScenario::rebound_attack
                    ? settings::StableKey::u : settings::StableKey::k);
        } else if (config.stage11b_validation
                       == Stage11BValidationScenario::rebound_attack
                   && (frame == 26U || frame == 27U)) {
            snapshot.escape = true;
        } else if (config.stage11b_validation
                       == Stage11BValidationScenario::rebound_attack
                   && frame == 28U) {
            inject_stage11b_pressed(snapshot, settings::StableKey::j);
        } else if (config.stage11b_validation
                       == Stage11BValidationScenario::rebound_attack
                   && frame == 29U) {
            inject_stage11b_pressed(snapshot, settings::StableKey::u);
        }
        break;
    case Stage11BValidationScenario::none:
        break;
    }
    return snapshot;
}

[[nodiscard]] bool stage11b_validation_complete(
    const RaylibHostConfig& config, const Stage11BValidationState& state,
    const PauseMenuState& pause_menu) noexcept {
    switch (config.stage11b_validation) {
    case Stage11BValidationScenario::none: return false;
    case Stage11BValidationScenario::paused_freeze:
        return state.paused_presented >= 120U && state.pause_capture_while_paused
            && state.resume_observed
            && state.resume_ticks_after == state.resume_ticks_before + 1U;
    case Stage11BValidationScenario::settings_page:
    case Stage11BValidationScenario::restarted_settings:
    case Stage11BValidationScenario::single_slot_recovery:
    case Stage11BValidationScenario::corrupt_defaults:
        return state.injected_frame >= 4U
            && pause_menu.screen == PauseScreen::settings;
    case Stage11BValidationScenario::rebound_attack:
        return state.injected_frame >= 29U && state.old_attack_checked
            && state.new_attack_count != 0U
            && pause_menu.screen == PauseScreen::closed;
    case Stage11BValidationScenario::conflict_swap:
        return state.injected_frame >= 25U
            && pause_menu.screen == PauseScreen::settings;
    }
    return false;
}

[[nodiscard]] std::uint64_t stage11b_snapshot_hash(
    const dungeon::DungeonSnapshot& snapshot) noexcept {
    std::uint64_t hash = 1469598103934665603ULL;
    const auto mix = [&hash](std::uint64_t value) noexcept {
        hash ^= value;
        hash *= 1099511628211ULL;
    };
    mix(snapshot.depth);
    mix(snapshot.room_index);
    if (snapshot.combat.has_value()) {
        const combat::CombatSnapshot& combat = *snapshot.combat;
        mix(static_cast<std::uint64_t>(combat.player.hp));
        for (const combat::MonsterSnapshot& monster : combat.monsters) {
            mix(static_cast<std::uint64_t>(monster.hp));
            mix(monster.active ? 1U : 0U);
        }
    }
    return hash;
}

void write_stage11b_validation_summary(const RaylibHostConfig& config,
    const Stage11BValidationState& state,
    const PauseMenuState& pause_menu) noexcept {
    if (!config.validation_summary_file.has_value()
        || config.stage11b_validation == Stage11BValidationScenario::none) {
        return;
    }
    try {
        std::ofstream stream(*config.validation_summary_file,
            std::ios::out | std::ios::trunc);
        if (!stream) return;
        const auto light = settings::binding_for(pause_menu.committed,
            settings::SettingAction::light_attack);
        const auto jump = settings::binding_for(pause_menu.committed,
            settings::SettingAction::jump);
        const auto draft_light = settings::binding_for(pause_menu.draft,
            settings::SettingAction::light_attack);
        stream << "scenario=" << static_cast<unsigned>(config.stage11b_validation) << '\n'
               << "injected_frame=" << state.injected_frame << '\n'
               << "paused_tick_before=" << state.paused_ticks_before << '\n'
               << "paused_tick_after=" << state.paused_ticks_after << '\n'
               << "pause_capture_while_paused="
               << (state.pause_capture_while_paused ? 1 : 0) << '\n'
               << "resume_tick_before=" << state.resume_ticks_before << '\n'
               << "resume_tick_after=" << state.resume_ticks_after << '\n'
               << "player_monster_hash_before=" << state.player_monster_hash_before << '\n'
               << "player_monster_hash_after=" << state.player_monster_hash_after << '\n'
               << "committed_revision=" << pause_menu.committed.revision << '\n'
               << "old_attack_count=" << state.old_attack_count << '\n'
               << "new_attack_count=" << state.new_attack_count << '\n'
               << "recovery_notice_visible="
               << (state.recovery_notice_visible ? 1 : 0) << '\n'
               << "pause_screen=" << static_cast<unsigned>(pause_menu.screen) << '\n'
               << "pause_row=" << pause_menu.selected_row << '\n'
               << "light_attack=" << stable_key_label(light) << '\n'
               << "draft_light_attack=" << stable_key_label(draft_light) << '\n'
               << "jump=" << stable_key_label(jump) << '\n'
               << "load_status=" << static_cast<unsigned>(state.load_status) << '\n';
    } catch (...) {
        TraceLog(LOG_WARNING, "failed to write stage11b validation summary");
    }
}

const combat::MonsterSnapshot* nearest_living_monster(
    const combat::CombatSnapshot& state) noexcept {
    const combat::MonsterSnapshot* best = nullptr;
    float best_distance = 0.0F;
    for (const combat::MonsterSnapshot& monster : state.monsters) {
        if (!monster.active || monster.hp <= 0) continue;
        const float x = monster.position.x - state.player.position.x;
        const float y = monster.position.y - state.player.position.y;
        const float distance = x * x + y * y;
        if (best == nullptr || distance < best_distance) {
            best = &monster;
            best_distance = distance;
        }
    }
    return best;
}

combat::MovementInput validation_movement_toward(
    combat::Vec3 from, combat::Vec3 to) noexcept {
    combat::MovementInput movement{};
    if (to.x - from.x > 0.45F) movement.x = 1;
    else if (to.x - from.x < -0.45F) movement.x = -1;
    if (to.y - from.y > 0.25F) movement.y = 1;
    else if (to.y - from.y < -0.25F) movement.y = -1;
    return movement;
}

// STAGE11D_LOOT_VALIDATION_SEAM_BEGIN safe_movement
combat::MovementInput stage11d_safe_movement_toward(
    combat::Vec3 from, combat::Vec3 to,
    const dungeon::DungeonSnapshot& snapshot) noexcept {
    constexpr std::array<combat::MovementInput, 9> kCandidates{{
        {-1, -1}, {0, -1}, {1, -1},
        {-1,  0}, {0,  0}, {1,  0},
        {-1,  1}, {0,  1}, {1,  1},
    }};
    constexpr float kStep = 0.10F;
    constexpr float kDiagonalStep = 0.07071068F;
    constexpr float kPickupGuardSquared = 1.60F * 1.60F;
    constexpr float kApproachGuardSquared = 1.75F * 1.75F;
    const auto near_from = [&](combat::Vec3 position) noexcept {
        const float x = position.x - from.x;
        const float y = position.y - from.y;
        return x * x + y * y < kApproachGuardSquared;
    };
    bool avoidance_active = false;
    for (std::size_t index = 0U; index < snapshot.ground_item_count; ++index) {
        avoidance_active = avoidance_active
            || near_from(snapshot.ground_items[index].position);
    }
    if (snapshot.combat.has_value()) {
        for (std::size_t index = 0U;
             index < snapshot.combat->monster_count; ++index) {
            const auto& monster = snapshot.combat->monsters[index];
            const bool defeated = monster.hp <= 0
                || monster.reaction == combat::ReactionState::defeated
                || monster.ai_phase == combat::MonsterAiPhase::defeated;
            avoidance_active = avoidance_active
                || (defeated && near_from(monster.position));
        }
    }
    combat::MovementInput best{};
    float best_score = 1.0e30F;
    for (const auto candidate : kCandidates) {
        const bool diagonal = candidate.x != 0 && candidate.y != 0;
        const float step = diagonal ? kDiagonalStep : kStep;
        const combat::Vec3 next{
            from.x + static_cast<float>(candidate.x) * step,
            from.y + static_cast<float>(candidate.y) * step,
            from.z,
        };
        bool safe = true;
        const auto approaches_pickup = [&](combat::Vec3 position) noexcept {
            const float current_x = position.x - from.x;
            const float current_y = position.y - from.y;
            const float next_x = position.x - next.x;
            const float next_y = position.y - next.y;
            const float current_distance = current_x * current_x
                + current_y * current_y;
            const float next_distance = next_x * next_x + next_y * next_y;
            return next_distance < kPickupGuardSquared
                || (current_distance < kApproachGuardSquared
                    && next_distance + 0.0001F < current_distance);
        };
        for (std::size_t index = 0U; index < snapshot.ground_item_count; ++index) {
            if (approaches_pickup(snapshot.ground_items[index].position)) {
                safe = false;
                break;
            }
        }
        if (safe && snapshot.combat.has_value()) {
            for (std::size_t index = 0U;
                 index < snapshot.combat->monster_count; ++index) {
                const auto& monster = snapshot.combat->monsters[index];
                const bool defeated = monster.hp <= 0
                    || monster.reaction == combat::ReactionState::defeated
                    || monster.ai_phase == combat::MonsterAiPhase::defeated;
                if (defeated && approaches_pickup(monster.position)) {
                    safe = false;
                    break;
                }
            }
        }
        if (!safe) continue;
        const float target_x = to.x - next.x;
        const float target_y = to.y - next.y;
        const bool stopped = candidate.x == 0 && candidate.y == 0;
        const float score = target_x * target_x + target_y * target_y
            + (stopped && avoidance_active ? 1.0F : 0.0F);
        if (score < best_score) {
            best = candidate;
            best_score = score;
        }
    }
    return best;
}
// STAGE11D_LOOT_VALIDATION_SEAM_END safe_movement

combat::Vec3 validation_door_position(
    dungeon::ExitDirection direction) noexcept {
    switch (direction) {
    case dungeon::ExitDirection::up: return {0.0F, -5.5F, 0.0F};
    case dungeon::ExitDirection::down: return {0.0F, 5.5F, 0.0F};
    case dungeon::ExitDirection::left: return {-12.0F, 0.0F, 0.0F};
    case dungeon::ExitDirection::right: return {12.0F, 0.0F, 0.0F};
    case dungeon::ExitDirection::none: return {};
    }
    return {};
}

combat::MovementInput validation_exit_movement(
    combat::Vec3 player,
    dungeon::ExitDirection direction) noexcept {
    combat::MovementInput movement = validation_movement_toward(
        player, validation_door_position(direction));
    switch (direction) {
    case dungeon::ExitDirection::up: movement.y = -1; break;
    case dungeon::ExitDirection::down: movement.y = 1; break;
    case dungeon::ExitDirection::left: movement.x = -1; break;
    case dungeon::ExitDirection::right: movement.x = 1; break;
    case dungeon::ExitDirection::none: break;
    }
    return movement;
}

bool validation_attack_lane(
    const combat::CombatSnapshot& state,
    const combat::MonsterSnapshot& target) noexcept;
dungeon::ExitDirection validation_direction(
    const RaylibHostConfig& config) noexcept;

void inject_stage11c_binding(PhysicalKeySnapshot& snapshot,
    const settings::SettingsData& settings_data,
    settings::SettingAction action, bool pressed) noexcept {
    const settings::StableKey key = settings::binding_for(settings_data, action);
    const std::size_t index = static_cast<std::size_t>(key);
    if (index >= snapshot.down.size()) return;
    snapshot.down[index] = true;
    if (pressed) snapshot.pressed[index] = true;
}

void inject_stage11c_movement(PhysicalKeySnapshot& snapshot,
    const settings::SettingsData& settings_data,
    combat::MovementInput movement) noexcept {
    if (movement.x < 0) {
        inject_stage11c_binding(snapshot, settings_data,
            settings::SettingAction::move_left, false);
    } else if (movement.x > 0) {
        inject_stage11c_binding(snapshot, settings_data,
            settings::SettingAction::move_right, false);
    }
    if (movement.y < 0) {
        inject_stage11c_binding(snapshot, settings_data,
            settings::SettingAction::move_up, false);
    } else if (movement.y > 0) {
        inject_stage11c_binding(snapshot, settings_data,
            settings::SettingAction::move_down, false);
    }
}

[[nodiscard]] PhysicalKeySnapshot inject_stage11c_physical_edges(
    PhysicalKeySnapshot snapshot, const RaylibHostConfig& config,
    const settings::SettingsData& settings_data,
    const dungeon::DungeonSnapshot& current,
    Stage11CHudValidationState& state) noexcept {
    using Scenario = Stage11CHudValidationScenario;
    if (config.stage11c_hud_validation == Scenario::none
            || snapshot.focus_lost) {
        return snapshot;
    }
    ++state.injected_frames;
    if (config.stage11c_hud_validation == Scenario::debug_overlay) {
        if (!state.debug_visible) snapshot.f1 = true;
        return snapshot;
    }
    if (!current.combat.has_value()) return snapshot;
    const combat::CombatSnapshot& combat_snapshot = *current.combat;
    if (current.phase == dungeon::RoomPhase::combat) {
        const combat::MonsterSnapshot* const target =
            nearest_living_monster(combat_snapshot);
        if (target == nullptr) return snapshot;
        const bool clears_room = config.stage11c_hud_validation
                == Scenario::cleared_exit
            || config.stage11c_hud_validation == Scenario::level_up_points
            || config.stage11c_hud_validation == Scenario::abyss_abandon;
        combat::Vec3 destination = target->position;
        if (clears_room) {
            destination.x += combat_snapshot.player.position.x
                    <= target->position.x ? -1.0F : 1.0F;
        }
        inject_stage11c_movement(snapshot, settings_data,
            validation_movement_toward(
                combat_snapshot.player.position, destination));
        if (clears_room && validation_attack_lane(combat_snapshot, *target)) {
            inject_stage11c_binding(snapshot, settings_data,
                settings::SettingAction::light_attack, true);
        }
        return snapshot;
    }
    if (config.stage11c_hud_validation != Scenario::abyss_abandon
            || current.phase != dungeon::RoomPhase::awaiting_exit) {
        return snapshot;
    }
    const dungeon::ExitDirection direction = current.is_abyss
        ? dungeon::ExitDirection::right : validation_direction(config);
    inject_stage11c_movement(snapshot, settings_data,
        validation_exit_movement(combat_snapshot.player.position, direction));
    return snapshot;
}

// STAGE11D_LOOT_VALIDATION_SEAM_BEGIN physical_driver
[[nodiscard]] PhysicalKeySnapshot inject_stage11d_physical_edges(
    PhysicalKeySnapshot snapshot, const RaylibHostConfig& config,
    const settings::SettingsData& settings_data,
    const dungeon::DungeonSnapshot& current,
    Stage11DLootValidationState& state) noexcept {
    using Scenario = Stage11DLootValidationScenario;
    if (config.stage11d_loot_validation == Scenario::none
            || snapshot.focus_lost) {
        return snapshot;
    }
    ++state.injected_frames;
    if (config.stage11d_loot_validation == Scenario::rare_only_abyss
            && state.captured && current.combat.has_value()) {
        for (std::size_t index = 0U; index < current.ground_item_count; ++index) {
            const auto& item = current.ground_items[index];
            if (item.item_id != state.abyss_item_id) continue;
            inject_stage11c_movement(snapshot, settings_data,
                validation_movement_toward(current.combat->player.position,
                    item.position));
            state.abyss_claim_requested = true;
            break;
        }
        return snapshot;
    }
    const bool ordinary_ready = stage11d_has_three_ordinary_rarities(current);
    if (config.stage11d_loot_validation == Scenario::preview_cancel
            && ordinary_ready) {
        const std::uint8_t phase = ++state.preview_phase;
        if (phase == 1U || phase == 12U || phase == 13U) snapshot.escape = true;
        else if (phase == 2U || (phase >= 4U && phase <= 10U)) {
            inject_stage11b_pressed(snapshot, settings::StableKey::arrow_down);
        } else if (phase == 3U) snapshot.enter = true;
        else if (phase == 11U) {
            inject_stage11b_pressed(snapshot, settings::StableKey::arrow_right);
        }
        return snapshot;
    }
    if (ordinary_ready) {
        if (config.stage11d_loot_validation == Scenario::pickup_feedback) {
            const auto* ground = stage11d_nearest_ground(current);
            if (ground != nullptr && current.combat.has_value()) {
                inject_stage11c_movement(snapshot, settings_data,
                    validation_movement_toward(current.combat->player.position,
                        ground->position));
            }
        }
        return snapshot;
    }
    if (!current.combat.has_value()
            || current.phase != dungeon::RoomPhase::combat) {
        return snapshot;
    }
    const auto& player = current.combat->player;
    const bool aggressive_abyss = config.stage11d_loot_validation
        == Scenario::rare_only_abyss;
    const auto* target = aggressive_abyss
        ? nearest_living_monster(*current.combat)
        : stage11d_priority_monster(*current.combat);
    if (target == nullptr) return snapshot;
    state.target_ordinal = target->spawn_ordinal;
    if (aggressive_abyss) {
        inject_stage11c_movement(snapshot, settings_data,
            validation_movement_toward(player.position, target->position));
        if (player.hurt_ticks == 0U
                && current.combat->diagnostics.input_size == 0U
                && validation_attack_lane(*current.combat, *target)) {
            inject_stage11c_binding(snapshot, settings_data,
                settings::SettingAction::light_attack, true);
        }
        return snapshot;
    }
    const bool needs_launcher_setup = target->hp == target->max_hp;
    constexpr float kLauncherDistance = 1.68F;
    constexpr float kComboDistance = 1.98F;
    const float action_distance = needs_launcher_setup
        ? kLauncherDistance : kComboDistance;
    combat::Vec3 destination = target->position;
    destination.x += player.position.x <= target->position.x
        ? -action_distance : action_distance;
    const combat::MovementInput movement = stage11d_safe_movement_toward(
        player.position, destination, current);
    inject_stage11c_movement(snapshot, settings_data, movement);
    bool nearby_threat = false;
    for (std::size_t index = 0U;
         index < current.combat->monster_count; ++index) {
        const auto& monster = current.combat->monsters[index];
        if (!monster.active || monster.hp <= 0
                || (monster.ai_phase != combat::MonsterAiPhase::telegraph
                    && monster.ai_phase != combat::MonsterAiPhase::active)) {
            continue;
        }
        const float threat_x = monster.position.x - player.position.x;
        const float threat_y = monster.position.y - player.position.y;
        nearby_threat = threat_x * threat_x + threat_y * threat_y <= 9.0F;
        if (nearby_threat) break;
    }
    const bool action_ready = player.hurt_ticks == 0U
        && player.active_attack == combat::AttackId::none
        && current.combat->diagnostics.input_size == 0U;
    const bool light_combo_can_start_or_buffer =
        player.active_attack == combat::AttackId::none
        || player.active_attack == combat::AttackId::j1
        || player.active_attack == combat::AttackId::j2;
    const bool priority_combo = !needs_launcher_setup
        && light_combo_can_start_or_buffer
        && player.hurt_ticks == 0U
        && current.combat->diagnostics.input_size == 0U
        && stage11d_attack_lane(*current.combat, *target, 1.88F, 2.08F);
    if (needs_launcher_setup && action_ready
            && stage11d_attack_lane(
                *current.combat, *target, 1.55F, 1.72F)) {
        inject_stage11c_binding(snapshot, settings_data,
            settings::SettingAction::launcher, true);
    } else if (priority_combo) {
        inject_stage11c_binding(snapshot, settings_data,
            settings::SettingAction::light_attack, true);
    } else if (action_ready && player.position.z <= 0.01F && nearby_threat) {
        inject_stage11c_binding(snapshot, settings_data,
            settings::SettingAction::jump, true);
    }
    return snapshot;
}
// STAGE11D_LOOT_VALIDATION_SEAM_END physical_driver

[[nodiscard]] std::uint64_t stage11c_production_snapshot_hash(
    const dungeon::DungeonSnapshot& snapshot) noexcept {
    std::uint64_t hash = 1469598103934665603ULL;
    const auto mix = [&hash](std::uint64_t value) noexcept {
        hash ^= value;
        hash *= 1099511628211ULL;
    };
    mix(snapshot.session_tick);
    mix(snapshot.root_seed);
    mix(snapshot.commit_generation);
    mix(snapshot.room_index);
    mix(snapshot.room_seed);
    mix(snapshot.depth);
    mix(snapshot.floor_room_index);
    mix(static_cast<std::uint64_t>(snapshot.phase));
    mix(snapshot.is_abyss ? 1U : 0U);
    mix(snapshot.abyss_exit_confirmation_armed ? 1U : 0U);
    mix(snapshot.remaining_targets);
    mix(snapshot.progression.level);
    mix(snapshot.progression.experience);
    mix(snapshot.progression.unspent_passive_points);
    if (snapshot.combat.has_value()) {
        const combat::PlayerSnapshot& player = snapshot.combat->player;
        mix(static_cast<std::uint64_t>(player.hp));
        mix(static_cast<std::uint64_t>(player.max_hp));
        mix(static_cast<std::uint64_t>(player.barrier));
        mix(player.slow_ticks);
        mix(player.corrosion_ticks);
        mix(player.invulnerability_ticks);
        for (const combat::MonsterSnapshot& monster : snapshot.combat->monsters) {
            mix(monster.active ? 1U : 0U);
            mix(static_cast<std::uint64_t>(monster.hp));
            mix(monster.spawn_ordinal);
        }
    }
    return hash;
}

[[nodiscard]] const char* stage11c_scenario_name(
    Stage11CHudValidationScenario scenario) noexcept {
    switch (scenario) {
    case Stage11CHudValidationScenario::none: return "none";
    case Stage11CHudValidationScenario::normal_combat: return "normal_combat";
    case Stage11CHudValidationScenario::low_health_status: return "low_health_status";
    case Stage11CHudValidationScenario::cleared_exit: return "cleared_exit";
    case Stage11CHudValidationScenario::abyss_abandon: return "abyss_abandon";
    case Stage11CHudValidationScenario::level_up_points: return "level_up_points";
    case Stage11CHudValidationScenario::debug_overlay: return "debug_overlay";
    }
    return "invalid";
}

[[nodiscard]] bool stage11c_hud_validation_reached(
    const dungeon::DungeonSnapshot& snapshot,
    Stage11CHudValidationScenario scenario,
    const Stage11CHudValidationState& state, bool draw_debug) noexcept {
    using Scenario = Stage11CHudValidationScenario;
    if (!snapshot.combat.has_value()) return false;
    const combat::PlayerSnapshot& player = snapshot.combat->player;
    switch (scenario) {
    case Scenario::none:
        return false;
    case Scenario::normal_combat:
        return state.injected_frames >= 4U
            && snapshot.phase == dungeon::RoomPhase::combat
            && snapshot.remaining_targets != 0U;
    case Scenario::low_health_status:
        return player.hp > 0 && player.max_hp > 0
            && static_cast<std::int64_t>(player.hp) * 4
                <= static_cast<std::int64_t>(player.max_hp)
            && (player.slow_ticks != 0U || player.corrosion_ticks != 0U
                || player.invulnerability_ticks != 0U);
    case Scenario::cleared_exit:
        return snapshot.phase == dungeon::RoomPhase::awaiting_exit
            && snapshot.progression.level > 1U;
    case Scenario::abyss_abandon:
        return snapshot.is_abyss
            && snapshot.phase == dungeon::RoomPhase::awaiting_exit
            && snapshot.abyss_exit_confirmation_armed;
    case Scenario::level_up_points:
        return snapshot.phase == dungeon::RoomPhase::awaiting_exit
            && snapshot.last_levels_gained != 0U
            && snapshot.progression.unspent_passive_points != 0U;
    case Scenario::debug_overlay:
        return state.injected_frames >= 4U && draw_debug;
    }
    return false;
}

void write_stage11c_rect(std::ostream& stream, const char* name,
    HudRect rect) {
    stream << name << '=' << rect.x << ',' << rect.y << ','
           << rect.width << ',' << rect.height << '\n';
}

void write_stage11c_hud_validation_summary(const RaylibHostConfig& config,
    const Stage11CHudValidationState& state) noexcept {
    if (!config.validation_summary_file.has_value()
            || config.stage11c_hud_validation
                == Stage11CHudValidationScenario::none) {
        return;
    }
    try {
        std::ofstream stream(*config.validation_summary_file,
            std::ios::out | std::ios::trunc);
        if (!stream) return;
        stream << "scenario="
               << stage11c_scenario_name(config.stage11c_hud_validation) << '\n';
        write_stage11c_rect(stream, "safe_rect", state.layout.safe_area);
        write_stage11c_rect(stream, "player_rect", state.layout.player_panel);
        write_stage11c_rect(stream, "objective_rect", state.layout.objective_panel);
        write_stage11c_rect(stream, "navigation_rect", state.layout.navigation_panel);
        write_stage11c_rect(stream, "primary_notice_rect", state.layout.primary_notice);
        write_stage11c_rect(stream, "secondary_notice_rect", state.layout.secondary_notice);
        write_stage11c_rect(stream, "debug_rect", state.layout.debug_panel);
        const PlayerHudModel& player = state.model.player;
        stream << "player_values=" << player.hp << ',' << player.max_hp << ','
               << player.barrier << ',' << player.max_barrier << ','
               << static_cast<unsigned>(player.level) << ','
               << player.experience << ',' << player.required_experience << ','
               << static_cast<unsigned>(player.unspent_passive_points) << '\n'
               << "status_tags=";
        for (std::uint8_t index = 0U; index < player.status_tag_count; ++index) {
            if (index != 0U) stream << ',';
            stream << static_cast<unsigned>(player.status_tags[index]);
        }
        stream << '\n'
               << "objective=" << state.model.room.objective.bytes.data() << '\n'
               << "notice_kinds="
               << static_cast<unsigned>(state.notices.primary.kind) << ','
               << static_cast<unsigned>(state.notices.secondary.kind) << '\n'
               << "notice_texts="
               << state.notices.primary.text.bytes.data() << '|'
               << state.notices.secondary.text.bytes.data() << '\n'
               << "navigation_values=" << state.model.navigation.depth << ','
               << state.model.navigation.floor_room << ','
               << static_cast<unsigned>(state.model.navigation.ecology);
        for (std::uint32_t bias : state.model.navigation.biases) {
            stream << ',' << bias;
        }
        const bool stage11c_validation_result = state.captured
            && state.cjk_font_ready && state.production_snapshot_hash != 0U;
        stream << '\n'
               << "font_ready=" << (state.cjk_font_ready ? 1 : 0) << '\n'
               << "f1=" << (state.debug_visible ? 1 : 0) << '\n'
               << "production_snapshot_hash="
               << state.production_snapshot_hash << '\n'
               << "result="
               << (stage11c_validation_result ? "pass" : "fail") << '\n';
    } catch (...) {
        TraceLog(LOG_WARNING, "failed to write stage11c HUD validation summary");
    }
}

// STAGE11D_LOOT_VALIDATION_SEAM_BEGIN evidence_semantics
[[nodiscard]] bool stage11d_view_has_rarity(
    const GroundLootView& view, const dungeon::DungeonSnapshot& snapshot,
    items::ItemRarity rarity, bool abyss) noexcept {
    for (std::size_t label_index = 0U; label_index < view.count; ++label_index) {
        const auto& label = view.labels[label_index];
        for (std::size_t item_index = 0U;
             item_index < snapshot.ground_item_count; ++item_index) {
            const auto& item = snapshot.ground_items[item_index];
            if (item.ordinal == label.ordinal && item.rarity == rarity
                    && (item.source == dungeon::GroundItemSource::abyss_chest)
                        == abyss) {
                return true;
            }
        }
    }
    return false;
}

void stage11d_record_semantics(Stage11DLootValidationState& state,
    const dungeon::DungeonSnapshot& snapshot,
    const items::ItemOwnershipState* ownership,
    const GroundLootView& view, HudNoticeView notices) noexcept {
    state.view = view;
    state.notices = notices;
    state.snapshot_item_count = snapshot.ground_item_count;
    state.snapshot_item_ids.fill(0U);
    for (std::size_t index = 0U; index < snapshot.ground_item_count; ++index) {
        const auto& item = snapshot.ground_items[index];
        state.snapshot_item_ids[index] = item.item_id;
        if (item.source == dungeon::GroundItemSource::abyss_chest) {
            state.abyss_item_id = item.item_id;
            state.abyss_rarity = item.rarity;
        } else if (item.rarity == items::ItemRarity::normal) {
            state.normal_item_id = item.item_id;
        } else if (item.rarity == items::ItemRarity::magic) {
            state.magic_item_id = item.item_id;
        } else if (item.rarity == items::ItemRarity::rare) {
            state.rare_item_id = item.item_id;
        }
    }
    state.inventory_item_count = 0U;
    state.inventory_item_ids.fill(0U);
    if (ownership != nullptr) {
        for (const auto& item : ownership->items) {
            if (state.inventory_item_count >= state.inventory_item_ids.size()) break;
            state.inventory_item_ids[state.inventory_item_count++] = item.id;
        }
    }
}

[[nodiscard]] bool stage11d_target_visible(
    const RaylibHostConfig& config, const dungeon::DungeonSnapshot& snapshot,
    const PauseMenuState& pause_menu, const DungeonRenderStatus& status,
    settings::LootFilterMode mode, const GroundLootView& view,
    HudNoticeView notices, Stage11DLootValidationState& state) noexcept {
    using Scenario = Stage11DLootValidationScenario;
    const Scenario scenario = config.stage11d_loot_validation;
    if (scenario == Scenario::none) return false;
    state.ordinary_normal_count = 0U;
    state.ordinary_magic_count = 0U;
    state.ordinary_rare_count = 0U;
    state.remaining_targets = snapshot.remaining_targets;
    state.live_inventory_count = snapshot.inventory_count;
    state.last_phase = snapshot.phase;
    state.player_hp = snapshot.combat.has_value()
        ? snapshot.combat->player.hp : 0;
    if (status.loot_pickup.valid) {
        state.pickup_item_id = status.loot_pickup.item_id;
        state.pickup_commit_generation = status.loot_pickup.commit_generation;
    }
    if (notices.primary.kind == HudNoticeKind::loot_pickup
            || notices.secondary.kind == HudNoticeKind::loot_pickup) {
        const HudLayout layout = make_hud_layout(
            GetScreenWidth(), GetScreenHeight(), false);
        state.pickup_notice_rect = notices.primary.kind
                == HudNoticeKind::loot_pickup
            ? layout.primary_notice : layout.secondary_notice;
    }
    if (snapshot.combat.has_value()) {
        for (std::size_t index = 0U;
             index < snapshot.combat->monster_count; ++index) {
            const auto& monster = snapshot.combat->monsters[index];
            if (monster.spawn_ordinal >= state.last_monster_hp.size()) continue;
            const std::size_t ordinal = monster.spawn_ordinal;
            state.seen_ordinal_bits = static_cast<std::uint8_t>(
                state.seen_ordinal_bits | (1U << ordinal));
            state.last_monster_hp[ordinal] = monster.hp;
            state.min_monster_hp[ordinal] = (std::min)(
                state.min_monster_hp[ordinal], monster.hp);
            state.monster_affix_danger[ordinal] =
                combat::monster_affix_danger_score(monster.affixes);
            state.monster_ai_phase[ordinal] = static_cast<std::uint8_t>(
                monster.ai_phase);
            if (monster.hp <= 0
                    || monster.reaction == combat::ReactionState::defeated
                    || monster.ai_phase == combat::MonsterAiPhase::defeated) {
                const std::uint8_t bit = static_cast<std::uint8_t>(1U << ordinal);
                if ((state.defeated_ordinal_bits & bit) == 0U) {
                    const float x = monster.position.x
                        - snapshot.combat->player.position.x;
                    const float y = monster.position.y
                        - snapshot.combat->player.position.y;
                    state.defeat_distance_milli[ordinal] =
                        static_cast<std::uint32_t>(std::lround(
                            std::sqrt(x * x + y * y) * 1000.0F));
                    state.defeat_player_hp[ordinal] = snapshot.combat->player.hp;
                }
                state.defeated_ordinal_bits = static_cast<std::uint8_t>(
                    state.defeated_ordinal_bits | bit);
            }
        }
    }
    for (std::size_t index = 0U; index < snapshot.ground_item_count; ++index) {
        const auto& item = snapshot.ground_items[index];
        if (item.source != dungeon::GroundItemSource::monster_drop) continue;
        if (item.rarity == items::ItemRarity::normal) {
            ++state.ordinary_normal_count;
            state.observed_normal_item_id = item.item_id;
            state.observed_normal_ordinal = item.ordinal;
        } else if (item.rarity == items::ItemRarity::magic) {
            ++state.ordinary_magic_count;
            state.observed_magic_item_id = item.item_id;
            state.observed_magic_ordinal = item.ordinal;
        } else if (item.rarity == items::ItemRarity::rare) {
            ++state.ordinary_rare_count;
            state.observed_rare_item_id = item.item_id;
            state.observed_rare_ordinal = item.ordinal;
        }
    }
    state.max_ordinary_normal_count = (std::max)(
        state.max_ordinary_normal_count, state.ordinary_normal_count);
    state.max_ordinary_magic_count = (std::max)(
        state.max_ordinary_magic_count, state.ordinary_magic_count);
    state.max_ordinary_rare_count = (std::max)(
        state.max_ordinary_rare_count, state.ordinary_rare_count);
    state.max_ground_item_count = (std::max)(state.max_ground_item_count,
        static_cast<std::uint32_t>(snapshot.ground_item_count));
    state.max_inventory_count = (std::max)(state.max_inventory_count,
        snapshot.inventory_count);
    state.min_remaining_targets = (std::min)(
        state.min_remaining_targets, snapshot.remaining_targets);
    if (scenario == Scenario::pickup_feedback) {
        const bool notice = notices.primary.kind == HudNoticeKind::loot_pickup
            || notices.secondary.kind == HudNoticeKind::loot_pickup;
        return status.loot_pickup.valid && notice;
    }
    if (scenario == Scenario::rare_only_abyss) {
        for (std::size_t index = 0U; index < snapshot.ground_item_count; ++index) {
            const auto& item = snapshot.ground_items[index];
            if (item.source == dungeon::GroundItemSource::abyss_chest
                    && item.rarity != items::ItemRarity::rare
                    && ground_loot_visible(item, mode)) {
                return true;
            }
        }
        return false;
    }
    if (!stage11d_has_three_ordinary_rarities(snapshot)) return false;
    if (scenario == Scenario::preview_cancel) {
        if (state.preview_phase == 11U) state.preview_visible_count = view.count;
        if (state.preview_phase < 13U || pause_menu.screen != PauseScreen::closed) {
            return false;
        }
        state.restored_visible_count = view.count;
        return state.preview_visible_count < state.restored_visible_count
            && pause_menu.committed.loot_filter_mode
                == settings::LootFilterMode::show_all
            && pause_menu.draft.loot_filter_mode
                == settings::LootFilterMode::show_all;
    }
    const bool normal = stage11d_view_has_rarity(
        view, snapshot, items::ItemRarity::normal, false);
    const bool magic = stage11d_view_has_rarity(
        view, snapshot, items::ItemRarity::magic, false);
    const bool rare = stage11d_view_has_rarity(
        view, snapshot, items::ItemRarity::rare, false);
    if (scenario == Scenario::show_all) return normal && magic && rare;
    if (scenario == Scenario::magic_or_better) return !normal && magic && rare;
    return scenario == Scenario::rare_only && !normal && !magic && rare;
}

[[nodiscard]] const char* stage11d_scenario_name(
    Stage11DLootValidationScenario scenario) noexcept {
    switch (scenario) {
    case Stage11DLootValidationScenario::none: return "none";
    case Stage11DLootValidationScenario::show_all: return "show_all";
    case Stage11DLootValidationScenario::magic_or_better: return "magic_or_better";
    case Stage11DLootValidationScenario::rare_only: return "rare_only";
    case Stage11DLootValidationScenario::rare_only_abyss: return "rare_only_abyss";
    case Stage11DLootValidationScenario::preview_cancel: return "preview_cancel";
    case Stage11DLootValidationScenario::pickup_feedback: return "pickup_feedback";
    }
    return "invalid";
}

void write_stage11d_loot_validation_summary(const RaylibHostConfig& config,
    const Stage11DLootValidationState& state,
    const PauseMenuState& pause_menu) noexcept {
    if (!config.validation_summary_file.has_value()
            || config.stage11d_loot_validation
                == Stage11DLootValidationScenario::none) return;
    try {
        std::ofstream stream(*config.validation_summary_file,
            std::ios::out | std::ios::trunc);
        if (!stream) return;
        stream << "scenario=" << stage11d_scenario_name(
            config.stage11d_loot_validation) << '\n'
            << "committed_mode=" << static_cast<unsigned>(
                pause_menu.committed.loot_filter_mode) << '\n'
            << "draft_mode=" << static_cast<unsigned>(
                pause_menu.draft.loot_filter_mode) << '\n'
            << "snapshot_count=" << state.snapshot_item_count << '\n'
            << "visible_count=" << state.view.count << '\n'
            << "inventory_count=" << state.inventory_item_count << '\n'
            << "normal_item_id=" << state.normal_item_id << '\n'
            << "magic_item_id=" << state.magic_item_id << '\n'
            << "rare_item_id=" << state.rare_item_id << '\n'
            << "abyss_item_id=" << state.abyss_item_id << '\n'
            << "abyss_rarity=" << static_cast<unsigned>(
                state.abyss_rarity) << '\n'
            << "preview_visible_count=" << state.preview_visible_count << '\n'
            << "restored_visible_count=" << state.restored_visible_count << '\n'
            << "abyss_claimed=" << (state.abyss_claimed ? 1 : 0) << '\n'
            << "progress_phase=" << static_cast<unsigned>(state.last_phase) << '\n'
            << "progress_remaining=" << static_cast<unsigned>(
                state.remaining_targets) << '\n'
            << "progress_inventory=" << state.live_inventory_count << '\n'
            << "progress_hp=" << state.player_hp << '\n'
            << "progress_rarities=" << state.ordinary_normal_count << ','
                << state.ordinary_magic_count << ','
                << state.ordinary_rare_count << '\n'
            << "max_ground_rarities=" << state.max_ordinary_normal_count << ','
                << state.max_ordinary_magic_count << ','
                << state.max_ordinary_rare_count << '\n'
            << "max_ground_count=" << state.max_ground_item_count << '\n'
            << "min_remaining=" << static_cast<unsigned>(
                state.min_remaining_targets) << '\n'
            << "max_inventory=" << state.max_inventory_count << '\n'
            << "observed_normal=" << state.observed_normal_item_id << ','
                << state.observed_normal_ordinal << '\n'
            << "observed_magic=" << state.observed_magic_item_id << ','
                << state.observed_magic_ordinal << '\n'
            << "observed_rare=" << state.observed_rare_item_id << ','
                << state.observed_rare_ordinal << '\n'
            << "monster_seen_bits=" << static_cast<unsigned>(
                state.seen_ordinal_bits) << '\n'
            << "monster_defeated_bits=" << static_cast<unsigned>(
                state.defeated_ordinal_bits) << '\n'
            << "monster_last_hp=" << state.last_monster_hp[0] << ','
                << state.last_monster_hp[1] << ','
                << state.last_monster_hp[2] << '\n'
            << "monster_min_hp=" << state.min_monster_hp[0] << ','
                << state.min_monster_hp[1] << ','
                << state.min_monster_hp[2] << '\n'
            << "monster_affix_danger=" << state.monster_affix_danger[0]
                << ',' << state.monster_affix_danger[1] << ','
                << state.monster_affix_danger[2] << '\n'
            << "monster_ai_phase=" << static_cast<unsigned>(
                state.monster_ai_phase[0]) << ',' << static_cast<unsigned>(
                state.monster_ai_phase[1]) << ',' << static_cast<unsigned>(
                state.monster_ai_phase[2]) << '\n'
            << "defeat_distance_milli=" << state.defeat_distance_milli[0]
                << ',' << state.defeat_distance_milli[1] << ','
                << state.defeat_distance_milli[2] << '\n'
            << "defeat_player_hp=" << state.defeat_player_hp[0] << ','
                << state.defeat_player_hp[1] << ','
                << state.defeat_player_hp[2] << '\n'
            << "target_ordinal=" << state.target_ordinal << '\n'
            << "pickup_item_id=" << state.pickup_item_id << '\n'
            << "pickup_commit_generation="
                << state.pickup_commit_generation << '\n'
            << "pickup_notice_rect=" << state.pickup_notice_rect.x << ','
                << state.pickup_notice_rect.y << ','
                << state.pickup_notice_rect.width << ','
                << state.pickup_notice_rect.height << '\n'
            << "notice_text=";
        const HudNotice& notice = state.notices.primary.kind
                == HudNoticeKind::loot_pickup
            ? state.notices.primary : state.notices.secondary;
        stream << notice.text.bytes.data() << '\n' << "snapshot_ids=";
        for (std::size_t index = 0U; index < state.snapshot_item_count; ++index) {
            if (index != 0U) stream << ',';
            stream << state.snapshot_item_ids[index];
        }
        stream << '\n' << "inventory_ids=";
        for (std::size_t index = 0U; index < state.inventory_item_count; ++index) {
            if (index != 0U) stream << ',';
            stream << state.inventory_item_ids[index];
        }
        stream << '\n' << "label_rects=";
        for (std::size_t index = 0U; index < state.view.count; ++index) {
            if (index != 0U) stream << ';';
            const auto& label = state.view.labels[index];
            stream << label.ordinal << ',' << label.rect.x << ',' << label.rect.y
                << ',' << label.rect.width << ',' << label.rect.height;
        }
        stream << '\n' << "result=" << (state.captured
            && (config.stage11d_loot_validation
                    != Stage11DLootValidationScenario::rare_only_abyss
                || state.abyss_claimed) ? "pass" : "fail") << '\n';
    } catch (...) {
        TraceLog(LOG_WARNING, "failed to write stage11d loot validation summary");
    }
}
// STAGE11D_LOOT_VALIDATION_SEAM_END evidence_semantics

bool validation_attack_lane(
    const combat::CombatSnapshot& state,
    const combat::MonsterSnapshot& target) noexcept {
    const float x = target.position.x - state.player.position.x;
    const float y = target.position.y - state.player.position.y;
    const bool facing = std::fabs(x) <= 0.20F
        || (x > 0.0F && state.player.facing == combat::Facing::right)
        || (x < 0.0F && state.player.facing == combat::Facing::left);
    return facing && std::fabs(x) <= 1.70F && std::fabs(y) <= 0.55F;
}

dungeon::ExitDirection validation_direction(
    const RaylibHostConfig& config) noexcept {
    return config.validation_abyss_direction < 4U
        ? static_cast<dungeon::ExitDirection>(
            config.validation_abyss_direction)
        : dungeon::ExitDirection::none;
}

combat::MovementInput stage10_validation_input(
    dungeon::DungeonSession& session,
    const dungeon::DungeonSnapshot& snapshot,
    const RaylibHostConfig& config,
    Stage10ValidationState& state) noexcept {
    const Stage10ValidationScenario scenario = config.stage10_validation;
    state.entered_abyss = state.entered_abyss || snapshot.is_abyss;
    if (scenario == Stage10ValidationScenario::room_reset
            && snapshot.is_abyss
            && snapshot.phase == dungeon::RoomPhase::combat
            && !state.reset_requested) {
        state.reset_requested = session.reset_current_room()
            != dungeon::RequestResult::rejected;
        return {};
    }
    if (scenario == Stage10ValidationScenario::player_death
            && snapshot.is_abyss) {
        return {};
    }
    if (!snapshot.combat.has_value()) return {};
    if (snapshot.phase == dungeon::RoomPhase::combat) {
        const auto* const target = nearest_living_monster(*snapshot.combat);
        if (target == nullptr) return {};
        if (scenario == Stage10ValidationScenario::abyss_hole_descent
                && snapshot.is_abyss && snapshot.remaining_targets == 1U) {
            const auto& monster = target->position;
            const float x = monster.x - kHoleCenter.x;
            const float y = monster.y - kHoleCenter.y;
            if (x * x + y * y > 1.44F) {
                return validation_movement_toward(
                    snapshot.combat->player.position, kHoleCenter);
            }
        }
        const auto movement = validation_movement_toward(
            snapshot.combat->player.position, target->position);
        if (snapshot.combat->player.hurt_ticks == 0U
                && snapshot.combat->player.active_attack
                    == combat::AttackId::none
                && snapshot.combat->diagnostics.input_size == 0U
                && validation_attack_lane(*snapshot.combat, *target)) {
            static_cast<void>(session.queue_action(combat::Action::light));
        }
        return movement;
    }
    if (snapshot.phase != dungeon::RoomPhase::awaiting_exit) return {};
    if (!snapshot.is_abyss) {
        return validation_exit_movement(snapshot.combat->player.position,
            validation_direction(config));
    }
    if (scenario == Stage10ValidationScenario::exit_confirmation) {
        return snapshot.abyss_exit_confirmation_armed
            ? combat::MovementInput{}
            : validation_exit_movement(snapshot.combat->player.position,
                dungeon::ExitDirection::right);
    }
    if (scenario == Stage10ValidationScenario::abyss_hole_descent) {
        state.descent_warning_seen = state.descent_warning_seen
            || snapshot.abyss_exit_confirmation_armed;
        const combat::MovementInput movement = validation_movement_toward(
            snapshot.combat->player.position, kHoleCenter);
        if (can_prompt_descent(snapshot, snapshot.combat->player.position)) {
            static_cast<void>(session.request_descent(true));
        }
        return movement;
    }
    return {};
}

combat::MovementInput stage11_validation_input(
    dungeon::DungeonSession& session,
    const dungeon::DungeonSnapshot& snapshot,
    const RaylibHostConfig& config,
    Stage11ValidationState& state) noexcept {
    state.entered_abyss = state.entered_abyss || snapshot.is_abyss;
    state.saw_depth_two = state.saw_depth_two || snapshot.depth > 1U;
    const auto scenario = config.stage11_validation;
    const bool drive_to_abyss = scenario
        == Stage11ValidationScenario::abyss_death_recap
        && !state.entered_abyss;
    const bool drive_to_depth = scenario
        == Stage11ValidationScenario::deep_continue
        && !state.saw_depth_two;
    if (!drive_to_abyss && !drive_to_depth) return {};
    if (!snapshot.combat.has_value()) return {};
    if (snapshot.phase == dungeon::RoomPhase::combat) {
        const auto* const target = nearest_living_monster(*snapshot.combat);
        if (target == nullptr) return {};
        const auto movement = validation_movement_toward(
            snapshot.combat->player.position, target->position);
        if (snapshot.combat->diagnostics.input_size == 0U
                && validation_attack_lane(*snapshot.combat, *target)) {
            static_cast<void>(session.queue_action(combat::Action::light));
        }
        return movement;
    }
    if (snapshot.phase != dungeon::RoomPhase::awaiting_exit) return {};
    if (drive_to_abyss) {
        return validation_exit_movement(snapshot.combat->player.position,
            validation_direction(config));
    }
    const auto movement = validation_movement_toward(
        snapshot.combat->player.position, kHoleCenter);
    if (can_prompt_descent(snapshot, snapshot.combat->player.position)) {
        static_cast<void>(session.request_descent(true));
    }
    return movement;
}

bool stage11_validation_reached(const dungeon::DungeonSnapshot& snapshot,
    const RaylibHostConfig& config,
    const Stage11ValidationState& state) noexcept {
    const bool pending = snapshot.death.has_value()
        && snapshot.death->can_continue && !snapshot.death->saving;
    switch (config.stage11_validation) {
    case Stage11ValidationScenario::none: return false;
    case Stage11ValidationScenario::normal_death_recap:
    case Stage11ValidationScenario::restart_same_recap:
        return pending && !snapshot.death->checkpoint.death_was_abyss;
    case Stage11ValidationScenario::abyss_death_recap:
        return pending && state.entered_abyss
            && snapshot.death->checkpoint.death_was_abyss;
    case Stage11ValidationScenario::deep_continue:
        return state.continue_requested && state.saw_depth_two
            && !snapshot.death.has_value() && snapshot.depth == 1U
            && snapshot.combat.has_value()
            && snapshot.combat->player.hp > 0;
    case Stage11ValidationScenario::floor_one_continue:
        return state.continue_requested && !snapshot.death.has_value()
            && snapshot.depth == 1U && snapshot.combat.has_value()
            && snapshot.combat->player.hp > 0;
    }
    return false;
}

bool has_environment_visual(
    const dungeon::DungeonSnapshot& snapshot,
    combat::HazardKind kind,
    bool require_warning) noexcept {
    if (!snapshot.combat.has_value()) return false;
    for (std::size_t index = 0U;
         index < snapshot.combat->hazard_count; ++index) {
        const combat::HazardSnapshot& hazard = snapshot.combat->hazards[index];
        if (hazard.active
                && hazard.source == combat::HazardSource::abyss_environment
                && hazard.kind == kind
                && (!require_warning || hazard.telegraph_ticks != 0U)) {
            return true;
        }
    }
    return false;
}

bool stage10_validation_reached(
    const dungeon::DungeonSnapshot& snapshot,
    const RaylibHostConfig& config,
    const Stage10ValidationState& state) noexcept {
    switch (config.stage10_validation) {
    case Stage10ValidationScenario::none:
        return false;
    case Stage10ValidationScenario::abyss_door: {
        const auto direction = validation_direction(config);
        const std::size_t index = static_cast<std::size_t>(direction);
        return !snapshot.is_abyss
            && snapshot.phase == dungeon::RoomPhase::awaiting_exit
            && index < snapshot.abyss_doors.size()
            && snapshot.abyss_doors[index];
    }
    case Stage10ValidationScenario::thunderstorm_warning:
        return snapshot.abyss_rule == abyss::AbyssRuleId::thunderstorm
            && has_environment_visual(snapshot,
                combat::HazardKind::thunderstorm, true);
    case Stage10ValidationScenario::hunting_flames_warning:
        return snapshot.abyss_rule == abyss::AbyssRuleId::hunting_flames
            && has_environment_visual(snapshot,
                combat::HazardKind::hunting_flame, true);
    case Stage10ValidationScenario::chaos_expansion:
        return snapshot.abyss_rule == abyss::AbyssRuleId::chaos_expansion
            && has_environment_visual(snapshot,
                combat::HazardKind::chaos_expansion, false);
    case Stage10ValidationScenario::reward_chest:
        return snapshot.is_abyss && snapshot.ground_item_count != 0U;
    case Stage10ValidationScenario::pending_reward:
        return snapshot.is_abyss && snapshot.abyss_pending_rewards != 0U;
    case Stage10ValidationScenario::exit_confirmation:
        return snapshot.is_abyss
            && snapshot.abyss_exit_confirmation_armed;
    case Stage10ValidationScenario::player_death:
    case Stage10ValidationScenario::room_reset:
        return state.entered_abyss && !snapshot.is_abyss;
    case Stage10ValidationScenario::leave_started:
        return snapshot.is_abyss
            && snapshot.phase == dungeon::RoomPhase::combat;
    case Stage10ValidationScenario::restarted_failed:
        return !snapshot.is_abyss && snapshot.room_index == 1U;
    case Stage10ValidationScenario::abyss_hole_descent:
        return state.entered_abyss && state.descent_warning_seen
            && snapshot.depth > 1U;
    }
    return false;
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
        DungeonRuntime runtime(runtime_config);
        const bool initialized = runtime.initialize();
        if (!initialized && runtime.state() != DungeonRuntimeState::recovery_required) {
            return HostExitCode::save_initialization_failed;
        }

        SetConfigFlags(initial_window_flags(committed_settings));
        InitWindow(config.window_width, config.window_height, config.window_title);
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
                ) {
            static_cast<void>(SetForegroundWindow(GetWindowHandle()));
        }
#endif
        core::FixedStepRunner fixed_step;
        CombatRenderer renderer;
        const bool hud_resources_ready = renderer.initialize_resources();
        PauseMenuRenderer pause_menu_renderer;
        static_cast<void>(pause_menu_renderer.initialize());
        CombatFeedback feedback;
        GameAudio audio;
        InventoryRenderer inventory;
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
        std::uint32_t presented_frame_count = 0U;
        unsigned validation_capture_tick = 0U;
        unsigned validation_capture_count = 0U;
        Stage10ValidationState stage10_validation_state{};
        Stage11ValidationState stage11_validation_state{};
        Stage11BValidationState stage11b_validation_state{};
        stage11b_validation_state.load_status = loaded.status;
        Stage11CHudValidationState stage11c_validation_state{};
        stage11c_validation_state.cjk_font_ready = hud_resources_ready;
// STAGE11D_LOOT_VALIDATION_SEAM_BEGIN runtime_state
        Stage11DLootValidationState stage11d_validation_state{};
// STAGE11D_LOOT_VALIDATION_SEAM_END runtime_state
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
        dungeon::DungeonSnapshot current{};
        dungeon::DungeonSnapshot previous{};
        if (runtime.session() != nullptr) {
            current = runtime.session()->snapshot();
            previous = current;
            drain_events(*runtime.session(), renderer, feedback, audio);
        }

        while (!exit_requested) {
            const bool window_close_requested = WindowShouldClose();
            const PhysicalKeySnapshot sampled_physical_keys = sample_physical_keys();
            const PhysicalKeySnapshot stage11b_physical_keys =
                inject_stage11b_physical_edges(
                sampled_physical_keys, config, stage11b_validation_state);
            const PhysicalKeySnapshot stage11c_physical_keys = inject_stage11c_physical_edges(
                stage11b_physical_keys, config, input_settings, current,
                stage11c_validation_state);
// STAGE11D_LOOT_VALIDATION_SEAM_BEGIN runtime_input
            const PhysicalKeySnapshot physical_keys = inject_stage11d_physical_edges(
                stage11c_physical_keys, config, input_settings, current,
                stage11d_validation_state);
// STAGE11D_LOOT_VALIDATION_SEAM_END runtime_input
            HostFrameInput frame_input = map_host_frame_input(
                input_settings, physical_keys);
            if (recovery_requested(runtime.state() == DungeonRuntimeState::recovery_required,
                    frame_input.keys.recovery)) {
                if (runtime.recover_with_new_run() && runtime.session() != nullptr) {
                    current = runtime.session()->snapshot();
                    previous = current;
                    drain_events(*runtime.session(), renderer, feedback, audio);
                }
            }
            if (runtime.state() == DungeonRuntimeState::recovery_required) {
                if (inventory.is_open()) {
                    inventory.close();
                    fixed_step.clear_accumulator();
                }
                if (frame_input.keys.escape || window_close_requested) {
                    exit_requested = true;
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
                present_frame_and_maybe_capture(capture_path.has_value()
                    ? capture_path->c_str() : nullptr);
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
                exit_requested = true;
                continue;
            }
            dungeon::RequestResult death_continue_result =
                dungeon::RequestResult::rejected;
            if (death_gate.continue_death) {
                death_continue_result = runtime.request_death_continue();
                if (death_continue_result
                        != dungeon::RequestResult::rejected) {
                    previous = current;
                    current = session->snapshot();
                }
            }
            if (validation_continue
                    && death_continue_result
                        != dungeon::RequestResult::rejected) {
                stage11_validation_state.continue_requested = true;
            }
            if (!death_gate.forward_gameplay && window_close_requested) {
                exit_requested = true;
                continue;
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
                current = session->snapshot();
                previous = current;
                drain_events(*session, renderer, feedback, audio);
                if (runtime.state() != DungeonRuntimeState::running) {
                    inventory.close();
                    fixed_step.clear_accumulator();
                    inventory_toggled_this_frame = true;
                }
            }
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
                exit_requested = true;
                continue;
            }
            if (input_settings.revision != input_revision_before) {
                refresh_control_hints(control_hints, input_settings);
            }
            const bool pause_open = pause_menu.screen != PauseScreen::closed;
            const bool pause_blocks_gameplay = pause_open || pause_was_open;
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
                && host_gate.forward_gameplay && !pause_blocks_gameplay;
            const bool forward_movement = passive_input_gate.forward_movement
                && inventory_gate.forward_movement
                && death_gate.forward_gameplay
                && host_gate.forward_gameplay && !pause_blocks_gameplay;
            const bool forward_descent = passive_input_gate.forward_descent
                && inventory_gate.forward_descent
                && death_gate.forward_gameplay
                && host_gate.forward_gameplay && !pause_blocks_gameplay;
            if (forward_actions && inventory_gate.forward_room_reset
                && frame_input.keys.reset) {
                const dungeon::RequestResult reset =
                    session->reset_current_room();
                if (reset != dungeon::RequestResult::rejected) {
                    current = session->snapshot();
                    previous = current;
                    drain_events(*session, renderer, feedback, audio);
                }
            }
            if (!pause_blocks_gameplay && death_gate.forward_gameplay
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
                    current = session->snapshot();
                }
            }
            if (forward_actions) {
                const SubmittedFrameActions submitted_actions =
                    submit_frame_actions(*session, frame_input);
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
                const auto snapshot = session->snapshot();
                const bool in_range = snapshot.combat.has_value()
                    && can_prompt_descent(
                        snapshot, snapshot.combat->player.position);
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
                        || config.stage11d_loot_validation
                            != Stage11DLootValidationScenario::none
// STAGE11D_LOOT_VALIDATION_SEAM_END fixed_step
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
                        step_movement = stage11_validation_input(*session,
                            current, config, stage11_validation_state);
                    } else if (config.stage10_validation
                            != Stage10ValidationScenario::none) {
                        step_movement = stage10_validation_input(*session,
                            current, config, stage10_validation_state);
                    } else {
                        step_movement = movement;
                    }
                }
                runtime.fixed_tick(step_movement,
                    loot_pickup_policy(live_settings.loot_filter_mode));
                ++stage11b_validation_state.fixed_ticks;
                current = session->snapshot();
// STAGE11D_LOOT_VALIDATION_SEAM_BEGIN abyss_claim
                if (stage11d_validation_state.abyss_claim_requested) {
                    bool still_ground = false;
                    for (std::size_t index = 0U;
                         index < current.ground_item_count; ++index) {
                        still_ground = still_ground
                            || current.ground_items[index].item_id
                                == stage11d_validation_state.abyss_item_id;
                    }
                    bool now_owned = false;
                    for (const auto& item : session->item_state().items) {
                        now_owned = now_owned
                            || item.id == stage11d_validation_state.abyss_item_id;
                    }
                    stage11d_validation_state.abyss_claimed =
                        !still_ground && now_owned;
                }
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
                drain_events(*session, renderer, feedback, audio);
                if (stage10_validation_reached(
                        current, config, stage10_validation_state)) {
                    break;
                }
                if (stage11_validation_reached(
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

            // Observe after all possible fixed-step changes and before every
            // presented frame, including death/recovery-owned overlay frames.
            const HudPresentedFrame hud_presented_frame = current.death.has_value()
                ? HudPresentedFrame::death_overlay : HudPresentedFrame::normal;
            renderer.observe_presented_hud_frame(hud_presented_frame,
                previous, current, runtime.render_status(), control_hints,
                frame_seconds, pause_blocks_gameplay);
            const bool stage11c_target_visible = stage11c_hud_validation_reached(
                current, config.stage11c_hud_validation,
                stage11c_validation_state, draw_debug);
            if (stage11c_target_visible) {
                ++stage11c_validation_state.target_presented_frames;
                stage11c_validation_state.production_snapshot_hash =
                    stage11c_production_snapshot_hash(current);
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
            const settings::LootFilterMode presented_loot_filter =
                renderer_loot_filter_mode(
                    pause_menu.screen, live_settings, pause_menu.draft);
            renderer.set_loot_filter_mode(presented_loot_filter);
            BeginDrawing();
            ClearBackground(Color{13, 17, 27, 255});
            dungeon::DungeonSnapshot presented_snapshot = current;
            if (config.stage12_material_showcase) {
                apply_stage12_material_showcase(presented_snapshot);
            }
            const GroundLootView ground_loot_view = renderer.draw(
                previous, presented_snapshot, runtime.render_status(),
                static_cast<float>(frame.interpolation_alpha), draw_debug,
                feedback, audio_ready);
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
            if (passive_overlay_open) {
                draw_passive_tree_overlay(current, runtime.render_status());
            }
            if (inventory.is_open()) {
                inventory.draw(*session, current, runtime.render_status());
            }
            if (stage11b_validation_state.resume_observed) {
                stage11b_validation_state.resume_ticks_after =
                    stage11b_validation_state.fixed_ticks;
            }
            if (pause_menu.screen != PauseScreen::closed) {
                pause_menu_renderer.draw(pause_menu);
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
                        stage11b_snapshot_hash(current);
                }
                ++stage11b_validation_state.paused_presented;
                stage11b_validation_state.paused_ticks_after =
                    stage11b_validation_state.fixed_ticks;
                stage11b_validation_state.player_monster_hash_after =
                    stage11b_snapshot_hash(current);
            }
            const bool stage10_target_visible = stage10_validation_reached(
                current, config, stage10_validation_state);
            const bool stage11_target_visible = stage11_validation_reached(
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
            const bool stage11b_reached = stage11b_validation_complete(
                config, stage11b_validation_state, pause_menu);
            const bool stage11c_reached = stage11c_target_visible
                && stage11c_validation_state.target_presented_frames >= 4U;
// STAGE11D_LOOT_VALIDATION_SEAM_BEGIN reached
            const bool stage11d_reached = stage11d_validation_state.captured
                && (config.stage11d_loot_validation
                        != Stage11DLootValidationScenario::rare_only_abyss
                    || stage11d_validation_state.abyss_claimed);
// STAGE11D_LOOT_VALIDATION_SEAM_END reached
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
                || stage11d_reached;
// STAGE11D_LOOT_VALIDATION_SEAM_END reached_merge
// STAGE11D_LOOT_VALIDATION_SEAM_BEGIN visible_capture
            const bool loot_validation_visible_capture =
                stage11d_validation_state.target_visible;
            const bool no_loot_validation_active =
                config.stage11d_loot_validation
                    == Stage11DLootValidationScenario::none;
// STAGE11D_LOOT_VALIDATION_SEAM_END visible_capture
            std::optional<std::string> capture_path{};
            bool captured_stage10_target = false;
            if ((validation_reached || loot_validation_visible_capture
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
            present_frame_and_maybe_capture(capture_path.has_value()
                ? capture_path->c_str() : nullptr);
            ++presented_frame_count;
            if (captured_stage10_target && stage11c_reached) {
                stage11c_validation_state.captured = true;
            }
// STAGE11D_LOOT_VALIDATION_SEAM_BEGIN captured
            if (captured_stage10_target
                    && stage11d_validation_state.target_visible) {
                stage11d_validation_state.captured = true;
            }
// STAGE11D_LOOT_VALIDATION_SEAM_END captured
            stage10_validation_captured = stage10_validation_captured
                || captured_stage10_target;
            if (config.validation_exit_after_presented_frames != 0U
                    && presented_frame_count
                        >= config.validation_exit_after_presented_frames) {
                exit_requested = true;
            }
            if (validation_reached
                    && (!config.validation_capture_file.has_value()
                        || stage10_validation_captured)) {
                exit_requested = true;
            }
        }
        write_stage11b_validation_summary(config, stage11b_validation_state,
            pause_menu);
        write_stage11c_hud_validation_summary(config,
            stage11c_validation_state);
// STAGE11D_LOOT_VALIDATION_SEAM_BEGIN summary
        write_stage11d_loot_validation_summary(config,
            stage11d_validation_state, pause_menu);
// STAGE11D_LOOT_VALIDATION_SEAM_END summary
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
