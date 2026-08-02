#include "abyss/abyss_rules.hpp"
#include "checkpoint/room_checkpoint_validation.hpp"
#include "checkpoint/room_progress_checkpoint.hpp"
#include "dungeon/dungeon_progression.hpp"
#include "dungeon/dungeon_session.hpp"
#include "dungeon/room_generation.hpp"
#include "persistence/room_progress_codec.hpp"
#include "persistence/save_commit_worker.hpp"

#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <new>
#include <optional>
#include <thread>
#include <vector>

namespace {

namespace dungeon = arpg::dungeon;
namespace persistence = arpg::persistence;
namespace checkpoint = arpg::checkpoint;

struct PathResult final {
    dungeon::checkpoint::DeathCheckpoint death{};
    dungeon::checkpoint::DungeonRunState continued{};
};

std::filesystem::path fresh_directory(const std::filesystem::path& root,
    const char* name) {
    const auto directory = root / name;
    std::error_code error;
    std::filesystem::remove_all(directory, error);
    if (error) return {};
    std::filesystem::create_directories(directory, error);
    return error ? std::filesystem::path{} : directory;
}

bool wait_completion(persistence::SaveCommitWorker& worker,
    persistence::SaveCommitCompletion& completion) noexcept {
    const auto deadline = std::chrono::steady_clock::now()
        + std::chrono::seconds{10};
    while (std::chrono::steady_clock::now() < deadline) {
        if (worker.try_take_completion(completion)) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds{1});
    }
    return false;
}

struct CommitReceipt final {
    std::uint64_t persistence_revision{};
    std::uint64_t commit_generation{};
    persistence::SaveSlot active_slot{persistence::SaveSlot::none};
};

[[nodiscard]] persistence::SaveSlot slot_for_revision(
    const std::uint64_t revision) noexcept {
    return (revision & 1U) != 0U
        ? persistence::SaveSlot::a : persistence::SaveSlot::b;
}

class V9FixtureStore final {
public:
    ~V9FixtureStore() { stop_and_destroy_transaction(); }

    bool initialize(const std::filesystem::path& directory) {
        directory_ = directory;
        storage_.reset(new (std::nothrow) persistence::SaveCommitStorage{});
        if (storage_ == nullptr || !storage_->initialize({directory_})
                || storage_->load_state()
                    != persistence::SaveLoadState::empty) {
            return false;
        }
        return start_worker();
    }

    std::optional<CommitReceipt> commit(
        const dungeon::DungeonSession& session,
        const dungeon::checkpoint::DungeonRunState& state) {
        if (worker_ == nullptr || storage_ == nullptr) return {};
        const std::uint64_t revision = next_revision_;
        const std::uint64_t intent = state.commit_generation;
        const persistence::SaveSlot expected_active =
            slot_for_revision(revision);
        const auto lease = worker_->acquire_capture_slot(
            revision, persistence::SaveCommitRequestKind::exact, intent);
        if (lease.state != persistence::SaveCommitSubmitState::accepted) {
            return {};
        }
        if (lease.job_slot >= 2U || lease.revision != revision
                || lease.intent != intent || lease.token == 0U
                || lease.epoch == 0U
                || lease.kind
                    != persistence::SaveCommitRequestKind::exact) {
            worker_->cancel_capture(lease);
            return {};
        }
        std::unique_ptr<checkpoint::SaveCheckpointSlot> expected{
            new (std::nothrow) checkpoint::SaveCheckpointSlot{}};
        auto* const job = worker_->capture_job(lease);
        if (expected == nullptr || job == nullptr
                || !session.capture_save_checkpoint(
                    job->checkpoint, revision, &state)
                || !session.capture_save_checkpoint(
                    *expected, revision, &state)
                || job->checkpoint.persistence_revision != revision
                || expected->persistence_revision != revision
                || !dungeon::same_run_state(
                    job->checkpoint.state, expected->state)
                || !checkpoint::same_room_progress_checkpoint(
                    job->checkpoint.room_progress,
                    expected->room_progress)) {
            worker_->cancel_capture(lease);
            return {};
        }
        if (worker_->submit(lease).state
                != persistence::SaveCommitSubmitState::accepted) {
            worker_->cancel_capture(lease);
            return {};
        }
        persistence::SaveCommitCompletion completion{};
        if (!wait_completion(*worker_, completion)
                || completion.job_slot != lease.job_slot
                || completion.revision != lease.revision
                || completion.intent != lease.intent
                || completion.token != lease.token
                || completion.epoch != lease.epoch
                || completion.kind != lease.kind
                || completion.result.state
                    != persistence::SaveCommitState::committed
                || completion.result.error != persistence::SaveError::none
                || completion.result.active_slot
                    == persistence::SaveSlot::none
                || completion.result.active_slot != expected_active
                || !inspect_active_envelope(
                    completion.result.active_slot, revision)) {
            return {};
        }
        expected_ = std::move(expected);
        last_active_slot_ = completion.result.active_slot;
        last_revision_ = revision;
        ++next_revision_;
        return CommitReceipt{
            revision, state.commit_generation, completion.result.active_slot};
    }

