#include "death_overlay_font.hpp"

#include <cstdint>

namespace arpg::platform {
namespace {

int next_codepoint(const char*& cursor) noexcept {
    const auto* bytes = reinterpret_cast<const unsigned char*>(cursor);
    if (bytes[0] == 0U) return 0;
    if (bytes[0] < 0x80U) {
        ++cursor;
        return bytes[0];
    }
    if ((bytes[0] & 0xE0U) == 0xC0U
            && (bytes[1] & 0xC0U) == 0x80U) {
        cursor += 2;
        return static_cast<int>(((bytes[0] & 0x1FU) << 6U)
            | (bytes[1] & 0x3FU));
    }
    if ((bytes[0] & 0xF0U) == 0xE0U
            && (bytes[1] & 0xC0U) == 0x80U
            && (bytes[2] & 0xC0U) == 0x80U) {
        cursor += 3;
        return static_cast<int>(((bytes[0] & 0x0FU) << 12U)
            | ((bytes[1] & 0x3FU) << 6U) | (bytes[2] & 0x3FU));
    }
    if ((bytes[0] & 0xF8U) == 0xF0U
            && (bytes[1] & 0xC0U) == 0x80U
            && (bytes[2] & 0xC0U) == 0x80U
            && (bytes[3] & 0xC0U) == 0x80U) {
        cursor += 4;
        return static_cast<int>(((bytes[0] & 0x07U) << 18U)
            | ((bytes[1] & 0x3FU) << 12U)
            | ((bytes[2] & 0x3FU) << 6U) | (bytes[3] & 0x3FU));
    }
    ++cursor;
    return '?';
}

void add_codepoint(DeathOverlayFontPlan& plan, int codepoint) noexcept {
    if (death_overlay_font_has_codepoint(plan, codepoint)
            || plan.codepoint_count >= plan.codepoints.size()) {
        return;
    }
    plan.codepoints[plan.codepoint_count++] = codepoint;
}

void add_text(DeathOverlayFontPlan& plan, const char* text) noexcept {
    const char* cursor = text;
    while (*cursor != '\0') add_codepoint(plan, next_codepoint(cursor));
}

}  // namespace

DeathOverlayFontPlan death_overlay_font_plan() noexcept {
    DeathOverlayFontPlan plan{};
    plan.candidate_paths = {{
        "C:/Windows/Fonts/NotoSansSC-VF.ttf",
        "C:/Windows/Fonts/simhei.ttf",
        "C:/Windows/Fonts/Deng.ttf",
        "C:/Windows/Fonts/simfang.ttf",
    }};
    plan.candidate_count = plan.candidate_paths.size();
    for (int codepoint = 32; codepoint <= 126; ++codepoint) {
        add_codepoint(plan, codepoint);
    }
    add_codepoint(plan, 0x2192);
    add_codepoint(plan, 0xFF1A);
    add_text(plan,
        "死亡回顾第层房间火焰水闪电混沌深渊普通致死来源未知伤害物理"
        "怪物攻击投射地面危险词缀环境最后一击原始最终护盾损失生命"
        "最近秒实际承伤时防御护甲减闪避率元素上限正在记录继续保存失败请重试设置已恢复默认值"
        "剩余出口已开放未分配点减速无敌目标个敌人待结算经验深度 · 层房间生态第波下一波即将开始待领奖励未领取"
        "正在保存房间正在处理撤退房间状态异常"
        "投弹者冲锋之壁垒支援射手突袭追猎灾术师原生燃烧连锁爆破雷暴"
        "猎杀之焰扩张强力狂热迅捷装甲多重寒冷腐蚀闪现无规则追猎狂怒"
        "沉重脚步疲惫恢复献祭");
    return plan;
}

bool death_overlay_font_has_codepoint(
    const DeathOverlayFontPlan& plan,
    int codepoint) noexcept {
    for (std::size_t index = 0U; index < plan.codepoint_count; ++index) {
        if (plan.codepoints[index] == codepoint) return true;
    }
    return false;
}

bool death_overlay_font_covers_text(
    const DeathOverlayFontPlan& plan,
    const char* utf8_text) noexcept {
    if (utf8_text == nullptr) return false;
    const char* cursor = utf8_text;
    while (*cursor != '\0') {
        if (!death_overlay_font_has_codepoint(plan, next_codepoint(cursor))) {
            return false;
        }
    }
    return true;
}

}  // namespace arpg::platform
