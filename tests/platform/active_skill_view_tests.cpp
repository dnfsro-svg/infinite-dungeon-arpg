#include "test_framework.hpp"

#include "active_skill_renderer.hpp"
#include "active_skill_view.hpp"
#include "combat/active_skill_runtime.hpp"
#include "skills/active_skill_catalog.hpp"
#include "skills/skill_loadout.hpp"

#include <array>
#include <cstddef>
#include <cstring>

namespace {

namespace combat = arpg::combat;
namespace platform = arpg::platform;
namespace skills = arpg::skills;

bool overlaps(Rectangle left, Rectangle right) noexcept {
    return left.x < right.x + right.width
        && right.x < left.x + left.width
        && left.y < right.y + right.height
        && right.y < left.y + left.height;
}

arpg::test::Failure hud_projects_exactly_five_numbered_slots_and_catalog_names()
    noexcept {
    const skills::SkillLoadoutState loadout = skills::default_skill_loadout();
    const platform::ActiveSkillHudModel view =
        platform::make_active_skill_hud_model(loadout, {});

    ARPG_REQUIRE(view.slots.size() == skills::kActiveSkillSlotCount);
    for (std::size_t index = 0U; index < view.slots.size(); ++index) {
        ARPG_REQUIRE(view.slots[index].key_number == index + 1U);
    }
    ARPG_REQUIRE(!view.slots[0U].empty);
    ARPG_REQUIRE(!view.slots[1U].empty);
    ARPG_REQUIRE(view.slots[2U].empty);
    ARPG_REQUIRE(view.slots[3U].empty);
    ARPG_REQUIRE(view.slots[4U].empty);
    ARPG_REQUIRE(std::strcmp(view.slots[0U].name.data(), u8"拔刀斩") == 0);
    ARPG_REQUIRE(std::strcmp(
        view.slots[1U].name.data(), u8"极·鬼剑术（暴风式）") == 0);
    ARPG_REQUIRE(view.slots[2U].name[0U] == '\0');
    return {};
}

arpg::test::Failure hud_cooldown_ratios_are_clamped_and_empty_slots_stay_zero()
    noexcept {
    const skills::SkillLoadoutState loadout = skills::default_skill_loadout();
    std::array<std::uint16_t, skills::kActiveSkillCount> cooldowns{{
        static_cast<std::uint16_t>(skills::kDrawSlashCooldownTicks / 2U),
        0xFFFFU,
    }};
    const platform::ActiveSkillHudModel view =
        platform::make_active_skill_hud_model(loadout, cooldowns);

    ARPG_REQUIRE(arpg::test::near(view.slots[0U].cooldown_ratio, 0.5F));
    ARPG_REQUIRE(arpg::test::near(view.slots[1U].cooldown_ratio, 1.0F));
    for (std::size_t index = 2U; index < view.slots.size(); ++index) {
        ARPG_REQUIRE(arpg::test::near(
            view.slots[index].cooldown_ratio, 0.0F));
    }
    return {};
}

arpg::test::Failure hud_layout_is_bottom_centered_with_fixed_slot_geometry()
    noexcept {
    constexpr std::array<std::array<int, 2>, 3> kViewports{{
        {{1280, 720}}, {{1600, 900}}, {{1920, 1080}},
    }};
    constexpr float kExpectedWidth = 5.0F * 58.0F + 4.0F * 8.0F;
    for (const auto viewport : kViewports) {
        const platform::ActiveSkillHudLayout layout =
            platform::active_skill_hud_layout(viewport[0], viewport[1]);
        ARPG_REQUIRE(arpg::test::near(layout.bounds.width, kExpectedWidth));
        ARPG_REQUIRE(arpg::test::near(layout.bounds.x
            + layout.bounds.width * 0.5F,
            static_cast<float>(viewport[0]) * 0.5F));
        ARPG_REQUIRE(layout.bounds.y + layout.bounds.height
            <= static_cast<float>(viewport[1]));
        for (std::size_t index = 0U; index < layout.slots.size(); ++index) {
            ARPG_REQUIRE(arpg::test::near(layout.slots[index].width, 58.0F));
            ARPG_REQUIRE(arpg::test::near(layout.slots[index].height, 58.0F));
            if (index != 0U) {
                ARPG_REQUIRE(arpg::test::near(layout.slots[index].x
                    - (layout.slots[index - 1U].x
                        + layout.slots[index - 1U].width), 8.0F));
                ARPG_REQUIRE(!overlaps(
                    layout.slots[index - 1U], layout.slots[index]));
            }
        }
    }
    return {};
}

arpg::test::Failure native_effect_plan_uses_snapshot_timing_and_twelve_swords()
    noexcept {
    combat::CombatSnapshot snapshot{};
    snapshot.player.facing = combat::Facing::left;
    snapshot.active_skill.id = skills::ActiveSkillId::draw_slash;
    snapshot.active_skill.phase = combat::ActiveSkillPhase::strikes;
    snapshot.active_skill.elapsed_ticks = combat::kDrawSlashStartupTicks;
    snapshot.active_skill.locked_center = {2.0F, 3.0F, 0.0F};
    platform::ActiveSkillEffectPlan plan =
        platform::make_active_skill_effect_plan(snapshot, nullptr);
    ARPG_REQUIRE(plan.draw_slash.visible);
    ARPG_REQUIRE(plan.draw_slash.facing == combat::Facing::left);
    ARPG_REQUIRE(arpg::test::near(plan.draw_slash.center.x, 2.0F));
    snapshot.active_skill.elapsed_ticks = static_cast<std::uint16_t>(
        combat::kDrawSlashStartupTicks + 8U);
    plan = platform::make_active_skill_effect_plan(snapshot, nullptr);
    ARPG_REQUIRE(!plan.draw_slash.visible);

    snapshot.active_skill.id = skills::ActiveSkillId::storm_swords;
    snapshot.active_skill.phase = combat::ActiveSkillPhase::strikes;
    snapshot.active_skill.strike_index = 4U;
    plan = platform::make_active_skill_effect_plan(snapshot, nullptr);
    ARPG_REQUIRE(plan.storm_swords.visible);
    ARPG_REQUIRE(plan.storm_swords.sword_count == combat::kStormStrikeCount);
    for (std::size_t index = 0U; index < plan.storm_swords.swords.size(); ++index) {
        ARPG_REQUIRE(plan.storm_swords.swords[index].highlighted
            == (index == 3U));
    }

    combat::CombatEvent finisher{};
    finisher.tick = 100U;
    finisher.skill = skills::ActiveSkillId::storm_swords;
    finisher.finisher = true;
    snapshot.tick = 103U;
    snapshot.active_skill.phase = combat::ActiveSkillPhase::recovery;
    plan = platform::make_active_skill_effect_plan(snapshot, &finisher);
    ARPG_REQUIRE(plan.storm_swords.finisher_visible);
    ARPG_REQUIRE(plan.screen_flash_alpha > 0.0F);
    snapshot.tick = 108U;
    plan = platform::make_active_skill_effect_plan(snapshot, &finisher);
    ARPG_REQUIRE(!plan.storm_swords.finisher_visible);
    ARPG_REQUIRE(arpg::test::near(plan.screen_flash_alpha, 0.0F));
    return {};
}

arpg::test::Failure storm_finisher_persists_from_snapshot_without_hit_event()
    noexcept {
    constexpr std::uint16_t kFinisherTick = static_cast<std::uint16_t>(
        combat::kStormStartupTicks
        + static_cast<std::uint16_t>(combat::kStormStrikeCount)
            * combat::kStormStrikeIntervalTicks);
    combat::CombatSnapshot snapshot{};
    snapshot.active_skill.id = skills::ActiveSkillId::storm_swords;
    snapshot.active_skill.locked_center = {4.0F, -2.0F, 0.0F};
    snapshot.active_skill.phase = combat::ActiveSkillPhase::finisher;
    snapshot.active_skill.elapsed_ticks = kFinisherTick;
    platform::ActiveSkillEffectPlan plan =
        platform::make_active_skill_effect_plan(snapshot, nullptr);
    ARPG_REQUIRE(plan.storm_swords.finisher_visible);
    ARPG_REQUIRE(plan.screen_flash_alpha > 0.0F);

    snapshot.active_skill.phase = combat::ActiveSkillPhase::recovery;
    for (std::uint16_t age = 1U; age <= 7U; ++age) {
        snapshot.active_skill.elapsed_ticks = static_cast<std::uint16_t>(
            kFinisherTick + age);
        plan = platform::make_active_skill_effect_plan(snapshot, nullptr);
        ARPG_REQUIRE(plan.storm_swords.finisher_visible);
        ARPG_REQUIRE(plan.storm_swords.finisher_opacity > 0.0F);
        ARPG_REQUIRE(plan.screen_flash_alpha > 0.0F);
        ARPG_REQUIRE(arpg::test::near(
            plan.storm_swords.center.x, 4.0F));
        ARPG_REQUIRE(arpg::test::near(
            plan.storm_swords.center.y, -2.0F));
    }

    snapshot.active_skill.elapsed_ticks = static_cast<std::uint16_t>(
        kFinisherTick + 8U);
    plan = platform::make_active_skill_effect_plan(snapshot, nullptr);
    ARPG_REQUIRE(!plan.storm_swords.finisher_visible);
    ARPG_REQUIRE(arpg::test::near(plan.screen_flash_alpha, 0.0F));
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"active skill HUD fixed slots", &hud_projects_exactly_five_numbered_slots_and_catalog_names},
    {"active skill HUD cooldown clamp", &hud_cooldown_ratios_are_clamped_and_empty_slots_stay_zero},
    {"active skill HUD fixed layout", &hud_layout_is_bottom_centered_with_fixed_slot_geometry},
    {"active skill native effect plan", &native_effect_plan_uses_snapshot_timing_and_twelve_swords},
    {"storm finisher snapshot lifetime", &storm_finisher_persists_from_snapshot_without_hit_event},
};

}  // namespace

arpg::test::TestSuite active_skill_view_suite() noexcept {
    return arpg::test::make_suite("active_skill_view", kCases);
}
