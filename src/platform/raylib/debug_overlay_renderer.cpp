#include "debug_overlay_renderer.hpp"

#include "ui_text_renderer.hpp"
#include "ui_utf8.hpp"

#include <raylib.h>

namespace arpg::platform {
namespace {

const char* attack_name(combat::AttackId id) noexcept {
    switch (id) {
    case combat::AttackId::j1: return u8"轻攻击Ⅰ";
    case combat::AttackId::j2: return u8"轻攻击Ⅱ";
    case combat::AttackId::j3: return u8"轻攻击Ⅲ";
    case combat::AttackId::launcher: return u8"挑飞";
    case combat::AttackId::air_j: return u8"空中轻攻击";
    case combat::AttackId::none: return u8"无";
    }
    return u8"未知";
}

const char* phase_name(combat::AttackPhase phase) noexcept {
    switch (phase) {
    case combat::AttackPhase::startup: return u8"起手";
    case combat::AttackPhase::active: return u8"生效";
    case combat::AttackPhase::recovery: return u8"后摇";
    case combat::AttackPhase::finished: return u8"结束";
    }
    return u8"未知";
}

const char* player_state_name(combat::PlayerState state) noexcept {
    switch (state) {
    case combat::PlayerState::idle: return u8"待机";
    case combat::PlayerState::move: return u8"移动";
    case combat::PlayerState::attack_startup: return u8"攻击起手";
    case combat::PlayerState::attack_active: return u8"攻击生效";
    case combat::PlayerState::attack_recovery: return u8"攻击后摇";
    case combat::PlayerState::jump_rise: return u8"上升";
    case combat::PlayerState::jump_fall: return u8"下落";
    case combat::PlayerState::landing: return u8"落地";
    }
    return u8"未知";
}

const char* event_name(combat::CombatEventKind kind) noexcept {
    switch (kind) {
    case combat::CombatEventKind::swing: return u8"挥击";
    case combat::CombatEventKind::hit: return u8"命中";
    case combat::CombatEventKind::impact_summary: return u8"冲击汇总";
    case combat::CombatEventKind::landing: return u8"落地";
    case combat::CombatEventKind::break_started: return u8"破防开始";
    case combat::CombatEventKind::defeated: return u8"已击败";
    case combat::CombatEventKind::respawned: return u8"已重生";
    case combat::CombatEventKind::reset: return u8"已重置";
    case combat::CombatEventKind::player_hit: return u8"玩家命中";
    case combat::CombatEventKind::player_hurt_started: return u8"玩家受击";
    case combat::CombatEventKind::player_health_reset: return u8"玩家生命重置";
    }
    return u8"未知";
}

const char* ecology_name(dungeon::DungeonElement element) noexcept {
    switch (element) {
    case dungeon::DungeonElement::fire: return u8"火焰";
    case dungeon::DungeonElement::water: return u8"水流";
    case dungeon::DungeonElement::lightning: return u8"闪电";
    case dungeon::DungeonElement::chaos: return u8"混沌";
    }
    return u8"未知";
}

const char* save_indicator_name(SaveIndicator indicator) noexcept {
    switch (indicator) {
    case SaveIndicator::none: return u8"空闲";
    case SaveIndicator::saving: return u8"保存中";
    case SaveIndicator::saved: return u8"已保存";
    case SaveIndicator::recovered: return u8"已恢复";
    case SaveIndicator::error: return u8"错误";
    }
    return u8"未知";
}

const char* save_slot_name(persistence::SaveSlot slot) noexcept {
    switch (slot) {
    case persistence::SaveSlot::none: return u8"无";
    case persistence::SaveSlot::a: return "A";
    case persistence::SaveSlot::b: return "B";
    }
    return u8"未知";
}

}  // namespace

DebugOverlayDiagnosticsPlan make_debug_overlay_diagnostics_plan(
    const dungeon::DungeonSnapshot& current,
    const HudBuildDiagnostics& hud_diagnostics,
    std::uint32_t notice_drops,
    std::uint64_t binding_revision,
    const combat::CombatEvent& last_event,
    bool has_last_event,
    bool cjk_font_ready) noexcept {
    DebugOverlayDiagnosticsPlan plan{};
    plan.total_budget = current.encounter.total_budget;
    plan.current_wave_budget = current.encounter.current_wave_budget;
    plan.ground_saturation = current.diagnostics.ground_saturation_count;
    plan.room_index_overflow = current.diagnostics.room_index_overflow;
    if (current.combat.has_value()) {
        const combat::CombatSnapshot& combat = *current.combat;
        plan.active_monsters = combat.monster_count;
        plan.active_projectiles = combat.projectile_count;
        plan.active_hazards = combat.hazard_count;
        plan.projectile_saturation = combat.diagnostics.projectile_saturation_count;
        plan.projectile_invalid_owner = combat.diagnostics.projectile_invalid_owner_count;
        plan.hazard_saturation = combat.diagnostics.hazard_saturation_count;
        plan.hazard_invalid_owner = combat.diagnostics.hazard_invalid_owner_count;
    }
    plan.hud = hud_diagnostics;
    plan.notice_drops = notice_drops;
    plan.binding_revision = binding_revision;
    plan.last_event = last_event;
    plan.has_last_event = has_last_event;
    plan.cjk_font_ready = cjk_font_ready;
    return plan;
}

DebugOverlayView make_debug_overlay_view(
    const dungeon::DungeonSnapshot& current,
    const DungeonRenderStatus& runtime_status,
    const CombatFeedback& feedback,
    bool audio_ready,
    const DebugOverlayDiagnosticsPlan& diagnostics) noexcept {
    constexpr Color kText{218, 226, 239, 255};
    constexpr Color kWarning{255, 210, 118, 255};
    constexpr Color kError{255, 151, 117, 255};
    DebugOverlayView view{};
    view.line_count = view.lines.size();

    static_cast<void>(ui_utf8_format(view.lines[0].text.data(),
        view.lines[0].text.size(),
        u8"根种子 %016llX 房间种子 %016llX 提交代次 %llu",
        static_cast<unsigned long long>(current.root_seed),
        static_cast<unsigned long long>(current.room_seed),
        static_cast<unsigned long long>(current.commit_generation)));
    view.lines[0].color = kText;

    static_cast<void>(ui_utf8_format(view.lines[1].text.data(),
        view.lines[1].text.size(),
        u8"存档 %s 生态 %s 洞口 %s 深渊 %s 槽 %s 错误 %u 故障 %u",
        save_indicator_name(runtime_status.indicator), ecology_name(current.ecology),
        current.has_hole ? u8"是" : u8"否",
        current.is_abyss ? u8"是" : u8"否",
        save_slot_name(runtime_status.active_slot),
        static_cast<unsigned>(runtime_status.error),
        static_cast<unsigned>(current.diagnostics.fault)));
    view.lines[1].color = runtime_status.indicator == SaveIndicator::error
            || runtime_status.faulted || runtime_status.recovery_required
        ? kError
        : runtime_status.indicator == SaveIndicator::saving
            ? kWarning : kText;

    static_cast<void>(ui_utf8_format(view.lines[2].text.data(),
        view.lines[2].text.size(),
        u8"预算 总计/当前 %u/%u 实体 怪物/投射物/危险 %llu/%llu/%llu",
        static_cast<unsigned>(diagnostics.total_budget),
        static_cast<unsigned>(diagnostics.current_wave_budget),
        static_cast<unsigned long long>(diagnostics.active_monsters),
        static_cast<unsigned long long>(diagnostics.active_projectiles),
        static_cast<unsigned long long>(diagnostics.active_hazards)));
    view.lines[2].color = kText;

    static_cast<void>(ui_utf8_format(view.lines[3].text.data(),
        view.lines[3].text.size(),
        u8"地下城 事件溢出 %u 转发溢出 %u 拒绝出口 %u 地面饱和 %u 房间索引溢出 %s",
        current.diagnostics.event_overflow_count,
        current.diagnostics.combat_relay_overflow_count,
        current.diagnostics.rejected_exit_count,
        diagnostics.ground_saturation,
        diagnostics.room_index_overflow ? u8"是" : u8"否"));
    view.lines[3].color = diagnostics.room_index_overflow
        ? kWarning : kText;

    if (current.combat.has_value()) {
        const combat::CombatSnapshot& state = *current.combat;
        static_cast<void>(ui_utf8_format(view.lines[4].text.data(),
            view.lines[4].text.size(),
            u8"战斗 帧 %llu 玩家状态 %s 高度 %.2f 命中停顿 %u",
            static_cast<unsigned long long>(state.tick), player_state_name(state.player.state),
            state.player.position.z, state.player.hit_stop_ticks));
        static_cast<void>(ui_utf8_format(view.lines[5].text.data(),
            view.lines[5].text.size(),
            u8"效果 所有者/活跃 %llu/%llu 溢出 %u 指令溢出 %u",
            static_cast<unsigned long long>(state.diagnostics.effect_owner_count),
            static_cast<unsigned long long>(state.diagnostics.active_effect_count),
            state.diagnostics.effect_overflow_count,
            state.diagnostics.effect_command_overflow_count));
        static_cast<void>(ui_utf8_format(view.lines[6].text.data(),
            view.lines[6].text.size(), u8"动作 %s %s 连段 %u",
            attack_name(state.player.active_attack),
            phase_name(state.player.attack_phase), state.player.combo_stage));
        static_cast<void>(ui_utf8_format(view.lines[7].text.data(),
            view.lines[7].text.size(),
            u8"输入 队列 %llu 过期 %u 溢出 %u 事件溢出 %u",
            static_cast<unsigned long long>(state.diagnostics.input_size),
            state.diagnostics.input_expired_count, state.diagnostics.input_overflow_count,
            state.diagnostics.event_overflow_count));
    } else {
        static_cast<void>(ui_utf8_format(view.lines[4].text.data(),
            view.lines[4].text.size(), u8"战斗 转场期间无战斗快照"));
        static_cast<void>(ui_utf8_format(view.lines[5].text.data(),
            view.lines[5].text.size(), u8"效果 转场期间无战斗快照"));
        static_cast<void>(ui_utf8_format(view.lines[6].text.data(),
            view.lines[6].text.size(), u8"动作 转场期间无战斗快照"));
        static_cast<void>(ui_utf8_format(view.lines[7].text.data(),
            view.lines[7].text.size(), u8"输入 转场期间无战斗快照"));
    }
    for (std::size_t index = 4U; index < 8U; ++index) {
        view.lines[index].color = current.combat.has_value() ? kText : kWarning;
    }

    if (diagnostics.has_last_event) {
        static_cast<void>(ui_utf8_format(view.lines[8].text.data(),
            view.lines[8].text.size(), u8"最近事件 %s 目标 %u",
            event_name(diagnostics.last_event.kind),
            static_cast<unsigned>(diagnostics.last_event.target_ordinal)));
    } else {
        static_cast<void>(ui_utf8_format(view.lines[8].text.data(),
            view.lines[8].text.size(), u8"最近事件 无"));
    }
    view.lines[8].color = kText;

    static_cast<void>(ui_utf8_format(view.lines[9].text.data(),
        view.lines[9].text.size(),
        u8"反馈 效果 %u 丢弃 %u 震屏 %.1f 音频%s",
        static_cast<unsigned>(feedback.active_count()), feedback.dropped_count(),
        feedback.shake_amplitude(), audio_ready ? u8"已就绪" : u8"不可用"));
    view.lines[9].color = audio_ready ? kText : kError;

    static_cast<void>(ui_utf8_format(view.lines[10].text.data(),
        view.lines[10].text.size(),
        u8"资源 投射物饱和/无效 %u/%u 危险饱和/无效 %u/%u",
        diagnostics.projectile_saturation, diagnostics.projectile_invalid_owner,
        diagnostics.hazard_saturation, diagnostics.hazard_invalid_owner));
    view.lines[10].color = kText;

    static_cast<void>(ui_utf8_format(view.lines[11].text.data(),
        view.lines[11].text.size(),
        u8"界面 钳制 %u 截断 %u 战斗缺失 %s 通知丢弃 %u 绑定版本 %llu 中文字体 %s",
        diagnostics.hud.clamped_values, diagnostics.hud.truncated_texts,
        diagnostics.hud.combat_snapshot_missing ? u8"是" : u8"否",
        diagnostics.notice_drops,
        static_cast<unsigned long long>(diagnostics.binding_revision),
        diagnostics.cjk_font_ready ? u8"已就绪" : u8"不可用"));
    view.lines[11].color = diagnostics.cjk_font_ready ? kText : kError;
    return view;
}

DebugOverlayLayout debug_overlay_layout(
    int screen_width, int screen_height) noexcept {
    DebugOverlayLayout layout{};
    if (screen_width <= 0 || screen_height <= 0) return layout;
    constexpr float kMargin = 8.0F;
    constexpr float kGap = 8.0F;
    constexpr float kPadding = 8.0F;
    constexpr float kLineStep = 24.0F;
    constexpr float kLineHeight = 20.0F;
    layout.column_count = screen_width < 1280 ? 1U : 2U;
    layout.font_size = 13.0F;
    layout.spacing = 0.5F;
    layout.backing_color = {0x0B, 0x10, 0x19, 255};
    const std::size_t rows = layout.column_count == 1U ? 12U : 6U;
    const float total_width = static_cast<float>(screen_width) - 2.0F * kMargin;
    const float column_width = layout.column_count == 1U
        ? total_width : (total_width - kGap) * 0.5F;
    const float backing_height = 2.0F * kPadding
        + static_cast<float>(rows) * kLineStep;
    for (std::size_t column = 0U; column < layout.column_count; ++column) {
        const float x = kMargin
            + static_cast<float>(column) * (column_width + kGap);
        layout.column_backings[column] = {
            x, kMargin, column_width, backing_height};
        for (std::size_t row = 0U; row < rows; ++row) {
            const std::size_t index = column * rows + row;
            layout.line_bounds[index] = {
                x + kPadding,
                kMargin + kPadding + static_cast<float>(row) * kLineStep,
                column_width - 2.0F * kPadding,
                kLineHeight,
            };
        }
    }
    return layout;
}

void DebugOverlayRenderer::draw(Font shared_ui_font,
    bool shared_ui_font_ready,
    const dungeon::DungeonSnapshot& current,
    const DungeonRenderStatus& runtime_status,
    const CombatFeedback& feedback,
    bool audio_ready,
    const DebugOverlayDiagnosticsPlan& diagnostics) const noexcept {
    const DebugOverlayView view = make_debug_overlay_view(
        current, runtime_status, feedback, audio_ready, diagnostics);
    const DebugOverlayLayout layout = debug_overlay_layout(
        GetScreenWidth(), GetScreenHeight());
    for (std::size_t column = 0U; column < layout.column_count; ++column) {
        DrawRectangleRec(layout.column_backings[column], layout.backing_color);
    }
    if (!shared_ui_font_ready || !IsFontValid(shared_ui_font)) return;
    for (std::size_t index = 0U; index < view.line_count; ++index) {
        const Rectangle bounds = layout.line_bounds[index];
        draw_crisp_ui_text(shared_ui_font, view.lines[index].text.data(),
            {bounds.x, bounds.y + 2.0F}, layout.font_size,
            layout.spacing, view.lines[index].color, 1);
    }
}

}  // namespace arpg::platform
