#include "raylib_host.hpp"

#include "combat_audio.hpp"
#include "combat_feedback.hpp"
#include "combat_renderer.hpp"
#include "control_hints.hpp"
#include "core/fixed_step.hpp"
#include "dungeon_runtime.hpp"
#include "dungeon_view_math.hpp"
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
    CombatFeedback& feedback, CombatAudio& audio) noexcept {
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
            audio.stop_all();
        }
    }
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

std::optional<std::string> host_screenshot_path() noexcept {
    try {
        return (std::filesystem::path{
            GetApplicationDirectory()} / "stage8-equipment-loot.png").string();
    } catch (...) {
        TraceLog(LOG_WARNING, "failed to construct screenshot path");
        return std::nullopt;
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
    std::uint32_t old_attack_count{};
    std::uint32_t new_attack_count{};
    bool old_attack_checked{};
    settings::SettingsLoadStatus load_status{
        settings::SettingsLoadStatus::defaults_missing};
};

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
    // Formal runs must be deterministic even when the desktop test runner has
    // unrelated keys held.  This is still a physical snapshot: the scenario
    // below adds only StableKey/escape/enter edges before the normal mapper.
    snapshot.down.fill(false);
    snapshot.pressed.fill(false);
    snapshot.escape = false;
    snapshot.enter = false;
    snapshot.f1 = false;
    snapshot.f12 = false;
    snapshot.v = false;
    snapshot.mouse_left = false;
    snapshot.mouse_right = false;
    snapshot.mouse_wheel = 0.0F;
    const std::uint32_t frame = ++state.injected_frame;
    switch (config.stage11b_validation) {
    case Stage11BValidationScenario::paused_freeze:
        if (frame == 1U) snapshot.escape = true;
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
        else if (frame == 2U || (frame >= 4U && frame <= 10U)
            || (frame >= 13U && frame <= 19U)) {
            inject_stage11b_pressed(snapshot, settings::StableKey::arrow_down);
        } else if (frame == 3U || frame == 11U || frame == 20U) {
            snapshot.enter = true;
        } else if (frame == 12U) {
            inject_stage11b_pressed(snapshot,
                config.stage11b_validation == Stage11BValidationScenario::rebound_attack
                    ? settings::StableKey::u : settings::StableKey::k);
        } else if (config.stage11b_validation
                       == Stage11BValidationScenario::rebound_attack
                   && (frame == 21U || frame == 22U)) {
            snapshot.escape = true;
        } else if (config.stage11b_validation
                       == Stage11BValidationScenario::rebound_attack
                   && frame == 23U) {
            inject_stage11b_pressed(snapshot, settings::StableKey::j);
        } else if (config.stage11b_validation
                       == Stage11BValidationScenario::rebound_attack
                   && frame == 24U) {
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
        return state.paused_presented >= 120U;
    case Stage11BValidationScenario::settings_page:
    case Stage11BValidationScenario::restarted_settings:
    case Stage11BValidationScenario::single_slot_recovery:
    case Stage11BValidationScenario::corrupt_defaults:
        return state.injected_frame >= 4U
            && pause_menu.screen == PauseScreen::settings;
    case Stage11BValidationScenario::rebound_attack:
        return state.injected_frame >= 24U && state.old_attack_checked
            && state.new_attack_count != 0U
            && pause_menu.screen == PauseScreen::closed;
    case Stage11BValidationScenario::conflict_swap:
        return state.injected_frame >= 20U
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
    const Stage11BValidationState& state, const PauseMenuState& pause_menu,
    const dungeon::DungeonSnapshot& snapshot) noexcept {
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
               << "player_monster_hash=" << stage11b_snapshot_hash(snapshot) << '\n'
               << "committed_revision=" << pause_menu.committed.revision << '\n'
               << "old_attack_count=" << state.old_attack_count << '\n'
               << "new_attack_count=" << state.new_attack_count << '\n'
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
        const LiveSettingsResult result = apply_live_settings(
            live_settings, pause_menu.draft, settings_backend);
        if (result == LiveSettingsResult::applied) {
            live_settings = pause_menu.draft;
            pause_menu.message = nullptr;
        } else {
            pause_menu.message = kSettingsPreviewFailed;
        }
        break;
    }
    case PauseCommand::apply: {
        const LiveSettingsResult preview = apply_live_settings(
            live_settings, pause_menu.draft, settings_backend);
        if (preview != LiveSettingsResult::applied) {
            const LiveSettingsResult rollback = rollback_live_settings(
                live_settings, pause_menu.committed, settings_backend);
            if (rollback == LiveSettingsResult::applied) {
                live_settings = pause_menu.committed;
                pause_menu.message = kSettingsPreviewFailed;
            } else {
                pause_menu.message = kSettingsRollbackFailed;
            }
            break;
        }
        const settings::SettingsData previewed = pause_menu.draft;
        live_settings = previewed;
        settings::SettingsData save_draft = previewed;
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
        core::FixedStepRunner fixed_step;
        CombatRenderer renderer;
        static_cast<void>(renderer.initialize_resources());
        CombatFeedback feedback;
        CombatAudio audio;
        InventoryRenderer inventory;
        const bool audio_ready = audio.initialize();
        if (audio_ready) {
            SetMasterVolume(master_volume_fraction(
                committed_settings.master_sfx_percent));
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
            const PhysicalKeySnapshot physical_keys = inject_stage11b_physical_edges(
                sampled_physical_keys, config, stage11b_validation_state);
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
                draw_recovery_screen(runtime.render_status());
                std::optional<std::string> capture_path =
                    validation_capture_path();
                if (frame_input.keys.f12 || frame_input.keys.v) {
                    capture_path = host_screenshot_path();
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
                config.stage11b_validation == Stage11BValidationScenario::none
                    ? !physical_keys.focus_lost : true,
            };
            PauseInput pause_input = pause_input_from_snapshot(
                physical_keys, escape_consumed,
                pause_menu.screen == PauseScreen::capture_binding);
            if (config.stage11b_validation != Stage11BValidationScenario::none) {
                // The scenario injects only keyboard edges.  A noninteractive
                // test desktop may report a focus transition even though its
                // real raylib window remains the input target; do not turn
                // that desktop artifact into a synthetic binding cancellation.
                pause_input.focus_lost = false;
            }
            const PauseCommand pause_command = update_pause_menu(
                pause_menu, pause_context, pause_input);
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
                submit_frame_actions(*session, frame_input);
            }
            if (config.stage11b_validation
                    == Stage11BValidationScenario::rebound_attack) {
                if (stage11b_validation_state.injected_frame == 23U) {
                    stage11b_validation_state.old_attack_checked = true;
                    if (forward_actions && frame_input.combat_actions[0]) {
                        ++stage11b_validation_state.old_attack_count;
                    }
                } else if (stage11b_validation_state.injected_frame == 24U
                    && forward_actions && frame_input.combat_actions[0]) {
                    ++stage11b_validation_state.new_attack_count;
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
                            != Stage11ValidationScenario::none) {
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
                runtime.fixed_tick(step_movement);
                ++stage11b_validation_state.fixed_ticks;
                current = session->snapshot();
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

            BeginDrawing();
            ClearBackground(Color{13, 17, 27, 255});
            renderer.draw(previous, current, runtime.render_status(),
                static_cast<float>(frame.interpolation_alpha), draw_debug,
                feedback, audio_ready, control_hints);
            if (passive_overlay_open) {
                draw_passive_tree_overlay(current, runtime.render_status());
            }
            if (inventory.is_open()) {
                inventory.draw(*session, current, runtime.render_status());
            }
            if (pause_menu.screen != PauseScreen::closed) {
                draw_pause_menu(pause_menu);
            }
            if (config.stage11b_validation
                    == Stage11BValidationScenario::paused_freeze
                && pause_menu.screen != PauseScreen::closed) {
                if (stage11b_validation_state.paused_presented == 0U) {
                    stage11b_validation_state.paused_ticks_before =
                        stage11b_validation_state.fixed_ticks;
                }
                ++stage11b_validation_state.paused_presented;
                stage11b_validation_state.paused_ticks_after =
                    stage11b_validation_state.fixed_ticks;
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
            const bool stage11b_visible_capture =
                (config.stage11b_validation == Stage11BValidationScenario::rebound_attack
                    || config.stage11b_validation == Stage11BValidationScenario::conflict_swap)
                && stage11b_validation_state.injected_frame == 20U;
            const bool validation_reached = stage10_reached || stage11_reached
                || stage11b_reached;
            std::optional<std::string> capture_path{};
            bool captured_stage10_target = false;
            if ((validation_reached || stage11b_visible_capture)
                    && !stage10_validation_captured
                    && config.validation_capture_file.has_value()) {
                capture_path = config.validation_capture_file->string();
                captured_stage10_target = true;
            }
            if (death_gate.screenshot) {
                capture_path = host_screenshot_path();
                captured_stage10_target = false;
            } else if (!capture_path.has_value()) {
                capture_path = validation_capture_path();
            }
            present_frame_and_maybe_capture(capture_path.has_value()
                ? capture_path->c_str() : nullptr);
            ++presented_frame_count;
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
            pause_menu, current);
        audio.shutdown();
        renderer.shutdown_resources();
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
