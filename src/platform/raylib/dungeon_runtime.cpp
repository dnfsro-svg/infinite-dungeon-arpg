#include "dungeon_runtime.hpp"

#include "dungeon/abyss_checkpoint_migration.hpp"
#include "dungeon/room_generation.hpp"
#include "dungeon/room_progress_checkpoint.hpp"
#include "persistence/save_paths.hpp"

#include <algorithm>
#include <chrono>
#include <limits>
#include <thread>
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
    : config_(std::move(config)) {}

DungeonRuntime::~DungeonRuntime() {
    if (save_worker_ != nullptr) save_worker_->stop_and_join();
}

std::optional<std::uint64_t> DungeonRuntime::select_new_run_seed() const noexcept {
    if (config_.new_run_seed.has_value()) {
        return config_.new_run_seed;
    }
    if (config_.seed_provider != nullptr) {
        return config_.seed_provider(config_.seed_context);
    }
    return persistence::system_root_seed();
}

bool DungeonRuntime::repair_pending_death_target(
    dungeon::DungeonRunState& checkpoint) const noexcept {
    if (!config_.continue_pending_death_on_initialize
            || checkpoint.death.lifecycle
                != dungeon::checkpoint::DeathLifecycle::pending_continue) {
        return true;
    }
    if (checkpoint.commit_generation < 2U
            || checkpoint.death_sequence == 0U) {
        return false;
    }

    dungeon::checkpoint::RoomDescriptor death_anchor =
        checkpoint.current_room;
    death_anchor.is_abyss = checkpoint.death.death_was_abyss;
    const dungeon::DeathRetreatTargetResult regenerated =
        dungeon::make_death_retreat_target(
            death_anchor, checkpoint.commit_generation - 1U,
            checkpoint.death_sequence - 1U, checkpoint.death_sequence,
            config_.rules);
    if (regenerated.fault != dungeon::DungeonFault::none) {
        return false;
    }
    checkpoint.death.target_room = regenerated.room;
    return true;
}

bool DungeonRuntime::continue_pending_death_on_initialize() noexcept {
    if (session_ == nullptr
            || session_->phase() == dungeon::RoomPhase::faulted) {
        state_ = DungeonRuntimeState::faulted;
        return false;
    }
    if (!config_.continue_pending_death_on_initialize
            || session_->phase() != dungeon::RoomPhase::death_pending) {
        return true;
    }
    // Startup continuation is an internal recovery step. The restored combat
    // snapshot deliberately requires host-input rearming, so routing this
    // through the external authority gate would reject every valid V9 death.
    if (session_->request_death_continue()
            != dungeon::RequestResult::accepted) {
        state_ = DungeonRuntimeState::faulted;
        return false;
    }
    service_pending_save();
    const auto deadline = std::chrono::steady_clock::now()
        + std::chrono::seconds{10};
    while (exact_flight_.active
            && state() == DungeonRuntimeState::running
            && std::chrono::steady_clock::now() < deadline) {
        poll_save_completion();
        std::this_thread::sleep_for(std::chrono::milliseconds{1});
    }
    if (exact_flight_.active) {
        state_ = DungeonRuntimeState::faulted;
        return false;
    }
    if (state() != DungeonRuntimeState::running
            || session_->phase() != dungeon::RoomPhase::transitioning) {
        state_ = DungeonRuntimeState::faulted;
        return false;
    }
    return true;
}

