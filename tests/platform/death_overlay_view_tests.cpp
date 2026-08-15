#include "test_framework.hpp"
#include "allocation_probe.hpp"

#include "death_overlay_font.hpp"
#include "death_overlay_renderer.hpp"
#include "death_overlay_view.hpp"
#include "dungeon_view_math.hpp"
#include "stable_key_raylib.hpp"
#include "ui_text_catalog.hpp"
#include "ui_text_contrast.hpp"
#include "ui_text_layout.hpp"
#include "ui_utf8.hpp"

#include "combat/monster_affix_types.hpp"

#include <cstring>
#include <iterator>
#include <limits>

namespace {

namespace checkpoint = arpg::dungeon::checkpoint;
namespace dungeon = arpg::dungeon;
namespace platform = arpg::platform;
namespace settings = arpg::settings;

dungeon::DungeonSnapshot death_snapshot(
    checkpoint::DeathSourceKind source_kind,
    bool abyss = false) noexcept {
    dungeon::DungeonSnapshot snapshot{};
    snapshot.phase = dungeon::RoomPhase::death_pending;
    snapshot.death.emplace();
    auto& death = *snapshot.death;
    death.can_continue = true;
    auto& checkpoint = death.checkpoint;
    checkpoint.lifecycle = checkpoint::DeathLifecycle::pending_continue;
    checkpoint.data_version = checkpoint::kDeathCheckpointDataVersion;
    checkpoint.death_depth = 12U;
    checkpoint.death_floor_room_index = 7U;
    checkpoint.death_ecology = checkpoint::DungeonElement::lightning;
    checkpoint.death_was_abyss = abyss;
    checkpoint.source_kind = source_kind;
    checkpoint.source_monster_id = static_cast<std::uint8_t>(
        arpg::combat::MonsterId::fire_charger);
    checkpoint.source_detail_id = source_kind
            == checkpoint::DeathSourceKind::monster_affix
        ? static_cast<std::uint16_t>(
            arpg::combat::MonsterAffixId::burning_ground)
        : 0U;
    checkpoint.damage_type = checkpoint::DeathDamageType::fire;
    checkpoint.raw_damage = 123U;
    checkpoint.barrier_loss = 23U;
    checkpoint.health_loss = 77U;
    checkpoint.final_damage = 100U;
    checkpoint.recent_damage = {{11U, 22U, 33U, 44U, 55U}};
    checkpoint.hp = 0;
    checkpoint.max_hp = 500;
    checkpoint.barrier = 0;
    checkpoint.max_barrier = 200;
    checkpoint.armor = 1000;
    checkpoint.evasion = 2000;
    checkpoint.armor_reduction_bp = 4000;
    checkpoint.evasion_rate_bp = 3500;
    checkpoint.damage_reduction = {{1000, 2000, 3000, 4000}};
    checkpoint.damage_reduction_cap = {{7500, 7600, 7700, 7800}};
    checkpoint.target_room.depth = 11U;
    return snapshot;
}

bool contains(
    const platform::DeathOverlayView& view,
    const char* needle) noexcept {
    for (std::size_t index = 0U; index < view.line_count; ++index) {
        if (std::strstr(view.lines[index].text.data(), needle) != nullptr) {
            return true;
        }
    }
    return false;
}

bool ascii_only(const char* text) noexcept {
    for (const auto* byte = reinterpret_cast<const unsigned char*>(text);
         *byte != 0U; ++byte) {
        if (*byte > 0x7FU) return false;
    }
    return true;
}

bool valid_fixed_view_text(
    const platform::DeathOverlayView& view) noexcept {
    if (!platform::ui_utf8_is_valid(view.title.data())
            || !platform::ui_utf8_is_valid(view.prompt.data())
            || view.title.back() != '\0'
            || view.prompt.back() != '\0') {
        return false;
    }
    for (std::size_t index = 0U; index < view.line_count; ++index) {
        if (!platform::ui_utf8_is_valid(view.lines[index].text.data())
                || view.lines[index].text.back() != '\0') {
            return false;
        }
    }
    return true;
}

bool contains_formal_english_token(
    const platform::DeathOverlayView& view) noexcept {
    constexpr const char* kForbidden[] = {
        "DEATH RECAP", "Depth ", "Room ", "Source:", "Last hit",
        "Recent 5s", "Defense at death", "Return to", "Save failed",
        "Continue", "PHYSICAL DAMAGE", "FIRE DAMAGE", "WATER DAMAGE",
        "LIGHTNING DAMAGE", "CHAOS DAMAGE",
    };
    for (const char* token : kForbidden) {
        if (std::strstr(view.title.data(), token) != nullptr
                || std::strstr(view.prompt.data(), token) != nullptr) {
            return true;
        }
        for (std::size_t index = 0U; index < view.line_count; ++index) {
            if (std::strstr(view.lines[index].text.data(), token) != nullptr) {
                return true;
            }
        }
    }
    return false;
}

arpg::test::Failure hidden_without_death_and_maps_complete_recap() noexcept {
    dungeon::DungeonSnapshot live{};
    ARPG_REQUIRE(!platform::build_death_overlay_view(live).visible);

    const auto view = platform::build_death_overlay_view(
        death_snapshot(checkpoint::DeathSourceKind::monster_affix),
        u8"右 Shift");
    ARPG_REQUIRE(view.visible);
    ARPG_REQUIRE(contains(view, "第 12 层"));
    ARPG_REQUIRE(contains(view, "房间 7"));
    ARPG_REQUIRE(contains(view, "闪电"));
    ARPG_REQUIRE(contains(view, "普通房"));
    ARPG_REQUIRE(contains(view, "怪物词缀"));
    ARPG_REQUIRE(contains(view, "灼烧地面"));
    ARPG_REQUIRE(contains(view, "火焰伤害"));
    ARPG_REQUIRE(contains(view, "原始 123"));
    ARPG_REQUIRE(contains(view, "护盾损失 23"));
    ARPG_REQUIRE(contains(view, "生命损失 77"));
    ARPG_REQUIRE(contains(view, "最终 100"));
    ARPG_REQUIRE(contains(view, "物理 11"));
    ARPG_REQUIRE(contains(view, "火 22"));
    ARPG_REQUIRE(contains(view, "水 33"));
    ARPG_REQUIRE(contains(view, "电 44"));
    ARPG_REQUIRE(contains(view, "混沌 55"));
    ARPG_REQUIRE(contains(view, "生命 0/500"));
    ARPG_REQUIRE(contains(view, "护盾 0/200"));
    ARPG_REQUIRE(contains(view, "护甲 1000"));
    ARPG_REQUIRE(contains(view, "减伤 40.00%"));
    ARPG_REQUIRE(contains(view, "闪避 2000"));
    ARPG_REQUIRE(contains(view, "闪避率 35.00%"));
    ARPG_REQUIRE(contains(view, "火 10.00%/75.00%"));
    ARPG_REQUIRE(contains(view, "水 20.00%/76.00%"));
    ARPG_REQUIRE(contains(view, "电 30.00%/77.00%"));
    ARPG_REQUIRE(contains(view, "混沌 40.00%/78.00%"));
    ARPG_REQUIRE(contains(view, "第 12 层 → 第 11 层"));
    ARPG_REQUIRE(std::strcmp(view.prompt.data(),
        u8"右 Shift：继续｜ESC：退出游戏") == 0);
    ARPG_REQUIRE(valid_fixed_view_text(view));
    ARPG_REQUIRE(!contains_formal_english_token(view));
    return {};
}

arpg::test::Failure same_floor_recap_returns_to_local_entrance() noexcept {
    auto snapshot = death_snapshot(checkpoint::DeathSourceKind::monster_attack);
    snapshot.death->checkpoint.target_room.depth =
        snapshot.death->checkpoint.death_depth;

    const auto view = platform::build_death_overlay_view(snapshot);
    ARPG_REQUIRE(view.line_count == platform::kDeathOverlayLineCapacity);
    ARPG_REQUIRE(contains(view, "返回本层入口"));
    ARPG_REQUIRE(!contains(view, "第 12 层 → 第 12 层"));
    ARPG_REQUIRE(platform::death_overlay_font_covers_text(
        platform::death_overlay_font_plan(), "返回本层入口"));

    const auto ascii = platform::build_death_overlay_ascii_view(snapshot);
    ARPG_REQUIRE(ascii.line_count == platform::kDeathOverlayLineCapacity);
    ARPG_REQUIRE(contains(ascii, "Return to this floor entrance"));
    ARPG_REQUIRE(!contains(ascii, "Depth 12 -> Depth 12"));
    return {};
}

arpg::test::Failure maps_abyss_unknown_and_all_prompt_states() noexcept {
    auto abyss = death_snapshot(checkpoint::DeathSourceKind::unknown, true);
    abyss.death->checkpoint.source_monster_id = 0xFFU;
    const auto unknown = platform::build_death_overlay_view(abyss);
    ARPG_REQUIRE(contains(unknown, "深渊房"));
    ARPG_REQUIRE(contains(unknown, "未知来源"));

    auto saving = death_snapshot(checkpoint::DeathSourceKind::projectile);
    saving.death->saving = true;
    saving.death->can_continue = false;
    const auto saving_view = platform::build_death_overlay_view(saving, "Q");
    ARPG_REQUIRE(std::strcmp(saving_view.prompt.data(),
        u8"正在记录死亡｜ESC：退出游戏") == 0);
    ARPG_REQUIRE(platform::ui_utf8_is_valid(saving_view.prompt.data()));
    ARPG_REQUIRE(saving_view.prompt.back() == '\0');

    auto failed = death_snapshot(checkpoint::DeathSourceKind::monster_attack);
    failed.death->continue_failed = true;
    const auto failed_view = platform::build_death_overlay_view(failed, "Q");
    ARPG_REQUIRE(std::strcmp(failed_view.prompt.data(),
        u8"保存失败，Q：重试｜ESC：退出游戏") == 0);
    ARPG_REQUIRE(platform::ui_utf8_is_valid(failed_view.prompt.data()));
    ARPG_REQUIRE(failed_view.prompt.back() == '\0');

    auto ready = death_snapshot(checkpoint::DeathSourceKind::monster_attack);
    const auto ready_view = platform::build_death_overlay_view(
        ready, u8"小键盘5");
    ARPG_REQUIRE(std::strcmp(ready_view.prompt.data(),
        u8"小键盘5：继续｜ESC：退出游戏") == 0);
    ARPG_REQUIRE(platform::ui_utf8_is_valid(ready_view.prompt.data()));
    ARPG_REQUIRE(ready_view.prompt.back() == '\0');

    const auto right_shift_view = platform::build_death_overlay_view(
        failed, u8"右 Shift");
    ARPG_REQUIRE(std::strcmp(right_shift_view.prompt.data(),
        u8"保存失败，右 Shift：重试｜ESC：退出游戏") == 0);
    ARPG_REQUIRE(platform::ui_utf8_is_valid(right_shift_view.prompt.data()));
    ARPG_REQUIRE(right_shift_view.prompt.back() == '\0');

    const auto right_control_view = platform::build_death_overlay_view(
        failed, u8"右 Ctrl");
    ARPG_REQUIRE(std::strcmp(right_control_view.prompt.data(),
        u8"保存失败，右 Ctrl：重试｜ESC：退出游戏") == 0);
    ARPG_REQUIRE(platform::ui_utf8_is_valid(right_control_view.prompt.data()));
    ARPG_REQUIRE(right_control_view.prompt.back() == '\0');

    constexpr const char kLongestStableBindingPrompt[] =
        u8"保存失败，下方向键：重试｜ESC：退出游戏";
    static_assert(sizeof(kLongestStableBindingPrompt) == 58U);
    static_assert(sizeof(kLongestStableBindingPrompt) <= 64U);
    const auto longest_stable_binding_view = platform::build_death_overlay_view(
        failed, u8"下方向键");
    ARPG_REQUIRE(std::strcmp(longest_stable_binding_view.prompt.data(),
        kLongestStableBindingPrompt) == 0);
    ARPG_REQUIRE(platform::ui_utf8_is_valid(
        longest_stable_binding_view.prompt.data()));
    ARPG_REQUIRE(longest_stable_binding_view.prompt.back() == '\0');

    constexpr const char kFailedPromptPrefix[] = u8"保存失败，";
    constexpr const char kFailedPromptSuffix[] =
        u8"：重试｜ESC：退出游戏";
    for (std::size_t raw = 0U;
         raw < static_cast<std::size_t>(settings::StableKey::count); ++raw) {
        const char* label = platform::stable_key_label(
            static_cast<settings::StableKey>(raw));
        ARPG_REQUIRE(label != nullptr && label[0] != '\0');
        const auto stable_binding_view = platform::build_death_overlay_view(
            failed, label);
        ARPG_REQUIRE(std::strncmp(stable_binding_view.prompt.data(),
            kFailedPromptPrefix, sizeof(kFailedPromptPrefix) - 1U) == 0);
        const char* binding_begin = stable_binding_view.prompt.data()
            + sizeof(kFailedPromptPrefix) - 1U;
        const std::size_t label_size = std::strlen(label);
        ARPG_REQUIRE(std::strncmp(binding_begin, label, label_size) == 0);
        ARPG_REQUIRE(std::strcmp(binding_begin + label_size,
            kFailedPromptSuffix) == 0);
        ARPG_REQUIRE(platform::ui_utf8_is_valid(
            stable_binding_view.prompt.data()));
        ARPG_REQUIRE(stable_binding_view.prompt.back() == '\0');
    }
    return {};
}

arpg::test::Failure all_source_catalog_ids_have_stable_chinese_names() noexcept {
    constexpr const char* kMonsters[] = {
        "烈焰爆破者", "烈焰冲锋者", "水流壁垒", "水流援护者",
        "雷电射手", "雷电突进者", "混沌追猎者", "混沌危机体",
    };
    for (std::size_t index = 0U; index < std::size(kMonsters); ++index) {
        auto snapshot = death_snapshot(checkpoint::DeathSourceKind::monster_attack);
        snapshot.death->checkpoint.source_monster_id =
            static_cast<std::uint8_t>(index);
        const auto id = static_cast<arpg::combat::MonsterId>(index);
        ARPG_REQUIRE(std::strcmp(platform::ui_monster_name(id),
            kMonsters[index]) == 0);
        ARPG_REQUIRE(contains(platform::build_death_overlay_view(snapshot),
            platform::ui_monster_name(id)));
    }

    constexpr const char* kHazards[] = {
        "原生地面危险", "燃烧地面", "连锁闪电", "死亡爆破",
        "雷暴", "猎杀之焰", "混沌扩张",
    };
    for (std::size_t index = 0U; index < std::size(kHazards); ++index) {
        auto snapshot = death_snapshot(checkpoint::DeathSourceKind::ground_hazard);
        snapshot.death->checkpoint.source_detail_id =
            static_cast<std::uint16_t>(index);
        ARPG_REQUIRE(contains(platform::build_death_overlay_view(snapshot),
            kHazards[index]));
    }

    constexpr const char* kAffixes[] = {
        "强力", "狂热", "迅捷", "装甲", "护盾", "多重投射",
        "灼烧地面", "冰寒", "连锁闪电", "混沌腐蚀", "闪烁突袭", "死亡爆破",
    };
    for (std::size_t index = 0U; index < std::size(kAffixes); ++index) {
        auto snapshot = death_snapshot(checkpoint::DeathSourceKind::monster_affix);
        snapshot.death->checkpoint.source_detail_id =
            static_cast<std::uint16_t>(index);
        const auto id = static_cast<arpg::combat::MonsterAffixId>(index);
        ARPG_REQUIRE(std::strcmp(platform::ui_monster_affix_name(id),
            kAffixes[index]) == 0);
        ARPG_REQUIRE(contains(platform::build_death_overlay_view(snapshot),
            platform::ui_monster_affix_name(id)));
    }

    constexpr const char* kAbyssRules[] = {
        "雷暴", "猎杀之焰", "混沌扩张", "迅捷追猎",
        "深渊壁垒", "深渊狂怒", "沉重脚步", "疲惫恢复", "生命献祭",
    };
    for (std::size_t index = 0U; index < std::size(kAbyssRules); ++index) {
        auto snapshot = death_snapshot(checkpoint::DeathSourceKind::abyss_environment);
        snapshot.death->checkpoint.source_monster_id = 0xFFU;
        snapshot.death->checkpoint.source_detail_id =
            static_cast<std::uint16_t>(index);
        const auto rule = static_cast<arpg::abyss::AbyssRuleId>(index);
        ARPG_REQUIRE(std::strcmp(platform::abyss_rule_label(rule),
            kAbyssRules[index]) == 0);
        ARPG_REQUIRE(contains(platform::build_death_overlay_view(snapshot),
            platform::abyss_rule_label(rule)));
    }
    ARPG_REQUIRE(std::strcmp(platform::abyss_rule_label(
        arpg::abyss::AbyssRuleId::none), u8"无规则") == 0);

    constexpr struct DamageExpectation final {
        checkpoint::DeathDamageType type;
        const char* label;
    } kDamageTypes[] = {
        {checkpoint::DeathDamageType::physical, u8"物理伤害"},
        {checkpoint::DeathDamageType::fire, u8"火焰伤害"},
        {checkpoint::DeathDamageType::water, u8"水流伤害"},
        {checkpoint::DeathDamageType::lightning, u8"闪电伤害"},
        {checkpoint::DeathDamageType::chaos, u8"混沌伤害"},
    };
    for (const auto& expectation : kDamageTypes) {
        auto snapshot = death_snapshot(checkpoint::DeathSourceKind::projectile);
        snapshot.death->checkpoint.damage_type = expectation.type;
        const auto view = platform::build_death_overlay_view(snapshot);
        ARPG_REQUIRE(contains(view, u8"投射物"));
        ARPG_REQUIRE(contains(view, expectation.label));
        ARPG_REQUIRE(valid_fixed_view_text(view));
        ARPG_REQUIRE(!contains_formal_english_token(view));
    }

    constexpr checkpoint::DungeonElement kEcologies[] = {
        checkpoint::DungeonElement::fire,
        checkpoint::DungeonElement::water,
        checkpoint::DungeonElement::lightning,
        checkpoint::DungeonElement::chaos,
    };
    constexpr checkpoint::DeathSourceKind kAllSourceKinds[] = {
        checkpoint::DeathSourceKind::monster_attack,
        checkpoint::DeathSourceKind::projectile,
        checkpoint::DeathSourceKind::ground_hazard,
        checkpoint::DeathSourceKind::monster_affix,
        checkpoint::DeathSourceKind::abyss_environment,
        checkpoint::DeathSourceKind::unknown,
    };
    const auto glyph_plan = platform::death_overlay_font_plan();
    for (const auto ecology : kEcologies) {
        for (const auto source_kind : kAllSourceKinds) {
            for (const auto& damage : kDamageTypes) {
                auto snapshot = death_snapshot(source_kind,
                    source_kind == checkpoint::DeathSourceKind::abyss_environment);
                auto& death = snapshot.death->checkpoint;
                death.death_ecology = ecology;
                death.damage_type = damage.type;
                if (source_kind == checkpoint::DeathSourceKind::ground_hazard) {
                    death.source_detail_id = static_cast<std::uint16_t>(
                        arpg::combat::HazardKind::death_blast);
                } else if (source_kind
                        == checkpoint::DeathSourceKind::monster_affix) {
                    death.source_detail_id = static_cast<std::uint16_t>(
                        arpg::combat::MonsterAffixId::death_blast);
                } else if (source_kind
                        == checkpoint::DeathSourceKind::abyss_environment) {
                    death.source_monster_id = 0xFFU;
                    death.source_detail_id = static_cast<std::uint16_t>(
                        arpg::abyss::AbyssRuleId::life_sacrifice);
                }
                const auto view = platform::build_death_overlay_view(
                    snapshot, u8"右 Shift");
                ARPG_REQUIRE(valid_fixed_view_text(view));
                ARPG_REQUIRE(!contains_formal_english_token(view));
                ARPG_REQUIRE(platform::death_overlay_font_covers_text(
                    glyph_plan, view.title.data()));
                ARPG_REQUIRE(platform::death_overlay_font_covers_text(
                    glyph_plan, view.prompt.data()));
                for (std::size_t line = 0U; line < view.line_count; ++line) {
                    ARPG_REQUIRE(platform::death_overlay_font_covers_text(
                        glyph_plan, view.lines[line].text.data()));
                }
            }
        }
    }

    constexpr checkpoint::DeathSourceKind kCatalogKinds[] = {
        checkpoint::DeathSourceKind::monster_attack,
        checkpoint::DeathSourceKind::ground_hazard,
        checkpoint::DeathSourceKind::monster_affix,
        checkpoint::DeathSourceKind::abyss_environment,
    };
    for (const auto kind : kCatalogKinds) {
        auto snapshot = death_snapshot(kind);
        snapshot.death->checkpoint.source_monster_id = kind
                == checkpoint::DeathSourceKind::abyss_environment
            ? 0xFFU : 0xFEU;
        snapshot.death->checkpoint.source_detail_id = 0xFFFFU;
        ARPG_REQUIRE(contains(platform::build_death_overlay_view(snapshot),
            "未知来源"));
    }
    return {};
}

arpg::test::Failure wide_catalog_ids_never_alias_valid_entries() noexcept {
    constexpr std::uint16_t kWideIds[] = {
        256U, 257U, 512U, 513U, 0xFFFFU,
    };
    for (const std::uint16_t id : kWideIds) {
        auto hazard = death_snapshot(checkpoint::DeathSourceKind::ground_hazard);
        hazard.death->checkpoint.source_detail_id = id;
        ARPG_REQUIRE(contains(platform::build_death_overlay_view(hazard),
            "未知来源"));

        auto abyss = death_snapshot(checkpoint::DeathSourceKind::abyss_environment);
        abyss.death->checkpoint.source_monster_id = 0xFFU;
        abyss.death->checkpoint.source_detail_id = id;
        ARPG_REQUIRE(contains(platform::build_death_overlay_view(abyss),
            "未知来源"));
    }
    return {};
}

arpg::test::Failure ascii_fallback_maps_complete_recap_and_prompts() noexcept {
    auto snapshot = death_snapshot(checkpoint::DeathSourceKind::monster_affix);
    const auto view = platform::build_death_overlay_ascii_view(snapshot);
    ARPG_REQUIRE(view.visible);
    ARPG_REQUIRE(std::strcmp(view.title.data(), "DEATH RECAP") == 0);
    ARPG_REQUIRE(view.line_count == platform::kDeathOverlayLineCapacity);
    ARPG_REQUIRE(contains(view, "Depth 12"));
    ARPG_REQUIRE(contains(view, "Room 7"));
    ARPG_REQUIRE(contains(view, "LIGHTNING"));
    ARPG_REQUIRE(contains(view, "NORMAL"));
    ARPG_REQUIRE(contains(view, "MONSTER AFFIX"));
    ARPG_REQUIRE(contains(view, "BURNING GROUND"));
    ARPG_REQUIRE(contains(view, "FIRE CHARGER"));
    ARPG_REQUIRE(contains(view, "FIRE DAMAGE"));
    ARPG_REQUIRE(contains(view, "Raw 123"));
    ARPG_REQUIRE(contains(view, "Barrier loss 23"));
    ARPG_REQUIRE(contains(view, "HP loss 77"));
    ARPG_REQUIRE(contains(view, "Final 100"));
    ARPG_REQUIRE(contains(view, "Physical 11"));
    ARPG_REQUIRE(contains(view, "Fire 22"));
    ARPG_REQUIRE(contains(view, "Water 33"));
    ARPG_REQUIRE(contains(view, "Lightning 44"));
    ARPG_REQUIRE(contains(view, "Chaos 55"));
    ARPG_REQUIRE(contains(view, "HP 0/500"));
    ARPG_REQUIRE(contains(view, "Barrier 0/200"));
    ARPG_REQUIRE(contains(view, "Armor 1000"));
    ARPG_REQUIRE(contains(view, "Armor reduction 40.00%"));
    ARPG_REQUIRE(contains(view, "Evasion 2000"));
    ARPG_REQUIRE(contains(view, "Evasion rate 35.00%"));
    ARPG_REQUIRE(contains(view, "Fire 10.00%/75.00%"));
    ARPG_REQUIRE(contains(view, "Water 20.00%/76.00%"));
    ARPG_REQUIRE(contains(view, "Lightning 30.00%/77.00%"));
    ARPG_REQUIRE(contains(view, "Chaos 40.00%/78.00%"));
    ARPG_REQUIRE(contains(view, "Depth 12 -> Depth 11"));
    ARPG_REQUIRE(std::strcmp(view.prompt.data(), "E Continue") == 0);
    ARPG_REQUIRE(ascii_only(view.title.data()));
    ARPG_REQUIRE(ascii_only(view.prompt.data()));
    for (std::size_t index = 0U; index < view.line_count; ++index) {
        ARPG_REQUIRE(ascii_only(view.lines[index].text.data()));
    }

    auto unknown = death_snapshot(checkpoint::DeathSourceKind::ground_hazard, true);
    unknown.death->checkpoint.source_monster_id = 0xFFU;
    unknown.death->checkpoint.source_detail_id = 256U;
    const auto unknown_view = platform::build_death_overlay_ascii_view(unknown);
    ARPG_REQUIRE(contains(unknown_view, "ABYSS"));
    ARPG_REQUIRE(contains(unknown_view, "GROUND HAZARD / UNKNOWN / FIRE DAMAGE"));

    auto saving = snapshot;
    saving.death->saving = true;
    saving.death->can_continue = false;
    ARPG_REQUIRE(std::strcmp(platform::build_death_overlay_ascii_view(saving)
        .prompt.data(), "Saving death...") == 0);
    auto failed = snapshot;
    failed.death->continue_failed = true;
    ARPG_REQUIRE(std::strcmp(platform::build_death_overlay_ascii_view(failed)
        .prompt.data(), "Save failed - press E to retry") == 0);
    return {};
}

arpg::test::Failure font_plan_covers_all_overlay_text_and_ascii() noexcept {
    const auto plan = platform::death_overlay_font_plan();
    ARPG_REQUIRE(plan.codepoint_count > 95U);
    ARPG_REQUIRE(std::strcmp(plan.candidate_paths[0],
        "assets/fonts/NotoSansCJKsc-Medium.otf") == 0);
    ARPG_REQUIRE(plan.candidate_count == 1U);
    for (int codepoint = 32; codepoint <= 126; ++codepoint) {
        ARPG_REQUIRE(platform::death_overlay_font_has_codepoint(plan, codepoint));
    }

    for (std::uint8_t monster = 0U;
         monster < static_cast<std::uint8_t>(arpg::combat::MonsterId::count);
         ++monster) {
        auto snapshot = death_snapshot(checkpoint::DeathSourceKind::monster_attack);
        snapshot.death->checkpoint.source_monster_id = monster;
        const auto view = platform::build_death_overlay_view(snapshot);
        ARPG_REQUIRE(valid_fixed_view_text(view));
        ARPG_REQUIRE(!contains_formal_english_token(view));
        ARPG_REQUIRE(platform::death_overlay_font_covers_text(
            plan, view.title.data()));
        ARPG_REQUIRE(platform::death_overlay_font_covers_text(
            plan, view.prompt.data()));
        for (std::size_t index = 0U; index < view.line_count; ++index) {
            ARPG_REQUIRE(platform::death_overlay_font_covers_text(
                plan, view.lines[index].text.data()));
        }
    }
    ARPG_REQUIRE(plan.codepoint_count == 639U);
    ARPG_REQUIRE(plan.codepoint_count < platform::kUiGlyphCapacity);
    return {};
}

bool inside(
    platform::DeathOverlayRect inner,
    platform::DeathOverlayRect outer) noexcept {
    return inner.x >= outer.x && inner.y >= outer.y
        && inner.x + inner.width <= outer.x + outer.width
        && inner.y + inner.height <= outer.y + outer.height;
}

bool overlaps(
    platform::DeathOverlayRect lhs,
    platform::DeathOverlayRect rhs) noexcept {
    return lhs.x < rhs.x + rhs.width && rhs.x < lhs.x + lhs.width
        && lhs.y < rhs.y + rhs.height && rhs.y < lhs.y + lhs.height;
}

arpg::test::Failure layouts_stay_in_bounds_and_clear_of_prompt() noexcept {
    constexpr struct Viewport { int width; int height; } viewports[] = {
        {800, 450}, {1280, 720}, {1920, 1080},
    };
    for (const auto viewport : viewports) {
        const auto view = platform::build_death_overlay_view(
            death_snapshot(checkpoint::DeathSourceKind::monster_affix));
        const auto layout = platform::death_overlay_layout(
            view, viewport.width, viewport.height);
        const platform::DeathOverlayRect screen{
            0.0F, 0.0F,
            static_cast<float>(viewport.width),
            static_cast<float>(viewport.height)};
        ARPG_REQUIRE(inside(layout.panel, screen));
        ARPG_REQUIRE(inside(layout.title, layout.panel));
        ARPG_REQUIRE(inside(layout.prompt, layout.panel));
        const float panel_center = layout.panel.x + layout.panel.width * 0.5F;
        ARPG_REQUIRE(arpg::test::near(
            layout.title.x + layout.title.width * 0.5F, panel_center));
        ARPG_REQUIRE(arpg::test::near(
            layout.prompt.x + layout.prompt.width * 0.5F, panel_center));
        for (std::size_t index = 0U; index < view.line_count; ++index) {
            const auto bounds = layout.line_bounds[index];
            ARPG_REQUIRE(inside(bounds, layout.panel));
            ARPG_REQUIRE(!overlaps(bounds, layout.title));
            ARPG_REQUIRE(!overlaps(bounds, layout.prompt));
            for (std::size_t other = index + 1U;
                 other < view.line_count; ++other) {
                ARPG_REQUIRE(!overlaps(bounds, layout.line_bounds[other]));
            }
        }
    }

    const platform::DeathOverlayTextStyle style =
        platform::death_overlay_text_style();
    ARPG_REQUIRE(style.backing.a == 255U);
    for (const Color color : {
             style.primary, style.danger, style.warning, style.error}) {
        ARPG_REQUIRE(color.a == 255U);
        ARPG_REQUIRE(platform::ui_luma_contrast_ratio(
            color, style.backing) >= 4.5F);
    }
    return {};
}

arpg::test::Failure desktop_layout_is_compact_and_hierarchical() noexcept {
    const auto view = platform::build_death_overlay_view(
        death_snapshot(checkpoint::DeathSourceKind::monster_affix));
    const auto layout = platform::death_overlay_layout(view, 1280, 720);

    ARPG_REQUIRE(arpg::test::near(layout.panel.x, 180.0F));
    ARPG_REQUIRE(arpg::test::near(layout.panel.y, 80.0F));
    ARPG_REQUIRE(arpg::test::near(layout.panel.width, 920.0F));
    ARPG_REQUIRE(arpg::test::near(layout.panel.height, 560.0F));
    ARPG_REQUIRE(layout.title_font_size == 32);
    ARPG_REQUIRE(layout.heading_font_size == 22);
    ARPG_REQUIRE(layout.body_font_size == 20);
    ARPG_REQUIRE(layout.prompt_font_size == 24);
    ARPG_REQUIRE(layout.title_font_size > layout.heading_font_size);
    ARPG_REQUIRE(layout.heading_font_size > layout.body_font_size);
    ARPG_REQUIRE(layout.prompt_font_size >= layout.body_font_size);

    for (std::size_t index = 0U; index < view.line_count; ++index) {
        const float required_height = static_cast<float>(
            (view.lines[index].heading
                    ? layout.heading_font_size : layout.body_font_size)
                + 4);
        ARPG_REQUIRE(layout.line_bounds[index].height >= required_height);
    }
    return {};
}

arpg::test::Failure layout_uses_each_line_column_metadata() noexcept {
    platform::DeathOverlayView view{};
    view.visible = true;
    view.line_count = 4U;
    view.lines[0].column = platform::DeathOverlayColumn::right;
    view.lines[1].column = platform::DeathOverlayColumn::left;
    view.lines[2].column = platform::DeathOverlayColumn::full;
    view.lines[3].column = platform::DeathOverlayColumn::right;

    const auto layout = platform::death_overlay_layout(view, 1280, 720);
    const float panel_center = layout.panel.x + layout.panel.width * 0.5F;
    ARPG_REQUIRE(layout.line_bounds[0].x >= panel_center);
    ARPG_REQUIRE(layout.line_bounds[1].x < panel_center);
    ARPG_REQUIRE(layout.line_bounds[2].width > layout.line_bounds[0].width);
    ARPG_REQUIRE(arpg::test::near(
        layout.line_bounds[0].y, layout.line_bounds[1].y));
    ARPG_REQUIRE(layout.line_bounds[3].y > layout.line_bounds[0].y);
    return {};
}

std::size_t utf8_glyph_count(const char* text) noexcept {
    std::size_t count = 0U;
    for (const auto* byte = reinterpret_cast<const unsigned char*>(text);
         *byte != 0U; ++byte) {
        if ((*byte & 0xC0U) != 0x80U) ++count;
    }
    return count;
}

platform::UiTextExtent measure_death_text_for_test(
    const char* text, float font_size, float spacing, void*) noexcept {
    const float glyphs = static_cast<float>(utf8_glyph_count(text));
    return {
        glyphs * font_size * 0.55F
            + (glyphs > 1.0F ? (glyphs - 1.0F) * spacing : 0.0F),
        font_size,
    };
}

arpg::test::Failure worst_case_values_fit_compact_columns() noexcept {
    auto snapshot = death_snapshot(checkpoint::DeathSourceKind::monster_affix);
    auto& death = snapshot.death->checkpoint;
    death.raw_damage = (std::numeric_limits<std::uint64_t>::max)();
    death.barrier_loss = death.raw_damage;
    death.health_loss = death.raw_damage;
    death.final_damage = death.raw_damage;
    death.recent_damage.fill(death.raw_damage);
    death.hp = (std::numeric_limits<std::int32_t>::min)();
    death.max_hp = (std::numeric_limits<std::int32_t>::max)();
    death.barrier = death.hp;
    death.max_barrier = death.max_hp;
    death.armor = (std::numeric_limits<std::int64_t>::max)();
    death.evasion = death.armor;
    death.armor_reduction_bp = (std::numeric_limits<std::int32_t>::min)();
    death.evasion_rate_bp = death.armor_reduction_bp;
    death.damage_reduction.fill(death.armor_reduction_bp);
    death.damage_reduction_cap.fill((std::numeric_limits<std::int32_t>::max)());

    const auto view = platform::build_death_overlay_view(snapshot);
    ARPG_REQUIRE(valid_fixed_view_text(view));
    for (std::size_t index = 0U; index < view.line_count; ++index) {
        if (view.lines[index].column != platform::DeathOverlayColumn::full) {
            ARPG_REQUIRE(utf8_glyph_count(view.lines[index].text.data()) <= 36U);
        }
    }
    const auto ascii_view = platform::build_death_overlay_ascii_view(snapshot);
    for (std::size_t index = 0U; index < ascii_view.line_count; ++index) {
        if (ascii_view.lines[index].column != platform::DeathOverlayColumn::full) {
            ARPG_REQUIRE(utf8_glyph_count(
                ascii_view.lines[index].text.data()) <= 36U);
        }
    }

    constexpr const char* kLongBinding =
        u8"右小键盘5右小键盘5右小键盘5右小键盘5"
        u8"右小键盘5右小键盘5右小键盘5右小键盘5";
    const auto long_prompt = platform::build_death_overlay_view(
        snapshot, kLongBinding);
    ARPG_REQUIRE(valid_fixed_view_text(long_prompt));
    ARPG_REQUIRE(long_prompt.prompt.back() == '\0');

    const auto compact_layout = platform::death_overlay_layout(
        long_prompt, 800, 450);
    const Rectangle prompt_bounds{
        compact_layout.prompt.x,
        compact_layout.prompt.y,
        compact_layout.prompt.width,
        compact_layout.prompt.height,
    };
    std::array<char, platform::kDeathOverlayTextCapacity> fitted{};
    const auto fit = platform::fit_centered_ui_text(
        fitted.data(), fitted.size(), long_prompt.prompt.data(), prompt_bounds,
        static_cast<float>(compact_layout.prompt_font_size), 9.0F, 1.0F,
        &measure_death_text_for_test, nullptr);
    ARPG_REQUIRE(fit.drawable);
    ARPG_REQUIRE(platform::ui_utf8_is_valid(fit.text));
    ARPG_REQUIRE(fit.position.x >= prompt_bounds.x);
    ARPG_REQUIRE(fit.position.y >= prompt_bounds.y);
    ARPG_REQUIRE(fit.position.x + fit.size.x
        <= prompt_bounds.x + prompt_bounds.width);
    ARPG_REQUIRE(fit.position.y + fit.size.y
        <= prompt_bounds.y + prompt_bounds.height);

    const std::uint64_t allocations_before = arpg::test::allocation_count();
    std::uint64_t checksum{};
    for (std::size_t iteration = 0U; iteration < 100000U; ++iteration) {
        const auto generated = platform::build_death_overlay_view(
            snapshot, u8"右 Shift");
        checksum += generated.line_count;
        checksum += static_cast<unsigned char>(generated.prompt[0]);
    }
    ARPG_REQUIRE(arpg::test::allocation_count() == allocations_before);
    ARPG_REQUIRE(checksum != 0U);
    return {};
}

arpg::test::Failure death_overlay_material_plan_selects_authored_ui() noexcept {
    const platform::DeathOverlayMaterialPlan hidden =
        platform::death_overlay_material_plan(false);
    ARPG_REQUIRE(!hidden.visible);

    const platform::DeathOverlayMaterialPlan visible =
        platform::death_overlay_material_plan(true);
    ARPG_REQUIRE(visible.visible);
    ARPG_REQUIRE(visible.panel
        == platform::MaterialSpriteId::ui_warning_modal);
    ARPG_REQUIRE(visible.title_plate
        == platform::MaterialSpriteId::ui_label_plate);
    ARPG_REQUIRE(arpg::test::near(visible.panel_border_pixels, 32.0F));
    ARPG_REQUIRE(visible.dimmer_alpha == 232U);
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"hidden and complete recap mapping", &hidden_without_death_and_maps_complete_recap},
    {"same floor returns to local entrance",
        &same_floor_recap_returns_to_local_entrance},
    {"abyss unknown and prompt states", &maps_abyss_unknown_and_all_prompt_states},
    {"all source ids have Chinese names", &all_source_catalog_ids_have_stable_chinese_names},
    {"wide source ids never alias", &wide_catalog_ids_never_alias_valid_entries},
    {"ASCII fallback maps complete recap", &ascii_fallback_maps_complete_recap_and_prompts},
    {"font plan covers overlay text", &font_plan_covers_all_overlay_text_and_ascii},
    {"layouts fit supported windows", &layouts_stay_in_bounds_and_clear_of_prompt},
    {"desktop layout is compact and hierarchical",
        &desktop_layout_is_compact_and_hierarchical},
    {"layout uses line column metadata",
        &layout_uses_each_line_column_metadata},
    {"worst case values fit compact columns", &worst_case_values_fit_compact_columns},
    {"death overlay selects authored UI materials",
        &death_overlay_material_plan_selects_authored_ui},
};

}  // namespace

arpg::test::TestSuite death_overlay_view_suite() noexcept {
    return arpg::test::make_suite("death_overlay_view", kCases);
}
