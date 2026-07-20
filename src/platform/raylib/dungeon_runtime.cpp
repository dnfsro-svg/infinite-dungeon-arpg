#include "dungeon_runtime.hpp"

#include "dungeon/abyss_checkpoint_migration.hpp"
#include "persistence/save_paths.hpp"

#include <limits>
#include <utility>

namespace arpg::platform {
namespace {

[[nodiscard]] bool pickup_save_kind(
    dungeon::PendingSaveKind kind) noexcept {
    return kind == dungeon::PendingSaveKind::loot_pickup
        || kind == dungeon::PendingSaveKind::abyss_reward_claim;
}

struct PickupCandidate final {
    LootPickupReceipt receipt{};
    std::uint16_t ordinal{};
};

[[nodiscard]] std::optional<PickupCandidate> pickup_candidate(
    const dungeon::DungeonSnapshot& snapshot,
    dungeon::PendingSaveKind kind,
    std::uint16_t ordinal,
    std::uint64_t expected_generation) noexcept {
    if (!pickup_save_kind(kind) || !snapshot.pending_save_kind.has_value()
            || *snapshot.pending_save_kind != kind
            || !snapshot.pending_pickup_ordinal.has_value()
            || *snapshot.pending_pickup_ordinal != ordinal
            || expected_generation <= snapshot.commit_generation) {
        return std::nullopt;
    }
    for (std::size_t index = 0U; index < snapshot.ground_item_count
            && index < snapshot.ground_items.size(); ++index) {
        const dungeon::GroundItemSnapshot& item = snapshot.ground_items[index];
        if (item.ordinal != ordinal || item.item_id == 0U) continue;
        const bool source_matches =
            (kind == dungeon::PendingSaveKind::loot_pickup
                && item.source == dungeon::GroundItemSource::monster_drop)
            || (kind == dungeon::PendingSaveKind::abyss_reward_claim
                && item.source == dungeon::GroundItemSource::abyss_chest);
        if (!source_matches) return std::nullopt;
        return PickupCandidate{{true, expected_generation, item.item_id,
            item.base_id, item.item_level, item.rarity, item.source}, ordinal};
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<PickupCandidate> capture_pickup_candidate(
    const dungeon::DungeonSession& session,
    const dungeon::PendingSave& pending) noexcept {
    const dungeon::DungeonSnapshot snapshot = session.snapshot();
    return pickup_candidate(snapshot, pending.kind, pending.pickup_ordinal,
        pending.expected_generation);
}

[[nodiscard]] bool ground_contains_item(
    const dungeon::DungeonSnapshot& snapshot,
    std::uint64_t item_id) noexcept {
    for (std::size_t index = 0U; index < snapshot.ground_item_count
            && index < snapshot.ground_items.size(); ++index) {
        if (snapshot.ground_items[index].item_id == item_id) return true;
    }
    return false;
}

[[nodiscard]] bool ground_contains_ordinal(
    const dungeon::DungeonSnapshot& snapshot,
    std::uint16_t ordinal) noexcept {
    for (std::size_t index = 0U; index < snapshot.ground_item_count
            && index < snapshot.ground_items.size(); ++index) {
        if (snapshot.ground_items[index].ordinal == ordinal) return true;
    }
    return false;
}

[[nodiscard]] bool pickup_was_committed(
    const dungeon::DungeonSession& session,
    const PickupCandidate& candidate) noexcept {
    const dungeon::DungeonSnapshot snapshot = session.snapshot();
    return snapshot.commit_generation == candidate.receipt.commit_generation
        && !ground_contains_item(snapshot, candidate.receipt.item_id)
        && !ground_contains_ordinal(snapshot, candidate.ordinal);
}

}  // namespace

DungeonRuntime::DungeonRuntime(DungeonRuntimeConfig config)
    : config_(std::move(config)), store_(config_.save) {}

std::optional<std::uint64_t> DungeonRuntime::select_new_run_seed() const noexcept {
    if (config_.new_run_seed.has_value()) {
        return config_.new_run_seed;
    }
    if (config_.seed_provider != nullptr) {
        return config_.seed_provider(config_.seed_context);
    }
    return persistence::system_root_seed();
}

void DungeonRuntime::sync_load_status(
    const persistence::SaveLoadResult& result) noexcept {
    status_.active_slot = result.active_slot;
    status_.error = result.error;
    if (result.state == persistence::SaveLoadState::ready && result.recovered) {
        status_.indicator = SaveIndicator::recovered;
    } else if (result.state == persistence::SaveLoadState::ready) {
        status_.indicator = SaveIndicator::none;
    } else if (result.state == persistence::SaveLoadState::empty) {
        status_.indicator = SaveIndicator::none;
    } else {
        status_.indicator = SaveIndicator::error;
    }
}

void DungeonRuntime::sync_commit_status(
    const persistence::SaveCommitResult& result) noexcept {
    status_.active_slot = result.active_slot;
    status_.error = result.error;
    status_.indicator = result.state == persistence::SaveCommitState::committed
        ? SaveIndicator::saved : SaveIndicator::error;
}

dungeon::PendingSaveResult DungeonRuntime::to_session_result(
    persistence::SaveCommitResult&& saved) noexcept {
    dungeon::SaveDisposition disposition = dungeon::SaveDisposition::indeterminate;
    if (saved.state == persistence::SaveCommitState::committed) {
        disposition = dungeon::SaveDisposition::committed;
    } else if (saved.state == persistence::SaveCommitState::not_committed) {
        disposition = dungeon::SaveDisposition::not_committed;
    }
    return {disposition, saved.verified_state.commit_generation,
        std::move(saved.verified_state), std::nullopt};
}

bool DungeonRuntime::initialize() noexcept {
    session_.reset();
    status_ = {};
    if (dungeon::validate_rules(config_.rules) != dungeon::DungeonFault::none) {
        state_ = DungeonRuntimeState::faulted;
        return false;
    }

    persistence::SaveLoadResult loaded = store_.load();
    sync_load_status(loaded);
    if (loaded.state == persistence::SaveLoadState::ready) {
        dungeon::DungeonRunState checkpoint = std::move(loaded.checkpoint);
        if (loaded.migrated) {
            if (checkpoint.commit_generation
                    == (std::numeric_limits<std::uint64_t>::max)()) {
                state_ = DungeonRuntimeState::faulted;
                return false;
            }
            try {
                checkpoint = dungeon::migrate_legacy_abyss_checkpoint(
                    checkpoint);
            } catch (...) {
                state_ = DungeonRuntimeState::faulted;
                return false;
            }
            ++checkpoint.commit_generation;
            persistence::SaveCommitResult migrated =
                store_.commit(checkpoint);
            sync_commit_status(migrated);
            if (migrated.state != persistence::SaveCommitState::committed
                    || migrated.verified_state.commit_generation
                        != checkpoint.commit_generation
                    || !dungeon::same_run_state(
                        migrated.verified_state, checkpoint)) {
                state_ = DungeonRuntimeState::faulted;
                return false;
            }
            checkpoint = std::move(migrated.verified_state);
        }
        if (checkpoint.abyss.lifecycle
                == abyss::AbyssLifecycle::started) {
            if (!checkpoint.current_room.is_abyss
                    || checkpoint.commit_generation
                        == (std::numeric_limits<std::uint64_t>::max)()) {
                state_ = DungeonRuntimeState::faulted;
                return false;
            }
            dungeon::DungeonRunState failed = std::move(checkpoint);
            ++failed.commit_generation;
            failed.current_room.is_abyss = false;
            failed.abyss.lifecycle = abyss::AbyssLifecycle::failed;
            persistence::SaveCommitResult published =
                store_.commit(failed);
            sync_commit_status(published);
            if (published.state != persistence::SaveCommitState::committed
                    || published.verified_state.commit_generation
                        != failed.commit_generation
                    || !dungeon::same_run_state(
                        published.verified_state, failed)) {
                state_ = DungeonRuntimeState::faulted;
                return false;
            }
            checkpoint = std::move(published.verified_state);
        }
        session_.emplace(config_.rules, std::move(checkpoint));
        state_ = DungeonRuntimeState::running;
        return true;
    }
    if (loaded.state == persistence::SaveLoadState::recovery_required) {
        state_ = DungeonRuntimeState::recovery_required;
        return false;
    }
    if (loaded.state != persistence::SaveLoadState::empty) {
        state_ = DungeonRuntimeState::faulted;
        return false;
    }

    const auto root_seed = select_new_run_seed();
    if (!root_seed.has_value()) {
        state_ = DungeonRuntimeState::faulted;
        return false;
    }
    const dungeon::RunStateBuildResult built =
        dungeon::make_initial_run_state(*root_seed, config_.rules);
    if (built.fault != dungeon::DungeonFault::none) {
        state_ = DungeonRuntimeState::faulted;
        return false;
    }
    const persistence::SaveCommitResult committed = store_.commit(built.state);
    sync_commit_status(committed);
    if (committed.state != persistence::SaveCommitState::committed) {
        state_ = DungeonRuntimeState::faulted;
        return false;
    }
    session_.emplace(config_.rules, committed.verified_state);
    state_ = DungeonRuntimeState::running;
    return true;
}

DungeonRuntimeState DungeonRuntime::state() const noexcept {
    if (session_.has_value()
        && session_->snapshot().phase == dungeon::RoomPhase::faulted) {
        return DungeonRuntimeState::faulted;
    }
    return state_;
}

dungeon::DungeonSession* DungeonRuntime::session() noexcept {
    return session_.has_value() ? &*session_ : nullptr;
}

const dungeon::DungeonSession* DungeonRuntime::session() const noexcept {
    return session_.has_value() ? &*session_ : nullptr;
}

dungeon::RequestResult DungeonRuntime::request_pickup(
    std::uint16_t drop_ordinal) noexcept {
    return state() == DungeonRuntimeState::running && session_.has_value()
        ? session_->request_pickup(drop_ordinal)
        : dungeon::RequestResult::rejected;
}

dungeon::RequestResult DungeonRuntime::request_equip(
    std::uint64_t item_id) noexcept {
    return state() == DungeonRuntimeState::running && session_.has_value()
        ? session_->request_equip(item_id)
        : dungeon::RequestResult::rejected;
}

dungeon::RequestResult DungeonRuntime::request_unequip(
    items::ItemSlot slot) noexcept {
    return state() == DungeonRuntimeState::running && session_.has_value()
        ? session_->request_unequip(slot)
        : dungeon::RequestResult::rejected;
}

dungeon::RequestResult DungeonRuntime::request_craft(
    items::MaterialId material, std::uint64_t item_id,
    std::optional<items::DirectedCategory> directed_category) noexcept {
    return state() == DungeonRuntimeState::running && session_.has_value()
        ? session_->request_craft(material, item_id, directed_category)
        : dungeon::RequestResult::rejected;
}

dungeon::RequestResult DungeonRuntime::request_recipe(
    const std::array<std::uint64_t, 3>& item_ids) noexcept {
    return state() == DungeonRuntimeState::running && session_.has_value()
        ? session_->request_recipe(item_ids)
        : dungeon::RequestResult::rejected;
}

dungeon::RequestResult DungeonRuntime::request_death_continue() noexcept {
    return state() == DungeonRuntimeState::running && session_.has_value()
        ? session_->request_death_continue()
        : dungeon::RequestResult::rejected;
}

const items::ItemOwnershipState* DungeonRuntime::item_state() const noexcept {
    return session_.has_value() ? &session_->item_state() : nullptr;
}

DungeonRenderStatus DungeonRuntime::render_status() const noexcept {
    DungeonRenderStatus status = status_;
    const DungeonRuntimeState runtime_state = state();
    status.recovery_required =
        runtime_state == DungeonRuntimeState::recovery_required;
    status.faulted = runtime_state == DungeonRuntimeState::faulted;
    return status;
}

void DungeonRuntime::fixed_tick(combat::MovementInput movement,
    dungeon::AutoPickupPolicy pickup_policy) noexcept {
    if (state() != DungeonRuntimeState::running || !session_.has_value()) {
        return;
    }
    session_->tick(movement, pickup_policy);
    service_pending_save();
}

void DungeonRuntime::service_pending_save() noexcept {
    if (state() != DungeonRuntimeState::running || !session_.has_value()) {
        return;
    }
    const dungeon::PendingSave* const pending = session_->pending_save_view();
    if (pending == nullptr) {
        return;
    }
    const dungeon::PendingSaveKind pending_kind = pending->kind;
    const std::uint64_t expected_generation = pending->expected_generation;
    const auto candidate = capture_pickup_candidate(*session_, *pending);
    const bool committed_exact = commit_and_resolve_pending(
        *pending, pending_kind, expected_generation);
    if (state() == DungeonRuntimeState::faulted) {
        state_ = DungeonRuntimeState::faulted;
    }
    if (candidate.has_value() && committed_exact
            && state_ == DungeonRuntimeState::running
            && pickup_was_committed(*session_, *candidate)) {
        status_.loot_pickup = candidate->receipt;
    }
}

bool DungeonRuntime::commit_and_resolve_pending(
    const dungeon::PendingSave& pending,
    dungeon::PendingSaveKind pending_kind,
    std::uint64_t expected_generation) noexcept {
    const bool pending_state_matches = expected_generation
        == pending.next_state.commit_generation;
    status_.indicator = SaveIndicator::saving;
    persistence::SaveCommitResult saved = store_.commit(pending.next_state);
    const bool committed_exact = saved.state
            == persistence::SaveCommitState::committed
        && saved.verified_state.commit_generation == expected_generation
        && pending_state_matches
        && dungeon::same_run_state(saved.verified_state, pending.next_state);
    sync_commit_status(saved);
    dungeon::PendingSaveResult result = to_session_result(std::move(saved));
    result.kind = pending_kind;
    session_->resolve_pending_save(result);
    return committed_exact;
}

void DungeonRuntime::service_pending_transition() noexcept {
    service_pending_save();
}

bool DungeonRuntime::recover_with_new_run() noexcept {
    if (state_ != DungeonRuntimeState::recovery_required) {
        return false;
    }
    const auto root_seed = select_new_run_seed();
    if (!root_seed.has_value()) {
        state_ = DungeonRuntimeState::faulted;
        return false;
    }
    const dungeon::RunStateBuildResult built =
        dungeon::make_initial_run_state(*root_seed, config_.rules);
    if (built.fault != dungeon::DungeonFault::none) {
        state_ = DungeonRuntimeState::faulted;
        return false;
    }
    const persistence::SaveLoadResult created =
        store_.archive_invalid_and_create(built.state);
    sync_load_status(created);
    if (created.state != persistence::SaveLoadState::ready
        || created.checkpoint.commit_generation != 1U) {
        state_ = DungeonRuntimeState::faulted;
        return false;
    }
    session_.emplace(config_.rules, created.checkpoint);
    status_.indicator = SaveIndicator::saved;
    state_ = DungeonRuntimeState::running;
    return true;
}

}  // namespace arpg::platform