bool DungeonRuntime::initialize() noexcept {
    if (save_worker_ != nullptr) save_worker_->stop_and_join();
    save_worker_.reset();
    save_storage_.reset();
    session_.reset();
    render_snapshot_.reset();
    status_ = {};
    exact_flight_ = {};
    authority_revision_ = 0U;
    durable_revision_ = 0U;
    fixed_tick_count_ = 0U;
    next_background_tick_ = 300U;
    background_due_ = false;
    background_flight_ = {};
    progress_dirty_ = false;
    gameplay_rearm_required_ = false;
    clean_shutdown_state_ = CleanShutdownState::idle;
    if (dungeon::validate_rules(config_.rules) != dungeon::DungeonFault::none) {
        state_ = DungeonRuntimeState::faulted;
        return false;
    }

    render_snapshot_.reset(
        new (std::nothrow) dungeon::DungeonRenderSnapshot{});
    if (render_snapshot_ == nullptr) {
        state_ = DungeonRuntimeState::faulted;
        return false;
    }

    save_storage_.reset(new (std::nothrow) persistence::SaveCommitStorage{});
    if (save_storage_ == nullptr
            || !save_storage_->initialize(config_.save)) {
        state_ = DungeonRuntimeState::faulted;
        status_.indicator = SaveIndicator::error;
        status_.error = persistence::SaveError::directory_unavailable;
        return false;
    }
    const persistence::SaveLoadState load_state = save_storage_->load_state();
    status_.active_slot = save_storage_->loaded_slot();
    status_.indicator = load_state == persistence::SaveLoadState::ready
            && save_storage_->loaded_recovered()
        ? SaveIndicator::recovered : SaveIndicator::none;
    if (load_state == persistence::SaveLoadState::ready) {
        const dungeon::checkpoint::SaveCheckpointSlot* const loaded =
            save_storage_->loaded_checkpoint();
        if (loaded == nullptr) {
            state_ = DungeonRuntimeState::faulted;
            return false;
        }
        dungeon::DungeonRunState checkpoint{};
        try {
            checkpoint = loaded->state;
        } catch (...) {
            state_ = DungeonRuntimeState::faulted;
            return false;
        }
        durable_revision_ = loaded->persistence_revision;
        authority_revision_ = durable_revision_;
        bool rewrite = false;
        if (save_storage_->loaded_migrated()
                && save_storage_->loaded_format()
                    < persistence::kCheckpointFormatVersion) {
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
            rewrite = true;
        } else if (save_storage_->loaded_migrated()
                && save_storage_->loaded_format()
                    < persistence::kLatestCheckpointFormatVersion) {
            // V9 already contains current durable authority. Advance only
            // the outer revision and rewrite immediately so a zero-tick exit
            // cannot leave an old format active after successful startup.
            rewrite = true;
        } else if (save_storage_->loaded_migrated()) {
            progress_dirty_ = true;
        }
        if (save_storage_->loaded_format()
                    < persistence::kCheckpointFormatVersionV9
                && checkpoint.abyss.lifecycle
                == abyss::AbyssLifecycle::started
                && loaded->room_progress.lifecycle
                    == dungeon::checkpoint::RoomProgressLifecycle::none) {
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
            checkpoint = std::move(failed);
            rewrite = true;
        }
        if (!repair_pending_death_target(checkpoint)) {
            state_ = DungeonRuntimeState::faulted;
            return false;
        }
        std::unique_ptr<dungeon::DungeonSession> candidate{
            new (std::nothrow) dungeon::DungeonSession{
                config_.rules, std::move(checkpoint)}};
        if (candidate == nullptr
                || candidate->phase() == dungeon::RoomPhase::faulted) {
            state_ = DungeonRuntimeState::faulted;
            return false;
        }
        if (loaded->room_progress.lifecycle
                    != dungeon::checkpoint::RoomProgressLifecycle::none
                && !candidate->restore_room_progress_checkpoint(*loaded)) {
            state_ = DungeonRuntimeState::faulted;
            return false;
        }
        session_ = std::move(candidate);
        gameplay_rearm_required_ = loaded->room_progress.lifecycle
            != dungeon::checkpoint::RoomProgressLifecycle::none;
        save_storage_->release_loaded_checkpoints();
        if (!start_worker()) {
            state_ = DungeonRuntimeState::faulted;
            return false;
        }
        state_ = DungeonRuntimeState::running;
        if (rewrite) {
            if (authority_revision_
                    == (std::numeric_limits<std::uint64_t>::max)()) {
                state_ = DungeonRuntimeState::faulted;
                return false;
            }
            ++authority_revision_;
            progress_dirty_ = true;
            if (!commit_initial_checkpoint()) {
                state_ = DungeonRuntimeState::faulted;
                session_.reset();
                return false;
            }
        }
        return continue_pending_death_on_initialize();
    }
    if (load_state == persistence::SaveLoadState::recovery_required) {
        state_ = DungeonRuntimeState::recovery_required;
        status_.indicator = SaveIndicator::error;
        status_.error = persistence::SaveError::read_failed;
        return false;
    }
    if (load_state != persistence::SaveLoadState::empty) {
        state_ = DungeonRuntimeState::faulted;
        status_.indicator = SaveIndicator::error;
        status_.error = persistence::SaveError::read_failed;
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
    session_.reset(new (std::nothrow)
        dungeon::DungeonSession{config_.rules, built.state});
    if (session_ == nullptr) {
        state_ = DungeonRuntimeState::faulted;
        return false;
    }
    save_storage_->release_loaded_checkpoints();
    if (!start_worker()) {
        state_ = DungeonRuntimeState::faulted;
        return false;
    }
    authority_revision_ = 1U;
    progress_dirty_ = true;
    state_ = DungeonRuntimeState::running;
    if (!commit_initial_checkpoint()) {
        state_ = DungeonRuntimeState::faulted;
        session_.reset();
        return false;
    }
    return true;
}

bool DungeonRuntime::start_worker() noexcept {
    if (save_storage_ == nullptr) return false;
    save_worker_.reset(new (std::nothrow)
        persistence::SaveCommitWorker{*save_storage_});
    return save_worker_ != nullptr && save_worker_->start();
}

bool DungeonRuntime::commit_initial_checkpoint() noexcept {
    if (save_worker_ == nullptr || session_ == nullptr
            || authority_revision_ == 0U) {
        status_.error = persistence::SaveError::invalid_checkpoint;
        return false;
    }
    const auto lease = save_worker_->acquire_capture_slot(
        authority_revision_, persistence::SaveCommitRequestKind::exact,
        authority_revision_);
    if (lease.state != persistence::SaveCommitSubmitState::accepted
            && lease.state
                != persistence::SaveCommitSubmitState::superseded_background) {
        status_.error = persistence::SaveError::write_failed;
        return false;
    }
    if (lease.state
            == persistence::SaveCommitSubmitState::superseded_background) {
        background_flight_ = {};
    }
    persistence::SaveCommitJobSlot* const job =
        save_worker_->capture_job(lease);
    session_->clear_buffered_gameplay_input();
    if (job == nullptr || !session_->capture_save_checkpoint(
            job->checkpoint, authority_revision_)) {
        save_worker_->cancel_capture(lease);
        status_.error = persistence::SaveError::invalid_checkpoint;
        return false;
    }
    const persistence::SaveCommitSubmitState submitted =
        save_worker_->submit(lease).state;
    if (submitted != persistence::SaveCommitSubmitState::accepted
            && submitted
                != persistence::SaveCommitSubmitState::superseded_background) {
        save_worker_->cancel_capture(lease);
        status_.error = persistence::SaveError::write_failed;
        return false;
    }
    persistence::SaveCommitCompletion completion{};
    const auto deadline = std::chrono::steady_clock::now()
        + std::chrono::seconds{10};
    while (!save_worker_->try_take_completion(completion)
            && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds{1});
    }
    if (completion.token == 0U) {
        status_.error = persistence::SaveError::write_failed;
        return false;
    }
    status_.active_slot = completion.result.active_slot;
    status_.error = completion.result.error;
    if (completion.result.state != persistence::SaveCommitState::committed) {
        status_.indicator = SaveIndicator::error;
    } else if (status_.indicator != SaveIndicator::recovered) {
        status_.indicator = SaveIndicator::saved;
    }
    if (completion.token != lease.token || completion.epoch != lease.epoch
            || completion.revision != authority_revision_
            || completion.result.state
                != persistence::SaveCommitState::committed) {
        if (status_.error == persistence::SaveError::none) {
            status_.error = persistence::SaveError::invalid_checkpoint;
        }
        return false;
    }
    durable_revision_ = authority_revision_;
    progress_dirty_ = false;
    return true;
}

