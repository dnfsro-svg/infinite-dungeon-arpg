#include "test_framework.hpp"

#include "host_input.hpp"

#include "dungeon/dungeon_session.hpp"
#include "platform/settings/settings_types.hpp"

#include <array>
#include <cstddef>
#include <utility>

namespace {

using namespace arpg;

constexpr std::array<int, skills::kActiveSkillSlotCount> kNumpadSkillKeys{{
    KEY_KP_1, KEY_KP_2, KEY_KP_3, KEY_KP_4, KEY_KP_5,
}};

constexpr std::array<int, skills::kActiveSkillSlotCount> kMainNumberKeys{{
    KEY_ONE, KEY_TWO, KEY_THREE, KEY_FOUR, KEY_FIVE,
}};

struct KeySourceState final {
    std::array<bool, 512> pressed{};
    std::array<bool, 512> down{};
};

bool source_pressed(void* context, int key) noexcept {
    const auto& state = *static_cast<KeySourceState*>(context);
    return key >= 0 && static_cast<std::size_t>(key) < state.pressed.size()
        && state.pressed[static_cast<std::size_t>(key)];
}

bool source_down(void* context, int key) noexcept {
    const auto& state = *static_cast<KeySourceState*>(context);
    return key >= 0 && static_cast<std::size_t>(key) < state.down.size()
        && state.down[static_cast<std::size_t>(key)];
}

platform::PhysicalKeySource source_for(KeySourceState& state) noexcept {
    return {&state, &source_pressed, &source_down,
        nullptr, nullptr, nullptr, nullptr, nullptr};
}

void commit_pending_save(dungeon::DungeonSession& session) noexcept {
    const auto pending = session.pending_save();
    if (!pending.has_value()) return;
    session.resolve_pending_save({dungeon::SaveDisposition::committed,
        pending->expected_generation, std::move(pending->next_state),
        pending->kind});
}

test::Failure sampler_preserves_multiple_numpad_pressed_edges() noexcept {
    KeySourceState state{};
    state.pressed[KEY_KP_1] = true;
    state.pressed[KEY_KP_5] = true;
    const auto snapshot = platform::sample_physical_keys(source_for(state));
    ARPG_REQUIRE(snapshot.active_skill_slots[0U]);
    ARPG_REQUIRE(!snapshot.active_skill_slots[1U]);
    ARPG_REQUIRE(!snapshot.active_skill_slots[2U]);
    ARPG_REQUIRE(!snapshot.active_skill_slots[3U]);
    ARPG_REQUIRE(snapshot.active_skill_slots[4U]);
    return {};
}

test::Failure held_numpad_keys_do_not_repeat_as_active_skill_edges() noexcept {
    KeySourceState state{};
    state.down[KEY_KP_1] = true;
    const auto snapshot = platform::sample_physical_keys(source_for(state));
    for (bool pressed : snapshot.active_skill_slots) {
        ARPG_REQUIRE(!pressed);
    }
    return {};
}

test::Failure numpad_pressed_edges_map_to_exact_active_skill_slots() noexcept {
    for (std::size_t slot = 0U; slot < kNumpadSkillKeys.size(); ++slot) {
        KeySourceState state{};
        state.pressed[static_cast<std::size_t>(kNumpadSkillKeys[slot])] = true;
        const auto snapshot = platform::sample_physical_keys(source_for(state));
        for (std::size_t candidate = 0U;
                candidate < skills::kActiveSkillSlotCount; ++candidate) {
            ARPG_REQUIRE(snapshot.active_skill_slots[candidate]
                == (candidate == slot));
        }
    }
    return {};
}

test::Failure main_numbers_are_rejected_and_down_is_not_a_pressed_edge()
    noexcept {
    KeySourceState state{};
    for (int key : kMainNumberKeys) {
        state.pressed[static_cast<std::size_t>(key)] = true;
    }
    for (int key : kNumpadSkillKeys) {
        state.down[static_cast<std::size_t>(key)] = true;
    }
    auto snapshot = platform::sample_physical_keys(source_for(state));
    for (bool requested : snapshot.active_skill_slots) {
        ARPG_REQUIRE(!requested);
    }

    state.pressed[KEY_KP_3] = true;
    snapshot = platform::sample_physical_keys(source_for(state));
    for (std::size_t slot = 0U; slot < skills::kActiveSkillSlotCount; ++slot) {
        ARPG_REQUIRE(snapshot.active_skill_slots[slot] == (slot == 2U));
    }
    return {};
}

test::Failure mapper_forwards_slots_and_counts_them_as_attack_input() noexcept {
    platform::PhysicalKeySnapshot snapshot{};
    snapshot.active_skill_slots[0U] = true;
    snapshot.active_skill_slots[3U] = true;
    const auto input = platform::map_host_frame_input(
        settings::default_settings(), snapshot);
    ARPG_REQUIRE(input.active_skill_slots == snapshot.active_skill_slots);
    ARPG_REQUIRE(input.keys.attack);
    return {};
}

test::Failure submit_routes_same_frame_slots_in_order_until_one_is_accepted()
    noexcept {
    dungeon::DungeonSession session{};
    session.tick({});
    platform::HostFrameInput input{};
    input.active_skill_slots[0U] = true;
    input.active_skill_slots[1U] = true;
    const auto submitted = platform::submit_frame_actions(session, input);
    ARPG_REQUIRE(submitted.skills[0U] == combat::SkillCastResult::accepted);
    for (std::size_t slot = 1U; slot < skills::kActiveSkillSlotCount; ++slot) {
        ARPG_REQUIRE(submitted.skills[slot] == combat::SkillCastResult::none);
    }
    return {};
}

test::Failure submit_skips_an_empty_earlier_slot_and_accepts_later_slot()
    noexcept {
    dungeon::DungeonSession session{};
    session.tick({});
    ARPG_REQUIRE(session.request_remove_active_skill(0U)
        == dungeon::RequestResult::accepted);
    commit_pending_save(session);

    platform::HostFrameInput input{};
    input.active_skill_slots[0U] = true;
    input.active_skill_slots[1U] = true;
    const auto submitted = platform::submit_frame_actions(session, input);
    ARPG_REQUIRE(submitted.skills[0U] == combat::SkillCastResult::none);
    ARPG_REQUIRE(submitted.skills[1U] == combat::SkillCastResult::accepted);
    return {};
}

test::Failure submit_skips_a_cooling_earlier_slot_and_accepts_later_slot()
    noexcept {
    dungeon::DungeonSession session{};
    session.tick({});
    ARPG_REQUIRE(session.request_active_skill_slot(0U)
        == combat::SkillCastResult::accepted);
    bool cooling_down = false;
    for (std::size_t tick = 0U; tick < 512U; ++tick) {
        session.tick({});
        const auto snapshot = session.snapshot();
        cooling_down = snapshot.combat.has_value()
            && snapshot.combat->active_skill.id == skills::ActiveSkillId::none
            && snapshot.combat->skill_cooldowns[0U] != 0U;
        if (cooling_down) break;
    }
    ARPG_REQUIRE(cooling_down);

    platform::HostFrameInput input{};
    input.active_skill_slots[0U] = true;
    input.active_skill_slots[1U] = true;
    const auto submitted = platform::submit_frame_actions(session, input);
    ARPG_REQUIRE(submitted.skills[0U]
        == combat::SkillCastResult::cooling_down);
    ARPG_REQUIRE(submitted.skills[1U] == combat::SkillCastResult::accepted);
    return {};
}

test::Failure submit_semantics_keep_only_the_first_accepted_slot() noexcept {
    dungeon::DungeonSession session{};
    session.tick({});
    platform::HostFrameInput input{};
    input.active_skill_slots.fill(true);
    const auto submitted = platform::submit_frame_actions(session, input);
    ARPG_REQUIRE(submitted.skills[0U] == combat::SkillCastResult::accepted);
    for (std::size_t slot = 1U; slot < skills::kActiveSkillSlotCount; ++slot) {
        ARPG_REQUIRE(submitted.skills[slot] == combat::SkillCastResult::none);
    }
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"multiple numpad edges remain distinct",
        &sampler_preserves_multiple_numpad_pressed_edges},
    {"held numpad keys do not repeat",
        &held_numpad_keys_do_not_repeat_as_active_skill_edges},
    {"mapper forwards skill slots through attack gate",
        &mapper_forwards_slots_and_counts_them_as_attack_input},
    {"submit routes same frame skill slots in order",
        &submit_routes_same_frame_slots_in_order_until_one_is_accepted},
    {"submit skips empty earlier skill slot",
        &submit_skips_an_empty_earlier_slot_and_accepts_later_slot},
    {"submit skips cooling earlier skill slot",
        &submit_skips_a_cooling_earlier_slot_and_accepts_later_slot},
    {"numpad edges map to exact active skill slots",
        &numpad_pressed_edges_map_to_exact_active_skill_slots},
    {"main numbers are rejected and held keys do not repeat",
        &main_numbers_are_rejected_and_down_is_not_a_pressed_edge},
    {"submit semantics keep only the first accepted slot",
        &submit_semantics_keep_only_the_first_accepted_slot},
};

}  // namespace

arpg::test::TestSuite active_skill_input_suite() noexcept {
    return arpg::test::make_suite("active_skill_input", kCases);
}
