#include "dungeon_view_math.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace arpg::platform {
namespace {

constexpr float kTransitionSeconds = 0.12F;
constexpr float kTwoPi = 6.28318530717958647692F;

constexpr Rgba8 kFireFrame{236U, 92U, 54U, 255U};
constexpr Rgba8 kWaterFrame{64U, 156U, 236U, 255U};
constexpr Rgba8 kLightningFrame{236U, 218U, 72U, 255U};
constexpr Rgba8 kChaosFrame{154U, 76U, 210U, 255U};

}  // namespace

DoorVisualMode door_visual_mode(
    dungeon::RoomPhase phase,
    bool has_active_room,
    bool exits_open) noexcept {
    if (!has_active_room || phase == dungeon::RoomPhase::transitioning) {
        return DoorVisualMode::hidden;
    }
    if (exits_open) {
        return DoorVisualMode::open;
    }
    return DoorVisualMode::closed;
}

DoorTheme door_theme(dungeon::ExitDirection direction) noexcept {
    switch (direction) {
    case dungeon::ExitDirection::up:
        return {dungeon::DungeonElement::fire, "FIRE", "\xE2\x86\x91", kFireFrame};
    case dungeon::ExitDirection::down:
        return {dungeon::DungeonElement::water, "WATER", "\xE2\x86\x93", kWaterFrame};
    case dungeon::ExitDirection::left:
        return {dungeon::DungeonElement::lightning, "LIGHTNING", "\xE2\x86\x90", kLightningFrame};
    case dungeon::ExitDirection::right:
        return {dungeon::DungeonElement::chaos, "CHAOS", "\xE2\x86\x92", kChaosFrame};
    case dungeon::ExitDirection::none:
        return {dungeon::DungeonElement::chaos, "UNKNOWN", "?", kChaosFrame};
    }
    return {dungeon::DungeonElement::chaos, "UNKNOWN", "?", kChaosFrame};
}

DoorArrowGeometry door_arrow_geometry(dungeon::ExitDirection direction,
    float center_x, float center_y, float scale) noexcept {
    const float safe_scale = std::max(0.0F, scale);
    Vector2 outward{};
    switch (direction) {
    case dungeon::ExitDirection::up:
        outward = {0.0F, -1.0F};
        break;
    case dungeon::ExitDirection::down:
        outward = {0.0F, 1.0F};
        break;
    case dungeon::ExitDirection::left:
        outward = {-1.0F, 0.0F};
        break;
    case dungeon::ExitDirection::right:
        outward = {1.0F, 0.0F};
        break;
    case dungeon::ExitDirection::none:
        outward = {0.0F, -1.0F};
        break;
    }
    const Vector2 perpendicular{-outward.y, outward.x};
    const float shaft = 14.0F * safe_scale;
    const float tip = 20.0F * safe_scale;
    const float head_length = 9.0F * safe_scale;
    const float head_width = 7.0F * safe_scale;
    const Vector2 center{center_x, center_y};
    const Vector2 arrow_tip{center.x + outward.x * tip,
        center.y + outward.y * tip};
    return {
        {center.x - outward.x * shaft, center.y - outward.y * shaft},
        arrow_tip,
        {arrow_tip.x - outward.x * head_length + perpendicular.x * head_width,
            arrow_tip.y - outward.y * head_length + perpendicular.y * head_width},
        {arrow_tip.x - outward.x * head_length - perpendicular.x * head_width,
            arrow_tip.y - outward.y * head_length - perpendicular.y * head_width},
        2.5F * safe_scale,
    };
}

bool abyss_door_marker(
    const dungeon::DungeonSnapshot& snapshot,
    dungeon::ExitDirection direction) noexcept {
    const auto index = static_cast<std::size_t>(direction);
    return index < snapshot.abyss_doors.size()
        && snapshot.abyss_doors[index];
}