DungeonRuntimeState DungeonRuntime::state() const noexcept {
    if (session_ != nullptr
        && session_->phase() == dungeon::RoomPhase::faulted) {
        return DungeonRuntimeState::faulted;
    }
    return state_;
}

dungeon::DungeonSession* DungeonRuntime::session() noexcept {
    return session_.get();
}

const dungeon::DungeonSession* DungeonRuntime::session() const noexcept {
    return session_.get();
}

dungeon::DungeonRenderSnapshot*
DungeonRuntime::render_snapshot_storage() noexcept {
    return render_snapshot_.get();
}

const dungeon::DungeonRenderSnapshot*
DungeonRuntime::render_snapshot_storage() const noexcept {
    return render_snapshot_.get();
}

dungeon::RequestResult DungeonRuntime::request_pickup(
    std::uint16_t drop_ordinal) noexcept {
    return authority_requests_enabled()
        ? session_->request_pickup(drop_ordinal)
        : dungeon::RequestResult::rejected;
}

dungeon::RequestResult DungeonRuntime::request_equip(
    std::uint64_t item_id) noexcept {
    return authority_requests_enabled()
        ? session_->request_equip(item_id)
        : dungeon::RequestResult::rejected;
}

dungeon::RequestResult DungeonRuntime::request_unequip(
    items::ItemSlot slot) noexcept {
    return authority_requests_enabled()
        ? session_->request_unequip(slot)
        : dungeon::RequestResult::rejected;
}

