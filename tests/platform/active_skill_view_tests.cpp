#include "test_framework.hpp"
#include "allocation_probe.hpp"

#include "active_skill_renderer.hpp"
#include "active_skill_view.hpp"
#include "combat/active_skill_runtime.hpp"
#include "skills/active_skill_catalog.hpp"
#include "skills/skill_loadout.hpp"
#include "ui_typography.hpp"

#include <array>
#include <cstddef>
#include <cstring>
#include <type_traits>
#include <utility>

namespace {

namespace combat = arpg::combat;
namespace platform = arpg::platform;
namespace skills = arpg::skills;

template <typename Slot, typename = void>
struct SkillKeyLabelContract final {
    [[nodiscard]] static bool matches(
        const Slot&, const char*) noexcept {
        return false;
    }
};

template <typename Slot>
struct SkillKeyLabelContract<Slot,
    std::void_t<decltype(std::declval<const Slot&>().key_label)>> final {
    [[nodiscard]] static bool matches(
        const Slot& slot, const char* expected) noexcept {
        return std::strcmp(slot.key_label.data(), expected) == 0;
    }
};

bool overlaps(Rectangle left, Rectangle right) noexcept {
    return left.x < right.x + right.width
        && right.x < left.x + left.width
        && left.y < right.y + right.height
        && right.y < left.y + left.height;
}

arpg::test::Failure active_skill_visual_frames_reach_complete_material_timelines()
    noexcept {
    std::array<bool, 36U> draw_slash_frames{};
    for (std::uint16_t tick = 0U; tick < 90U; ++tick) {
        const std::size_t frame = platform::active_skill_visual_frame_index(
            skills::ActiveSkillId::draw_slash, tick);
        ARPG_REQUIRE(frame < draw_slash_frames.size());
        draw_slash_frames[frame] = true;
    }
    for (const bool reached : draw_slash_frames) ARPG_REQUIRE(reached);

    std::array<bool, 24U> storm_frames{};
    for (std::uint16_t tick = 0U; tick < 360U; ++tick) {
        const std::size_t frame = platform::active_skill_visual_frame_index(
            skills::ActiveSkillId::storm_swords, tick);
        ARPG_REQUIRE(frame < storm_frames.size());
        storm_frames[frame] = true;
    }
    for (const bool reached : storm_frames) ARPG_REQUIRE(reached);
    ARPG_REQUIRE(storm_frames[16U]);
    for (std::size_t frame = 20U; frame < 24U; ++frame) {
        ARPG_REQUIRE(storm_frames[frame]);
    }
    ARPG_REQUIRE(platform::active_skill_visual_frame_index(
        skills::ActiveSkillId::storm_swords, 71U) == 7U);
    ARPG_REQUIRE(platform::active_skill_visual_frame_index(
        skills::ActiveSkillId::storm_swords, 72U) == 8U);
    ARPG_REQUIRE(platform::active_skill_visual_frame_index(
        skills::ActiveSkillId::storm_swords, 323U) == 15U);
    ARPG_REQUIRE(platform::active_skill_visual_frame_index(
        skills::ActiveSkillId::storm_swords, 324U) == 16U);
    ARPG_REQUIRE(platform::active_skill_visual_frame_index(
        skills::ActiveSkillId::storm_swords, 341U) == 19U);
    ARPG_REQUIRE(platform::active_skill_visual_frame_index(
        skills::ActiveSkillId::storm_swords, 342U) == 20U);
    ARPG_REQUIRE(platform::active_skill_visual_frame_index(
        skills::ActiveSkillId::storm_swords, 359U) == 23U);
    return {};
}

arpg::test::Failure active_skill_plan_separates_player_and_effect_positions()
    noexcept {
    combat::CombatSnapshot snapshot{};
    snapshot.player.position = {7.0F, -3.0F, 2.0F};
    snapshot.active_skill.id = skills::ActiveSkillId::draw_slash;
    snapshot.active_skill.locked_center = {-11.0F, 5.0F, 0.0F};
    const platform::ActiveSkillEffectPlan first =
        platform::make_active_skill_effect_plan(snapshot, nullptr, true);
    ARPG_REQUIRE(arpg::test::near(first.player_position.x, 7.0F));
    ARPG_REQUIRE(arpg::test::near(first.player_position.y, -3.0F));
    ARPG_REQUIRE(arpg::test::near(first.player_position.z, 2.0F));
    ARPG_REQUIRE(arpg::test::near(first.effect_center.x, -11.0F));
    ARPG_REQUIRE(arpg::test::near(first.effect_center.y, 5.0F));

    snapshot.active_skill.locked_center = {23.0F, 17.0F, 0.0F};
    const platform::ActiveSkillEffectPlan moved_effect =
        platform::make_active_skill_effect_plan(snapshot, nullptr, true);
    ARPG_REQUIRE(arpg::test::near(moved_effect.player_position.x, 7.0F));
    ARPG_REQUIRE(arpg::test::near(moved_effect.player_position.y, -3.0F));
    ARPG_REQUIRE(arpg::test::near(moved_effect.effect_center.x, 23.0F));
    ARPG_REQUIRE(arpg::test::near(moved_effect.effect_center.y, 17.0F));
    return {};
}

arpg::test::Failure material_and_fallback_skill_plans_are_exclusive_and_allocate_nothing()
    noexcept {
    const platform::ActiveSkillDrawRuntimeStatus untouched_runtime{};
    ARPG_REQUIRE(untouched_runtime.mode
        == platform::ActiveSkillVisualMode::none);
    ARPG_REQUIRE(untouched_runtime.atlas == platform::MaterialAtlasId::count);
    ARPG_REQUIRE(untouched_runtime.atlas_frame == 0U);
    ARPG_REQUIRE(!untouched_runtime.material_frame_drawn);
    ARPG_REQUIRE(!untouched_runtime.base_player_drawn);
    ARPG_REQUIRE(untouched_runtime.procedural_main_visual_count == 0U);

    platform::ActiveSkillEffectPlan empty_procedural_plan{};
    empty_procedural_plan.mode =
        platform::ActiveSkillVisualMode::procedural_fallback;
    empty_procedural_plan.storm_swords.visible = true;
    const platform::ActiveSkillDrawRuntimeStatus empty_procedural_runtime =
        platform::ActiveSkillRenderer{}.draw_world(
            empty_procedural_plan, platform::MaterialPack{},
            1280.0F, 720.0F);
    ARPG_REQUIRE(
        empty_procedural_runtime.procedural_main_visual_count == 0U);

    combat::CombatSnapshot snapshot{};
    snapshot.player.position = {3.0F, 4.0F, 0.0F};
    snapshot.active_skill.id = skills::ActiveSkillId::storm_swords;
    snapshot.active_skill.phase = combat::ActiveSkillPhase::strikes;
    snapshot.active_skill.elapsed_ticks = 200U;
    snapshot.active_skill.locked_center = {9.0F, -8.0F, 0.0F};
    snapshot.active_skill.spawned_sword_count = 24U;
    snapshot.active_skill.transients_active = true;

    const platform::ActiveSkillEffectPlan healthy =
        platform::make_active_skill_effect_plan(snapshot, nullptr, true);
    ARPG_REQUIRE(healthy.mode == platform::ActiveSkillVisualMode::material);
    ARPG_REQUIRE(healthy.suppress_base_player);
    ARPG_REQUIRE(healthy.procedural_main_visual_count == 0U);
    ARPG_REQUIRE(healthy.atlas == platform::MaterialAtlasId::skill_storm_swords);
    ARPG_REQUIRE(!healthy.storm_swords.visible);
    ARPG_REQUIRE(healthy.storm_swords.sword_count == 0U);
    ARPG_REQUIRE(!healthy.storm_swords.finisher_visible);

    const platform::ActiveSkillEffectPlan fallback =
        platform::make_active_skill_effect_plan(snapshot, nullptr, false);
    ARPG_REQUIRE(fallback.mode
        == platform::ActiveSkillVisualMode::procedural_fallback);
    ARPG_REQUIRE(!fallback.suppress_base_player);
    ARPG_REQUIRE(fallback.procedural_main_visual_count > 0U);

    const std::uint64_t before = arpg::test::allocation_count();
    for (std::size_t iteration = 0U; iteration < 10'000U; ++iteration) {
        const auto plan = platform::make_active_skill_effect_plan(
            snapshot, nullptr, (iteration & 1U) == 0U);
        ARPG_REQUIRE(plan.atlas_frame < 24U);
    }
    ARPG_REQUIRE(arpg::test::allocation_count() == before);
    return {};
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
        view.slots[1U].name.data(), u8"暴风式") == 0);
    ARPG_REQUIRE(view.slots[2U].name[0U] == '\0');
    return {};
}

