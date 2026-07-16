#include "test_framework.hpp"

#include "dungeon_test_support.hpp"

#include "dungeon/dungeon_progression.hpp"

#include <cstdint>

namespace {

using arpg::combat::MovementInput;
using arpg::dungeon::DungeonElement;
using arpg::dungeon::DungeonRules;
using arpg::dungeon::DungeonRunState;
using arpg::dungeon::DungeonSession;
using arpg::dungeon::ExitDirection;
using arpg::dungeon::RoomPhase;

DungeonRules two_wave_rules() noexcept {
    DungeonRules rules;
    rules.encounter.base_budget = 16U;
    rules.encounter.max_budget = 16U;
    return rules;
}

DungeonRunState state_for_seed(std::uint64_t seed, const DungeonRules& rules) noexcept {
    return arpg::dungeon::make_initial_run_state(seed, rules).state;
}

bool all_exits_closed(const arpg::dungeon::DungeonSnapshot& state) noexcept {
    for (const bool open : state.exits_open) {
        if (open) {
            return false;
        }
    }
    return true;
}

bool clear_current_wave(DungeonSession& session) noexcept {
    for (int tick = 0; tick < 128; ++tick) {
        const auto state = session.snapshot();
        if (state.phase == RoomPhase::wave_delay
                || state.phase == RoomPhase::cleared
                || state.phase == RoomPhase::awaiting_exit) {
            return true;
        }

        if (state.phase == RoomPhase::combat) {
            arpg::test::force_defeat_current_wave(session);
        }
        session.tick({});
    }
    return false;
}

bool tick_wave_delay(DungeonSession& session) noexcept {
    for (int tick = 0; tick < 64; ++tick) {
        session.tick({});
        if (session.snapshot().phase == RoomPhase::combat
                && session.snapshot().wave_index == 1U) {
            return true;
        }
    }
    return false;
}

arpg::test::Failure two_wave_room_keeps_exits_closed_until_last_wave() noexcept {
    const DungeonRules rules = two_wave_rules();
    DungeonSession session{rules, state_for_seed(0x2A11CEU, rules)};

    ARPG_REQUIRE(clear_current_wave(session));
    ARPG_REQUIRE(session.snapshot().phase == RoomPhase::wave_delay);
    ARPG_REQUIRE(session.snapshot().wave_count == 2U);
    ARPG_REQUIRE(session.snapshot().wave_index == 0U);
    ARPG_REQUIRE(session.snapshot().wave_delay_ticks > 0U);
    ARPG_REQUIRE(all_exits_closed(session.snapshot()));
    ARPG_REQUIRE(!session.request_descent(true));
    ARPG_REQUIRE(!session.queue_action(arpg::combat::Action::light));

    ARPG_REQUIRE(tick_wave_delay(session));
    ARPG_REQUIRE(session.snapshot().wave_index == 1U);
    ARPG_REQUIRE(session.snapshot().remaining_targets > 0U);
    ARPG_REQUIRE(clear_current_wave(session));
    session.tick({});
    ARPG_REQUIRE(session.snapshot().phase == RoomPhase::awaiting_exit);
    for (const bool open : session.snapshot().exits_open) {
        ARPG_REQUIRE(open);
    }
    return {};
}

arpg::test::Failure sealed_hole_stays_closed_through_wave_delay() noexcept {
    const DungeonRules rules = two_wave_rules();
    DungeonRunState state = state_for_seed(0xA8A55U, rules);
    state.current_room.has_hole = true;
    DungeonSession session{rules, state};

    ARPG_REQUIRE(clear_current_wave(session));
    ARPG_REQUIRE(session.snapshot().phase == RoomPhase::wave_delay);
    ARPG_REQUIRE(session.snapshot().has_hole);
    ARPG_REQUIRE(!session.request_descent(true));
    ARPG_REQUIRE(all_exits_closed(session.snapshot()));
    return {};
}

arpg::test::Failure reset_and_reload_rebuild_the_same_encounter_plan() noexcept {
    const DungeonRules rules = two_wave_rules();
    const DungeonRunState state = state_for_seed(0xC0FFEEU, rules);
    DungeonSession session{rules, state};
    const auto plan = arpg::test::encounter_plan(session);
    const auto before = session.snapshot();
    static_cast<void>(session.reset_current_room());
    const auto after_reset = session.snapshot();
    DungeonSession reloaded{rules, state};
    const auto after_reload = reloaded.snapshot();
    ARPG_REQUIRE(arpg::test::same_encounter_plan(
        plan, arpg::test::encounter_plan(session)));
    ARPG_REQUIRE(arpg::test::same_encounter_plan(
        plan, arpg::test::encounter_plan(reloaded)));

    ARPG_REQUIRE(before.encounter.total_budget == after_reset.encounter.total_budget);
    ARPG_REQUIRE(before.encounter.total_budget == after_reload.encounter.total_budget);
    ARPG_REQUIRE(before.wave_count == after_reset.wave_count);
    ARPG_REQUIRE(before.wave_count == after_reload.wave_count);
    ARPG_REQUIRE(before.combat->monster_count == after_reset.combat->monster_count);
    ARPG_REQUIRE(before.combat->monster_count == after_reload.combat->monster_count);
    for (std::size_t index = 0U; index < before.combat->monsters.size(); ++index) {
        const auto& first = before.combat->monsters[index];
        const auto& reset = after_reset.combat->monsters[index];
        const auto& reload = after_reload.combat->monsters[index];
        ARPG_REQUIRE(first.active == reset.active && first.active == reload.active);
        ARPG_REQUIRE(first.id == reset.id && first.id == reload.id);
        ARPG_REQUIRE(first.position.x == reset.position.x && first.position.x == reload.position.x);
        ARPG_REQUIRE(first.position.y == reset.position.y && first.position.y == reload.position.y);
    }
    return {};
}

arpg::test::Failure legacy_abyss_flag_without_lifecycle_faults() noexcept {
    DungeonRules rules = two_wave_rules();
    DungeonRunState abyss = state_for_seed(0xAB155U, rules);
    abyss.current_room.is_abyss = true;
    DungeonSession abyss_session{rules, abyss};
    const auto abyss_snapshot = abyss_session.snapshot();
    ARPG_REQUIRE(abyss_snapshot.phase == RoomPhase::faulted);
    ARPG_REQUIRE(abyss_snapshot.diagnostics.fault
        == arpg::dungeon::DungeonFault::invalid_abyss_state);
    ARPG_REQUIRE(!abyss_snapshot.combat.has_value());
    return {};
}

arpg::test::Failure four_chaser_fixture_reaches_awaiting_exit() noexcept {
    DungeonRules rules;
    DungeonSession session{rules, state_for_seed(0xA11CE5EEDULL, rules)};
    const auto initial = session.snapshot();
    ARPG_REQUIRE(initial.combat.has_value());
    ARPG_REQUIRE(initial.remaining_targets == 4U);
    for (const auto& monster : initial.combat->monsters) {
        if (monster.active) {
            ARPG_REQUIRE(monster.id == arpg::combat::MonsterId::chaos_chaser);
        }
    }

    arpg::test::EventSummary events;
    ARPG_REQUIRE(arpg::test::drive_until_cleared(session, events, 4096));
    if (session.snapshot().phase == RoomPhase::cleared) {
        session.tick({});
    }
    ARPG_REQUIRE(session.snapshot().phase == RoomPhase::awaiting_exit);
    return {};
}

arpg::test::Failure shooter_and_chasers_fixture_reaches_awaiting_exit() noexcept {
    DungeonSession session;
    const auto initial = session.snapshot();
    ARPG_REQUIRE(initial.combat.has_value());
    ARPG_REQUIRE(initial.remaining_targets == 3U);
    bool has_shooter = false;
    for (const auto& monster : initial.combat->monsters) {
        has_shooter = has_shooter
            || (monster.active
                && monster.id == arpg::combat::MonsterId::lightning_shooter);
    }
    ARPG_REQUIRE(has_shooter);

    arpg::test::EventSummary events;
    ARPG_REQUIRE(arpg::test::drive_until_cleared(session, events, 4096));
    if (session.snapshot().phase == RoomPhase::cleared) {
        session.tick({});
    }
    ARPG_REQUIRE(session.snapshot().phase == RoomPhase::awaiting_exit);
    return {};
}

arpg::test::Failure forced_defeat_advances_real_wave_lifecycle() noexcept {
    const DungeonRules rules = two_wave_rules();
    DungeonSession session{rules, state_for_seed(0x2A11CEU, rules)};
    session.tick({});
    arpg::test::force_defeat_current_wave(session);
    session.tick({});
    ARPG_REQUIRE(session.snapshot().phase == RoomPhase::wave_delay);
    ARPG_REQUIRE(session.snapshot().remaining_targets == 0U);
    ARPG_REQUIRE(all_exits_closed(session.snapshot()));
    return {};
}

arpg::test::Failure wave_delay_freezes_combat_and_preserves_health_until_wave_one() noexcept {
    const DungeonRules rules = two_wave_rules();
    DungeonSession session{rules, state_for_seed(0x2A11CEU, rules)};
    session.tick({});
    arpg::test::damage_current_player(session, 100);
    arpg::test::force_defeat_current_wave(session);
    session.tick({});
    const auto delay = session.snapshot();
    ARPG_REQUIRE(delay.phase == RoomPhase::wave_delay);
    const int hp = delay.combat->player.hp;
    const auto player = delay.combat->player;
    const auto combat_tick = delay.combat->tick;
    for (int tick = 0; tick < 44; ++tick) {
        session.tick({});
        const auto frozen = session.snapshot();
        ARPG_REQUIRE(frozen.phase == RoomPhase::wave_delay);
        ARPG_REQUIRE(frozen.combat->tick == combat_tick);
        ARPG_REQUIRE(frozen.combat->player.hp == hp);
        ARPG_REQUIRE(frozen.combat->player.state == player.state);
        ARPG_REQUIRE(all_exits_closed(frozen));
    }
    session.tick({});
    const auto wave_one = session.snapshot();
    ARPG_REQUIRE(wave_one.phase == RoomPhase::combat);
    ARPG_REQUIRE(wave_one.wave_index == 1U);
    ARPG_REQUIRE(wave_one.combat->player.hp == hp);
    return {};
}

arpg::test::Failure hole_stays_rejected_for_every_delay_tick() noexcept {
    const DungeonRules rules = two_wave_rules();
    auto state = state_for_seed(0x2A11CEU, rules);
    state.current_room.has_hole = true;
    DungeonSession session{rules, state};
    session.tick({});
    arpg::test::force_defeat_current_wave(session);
    session.tick({});
    for (int tick = 0; tick < 44; ++tick) {
        const auto snapshot = session.snapshot();
        ARPG_REQUIRE(snapshot.phase == RoomPhase::wave_delay);
        ARPG_REQUIRE(snapshot.has_hole);
        ARPG_REQUIRE(!session.request_descent(true));
        ARPG_REQUIRE(!session.pending_transition().has_value());
        session.tick({});
    }
    session.tick({});
    ARPG_REQUIRE(session.snapshot().wave_index == 1U);
    return {};
}

arpg::test::Failure rollback_keeps_health_but_committed_room_resets_it() noexcept {
    DungeonRules rules;
    auto state = state_for_seed(0x515151U, rules);
    state.current_room.has_hole = true;
    const auto reach_awaiting = [](DungeonSession& session) noexcept {
        session.tick({});
        arpg::test::force_defeat_current_wave(session);
        session.tick({});
        if (session.snapshot().phase == RoomPhase::cleared) session.tick({});
        return session.snapshot().phase == RoomPhase::awaiting_exit;
    };

    DungeonSession rollback{rules, state};
    ARPG_REQUIRE(reach_awaiting(rollback));
    arpg::test::damage_current_player(rollback, 100);
    const int damaged = rollback.snapshot().combat->player.hp;
    ARPG_REQUIRE(damaged < rollback.snapshot().combat->player.max_hp);
    ARPG_REQUIRE(rollback.request_descent(true));
    const auto failed = rollback.pending_transition();
    ARPG_REQUIRE(failed.has_value());
    rollback.resolve_pending_transition({arpg::dungeon::SaveDisposition::not_committed, 0U, {}});
    ARPG_REQUIRE(rollback.snapshot().room_seed == state.current_room.seed);
    ARPG_REQUIRE(rollback.snapshot().combat->player.hp == damaged);

    DungeonSession committed{rules, state};
    ARPG_REQUIRE(reach_awaiting(committed));
    arpg::test::damage_current_player(committed, 100);
    ARPG_REQUIRE(committed.request_descent(true));
    const auto pending = committed.pending_transition();
    ARPG_REQUIRE(pending.has_value());
    committed.resolve_pending_transition({arpg::dungeon::SaveDisposition::committed,
        pending->next_state.commit_generation, pending->next_state});
    committed.tick({});
    const auto fresh = committed.snapshot();
    ARPG_REQUIRE(fresh.room_seed != state.current_room.seed);
    ARPG_REQUIRE(fresh.combat->player.hp == fresh.combat->player.max_hp);
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"two wave room keeps exits closed until last wave", &two_wave_room_keeps_exits_closed_until_last_wave},
    {"sealed hole stays closed through wave delay", &sealed_hole_stays_closed_through_wave_delay},
    {"reset and reload rebuild the same encounter plan", &reset_and_reload_rebuild_the_same_encounter_plan},
    {"legacy abyss flag without lifecycle faults", &legacy_abyss_flag_without_lifecycle_faults},
    {"four chaser fixture reaches awaiting exit", &four_chaser_fixture_reaches_awaiting_exit},
    {"shooter and chasers fixture reaches awaiting exit", &shooter_and_chasers_fixture_reaches_awaiting_exit},
    {"forced defeat advances real wave lifecycle", &forced_defeat_advances_real_wave_lifecycle},
    {"wave delay freezes combat and preserves health", &wave_delay_freezes_combat_and_preserves_health_until_wave_one},
    {"hole stays rejected for every delay tick", &hole_stays_rejected_for_every_delay_tick},
    {"rollback keeps health but committed room resets it", &rollback_keeps_health_but_committed_room_resets_it},
};

}  // namespace

arpg::test::TestSuite dungeon_wave_suite() noexcept {
    return arpg::test::make_suite("dungeon_waves", kCases);
}