dungeon::RequestResult DungeonRuntime::request_craft(
    items::MaterialId material, std::uint64_t item_id,
    std::optional<items::DirectedCategory> directed_category) noexcept {
    return authority_requests_enabled()
        ? session_->request_craft(material, item_id, directed_category)
        : dungeon::RequestResult::rejected;
}

dungeon::RequestResult DungeonRuntime::request_recipe(
    const std::array<std::uint64_t, 3>& item_ids) noexcept {
    return authority_requests_enabled()
        ? session_->request_recipe(item_ids)
        : dungeon::RequestResult::rejected;
}

dungeon::RequestResult DungeonRuntime::request_reinforcement(
    std::uint64_t item_id) noexcept {
    return authority_requests_enabled()
        ? session_->request_reinforcement(item_id)
        : dungeon::RequestResult::rejected;
}

dungeon::RequestResult DungeonRuntime::request_coupon(
    items::MaterialId coupon, std::uint64_t item_id) noexcept {
    return authority_requests_enabled()
        ? session_->request_coupon(coupon, item_id)
        : dungeon::RequestResult::rejected;
}

dungeon::RequestResult DungeonRuntime::request_death_continue() noexcept {
    return authority_requests_enabled()
        ? session_->request_death_continue()
        : dungeon::RequestResult::rejected;
}

const items::ItemOwnershipState* DungeonRuntime::item_state() const noexcept {
    return session_ != nullptr ? &session_->item_state() : nullptr;
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
    if (state() != DungeonRuntimeState::running || session_ == nullptr
            || clean_shutdown_state_ == CleanShutdownState::closing
            || clean_shutdown_state_ == CleanShutdownState::ready) {
        return;
    }
    pump_persistence_frame();
    if (state() != DungeonRuntimeState::running || exact_flight_.active
            || gameplay_rearm_required_
            || session_->pending_save_view() != nullptr
            || session_->phase() == dungeon::RoomPhase::committing) {
        return;
    }
    const bool persists_tick = session_->phase()
        != dungeon::RoomPhase::death_pending;
    session_->tick(movement, pickup_policy);
    if (persists_tick && authority_revision_
            == (std::numeric_limits<std::uint64_t>::max)()) {
        state_ = DungeonRuntimeState::faulted;
        return;
    }
    ++fixed_tick_count_;
    if (persists_tick) {
        ++authority_revision_;
        progress_dirty_ = true;
    }
    if (fixed_tick_count_ >= next_background_tick_) {
        background_due_ = true;
        next_background_tick_ = fixed_tick_count_
                > (std::numeric_limits<std::uint64_t>::max)() - 300U
            ? (std::numeric_limits<std::uint64_t>::max)()
            : fixed_tick_count_ + 300U;
    }
    pump_persistence_frame();
}