    std::unique_ptr<dungeon::DungeonSession> fresh_scan_and_restart(
        const checkpoint::DeathLifecycle death_lifecycle,
        const checkpoint::RoomProgressLifecycle room_lifecycle) {
        if (expected_ == nullptr || last_active_slot_ == persistence::SaveSlot::none
                || last_revision_ == 0U) return {};
        stop_and_destroy_transaction();
        storage_.reset(new (std::nothrow) persistence::SaveCommitStorage{});
        if (storage_ == nullptr || !storage_->initialize({directory_})
                || storage_->load_state()
                    != persistence::SaveLoadState::ready
                || storage_->loaded_format()
                    != persistence::kCheckpointFormatVersionV9
                || storage_->loaded_migrated()
                || storage_->loaded_slot() != last_active_slot_) {
            return {};
        }
        const checkpoint::SaveCheckpointSlot* const loaded =
            storage_->loaded_checkpoint();
        if (loaded == nullptr
                || loaded->persistence_revision != last_revision_
                || loaded->state.death.lifecycle != death_lifecycle
                || loaded->room_progress.lifecycle != room_lifecycle
                || !dungeon::same_run_state(loaded->state, expected_->state)
                || !checkpoint::same_room_progress_checkpoint(
                    loaded->room_progress, expected_->room_progress)
                || !inspect_active_envelope(
                    storage_->loaded_slot(), last_revision_)) {
            return {};
        }
        std::unique_ptr<dungeon::DungeonSession> restarted{};
        try {
            restarted.reset(new (std::nothrow) dungeon::DungeonSession{
                {}, loaded->state});
        } catch (...) {
            return {};
        }
        if (restarted == nullptr
                || !restarted->restore_room_progress_checkpoint(*loaded)) {
            return {};
        }
        storage_->release_loaded_checkpoints();
        next_revision_ = last_revision_ + 1U;
        if (!start_worker()) return {};
        return restarted;
    }

    [[nodiscard]] const checkpoint::SaveCheckpointSlot* latest() const noexcept {
        return expected_.get();
    }

private:
    bool start_worker() {
        if (storage_ == nullptr) return false;
        worker_.reset(new (std::nothrow)
            persistence::SaveCommitWorker{*storage_});
        return worker_ != nullptr && worker_->start();
    }

    void stop_and_destroy_transaction() noexcept {
        if (worker_ != nullptr) worker_->stop_and_join();
        worker_.reset();
        storage_.reset();
    }

    bool inspect_active_envelope(const persistence::SaveSlot slot,
        const std::uint64_t expected_revision) const {
        if (storage_ == nullptr || slot == persistence::SaveSlot::none) {
            return false;
        }
        const std::filesystem::path* const path = storage_->slot_path(
            slot == persistence::SaveSlot::a ? 0U : 1U);
        if (path == nullptr) return false;
        std::ifstream input(*path, std::ios::binary);
        std::vector<std::uint8_t> bytes{
            std::istreambuf_iterator<char>{input},
            std::istreambuf_iterator<char>{}};
        if (bytes.empty()) return {};
        std::uint64_t actual_revision{};
        return persistence::inspect_checkpoint_v9_envelope(
            bytes.data(), bytes.size(), actual_revision)
                == persistence::CodecError::none
            && actual_revision == expected_revision;
    }

