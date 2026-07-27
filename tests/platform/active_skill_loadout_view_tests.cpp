#include "test_framework.hpp"

#include "inventory_view_math.hpp"
#include "skills/skill_loadout.hpp"

#include <array>
#include <cstddef>
#include <cstring>

namespace {

namespace platform = arpg::platform;
namespace skills = arpg::skills;

bool overlaps(Rectangle left, Rectangle right) noexcept {
    return left.x < right.x + right.width
        && right.x < left.x + left.width
        && left.y < right.y + right.height
        && right.y < left.y + left.height;
}

Vector2 center(Rectangle rectangle) noexcept {
    return {rectangle.x + rectangle.width * 0.5F,
        rectangle.y + rectangle.height * 0.5F};
}

arpg::test::Failure view_has_five_main_slots_five_read_only_supports_and_inventory()
    noexcept {
    skills::SkillLoadoutState state = skills::default_skill_loadout();
    state.slots[0U].active = skills::ActiveSkillId::none;
    platform::ActiveSkillLoadoutSelection selection{};
    selection.selected_slot = 1U;
    const platform::ActiveSkillLoadoutView view =
        platform::make_active_skill_loadout_view(state, selection, false);

    ARPG_REQUIRE(view.slots.size() == skills::kActiveSkillSlotCount);
    ARPG_REQUIRE(view.slots[1U].selected);
    for (const platform::ActiveSkillLoadoutSlotView& slot : view.slots) {
        ARPG_REQUIRE(slot.support_empty.size()
            == skills::kSupportSlotsPerActive);
        for (const bool empty : slot.support_empty) ARPG_REQUIRE(empty);
    }
    ARPG_REQUIRE(std::strcmp(
        view.slots[1U].name.data(), u8"极·鬼剑术（暴风式）") == 0);
    ARPG_REQUIRE(view.inventory_count == 1U);
    ARPG_REQUIRE(view.inventory[0U].id == skills::ActiveSkillId::draw_slash);
    ARPG_REQUIRE(std::strcmp(
        view.inventory[0U].name.data(), u8"拔刀斩") == 0);
    return {};
}

arpg::test::Failure loadout_hit_regions_do_not_overlap_at_supported_resolutions()
    noexcept {
    constexpr std::array<std::array<int, 2>, 3> kViewports{{
        {{1280, 720}}, {{1600, 900}}, {{1920, 1080}},
    }};
    for (const auto viewport : kViewports) {
        const platform::ActiveSkillLoadoutLayout layout =
            platform::active_skill_loadout_layout(viewport[0], viewport[1]);
        std::array<Rectangle, 13> hit_regions{};
        std::size_t count = 0U;
        for (const Rectangle rectangle : layout.main_slots) {
            hit_regions[count++] = rectangle;
        }
        for (const Rectangle rectangle : layout.inventory_slots) {
            hit_regions[count++] = rectangle;
        }
        hit_regions[count++] = layout.remove_button;
        hit_regions[count++] = layout.equipment_page_button;
        hit_regions[count++] = layout.skill_stones_page_button;
        for (std::size_t left = 0U; left < count; ++left) {
            for (std::size_t right = left + 1U; right < count; ++right) {
                ARPG_REQUIRE(!overlaps(hit_regions[left], hit_regions[right]));
            }
        }
        for (const Rectangle support : layout.support_slots) {
            for (std::size_t interactive = 0U;
                 interactive < count; ++interactive) {
                ARPG_REQUIRE(!overlaps(support, hit_regions[interactive]));
            }
        }
    }
    return {};
}

arpg::test::Failure slot_clicks_emit_only_select_remove_and_swap_commands()
    noexcept {
    const skills::SkillLoadoutState state = skills::default_skill_loadout();
    const platform::ActiveSkillLoadoutLayout layout =
        platform::active_skill_loadout_layout(1280, 720);
    platform::ActiveSkillLoadoutSelection selection{};

    auto command = platform::active_skill_loadout_command_after_click(
        state, layout, center(layout.main_slots[0U]), selection, false);
    ARPG_REQUIRE(command.has_value());
    ARPG_REQUIRE(command->kind
        == platform::ActiveSkillLoadoutActionKind::select);
    ARPG_REQUIRE(selection.selected_slot == 0U);

    command = platform::active_skill_loadout_command_after_click(
        state, layout, center(layout.remove_button), selection, false);
    ARPG_REQUIRE(command.has_value());
    ARPG_REQUIRE(command->kind
        == platform::ActiveSkillLoadoutActionKind::remove);
    ARPG_REQUIRE(command->slot == 0U);
    ARPG_REQUIRE(selection.selected_slot == 0U);

    command = platform::active_skill_loadout_command_after_click(
        state, layout, center(layout.main_slots[1U]), selection, false);
    ARPG_REQUIRE(command.has_value());
    ARPG_REQUIRE(command->kind
        == platform::ActiveSkillLoadoutActionKind::swap);
    ARPG_REQUIRE(command->slot == 0U);
    ARPG_REQUIRE(command->other_slot == 1U);
    ARPG_REQUIRE(selection.selected_slot == 0U);
    return {};
}

arpg::test::Failure inventory_selection_then_empty_slot_emits_equip_command()
    noexcept {
    skills::SkillLoadoutState state = skills::default_skill_loadout();
    state.slots[0U].active = skills::ActiveSkillId::none;
    const platform::ActiveSkillLoadoutLayout layout =
        platform::active_skill_loadout_layout(1280, 720);
    platform::ActiveSkillLoadoutSelection selection{};

    auto command = platform::active_skill_loadout_command_after_click(
        state, layout, center(layout.inventory_slots[0U]), selection, false);
    ARPG_REQUIRE(command.has_value());
    ARPG_REQUIRE(command->kind
        == platform::ActiveSkillLoadoutActionKind::select);
    ARPG_REQUIRE(selection.selected_inventory
        == skills::ActiveSkillId::draw_slash);

    command = platform::active_skill_loadout_command_after_click(
        state, layout, center(layout.main_slots[0U]), selection, false);
    ARPG_REQUIRE(command.has_value());
    ARPG_REQUIRE(command->kind
        == platform::ActiveSkillLoadoutActionKind::equip);
    ARPG_REQUIRE(command->skill == skills::ActiveSkillId::draw_slash);
    ARPG_REQUIRE(command->slot == 0U);
    ARPG_REQUIRE(selection.selected_inventory
        == skills::ActiveSkillId::draw_slash);

    const skills::SkillLoadoutState authoritative = state;
    platform::advance_active_skill_loadout_selection(selection, *command);
    ARPG_REQUIRE(selection.selected_slot == 0U);
    ARPG_REQUIRE(selection.selected_inventory == skills::ActiveSkillId::none);
    ARPG_REQUIRE(authoritative.slots[0U].active
        == skills::ActiveSkillId::none);
    ARPG_REQUIRE(authoritative.slots[1U].active
        == skills::ActiveSkillId::storm_swords);

    platform::advance_active_skill_loadout_selection(selection,
        {platform::ActiveSkillLoadoutActionKind::swap,
            0U, 1U, skills::ActiveSkillId::none});
    ARPG_REQUIRE(selection.selected_slot == 1U);
    ARPG_REQUIRE(authoritative.slots[0U].active
        == skills::ActiveSkillId::none);
    ARPG_REQUIRE(authoritative.slots[1U].active
        == skills::ActiveSkillId::storm_swords);
    return {};
}

arpg::test::Failure pending_save_and_support_slots_never_emit_edit_commands()
    noexcept {
    const skills::SkillLoadoutState state = skills::default_skill_loadout();
    const platform::ActiveSkillLoadoutLayout layout =
        platform::active_skill_loadout_layout(1280, 720);
    platform::ActiveSkillLoadoutSelection selection{};

    const auto support = platform::active_skill_loadout_command_after_click(
        state, layout, center(layout.support_slots[0U]), selection, false);
    ARPG_REQUIRE(!support.has_value());
    const auto pending = platform::active_skill_loadout_command_after_click(
        state, layout, center(layout.main_slots[0U]), selection, true);
    ARPG_REQUIRE(!pending.has_value());
    ARPG_REQUIRE(selection.selected_slot
        == platform::kNoActiveSkillLoadoutSelection);
    ARPG_REQUIRE(selection.selected_inventory == skills::ActiveSkillId::none);
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"active skill loadout view projection", &view_has_five_main_slots_five_read_only_supports_and_inventory},
    {"active skill loadout non-overlapping layout", &loadout_hit_regions_do_not_overlap_at_supported_resolutions},
    {"active skill loadout slot commands", &slot_clicks_emit_only_select_remove_and_swap_commands},
    {"active skill loadout equip command", &inventory_selection_then_empty_slot_emits_equip_command},
    {"active skill support read only and pending disabled", &pending_save_and_support_slots_never_emit_edit_commands},
};

}  // namespace

arpg::test::TestSuite active_skill_loadout_view_suite() noexcept {
    return arpg::test::make_suite("active_skill_loadout_view", kCases);
}