bool DungeonRuntime::request_clean_shutdown() noexcept {
    if (state() != DungeonRuntimeState::running || session_ == nullptr
            || clean_shutdown_state_ == CleanShutdownState::ready
            || clean_shutdown_state_ == CleanShutdownState::faulted) {
        return false;
    }
    clean_shutdown_state_ = CleanShutdownState::closing;
    pump_persistence_frame();
    return true;
}

CleanShutdownState DungeonRuntime::clean_shutdown_state() const noexcept {
    return clean_shutdown_state_;
}

bool DungeonRuntime::gameplay_rearm_required() const noexcept {
    return gameplay_rearm_required_;
}

bool DungeonRuntime::authority_requests_enabled() const noexcept {
    return state() == DungeonRuntimeState::running && session_ != nullptr
        && !exact_flight_.active && !gameplay_rearm_required_
        && clean_shutdown_state_ != CleanShutdownState::closing
        && clean_shutdown_state_ != CleanShutdownState::ready;
}

void DungeonRuntime::acknowledge_gameplay_rearmed() noexcept {
    if (!exact_flight_.active
            && clean_shutdown_state_ != CleanShutdownState::closing) {
        gameplay_rearm_required_ = false;
    }
}

void DungeonRuntime::service_pending_save() noexcept {
    pump_persistence_frame();
}

void DungeonRuntime::pump_persistence_frame() noexcept {
    if (session_ == nullptr) return;
    poll_save_completion();
    if (state() != DungeonRuntimeState::running) return;
    if (clean_shutdown_state_ == CleanShutdownState::closing) {
        if (!exact_flight_.active) submit_pending_exact();
        if (!exact_flight_.active
                && session_->pending_save_view() == nullptr) {
            if (!background_flight_.active) {
                submit_shutdown_exact();
            }
        }
        poll_save_completion();
        return;
    }
    submit_pending_exact();
    poll_save_completion();
    submit_background_if_due();
}

void DungeonRuntime::fault_persistence_runtime(
    persistence::SaveError error) noexcept {
    status_.indicator = SaveIndicator::error;
    status_.error = error == persistence::SaveError::none
        ? persistence::SaveError::invalid_checkpoint : error;
    state_ = DungeonRuntimeState::faulted;
    if (clean_shutdown_state_ == CleanShutdownState::closing) {
        clean_shutdown_state_ = CleanShutdownState::faulted;
    }
}

void DungeonRuntime::submit_pending_exact() noexcept {
    if (exact_flight_.active || save_worker_ == nullptr
            || session_ == nullptr) return;
    const dungeon::PendingSave* const pending = session_->pending_save_view();
    if (pending == nullptr) return;
    if (authority_revision_
            == (std::numeric_limits<std::uint64_t>::max)()) {
        fault_persistence_runtime(persistence::SaveError::invalid_checkpoint);
        return;
    }
    const std::uint64_t revision = authority_revision_ + 1U;
    const std::uint64_t intent =
        (pending->expected_generation << 8U)
        ^ static_cast<std::uint64_t>(pending->kind);
    const auto lease = save_worker_->acquire_capture_slot(revision,
        persistence::SaveCommitRequestKind::exact, intent);
    if (lease.state == persistence::SaveCommitSubmitState::busy
            || lease.state == persistence::SaveCommitSubmitState::stopped) {
        if (lease.state == persistence::SaveCommitSubmitState::stopped) {
            fault_persistence_runtime(persistence::SaveError::write_failed);
        }
        return;
    }
    if (lease.state
            == persistence::SaveCommitSubmitState::superseded_background) {
        background_flight_ = {};
    }
    persistence::SaveCommitJobSlot* const job =
        save_worker_->capture_job(lease);
    session_->clear_buffered_gameplay_input();
    if (job == nullptr || !session_->capture_save_checkpoint(
            job->checkpoint, revision, &pending->next_state)) {
        save_worker_->cancel_capture(lease);
        fault_persistence_runtime(persistence::SaveError::invalid_checkpoint);
        return;
    }
    const auto candidate = capture_pickup_candidate(*session_, *pending);
    const persistence::SaveCommitSubmission submitted =
        save_worker_->submit(lease);
    if (submitted.state == persistence::SaveCommitSubmitState::busy
            || submitted.state == persistence::SaveCommitSubmitState::stopped) {
        save_worker_->cancel_capture(lease);
        if (submitted.state == persistence::SaveCommitSubmitState::stopped) {
            fault_persistence_runtime(persistence::SaveError::write_failed);
        }
        return;
    }
    authority_revision_ = revision;
    exact_flight_.active = true;
    exact_flight_.revision = revision;
    exact_flight_.intent = intent;
    exact_flight_.token = lease.token;
    exact_flight_.epoch = lease.epoch;
    exact_flight_.expected_generation = pending->expected_generation;
    exact_flight_.kind = pending->kind;
    exact_flight_.pickup = candidate.has_value()
        ? std::optional<LootPickupReceipt>{candidate->receipt} : std::nullopt;
    exact_flight_.pickup_ordinal = candidate.has_value()
        ? candidate->ordinal : 0xFFFFU;
    exact_flight_.shutdown = false;
    gameplay_rearm_required_ = true;
    status_.indicator = SaveIndicator::saving;
}

