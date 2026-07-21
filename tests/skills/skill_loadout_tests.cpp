#include "test_framework.hpp"

#include "skills/active_skill_catalog.hpp"
#include "skills/skill_loadout.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace {

using namespace arpg::skills;

arpg::test::Failure default_loadout_equips_owned_active_stones_and_empty_supports() noexcept {
    const SkillLoadoutState state = default_skill_loadout();
    ARPG_REQUIRE(state.owned_active_bits == ((std::uint64_t{1U} << 0U) | (std::uint64_t{1U} << 1U)));
    ARPG_REQUIRE(state.slots[0].active == ActiveSkillId::draw_slash);
    ARPG_REQUIRE(state.slots[1].active == ActiveSkillId::storm_swords);
    for (std::size_t index = 2U; index < state.slots.size(); ++index)
        ARPG_REQUIRE(state.slots[index].active == ActiveSkillId::none);
    for (const ActiveSkillSlot& slot : state.slots) {
        for (const SupportSkillId support : slot.supports)
            ARPG_REQUIRE(support == SupportSkillId::none);
    }
    ARPG_REQUIRE(validate_skill_loadout(state) == SkillLoadoutError::none);
    return {};
}

arpg::test::Failure catalog_defines_the_two_active_skills() noexcept {
    const ActiveSkillDefinition* draw = active_skill_definition(ActiveSkillId::draw_slash);
    const ActiveSkillDefinition* storm = active_skill_definition(ActiveSkillId::storm_swords);
    ARPG_REQUIRE(draw != nullptr);
    ARPG_REQUIRE(storm != nullptr);
    ARPG_REQUIRE(std::strcmp(
        draw->display_name, u8"\u62D4\u5200\u65A9") == 0);
    ARPG_REQUIRE(std::strcmp(
        storm->display_name,
        u8"\u6781\u00B7\u9B3C\u5251\u672F\uFF08\u66B4\u98CE\u5F0F\uFF09")
        == 0);
    ARPG_REQUIRE(draw->cooldown_ticks == kDrawSlashCooldownTicks);
    ARPG_REQUIRE(storm->cooldown_ticks == kStormSwordsCooldownTicks);
    ARPG_REQUIRE(active_skill_definition(ActiveSkillId::none) == nullptr);
    return {};
}

arpg::test::Failure duplicate_active_stone_is_rejected() noexcept {
    SkillLoadoutState state = default_skill_loadout();
    ARPG_REQUIRE(equip_active_skill(state, ActiveSkillId::draw_slash, 2U) == SkillLoadoutError::duplicate_active);
    return {};
}

arpg::test::Failure unowned_active_stone_is_rejected() noexcept {
    SkillLoadoutState state{};
    ARPG_REQUIRE(equip_active_skill(state, ActiveSkillId::draw_slash, 0U) == SkillLoadoutError::active_not_owned);
    return {};
}

arpg::test::Failure invalid_active_id_is_rejected() noexcept {
    SkillLoadoutState state = default_skill_loadout();
    const auto invalid = static_cast<ActiveSkillId>(42U);
    ARPG_REQUIRE(equip_active_skill(state, invalid, 0U) == SkillLoadoutError::invalid_active_id);
    return {};
}

arpg::test::Failure invalid_support_id_is_rejected_by_validation() noexcept {
    SkillLoadoutState state = default_skill_loadout();
    state.slots[0].supports[0] = SupportSkillId::count;
    ARPG_REQUIRE(validate_skill_loadout(state) == SkillLoadoutError::invalid_support_id);
    return {};
}

arpg::test::Failure removing_an_active_stone_empties_its_slot() noexcept {
    SkillLoadoutState state = default_skill_loadout();
    ARPG_REQUIRE(remove_active_skill(state, 0U) == SkillLoadoutError::none);
    ARPG_REQUIRE(state.slots[0].active == ActiveSkillId::none);
    return {};
}

arpg::test::Failure equipping_an_owned_active_stone_uses_an_empty_slot() noexcept {
    SkillLoadoutState state = default_skill_loadout();
    ARPG_REQUIRE(remove_active_skill(state, 1U) == SkillLoadoutError::none);
    ARPG_REQUIRE(equip_active_skill(state, ActiveSkillId::storm_swords, 4U) == SkillLoadoutError::none);
    ARPG_REQUIRE(state.slots[4].active == ActiveSkillId::storm_swords);
    return {};
}

arpg::test::Failure occupied_destination_is_rejected_without_overwriting() noexcept {
    SkillLoadoutState state = default_skill_loadout();
    ARPG_REQUIRE(equip_active_skill(state, ActiveSkillId::storm_swords, 0U) == SkillLoadoutError::destination_occupied);
    ARPG_REQUIRE(state.slots[0].active == ActiveSkillId::draw_slash);
    return {};
}

arpg::test::Failure active_slots_can_be_swapped() noexcept {
    SkillLoadoutState state = default_skill_loadout();
    ARPG_REQUIRE(swap_active_skill_slots(state, 0U, 1U) == SkillLoadoutError::none);
    ARPG_REQUIRE(state.slots[0].active == ActiveSkillId::storm_swords);
    ARPG_REQUIRE(state.slots[1].active == ActiveSkillId::draw_slash);
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"default loadout equips owned active stones and empty supports", &default_loadout_equips_owned_active_stones_and_empty_supports},
    {"catalog defines two active skills", &catalog_defines_the_two_active_skills},
    {"duplicate active stone rejected", &duplicate_active_stone_is_rejected},
    {"unowned active stone rejected", &unowned_active_stone_is_rejected},
    {"invalid active id rejected", &invalid_active_id_is_rejected},
    {"invalid support id rejected", &invalid_support_id_is_rejected_by_validation},
    {"removing active stone empties slot", &removing_an_active_stone_empties_its_slot},
    {"owned active stone equips empty slot", &equipping_an_owned_active_stone_uses_an_empty_slot},
    {"occupied destination rejected", &occupied_destination_is_rejected_without_overwriting},
    {"active slots swap", &active_slots_can_be_swapped},
};

}  // namespace

arpg::test::TestSuite skill_loadout_suite() noexcept {
    return arpg::test::make_suite("skill_loadout", kCases);
}
