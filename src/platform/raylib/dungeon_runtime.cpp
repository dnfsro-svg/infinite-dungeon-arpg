#include "dungeon_runtime.hpp"

#include "persistence/save_paths.hpp"

#include <utility>

namespace arpg::platform {

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
        std::move(saved.verified_state)};
}

bool DungeonRuntime::initialize() noexcept {
    session_.reset();
    status_ = {};
    if (dungeon::validate_rules(config_.rules) != dungeon::DungeonFault::none) {
        state_ = DungeonRuntimeState::faulted;
        return false;
    }

    const persistence::SaveLoadResult loaded = store_.load();
    sync_load_status(loaded);
    if (loaded.state == persistence::SaveLoadState::ready) {
        session_.emplace(config_.rules, loaded.checkpoint);
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

dungeon::RequestResult DungeonRuntime::request_recipe(
    const std::array<std::uint64_t, 3>& item_ids) noexcept {
    return state() == DungeonRuntimeState::running && session_.has_value()
        ? session_->request_recipe(item_ids)
        : dungeon::RequestResult::rejected;
}

const items::ItemOwnershipState* DungeonRuntime::item_state() const noexcept {
    return session_.has_value() ? &session_->item_state() : nullptr;
}

DungeonRenderStatus DungeonRuntime::render_status() const noexcept {
    return status_;
}

void DungeonRuntime::service_pending_save() noexcept {
    if (state() != DungeonRuntimeState::running || !session_.has_value()) {
        return;
    }
    const dungeon::PendingSave* const pending = session_->pending_save_view();
    if (pending == nullptr) {
        return;
    }
    status_.indicator = SaveIndicator::saving;
    persistence::SaveCommitResult saved = store_.commit(pending->next_state);
    sync_commit_status(saved);
    dungeon::PendingSaveResult result = to_session_result(std::move(saved));
    session_->resolve_pending_save(result);
    if (session_->snapshot().phase == dungeon::RoomPhase::faulted) {
        state_ = DungeonRuntimeState::faulted;
    }
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
