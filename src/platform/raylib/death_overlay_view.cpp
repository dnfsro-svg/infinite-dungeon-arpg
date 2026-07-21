#include "death_overlay_view.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>

namespace arpg::platform {
namespace {

namespace checkpoint = dungeon::checkpoint;

using checkpoint::DeathCheckpoint;
using checkpoint::DeathDamageType;
using checkpoint::DeathSourceKind;

template <std::size_t Size>
void copy_text(std::array<char, Size>& destination, const char* text) noexcept {
    if constexpr (Size != 0U) {
        std::snprintf(destination.data(), destination.size(), "%s", text);
    }
}

const char* ecology_name(checkpoint::DungeonElement ecology) noexcept {
    switch (ecology) {
    case checkpoint::DungeonElement::fire: return "火焰";
    case checkpoint::DungeonElement::water: return "水";
    case checkpoint::DungeonElement::lightning: return "闪电";
    case checkpoint::DungeonElement::chaos: return "混沌";
    }
    return "未知";
}

const char* damage_name(DeathDamageType type) noexcept {
    switch (type) {
    case DeathDamageType::physical: return "物理伤害";
    case DeathDamageType::fire: return "火焰伤害";
    case DeathDamageType::water: return "水伤害";
    case DeathDamageType::lightning: return "闪电伤害";
    case DeathDamageType::chaos: return "混沌伤害";
    }
    return "未知伤害";
}

const char* monster_name(std::uint8_t id) noexcept {
    constexpr std::array<const char*,
        static_cast<std::size_t>(combat::MonsterId::count)> kNames{{
        "火焰投弹者", "火焰冲锋者", "水之壁垒", "水之支援者",
        "闪电射手", "闪电突袭者", "混沌追猎者", "混沌灾术师",
    }};
    return id < kNames.size() ? kNames[id] : nullptr;
}

const char* hazard_name(std::uint16_t id) noexcept {
    if (id > static_cast<std::uint16_t>(combat::HazardKind::chaos_expansion)) {
        return nullptr;
    }
    switch (static_cast<combat::HazardKind>(id)) {
    case combat::HazardKind::native: return "原生地面危险";
    case combat::HazardKind::burning: return "燃烧地面";
    case combat::HazardKind::chain_lightning: return "连锁闪电";
    case combat::HazardKind::death_blast: return "死亡爆破";
    case combat::HazardKind::thunderstorm: return "雷暴";
    case combat::HazardKind::hunting_flame: return "猎杀之焰";
    case combat::HazardKind::chaos_expansion: return "混沌扩张";
    }
    return nullptr;
}

const char* affix_name(std::uint16_t id) noexcept {
    constexpr std::array<const char*,
        static_cast<std::size_t>(combat::MonsterAffixId::count)> kNames{{
        "强力", "狂热", "迅捷", "装甲", "护盾", "多重投射",
        "燃烧地面", "寒冷", "连锁闪电", "混沌腐蚀", "闪现突袭", "死亡爆破",
    }};
    return id < kNames.size() ? kNames[id] : nullptr;
}

const char* abyss_rule_name(std::uint16_t id) noexcept {
    if (id > static_cast<std::uint16_t>(abyss::AbyssRuleId::life_sacrifice)) {
        return nullptr;
    }
    switch (static_cast<abyss::AbyssRuleId>(id)) {
    case abyss::AbyssRuleId::none: return nullptr;
    case abyss::AbyssRuleId::thunderstorm: return "雷暴";
    case abyss::AbyssRuleId::hunting_flames: return "猎杀之焰";
    case abyss::AbyssRuleId::chaos_expansion: return "混沌扩张";
    case abyss::AbyssRuleId::swift_pursuit: return "迅捷追猎";
    case abyss::AbyssRuleId::abyss_bulwark: return "深渊壁垒";
    case abyss::AbyssRuleId::abyss_fury: return "深渊狂怒";
    case abyss::AbyssRuleId::heavy_steps: return "沉重脚步";
    case abyss::AbyssRuleId::exhausted_recovery: return "疲惫恢复";
    case abyss::AbyssRuleId::life_sacrifice: return "生命献祭";
    }
    return nullptr;
}

const char* ascii_ecology_name(checkpoint::DungeonElement ecology) noexcept {
    switch (ecology) {
    case checkpoint::DungeonElement::fire: return "FIRE";
    case checkpoint::DungeonElement::water: return "WATER";
    case checkpoint::DungeonElement::lightning: return "LIGHTNING";
    case checkpoint::DungeonElement::chaos: return "CHAOS";
    }
    return "UNKNOWN";
}

const char* ascii_damage_name(DeathDamageType type) noexcept {
    switch (type) {
    case DeathDamageType::physical: return "PHYSICAL DAMAGE";
    case DeathDamageType::fire: return "FIRE DAMAGE";
    case DeathDamageType::water: return "WATER DAMAGE";
    case DeathDamageType::lightning: return "LIGHTNING DAMAGE";
    case DeathDamageType::chaos: return "CHAOS DAMAGE";
    }
    return "UNKNOWN DAMAGE";
}

const char* ascii_monster_name(std::uint8_t id) noexcept {
    constexpr std::array<const char*,
        static_cast<std::size_t>(combat::MonsterId::count)> kNames{{
        "FIRE BOMBER", "FIRE CHARGER", "WATER BULWARK", "WATER SUPPORT",
        "LIGHTNING SHOOTER", "LIGHTNING DASHER", "CHAOS CHASER", "CHAOS HAZARD",
    }};
    return id < kNames.size() ? kNames[id] : nullptr;
}

const char* ascii_hazard_name(std::uint16_t id) noexcept {
    constexpr std::array<const char*, 7U> kNames{{
        "NATIVE HAZARD", "BURNING GROUND", "CHAIN LIGHTNING", "DEATH BLAST",
        "THUNDERSTORM", "HUNTING FLAMES", "CHAOS EXPANSION",
    }};
    return id < kNames.size() ? kNames[id] : nullptr;
}

const char* ascii_affix_name(std::uint16_t id) noexcept {
    constexpr std::array<const char*,
        static_cast<std::size_t>(combat::MonsterAffixId::count)> kNames{{
        "MIGHTY", "FRENZY", "SWIFT", "ARMORED", "SHIELDING", "MULTISHOT",
        "BURNING GROUND", "CHILLING", "CHAIN LIGHTNING", "CHAOS CORROSION",
        "BLINK ASSAULT", "DEATH BLAST",
    }};
    return id < kNames.size() ? kNames[id] : nullptr;
}

const char* ascii_abyss_rule_name(std::uint16_t id) noexcept {
    constexpr std::array<const char*, 9U> kNames{{
        "THUNDERSTORM", "HUNTING FLAMES", "CHAOS EXPANSION", "SWIFT PURSUIT",
        "ABYSS BULWARK", "ABYSS FURY", "HEAVY STEPS", "EXHAUSTED RECOVERY",
        "LIFE SACRIFICE",
    }};
    return id < kNames.size() ? kNames[id] : nullptr;
}

void add_line(DeathOverlayView& view, DeathOverlayColumn column,
    const char* text, bool heading = false) noexcept {
    if (view.line_count >= view.lines.size()) return;
    DeathOverlayLine& line = view.lines[view.line_count++];
    line.column = column;
    line.heading = heading;
    copy_text(line.text, text);
}

void add_source_line(
    DeathOverlayView& view,
    const DeathCheckpoint& death) noexcept {
    char text[kDeathOverlayTextCapacity]{};
    const char* monster = monster_name(death.source_monster_id);
    const char* kind = nullptr;
    const char* detail = nullptr;
    switch (death.source_kind) {
    case DeathSourceKind::monster_attack:
        kind = "怪物攻击";
        detail = monster;
        break;
    case DeathSourceKind::projectile:
        kind = "投射物";
        detail = monster;
        break;
    case DeathSourceKind::ground_hazard:
        kind = "地面危险";
        detail = hazard_name(death.source_detail_id);
        break;
    case DeathSourceKind::monster_affix: {
        kind = "怪物词缀";
        detail = affix_name(death.source_detail_id);
        break;
    }
    case DeathSourceKind::abyss_environment:
        kind = "深渊环境";
        detail = abyss_rule_name(death.source_detail_id);
        break;
    case DeathSourceKind::unknown:
        break;
    }
    if (kind == nullptr || detail == nullptr) {
        std::snprintf(text, sizeof(text), "致死来源：未知来源 / %s",
            damage_name(death.damage_type));
    } else if (monster != nullptr && death.source_kind != DeathSourceKind::monster_attack
            && death.source_kind != DeathSourceKind::projectile
            && death.source_kind != DeathSourceKind::abyss_environment) {
        std::snprintf(text, sizeof(text), "致死来源：%s / %s / %s / %s",
            kind, detail, monster, damage_name(death.damage_type));
    } else {
        std::snprintf(text, sizeof(text), "致死来源：%s / %s / %s",
            kind, detail, damage_name(death.damage_type));
    }
    add_line(view, DeathOverlayColumn::full, text);
}

void add_ascii_source_line(
    DeathOverlayView& view,
    const DeathCheckpoint& death) noexcept {
    char text[kDeathOverlayTextCapacity]{};
    const char* kind = "UNKNOWN";
    const char* detail = nullptr;
    const char* monster = ascii_monster_name(death.source_monster_id);
    switch (death.source_kind) {
    case DeathSourceKind::monster_attack:
        kind = "MONSTER ATTACK";
        detail = monster;
        break;
    case DeathSourceKind::projectile:
        kind = "PROJECTILE";
        detail = monster;
        break;
    case DeathSourceKind::ground_hazard:
        kind = "GROUND HAZARD";
        detail = ascii_hazard_name(death.source_detail_id);
        break;
    case DeathSourceKind::monster_affix:
        kind = "MONSTER AFFIX";
        detail = ascii_affix_name(death.source_detail_id);
        break;
    case DeathSourceKind::abyss_environment:
        kind = "ABYSS ENVIRONMENT";
        detail = ascii_abyss_rule_name(death.source_detail_id);
        break;
    case DeathSourceKind::unknown:
        break;
    }
    if (detail == nullptr) detail = "UNKNOWN";
    const bool include_monster = monster != nullptr
        && death.source_kind != DeathSourceKind::monster_attack
        && death.source_kind != DeathSourceKind::projectile
        && death.source_kind != DeathSourceKind::abyss_environment;
    if (include_monster) {
        std::snprintf(text, sizeof(text), "Source: %s / %s / %s / %s",
            kind, detail, monster, ascii_damage_name(death.damage_type));
    } else {
        std::snprintf(text, sizeof(text), "Source: %s / %s / %s",
            kind, detail, ascii_damage_name(death.damage_type));
    }
    add_line(view, DeathOverlayColumn::full, text);
}

DeathOverlayRect make_rect(
    float x, float y, float width, float height) noexcept {
    return {x, y, (std::max)(0.0F, width), (std::max)(0.0F, height)};
}

}  // namespace

DeathOverlayView build_death_overlay_view(
    const dungeon::DungeonSnapshot& snapshot) noexcept {
    DeathOverlayView view{};
    if (!snapshot.death.has_value()) return view;

    view.visible = true;
    copy_text(view.title, "死亡回顾");
    const auto& state = *snapshot.death;
    const DeathCheckpoint& death = state.checkpoint;
    char text[kDeathOverlayTextCapacity]{};

    std::snprintf(text, sizeof(text), "第 %llu 层 | 房间 %llu | %s | %s",
        static_cast<unsigned long long>(death.death_depth),
        static_cast<unsigned long long>(death.death_floor_room_index),
        ecology_name(death.death_ecology),
        death.death_was_abyss ? "深渊房" : "普通房");
    add_line(view, DeathOverlayColumn::full, text);
    add_source_line(view, death);
    std::snprintf(text, sizeof(text), "第 %llu 层 → 第 %llu 层",
        static_cast<unsigned long long>(death.death_depth),
        static_cast<unsigned long long>(death.target_room.depth));
    add_line(view, DeathOverlayColumn::full, text);

    add_line(view, DeathOverlayColumn::left, "最后一击", true);
    std::snprintf(text, sizeof(text), "原始 %llu",
        static_cast<unsigned long long>(death.raw_damage));
    add_line(view, DeathOverlayColumn::left, text);
    std::snprintf(text, sizeof(text), "护盾损失 %llu",
        static_cast<unsigned long long>(death.barrier_loss));
    add_line(view, DeathOverlayColumn::left, text);
    std::snprintf(text, sizeof(text), "生命损失 %llu",
        static_cast<unsigned long long>(death.health_loss));
    add_line(view, DeathOverlayColumn::left, text);
    std::snprintf(text, sizeof(text), "最终 %llu",
        static_cast<unsigned long long>(death.final_damage));
    add_line(view, DeathOverlayColumn::left, text);
    add_line(view, DeathOverlayColumn::left, "最近 5 秒实际承伤", true);
    constexpr const char* kRecentLabels[] = {"物理", "火", "水", "电", "混沌"};
    for (std::size_t index = 0U; index < death.recent_damage.size(); ++index) {
        std::snprintf(text, sizeof(text), "%s %llu", kRecentLabels[index],
            static_cast<unsigned long long>(death.recent_damage[index]));
        add_line(view, DeathOverlayColumn::left, text);
    }

    add_line(view, DeathOverlayColumn::right, "死亡时防御", true);
    std::snprintf(text, sizeof(text), "生命 %d/%d", death.hp, death.max_hp);
    add_line(view, DeathOverlayColumn::right, text);
    std::snprintf(text, sizeof(text), "护盾 %d/%d",
        death.barrier, death.max_barrier);
    add_line(view, DeathOverlayColumn::right, text);
    std::snprintf(text, sizeof(text), "护甲 %lld",
        static_cast<long long>(death.armor));
    add_line(view, DeathOverlayColumn::right, text);
    std::snprintf(text, sizeof(text), "减伤 %.2f%%",
        static_cast<double>(death.armor_reduction_bp) / 100.0);
    add_line(view, DeathOverlayColumn::right, text);
    std::snprintf(text, sizeof(text), "闪避 %lld",
        static_cast<long long>(death.evasion));
    add_line(view, DeathOverlayColumn::right, text);
    std::snprintf(text, sizeof(text), "闪避率 %.2f%%",
        static_cast<double>(death.evasion_rate_bp) / 100.0);
    add_line(view, DeathOverlayColumn::right, text);
    add_line(view, DeathOverlayColumn::right, "元素减伤/上限", true);
    constexpr const char* kElementLabels[] = {"火", "水", "电", "混沌"};
    for (std::size_t index = 0U; index < death.damage_reduction.size(); ++index) {
        std::snprintf(text, sizeof(text), "%s %.2f%%/%.2f%%",
            kElementLabels[index],
            static_cast<double>(death.damage_reduction[index]) / 100.0,
            static_cast<double>(death.damage_reduction_cap[index]) / 100.0);
        add_line(view, DeathOverlayColumn::right, text);
    }

    if (state.saving) {
        copy_text(view.prompt, "正在记录死亡");
    } else if (state.continue_failed) {
        copy_text(view.prompt, "保存失败，请重试");
    } else if (state.can_continue) {
        copy_text(view.prompt, "E 继续");
    }
    return view;
}

DeathOverlayView build_death_overlay_ascii_view(
    const dungeon::DungeonSnapshot& snapshot) noexcept {
    DeathOverlayView view{};
    if (!snapshot.death.has_value()) return view;

    view.visible = true;
    copy_text(view.title, "DEATH RECAP");
    const auto& state = *snapshot.death;
    const DeathCheckpoint& death = state.checkpoint;
    char text[kDeathOverlayTextCapacity]{};

    std::snprintf(text, sizeof(text), "Depth %llu | Room %llu | %s | %s",
        static_cast<unsigned long long>(death.death_depth),
        static_cast<unsigned long long>(death.death_floor_room_index),
        ascii_ecology_name(death.death_ecology),
        death.death_was_abyss ? "ABYSS" : "NORMAL");
    add_line(view, DeathOverlayColumn::full, text);
    add_ascii_source_line(view, death);
    std::snprintf(text, sizeof(text), "Depth %llu -> Depth %llu",
        static_cast<unsigned long long>(death.death_depth),
        static_cast<unsigned long long>(death.target_room.depth));
    add_line(view, DeathOverlayColumn::full, text);

    add_line(view, DeathOverlayColumn::left, "Last hit", true);
    std::snprintf(text, sizeof(text), "Raw %llu",
        static_cast<unsigned long long>(death.raw_damage));
    add_line(view, DeathOverlayColumn::left, text);
    std::snprintf(text, sizeof(text), "Barrier loss %llu",
        static_cast<unsigned long long>(death.barrier_loss));
    add_line(view, DeathOverlayColumn::left, text);
    std::snprintf(text, sizeof(text), "HP loss %llu",
        static_cast<unsigned long long>(death.health_loss));
    add_line(view, DeathOverlayColumn::left, text);
    std::snprintf(text, sizeof(text), "Final %llu",
        static_cast<unsigned long long>(death.final_damage));
    add_line(view, DeathOverlayColumn::left, text);
    add_line(view, DeathOverlayColumn::left, "Recent 5s damage", true);
    constexpr const char* kRecentLabels[] = {
        "Physical", "Fire", "Water", "Lightning", "Chaos",
    };
    for (std::size_t index = 0U; index < death.recent_damage.size(); ++index) {
        std::snprintf(text, sizeof(text), "%s %llu", kRecentLabels[index],
            static_cast<unsigned long long>(death.recent_damage[index]));
        add_line(view, DeathOverlayColumn::left, text);
    }

    add_line(view, DeathOverlayColumn::right, "Defense at death", true);
    std::snprintf(text, sizeof(text), "HP %d/%d", death.hp, death.max_hp);
    add_line(view, DeathOverlayColumn::right, text);
    std::snprintf(text, sizeof(text), "Barrier %d/%d",
        death.barrier, death.max_barrier);
    add_line(view, DeathOverlayColumn::right, text);
    std::snprintf(text, sizeof(text), "Armor %lld",
        static_cast<long long>(death.armor));
    add_line(view, DeathOverlayColumn::right, text);
    std::snprintf(text, sizeof(text), "Armor reduction %.2f%%",
        static_cast<double>(death.armor_reduction_bp) / 100.0);
    add_line(view, DeathOverlayColumn::right, text);
    std::snprintf(text, sizeof(text), "Evasion %lld",
        static_cast<long long>(death.evasion));
    add_line(view, DeathOverlayColumn::right, text);
    std::snprintf(text, sizeof(text), "Evasion rate %.2f%%",
        static_cast<double>(death.evasion_rate_bp) / 100.0);
    add_line(view, DeathOverlayColumn::right, text);
    add_line(view, DeathOverlayColumn::right, "Element reduction/cap", true);
    constexpr const char* kElementLabels[] = {
        "Fire", "Water", "Lightning", "Chaos",
    };
    for (std::size_t index = 0U; index < death.damage_reduction.size(); ++index) {
        std::snprintf(text, sizeof(text), "%s %.2f%%/%.2f%%",
            kElementLabels[index],
            static_cast<double>(death.damage_reduction[index]) / 100.0,
            static_cast<double>(death.damage_reduction_cap[index]) / 100.0);
        add_line(view, DeathOverlayColumn::right, text);
    }

    if (state.saving) {
        copy_text(view.prompt, "Saving death...");
    } else if (state.continue_failed) {
        copy_text(view.prompt, "Save failed - press E to retry");
    } else if (state.can_continue) {
        copy_text(view.prompt, "E Continue");
    }
    return view;
}

DeathOverlayLayout death_overlay_layout(
    int screen_width,
    int screen_height) noexcept {
    const float width = static_cast<float>((std::max)(screen_width, 1));
    const float height = static_cast<float>((std::max)(screen_height, 1));
    const bool compact = width < 1000.0F || height < 600.0F;
    const float margin = compact ? 14.0F : 48.0F;
    const float panel_width = (std::min)(width - margin * 2.0F,
        compact ? 772.0F : 1040.0F);
    const float panel_height = (std::min)(height - margin * 2.0F,
        compact ? 422.0F : 624.0F);
    const float panel_x = (width - panel_width) * 0.5F;
    const float panel_y = (height - panel_height) * 0.5F;

    DeathOverlayLayout layout{};
    layout.panel = make_rect(panel_x, panel_y, panel_width, panel_height);
    layout.title_font_size = compact ? 28 : 36;
    layout.body_font_size = compact ? 15 : 18;
    layout.prompt_font_size = compact ? 22 : 26;
    layout.title = make_rect(panel_x + 24.0F, panel_y + 16.0F,
        panel_width - 48.0F, static_cast<float>(layout.title_font_size + 6));

    const float full_x = panel_x + 28.0F;
    const float full_width = panel_width - 56.0F;
    const float full_y = panel_y + (compact ? 54.0F : 66.0F);
    const float full_step = compact ? 24.0F : 30.0F;
    const float column_gap = compact ? 18.0F : 32.0F;
    const float column_width = (full_width - column_gap) * 0.5F;
    const float column_y = panel_y + (compact ? 136.0F : 174.0F);
    const float column_step = compact ? 20.0F : 27.0F;
    std::size_t full_index = 0U;
    std::size_t left_index = 0U;
    std::size_t right_index = 0U;
    for (std::size_t index = 0U; index < layout.line_bounds.size(); ++index) {
        DeathOverlayColumn column = DeathOverlayColumn::right;
        if (index < 3U) column = DeathOverlayColumn::full;
        else if (index < 14U) column = DeathOverlayColumn::left;
        if (column == DeathOverlayColumn::full) {
            layout.line_bounds[index] = make_rect(full_x,
                full_y + full_step * static_cast<float>(full_index++),
                full_width, static_cast<float>(layout.body_font_size + 4));
        } else if (column == DeathOverlayColumn::left) {
            layout.line_bounds[index] = make_rect(full_x,
                column_y + column_step * static_cast<float>(left_index++),
                column_width, static_cast<float>(layout.body_font_size + 4));
        } else {
            layout.line_bounds[index] = make_rect(
                full_x + column_width + column_gap,
                column_y + column_step * static_cast<float>(right_index++),
                column_width, static_cast<float>(layout.body_font_size + 4));
        }
    }
    layout.prompt = make_rect(panel_x + 28.0F,
        panel_y + panel_height - (compact ? 45.0F : 58.0F),
        panel_width - 56.0F,
        static_cast<float>(layout.prompt_font_size + 8));
    return layout;
}

}  // namespace arpg::platform