void DungeonRuntime::submit_shutdown_exact() noexcept {
    if (clean_shutdown_state_ != CleanShutdownState::closing
            || exact_flight_.active || session_ == nullptr
            || save_worker_ == nullptr) return;
    const std::uint64_t revision = authority_revision_;
    if (revision == 0U) {
        fault_persistence_runtime(persistence::SaveError::invalid_checkpoint);
        return;
    }
    constexpr std::uint64_t kShutdownIntent =
        (std::numeric_limits<std::uint64_t>::max)();
    const bool verify_durable = durable_revision_ == authority_revision_
        && !progress_dirty_;
    const auto lease = save_worker_->acquire_capture_slot(revision,
        persistence::SaveCommitRequestKind::exact, kShutdownIntent,
        verify_durable
            ? persistence::SaveCommitPayloadKind::verify_durable
            : persistence::SaveCommitPayloadKind::captured_checkpoint);
    if (lease.state == persistence::SaveCommitSubmitState::stopped) {
        fault_persistence_runtime(persistence::SaveError::write_failed);
        return;
    }
    if (lease.state == persistence::SaveCommitSubmitState::busy) {
        return;
    }
    if (lease.state
            == persistence::SaveCommitSubmitState::superseded_background) {
        background_flight_ = {};
    }
    session_->clear_buffered_gameplay_input();
    if (!verify_durable) {
        persistence::SaveCommitJobSlot* const job =
            save_worker_->capture_job(lease);
        if (job == nullptr || !session_->capture_save_checkpoint(
                job->checkpoint, revision)) {
            save_worker_->cancel_capture(lease);
            fault_persistence_runtime(
                persistence::SaveError::invalid_checkpoint);
            return;
        }
    }
    const persistence::SaveCommitSubmitState submitted =
        save_worker_->submit(lease).state;
    if (submitted == persistence::SaveCommitSubmitState::stopped) {
        save_worker_->cancel_capture(lease);
        fault_persistence_runtime(persistence::SaveError::write_failed);
        return;
    }
    if (submitted == persistence::SaveCommitSubmitState::busy) {
        save_worker_->cancel_capture(lease);
        return;
    }
    exact_flight_ = {};
    exact_flight_.active = true;
    exact_flight_.revision = revision;
    exact_flight_.intent = kShutdownIntent;
    exact_flight_.token = lease.token;
    exact_flight_.epoch = lease.epoch;
    exact_flight_.shutdown = true;
    gameplay_rearm_required_ = true;
    status_.indicator = SaveIndicator::saving;
}

void DungeonRuntime::poll_save_completion() noexcept {
    if (save_worker_ == nullptr || session_ == nullptr) return;
    persistence::SaveCommitCompletion completion{};
    if (!save_worker_->try_take_completion(completion)) return;
    apply_save_completion(completion);
}

