#include "test_framework.hpp"

#include "dungeon_test_support.hpp"

#include "dungeon/dungeon_progression.hpp"
#include "dungeon/dungeon_session.hpp"
#include "persistence/save_store.hpp"
#include "progression/progression_rules.hpp"
#include "skills/skill_loadout.hpp"

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <utility>

namespace {

namespace dungeon = arpg::dungeon;
namespace persistence = arpg::persistence;
namespace skills = arpg::skills;

struct TempDirectory final {
    std::filesystem::path path{};

    TempDirectory() noexcept {
        std::error_code error;
        path = std::filesystem::temp_directory_path(error)
            / "arpg_skill_loadout_transaction"
            / std::to_string(static_cast<unsigned long long>(
                std::hash<std::string>{}(std::to_string(
                    reinterpret_cast<std::uintptr_t>(this)))));
        std::filesystem::remove_all(path, error);
        std::filesystem::create_directories(path, error);
        if (error) path.clear();
    }

    ~TempDirectory() noexcept {
        std::error_code error;
        std::filesystem::remove_all(path, error);
    }
};

struct FaultContext final {
    persistence::SaveFaultPoint point{};
    bool triggered{};
};

bool fail_once(persistence::SaveFaultPoint point, void* opaque) noexcept {
    auto* const context = static_cast<FaultContext*>(opaque);
    if (context == nullptr || point != context->point || context->triggered)
        return false;
    context->triggered = true;
    return true;
}

persistence::SaveStore make_store(
    const std::filesystem::path& directory,
    FaultContext* fault = nullptr) {
    persistence::SaveStoreConfig config{};
    config.directory = directory;
    if (fault != nullptr) {
        config.fault_hook = &fail_once;
        config.fault_context = fault;
    }
    return persistence::SaveStore(std::move(config));
}

dungeon::PendingSaveResult to_session_result(
    persistence::SaveCommitResult saved,
    dungeon::PendingSaveKind kind) noexcept {
    dungeon::SaveDisposition disposition =
        dungeon::SaveDisposition::indeterminate;
    if (saved.state == persistence::SaveCommitState::committed) {
        disposition = dungeon::SaveDisposition::committed;
    } else if (saved.state == persistence::SaveCommitState::not_committed) {
        disposition = dungeon::SaveDisposition::not_committed;
    }
    const std::uint64_t generation = saved.verified_state.commit_generation;
    return {disposition, generation, std::move(saved.verified_state), kind};
}

bool same_loadout(const skills::SkillLoadoutState& left,
    const skills::SkillLoadoutState& right) noexcept {
    if (left.owned_active_bits != right.owned_active_bits) return false;
    for (std::size_t slot = 0U; slot < left.slots.size(); ++slot) {
        if (left.slots[slot].active != right.slots[slot].active
                || left.slots[slot].supports != right.slots[slot].supports) {
            return false;
        }
    }
    return true;
}

bool same_progression(const arpg::progression::ProgressionState& left,
    const arpg::progression::ProgressionState& right) noexcept {
    return left.level == right.level
        && left.experience == right.experience
        && left.earned_passive_points == right.earned_passive_points
        && left.unspent_passive_points == right.unspent_passive_points;
}

enum class IsolatedMutation : std::uint8_t { remove, equip, swap };

bool commit_pending_with_store(
    dungeon::DungeonSession& session,
    persistence::SaveStore& store) noexcept {
    const dungeon::PendingSave* const pending = session.pending_save_view();
    if (pending == nullptr) return false;
    const dungeon::PendingSaveKind kind = pending->kind;
    auto saved = store.commit(pending->next_state);
    const bool committed = saved.state
        == persistence::SaveCommitState::committed;
    session.resolve_pending_save(to_session_result(std::move(saved), kind));
    return committed
        && session.snapshot().phase != dungeon::RoomPhase::faulted;
}

dungeon::DungeonRunState initial_state(std::uint64_t seed) noexcept {
    return dungeon::make_initial_run_state(seed, dungeon::DungeonRules{}).state;
}

bool isolated_mutation_changes_only_loadout_and_generation(
    IsolatedMutation mutation, std::uint64_t seed) noexcept {
    TempDirectory directory;
    if (directory.path.empty()) return false;
    auto state = initial_state(seed);
    if (mutation == IsolatedMutation::equip) {
        if (skills::remove_active_skill(state.skill_loadout, 0U)
                != skills::SkillLoadoutError::none) {
            return false;
        }
    } else if (mutation == IsolatedMutation::swap) {
        if (skills::swap_active_skill_slots(state.skill_loadout, 0U, 4U)
                != skills::SkillLoadoutError::none) {
            return false;
        }
    }
    auto store = make_store(directory.path);
    auto seeded = store.commit(state);
    if (seeded.state != persistence::SaveCommitState::committed) return false;
    dungeon::DungeonSession session{{}, std::move(seeded.verified_state)};
    const dungeon::DungeonRunState before = arpg::test::stable_state(session);
    const arpg::progression::ProgressionRules progression_rules =
        arpg::progression::default_progression_rules();
    const arpg::progression::ProgressionState live =
        arpg::progression::apply_experience(
            before.progression, 1U, progression_rules).state;
    if (!arpg::progression::valid_progression_state(
            live, progression_rules)
            || same_progression(live, before.progression)) {
        return false;
    }
    arpg::test::set_room_progression(session, live);

    skills::SkillLoadoutState expected_loadout = before.skill_loadout;
    dungeon::RequestResult requested = dungeon::RequestResult::faulted;
    if (mutation == IsolatedMutation::remove) {
        if (skills::remove_active_skill(expected_loadout, 0U)
                != skills::SkillLoadoutError::none) {
            return false;
        }
        requested = session.request_remove_active_skill(0U);
    } else if (mutation == IsolatedMutation::equip) {
        if (skills::equip_active_skill(expected_loadout,
                skills::ActiveSkillId::draw_slash, 4U)
                != skills::SkillLoadoutError::none) {
            return false;
        }
        requested = session.request_equip_active_skill(
            skills::ActiveSkillId::draw_slash, 4U);
    } else {
        if (skills::swap_active_skill_slots(expected_loadout, 1U, 4U)
                != skills::SkillLoadoutError::none) {
            return false;
        }
        requested = session.request_swap_active_skill_slots(1U, 4U);
    }
    if (requested != dungeon::RequestResult::accepted) return false;
    const dungeon::PendingSave* const pending = session.pending_save_view();
    if (pending == nullptr
            || pending->kind != dungeon::PendingSaveKind::skill_loadout
            || !dungeon::same_run_state(
                arpg::test::stable_state(session), before)
            || session.snapshot().commit_generation != before.commit_generation
            || !same_loadout(session.snapshot().skill_loadout,
                before.skill_loadout)
            || !same_progression(session.snapshot().progression, live)) {
        return false;
    }

    dungeon::DungeonRunState expected = before;
    ++expected.commit_generation;
    expected.skill_loadout = expected_loadout;
    if (!dungeon::same_run_state(pending->next_state, expected)
            || !commit_pending_with_store(session, store)
            || !dungeon::same_run_state(
                arpg::test::stable_state(session), expected)
            || !same_progression(session.snapshot().progression, live)
            || !same_loadout(session.snapshot().skill_loadout,
                expected_loadout)) {
        return false;
    }
    return true;
}

arpg::test::Failure snapshot_starts_with_value_owned_default_loadout() noexcept {
    dungeon::DungeonSession session{};
    auto snapshot = session.snapshot();
    const auto expected = skills::default_skill_loadout();
    ARPG_REQUIRE(same_loadout(snapshot.skill_loadout, expected));
    ARPG_REQUIRE(snapshot.skill_loadout.slots[0U].active
        == skills::ActiveSkillId::draw_slash);
    ARPG_REQUIRE(snapshot.skill_loadout.slots[1U].active
        == skills::ActiveSkillId::storm_swords);
    for (std::size_t slot = 2U; slot < skills::kActiveSkillSlotCount; ++slot) {
        ARPG_REQUIRE(snapshot.skill_loadout.slots[slot].active
            == skills::ActiveSkillId::none);
    }
    snapshot.skill_loadout.slots[0U].active = skills::ActiveSkillId::none;
    ARPG_REQUIRE(session.snapshot().skill_loadout.slots[0U].active
        == skills::ActiveSkillId::draw_slash);
    return {};
}

arpg::test::Failure real_store_remove_equip_and_swap_publish_atomically()
    noexcept {
    TempDirectory directory;
    ARPG_REQUIRE(!directory.path.empty());
    auto store = make_store(directory.path);
    auto seeded = store.commit(initial_state(0x51717A01ULL));
    ARPG_REQUIRE(seeded.state == persistence::SaveCommitState::committed);
    dungeon::DungeonSession session{{}, std::move(seeded.verified_state)};

    auto stable = session.snapshot();
    ARPG_REQUIRE(session.request_remove_active_skill(0U)
        == dungeon::RequestResult::accepted);
    ARPG_REQUIRE(session.pending_save_view() != nullptr);
    ARPG_REQUIRE(session.pending_save_view()->kind
        == dungeon::PendingSaveKind::skill_loadout);
    ARPG_REQUIRE(same_loadout(session.snapshot().skill_loadout,
        stable.skill_loadout));
    ARPG_REQUIRE(session.snapshot().commit_generation
        == stable.commit_generation);
    ARPG_REQUIRE(session.pending_save_view()->next_state.skill_loadout
        .slots[0U].active == skills::ActiveSkillId::none);
    ARPG_REQUIRE(commit_pending_with_store(session, store));
    auto published = session.snapshot();
    ARPG_REQUIRE(published.commit_generation == stable.commit_generation + 1U);
    ARPG_REQUIRE(published.skill_loadout.slots[0U].active
        == skills::ActiveSkillId::none);

    stable = published;
    ARPG_REQUIRE(session.request_equip_active_skill(
        skills::ActiveSkillId::draw_slash, 4U)
        == dungeon::RequestResult::accepted);
    ARPG_REQUIRE(same_loadout(session.snapshot().skill_loadout,
        stable.skill_loadout));
    ARPG_REQUIRE(session.pending_save_view()->next_state.skill_loadout
        .slots[4U].active == skills::ActiveSkillId::draw_slash);
    ARPG_REQUIRE(commit_pending_with_store(session, store));
    published = session.snapshot();
    ARPG_REQUIRE(published.commit_generation == stable.commit_generation + 1U);
    ARPG_REQUIRE(published.skill_loadout.slots[4U].active
        == skills::ActiveSkillId::draw_slash);

    stable = published;
    ARPG_REQUIRE(session.request_swap_active_skill_slots(1U, 4U)
        == dungeon::RequestResult::accepted);
    ARPG_REQUIRE(same_loadout(session.snapshot().skill_loadout,
        stable.skill_loadout));
    const auto& candidate = session.pending_save_view()->next_state.skill_loadout;
    ARPG_REQUIRE(candidate.slots[1U].active
        == skills::ActiveSkillId::draw_slash);
    ARPG_REQUIRE(candidate.slots[4U].active
        == skills::ActiveSkillId::storm_swords);
    ARPG_REQUIRE(commit_pending_with_store(session, store));
    published = session.snapshot();
    ARPG_REQUIRE(published.commit_generation == stable.commit_generation + 1U);
    ARPG_REQUIRE(published.skill_loadout.slots[1U].active
        == skills::ActiveSkillId::draw_slash);
    ARPG_REQUIRE(published.skill_loadout.slots[4U].active
        == skills::ActiveSkillId::storm_swords);
    return {};
}

arpg::test::Failure remove_changes_only_loadout_and_generation() noexcept {
    ARPG_REQUIRE(isolated_mutation_changes_only_loadout_and_generation(
        IsolatedMutation::remove, 0x51717A11ULL));
    return {};
}

arpg::test::Failure equip_changes_only_loadout_and_generation() noexcept {
    ARPG_REQUIRE(isolated_mutation_changes_only_loadout_and_generation(
        IsolatedMutation::equip, 0x51717A12ULL));
    return {};
}

arpg::test::Failure swap_changes_only_loadout_and_generation() noexcept {
    ARPG_REQUIRE(isolated_mutation_changes_only_loadout_and_generation(
        IsolatedMutation::swap, 0x51717A13ULL));
    return {};
}

arpg::test::Failure failed_store_preserves_stable_bytes_and_reuses_candidate()
    noexcept {
    TempDirectory directory;
    ARPG_REQUIRE(!directory.path.empty());
    auto state = initial_state(0x51717A02ULL);
    std::swap(state.skill_loadout.slots[0U], state.skill_loadout.slots[4U]);
    auto healthy = make_store(directory.path);
    auto seeded = healthy.commit(state);
    ARPG_REQUIRE(seeded.state == persistence::SaveCommitState::committed);
    dungeon::DungeonSession session{{}, std::move(seeded.verified_state)};
    const dungeon::DungeonRunState stable_before =
        arpg::test::stable_state(session);
    const auto visible_before = session.snapshot();

    ARPG_REQUIRE(session.request_remove_active_skill(4U)
        == dungeon::RequestResult::accepted);
    ARPG_REQUIRE(same_loadout(session.snapshot().skill_loadout,
        visible_before.skill_loadout));
    const auto pending = *session.pending_save();
    FaultContext fault{persistence::SaveFaultPoint::before_publish, false};
    auto faulty = make_store(directory.path, &fault);
    auto saved = faulty.commit(pending.next_state);
    ARPG_REQUIRE(saved.state == persistence::SaveCommitState::not_committed);
    ARPG_REQUIRE(fault.triggered);
    session.resolve_pending_save(to_session_result(
        std::move(saved), pending.kind));

    const auto visible_after = session.snapshot();
    ARPG_REQUIRE(dungeon::same_run_state(
        arpg::test::stable_state(session), stable_before));
    ARPG_REQUIRE(visible_after.commit_generation
        == visible_before.commit_generation);
    ARPG_REQUIRE(visible_after.skill_loadout.owned_active_bits
        == visible_before.skill_loadout.owned_active_bits);
    ARPG_REQUIRE(std::memcmp(visible_after.skill_loadout.slots.data(),
        visible_before.skill_loadout.slots.data(),
        sizeof(visible_after.skill_loadout.slots)) == 0);
    ARPG_REQUIRE(!session.pending_save().has_value());

    ARPG_REQUIRE(session.request_remove_active_skill(4U)
        == dungeon::RequestResult::accepted);
    ARPG_REQUIRE(session.pending_save_view()->next_state.skill_loadout
        .slots[4U].active == skills::ActiveSkillId::none);
    ARPG_REQUIRE(commit_pending_with_store(session, healthy));
    return {};
}

arpg::test::Failure invalid_empty_occupied_and_duplicate_requests_reject()
    noexcept {
    dungeon::DungeonSession session{};
    const auto before = session.snapshot();
    ARPG_REQUIRE(session.request_remove_active_skill(5U)
        == dungeon::RequestResult::rejected);
    ARPG_REQUIRE(session.request_remove_active_skill(2U)
        == dungeon::RequestResult::rejected);
    ARPG_REQUIRE(session.request_equip_active_skill(
        skills::ActiveSkillId::none, 2U)
        == dungeon::RequestResult::rejected);
    ARPG_REQUIRE(session.request_equip_active_skill(
        static_cast<skills::ActiveSkillId>(2U), 2U)
        == dungeon::RequestResult::rejected);
    ARPG_REQUIRE(session.request_equip_active_skill(
        skills::ActiveSkillId::draw_slash, 2U)
        == dungeon::RequestResult::rejected);
    ARPG_REQUIRE(session.request_equip_active_skill(
        skills::ActiveSkillId::draw_slash, 1U)
        == dungeon::RequestResult::rejected);
    ARPG_REQUIRE(session.request_swap_active_skill_slots(0U, 0U)
        == dungeon::RequestResult::rejected);
    ARPG_REQUIRE(session.request_swap_active_skill_slots(2U, 3U)
        == dungeon::RequestResult::rejected);
    ARPG_REQUIRE(session.request_swap_active_skill_slots(0U, 5U)
        == dungeon::RequestResult::rejected);
    ARPG_REQUIRE(!session.pending_save().has_value());
    ARPG_REQUIRE(same_loadout(session.snapshot().skill_loadout,
        before.skill_loadout));

    auto unowned = initial_state(0x51717A03ULL);
    unowned.skill_loadout = {};
    unowned.skill_loadout.owned_active_bits = 0x1U;
    unowned.skill_loadout.slots[0U].active =
        skills::ActiveSkillId::draw_slash;
    dungeon::DungeonSession unowned_session{{}, std::move(unowned)};
    ARPG_REQUIRE(unowned_session.request_equip_active_skill(
        skills::ActiveSkillId::storm_swords, 4U)
        == dungeon::RequestResult::rejected);
    ARPG_REQUIRE(!unowned_session.pending_save().has_value());
    return {};
}

arpg::test::Failure pending_commit_death_and_rebuild_states_reject() noexcept {
    dungeon::DungeonSession pending{};
    ARPG_REQUIRE(pending.request_remove_active_skill(0U)
        == dungeon::RequestResult::accepted);
    ARPG_REQUIRE(pending.request_swap_active_skill_slots(0U, 1U)
        == dungeon::RequestResult::rejected);

    dungeon::DungeonSession committing{};
    arpg::test::set_phase(committing, dungeon::RoomPhase::committing);
    ARPG_REQUIRE(committing.request_remove_active_skill(0U)
        == dungeon::RequestResult::rejected);
    ARPG_REQUIRE(!committing.pending_save().has_value());

    dungeon::DungeonSession death{};
    arpg::test::set_phase(death, dungeon::RoomPhase::death_pending);
    ARPG_REQUIRE(death.request_remove_active_skill(0U)
        == dungeon::RequestResult::rejected);
    ARPG_REQUIRE(!death.pending_save().has_value());

    dungeon::DungeonSession rebuilding{};
    arpg::test::set_phase(rebuilding, dungeon::RoomPhase::transitioning);
    ARPG_REQUIRE(rebuilding.request_remove_active_skill(0U)
        == dungeon::RequestResult::rejected);
    ARPG_REQUIRE(!rebuilding.pending_save().has_value());
    return {};
}

arpg::test::Failure committed_receipt_must_match_full_candidate_loadout()
    noexcept {
    dungeon::DungeonSession session{};
    const auto stable = session.snapshot().skill_loadout;
    ARPG_REQUIRE(session.request_remove_active_skill(0U)
        == dungeon::RequestResult::accepted);
    const auto pending = *session.pending_save();
    dungeon::DungeonRunState wrong = pending.next_state;
    wrong.skill_loadout = stable;
    session.resolve_pending_save({dungeon::SaveDisposition::committed,
        pending.expected_generation, std::move(wrong), pending.kind});
    ARPG_REQUIRE(session.snapshot().phase == dungeon::RoomPhase::faulted);
    ARPG_REQUIRE(session.snapshot().diagnostics.fault
        == dungeon::DungeonFault::save_receipt_mismatch);
    ARPG_REQUIRE(same_loadout(session.snapshot().skill_loadout, stable));
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"snapshot starts with value default loadout",
        &snapshot_starts_with_value_owned_default_loadout},
    {"real store remove equip swap publish atomically",
        &real_store_remove_equip_and_swap_publish_atomically},
    {"remove changes only loadout and generation",
        &remove_changes_only_loadout_and_generation},
    {"equip changes only loadout and generation",
        &equip_changes_only_loadout_and_generation},
    {"swap changes only loadout and generation",
        &swap_changes_only_loadout_and_generation},
    {"failed store preserves bytes and reuses candidate",
        &failed_store_preserves_stable_bytes_and_reuses_candidate},
    {"invalid empty occupied duplicate requests reject",
        &invalid_empty_occupied_and_duplicate_requests_reject},
    {"pending transient states reject",
        &pending_commit_death_and_rebuild_states_reject},
    {"committed receipt matches full loadout",
        &committed_receipt_must_match_full_candidate_loadout},
};

}  // namespace

arpg::test::TestSuite dungeon_skill_loadout_transaction_suite() noexcept {
    return arpg::test::make_suite(
        "dungeon_skill_loadout_transaction", kCases);
}
