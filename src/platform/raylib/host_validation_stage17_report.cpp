#include "host_validation_stage17.hpp"

#include "raylib_host.hpp"

#include <raylib.h>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <string>

namespace arpg::platform::host_validation {
namespace {

[[nodiscard]] bool stage17_draw_runtime_valid(
    const Stage17SkillDrawRuntimeEvidence& evidence) noexcept {
    return evidence.samples > 0U && evidence.valid
        && evidence.material_frame_drawn && !evidence.base_player_drawn
        && evidence.procedural_main_visual_peak == 0U;
}


[[nodiscard]] const char* stage17_capture_stem(
    const Stage17Capture capture) noexcept {
    switch (capture) {
    case Stage17Capture::initial: return "01-new-default-";
    case Stage17Capture::draw_windup: return "02-draw-slash-windup-";
    case Stage17Capture::draw_hit: return "03-draw-slash-hit-";
    case Stage17Capture::skill_empty:
        return "poe2-inventory-skill-empty-";
    case Stage17Capture::skill_removed:
        return "poe2-inventory-skill-removed-";
    case Stage17Capture::skill_selected:
        return "poe2-inventory-skill-selected-";
    case Stage17Capture::skill_swapped:
        return "poe2-inventory-skill-swapped-";
    case Stage17Capture::storm_array: return "04-storm-ground-array-";
    case Stage17Capture::storm_aerial: return "05-storm-aerial-array-";
    case Stage17Capture::storm_finisher: return "06-storm-finisher-";
    case Stage17Capture::restarted: return "07-restarted-loadout-";
    case Stage17Capture::none: break;
    }
    return nullptr;
}

[[nodiscard]] std::string stage17_resolution_name(
    const RaylibHostConfig& config) {
    return std::to_string(config.window_width) + 'x'
        + std::to_string(config.window_height);
}

[[nodiscard]] std::string stage17_capture_name(
    const RaylibHostConfig& config, const Stage17Capture capture) {
    const char* const stem = stage17_capture_stem(capture);
    if (stem == nullptr) return {};
    return std::string{stem} + stage17_resolution_name(config) + ".png";
}

[[nodiscard]] bool stage17_legacy_capture(
    const Stage17Capture capture) noexcept {
    return capture == Stage17Capture::initial
        || capture == Stage17Capture::draw_windup
        || capture == Stage17Capture::draw_hit;
}

[[nodiscard]] bool stage17_legacy_resolution(
    const RaylibHostConfig& config) noexcept {
    return config.window_width == 1280 && config.window_height == 720;
}

void complete_stage17_capture(Stage17SkillStonesValidationState& state,
    const bool skipped) noexcept {
    switch (state.capture_pending) {
    case Stage17Capture::initial:
        state.initial_captured = !skipped;
        state.initial_skipped = skipped;
        state.step = Stage17ValidationStep::empty_slot_3;
        break;
    case Stage17Capture::draw_windup:
        state.draw_windup_captured = !skipped;
        state.draw_windup_skipped = skipped;
        break;
    case Stage17Capture::draw_hit:
        state.draw_captured = !skipped;
        state.draw_hit_skipped = skipped;
        break;
    case Stage17Capture::skill_empty:
        if (!skipped) state.skill_empty_captured = true;
        state.step = Stage17ValidationStep::remove_slot_1;
        break;
    case Stage17Capture::skill_removed:
        if (!skipped) state.skill_removed_captured = true;
        state.step = Stage17ValidationStep::select_draw_inventory;
        break;
    case Stage17Capture::skill_selected:
        if (!skipped) state.skill_selected_captured = true;
        state.step = Stage17ValidationStep::equip_slot_5;
        break;
    case Stage17Capture::skill_swapped:
        if (!skipped) state.skill_swapped_captured = true;
        state.step = Stage17ValidationStep::close_inventory;
        break;
    case Stage17Capture::storm_array:
        if (!skipped) state.storm_array_captured = true;
        break;
    case Stage17Capture::storm_aerial:
        if (!skipped) state.storm_aerial_captured = true;
        break;
    case Stage17Capture::storm_finisher:
        if (!skipped) state.storm_finisher_captured = true;
        break;
    case Stage17Capture::restarted:
        if (!skipped) state.restart_captured = true;
        state.step = Stage17ValidationStep::complete;
        break;
    case Stage17Capture::none:
        break;
    }
    state.capture_pending = Stage17Capture::none;
}

void skip_stage17_capture_without_file(
    Stage17SkillStonesValidationState& state) noexcept {
    if (stage17_legacy_capture(state.capture_pending)) {
        complete_stage17_capture(state, true);
    }
}

void write_stage17_capture_field(std::ostream& stream, const char* prefix,
    const RaylibHostConfig& config, const Stage17Capture capture,
    const bool captured, const bool skipped) {
    stream << prefix << "_captured=" << (captured ? 1 : 0) << '\n'
           << prefix << "_skipped=" << (skipped ? 1 : 0) << '\n'
           << prefix << "_capture_file="
           << (captured ? stage17_capture_name(config, capture) : "N/A")
           << '\n';
}

}  // namespace

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
            && !state.initial_captured && !state.initial_skipped
            && state.presented_frames >= 2U) {
        state.capture_pending = Stage17Capture::initial;
    }
    if (config.stage17_skill_stones_validation
            == Stage17SkillStonesValidationScenario::production_sequence
            && !stage17_legacy_resolution(config)
            && stage17_legacy_capture(state.capture_pending)) {
        skip_stage17_capture_without_file(state);
        return std::nullopt;
    }
    if (state.capture_pending == Stage17Capture::none
            || GetScreenWidth() != config.window_width
            || GetScreenHeight() != config.window_height) {
        return std::nullopt;
    }
    const std::string name = stage17_capture_name(config,
        state.capture_pending);
    if (name.empty()) return std::nullopt;
    return (*config.screenshot_directory / name).string();
}

