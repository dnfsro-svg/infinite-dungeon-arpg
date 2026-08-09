#include "test_framework.hpp"

#include "death_overlay_view.hpp"
#include "death_overlay_font.hpp"

#include "combat/monster_affix_types.hpp"

#include <cstring>
#include <iterator>
#include <limits>

namespace {

namespace checkpoint = arpg::dungeon::checkpoint;
namespace dungeon = arpg::dungeon;
namespace platform = arpg::platform;

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

arpg::test::Failure hidden_without_death_and_maps_complete_recap() noexcept {
    dungeon::DungeonSnapshot live{};
    ARPG_REQUIRE(!platform::build_death_overlay_view(live).visible);

    const auto view = platform::build_death_overlay_view(
        death_snapshot(checkpoint::DeathSourceKind::monster_affix));
    ARPG_REQUIRE(view.visible);
    ARPG_REQUIRE(contains(view, "第 12 层"));
    ARPG_REQUIRE(contains(view, "房间 7"));
    ARPG_REQUIRE(contains(view, "闪电"));
    ARPG_REQUIRE(contains(view, "普通房"));
    ARPG_REQUIRE(contains(view, "怪物词缀"));
    ARPG_REQUIRE(contains(view, "燃烧地面"));
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
    ARPG_REQUIRE(std::strcmp(view.prompt.data(), "E 继续") == 0);
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
    ARPG_REQUIRE(std::strcmp(platform::build_death_overlay_view(saving)
        .prompt.data(), "正在记录死亡") == 0);

    auto failed = death_snapshot(checkpoint::DeathSourceKind::monster_attack);
    failed.death->continue_failed = true;
    ARPG_REQUIRE(std::strcmp(platform::build_death_overlay_view(failed)
        .prompt.data(), "保存失败，请重试") == 0);
    return {};
}

arpg::test::Failure all_source_catalog_ids_have_stable_chinese_names() noexcept {
    constexpr const char* kMonsters[] = {
        "火焰投弹者", "火焰冲锋者", "水之壁垒", "水之支援者",
        "闪电射手", "闪电突袭者", "混沌追猎者", "混沌灾术师",
    };
    for (std::size_t index = 0U; index < std::size(kMonsters); ++index) {
        auto snapshot = death_snapshot(checkpoint::DeathSourceKind::monster_attack);
        snapshot.death->checkpoint.source_monster_id =
            static_cast<std::uint8_t>(index);
        ARPG_REQUIRE(contains(platform::build_death_overlay_view(snapshot),
            kMonsters[index]));
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
        "燃烧地面", "寒冷", "连锁闪电", "混沌腐蚀", "闪现突袭", "死亡爆破",
    };
    for (std::size_t index = 0U; index < std::size(kAffixes); ++index) {
        auto snapshot = death_snapshot(checkpoint::DeathSourceKind::monster_affix);
        snapshot.death->checkpoint.source_detail_id =
            static_cast<std::uint16_t>(index);
        ARPG_REQUIRE(contains(platform::build_death_overlay_view(snapshot),
            kAffixes[index]));
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
        ARPG_REQUIRE(contains(platform::build_death_overlay_view(snapshot),
            kAbyssRules[index]));
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
        ARPG_REQUIRE(platform::death_overlay_font_covers_text(
            plan, view.title.data()));
        ARPG_REQUIRE(platform::death_overlay_font_covers_text(
            plan, view.prompt.data()));
        for (std::size_t index = 0U; index < view.line_count; ++index) {
            ARPG_REQUIRE(platform::death_overlay_font_covers_text(
                plan, view.lines[index].text.data()));
        }
    }
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