arpg::test::Failure hud_labels_skill_slots_as_numpad_keys() noexcept {
    const platform::ActiveSkillHudModel view =
        platform::make_active_skill_hud_model(
            skills::default_skill_loadout(), {});
    constexpr std::array<const char*, skills::kActiveSkillSlotCount>
        kExpectedLabels{{"Num1", "Num2", "Num3", "Num4", "Num5"}};

    for (std::size_t index = 0U; index < view.slots.size(); ++index) {
        ARPG_REQUIRE(SkillKeyLabelContract<
            platform::ActiveSkillHudSlot>::matches(
                view.slots[index], kExpectedLabels[index]));
    }
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

arpg::test::Failure hud_layout_is_bottom_centered_with_scaled_slot_geometry()
    noexcept {
    constexpr std::array<std::array<int, 2>, 3> kViewports{{
        {{1280, 720}}, {{1600, 900}}, {{1920, 1080}},
    }};
    for (const auto viewport : kViewports) {
        const float scale = platform::ui_viewport_scale(
            viewport[0], viewport[1]);
        const float expected_width = (5.0F * 76.0F + 4.0F * 8.0F) * scale;
        const platform::ActiveSkillHudLayout layout =
            platform::active_skill_hud_layout(viewport[0], viewport[1]);
        ARPG_REQUIRE(arpg::test::near(layout.bounds.width, expected_width));
        ARPG_REQUIRE(arpg::test::near(layout.bounds.x
            + layout.bounds.width * 0.5F,
            static_cast<float>(viewport[0]) * 0.5F));
        ARPG_REQUIRE(layout.bounds.y + layout.bounds.height
            <= static_cast<float>(viewport[1]));
        for (std::size_t index = 0U; index < layout.slots.size(); ++index) {
            ARPG_REQUIRE(arpg::test::near(
                layout.slots[index].width, 76.0F * scale));
            ARPG_REQUIRE(arpg::test::near(
                layout.slots[index].height, 76.0F * scale));
            if (index != 0U) {
                ARPG_REQUIRE(arpg::test::near(layout.slots[index].x
                    - (layout.slots[index - 1U].x
                        + layout.slots[index - 1U].width), 8.0F * scale));
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
    snapshot.active_skill.elapsed_ticks = 46U;
    snapshot.active_skill.frame_index = 18U;
    snapshot.active_skill.locked_center = {2.0F, 3.0F, 0.0F};
    platform::ActiveSkillEffectPlan plan =
        platform::make_active_skill_effect_plan(snapshot, nullptr, false);
    ARPG_REQUIRE(plan.draw_slash.visible);
    ARPG_REQUIRE(plan.atlas == platform::MaterialAtlasId::skill_draw_slash);
    ARPG_REQUIRE(plan.atlas_frame == 18U);
    ARPG_REQUIRE(plan.draw_slash.facing == combat::Facing::left);
    ARPG_REQUIRE(arpg::test::near(plan.draw_slash.center.x, 2.0F));
    snapshot.active_skill.elapsed_ticks = static_cast<std::uint16_t>(
        46U + 8U);
    plan = platform::make_active_skill_effect_plan(snapshot, nullptr, false);
    ARPG_REQUIRE(plan.draw_slash.visible);

    snapshot.active_skill.id = skills::ActiveSkillId::storm_swords;
    snapshot.active_skill.phase = combat::ActiveSkillPhase::strikes;
    snapshot.active_skill.strike_index = 4U;
    snapshot.active_skill.spawned_sword_count = 12U;
    snapshot.active_skill.transients_active = true;
    plan = platform::make_active_skill_effect_plan(snapshot, nullptr, false);
    ARPG_REQUIRE(plan.storm_swords.visible);
    ARPG_REQUIRE(plan.atlas == platform::MaterialAtlasId::skill_storm_swords);
    ARPG_REQUIRE(plan.atlas_frame < 24U);
    ARPG_REQUIRE(plan.storm_swords.sword_count == 12U);
    for (std::size_t index = 0U; index < plan.storm_swords.sword_count; ++index) {
        ARPG_REQUIRE(plan.storm_swords.swords[index].highlighted
            == (index == 3U));
    }

    combat::CombatEvent finisher{};
    finisher.tick = 100U;
    finisher.skill = skills::ActiveSkillId::storm_swords;
    finisher.finisher = true;
    snapshot.tick = 103U;
    snapshot.active_skill.phase = combat::ActiveSkillPhase::recovery;
    plan = platform::make_active_skill_effect_plan(snapshot, &finisher, false);
    ARPG_REQUIRE(plan.storm_swords.finisher_visible);
    ARPG_REQUIRE(plan.screen_flash_alpha > 0.0F);
    snapshot.tick = 108U;
    plan = platform::make_active_skill_effect_plan(snapshot, &finisher, false);
    ARPG_REQUIRE(!plan.storm_swords.finisher_visible);
    ARPG_REQUIRE(arpg::test::near(plan.screen_flash_alpha, 0.0F));
    return {};
}

arpg::test::Failure storm_finisher_persists_from_snapshot_without_hit_event()
    noexcept {
    constexpr std::uint16_t kFinisherTick = 324U;
    combat::CombatSnapshot snapshot{};
    snapshot.active_skill.id = skills::ActiveSkillId::storm_swords;
    snapshot.active_skill.locked_center = {4.0F, -2.0F, 0.0F};
    snapshot.active_skill.phase = combat::ActiveSkillPhase::finisher;
    snapshot.active_skill.elapsed_ticks = kFinisherTick;
    snapshot.active_skill.frame_index = 129U;
    platform::ActiveSkillEffectPlan plan =
        platform::make_active_skill_effect_plan(snapshot, nullptr, false);
    ARPG_REQUIRE(plan.storm_swords.finisher_visible);
    ARPG_REQUIRE(plan.atlas_frame == 16U);
    ARPG_REQUIRE(plan.screen_flash_alpha > 0.0F);

    snapshot.active_skill.phase = combat::ActiveSkillPhase::recovery;
    for (std::uint16_t age = 1U; age <= 7U; ++age) {
        snapshot.active_skill.elapsed_ticks = static_cast<std::uint16_t>(
            kFinisherTick + age);
        plan = platform::make_active_skill_effect_plan(snapshot, nullptr, false);
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
    plan = platform::make_active_skill_effect_plan(snapshot, nullptr, false);
    ARPG_REQUIRE(!plan.storm_swords.finisher_visible);
    ARPG_REQUIRE(arpg::test::near(plan.screen_flash_alpha, 0.0F));
    return {};
}

arpg::test::Failure hud_cooldown_overlay_keeps_continuous_material_feedback()
    noexcept {
    const Rectangle bounds{100.0F, 200.0F, 58.0F, 58.0F};
    const platform::ActiveSkillCooldownOverlayPlan hidden =
        platform::make_active_skill_cooldown_overlay(bounds, 0.0F, false);
    ARPG_REQUIRE(!hidden.visible);
    const platform::ActiveSkillCooldownOverlayPlan half =
        platform::make_active_skill_cooldown_overlay(bounds, 0.5F, false);
    ARPG_REQUIRE(half.visible);
    ARPG_REQUIRE(arpg::test::near(half.bounds.x, bounds.x));
    ARPG_REQUIRE(arpg::test::near(half.bounds.y, 229.0F));
    ARPG_REQUIRE(arpg::test::near(half.bounds.width, bounds.width));
    ARPG_REQUIRE(arpg::test::near(half.bounds.height, 29.0F));
    const platform::ActiveSkillCooldownOverlayPlan full =
        platform::make_active_skill_cooldown_overlay(bounds, 4.0F, false);
    ARPG_REQUIRE(full.visible);
    ARPG_REQUIRE(arpg::test::near(full.bounds.height, bounds.height));
    ARPG_REQUIRE(!platform::make_active_skill_cooldown_overlay(
        bounds, 0.75F, true).visible);
    return {};
}

arpg::test::Failure storm_sword_plan_uses_spawned_sword_lifecycle_and_two_bands()
    noexcept {
    combat::CombatSnapshot snapshot{};
    snapshot.active_skill.id = skills::ActiveSkillId::storm_swords;
    snapshot.active_skill.phase = combat::ActiveSkillPhase::strikes;
    snapshot.active_skill.elapsed_ticks = 108U;
    snapshot.active_skill.spawned_sword_count = 15U;
    snapshot.active_skill.transients_active = true;
    const platform::ActiveSkillEffectPlan plan =
        platform::make_active_skill_effect_plan(snapshot, nullptr, false);

    ARPG_REQUIRE(plan.storm_swords.sword_count == 15U);
    ARPG_REQUIRE(plan.storm_swords.swords.size() == 24U);
    std::size_t ground_count = 0U;
    std::size_t aerial_count = 0U;
    for (std::size_t index = 0U; index < plan.storm_swords.sword_count; ++index) {
        const platform::StormSwordVisual& sword = plan.storm_swords.swords[index];
        ARPG_REQUIRE(sword.visible);
        if (sword.band == platform::StormSwordBand::ground) {
            ++ground_count;
        } else {
            ++aerial_count;
        }
    }
    ARPG_REQUIRE(ground_count == 12U);
    ARPG_REQUIRE(aerial_count == 3U);

    snapshot.active_skill.transients_active = false;
    snapshot.active_skill.id = skills::ActiveSkillId::none;
    const platform::ActiveSkillEffectPlan cleared =
        platform::make_active_skill_effect_plan(snapshot, nullptr, false);
    ARPG_REQUIRE(cleared.storm_swords.sword_count == 0U);
    ARPG_REQUIRE(!cleared.storm_swords.visible);
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"active skill visual frames reach complete material timelines",
        &active_skill_visual_frames_reach_complete_material_timelines},
    {"active skill plan separates player and effect positions",
        &active_skill_plan_separates_player_and_effect_positions},
    {"active skill material and fallback plans are exclusive and allocation free",
        &material_and_fallback_skill_plans_are_exclusive_and_allocate_nothing},
    {"active skill HUD fixed slots", &hud_projects_exactly_five_numbered_slots_and_catalog_names},
    {"active skill HUD labels numpad keys",
        &hud_labels_skill_slots_as_numpad_keys},
    {"active skill HUD cooldown clamp", &hud_cooldown_ratios_are_clamped_and_empty_slots_stay_zero},
    {"active skill HUD continuous cooldown overlay",
        &hud_cooldown_overlay_keeps_continuous_material_feedback},
    {"active skill HUD responsive layout",
        &hud_layout_is_bottom_centered_with_scaled_slot_geometry},
    {"active skill native effect plan", &native_effect_plan_uses_snapshot_timing_and_twelve_swords},
    {"storm finisher snapshot lifetime", &storm_finisher_persists_from_snapshot_without_hit_event},
    {"storm sword lifecycle uses ground and aerial bands",
     &storm_sword_plan_uses_spawned_sword_lifecycle_and_two_bands},
};

}  // namespace

arpg::test::TestSuite active_skill_view_suite() noexcept {
    return arpg::test::make_suite("active_skill_view", kCases);
}
