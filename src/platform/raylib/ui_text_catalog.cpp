#include "ui_text_catalog.hpp"

#include <array>
#include <cstddef>

namespace arpg::platform {
namespace {

constexpr const char* kUnknown = u8"未知";

constexpr std::array<const char*, 32U> kUiTexts{{
    u8"暂停", u8"继续", u8"设置", u8"退出",
    u8"设置", u8"音频", u8"显示", u8"按键绑定",
    u8"恢复默认", u8"应用", u8"取消", u8"设置已保存：", kUnknown,
    u8"生命", u8"护盾", u8"经验", u8"等级",
    u8"正在保存", u8"保存完成", u8"已恢复存档", u8"保存错误",
    u8"恢复", u8"无效存档槽",
    u8"背包", u8"装备", u8"物品详情",
    u8"被动树", u8"可用点数", u8"关闭",
    u8"调试就绪", u8"调试不可用", kUnknown,
}};

constexpr std::array<const char*, 18U> kItemBaseNames{{
    u8"精炼之刃", u8"守卫头盔", u8"守护外套", u8"斗士手套", u8"行者长靴", u8"元素护符",
    u8"迅捷匕首", u8"攻城巨刃", u8"暗影兜帽", u8"堡垒头盔", u8"夜行者外套", u8"城塞板甲",
    u8"猎人护腕", u8"碎骨护手", u8"轻捷长靴", u8"铁胫甲", u8"三相吊坠", u8"混沌之眼",
}};

constexpr std::array<const char*, 14U> kMaterialNames{{
    u8"蜕变石", u8"增幅石", u8"富豪石", u8"混沌石", u8"崇高石", u8"剥离石", u8"神圣石",
    u8"重铸石", u8"定向石", u8"强化石", u8"强化券+6", u8"强化券+9", u8"强化券+12", u8"强化券+15",
}};

constexpr std::array<const char*, 6U> kItemSlotNames{{
    u8"武器", u8"头盔", u8"胸甲", u8"手套", u8"靴子", u8"饰品",
}};

constexpr std::array<const char*, 3U> kItemRarityNames{{
    u8"普通", u8"魔法", u8"稀有",
}};

constexpr std::array<const char*, 12U> kPrefixAffixNames{{
    u8"锋利", u8"凶猛", u8"迅捷", u8"致命", u8"破甲", u8"坚固",
    u8"灵巧", u8"生命", u8"屏障", u8"抗性", u8"强韧", u8"猛攻",
}};

constexpr std::array<const char*, 12U> kSuffixAffixNames{{
    u8"火焰", u8"寒霜", u8"闪电", u8"混沌", u8"守护", u8"疾行",
    u8"跃动", u8"冲击", u8"吸收", u8"专注", u8"坚毅", u8"猎杀",
}};

constexpr std::array<const char*, 64U> kPassiveNodeNames{{
    u8"起源", u8"核心生命", u8"核心守护", u8"核心力量", u8"核心节奏", u8"核心冲力", u8"核心步伐", u8"核心腾跃",
    u8"火焰入口", u8"火焰分支", u8"火焰灌注", u8"火焰聚能", u8"火焰伤害", u8"火焰抗性", u8"火焰护持", u8"火焰爆发", u8"火焰近战", u8"火焰壁垒", u8"炽热打击", u8"烈焰屏障", u8"熔火活力", u8"灰烬誓约",
    u8"潮汐入口", u8"潮汐分支", u8"潮汐灌注", u8"潮汐聚能", u8"潮汐伤害", u8"潮汐抗性", u8"潮汐护持", u8"潮汐爆发", u8"潮汐近战", u8"潮汐壁垒", u8"深海打击", u8"冰潮屏障", u8"潮汐活力", u8"潮汐堡垒",
    u8"雷霆入口", u8"雷霆分支", u8"雷霆灌注", u8"雷霆聚能", u8"雷霆伤害", u8"雷霆抗性", u8"雷霆护持", u8"雷霆爆发", u8"雷霆近战", u8"雷霆壁垒", u8"风暴打击", u8"闪电屏障", u8"雷光活力", u8"风暴步伐",
    u8"混沌入口", u8"混沌分支", u8"混沌灌注", u8"混沌聚能", u8"混沌伤害", u8"混沌抗性", u8"混沌护持", u8"混沌爆发", u8"混沌近战", u8"混沌壁垒", u8"腐化打击", u8"虚空屏障", u8"血契活力", u8"鲜血契约",
}};

constexpr std::array<const char*, 8U> kMonsterNames{{
    u8"火焰爆弹", u8"火焰冲锋者", u8"水域壁垒", u8"水域支援者",
    u8"雷霆射手", u8"雷霆突进者", u8"混沌追猎者", u8"混沌危害体",
}};

constexpr std::array<const char*, 12U> kMonsterAffixNames{{
    u8"强壮", u8"狂暴", u8"迅捷", u8"装甲", u8"护盾", u8"多重射击",
    u8"燃烧地面", u8"冰冷", u8"连锁闪电", u8"混沌腐蚀", u8"闪现突袭", u8"死亡爆炸",
}};

constexpr std::array<const char*, 12U> kMonsterAffixShortNames{{
    u8"强壮", u8"狂暴", u8"迅捷", u8"装甲", u8"护盾", u8"多射",
    u8"灼地", u8"冰冷", u8"链电", u8"腐蚀", u8"闪袭", u8"死爆",
}};

constexpr std::array<const char*, 10U> kSettingActionNames{{
    u8"上移", u8"下移", u8"左移", u8"右移", u8"轻攻击", u8"跳跃", u8"挑飞", u8"交互", u8"背包", u8"被动树",
}};

constexpr std::array<const char*, 2U> kSkillNames{{
    u8"拔刀斩", u8"风暴剑阵",
}};

template <typename Id, std::size_t Count>
const char* value_or_unknown(const std::array<const char*, Count>& values,
    Id id) noexcept {
    const std::size_t index = static_cast<std::size_t>(id);
    return index < values.size() ? values[index] : kUnknown;
}

template <std::size_t Count>
void visit_values(const std::array<const char*, Count>& values,
    UiTextVisitor visitor, void* context) noexcept {
    for (const char* value : values) visitor(value, context);
}

}  // namespace

const char* ui_text(UiTextId id) noexcept {
    return value_or_unknown(kUiTexts, id);
}

const char* ui_item_base_name(std::uint8_t base_id) noexcept {
    if (base_id == 0U || base_id > kItemBaseNames.size()) return kUnknown;
    return kItemBaseNames[static_cast<std::size_t>(base_id - 1U)];
}

const char* ui_material_name(items::MaterialId id) noexcept {
    return value_or_unknown(kMaterialNames, id);
}

const char* ui_item_slot_name(items::ItemSlot id) noexcept {
    return value_or_unknown(kItemSlotNames, id);
}

const char* ui_item_rarity_name(items::ItemRarity id) noexcept {
    return value_or_unknown(kItemRarityNames, id);
}

const char* ui_item_affix_name(std::uint16_t affix_id) noexcept {
    if (affix_id >= 1U && affix_id <= 12U)
        return kPrefixAffixNames[static_cast<std::size_t>(affix_id - 1U)];
    if (affix_id >= 101U && affix_id <= 112U)
        return kSuffixAffixNames[static_cast<std::size_t>(affix_id - 101U)];
    return kUnknown;
}

const char* ui_passive_node_name(passives::PassiveNodeId id) noexcept {
    return value_or_unknown(kPassiveNodeNames, id);
}

const char* ui_monster_name(combat::MonsterId id) noexcept {
    return value_or_unknown(kMonsterNames, id);
}

const char* ui_monster_affix_name(combat::MonsterAffixId id) noexcept {
    return value_or_unknown(kMonsterAffixNames, id);
}

const char* ui_monster_affix_short_name(combat::MonsterAffixId id) noexcept {
    return value_or_unknown(kMonsterAffixShortNames, id);
}

const char* ui_setting_action_name(settings::SettingAction id) noexcept {
    return value_or_unknown(kSettingActionNames, id);
}

const char* ui_skill_name(skills::ActiveSkillId id) noexcept {
    return value_or_unknown(kSkillNames, id);
}

void visit_ui_text_catalog(UiTextVisitor visitor, void* context) noexcept {
    if (visitor == nullptr) return;
    visit_values(kUiTexts, visitor, context);
    visit_values(kItemBaseNames, visitor, context);
    visit_values(kMaterialNames, visitor, context);
    visit_values(kItemSlotNames, visitor, context);
    visit_values(kItemRarityNames, visitor, context);
    visit_values(kPrefixAffixNames, visitor, context);
    visit_values(kSuffixAffixNames, visitor, context);
    visit_values(kPassiveNodeNames, visitor, context);
    visit_values(kMonsterNames, visitor, context);
    visit_values(kMonsterAffixNames, visitor, context);
    visit_values(kMonsterAffixShortNames, visitor, context);
    visit_values(kSettingActionNames, visitor, context);
    visit_values(kSkillNames, visitor, context);
}

}  // namespace arpg::platform
