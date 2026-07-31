#pragma once

#include "checkpoint/room_progress_checkpoint.hpp"
#include "persistence/room_progress_codec.hpp"
#include "persistence/save_store.hpp"

#include <array>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <thread>

namespace arpg::persistence {

enum class SaveCommitRequestKind : std::uint8_t { background, exact };

enum class SaveCommitSubmitState : std::uint8_t {
    accepted,
    superseded_background,
    busy,
    stopped,
};

enum class SaveCommitSlotState : std::uint8_t {
    free,
    capturing,
    queued,
    active,
    completion_ready,
};

enum class SaveCommitDiskState : std::uint8_t {
    missing,
    valid,
    invalid,
    unavailable,
};

struct SaveCommitCaptureLease final {
    SaveCommitSubmitState state{SaveCommitSubmitState::busy};
    std::uint8_t job_slot{0xFFU};
    std::uint64_t revision{};
    std::uint64_t intent{};
    std::uint64_t token{};
    std::uint64_t epoch{};
    SaveCommitRequestKind kind{SaveCommitRequestKind::background};
};

struct SaveCommitSubmission final {
    SaveCommitSubmitState state{SaveCommitSubmitState::busy};
    std::uint8_t reclaimed_slot{0xFFU};
};

struct SaveCommitExactResult final {
    SaveCommitState state{SaveCommitState::indeterminate};
    SaveError error{SaveError::none};
    SaveSlot active_slot{SaveSlot::none};
};

struct SaveCommitCompletion final {
    std::uint8_t job_slot{0xFFU};
    std::uint64_t revision{};
    SaveCommitRequestKind kind{SaveCommitRequestKind::background};
    std::uint64_t intent{};
    std::uint64_t token{};
    std::uint64_t epoch{};
    SaveCommitExactResult result{};
};

struct SaveCommitJobSlot final {
    SaveCommitJobSlot() noexcept = default;
    SaveCommitJobSlot(const SaveCommitJobSlot&) = delete;
    SaveCommitJobSlot& operator=(const SaveCommitJobSlot&) = delete;
    SaveCommitJobSlot(SaveCommitJobSlot&&) = delete;
    SaveCommitJobSlot& operator=(SaveCommitJobSlot&&) = delete;

    checkpoint::SaveCheckpointSlot checkpoint{};
    std::uint64_t revision{};
    std::uint64_t intent{};
    SaveCommitRequestKind kind{SaveCommitRequestKind::background};
};

class SaveCommitStorage final {
public:
    SaveCommitStorage() noexcept = default;
    SaveCommitStorage(const SaveCommitStorage&) = delete;
    SaveCommitStorage& operator=(const SaveCommitStorage&) = delete;
    SaveCommitStorage(SaveCommitStorage&&) = delete;
    SaveCommitStorage& operator=(SaveCommitStorage&&) = delete;