    std::filesystem::path directory_{};
    std::unique_ptr<persistence::SaveCommitStorage> storage_{};
    std::unique_ptr<persistence::SaveCommitWorker> worker_{};
    std::unique_ptr<checkpoint::SaveCheckpointSlot> expected_{};
    persistence::SaveSlot last_active_slot_{persistence::SaveSlot::none};
    std::uint64_t last_revision_{};
    std::uint64_t next_revision_{1U};
};

std::optional<CommitReceipt> resolve_with_store(dungeon::DungeonSession& session,
    V9FixtureStore& store) {
    const dungeon::PendingSave* const pending = session.pending_save_view();
    if (pending == nullptr) return std::nullopt;
    const auto kind = pending->kind;
    const std::uint64_t expected_generation = pending->expected_generation;
    if (expected_generation != pending->next_state.commit_generation) {
        return std::nullopt;
    }
    auto committed = store.commit(session, pending->next_state);
    if (!committed.has_value()
            || committed->commit_generation != expected_generation) {
        return std::nullopt;
    }
    dungeon::checkpoint::DungeonRunState verified{};
    try {
        verified = pending->next_state;
    } catch (...) {
        return std::nullopt;
    }
    session.resolve_pending_save({dungeon::SaveDisposition::committed,
        expected_generation, std::move(verified), kind});
    return session.snapshot().phase == dungeon::RoomPhase::faulted
        ? std::nullopt : committed;
}

bool wait_for_real_death(dungeon::DungeonSession& session) {
    for (int tick = 0; tick < 30000; ++tick) {
        session.tick({});
        while (session.try_pop_combat_event().has_value()) {}
        while (session.try_pop_event().has_value()) {}
        const auto* pending = session.pending_save_view();
        if (pending != nullptr) {
            return pending->kind == dungeon::PendingSaveKind::death_retreat
                && pending->death_snapshot.has_value()
                && pending->next_state.death.lifecycle
                    == dungeon::checkpoint::DeathLifecycle::pending_continue;
        }
        if (session.snapshot().phase == dungeon::RoomPhase::faulted) return false;
    }
    return false;
}

std::optional<PathResult> exercise_path(const std::filesystem::path& directory,
    dungeon::checkpoint::DungeonRunState start) {
    V9FixtureStore store{};
    std::unique_ptr<dungeon::DungeonSession> session{
        new (std::nothrow) dungeon::DungeonSession{{}, start}};
    if (session == nullptr || !store.initialize(directory)) return std::nullopt;
    const dungeon::PendingSave* const constructor_pending =
        session->pending_save_view();
    std::unique_ptr<dungeon::checkpoint::DungeonRunState>
        constructor_next_state{};
    if (constructor_pending != nullptr) {
        constructor_next_state.reset(new (std::nothrow)
            dungeon::checkpoint::DungeonRunState{});
        if (constructor_next_state == nullptr) return std::nullopt;
        try {
            *constructor_next_state = constructor_pending->next_state;
        } catch (...) {
            return std::nullopt;
        }
    }
    const std::optional<dungeon::PendingSaveKind> constructor_kind =
        constructor_pending == nullptr
        ? std::nullopt : std::optional{constructor_pending->kind};
    const std::uint64_t constructor_generation = constructor_pending == nullptr
        ? 0U : constructor_pending->expected_generation;
    auto seeded = store.commit(*session, start);
    if (!seeded.has_value() || seeded->persistence_revision != 1U
            || seeded->active_slot != persistence::SaveSlot::a) {
        return std::nullopt;
    }
    if (constructor_kind.has_value()) {
        const auto* retained = session->pending_save_view();
        if (*constructor_kind != dungeon::PendingSaveKind::abyss_start
                || retained == nullptr || retained->kind != *constructor_kind
                || retained->expected_generation != constructor_generation
                || constructor_next_state == nullptr
                || !dungeon::same_run_state(
                    retained->next_state, *constructor_next_state)) {
            return std::nullopt;
        }
        const auto abyss_started = resolve_with_store(*session, store);
        if (!abyss_started.has_value()
                || abyss_started->persistence_revision != 2U
                || abyss_started->active_slot != persistence::SaveSlot::b) {
            return std::nullopt;
        }
    }
    if (!wait_for_real_death(*session)) return std::nullopt;
    const auto* death_pending = session->pending_save_view();
    if (death_pending == nullptr) return std::nullopt;
    const auto death = death_pending->next_state.death;
    const auto death_committed = resolve_with_store(*session, store);
    const std::uint64_t expected_death_revision =
        constructor_kind.has_value() ? 3U : 2U;
    if (!death_committed.has_value()
            || death_committed->persistence_revision
                != expected_death_revision
            || death_committed->active_slot
                != slot_for_revision(expected_death_revision)) {
        return std::nullopt;
    }

    session = store.fresh_scan_and_restart(
        checkpoint::DeathLifecycle::pending_continue,
        checkpoint::RoomProgressLifecycle::death_pending);
    if (session == nullptr) {
        return std::nullopt;
    }
    const auto recap = session->snapshot();
    if (!recap.death.has_value() || !recap.death->can_continue
            || recap.death->checkpoint.target_room.seed != death.target_room.seed
            || session->request_death_continue()
                != dungeon::RequestResult::accepted) {
        return std::nullopt;
    }
    const auto continue_committed = resolve_with_store(*session, store);
    const std::uint64_t expected_continue_revision =
        expected_death_revision + 1U;
    if (!continue_committed.has_value()
            || continue_committed->persistence_revision
                != expected_continue_revision
            || continue_committed->active_slot
                != slot_for_revision(expected_continue_revision)) {
        return std::nullopt;
    }
    session = store.fresh_scan_and_restart(
        checkpoint::DeathLifecycle::none,
        checkpoint::RoomProgressLifecycle::none);
    const checkpoint::SaveCheckpointSlot* const continued = store.latest();
    if (session == nullptr || continued == nullptr
            || continued->state.current_room.index != death.target_room.index
            || continued->state.current_room.seed != death.target_room.seed
            || session->snapshot().room_index != death.target_room.index
            || session->snapshot().room_seed != death.target_room.seed) {
        return std::nullopt;
    }
    try {
        return PathResult{death, continued->state};
    } catch (...) {
        return std::nullopt;
    }
}

