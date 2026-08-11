#include "test_framework.hpp"
#include "allocation_probe.hpp"

#include "active_skill_renderer.hpp"
#include "active_skill_view.hpp"
#include "combat/active_skill_runtime.hpp"
#include "skills/active_skill_catalog.hpp"
#include "skills/skill_loadout.hpp"
#include "ui_text_bounds_audit.hpp"
#include "ui_text_contrast.hpp"
#include "ui_text_layout.hpp"
#include "ui_typography.hpp"
#include "ui_utf8.hpp"

#include <array>
#include <cstddef>
#include <cstring>
#include <limits>
#include <type_traits>
#include <utility>

namespace {

namespace combat = arpg::combat;
namespace platform = arpg::platform;
namespace skills = arpg::skills;

platform::UiTextExtent measure_skill_text(const char* text, float size,
    float spacing, void*) noexcept {
    std::size_t count{};
    for (const unsigned char* cursor =
             reinterpret_cast<const unsigned char*>(text);
         cursor != nullptr && *cursor != 0U; ++cursor) {
        if ((*cursor & 0xC0U) != 0x80U) ++count;
    }
    return {count == 0U ? 0.0F
                        : static_cast<float>(count) * size
                            + static_cast<float>(count - 1U) * spacing,
        size};
}

platform::UiTextExtent fail_skill_text_measure(
    const char*, float, float, void*) noexcept {
    return {};
}

enum class SkillMeasureFailure : std::uint8_t {
    none,
    key,
    name,
    cooldown,
};

struct SkillMeasureContext final {
    SkillMeasureFailure failure{SkillMeasureFailure::none};
    std::size_t key_calls{};
    std::size_t name_calls{};
    std::size_t cooldown_calls{};
};

platform::UiTextExtent measure_skill_text_by_stage(const char* text,
    float size, float spacing, void* context) noexcept {
    SkillMeasureContext& state = *static_cast<SkillMeasureContext*>(context);
    if (std::strcmp(text, u8"小键盘1") == 0) {
        ++state.key_calls;
        if (state.failure == SkillMeasureFailure::key) return {};
    } else if (std::strcmp(text, u8"拔刀斩") == 0) {
        ++state.name_calls;
        if (state.failure == SkillMeasureFailure::name) return {};
    } else if (std::strcmp(text, u8"2秒") == 0) {
        ++state.cooldown_calls;
        if (state.failure == SkillMeasureFailure::cooldown) return {};
    }
    return measure_skill_text(text, size, spacing, nullptr);
}

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

bool same_rectangle(Rectangle left, Rectangle right) noexcept {
    return arpg::test::near(left.x, right.x)
        && arpg::test::near(left.y, right.y)
        && arpg::test::near(left.width, right.width)
        && arpg::test::near(left.height, right.height);
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
        kExpectedLabels{{u8"小键盘1", u8"小键盘2", u8"小键盘3",
            u8"小键盘4", u8"小键盘5"}};

    static_assert(std::tuple_size<decltype(
        platform::ActiveSkillHudSlot{}.key_label)>::value >= 16U,
        "Chinese numpad labels require at least 16 bytes");

    for (std::size_t index = 0U; index < view.slots.size(); ++index) {
        ARPG_REQUIRE(SkillKeyLabelContract<
            platform::ActiveSkillHudSlot>::matches(
                view.slots[index], kExpectedLabels[index]));
        ARPG_REQUIRE(platform::ui_utf8_is_valid(
            view.slots[index].key_label.data()));
        ARPG_REQUIRE(std::strstr(view.slots[index].key_label.data(), "Num")
            == nullptr);
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
    ARPG_REQUIRE(std::strcmp(view.slots[0U].cooldown_label.data(), u8"2秒")
        == 0);
    ARPG_REQUIRE(std::strcmp(view.slots[1U].cooldown_label.data(), u8"30秒")
        == 0);
    ARPG_REQUIRE(platform::ui_utf8_is_valid(
        view.slots[0U].cooldown_label.data()));
    ARPG_REQUIRE(std::strstr(view.slots[0U].cooldown_label.data(), "s")
        == nullptr);
    for (std::size_t index = 2U; index < view.slots.size(); ++index) {
        ARPG_REQUIRE(arpg::test::near(
            view.slots[index].cooldown_ratio, 0.0F));
        ARPG_REQUIRE(view.slots[index].cooldown_label[0] == '\0');
    }

    const platform::ActiveSkillHudLayout layout =
        platform::active_skill_hud_layout(800, 450);
    const platform::ActiveSkillHudTextRegions& regions =
        layout.text_regions[0U];
    platform::ActiveSkillHudSlot ready_slot = view.slots[1U];
    ready_slot.cooldown_ratio = 0.0F;
    ready_slot.cooldown_label[0U] = '\0';
    const platform::ActiveSkillHudTextPlan empty =
        platform::make_active_skill_hud_text_plan(view.slots[2U], regions,
            layout.scale, true, &measure_skill_text, nullptr);
    const platform::ActiveSkillHudTextPlan ready =
        platform::make_active_skill_hud_text_plan(ready_slot, regions,
            layout.scale, true, &measure_skill_text, nullptr);
    const platform::ActiveSkillHudTextPlan cooling =
        platform::make_active_skill_hud_text_plan(view.slots[0U], regions,
            layout.scale, true, &measure_skill_text, nullptr);

    ARPG_REQUIRE(empty.key.visible);
    ARPG_REQUIRE(!empty.name.visible);
    ARPG_REQUIRE(!empty.cooldown.visible);
    ARPG_REQUIRE(ready.key.visible);
    ARPG_REQUIRE(ready.name.visible);
    ARPG_REQUIRE(!ready.cooldown.visible);
    ARPG_REQUIRE(cooling.key.visible);
    ARPG_REQUIRE(cooling.name.visible);
    ARPG_REQUIRE(cooling.cooldown.visible);
    ARPG_REQUIRE(cooling.key.role == platform::UiTextAuditRole::hud_skill_key);
    ARPG_REQUIRE(cooling.name.role == platform::UiTextAuditRole::hud_skill_name);
    ARPG_REQUIRE(cooling.cooldown.role
        == platform::UiTextAuditRole::hud_skill_cooldown);

    constexpr std::array<const char*, skills::kActiveSkillSlotCount>
        kExpectedKeyLabels{{u8"小键盘1", u8"小键盘2", u8"小键盘3",
            u8"小键盘4", u8"小键盘5"}};
    for (std::size_t index = 0U; index < view.slots.size(); ++index) {
        const platform::ActiveSkillHudTextPlan plan =
            platform::make_active_skill_hud_text_plan(view.slots[index],
                layout.text_regions[index], layout.scale, true,
                &measure_skill_text, nullptr);
        ARPG_REQUIRE(plan.key.visible);
        ARPG_REQUIRE(std::strcmp(plan.key.text.data(),
            view.slots[index].key_label.data()) == 0);
        ARPG_REQUIRE(std::strcmp(plan.key.text.data(),
            kExpectedKeyLabels[index]) == 0);
        ARPG_REQUIRE(!plan.key.truncated);
        ARPG_REQUIRE(same_rectangle(plan.key.backing,
            layout.text_regions[index].key));
    }
    const platform::ActiveSkillHudTextPlan cooling_thirty =
        platform::make_active_skill_hud_text_plan(view.slots[1U],
            layout.text_regions[1U], layout.scale, true, &measure_skill_text,
            nullptr);
    for (const platform::ActiveSkillTextPlatePlan* plate :
         std::array<const platform::ActiveSkillTextPlatePlan*, 2U>{
             &cooling.name, &cooling_thirty.name}) {
        ARPG_REQUIRE(plate->visible);
        ARPG_REQUIRE(!plate->truncated);
    }
    ARPG_REQUIRE(std::strcmp(cooling.name.text.data(),
        view.slots[0U].name.data()) == 0);
    ARPG_REQUIRE(std::strcmp(cooling.name.text.data(), u8"拔刀斩") == 0);
    ARPG_REQUIRE(same_rectangle(cooling.name.backing, regions.name));
    ARPG_REQUIRE(std::strcmp(cooling_thirty.name.text.data(),
        view.slots[1U].name.data()) == 0);
    ARPG_REQUIRE(std::strcmp(cooling_thirty.name.text.data(), u8"暴风式") == 0);
    ARPG_REQUIRE(same_rectangle(cooling_thirty.name.backing,
        layout.text_regions[1U].name));
    ARPG_REQUIRE(cooling_thirty.cooldown.visible);
    ARPG_REQUIRE(std::strcmp(cooling_thirty.cooldown.text.data(),
        view.slots[1U].cooldown_label.data()) == 0);
    ARPG_REQUIRE(std::strcmp(cooling_thirty.cooldown.text.data(),
        u8"30秒") == 0);
    ARPG_REQUIRE(!cooling_thirty.cooldown.truncated);
    ARPG_REQUIRE(same_rectangle(cooling_thirty.cooldown.backing,
        layout.text_regions[1U].cooldown));

    for (const platform::ActiveSkillTextPlatePlan* plate :
         std::array<const platform::ActiveSkillTextPlatePlan*, 3U>{
             &cooling.key, &cooling.name, &cooling.cooldown}) {
        ARPG_REQUIRE(platform::ui_utf8_is_valid(plate->text.data()));
        ARPG_REQUIRE(platform::ui_text_bounds_inside(
            {plate->position.x, plate->position.y, plate->extent.x,
                plate->extent.y}, plate->backing));
        ARPG_REQUIRE(!plate->truncated);
    }
    ARPG_REQUIRE(platform::active_skill_text_plate_backing().a == 255U);
    ARPG_REQUIRE(platform::ui_luma_contrast_ratio(
        platform::ui_text_contrast_style().primary,
        platform::active_skill_text_plate_backing()) >= 4.5F);

    const auto plan_is_empty = [](const platform::ActiveSkillHudTextPlan& plan)
        noexcept {
        return !plan.key.visible && !plan.name.visible && !plan.cooldown.visible;
    };
    ARPG_REQUIRE(plan_is_empty(platform::make_active_skill_hud_text_plan(
        view.slots[0U], regions, layout.scale, false, &measure_skill_text,
        nullptr)));
    platform::ActiveSkillHudSlot invalid_utf8 = view.slots[0U];
    invalid_utf8.name[0U] = static_cast<char>(0xFFU);
    ARPG_REQUIRE(plan_is_empty(platform::make_active_skill_hud_text_plan(
        invalid_utf8, regions, layout.scale, true, &measure_skill_text,
        nullptr)));
    platform::ActiveSkillHudTextRegions invalid_regions = regions;
    invalid_regions.name.width = 0.0F;
    ARPG_REQUIRE(plan_is_empty(platform::make_active_skill_hud_text_plan(
        view.slots[0U], invalid_regions, layout.scale, true,
        &measure_skill_text, nullptr)));
    ARPG_REQUIRE(plan_is_empty(platform::make_active_skill_hud_text_plan(
        view.slots[0U], regions, layout.scale, true,
        &fail_skill_text_measure, nullptr)));

    for (const float ratio : std::array<float, 3U>{
             std::numeric_limits<float>::quiet_NaN(),
             std::numeric_limits<float>::infinity(),
             -std::numeric_limits<float>::infinity()}) {
        platform::ActiveSkillHudSlot non_finite_ratio = view.slots[0U];
        non_finite_ratio.cooldown_ratio = ratio;
        const platform::ActiveSkillHudTextPlan non_finite_plan =
            platform::make_active_skill_hud_text_plan(non_finite_ratio,
                regions, layout.scale, true, &measure_skill_text, nullptr);
        ARPG_REQUIRE(non_finite_plan.key.visible);
        ARPG_REQUIRE(non_finite_plan.name.visible);
        ARPG_REQUIRE(!non_finite_plan.cooldown.visible);
    }

    SkillMeasureContext name_failure{SkillMeasureFailure::name};
    const platform::ActiveSkillHudTextPlan failed_name =
        platform::make_active_skill_hud_text_plan(view.slots[0U], regions,
            layout.scale, true, &measure_skill_text_by_stage, &name_failure);
    ARPG_REQUIRE(name_failure.key_calls > 0U);
    ARPG_REQUIRE(name_failure.name_calls > 0U);
    ARPG_REQUIRE(name_failure.cooldown_calls > 0U);
    ARPG_REQUIRE(failed_name.key.visible);
    ARPG_REQUIRE(!failed_name.name.visible);
    ARPG_REQUIRE(failed_name.cooldown.visible);

    SkillMeasureContext cooldown_failure{SkillMeasureFailure::cooldown};
    const platform::ActiveSkillHudTextPlan failed_cooldown =
        platform::make_active_skill_hud_text_plan(view.slots[0U], regions,
            layout.scale, true, &measure_skill_text_by_stage,
            &cooldown_failure);
    ARPG_REQUIRE(cooldown_failure.key_calls > 0U);
    ARPG_REQUIRE(cooldown_failure.name_calls > 0U);
    ARPG_REQUIRE(cooldown_failure.cooldown_calls > 0U);
    ARPG_REQUIRE(failed_cooldown.key.visible);
    ARPG_REQUIRE(failed_cooldown.name.visible);
    ARPG_REQUIRE(!failed_cooldown.cooldown.visible);

    SkillMeasureContext key_failure{SkillMeasureFailure::key};
    const platform::ActiveSkillHudTextPlan failed_key =
        platform::make_active_skill_hud_text_plan(view.slots[0U], regions,
            layout.scale, true, &measure_skill_text_by_stage, &key_failure);
    ARPG_REQUIRE(key_failure.key_calls > 0U);
    ARPG_REQUIRE(key_failure.name_calls == 0U);
    ARPG_REQUIRE(key_failure.cooldown_calls == 0U);
    ARPG_REQUIRE(plan_is_empty(failed_key));

    static_cast<void>(platform::make_active_skill_hud_text_plan(view.slots[2U],
        regions, layout.scale, true, &measure_skill_text, nullptr));
    static_cast<void>(platform::make_active_skill_hud_text_plan(ready_slot,
        regions, layout.scale, true, &measure_skill_text, nullptr));
    static_cast<void>(platform::make_active_skill_hud_text_plan(view.slots[0U],
        regions, layout.scale, true, &measure_skill_text, nullptr));
    const std::uint64_t allocations_before = arpg::test::allocation_count();
    for (std::size_t iteration = 0U; iteration < 10'000U; ++iteration) {
        const platform::ActiveSkillHudLayout repeated_layout =
            platform::active_skill_hud_layout(800, 450);
        const platform::ActiveSkillHudTextPlan repeated_empty =
            platform::make_active_skill_hud_text_plan(
            view.slots[2U], repeated_layout.text_regions[0U],
            repeated_layout.scale, true, &measure_skill_text, nullptr);
        const platform::ActiveSkillHudTextPlan repeated_ready =
            platform::make_active_skill_hud_text_plan(
            ready_slot, repeated_layout.text_regions[0U],
            repeated_layout.scale, true, &measure_skill_text, nullptr);
        const platform::ActiveSkillHudTextPlan repeated_cooling =
            platform::make_active_skill_hud_text_plan(
            view.slots[0U], repeated_layout.text_regions[0U],
            repeated_layout.scale, true, &measure_skill_text, nullptr);
        ARPG_REQUIRE(repeated_empty.key.visible
            && !repeated_empty.name.visible && !repeated_empty.cooldown.visible);
        ARPG_REQUIRE(repeated_ready.key.visible
            && repeated_ready.name.visible && !repeated_ready.cooldown.visible);
        ARPG_REQUIRE(repeated_cooling.key.visible
            && repeated_cooling.name.visible && repeated_cooling.cooldown.visible);
    }
    ARPG_REQUIRE(arpg::test::allocation_count() == allocations_before);
    return {};
}

arpg::test::Failure hud_layout_is_bottom_centered_with_scaled_slot_geometry()
    noexcept {
    constexpr std::array<std::array<int, 2>, 3> kViewports{{
        {{800, 450}}, {{1280, 720}}, {{1920, 1080}},
    }};
    for (const auto viewport : kViewports) {
        const float scale = viewport[0] == 1920 ? 1.5F : 1.0F;
        const float expected_width = (5.0F * 76.0F + 4.0F * 8.0F) * scale;
        const platform::ActiveSkillHudLayout layout =
            platform::active_skill_hud_layout(viewport[0], viewport[1]);
        ARPG_REQUIRE(layout.valid);
        ARPG_REQUIRE(arpg::test::near(layout.scale, scale));
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
            const platform::ActiveSkillHudTextRegions& regions =
                layout.text_regions[index];
            ARPG_REQUIRE(arpg::test::near(regions.key.x,
                layout.slots[index].x + 4.0F * scale));
            ARPG_REQUIRE(arpg::test::near(regions.key.y,
                layout.slots[index].y - 20.0F * scale));
            ARPG_REQUIRE(arpg::test::near(regions.key.width,
                layout.slots[index].width - 8.0F * scale));
            ARPG_REQUIRE(arpg::test::near(regions.key.height, 18.0F * scale));
            ARPG_REQUIRE(arpg::test::near(regions.cooldown.x,
                layout.slots[index].x + 16.0F * scale));
            ARPG_REQUIRE(arpg::test::near(regions.cooldown.y,
                layout.slots[index].y + 28.0F * scale));
            ARPG_REQUIRE(arpg::test::near(regions.cooldown.width,
                layout.slots[index].width - 32.0F * scale));
            ARPG_REQUIRE(arpg::test::near(
                regions.cooldown.height, 20.0F * scale));
            ARPG_REQUIRE(arpg::test::near(regions.name.x,
                layout.slots[index].x + 7.0F * scale));
            ARPG_REQUIRE(arpg::test::near(regions.name.y,
                layout.slots[index].y + 77.0F * scale));
            ARPG_REQUIRE(arpg::test::near(regions.name.width,
                layout.slots[index].width - 14.0F * scale));
            ARPG_REQUIRE(arpg::test::near(regions.name.height, 18.0F * scale));
            ARPG_REQUIRE(platform::ui_text_bounds_inside(regions.visual_bounds,
                {0.0F, 0.0F, static_cast<float>(viewport[0]),
                    static_cast<float>(viewport[1])}));
            ARPG_REQUIRE(platform::ui_text_bounds_separated(
                regions.key, regions.cooldown));
            ARPG_REQUIRE(platform::ui_text_bounds_separated(
                regions.key, regions.name));
            ARPG_REQUIRE(platform::ui_text_bounds_separated(
                regions.cooldown, regions.name));
            if (index != 0U) {
                ARPG_REQUIRE(arpg::test::near(layout.slots[index].x
                    - (layout.slots[index - 1U].x
                        + layout.slots[index - 1U].width), 8.0F * scale));
                ARPG_REQUIRE(!overlaps(
                    layout.slots[index - 1U], layout.slots[index]));
            }
        }
        if (viewport[0] == 800) {
            ARPG_REQUIRE(arpg::test::near(layout.bounds.x, 194.0F));
            ARPG_REQUIRE(arpg::test::near(layout.bounds.y, 354.0F));
            ARPG_REQUIRE(arpg::test::near(layout.bounds.width, 412.0F));
            ARPG_REQUIRE(arpg::test::near(layout.bounds.height, 76.0F));
            const platform::ActiveSkillHudTextRegions& regions =
                layout.text_regions[0U];
            ARPG_REQUIRE(arpg::test::near(regions.key.y, 334.0F));
            ARPG_REQUIRE(arpg::test::near(
                regions.key.y + regions.key.height, 352.0F));
            ARPG_REQUIRE(arpg::test::near(regions.cooldown.y, 382.0F));
            ARPG_REQUIRE(arpg::test::near(
                regions.cooldown.y + regions.cooldown.height, 402.0F));
            ARPG_REQUIRE(arpg::test::near(regions.name.y, 431.0F));
            ARPG_REQUIRE(arpg::test::near(
                regions.name.y + regions.name.height, 449.0F));
            const Rectangle icon{layout.slots[0U].x + 8.0F,
                layout.slots[0U].y + 8.0F, 60.0F, 60.0F};
            ARPG_REQUIRE((regions.cooldown.width * regions.cooldown.height)
                / (icon.width * icon.height) <= 0.25F);
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