    [[nodiscard]] bool initialize(const SaveStoreConfig& config) noexcept;
    [[nodiscard]] const SaveCommitJobSlot* job(
        std::size_t index) const noexcept;
    [[nodiscard]] const std::filesystem::path* slot_path(
        std::size_t index) const noexcept;
    [[nodiscard]] const std::filesystem::path* temp_path(
        std::size_t index) const noexcept;
    [[nodiscard]] SaveLoadState load_state() const noexcept;
    [[nodiscard]] SaveSlot loaded_slot() const noexcept;
    [[nodiscard]] const checkpoint::SaveCheckpointSlot*
    loaded_checkpoint() const noexcept;
    [[nodiscard]] bool loaded_migrated() const noexcept;
    [[nodiscard]] std::uint32_t loaded_format() const noexcept;
    [[nodiscard]] bool loaded_recovered() const noexcept;
    [[nodiscard]] std::size_t resident_bytes() const noexcept;
    [[nodiscard]] bool archive_invalid_files() noexcept;
    void release_loaded_checkpoints() noexcept;

private:
    friend class SaveCommitWorker;
    void reset_initialization() noexcept;
    [[nodiscard]] std::size_t compute_resident_bytes() const noexcept;
    std::array<SaveCommitJobSlot, 2U> jobs_{};
    std::array<std::array<std::uint8_t, kMaximumEncodedCheckpointBytes>, 2U>
        buffers_{};
    std::array<std::filesystem::path, 2U> slot_paths_{};
    std::array<std::filesystem::path, 2U> temp_paths_{};
    SaveStoreConfig config_{};
    std::array<SaveCommitDiskState, 2U> disk_states_{};
    std::array<std::uint64_t, 2U> disk_revisions_{};
    std::array<std::size_t, 2U> disk_sizes_{};
    std::array<std::uint32_t, 2U> disk_formats_{};
    std::array<std::uint32_t, 2U> disk_checksums_{};
    SaveLoadState load_state_{SaveLoadState::blocked};
    std::uint8_t loaded_index_{0xFFU};
    bool loaded_migrated_{};
    bool loaded_recovered_{};
    bool equal_revision_conflict_{};
    bool initialized_{};
    std::size_t resident_bytes_{};
};

inline constexpr std::size_t kSaveCommitMaximumPathCodeUnits = 32767U;
inline constexpr std::size_t kSaveCommitResidentBudgetBytes =
    32U * 1024U * 1024U;
inline constexpr std::size_t kSaveCommitStorageResidentBytes =
    sizeof(SaveCommitStorage)
        + 2U * kMaximumCheckpointItemCount * sizeof(items::ItemInstance)
        + 5U * (kSaveCommitMaximumPathCodeUnits + 1U)
            * sizeof(std::filesystem::path::value_type);
static_assert(kSaveCommitStorageResidentBytes
    <= kSaveCommitResidentBudgetBytes);

class SaveCommitWorker final {
public:
    explicit SaveCommitWorker(SaveCommitStorage& storage) noexcept;
    ~SaveCommitWorker();
    SaveCommitWorker(const SaveCommitWorker&) = delete;
    SaveCommitWorker& operator=(const SaveCommitWorker&) = delete;
    SaveCommitWorker(SaveCommitWorker&&) = delete;
    SaveCommitWorker& operator=(SaveCommitWorker&&) = delete;

    [[nodiscard]] bool start() noexcept;
    [[nodiscard]] SaveCommitCaptureLease acquire_capture_slot(
        std::uint64_t revision,
        SaveCommitRequestKind kind,
        std::uint64_t intent = 0U) noexcept;
    [[nodiscard]] SaveCommitJobSlot* capture_job(
        const SaveCommitCaptureLease& lease) noexcept;
    [[nodiscard]] SaveCommitSubmission submit(
        const SaveCommitCaptureLease& lease) noexcept;
    void cancel_capture(const SaveCommitCaptureLease& lease) noexcept;
    [[nodiscard]] bool try_take_completion(
        SaveCommitCompletion& completion) noexcept;
    void stop_and_join() noexcept;
    [[nodiscard]] bool idle() const noexcept;

private:
    static constexpr std::uint8_t kNoSlot = 0xFFU;
    void run() noexcept;
    [[nodiscard]] SaveCommitExactResult commit(std::uint8_t job_slot) noexcept;

    SaveCommitStorage* storage_{};
    mutable std::mutex mutex_{};
    std::condition_variable wake_{};
    std::thread thread_{};
    SaveCommitCompletion completion_{};
    std::array<SaveCommitSlotState, 2U> slot_states_{};
    std::array<std::uint64_t, 2U> slot_tokens_{};
    std::uint8_t active_slot_{kNoSlot};
    std::uint8_t queued_slot_{kNoSlot};
    std::uint64_t next_token_{1U};
    std::uint64_t epoch_{};
    bool completion_ready_{};
    bool started_{};
    bool stopping_{};
};

static_assert(sizeof(SaveCommitWorker) <= 1024U);

}  // namespace arpg::persistence