EnvironmentHazardVisual environment_hazard_visual(
    const combat::HazardSnapshot& hazard) noexcept {
    if (!hazard.active
            || hazard.source != combat::HazardSource::abyss_environment
            || hazard.radius <= 0.0F) {
        return {};
    }

    Rgba8 active_outline{};
    Rgba8 active_fill{};
    switch (hazard.kind) {
    case combat::HazardKind::thunderstorm:
        active_outline = {132U, 211U, 255U, 235U};
        active_fill = {65U, 149U, 230U, 76U};
        break;
    case combat::HazardKind::hunting_flame:
        active_outline = {255U, 91U, 48U, 235U};
        active_fill = {224U, 49U, 30U, 88U};
        break;
    case combat::HazardKind::chaos_expansion:
        active_outline = {221U, 62U, 188U, 235U};
        active_fill = {145U, 25U, 126U, 84U};
        break;
    default:
        return {};
    }

    const bool warning = hazard.telegraph_ticks != 0U;
    return {
        warning ? EnvironmentHazardVisualMode::warning
                : EnvironmentHazardVisualMode::active,
        hazard.center,
        hazard.radius,
        warning ? Rgba8{255U, 203U, 91U, 48U} : active_fill,
        warning ? Rgba8{255U, 203U, 91U, 235U} : active_outline,
    };
}

namespace {

const char* abyss_danger_label(abyss::AbyssDanger danger) noexcept {
    switch (danger) {
    case abyss::AbyssDanger::low: return "ABYSS LOW";
    case abyss::AbyssDanger::medium: return "ABYSS MED";
    case abyss::AbyssDanger::high: return "ABYSS HIGH";
    }
    return "ABYSS";
}

const char* abyss_rule_label(abyss::AbyssRuleId rule) noexcept {
    switch (rule) {
    case abyss::AbyssRuleId::thunderstorm: return "THUNDERSTORM";
    case abyss::AbyssRuleId::hunting_flames: return "HUNTING FLAMES";
    case abyss::AbyssRuleId::chaos_expansion: return "CHAOS EXPANSION";
    case abyss::AbyssRuleId::swift_pursuit: return "SWIFT PURSUIT";
    case abyss::AbyssRuleId::abyss_bulwark: return "ABYSS BULWARK";
    case abyss::AbyssRuleId::abyss_fury: return "ABYSS FURY";
    case abyss::AbyssRuleId::heavy_steps: return "HEAVY STEPS";
    case abyss::AbyssRuleId::exhausted_recovery:
        return "EXHAUSTED RECOVERY";
    case abyss::AbyssRuleId::life_sacrifice: return "LIFE SACRIFICE";
    case abyss::AbyssRuleId::none: return "NO RULE";
    }
    return "NO RULE";
}

const char* abyss_effect_label(abyss::AbyssRuleId rule) noexcept {
    switch (rule) {
    case abyss::AbyssRuleId::thunderstorm:
        return "Every 180t: warn 45t/radius 0.8, hit 15% max HP lightning";
    case abyss::AbyssRuleId::hunting_flames:
        return "Every 240t: warn 45t/radius 1.0, burn 10% max HP/60t for 180t";
    case abyss::AbyssRuleId::chaos_expansion:
        return "Radius 1.0/2.3/3.6/4.9/6.2 per 180t, 8% max HP chaos/60t";
    case abyss::AbyssRuleId::swift_pursuit:
        return "Monsters: move +15%, cooldown x85%";
    case abyss::AbyssRuleId::abyss_bulwark:
        return "Monsters: armor x130%, shield +30% max HP";
    case abyss::AbyssRuleId::abyss_fury:
        return "Monsters: damage and attack speed x145%";
    case abyss::AbyssRuleId::heavy_steps:
        return "Player: ground move x85%";
    case abyss::AbyssRuleId::exhausted_recovery:
        return "Player: entry resources and future recovery x70%";
    case abyss::AbyssRuleId::life_sacrifice:
        return "Player: max HP x55%";
    case abyss::AbyssRuleId::none:
        return "";
    }
    return "";
}

}  // namespace

