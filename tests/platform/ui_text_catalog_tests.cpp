#include "test_framework.hpp"

#include "allocation_probe.hpp"
#include "shared_ui_font.hpp"
#include "ui_text_catalog.hpp"
#include "ui_utf8.hpp"

#include <array>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <utility>

namespace {

namespace platform = arpg::platform;

// Concrete render-ready examples approved for the T2-T5 consumers. Keeping
// these as separate catalog entries lets the shared visitor prove that future
// formatted UI does not rely on coincidental characters in an unrelated
// corpus string.
constexpr std::array<const char*, 18U> kApprovedInventoryProgressionSamples{{
    // T2: inventory detail, crafting, and reinforcement results.
    u8"基础 [局部] 武器物理伤害 +16",
    u8"3级 前缀 [全局] 火焰伤害 +12%",
    u8"三合一结果：成功 / 稀有 混沌之眼",
    u8"强化结果：成功 / +9 → +10",
    u8"强化结果：失败 / 装备丢失",
    // T3: ground loot, bounded material aggregation, and confirmation.
    u8"普通 暗影兜帽 · 物品等级 1",
    u8"已拾取：强化石 ×1、等13种",
    u8"危险：强化失败，装备丢失",
    u8"确认强化石？",
    // T4: passive interaction and autosave states.
    u8"分配 / 退款 / 相邻 / 断链",
    u8"保存状态：保存中 / 就绪 / 错误",
    u8"收益：+30% 火焰伤害 / 代价：-20% 水抗性",
    // T5: death attribution and the numeric formats used by the recap.
    u8"致死来源：烈焰爆破者 / 投射物 / 火焰伤害",
    u8"第 42 层 | 房间 7 | 火焰 | 深渊房",
    u8"最后一击：原始 42 / 护盾损失 12 / 生命损失 30 / 最终 42",
    u8"死亡时防御：生命 0/120 / 护盾 0/40 / 护甲 18 / 减伤 12.50%",
    u8"保存失败，E 重试",
    // Capacity-compatible vocabulary shared by T2-T5.
    u8"装备值 / 按键提示 / 小节点 / 重要节点 / 地面危险 / 来源 / 最终伤害 ×1.12",
}};

constexpr std::array<const char*, arpg::items::kMaterialCount>
    kExpectedMaterialDescriptions{{
        u8"选择物品应用", u8"选择物品应用", u8"选择物品应用",
        u8"选择物品应用", u8"选择物品应用", u8"选择物品应用",
        u8"选择物品应用", u8"选择物品应用",
        u8"右键切换",
        u8"强化装备", u8"强化装备", u8"强化装备", u8"强化装备",
        u8"强化装备",
    }};

constexpr std::array<platform::UiTextId, 43U> kPreTask4UiTextIds{{
    platform::UiTextId::pause_title,
    platform::UiTextId::pause_continue,
    platform::UiTextId::pause_settings,
    platform::UiTextId::pause_quit,
    platform::UiTextId::settings_title,
    platform::UiTextId::settings_audio,
    platform::UiTextId::settings_display,
    platform::UiTextId::settings_bindings,
    platform::UiTextId::settings_restore_defaults,
    platform::UiTextId::settings_apply,
    platform::UiTextId::settings_cancel,
    platform::UiTextId::settings_saved_prefix,
    platform::UiTextId::settings_unknown,
    platform::UiTextId::hud_health,
    platform::UiTextId::hud_barrier,
    platform::UiTextId::hud_experience,
    platform::UiTextId::hud_level,
    platform::UiTextId::save_saving,
    platform::UiTextId::save_saved,
    platform::UiTextId::save_recovered,
    platform::UiTextId::save_error,
    platform::UiTextId::recovery_title,
    platform::UiTextId::recovery_invalid_slots,
    platform::UiTextId::inventory_title,
    platform::UiTextId::equipment_title,
    platform::UiTextId::item_detail_title,
    platform::UiTextId::passive_title,
    platform::UiTextId::passive_points,
    platform::UiTextId::passive_close,
    platform::UiTextId::debug_ready,
    platform::UiTextId::debug_unavailable,
    platform::UiTextId::unknown,
    platform::UiTextId::settings_invalid_binding,
    platform::UiTextId::settings_invalid,
    platform::UiTextId::settings_revision_limit,
    platform::UiTextId::settings_preview_failed,
    platform::UiTextId::settings_save_failed_retry,
    platform::UiTextId::settings_save_busy,
    platform::UiTextId::settings_rollback_failed,
    platform::UiTextId::settings_saved,
    platform::UiTextId::settings_reloaded_elsewhere,
    platform::UiTextId::settings_storage_conflict,
    platform::UiTextId::settings_recovered_defaults,
}};

constexpr bool pre_task4_ui_text_ordinals_are_stable() noexcept {
    for (std::size_t index = 0U; index < kPreTask4UiTextIds.size(); ++index) {
        if (static_cast<std::size_t>(kPreTask4UiTextIds[index]) != index)
            return false;
    }
    return true;
}

static_assert(pre_task4_ui_text_ordinals_are_stable(),
    "Pre-Task4 UiTextId ordinals 0-42 are a stable compatibility contract");
static_assert(static_cast<std::size_t>(platform::UiTextId::hud_experience_maxed)
        == 43U,
    "UiTextId ordinals 0-43 are a stable compatibility contract");
static_assert(static_cast<std::size_t>(
        platform::UiTextId::recovery_dual_slot_instruction) == 44U,
    "Recovery instruction must append after the stable UiTextId range");

void count_text(const char* text, void* context) noexcept {
    auto* const count = static_cast<std::size_t*>(context);
    if (text != nullptr && text[0] != '\0') ++(*count);
}

struct CatalogAudit final {
    std::array<bool, kApprovedInventoryProgressionSamples.size()> reached{};
    std::array<std::size_t, 3U> material_description_counts{};
    std::size_t count{};
    bool valid_utf8{true};
};

void audit_catalog_text(const char* text, void* context) noexcept {
    auto& audit = *static_cast<CatalogAudit*>(context);
    if (text == nullptr || text[0] == '\0') {
        audit.valid_utf8 = false;
        return;
    }
    ++audit.count;
    audit.valid_utf8 = audit.valid_utf8 && platform::ui_utf8_is_valid(text);
    for (std::size_t index{};
         index < kApprovedInventoryProgressionSamples.size(); ++index) {
        if (std::strcmp(text, kApprovedInventoryProgressionSamples[index]) == 0)
            audit.reached[index] = true;
    }
    if (std::strcmp(text, u8"选择物品应用") == 0)
        ++audit.material_description_counts[0];
    if (std::strcmp(text, u8"右键切换") == 0)
        ++audit.material_description_counts[1];
    if (std::strcmp(text, u8"强化装备") == 0)
        ++audit.material_description_counts[2];
}

std::size_t utf8_codepoint_count(const char* text) noexcept {
    std::size_t count{};
    for (const auto* byte = reinterpret_cast<const unsigned char*>(text);
         *byte != 0U; ++byte) {
        if ((*byte & 0xC0U) != 0x80U) ++count;
    }
    return count;
}

arpg::test::Failure fixed_messages_are_chinese() noexcept {
    constexpr std::array<std::pair<platform::UiTextId, const char*>, 81U>
        kExpectedTexts{{
            {platform::UiTextId::pause_title, u8"暂停"},
            {platform::UiTextId::pause_continue, u8"继续游戏"},
            {platform::UiTextId::pause_settings, u8"设置"},
            {platform::UiTextId::pause_quit, u8"退出游戏"},
            {platform::UiTextId::settings_title, u8"设置"},
            {platform::UiTextId::settings_audio, u8"音频设置"},
            {platform::UiTextId::settings_display, u8"显示设置"},
            {platform::UiTextId::settings_bindings, u8"按键设置"},
            {platform::UiTextId::settings_restore_defaults, u8"恢复默认设置"},
            {platform::UiTextId::settings_apply, u8"应用"},
            {platform::UiTextId::settings_cancel, u8"取消"},
            {platform::UiTextId::settings_saved_prefix, u8"已保存："},
            {platform::UiTextId::settings_unknown, u8"未知"},
            {platform::UiTextId::settings_invalid_binding, u8"该按键不可绑定"},
            {platform::UiTextId::settings_invalid, u8"设置无效"},
            {platform::UiTextId::settings_revision_limit, u8"版本号已达上限"},
            {platform::UiTextId::settings_preview_failed, u8"实时预览失败"},
            {platform::UiTextId::settings_save_failed_retry, u8"设置保存失败，请重试"},
            {platform::UiTextId::settings_save_busy,
                u8"另一游戏实例正在保存，请稍后重试"},
            {platform::UiTextId::settings_rollback_failed, u8"设置回滚失败"},
            {platform::UiTextId::settings_saved, u8"设置已保存"},
            {platform::UiTextId::settings_reloaded_elsewhere, u8"另一实例修改已重载"},
            {platform::UiTextId::settings_storage_conflict, u8"设置存储冲突，需重启"},
            {platform::UiTextId::settings_recovered_defaults, u8"设置已恢复默认值"},
            {platform::UiTextId::hud_health, u8"生命"},
            {platform::UiTextId::hud_barrier, u8"护盾"},
            {platform::UiTextId::hud_experience, u8"经验"},
            {platform::UiTextId::hud_level, u8"等级"},
            {platform::UiTextId::save_saving, u8"正在保存"},
            {platform::UiTextId::save_saved, u8"保存完成"},
            {platform::UiTextId::save_recovered, u8"已恢复存档"},
            {platform::UiTextId::save_error,
                u8"保存失败，当前进度未保存，请重试"},
            {platform::UiTextId::recovery_title, u8"需要恢复存档"},
            {platform::UiTextId::recovery_invalid_slots, u8"无效存档槽"},
            {platform::UiTextId::inventory_title, u8"背包"},
            {platform::UiTextId::equipment_title, u8"装备"},
            {platform::UiTextId::item_detail_title, u8"物品详情"},
            {platform::UiTextId::passive_title, u8"被动星图"},
            {platform::UiTextId::passive_points, u8"未分配点"},
            {platform::UiTextId::passive_close, u8"关闭"},
            {platform::UiTextId::debug_ready, u8"调试就绪"},
            {platform::UiTextId::debug_unavailable, u8"调试不可用"},
            {platform::UiTextId::unknown, u8"未知"},
            {platform::UiTextId::hud_experience_maxed, u8"经验已满"},
            {platform::UiTextId::recovery_dual_slot_instruction,
                u8"两个存档槽均无效。按 N 将其归档并创建新游戏。"},
            {platform::UiTextId::inventory_page_skill_stones, u8"技能石"},
            {platform::UiTextId::inventory_page_materials, u8"材料"},
            {platform::UiTextId::inventory_empty_title, u8"背包无装备"},
            {platform::UiTextId::inventory_empty_hint, u8"击败怪物，拾取装备"},
            {platform::UiTextId::inventory_all_equipped_title, u8"无剩余装备"},
            {platform::UiTextId::inventory_all_equipped_hint, u8"左击左方装备取出"},
            {platform::UiTextId::inventory_filter_empty_title, u8"当前过滤无结果"},
            {platform::UiTextId::inventory_filter_empty_hint, u8"切换装备槽及稀有度"},
            {platform::UiTextId::inventory_skill_empty, u8"暂无未装备主动技能石"},
            {platform::UiTextId::inventory_skill_equip_hint, u8"选择技能石后点击空槽装备"},
            {platform::UiTextId::inventory_skill_swap_hint, u8"选择两个技能槽交换"},
            {platform::UiTextId::inventory_skill_remove_hint, u8"选择已装备技能后可取出"},
            {platform::UiTextId::inventory_comparison_title, u8"装备后变化"},
            {platform::UiTextId::inventory_no_change, u8"装备后无变化"},
            {platform::UiTextId::inventory_material_selected_format, u8"已选择：%s · 切换至装备 · 右键取消"},
            {platform::UiTextId::inventory_material_rejected,
                u8"应用失败 · 切换装备 · 右键取消"},
            {platform::UiTextId::inventory_material_target_hint_format,
                u8"已选择：%s · 可用装备已标出 · 右键取消"},
            {platform::UiTextId::inventory_directed_material_hint,
                u8"已选择：定向核心 · 右键核心切换 · 右键装备取消"},
            {platform::UiTextId::inventory_recipe_action_format, u8"三合一（%u/3）"},
            {platform::UiTextId::inventory_recipe_danger, u8"危险：三合一取消强化"},
            {platform::UiTextId::inventory_slot_filter_format, u8"装备槽：%s"},
            {platform::UiTextId::inventory_rarity_filter_format, u8"稀有度：%s"},
            {platform::UiTextId::inventory_context_hint, u8"左键选择 · 快速左击两次装备 · 右键三合一"},
            {platform::UiTextId::inventory_action_remove, u8"取出"},
            {platform::UiTextId::inventory_filter_all, u8"全部"},
            {platform::UiTextId::inventory_reinforcement_success_format, u8"强化结果：成功 / +%u → +%u"},
            {platform::UiTextId::inventory_reinforcement_failure_format, u8"强化结果：失败 / +%u → +%u"},
            {platform::UiTextId::inventory_reinforcement_destroyed_format, u8"强化结果：失败 / 装备丢失 / +%u"},
            {platform::UiTextId::inventory_coupon_success_format, u8"强化券结果：成功 / +%u → +%u"},
            {platform::UiTextId::recovery_conflicting_slots_instruction,
                u8"两个有效存档冲突。按 N 归档并创建新游戏。"},
            {platform::UiTextId::save_faulted_title,
                u8"存档提交状态不确定"},
            {platform::UiTextId::save_faulted_instruction,
                u8"请退出并重启；启动时会重新校验双槽存档。"},
            {platform::UiTextId::save_faulted_error_format,
                u8"故障码 %u｜ESC：退出游戏"},
            {platform::UiTextId::save_directory_busy_title,
                u8"存档正在使用"},
            {platform::UiTextId::save_directory_busy_instruction,
                u8"另一游戏实例正在使用此存档。请关闭其他实例后重试。"},
            {platform::UiTextId::inventory_level_locked_format,
                u8"需要等级 %u（当前 %u）· 无法装备"},
        }};
    for (const auto& expected : kExpectedTexts) {
        ARPG_REQUIRE(std::strcmp(
            platform::ui_text(expected.first), expected.second) == 0);
    }

    constexpr std::array<const char*, 18U> kExpectedItemBaseNames{{
        u8"精炼之刃", u8"守卫头盔", u8"守护外衣", u8"突击手套",
        u8"行者战靴", u8"元素护符", u8"迅捷匕首", u8"攻城巨刃",
        u8"暗影兜帽", u8"堡垒头盔", u8"夜行者外衣", u8"城塞胸甲",
        u8"猎手护腕", u8"碎骨护手", u8"轻捷战靴", u8"铁护胫",
        u8"三元吊坠", u8"混沌之眼",
    }};
    for (std::size_t index{}; index < kExpectedItemBaseNames.size(); ++index) {
        ARPG_REQUIRE(std::strcmp(platform::ui_item_base_name(
            static_cast<std::uint8_t>(index + 1U)),
            kExpectedItemBaseNames[index]) == 0);
    }

    constexpr std::array<const char*, 14U> kExpectedMaterialNames{{
        u8"嬗变石", u8"增幅石", u8"富豪石", u8"混沌石", u8"崇高石",
        u8"剥离石", u8"神圣石", u8"重铸石", u8"定向核心", u8"强化石",
        u8"+6 强化券", u8"+9 强化券", u8"+12 强化券", u8"+15 强化券",
    }};
    const platform::UiGlyphPlan glyph_plan = platform::ui_glyph_plan();
    ARPG_REQUIRE(glyph_plan.codepoint_count == 647U);
    ARPG_REQUIRE(!glyph_plan.capacity_exceeded);
    for (std::size_t index{}; index < kExpectedMaterialNames.size(); ++index) {
        const auto id = static_cast<arpg::items::MaterialId>(index);
        ARPG_REQUIRE(std::strcmp(platform::ui_material_name(id),
            kExpectedMaterialNames[index]) == 0);
        const char* const description = platform::ui_material_description(id);
        ARPG_REQUIRE(description != nullptr && description[0] != '\0');
        ARPG_REQUIRE(std::strcmp(
            description, kExpectedMaterialDescriptions[index]) == 0);
        ARPG_REQUIRE(platform::ui_utf8_is_valid(description));
        ARPG_REQUIRE(platform::ui_glyph_plan_covers_text(
            glyph_plan, description));
    }

    ARPG_REQUIRE(std::strcmp(platform::ui_monster_affix_short_name(
        arpg::combat::MonsterAffixId::death_blast), u8"死爆") == 0);
    ARPG_REQUIRE(std::strcmp(platform::ui_skill_name(
        arpg::skills::ActiveSkillId::storm_swords),
        u8"极·鬼剑术（暴风式）") == 0);
    return {};
}

arpg::test::Failure all_stable_ids_have_named_mappings() noexcept {
    constexpr std::array<platform::UiTextId, 51U> texts{{
        platform::UiTextId::pause_title, platform::UiTextId::pause_continue,
        platform::UiTextId::pause_settings, platform::UiTextId::pause_quit,
        platform::UiTextId::settings_title, platform::UiTextId::settings_audio,
        platform::UiTextId::settings_display, platform::UiTextId::settings_bindings,
        platform::UiTextId::settings_restore_defaults, platform::UiTextId::settings_apply,
        platform::UiTextId::settings_cancel, platform::UiTextId::settings_saved_prefix,
        platform::UiTextId::settings_unknown,
        platform::UiTextId::settings_invalid_binding,
        platform::UiTextId::settings_invalid,
        platform::UiTextId::settings_revision_limit,
        platform::UiTextId::settings_preview_failed,
        platform::UiTextId::settings_save_failed_retry,
        platform::UiTextId::settings_save_busy,
        platform::UiTextId::settings_rollback_failed,
        platform::UiTextId::settings_saved,
        platform::UiTextId::settings_reloaded_elsewhere,
        platform::UiTextId::settings_storage_conflict,
        platform::UiTextId::settings_recovered_defaults,
        platform::UiTextId::hud_health,
        platform::UiTextId::hud_barrier, platform::UiTextId::hud_experience,
        platform::UiTextId::hud_level, platform::UiTextId::save_saving,
        platform::UiTextId::save_saved, platform::UiTextId::save_recovered,
        platform::UiTextId::save_error, platform::UiTextId::recovery_title,
        platform::UiTextId::recovery_invalid_slots, platform::UiTextId::inventory_title,
        platform::UiTextId::equipment_title, platform::UiTextId::item_detail_title,
        platform::UiTextId::passive_title, platform::UiTextId::passive_points,
        platform::UiTextId::passive_close, platform::UiTextId::debug_ready,
        platform::UiTextId::debug_unavailable, platform::UiTextId::unknown,
        platform::UiTextId::hud_experience_maxed,
        platform::UiTextId::recovery_dual_slot_instruction,
        platform::UiTextId::recovery_conflicting_slots_instruction,
        platform::UiTextId::save_faulted_title,
        platform::UiTextId::save_faulted_instruction,
        platform::UiTextId::save_faulted_error_format,
        platform::UiTextId::save_directory_busy_title,
        platform::UiTextId::save_directory_busy_instruction,
    }};
    for (const platform::UiTextId id : texts)
        ARPG_REQUIRE(platform::ui_text(id)[0] != '\0');

    for (std::uint8_t id = 1U; id <= 18U; ++id)
        ARPG_REQUIRE(std::strcmp(platform::ui_item_base_name(id), u8"未知") != 0);
    for (std::uint8_t raw = 0U; raw < 14U; ++raw)
        ARPG_REQUIRE(std::strcmp(platform::ui_material_name(
            static_cast<arpg::items::MaterialId>(raw)), u8"未知") != 0);
    for (std::uint8_t raw = 0U; raw < 14U; ++raw)
        ARPG_REQUIRE(std::strcmp(platform::ui_material_description(
            static_cast<arpg::items::MaterialId>(raw)), u8"未知") != 0);
    for (std::uint8_t raw = 0U; raw < 6U; ++raw)
        ARPG_REQUIRE(std::strcmp(platform::ui_item_slot_name(
            static_cast<arpg::items::ItemSlot>(raw)), u8"未知") != 0);
    for (std::uint8_t raw = 0U; raw < 3U; ++raw)
        ARPG_REQUIRE(std::strcmp(platform::ui_item_rarity_name(
            static_cast<arpg::items::ItemRarity>(raw)), u8"未知") != 0);
    for (std::uint16_t id = 1U; id <= 12U; ++id)
        ARPG_REQUIRE(std::strcmp(platform::ui_item_affix_name(id), u8"未知") != 0);
    for (std::uint16_t id = 101U; id <= 112U; ++id)
        ARPG_REQUIRE(std::strcmp(platform::ui_item_affix_name(id), u8"未知") != 0);
    for (std::uint8_t id = 0U; id < 64U; ++id)
        ARPG_REQUIRE(std::strcmp(platform::ui_passive_node_name(id), u8"未知") != 0);
    for (std::uint8_t raw = 0U; raw < 8U; ++raw)
        ARPG_REQUIRE(std::strcmp(platform::ui_monster_name(
            static_cast<arpg::combat::MonsterId>(raw)), u8"未知") != 0);
    for (std::uint8_t raw = 0U; raw < 12U; ++raw) {
        const auto id = static_cast<arpg::combat::MonsterAffixId>(raw);
        ARPG_REQUIRE(std::strcmp(platform::ui_monster_affix_name(id), u8"未知") != 0);
        ARPG_REQUIRE(std::strcmp(platform::ui_monster_affix_short_name(id), u8"未知") != 0);
    }
    for (std::uint8_t raw = 0U; raw < 10U; ++raw)
        ARPG_REQUIRE(std::strcmp(platform::ui_setting_action_name(
            static_cast<arpg::settings::SettingAction>(raw)), u8"未知") != 0);
    for (std::uint8_t raw = 0U; raw < 2U; ++raw)
        ARPG_REQUIRE(std::strcmp(platform::ui_skill_name(
            static_cast<arpg::skills::ActiveSkillId>(raw)), u8"未知") != 0);

    constexpr std::array<const char*, 8U> kExpectedMonsterNames{{
        u8"烈焰爆破者", u8"烈焰冲锋者", u8"水流壁垒", u8"水流援护者",
        u8"雷电射手", u8"雷电突进者", u8"混沌追猎者", u8"混沌危机体",
    }};
    for (std::size_t index{}; index < kExpectedMonsterNames.size(); ++index) {
        ARPG_REQUIRE(std::strcmp(platform::ui_monster_name(
            static_cast<arpg::combat::MonsterId>(index)),
            kExpectedMonsterNames[index]) == 0);
    }

    constexpr std::array<const char*, 12U> kExpectedMonsterAffixNames{{
        u8"强力", u8"狂热", u8"迅捷", u8"装甲", u8"护盾", u8"多重投射",
        u8"灼烧地面", u8"冰寒", u8"连锁闪电", u8"混沌腐蚀", u8"闪烁突袭",
        u8"死亡爆破",
    }};
    constexpr std::array<const char*, 12U> kExpectedMonsterAffixShortNames{{
        u8"强", u8"狂", u8"迅", u8"甲", u8"盾", u8"多射",
        u8"灼地", u8"寒", u8"连雷", u8"蚀", u8"闪袭", u8"死爆",
    }};
    for (std::size_t index{}; index < kExpectedMonsterAffixNames.size(); ++index) {
        const auto id = static_cast<arpg::combat::MonsterAffixId>(index);
        ARPG_REQUIRE(std::strcmp(platform::ui_monster_affix_name(id),
            kExpectedMonsterAffixNames[index]) == 0);
        const char* const short_name = platform::ui_monster_affix_short_name(id);
        ARPG_REQUIRE(std::strcmp(short_name,
            kExpectedMonsterAffixShortNames[index]) == 0);
        ARPG_REQUIRE(platform::ui_utf8_is_valid(short_name));
        const std::size_t codepoint_count = utf8_codepoint_count(short_name);
        ARPG_REQUIRE(codepoint_count >= 1U && codepoint_count <= 2U);
    }
    return {};
}

arpg::test::Failure unknown_ids_have_a_single_safe_fallback() noexcept {
    ARPG_REQUIRE(std::strcmp(platform::ui_text(platform::UiTextId::count), u8"未知") == 0);
    ARPG_REQUIRE(std::strcmp(platform::ui_item_base_name(255U), u8"未知") == 0);
    ARPG_REQUIRE(std::strcmp(platform::ui_material_name(
        static_cast<arpg::items::MaterialId>(255U)), u8"未知") == 0);
    ARPG_REQUIRE(std::strcmp(platform::ui_material_description(
        static_cast<arpg::items::MaterialId>(255U)), u8"未知") == 0);
    ARPG_REQUIRE(std::strcmp(platform::ui_item_slot_name(
        static_cast<arpg::items::ItemSlot>(255U)), u8"未知") == 0);
    ARPG_REQUIRE(std::strcmp(platform::ui_item_rarity_name(
        static_cast<arpg::items::ItemRarity>(255U)), u8"未知") == 0);
    ARPG_REQUIRE(std::strcmp(platform::ui_item_affix_name(65535U), u8"未知") == 0);
    ARPG_REQUIRE(std::strcmp(platform::ui_passive_node_name(255U), u8"未知") == 0);
    ARPG_REQUIRE(std::strcmp(platform::ui_monster_name(
        static_cast<arpg::combat::MonsterId>(255U)), u8"未知") == 0);
    ARPG_REQUIRE(std::strcmp(platform::ui_monster_affix_name(
        static_cast<arpg::combat::MonsterAffixId>(255U)), u8"未知") == 0);
    ARPG_REQUIRE(std::strcmp(platform::ui_monster_affix_short_name(
        static_cast<arpg::combat::MonsterAffixId>(255U)), u8"未知") == 0);
    ARPG_REQUIRE(std::strcmp(platform::ui_setting_action_name(
        static_cast<arpg::settings::SettingAction>(255U)), u8"未知") == 0);
    ARPG_REQUIRE(std::strcmp(platform::ui_skill_name(
        static_cast<arpg::skills::ActiveSkillId>(255U)), u8"未知") == 0);
    return {};
}

arpg::test::Failure visitor_covers_the_complete_static_catalog() noexcept {
    CatalogAudit audit{};
    platform::visit_ui_text_catalog(&audit_catalog_text, &audit);
    std::printf("[ui-text-catalog] visited=%zu approved=%zu valid_utf8=%d\n",
        audit.count, kApprovedInventoryProgressionSamples.size(),
        audit.valid_utf8 ? 1 : 0);
    ARPG_REQUIRE(audit.valid_utf8);
    for (const bool reached : audit.reached) ARPG_REQUIRE(reached);
    ARPG_REQUIRE(audit.material_description_counts[0] == 8U);
    ARPG_REQUIRE(audit.material_description_counts[1] == 1U);
    ARPG_REQUIRE(audit.material_description_counts[2] == 5U);
    ARPG_REQUIRE(audit.count == 353U);
    return {};
}

arpg::test::Failure catalog_lookups_and_visiting_do_not_allocate() noexcept {
    std::size_t warmup_count{};
    platform::visit_ui_text_catalog(&count_text, &warmup_count);
    const std::uint64_t before = arpg::test::allocation_count();
    for (std::size_t iteration = 0U; iteration < 1000U; ++iteration) {
        std::size_t count{};
        platform::visit_ui_text_catalog(&count_text, &count);
        ARPG_REQUIRE(count == 353U);
        ARPG_REQUIRE(std::strcmp(platform::ui_item_base_name(18U), u8"未知") != 0);
        ARPG_REQUIRE(std::strcmp(platform::ui_passive_node_name(63U), u8"未知") != 0);
        ARPG_REQUIRE(std::strcmp(platform::ui_monster_affix_short_name(
            arpg::combat::MonsterAffixId::death_blast), u8"死爆") == 0);
    }
    ARPG_REQUIRE(arpg::test::allocation_count() == before);
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"fixed messages are Chinese", &fixed_messages_are_chinese},
    {"all stable ids have named mappings", &all_stable_ids_have_named_mappings},
    {"unknown ids have a single safe fallback", &unknown_ids_have_a_single_safe_fallback},
    {"visitor covers the complete static catalog", &visitor_covers_the_complete_static_catalog},
    {"catalog lookups and visiting do not allocate", &catalog_lookups_and_visiting_do_not_allocate},
};

}  // namespace

arpg::test::TestSuite ui_text_catalog_suite() noexcept {
    return arpg::test::make_suite("ui_text_catalog", kCases);
}