std::optional<dungeon::checkpoint::DungeonRunState> abyss_start_state() {
    constexpr std::array<dungeon::ExitDirection, 4> directions{{
        dungeon::ExitDirection::up, dungeon::ExitDirection::down,
        dungeon::ExitDirection::left, dungeon::ExitDirection::right}};
    for (std::uint64_t seed = 1U; seed < 100000U; ++seed) {
        const auto initial = dungeon::make_initial_run_state(seed, {});
        if (initial.fault != dungeon::DungeonFault::none) continue;
        const auto doors = dungeon::preview_abyss_doors(initial.state.current_room);
        for (std::size_t index = 0; index < doors.size(); ++index) {
            if (!doors[index]) continue;
            const auto target = dungeon::make_door_transition(
                initial.state, directions[index], {});
            if (target.fault == dungeon::DungeonFault::none
                    && target.state.current_room.is_abyss) return target.state;
        }
    }
    return std::nullopt;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 1 || argv == nullptr || argv[0] == nullptr) return 2;
    const auto root = std::filesystem::absolute(argv[0]).parent_path()
        / "stage11-death-fixture";
    const auto initial = dungeon::make_initial_run_state(0x11A001U, {});
    if (initial.fault != dungeon::DungeonFault::none) return 3;

    const auto normal = exercise_path(fresh_directory(root, "normal"), initial.state);
    if (!normal.has_value() || normal->death.target_room.depth != 1U
            || normal->continued.current_room.depth != 1U) return 4;

    const auto deep_state = dungeon::make_descent_transition(initial.state, {});
    if (deep_state.fault != dungeon::DungeonFault::none
            || deep_state.state.current_room.depth != 2U) return 5;
    const auto deep = exercise_path(fresh_directory(root, "deep"), deep_state.state);
    if (!deep.has_value() || deep->death.target_room.depth != 1U
            || deep->continued.current_room.depth != 1U) return 6;

    const auto abyss_state = abyss_start_state();
    if (!abyss_state.has_value()) return 7;
    const auto abyss = exercise_path(fresh_directory(root, "abyss"), *abyss_state);
    if (!abyss.has_value() || !abyss->death.death_was_abyss
            || !abyss->continued.last_abyss_resolution.valid) return 8;

    std::cout << "normal_death=PASS pending_restart=PASS deep_continue=PASS "
                 "floor_one_continue=PASS abyss_death=PASS\n";
    return 0;
}
