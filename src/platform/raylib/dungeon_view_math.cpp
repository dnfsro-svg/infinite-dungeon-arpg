#include "dungeon_view_math.hpp"

#include "ui_utf8.hpp"

#include <algorithm>
#include <cmath>

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
        return {dungeon::DungeonElement::fire, u8"火焰", "\xE2\x86\x91", kFireFrame};
    case dungeon::ExitDirection::down:
        return {dungeon::DungeonElement::water, u8"水流", "\xE2\x86\x93", kWaterFrame};
    case dungeon::ExitDirection::left:
        return {dungeon::DungeonElement::lightning, u8"闪电", "\xE2\x86\x90", kLightningFrame};
    case dungeon::ExitDirection::right:
        return {dungeon::DungeonElement::chaos, u8"混沌", "\xE2\x86\x92", kChaosFrame};
    case dungeon::ExitDirection::none:
        return {dungeon::DungeonElement::chaos, u8"未知", "?", kChaosFrame};
    }
    return {dungeon::DungeonElement::chaos, u8"未知", "?", kChaosFrame};
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
    case abyss::AbyssDanger::low: return u8"低危";
    case abyss::AbyssDanger::medium: return u8"中危";
    case abyss::AbyssDanger::high: return u8"高危";
    }
    return u8"未知";
}

}  // namespace

const char* abyss_rule_label(abyss::AbyssRuleId rule) noexcept {
    switch (rule) {
    case abyss::AbyssRuleId::thunderstorm: return u8"雷暴";
    case abyss::AbyssRuleId::hunting_flames: return u8"猎杀之焰";
    case abyss::AbyssRuleId::chaos_expansion: return u8"混沌扩张";
    case abyss::AbyssRuleId::swift_pursuit: return u8"迅捷追猎";
    case abyss::AbyssRuleId::abyss_bulwark: return u8"深渊壁垒";
    case abyss::AbyssRuleId::abyss_fury: return u8"深渊狂怒";
    case abyss::AbyssRuleId::heavy_steps: return u8"沉重脚步";
    case abyss::AbyssRuleId::exhausted_recovery: return u8"疲惫恢复";
    case abyss::AbyssRuleId::life_sacrifice: return u8"生命献祭";
    case abyss::AbyssRuleId::none: return u8"无规则";
    }
    return u8"无规则";
}

