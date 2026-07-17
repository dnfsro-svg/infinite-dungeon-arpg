#include "test_framework.hpp"

#include "death_overlay_view.hpp"

#include "combat/monster_affix_types.hpp"

#include <cstring>
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
    ARPG_REQUIRE(contains(view, "Burning Ground"));
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
        {1280, 720}, {800, 450},
    };
    for (const auto viewport : viewports) {
        const auto layout = platform::death_overlay_layout(
            viewport.width, viewport.height);
        const platform::DeathOverlayRect screen{
            0.0F, 0.0F,
            static_cast<float>(viewport.width),
            static_cast<float>(viewport.height)};
        ARPG_REQUIRE(inside(layout.panel, screen));
        ARPG_REQUIRE(inside(layout.title, layout.panel));
        ARPG_REQUIRE(inside(layout.prompt, layout.panel));
        for (const auto bounds : layout.line_bounds) {
            ARPG_REQUIRE(inside(bounds, layout.panel));
            ARPG_REQUIRE(!overlaps(bounds, layout.prompt));
        }
    }
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
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"hidden and complete recap mapping", &hidden_without_death_and_maps_complete_recap},
    {"abyss unknown and prompt states", &maps_abyss_unknown_and_all_prompt_states},
    {"layouts fit supported windows", &layouts_stay_in_bounds_and_clear_of_prompt},
    {"worst case values fit compact columns", &worst_case_values_fit_compact_columns},
};

}  // namespace

arpg::test::TestSuite death_overlay_view_suite() noexcept {
    return arpg::test::make_suite("death_overlay_view", kCases);
}
