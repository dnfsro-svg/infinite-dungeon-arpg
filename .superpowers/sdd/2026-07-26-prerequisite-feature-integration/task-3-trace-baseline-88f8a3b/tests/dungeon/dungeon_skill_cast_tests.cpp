#include "test_framework.hpp"

#include "dungeon/dungeon_session.hpp"

#include <cstdint>
#include <utility>

namespace {

using namespace arpg;

void commit_pending_save(dungeon::DungeonSession& session) noexcept {
    const auto pending = session.pending_save();
    if (!pending.has_value()) return;
    session.resolve_pending_save({dungeon::SaveDisposition::committed,
        pending->expected_generation, std::move(pending->next_state),
        pending->kind});
}

test::Failure stable_combat_slots_resolve_to_their_equipped_skills() noexcept {
    dungeon::DungeonSession draw_slash{};
    draw_slash.tick({});
    ARPG_REQUIRE(draw_slash.request_active_skill_slot(0U)
        == combat::SkillCastResult::accepted);

    dungeon::DungeonSession storm_swords{};
    storm_swords.tick({});
    ARPG_REQUIRE(storm_swords.request_active_skill_slot(1U)
        == combat::SkillCastResult::accepted);
    return {};
}

test::Failure empty_or_invalid_slot_has_no_combat_side_effect() noexcept {
    dungeon::DungeonSession session{};
    session.tick({});
    ARPG_REQUIRE(session.request_active_skill_slot(2U)
        == combat::SkillCastResult::none);
    ARPG_REQUIRE(session.request_active_skill_slot(5U)
        == combat::SkillCastResult::none);
    ARPG_REQUIRE(session.snapshot().combat.has_value());
    ARPG_REQUIRE(session.snapshot().combat->active_skill.id
        == skills::ActiveSkillId::none);
    return {};
}

test::Failure non_combat_room_phase_rejects_slot_casts() noexcept {
    dungeon::DungeonSession session{};
    ARPG_REQUIRE(session.request_active_skill_slot(0U)
        == combat::SkillCastResult::none);
    return {};
}

test::Failure slot_cast_follows_the_current_loadout_after_a_swap() noexcept {
    dungeon::DungeonSession session{};
    session.tick({});
    ARPG_REQUIRE(session.request_swap_active_skill_slots(0U, 1U)
        == dungeon::RequestResult::accepted);
    commit_pending_save(session);
    ARPG_REQUIRE(session.request_active_skill_slot(0U)
        == combat::SkillCastResult::accepted);
    ARPG_REQUIRE(session.snapshot().combat.has_value());
    ARPG_REQUIRE(session.snapshot().combat->active_skill.id
        == skills::ActiveSkillId::storm_swords);
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"stable combat slots resolve equipped skills",
        &stable_combat_slots_resolve_to_their_equipped_skills},
    {"empty and invalid slot has no side effect",
        &empty_or_invalid_slot_has_no_combat_side_effect},
    {"non combat phase rejects slot casts",
        &non_combat_room_phase_rejects_slot_casts},
    {"slot cast follows swapped loadout",
        &slot_cast_follows_the_current_loadout_after_a_swap},
};

}  // namespace

arpg::test::TestSuite dungeon_skill_cast_suite() noexcept {
    return arpg::test::make_suite("dungeon_skill_cast", kCases);
}
