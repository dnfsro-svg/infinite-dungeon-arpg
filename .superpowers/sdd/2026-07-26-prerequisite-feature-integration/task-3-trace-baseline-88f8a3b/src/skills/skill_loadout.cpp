#include "skills/skill_loadout.hpp"

namespace arpg::skills {
namespace {

constexpr ActiveSkillSlot empty_active_skill_slot() noexcept {
    return {ActiveSkillId::none, {{
        SupportSkillId::none, SupportSkillId::none,
        SupportSkillId::none, SupportSkillId::none,
        SupportSkillId::none,
    }}};
}

constexpr bool valid_active_skill_id(const ActiveSkillId id) noexcept {
    return static_cast<std::uint8_t>(id) < kActiveSkillCount;
}

constexpr bool owns_active_skill(
    const SkillLoadoutState& state, const ActiveSkillId id) noexcept {
    return (state.owned_active_bits &
        (std::uint64_t{1U} << static_cast<std::uint8_t>(id))) != 0U;
}

}  // namespace

SkillLoadoutState default_skill_loadout() noexcept {
    SkillLoadoutState state{};
    for (ActiveSkillSlot& slot : state.slots)
        slot = empty_active_skill_slot();
    state.owned_active_bits = (std::uint64_t{1U} <<
            static_cast<std::uint8_t>(ActiveSkillId::draw_slash)) |
        (std::uint64_t{1U} <<
            static_cast<std::uint8_t>(ActiveSkillId::storm_swords));
    state.slots[0].active = ActiveSkillId::draw_slash;
    state.slots[1].active = ActiveSkillId::storm_swords;
    return state;
}

SkillLoadoutError validate_skill_loadout(const SkillLoadoutState& state) noexcept {
    if ((state.owned_active_bits >> kActiveSkillCount) != 0U)
        return SkillLoadoutError::invalid_active_id;

    for (std::size_t index = 0U; index < state.slots.size(); ++index) {
        const ActiveSkillSlot& slot = state.slots[index];
        for (const SupportSkillId support : slot.supports) {
            if (support != SupportSkillId::none)
                return SkillLoadoutError::invalid_support_id;
        }
        if (slot.active == ActiveSkillId::none)
            continue;
        if (!valid_active_skill_id(slot.active))
            return SkillLoadoutError::invalid_active_id;
        if (!owns_active_skill(state, slot.active))
            return SkillLoadoutError::active_not_owned;
        for (std::size_t other = 0U; other < index; ++other) {
            if (state.slots[other].active == slot.active)
                return SkillLoadoutError::duplicate_active;
        }
    }
    return SkillLoadoutError::none;
}

SkillLoadoutError remove_active_skill(
    SkillLoadoutState& state, const std::size_t slot) noexcept {
    if (slot >= state.slots.size())
        return SkillLoadoutError::invalid_slot;
    state.slots[slot] = empty_active_skill_slot();
    return SkillLoadoutError::none;
}

SkillLoadoutError equip_active_skill(
    SkillLoadoutState& state, const ActiveSkillId active,
    const std::size_t slot) noexcept {
    if (slot >= state.slots.size())
        return SkillLoadoutError::invalid_slot;
    if (!valid_active_skill_id(active))
        return SkillLoadoutError::invalid_active_id;
    if (!owns_active_skill(state, active))
        return SkillLoadoutError::active_not_owned;
    if (state.slots[slot].active != ActiveSkillId::none)
        return SkillLoadoutError::destination_occupied;
    for (const ActiveSkillSlot& candidate : state.slots) {
        if (candidate.active == active)
            return SkillLoadoutError::duplicate_active;
    }
    state.slots[slot] = empty_active_skill_slot();
    state.slots[slot].active = active;
    return SkillLoadoutError::none;
}

SkillLoadoutError swap_active_skill_slots(
    SkillLoadoutState& state, const std::size_t left,
    const std::size_t right) noexcept {
    if (left >= state.slots.size() || right >= state.slots.size())
        return SkillLoadoutError::invalid_slot;
    const ActiveSkillSlot temporary = state.slots[left];
    state.slots[left] = state.slots[right];
    state.slots[right] = temporary;
    return SkillLoadoutError::none;
}

}  // namespace arpg::skills