AbyssHudValues abyss_hud_values(
    const dungeon::DungeonSnapshot& snapshot) noexcept {
    AbyssHudValues values{};
    values.visible = snapshot.is_abyss
        && snapshot.abyss_rule != abyss::AbyssRuleId::none;
    values.danger_label = abyss_danger_label(snapshot.abyss_danger);
    values.rule_label = abyss_rule_label(snapshot.abyss_rule);
    values.effect_label = abyss_effect_label(snapshot.abyss_rule);
    values.pending_rewards = snapshot.abyss_pending_rewards;
    values.unpicked_rewards = snapshot.abyss_unpicked_rewards;
    values.confirmation_visible = values.visible
        && snapshot.abyss_exit_confirmation_armed;
    values.confirmation_transition =
        snapshot.abyss_exit_confirmation_transition;
    values.confirmation_direction =
        snapshot.abyss_exit_confirmation_direction;
    if (values.confirmation_transition == dungeon::TransitionKind::descent) {
        values.confirmation_label =
            "Press E again to abandon remaining rewards and descend";
    } else {
        values.confirmation_label =
            "Touch the SAME door again to abandon ALL remaining rewards";
    }
    return values;
}

HoleVisualMode hole_visual_mode(
    const dungeon::DungeonSnapshot& snapshot) noexcept {
    if (!snapshot.has_active_room || !snapshot.has_hole) {
        return HoleVisualMode::hidden;
    }
    switch (snapshot.phase) {
    case dungeon::RoomPhase::locked:
        return HoleVisualMode::sealed;
    case dungeon::RoomPhase::combat:
        return snapshot.exits_unlocked
            ? HoleVisualMode::ready : HoleVisualMode::sealed;
    case dungeon::RoomPhase::cleared:
    case dungeon::RoomPhase::awaiting_exit:
        return snapshot.exits_unlocked
            ? HoleVisualMode::ready : HoleVisualMode::sealed;
    case dungeon::RoomPhase::committing:
        return HoleVisualMode::busy;
    case dungeon::RoomPhase::faulted:
        return HoleVisualMode::faulted;
    case dungeon::RoomPhase::transitioning:
        return HoleVisualMode::hidden;
    }
    return HoleVisualMode::hidden;
}

bool player_in_hole_range(
    combat::Vec3 position,
    combat::Vec3 center,
    float radius) noexcept {
    if (radius < 0.0F) {
        return false;
    }
    const float x = position.x - center.x;
    const float y = position.y - center.y;
    return x * x + y * y <= radius * radius;
}

bool can_prompt_descent(
    const dungeon::DungeonSnapshot& snapshot,
    combat::Vec3 player_position) noexcept {
    return hole_visual_mode(snapshot) == HoleVisualMode::ready
        && player_in_hole_range(
            player_position, kHoleCenter, kHoleInteractionRadius);
}

HoleInteractionPrompt hole_interaction_prompt(
    const char* interact_binding_label) noexcept {
    HoleInteractionPrompt prompt{};
    const char* const label = interact_binding_label != nullptr
            && interact_binding_label[0] != '\0'
        ? interact_binding_label : "E";
    static_cast<void>(std::snprintf(
        prompt.data(), prompt.size(), "%s: DESCEND", label));
    prompt.back() = '\0';
    return prompt;
}

Rgba8 ecosystem_tint(dungeon::DungeonElement element) noexcept {
    switch (element) {
    case dungeon::DungeonElement::fire:
        return {88U, 43U, 34U, 255U};
    case dungeon::DungeonElement::water:
        return {35U, 70U, 104U, 255U};
    case dungeon::DungeonElement::lightning:
        return {96U, 90U, 34U, 255U};
    case dungeon::DungeonElement::chaos:
        return {73U, 43U, 99U, 255U};
    }
    return {73U, 43U, 99U, 255U};
}

float abyss_pulse_alpha(float elapsed_seconds) noexcept {
    if (!std::isfinite(elapsed_seconds)) {
        return 0.5F;
    }
    const float phase = std::fmod(elapsed_seconds, 1.0F);
    const float alpha = 0.5F + 0.5F * std::sin(kTwoPi * phase);
    return std::clamp(alpha, 0.0F, 1.0F);
}

const char* save_indicator_label(SaveIndicator indicator) noexcept {
    switch (indicator) {
    case SaveIndicator::none:
        return "";
    case SaveIndicator::saving:
        return "SAVING";
    case SaveIndicator::saved:
        return "SAVED";
    case SaveIndicator::recovered:
        return "RECOVERED";
    case SaveIndicator::error:
        return "SAVE ERROR";
    }
    return "";
}