void DungeonRuntime::apply_save_completion(
    const persistence::SaveCommitCompletion& completion) noexcept {
    if (session_ == nullptr) return;
    if (completion.kind == persistence::SaveCommitRequestKind::background) {
        const bool matches = background_flight_.active
            && completion.revision == background_flight_.revision
            && completion.token == background_flight_.token
            && completion.epoch == background_flight_.epoch;
        if (!matches) return;
        background_flight_ = {};
        status_.active_slot = completion.result.active_slot;
        status_.error = completion.result.error;
        if (completion.result.state
                == persistence::SaveCommitState::committed) {
            durable_revision_ = (std::max)(
                durable_revision_, completion.revision);
            progress_dirty_ = authority_revision_ > durable_revision_;
            status_.indicator = SaveIndicator::saved;
        } else if (completion.result.state
                == persistence::SaveCommitState::indeterminate) {
            fault_persistence_runtime(completion.result.error);
        }
        return;
    }
    const bool receipt_matches = exact_flight_.active
        && completion.revision == exact_flight_.revision
        && completion.intent == exact_flight_.intent
        && completion.token == exact_flight_.token
        && completion.epoch == exact_flight_.epoch;
    if (!receipt_matches) return;
    if (exact_flight_.recovery_initial) {
        status_.active_slot = completion.result.active_slot;
        status_.error = completion.result.error;
        const std::uint64_t committed_revision = exact_flight_.revision;
        exact_flight_ = {};
        if (completion.result.state
                == persistence::SaveCommitState::committed) {
            durable_revision_ = committed_revision;
            progress_dirty_ = authority_revision_ > durable_revision_;
            status_.indicator = SaveIndicator::saved;
            state_ = DungeonRuntimeState::running;
        } else {
            fault_persistence_runtime(completion.result.error);
        }
        return;
    }
    if (exact_flight_.shutdown) {
        status_.active_slot = completion.result.active_slot;
        status_.error = completion.result.error;
        exact_flight_ = {};
        if (completion.result.state
                == persistence::SaveCommitState::committed) {
            durable_revision_ = completion.revision;
            progress_dirty_ = authority_revision_ > durable_revision_;
            status_.indicator = SaveIndicator::saved;
            clean_shutdown_state_ = CleanShutdownState::ready;
        } else if (completion.result.state
                == persistence::SaveCommitState::not_committed) {
            status_.indicator = SaveIndicator::error;
            clean_shutdown_state_ = CleanShutdownState::canceled;
        } else {
            status_.indicator = SaveIndicator::error;
            clean_shutdown_state_ = CleanShutdownState::faulted;
            state_ = DungeonRuntimeState::faulted;
        }
        return;
    }
    const dungeon::PendingSave* const pending = session_->pending_save_view();
    const bool matches = pending != nullptr
        && pending->kind == exact_flight_.kind
        && pending->expected_generation == exact_flight_.expected_generation;
    if (!matches) {
        exact_flight_ = {};
        fault_persistence_runtime(persistence::SaveError::invalid_checkpoint);
        return;
    }
    dungeon::PendingSaveResult resolved{};
    resolved.kind = exact_flight_.kind;
    resolved.generation = exact_flight_.expected_generation;
    if (completion.result.state
            == persistence::SaveCommitState::committed) {
        resolved.disposition = dungeon::SaveDisposition::committed;
        try {
            resolved.verified_state = pending->next_state;
        } catch (...) {
            resolved.disposition = dungeon::SaveDisposition::indeterminate;
        }
    } else if (completion.result.state
            == persistence::SaveCommitState::not_committed) {
        resolved.disposition = dungeon::SaveDisposition::not_committed;
    } else {
        resolved.disposition = dungeon::SaveDisposition::indeterminate;
    }
    status_.active_slot = completion.result.active_slot;
    status_.error = completion.result.error;
    status_.indicator = resolved.disposition == dungeon::SaveDisposition::committed
        ? SaveIndicator::saved : SaveIndicator::error;
    session_->resolve_pending_save(resolved);
    if (resolved.disposition == dungeon::SaveDisposition::committed) {
        durable_revision_ = completion.revision;
        progress_dirty_ = authority_revision_ > durable_revision_;
        if (exact_flight_.pickup.has_value()) {
            PickupCandidate candidate{*exact_flight_.pickup,
                exact_flight_.pickup_ordinal};
            if (pickup_was_committed(*session_, candidate)) {
                status_.loot_pickup = *exact_flight_.pickup;
            }
        }
    }
    exact_flight_ = {};
    if (resolved.disposition == dungeon::SaveDisposition::indeterminate
            || session_->phase() == dungeon::RoomPhase::faulted) {
        fault_persistence_runtime(status_.error);
    }
}

