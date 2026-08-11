#include "ui_text_catalog.hpp"

#include <array>
#include <cstddef>

namespace arpg::platform {
namespace {

constexpr const char* kUnknown = u8"未知";

constexpr std::array<const char*, 45U> kUiTexts{{
    u8"暂停", u8"继续游戏", u8"设置", u8"退出游戏",
    u8"设置", u8"音频设置", u8"显示设置", u8"按键设置",
    u8"恢复默认设置", u8"应用", u8"取消", u8"已保存：", kUnknown,
    u8"生命", u8"护盾", u8"经验", u8"等级",
    u8"正在保存", u8"保存完成", u8"已恢复存档", u8"存档错误",
    u8"需要恢复存档", u8"无效存档槽",
    u8"背包", u8"装备", u8"物品详情",
    u8"被动星图", u8"未分配点", u8"关闭",
    u8"调试就绪", u8"调试不可用", kUnknown,
    u8"该按键不可绑定", u8"设置无效", u8"版本号已达上限",
    u8"实时预览失败", u8"设置保存失败，请重试",
    u8"另一游戏实例正在保存，请稍后重试",
    u8"设置回滚失败", u8"设置已保存", u8"另一实例修改已重载",
    u8"设置存储冲突，需重启", u8"设置已恢复默认值",
    u8"经验已满",
    u8"两个存档槽均无效。按 N 将其归档并创建新游戏。",
}};

constexpr std::array<const char*, 18U> kItemBaseNames{{
    u8"精炼之刃", u8"守卫头盔", u8"守护外衣", u8"突击手套", u8"行者战靴", u8"元素护符",
    u8"迅捷匕首", u8"攻城巨刃", u8"暗影兜帽", u8"堡垒头盔", u8"夜行者外衣", u8"城塞胸甲",
    u8"猎手护腕", u8"碎骨护手", u8"轻捷战靴", u8"铁护胫", u8"三元吊坠", u8"混沌之眼",
}};

constexpr std::array<const char*, 14U> kMaterialNames{{
    u8"嬗变石", u8"增幅石", u8"富豪石", u8"混沌石", u8"崇高石", u8"剥离石", u8"神圣石",
    u8"重铸石", u8"定向核心", u8"强化石", u8"+6 强化券", u8"+9 强化券", u8"+12 强化券", u8"+15 强化券",
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
    u8"烈焰爆破者", u8"烈焰冲锋者", u8"水流壁垒", u8"水流援护者",
    u8"雷电射手", u8"雷电突进者", u8"混沌追猎者", u8"混沌危机体",
}};

constexpr std::array<const char*, 12U> kMonsterAffixNames{{
    u8"强力", u8"狂热", u8"迅捷", u8"装甲", u8"护盾", u8"多重投射",
    u8"灼烧地面", u8"冰寒", u8"连锁闪电", u8"混沌腐蚀", u8"闪烁突袭", u8"死亡爆破",
}};

constexpr std::array<const char*, 12U> kMonsterAffixShortNames{{
    u8"强", u8"狂", u8"迅", u8"甲", u8"盾", u8"多射",
    u8"灼地", u8"寒", u8"连雷", u8"蚀", u8"闪袭", u8"死爆",
}};

constexpr std::array<const char*, 10U> kSettingActionNames{{
    u8"上移", u8"下移", u8"左移", u8"右移", u8"轻攻击", u8"跳跃", u8"挑飞", u8"交互", u8"背包", u8"被动树",
}};

constexpr std::array<const char*, 2U> kSkillNames{{
    u8"拔刀斩", u8"极·鬼剑术（暴风式）",
}};

// Dynamic and formatted player-facing text has no stable lookup ID.  Keep
// its player-facing corpus in the single catalog visitor so the shared glyph
// plan covers every renderer without a second hand-maintained font list.
constexpr std::array<const char*, 85U> kUiDynamicGlyphCoverage{{
    u8"死亡回顾第层房间火焰水闪电混沌深渊普通致死来源未知伤害物理"
    "上移下移左移右移轻攻击跳跃挑飞交互背包被动树调试截图暂停小键盘秒经验已满"
    "怪物攻击投射地面危险词缀环境最后一击原始最终护盾损失生命"
    "最近秒实际承伤时防御护甲减闪避率元素上限正在记录继续保存失败请重试设置已恢复默认值返回本层入口"
    "方向键导航选择返回重置房间上下左右调整请按下可绑定用于的当前空格"
    "窗口模式全屏窗口化垂直同步开关拾取过滤显示全部魔法及以上仅稀有"
    "确认退出绑定按键总音量音效音乐环境音界面音｜（）？"
    "剩余出口已开放未分配点减速无敌目标个敌人待结算经验深度 · 层房间生态第波下一波即将开始待领奖励未领取"
    "正在保存房间正在处理撤退房间状态异常"
    "需要恢复存档再次交互，以放弃奖励离开后触碰同一出口全部剩余并下降进入下一层进入出口已清理升级至级有被动点打开背包树"
    "投弹者冲锋之壁垒支援射手突袭追猎灾术师原生燃烧连锁爆破雷暴"
    "猎杀之焰扩张强力狂热迅捷装甲多重寒冷腐蚀闪现无规则追猎狂怒"
    "沉重脚步疲惫恢复献祭"
    "装备背包技能石暂停继续设置退出游戏关闭选择提示材料",
    u8"生命药", u8"第 层 → 第 层",
    u8"生命", u8"护盾", u8"剩余", u8"出口已开放", u8"保存失败",
    u8"未分配点", u8"火焰", u8"水", u8"闪电", u8"混沌",
    u8"减速", u8"腐蚀", u8"无敌", u8"深度", u8"层房间", u8"生态",
    u8"第", u8"波", u8"下一波即将开始", u8"待领奖励", u8"未领取",
    u8"正在保存房间", u8"正在处理撤退", u8"房间状态异常", u8"深渊",
    u8"目标", u8"个敌人", u8"待结算经验",
    u8"需要恢复存档", u8"离开后再次触碰同一出口以放弃全部剩余奖励",
    u8"再次交互，放弃剩余奖励并下降", u8"进入下一层", u8"进入出口",
    u8"房间已清理", u8"奖励", u8"升级至级", u8"有未分配被动点",
    u8"打开背包", u8"打开被动树",
    u8"普通魔法稀有已拾取未知装备",
    u8"装备 / 材料", u8"技能石", u8"主动技能石槽",
    u8"空主技能槽", u8"辅助技能石（只读）", u8"未装备技能石",
    u8"无", u8"取出", u8"正在保存",
    u8"技能石背包关闭",
    u8"拔刀斩", u8"极·鬼剑术（暴风式）",
    u8"怪群规模：拥挤密集兽潮",
    u8"消灭（总计）剩余",
    u8"出口尚未开放，战斗仍可继续",
    u8"离开将放弃剩余奖励",
    u8"可见：怪 105/105 环境 73/105 掉落 53/331",
    u8"收益: 起始节点", u8"收益: 路线连接", u8"代价: 无",
    u8"设置界面…",
    u8"已拾取：强化石 ×1、混沌石 ×1",
    u8"材料袋",
    // T2-T5 render-ready format samples. Keep each sample independently
    // visitable so coverage cannot pass through an unrelated corpus string.
    u8"基础 [局部] 武器物理伤害 +16",
    u8"3级 前缀 [全局] 火焰伤害 +12%",
    u8"三合一结果：成功 / 稀有 混沌之眼",
    u8"强化结果：成功 / +9 → +10",
    u8"强化结果：失败 / 装备丢失",
    u8"普通 暗影兜帽 · 物品等级 1",
    u8"已拾取：强化石 ×1、等13种",
    u8"危险：强化失败，装备丢失",
    u8"确认强化石？",
    u8"分配 / 退款 / 相邻 / 断链",
    u8"保存状态：保存中 / 就绪 / 错误",
    u8"收益：+30% 火焰伤害 / 代价：-20% 水抗性",
    u8"致死来源：烈焰爆破者 / 投射物 / 火焰伤害",
    u8"第 42 层 | 房间 7 | 火焰 | 深渊房",
    u8"最后一击：原始 42 / 护盾损失 12 / 生命损失 30 / 最终 42",
    u8"死亡时防御：生命 0/120 / 护盾 0/40 / 护甲 18 / 减伤 12.50%",
    u8"保存失败，E 重试",
    u8"装备值 / 按键提示 / 小节点 / 重要节点 / 地面危险 / 来源 / 最终伤害 ×1.12",
    u8"火焰水流闪电混沌未知低危中危高危雷暴猎杀之焰混沌扩张迅捷追猎深渊壁垒深渊狂怒沉重脚步疲惫恢复生命献祭无规则"
    "每180帧：预警45帧/半径0.8，造成15%最大生命闪电伤害"
    "每240帧：预警45帧/半径1.0，灼烧180帧，每60帧造成10%最大生命伤害"
    "每180帧扩张至半径1.0/2.3/3.6/4.9/6.2，每60帧造成8%最大生命混沌伤害"
    "怪物：移动速度+15%，冷却时间×85%护甲×130%，护盾+30%伤害与攻击速度×145%"
    "玩家：地面移动速度×85%入场资源及后续恢复×70%最大生命×55%"
    "封闭可进入保存中异常已保存已恢复存档错误锁定战斗中等待下一波已清理等待离开切换房间处理撤退中"
    "上方下方左方右方无方向Q：下潜进入下一层离开后再次触碰同一出口以放弃全部剩余奖励再次交互，放弃剩余奖励并下潜"
    u8"辅助判定攻击受击待机移动预警生效恢复冷却已击败词缀ⅠⅡⅢ起手后摇结束空中无未知"
    "根种子房间种子提交代次存档空闲洞口槽错误故障预算总计当前实体怪物投射物危险地下城事件溢出转发拒绝出口地面饱和房间索引"
    "战斗帧玩家状态高度命中停顿效果所有者活跃指令动作连段输入队列过期最近事件目标反馈丢弃震屏音频就绪不可用资源无效界面钳制截断战斗缺失通知绑定版本中文字体转场期间无战斗快照挥击冲击汇总是否",
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
    visit_values(kUiDynamicGlyphCoverage, visitor, context);
}

}  // namespace arpg::platform
