#include "test_framework.hpp"

#include "inventory_view_math.hpp"
#include "skills/skill_loadout.hpp"
#include "ui_text_catalog.hpp"

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

bool inside(Rectangle inner, Rectangle outer) noexcept {
    return inner.x >= outer.x && inner.y >= outer.y
        && inner.x + inner.width <= outer.x + outer.width
        && inner.y + inner.height <= outer.y + outer.height;
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
    ARPG_REQUIRE(view.slots[0U].empty);
    ARPG_REQUIRE(view.slots[0U].name[0U] == '\0');
    ARPG_REQUIRE(std::strcmp(
        view.slots[1U].name.data(), u8"极·鬼剑术（暴风式）") == 0);
    ARPG_REQUIRE(std::strcmp(view.slots[1U].name.data(),
        platform::ui_skill_name(skills::ActiveSkillId::storm_swords)) == 0);
    ARPG_REQUIRE(view.inventory_count == 1U);
    ARPG_REQUIRE(view.inventory[0U].id == skills::ActiveSkillId::draw_slash);
    ARPG_REQUIRE(std::strcmp(
        view.inventory[0U].name.data(), u8"拔刀斩") == 0);
    ARPG_REQUIRE(std::strcmp(view.inventory[0U].name.data(),
        platform::ui_skill_name(skills::ActiveSkillId::draw_slash)) == 0);
    return {};
}

arpg::test::Failure loadout_hit_regions_do_not_overlap_at_supported_resolutions()
    noexcept {
    constexpr std::array<std::array<int, 2>, 3> kViewports{{
        {{800, 450}}, {{1280, 720}}, {{1920, 1080}},
    }};
    for (const auto viewport : kViewports) {
        const platform::InventoryShellLayout shell =
            platform::inventory_shell_layout(viewport[0], viewport[1]);
        const platform::ActiveSkillLoadoutLayout layout =
            platform::active_skill_loadout_layout(shell);
        const Rectangle viewport_bounds{0.0F, 0.0F,
            static_cast<float>(viewport[0]), static_cast<float>(viewport[1])};
        ARPG_REQUIRE(inside(layout.panel, viewport_bounds));
        ARPG_REQUIRE(layout.panel.x == shell.content.x);
        ARPG_REQUIRE(layout.remove_button.x == shell.action_bounds.x);
        ARPG_REQUIRE(layout.remove_button.y == shell.action_bounds.y);
        ARPG_REQUIRE(layout.remove_button.width == shell.action_bounds.width);
        ARPG_REQUIRE(layout.remove_button.height == shell.action_bounds.height);
        ARPG_REQUIRE(inside(layout.empty_bounds, shell.right_panel));
        ARPG_REQUIRE(std::fabs(layout.empty_bounds.x
            + layout.empty_bounds.width * 0.5F
            - (shell.right_panel.x + shell.right_panel.width * 0.5F))
            < 0.001F);
        ARPG_REQUIRE(std::fabs(layout.empty_bounds.y
            + layout.empty_bounds.height * 0.5F
            - (shell.right_panel.y + shell.right_panel.height * 0.5F))
            < 0.001F);
        ARPG_REQUIRE(layout.support_label_bounds.width > 0.0F
            && layout.support_label_bounds.height > 0.0F);
        ARPG_REQUIRE(inside(layout.support_label_bounds, shell.left_panel));
        std::array<Rectangle, 11> hit_regions{};
        std::size_t count = 0U;
        for (const Rectangle rectangle : layout.main_slots) {
            hit_regions[count++] = rectangle;
        }
        for (const Rectangle rectangle : layout.inventory_slots) {
            hit_regions[count++] = rectangle;
        }
        hit_regions[count++] = layout.remove_button;
        for (std::size_t left = 0U; left < count; ++left) {
            ARPG_REQUIRE(inside(hit_regions[left], viewport_bounds));
            for (std::size_t right = left + 1U; right < count; ++right) {
                ARPG_REQUIRE(!overlaps(hit_regions[left], hit_regions[right]));
            }
        }
        for (const Rectangle support : layout.support_slots) {
            ARPG_REQUIRE(inside(support, shell.left_panel));
            ARPG_REQUIRE(!overlaps(layout.support_label_bounds, support));
            for (std::size_t interactive = 0U;
                 interactive < count; ++interactive) {
                ARPG_REQUIRE(!overlaps(support, hit_regions[interactive]));
            }
        }
        for (const Rectangle main : layout.main_slots) {
            ARPG_REQUIRE(!overlaps(layout.support_label_bounds, main));
        }
        for (const Rectangle inventory : layout.inventory_slots) {
            ARPG_REQUIRE(inside(inventory, shell.right_panel));
        }
    }
    return {};
}