bool recovery_requested(
    bool runtime_recovery_required,
    bool n_pressed) noexcept {
    return runtime_recovery_required && n_pressed;
}

bool can_interpolate_room(
    const dungeon::DungeonSnapshot& previous,
    const dungeon::DungeonSnapshot& current) noexcept {
    return previous.has_active_room && current.has_active_room
        && previous.combat.has_value() && current.combat.has_value()
        && previous.room_index == current.room_index
        && previous.room_seed == current.room_seed
        && previous.room_instance_generation
            == current.room_instance_generation
        && previous.death.has_value() == current.death.has_value()
        && previous.phase != dungeon::RoomPhase::death_pending
        && current.phase != dungeon::RoomPhase::death_pending;
}

bool dungeon_event_clears_transients(
    dungeon::DungeonEventKind kind) noexcept {
    return kind == dungeon::DungeonEventKind::room_destroyed
        || kind == dungeon::DungeonEventKind::room_reset;
}

const char* room_phase_label(dungeon::RoomPhase phase) noexcept {
    switch (phase) {
    case dungeon::RoomPhase::locked:
        return "LOCKED";
    case dungeon::RoomPhase::combat:
        return "COMBAT";
    case dungeon::RoomPhase::wave_delay:
        return "WAVE DELAY";
    case dungeon::RoomPhase::cleared:
        return "CLEARED";
    case dungeon::RoomPhase::awaiting_exit:
        return "AWAITING EXIT";
    case dungeon::RoomPhase::committing:
        return "COMMITTING";
    case dungeon::RoomPhase::transitioning:
        return "TRANSITIONING";
    case dungeon::RoomPhase::faulted:
        return "FAULTED";
    }
    return "UNKNOWN";
}

const char* exit_direction_label(dungeon::ExitDirection direction) noexcept {
    switch (direction) {
    case dungeon::ExitDirection::up:
        return "UP";
    case dungeon::ExitDirection::down:
        return "DOWN";
    case dungeon::ExitDirection::left:
        return "LEFT";
    case dungeon::ExitDirection::right:
        return "RIGHT";
    case dungeon::ExitDirection::none:
        return "NONE";
    }
    return "UNKNOWN";
}

TransitionVisualState transition_after_dungeon_event(
    TransitionVisualState state,
    dungeon::DungeonEventKind kind) noexcept {
    if (kind == dungeon::DungeonEventKind::room_destroyed) {
        state.seconds_left = kTransitionSeconds;
    } else if (kind == dungeon::DungeonEventKind::room_reset) {
        state = {};
    }
    return state;
}

TransitionVisualState transition_after_room_phase(
    TransitionVisualState state,
    dungeon::RoomPhase phase) noexcept {
    const bool transitioning = phase == dungeon::RoomPhase::transitioning;
    if (transitioning && !state.transition_phase_seen) {
        state.seconds_left = kTransitionSeconds;
    }
    state.transition_phase_seen = transitioning;
    return state;
}

TransitionVisualState advance_transition(
    TransitionVisualState state,
    float frame_seconds) noexcept {
    state.seconds_left = std::max(
        0.0F,
        state.seconds_left - std::clamp(frame_seconds, 0.0F, 0.1F));
    return state;
}

float transition_overlay_alpha(float seconds_left) noexcept {
    return std::clamp(seconds_left / kTransitionSeconds, 0.0F, 1.0F);
}

ProgressionHudValues progression_hud_values(
    const dungeon::DungeonSnapshot& snapshot,
    const progression::ProgressionRules& rules) noexcept {
    ProgressionHudValues values{};
    values.level = snapshot.progression.level;
    values.experience = snapshot.progression.experience;
    values.unspent_passive_points =
        snapshot.progression.unspent_passive_points;
    values.pending_room_experience = snapshot.pending_room_experience;
    values.maximum_level = snapshot.progression.level
        >= progression::kMaximumLevel;
    if (!values.maximum_level && snapshot.progression.level != 0U) {
        values.required_experience = rules.experience_to_next[
            static_cast<std::size_t>(snapshot.progression.level - 1U)];
    }
    return values;
}

}  // namespace arpg::platform