void DungeonRuntime::submit_background_if_due() noexcept {
    if (!progress_dirty_ || !background_due_
            || background_flight_.active
            || exact_flight_.active || session_->pending_save_view() != nullptr
            || save_worker_ == nullptr) return;
    const auto lease = save_worker_->acquire_capture_slot(authority_revision_,
        persistence::SaveCommitRequestKind::background);
    if (lease.state != persistence::SaveCommitSubmitState::accepted
            && lease.state
                != persistence::SaveCommitSubmitState::superseded_background) {
        return;
    }
    persistence::SaveCommitJobSlot* const job =
        save_worker_->capture_job(lease);
    if (job == nullptr || !session_->capture_save_checkpoint(
            job->checkpoint, authority_revision_)) {
        save_worker_->cancel_capture(lease);
        return;
    }
    const persistence::SaveCommitSubmitState submitted =
        save_worker_->submit(lease).state;
    if (submitted != persistence::SaveCommitSubmitState::accepted
            && submitted
                != persistence::SaveCommitSubmitState::superseded_background) {
        save_worker_->cancel_capture(lease);
        return;
    }
    background_flight_.active = true;
    background_flight_.revision = authority_revision_;
    background_flight_.token = lease.token;
    background_flight_.epoch = lease.epoch;
    background_due_ = false;
}

bool DungeonRuntime::recover_with_new_run() noexcept {
    if (state_ != DungeonRuntimeState::recovery_required
            || save_storage_ == nullptr || save_worker_ != nullptr) {
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
    if (!save_storage_->archive_invalid_files()) {
        status_.indicator = SaveIndicator::error;
        status_.error = persistence::SaveError::archive_failed;
        state_ = DungeonRuntimeState::faulted;
        return false;
    }
    save_storage_->release_loaded_checkpoints();
    session_.reset(new (std::nothrow)
        dungeon::DungeonSession{config_.rules, built.state});
    if (session_ == nullptr || !start_worker()) {
        session_.reset();
        state_ = DungeonRuntimeState::faulted;
        return false;
    }
    authority_revision_ = 1U;
    durable_revision_ = 0U;
    progress_dirty_ = true;
    exact_flight_ = {};
    background_flight_ = {};
    constexpr std::uint64_t kRecoveryInitialIntent =
        (std::numeric_limits<std::uint64_t>::max)() - 1U;
    const auto lease = save_worker_->acquire_capture_slot(
        authority_revision_, persistence::SaveCommitRequestKind::exact,
        kRecoveryInitialIntent);
    if (lease.state != persistence::SaveCommitSubmitState::accepted
            && lease.state
                != persistence::SaveCommitSubmitState::superseded_background) {
        fault_persistence_runtime(persistence::SaveError::write_failed);
        return false;
    }
    persistence::SaveCommitJobSlot* const job =
        save_worker_->capture_job(lease);
    if (job == nullptr || !session_->capture_save_checkpoint(
            job->checkpoint, authority_revision_)) {
        save_worker_->cancel_capture(lease);
        fault_persistence_runtime(persistence::SaveError::invalid_checkpoint);
        return false;
    }
    const persistence::SaveCommitSubmitState submitted =
        save_worker_->submit(lease).state;
    if (submitted != persistence::SaveCommitSubmitState::accepted
            && submitted
                != persistence::SaveCommitSubmitState::superseded_background) {
        save_worker_->cancel_capture(lease);
        fault_persistence_runtime(persistence::SaveError::write_failed);
        return false;
    }
    exact_flight_.active = true;
    exact_flight_.revision = authority_revision_;
    exact_flight_.intent = kRecoveryInitialIntent;
    exact_flight_.token = lease.token;
    exact_flight_.epoch = lease.epoch;
    exact_flight_.recovery_initial = true;
    gameplay_rearm_required_ = true;
    status_.indicator = SaveIndicator::saving;
    return true;
}

}  // namespace arpg::platform