arpg::test::Failure slot_clicks_emit_only_select_remove_and_swap_commands()
    noexcept {
    const skills::SkillLoadoutState state = skills::default_skill_loadout();
    const platform::ActiveSkillLoadoutLayout layout =
        platform::active_skill_loadout_layout(
            platform::inventory_shell_layout(1280, 720));
    platform::ActiveSkillLoadoutSelection selection{};

    auto command = platform::active_skill_loadout_command_after_click(
        state, layout, center(layout.main_slots[1U]), selection, false);
    ARPG_REQUIRE(command.has_value());
    ARPG_REQUIRE(command->kind
        == platform::ActiveSkillLoadoutActionKind::select);
    ARPG_REQUIRE(command->slot == 1U);
    ARPG_REQUIRE(selection.selected_slot == 1U);

    command = platform::active_skill_loadout_command_after_click(
        state, layout, center(layout.main_slots[0U]), selection, false);
    ARPG_REQUIRE(command.has_value());
    ARPG_REQUIRE(command->kind
        == platform::ActiveSkillLoadoutActionKind::swap);
    ARPG_REQUIRE(command->slot == 1U);
    ARPG_REQUIRE(command->other_slot == 0U);
    ARPG_REQUIRE(selection.selected_slot == 1U);

    command = platform::active_skill_loadout_command_after_click(
        state, layout, center(layout.remove_button), selection, false);
    ARPG_REQUIRE(command.has_value());
    ARPG_REQUIRE(command->kind
        == platform::ActiveSkillLoadoutActionKind::remove);
    ARPG_REQUIRE(command->slot == 1U);
    ARPG_REQUIRE(selection.selected_slot == 1U);
    return {};
}

arpg::test::Failure occupied_slot_then_empty_slot_emits_swap_command()
    noexcept {
    const skills::SkillLoadoutState state = skills::default_skill_loadout();
    const platform::ActiveSkillLoadoutLayout layout =
        platform::active_skill_loadout_layout(
            platform::inventory_shell_layout(1280, 720));
    platform::ActiveSkillLoadoutSelection selection{};

    const auto select = platform::active_skill_loadout_command_after_click(
        state, layout, center(layout.main_slots[0U]), selection, false);
    ARPG_REQUIRE(select.has_value());
    ARPG_REQUIRE(select->kind
        == platform::ActiveSkillLoadoutActionKind::select);
    ARPG_REQUIRE(selection.selected_slot == 0U);

    const auto move = platform::active_skill_loadout_command_after_click(
        state, layout, center(layout.main_slots[2U]), selection, false);
    ARPG_REQUIRE(move.has_value());
    ARPG_REQUIRE(move->kind == platform::ActiveSkillLoadoutActionKind::swap);
    ARPG_REQUIRE(move->slot == 0U);
    ARPG_REQUIRE(move->other_slot == 2U);
    ARPG_REQUIRE(selection.selected_slot == 0U);
    return {};
}

arpg::test::Failure inventory_selection_then_empty_slot_emits_equip_command()
    noexcept {
    skills::SkillLoadoutState state = skills::default_skill_loadout();
    state.slots[0U].active = skills::ActiveSkillId::none;
    const platform::ActiveSkillLoadoutLayout layout =
        platform::active_skill_loadout_layout(
            platform::inventory_shell_layout(1280, 720));
    platform::ActiveSkillLoadoutSelection selection{};

    auto command = platform::active_skill_loadout_command_after_click(
        state, layout, center(layout.inventory_slots[0U]), selection, false);
    ARPG_REQUIRE(command.has_value());
    ARPG_REQUIRE(command->kind
        == platform::ActiveSkillLoadoutActionKind::select);
    ARPG_REQUIRE(selection.selected_inventory
        == skills::ActiveSkillId::draw_slash);

    command = platform::active_skill_loadout_command_after_click(
        state, layout, center(layout.main_slots[1U]), selection, false);
    ARPG_REQUIRE(!command.has_value());
    ARPG_REQUIRE(selection.selected_inventory
        == skills::ActiveSkillId::draw_slash);
    ARPG_REQUIRE(selection.selected_slot
        == platform::kNoActiveSkillLoadoutSelection);

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
        platform::active_skill_loadout_layout(
            platform::inventory_shell_layout(1280, 720));
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
    {"active skill loadout occupied to empty swap", &occupied_slot_then_empty_slot_emits_swap_command},
    {"active skill loadout equip command", &inventory_selection_then_empty_slot_emits_equip_command},
    {"active skill support read only and pending disabled", &pending_save_and_support_slots_never_emit_edit_commands},
};

}  // namespace

arpg::test::TestSuite active_skill_loadout_view_suite() noexcept {
    return arpg::test::make_suite("active_skill_loadout_view", kCases);
}