namespace {

const char* abyss_effect_label(abyss::AbyssRuleId rule) noexcept {
    switch (rule) {
    case abyss::AbyssRuleId::thunderstorm:
        return u8"每180帧：预警45帧/半径0.8，造成15%最大生命闪电伤害";
    case abyss::AbyssRuleId::hunting_flames:
        return u8"每240帧：预警45帧/半径1.0，灼烧180帧，每60帧造成10%最大生命伤害";
    case abyss::AbyssRuleId::chaos_expansion:
        return u8"每180帧扩张至半径1.0/2.3/3.6/4.9/6.2，每60帧造成8%最大生命混沌伤害";
    case abyss::AbyssRuleId::swift_pursuit:
        return u8"怪物：移动速度+15%，冷却时间×85%";
    case abyss::AbyssRuleId::abyss_bulwark:
        return u8"怪物：护甲×130%，护盾+30%最大生命";
    case abyss::AbyssRuleId::abyss_fury:
        return u8"怪物：伤害与攻击速度×145%";
    case abyss::AbyssRuleId::heavy_steps:
        return u8"玩家：地面移动速度×85%";
    case abyss::AbyssRuleId::exhausted_recovery:
        return u8"玩家：入场资源及后续恢复×70%";
    case abyss::AbyssRuleId::life_sacrifice:
        return u8"玩家：最大生命×55%";
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
        values.confirmation_label = u8"再次交互，放弃剩余奖励并下潜";
    } else {
        values.confirmation_label =
            u8"离开后再次触碰同一出口以放弃全部剩余奖励";
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

const char* hole_visual_label(HoleVisualMode mode) noexcept {
    switch (mode) {
    case HoleVisualMode::hidden: return "";
    case HoleVisualMode::sealed: return u8"封闭";
    case HoleVisualMode::ready: return u8"可进入";
    case HoleVisualMode::busy: return u8"保存中";
    case HoleVisualMode::faulted: return u8"异常";
    }
    return u8"异常";
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
    const char* interact_binding_label,
    HoleInteractionAction action) noexcept {
    HoleInteractionPrompt prompt{};
    if (interact_binding_label == nullptr
            || interact_binding_label[0] == '\0') return prompt;
    static_cast<void>(ui_utf8_format(prompt.data(), prompt.size(),
        action == HoleInteractionAction::descend
            ? u8"%s：下潜" : u8"%s：进入下一层",
        interact_binding_label));
    return prompt;
}

WorldPromptStyle world_prompt_style() noexcept {
    return {
        {0x0BU, 0x10U, 0x19U, 255U},
        {248U, 246U, 238U, 255U},
        {194U, 229U, 255U, 255U},
        {255U, 210U, 118U, 255U},
        {255U, 152U, 152U, 255U},
    };
}

Rgba8 world_prompt_text_color(Rgba8 requested) noexcept {
    requested.a = 255U;
    const WorldPromptStyle style = world_prompt_style();
    const auto luma = [](Rgba8 color) noexcept {
        return 0.2126F * static_cast<float>(color.r)
            + 0.7152F * static_cast<float>(color.g)
            + 0.0722F * static_cast<float>(color.b);
    };
    const float requested_luma = luma(requested);
    const float backing_luma = luma(style.backing);
    const float lighter = (std::max)(requested_luma, backing_luma);
    const float darker = (std::min)(requested_luma, backing_luma);
    return (lighter + 12.75F) / (darker + 12.75F) >= 4.5F
        ? requested : style.primary;
}

WorldPromptLayout world_prompt_layout(
    Vector2 anchor, Vector2 measured,
    float screen_width, float screen_height) noexcept {
    WorldPromptLayout layout{};
    if (!(screen_width > 0.0F) || !(screen_height > 0.0F)
            || !(measured.x > 0.0F) || !(measured.y > 0.0F)) {
        return layout;
    }
    const float viewport_scale = std::clamp((std::min)(
        screen_width / 1280.0F, screen_height / 720.0F), 0.625F, 1.5F);
    const float padding_x = 8.0F * viewport_scale;
    const float padding_y = 5.0F * viewport_scale;
    const float available_width = screen_width - padding_x * 2.0F;
    const float available_height = screen_height - padding_y * 2.0F;
    if (!(available_width > 0.0F) || !(available_height > 0.0F)) {
        return layout;
    }
    const float text_scale = (std::min)({
        1.0F, available_width / measured.x, available_height / measured.y});
    constexpr float kMinimumReadableTextHeight = 8.0F;
    if (!(text_scale > 0.0F)
            || measured.y * text_scale < kMinimumReadableTextHeight) {
        return layout;
    }
    const Vector2 scaled_measured{
        measured.x * text_scale, measured.y * text_scale};
    const float backing_width = scaled_measured.x + padding_x * 2.0F;
    const float backing_height = scaled_measured.y + padding_y * 2.0F;
    const float backing_x = std::clamp(
        anchor.x - backing_width * 0.5F,
        0.0F, screen_width - backing_width);
    const float backing_y = std::clamp(anchor.y,
        0.0F, screen_height - backing_height);
    layout.visible = true;
    layout.text_scale = text_scale;
    layout.backing = {
        backing_x, backing_y, backing_width, backing_height};
    layout.measured = scaled_measured;
    layout.text_position = {
        backing_x + (backing_width - scaled_measured.x) * 0.5F,
        backing_y + (backing_height - scaled_measured.y) * 0.5F,
    };
    return layout;
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
        return u8"保存中";
    case SaveIndicator::saved:
        return u8"已保存";
    case SaveIndicator::recovered:
        return u8"已恢复";
    case SaveIndicator::error:
        return u8"存档错误";
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
        return u8"锁定";
    case dungeon::RoomPhase::combat:
        return u8"战斗中";
    case dungeon::RoomPhase::wave_delay:
        return u8"等待下一波";
    case dungeon::RoomPhase::cleared:
        return u8"已清理";
    case dungeon::RoomPhase::awaiting_exit:
        return u8"等待离开";
    case dungeon::RoomPhase::committing:
        return u8"保存中";
    case dungeon::RoomPhase::transitioning:
        return u8"切换房间";
    case dungeon::RoomPhase::death_pending:
        return u8"处理撤退中";
    case dungeon::RoomPhase::faulted:
        return u8"异常";
    }
    return u8"未知";
}

const char* exit_direction_label(dungeon::ExitDirection direction) noexcept {
    switch (direction) {
    case dungeon::ExitDirection::up:
        return u8"上方";
    case dungeon::ExitDirection::down:
        return u8"下方";
    case dungeon::ExitDirection::left:
        return u8"左方";
    case dungeon::ExitDirection::right:
        return u8"右方";
    case dungeon::ExitDirection::none:
        return u8"无方向";
    }
    return u8"未知";
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