void mark_stage17_capture_complete(
    Stage17SkillStonesValidationState& state) noexcept {
    complete_stage17_capture(state, false);
}

namespace {

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

}  // namespace

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
        const bool legacy_resolution = stage17_legacy_resolution(config);
        const bool scaled_production_resolution =
            (config.window_width == 800 && config.window_height == 450)
            || (config.window_width == 1920 && config.window_height == 1080);
        const bool legacy_captures_valid = legacy_resolution
            ? state.initial_captured && !state.initial_skipped
                && state.draw_windup_captured && !state.draw_windup_skipped
                && state.draw_captured && !state.draw_hit_skipped
            : scaled_production_resolution
                && !state.initial_captured && state.initial_skipped
                && !state.draw_windup_captured && state.draw_windup_skipped
                && !state.draw_captured && state.draw_hit_skipped;
        const bool skill_captures_valid = state.skill_empty_captured
            && state.skill_removed_captured && state.skill_selected_captured
            && state.skill_swapped_captured;
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
                && legacy_captures_valid && skill_captures_valid
                && state.draw_accepted
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
               << "window=" << stage17_resolution_name(config) << '\n'
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
        write_stage17_capture_field(stream, "initial", config,
            Stage17Capture::initial, state.initial_captured,
            state.initial_skipped);
        write_stage17_capture_field(stream, "draw_windup", config,
            Stage17Capture::draw_windup, state.draw_windup_captured,
            state.draw_windup_skipped);
        write_stage17_capture_field(stream, "draw_hit", config,
            Stage17Capture::draw_hit, state.draw_captured,
            state.draw_hit_skipped);
        write_stage17_capture_field(stream, "skill_empty", config,
            Stage17Capture::skill_empty, state.skill_empty_captured, false);
        write_stage17_capture_field(stream, "skill_removed", config,
            Stage17Capture::skill_removed, state.skill_removed_captured, false);
        write_stage17_capture_field(stream, "skill_selected", config,
            Stage17Capture::skill_selected, state.skill_selected_captured, false);
        write_stage17_capture_field(stream, "skill_swapped", config,
            Stage17Capture::skill_swapped, state.skill_swapped_captured, false);
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

}  // namespace arpg::platform::host_validation
