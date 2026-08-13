#include "persistence/save_commit_worker.hpp"
#include "persistence/save_store_detail.hpp"

#include "checkpoint/room_checkpoint_validation.hpp"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <system_error>
#include <vector>

#if defined(_WIN32)
#define NOMINMAX
#include <Windows.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
#endif

namespace arpg::persistence {
namespace {

constexpr std::size_t kSlotCount = 2U;

enum class DirectoryLeaseState : std::uint8_t {
    acquired,
    busy,
    failed,
};

DirectoryLeaseState acquire_directory_lease(
    const std::filesystem::path& path,
    std::intptr_t& token) noexcept {
    token = -1;
#if defined(_WIN32)
    const HANDLE file = CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return DirectoryLeaseState::failed;
    OVERLAPPED overlapped{};
    if (LockFileEx(file,
            LOCKFILE_EXCLUSIVE_LOCK | LOCKFILE_FAIL_IMMEDIATELY,
            0U, 1U, 0U, &overlapped) == FALSE) {
        const DWORD error = GetLastError();
        static_cast<void>(CloseHandle(file));
        return error == ERROR_LOCK_VIOLATION
            ? DirectoryLeaseState::busy : DirectoryLeaseState::failed;
    }
    token = reinterpret_cast<std::intptr_t>(file);
    return DirectoryLeaseState::acquired;
#else
    int open_flags = O_CREAT | O_RDWR;
#if defined(O_CLOEXEC)
    open_flags |= O_CLOEXEC;
#endif
    const int descriptor = ::open(path.c_str(), open_flags, 0600);
    if (descriptor < 0) return DirectoryLeaseState::failed;
#if !defined(O_CLOEXEC)
    if (::fcntl(descriptor, F_SETFD, FD_CLOEXEC) != 0) {
        static_cast<void>(::close(descriptor));
        return DirectoryLeaseState::failed;
    }
#endif
    int result{};
    do {
        result = ::flock(descriptor, LOCK_EX | LOCK_NB);
    } while (result != 0 && errno == EINTR);
    if (result != 0) {
        const int error = errno;
        static_cast<void>(::close(descriptor));
        return error == EWOULDBLOCK || error == EAGAIN
            ? DirectoryLeaseState::busy : DirectoryLeaseState::failed;
    }
    token = static_cast<std::intptr_t>(descriptor);
    return DirectoryLeaseState::acquired;
#endif
}

void release_directory_lease_token(std::intptr_t& token) noexcept {
    if (token == -1) return;
#if defined(_WIN32)
    const HANDLE file = reinterpret_cast<HANDLE>(token);
    OVERLAPPED overlapped{};
    static_cast<void>(UnlockFileEx(file, 0U, 1U, 0U, &overlapped));
    static_cast<void>(CloseHandle(file));
#else
    const int descriptor = static_cast<int>(token);
    int result{};
    do {
        result = ::flock(descriptor, LOCK_UN);
    } while (result != 0 && errno == EINTR);
    static_cast<void>(::close(descriptor));
#endif
    token = -1;
}

std::size_t path_allocation_bytes(
    const std::filesystem::path& path) noexcept {
    const std::filesystem::path empty{};
    const std::size_t inline_capacity = empty.native().capacity();
    const std::size_t capacity = path.native().capacity();
    return capacity <= inline_capacity ? 0U
        : (capacity + 1U) * sizeof(std::filesystem::path::value_type);
}

}  // namespace

SaveCommitStorage::~SaveCommitStorage() noexcept {
    release_directory_lease();
}

void SaveCommitStorage::release_directory_lease() noexcept {
    release_directory_lease_token(directory_lease_token_);
}

void SaveCommitStorage::reset_initialization() noexcept {
    release_directory_lease();
    for (SaveCommitJobSlot& job : jobs_) {
        checkpoint::clear_save_checkpoint_slot(job.checkpoint);
        std::vector<items::ItemInstance>{}.swap(
            job.checkpoint.state.item_ownership.items);
        job.revision = 0U;
        job.intent = 0U;
        job.kind = SaveCommitRequestKind::background;
    }
    for (std::filesystem::path& path : slot_paths_) {
        std::filesystem::path{}.swap(path);
    }
    for (std::filesystem::path& path : temp_paths_) {
        std::filesystem::path{}.swap(path);
    }
    std::filesystem::path{}.swap(config_.directory);
    config_.fault_hook = nullptr;
    config_.fault_context = nullptr;
    config_.stamp_provider = nullptr;
    config_.stamp_context = nullptr;
    disk_states_ = {};
    disk_revisions_ = {};
    disk_sizes_ = {};
    disk_formats_ = {};
    disk_checksums_ = {};
    load_state_ = SaveLoadState::blocked;
    loaded_index_ = 0xFFU;
    loaded_migrated_ = false;
    loaded_recovered_ = false;
    equal_revision_conflict_ = false;
    initialized_ = false;
    resident_bytes_ = 0U;
}

std::size_t SaveCommitStorage::compute_resident_bytes() const noexcept {
    std::size_t result = sizeof(SaveCommitStorage);
    for (const SaveCommitJobSlot& job : jobs_) {
        result += job.checkpoint.state.item_ownership.items.capacity()
            * sizeof(items::ItemInstance);
    }
    result += path_allocation_bytes(config_.directory);
    for (const std::filesystem::path& path : slot_paths_) {
        result += path_allocation_bytes(path);
    }
    for (const std::filesystem::path& path : temp_paths_) {
        result += path_allocation_bytes(path);
    }
    return result;
}

namespace {

bool hook_failed(const SaveStoreConfig& config,
    SaveFaultPoint point) noexcept {
    return config.fault_hook != nullptr
        && config.fault_hook(point, config.fault_context);
}

#if defined(_WIN32)
enum class FileIoResult : std::uint8_t {
    ok,
    open_failed,
    read_failed,
    write_failed,
    flush_failed,
    close_failed,
    oversized,
};

FileIoResult write_file(const std::filesystem::path& path,
    const std::uint8_t* bytes, std::size_t size,
    const SaveStoreConfig& config) noexcept {
    const HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr,
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return FileIoResult::open_failed;
    std::size_t offset = 0U;
    FileIoResult result = FileIoResult::ok;
    while (offset < size) {
        const DWORD chunk = static_cast<DWORD>((std::min)(
            size - offset, static_cast<std::size_t>(0x7FFFFFFFU)));
        DWORD written{};
        if (!WriteFile(file, bytes + offset, chunk, &written, nullptr)
                || written != chunk) {
            result = FileIoResult::write_failed;
            break;
        }
        offset += written;
    }
    if (result == FileIoResult::ok
            && (hook_failed(config, SaveFaultPoint::temp_flush)
                || FlushFileBuffers(file) == FALSE)) {
        result = FileIoResult::flush_failed;
    }
    const bool close_failed =
        hook_failed(config, SaveFaultPoint::temp_close)
        || CloseHandle(file) == FALSE;
    return close_failed ? FileIoResult::close_failed : result;
}

FileIoResult read_file(const std::filesystem::path& path, std::uint8_t* bytes,
    std::size_t capacity, std::size_t& size) noexcept {
    size = 0U;
    const HANDLE file = CreateFileW(path.c_str(), GENERIC_READ,
        FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (file == INVALID_HANDLE_VALUE) return FileIoResult::open_failed;
    LARGE_INTEGER length{};
    FileIoResult result = FileIoResult::ok;
    if (GetFileSizeEx(file, &length) == FALSE || length.QuadPart < 0) {
        result = FileIoResult::read_failed;
    } else if (static_cast<unsigned long long>(length.QuadPart) > capacity) {
        result = FileIoResult::oversized;
    }
    if (result == FileIoResult::ok) {
        size = static_cast<std::size_t>(length.QuadPart);
        std::size_t offset = 0U;
        while (offset < size) {
            const DWORD chunk = static_cast<DWORD>((std::min)(
                size - offset, static_cast<std::size_t>(0x7FFFFFFFU)));
            DWORD read{};
            if (!ReadFile(file, bytes + offset, chunk, &read, nullptr)
                    || read != chunk) {
                result = FileIoResult::read_failed;
                break;
            }
            offset += read;
        }
    }
    if (CloseHandle(file) == FALSE) result = FileIoResult::close_failed;
    if (result != FileIoResult::ok) size = 0U;
    return result;
}

FileIoResult compare_file(const std::filesystem::path& path,
    const std::uint8_t* expected, std::size_t expected_size) noexcept {
    if (expected == nullptr) return FileIoResult::read_failed;
    const HANDLE file = CreateFileW(path.c_str(), GENERIC_READ,
        FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (file == INVALID_HANDLE_VALUE) return FileIoResult::open_failed;
    LARGE_INTEGER length{};
    FileIoResult result = FileIoResult::ok;
    if (GetFileSizeEx(file, &length) == FALSE || length.QuadPart < 0
            || static_cast<unsigned long long>(length.QuadPart)
                != expected_size) {
        result = FileIoResult::read_failed;
    }
    std::array<std::uint8_t, 4096U> chunk{};
    std::size_t offset{};
    while (result == FileIoResult::ok && offset < expected_size) {
        const DWORD requested = static_cast<DWORD>((std::min)(
            chunk.size(), expected_size - offset));
        DWORD read{};
        if (!ReadFile(file, chunk.data(), requested, &read, nullptr)
                || read != requested
                || std::memcmp(chunk.data(), expected + offset,
                    requested) != 0) {
            result = FileIoResult::read_failed;
            break;
        }
        offset += read;
    }
    if (CloseHandle(file) == FALSE) result = FileIoResult::close_failed;
    return result;
}

bool publish_file(const std::filesystem::path& temp,
    const std::filesystem::path& target) noexcept {
    // MOVEFILE_WRITE_THROUGH is the strongest documented Win32 durability
    // contract for a same-volume replacement. Windows does not expose a
    // portable, documented equivalent of POSIX fsync for directory handles.
    return MoveFileExW(temp.c_str(), target.c_str(),
        MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != FALSE;
}

bool remove_file(const std::filesystem::path& path) noexcept {
    if (DeleteFileW(path.c_str()) != FALSE) return true;
    const DWORD error = GetLastError();
    return error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND;
}

SaveCommitDiskState disk_file_state(
    const std::filesystem::path& path) noexcept {
    if (GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES) {
        return SaveCommitDiskState::valid;
    }
    const DWORD error = GetLastError();
    return error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND
        ? SaveCommitDiskState::missing : SaveCommitDiskState::unavailable;
}
#else
enum class FileIoResult : std::uint8_t {
    ok, open_failed, read_failed, write_failed, flush_failed, close_failed,
    oversized
};
FileIoResult write_file(const std::filesystem::path&, const std::uint8_t*,
    std::size_t, const SaveStoreConfig&) noexcept {
    return FileIoResult::open_failed;
}
FileIoResult read_file(const std::filesystem::path&, std::uint8_t*,
    std::size_t, std::size_t&) noexcept {
    return FileIoResult::open_failed;
}
FileIoResult compare_file(const std::filesystem::path&,
    const std::uint8_t*, std::size_t) noexcept {
    return FileIoResult::open_failed;
}
bool publish_file(const std::filesystem::path&,
    const std::filesystem::path&) noexcept { return false; }
bool remove_file(const std::filesystem::path&) noexcept { return false; }
SaveCommitDiskState disk_file_state(
    const std::filesystem::path&) noexcept {
    return SaveCommitDiskState::unavailable;
}
#endif

}  // namespace

bool SaveCommitStorage::initialize(const SaveStoreConfig& config) noexcept {
    SaveError error{SaveError::none};
    return initialize(config, error);
}

bool SaveCommitStorage::initialize(
    const SaveStoreConfig& config, SaveError& error) noexcept {
    error = SaveError::directory_unavailable;
    try {
        if (initialized_ || config.directory.empty()) return false;
        std::error_code directory_error;
        std::filesystem::create_directories(
            config.directory, directory_error);
        if (directory_error) return false;
        const std::filesystem::path lease_path =
            config.directory / "run.lock";
        const auto path_within_bound = [](const std::filesystem::path& path) {
            return path.native().capacity()
                <= kSaveCommitMaximumPathCodeUnits;
        };
        if (!path_within_bound(config.directory)
                || !path_within_bound(lease_path)) return false;
        const DirectoryLeaseState lease = acquire_directory_lease(
            lease_path, directory_lease_token_);
        if (lease != DirectoryLeaseState::acquired) {
            if (lease == DirectoryLeaseState::busy) {
                error = SaveError::directory_busy;
            }
            return false;
        }
        config_ = config;
        slot_paths_[0U] = config.directory / "run_a.sav";
        slot_paths_[1U] = config.directory / "run_b.sav";
        temp_paths_[0U] = config.directory / "run_a.tmp";
        temp_paths_[1U] = config.directory / "run_b.tmp";
        for (SaveCommitJobSlot& job : jobs_) {
            job.checkpoint.state.item_ownership.items.reserve(
                kMaximumCheckpointItemCount);
            if (job.checkpoint.state.item_ownership.items.capacity()
                    != kMaximumCheckpointItemCount) {
                reset_initialization();
                return false;
            }
        }
        if (!path_within_bound(config_.directory)
                || !path_within_bound(slot_paths_[0U])
                || !path_within_bound(slot_paths_[1U])
                || !path_within_bound(temp_paths_[0U])
                || !path_within_bound(temp_paths_[1U])) {
            reset_initialization();
            return false;
        }
        const bool stale_temp = disk_file_state(temp_paths_[0U])
                != SaveCommitDiskState::missing
            || disk_file_state(temp_paths_[1U])
                != SaveCommitDiskState::missing;
        if (!remove_file(temp_paths_[0U])
                || !remove_file(temp_paths_[1U])
                || disk_file_state(temp_paths_[0U])
                    != SaveCommitDiskState::missing
                || disk_file_state(temp_paths_[1U])
                    != SaveCommitDiskState::missing) {
            reset_initialization();
            return false;
        }
        std::array<bool, 2U> migrated{};
        for (std::size_t index = 0U; index < kSlotCount; ++index) {
            disk_states_[index] = disk_file_state(slot_paths_[index]);
            if (disk_states_[index] != SaveCommitDiskState::valid) continue;
            const FileIoResult read = read_file(slot_paths_[index],
                buffers_[index].data(), kMaximumEncodedCheckpointBytes,
                disk_sizes_[index]);
            if (read != FileIoResult::ok) {
                disk_states_[index] = read == FileIoResult::oversized
                    ? SaveCommitDiskState::invalid
                    : SaveCommitDiskState::unavailable;
                continue;
            }
            if (disk_sizes_[index] >= 12U) {
                for (std::size_t byte = 0U; byte < 4U; ++byte) {
                    disk_formats_[index] |= static_cast<std::uint32_t>(
                        buffers_[index][8U + byte]) << (byte * 8U);
                }
            }
            if (decode_checkpoint_v10_into(buffers_[index].data(),
                    disk_sizes_[index], jobs_[index].checkpoint,
                    migrated[index]) != CodecError::none) {
                disk_states_[index] = SaveCommitDiskState::invalid;
                continue;
            }
            disk_revisions_[index] =
                jobs_[index].checkpoint.persistence_revision;
            disk_checksums_[index] = crc32_update(0U,
                buffers_[index].data(), disk_sizes_[index]);
        }
        if (disk_states_[0U] == SaveCommitDiskState::unavailable
                || disk_states_[1U] == SaveCommitDiskState::unavailable) {
            load_state_ = SaveLoadState::blocked;
        } else {
            const bool valid_a =
                disk_states_[0U] == SaveCommitDiskState::valid;
            const bool valid_b =
                disk_states_[1U] == SaveCommitDiskState::valid;
            if (!valid_a && !valid_b) {
                load_state_ = disk_states_[0U] == SaveCommitDiskState::missing
                        && disk_states_[1U] == SaveCommitDiskState::missing
                    ? SaveLoadState::empty : SaveLoadState::recovery_required;
            } else if (valid_a && valid_b
                    && disk_revisions_[0U] == disk_revisions_[1U]) {
                const bool exact = disk_sizes_[0U] == disk_sizes_[1U]
                    && std::equal(buffers_[0U].begin(),
                        buffers_[0U].begin() + disk_sizes_[0U],
                        buffers_[1U].begin());
                if (!exact) {
                    equal_revision_conflict_ = true;
                    load_state_ = SaveLoadState::recovery_required;
                } else {
                    load_state_ = SaveLoadState::ready;
                    loaded_index_ = 0U;
                }
            } else {
                load_state_ = SaveLoadState::ready;
                loaded_index_ = !valid_b || (valid_a
                        && disk_revisions_[0U] > disk_revisions_[1U])
                    ? 0U : 1U;
            }
            if (load_state_ == SaveLoadState::ready) {
                loaded_migrated_ = migrated[loaded_index_];
                loaded_recovered_ = stale_temp
                    || disk_states_[1U - loaded_index_]
                        == SaveCommitDiskState::invalid;
            }
        }
        resident_bytes_ = compute_resident_bytes();
        if (resident_bytes_ > kSaveCommitStorageResidentBytes
                || resident_bytes_ > kSaveCommitResidentBudgetBytes) {
            reset_initialization();
            return false;
        }
        initialized_ = true;
        error = SaveError::none;
        return true;
    } catch (...) {
        reset_initialization();
        return false;
    }
}

const SaveCommitJobSlot* SaveCommitStorage::job(
    const std::size_t index) const noexcept {
    return index < jobs_.size() ? &jobs_[index] : nullptr;
}

const std::filesystem::path* SaveCommitStorage::slot_path(
    const std::size_t index) const noexcept {
    return index < slot_paths_.size() ? &slot_paths_[index] : nullptr;
}

const std::filesystem::path* SaveCommitStorage::temp_path(
    const std::size_t index) const noexcept {
    return index < temp_paths_.size() ? &temp_paths_[index] : nullptr;
}

SaveLoadState SaveCommitStorage::load_state() const noexcept {
    return load_state_;
}

SaveSlot SaveCommitStorage::loaded_slot() const noexcept {
    return loaded_index_ == 0U ? SaveSlot::a
        : loaded_index_ == 1U ? SaveSlot::b : SaveSlot::none;
}

const checkpoint::SaveCheckpointSlot*
SaveCommitStorage::loaded_checkpoint() const noexcept {
    return load_state_ == SaveLoadState::ready && loaded_index_ < jobs_.size()
        ? &jobs_[loaded_index_].checkpoint : nullptr;
}

bool SaveCommitStorage::loaded_migrated() const noexcept {
    return loaded_migrated_;
}

std::uint32_t SaveCommitStorage::loaded_format() const noexcept {
    return loaded_index_ < disk_formats_.size()
        ? disk_formats_[loaded_index_] : 0U;
}

bool SaveCommitStorage::loaded_recovered() const noexcept {
    return loaded_recovered_;
}

std::size_t SaveCommitStorage::resident_bytes() const noexcept {
    return initialized_ ? resident_bytes_ : 0U;
}

bool SaveCommitStorage::archive_invalid_files() noexcept {
    if (!initialized_ || load_state_ != SaveLoadState::recovery_required) {
        return false;
    }
    std::vector<std::filesystem::path> invalid{};
    try {
        invalid.reserve(4U);
        if (equal_revision_conflict_) {
            invalid.push_back(slot_paths_[0U]);
            invalid.push_back(slot_paths_[1U]);
        }
        for (std::size_t index = 0U; index < disk_states_.size(); ++index) {
            if (disk_states_[index] == SaveCommitDiskState::invalid
                    || disk_states_[index] == SaveCommitDiskState::unavailable) {
                invalid.push_back(slot_paths_[index]);
            }
            std::error_code error;
            if (std::filesystem::exists(temp_paths_[index], error) && !error) {
                invalid.push_back(temp_paths_[index]);
            }
        }
    } catch (...) {
        return false;
    }
    const detail::ArchiveResult archived =
        detail::archive_files(config_, invalid);
    if (!archived.ok) return false;
    disk_states_.fill(SaveCommitDiskState::missing);
    disk_revisions_ = {};
    disk_sizes_ = {};
    disk_formats_ = {};
    disk_checksums_ = {};
    load_state_ = SaveLoadState::empty;
    loaded_index_ = 0xFFU;
    loaded_migrated_ = false;
    loaded_recovered_ = false;
    equal_revision_conflict_ = false;
    return true;
}

void SaveCommitStorage::release_loaded_checkpoints() noexcept {
    for (SaveCommitJobSlot& job : jobs_) {
        checkpoint::clear_save_checkpoint_slot(job.checkpoint);
    }
    loaded_index_ = 0xFFU;
}

SaveCommitWorker::SaveCommitWorker(SaveCommitStorage& storage) noexcept
    : storage_(&storage) {}

SaveCommitWorker::~SaveCommitWorker() { stop_and_join(); }

bool SaveCommitWorker::start() noexcept {
    std::lock_guard<std::mutex> lock{mutex_};
    if (started_ || storage_ == nullptr || !storage_->initialized_
            || storage_->loaded_index_ != kNoSlot
            || storage_->load_state_ == SaveLoadState::blocked
            || storage_->load_state_
                == SaveLoadState::recovery_required) return false;
    const std::uint64_t previous_epoch = epoch_;
    ++epoch_;
    if (epoch_ == 0U) ++epoch_;
    slot_states_.fill(SaveCommitSlotState::free);
    slot_tokens_ = {};
    active_slot_ = kNoSlot;
    queued_slot_ = kNoSlot;
    completion_ = {};
    completion_ready_ = false;
    stopping_ = false;
    const auto rollback_start = [this, previous_epoch]() noexcept {
        epoch_ = previous_epoch;
        started_ = false;
        stopping_ = false;
        active_slot_ = kNoSlot;
        queued_slot_ = kNoSlot;
        completion_ = {};
        completion_ready_ = false;
        slot_states_.fill(SaveCommitSlotState::free);
        slot_tokens_ = {};
    };
    if (hook_failed(storage_->config_, SaveFaultPoint::worker_start)) {
        rollback_start();
        return false;
    }
    try {
        thread_ = std::thread{&SaveCommitWorker::run, this};
        started_ = true;
        return true;
    } catch (...) {
        rollback_start();
        return false;
    }
}

SaveCommitCaptureLease SaveCommitWorker::acquire_capture_slot(
    const std::uint64_t revision, const SaveCommitRequestKind kind,
    const std::uint64_t intent,
    const SaveCommitPayloadKind payload) noexcept {
    std::lock_guard<std::mutex> lock{mutex_};
    if (!started_ || stopping_) {
        return {SaveCommitSubmitState::stopped};
    }
    if (revision == 0U
            || (payload == SaveCommitPayloadKind::verify_durable
                && kind != SaveCommitRequestKind::exact)) return {};
    SaveCommitSubmitState acquisition = SaveCommitSubmitState::accepted;
    std::uint8_t slot = kNoSlot;
    for (std::uint8_t index = 0U; index < kSlotCount; ++index) {
        if (slot_states_[index] == SaveCommitSlotState::free) {
            slot = index;
            break;
        }
    }
    if (slot == kNoSlot && queued_slot_ != kNoSlot) {
        const SaveCommitJobSlot& queued = storage_->jobs_[queued_slot_];
        if (queued.kind == SaveCommitRequestKind::background
                && revision > queued.revision) {
            slot = queued_slot_;
            queued_slot_ = kNoSlot;
            acquisition = SaveCommitSubmitState::superseded_background;
        }
    }
    if (slot == kNoSlot) return {};
    slot_states_[slot] = SaveCommitSlotState::capturing;
    std::uint64_t token = next_token_++;
    if (token == 0U) token = next_token_++;
    slot_tokens_[slot] = token;
    SaveCommitJobSlot& job = storage_->jobs_[slot];
    job.revision = revision;
    job.intent = intent;
    job.kind = kind;
    job.payload = payload;
    return {acquisition, slot, revision, intent, token, epoch_, kind,
        payload};
}

SaveCommitJobSlot* SaveCommitWorker::capture_job(
    const SaveCommitCaptureLease& lease) noexcept {
    std::lock_guard<std::mutex> lock{mutex_};
    if (!started_ || stopping_ || lease.epoch != epoch_
            || lease.job_slot >= kSlotCount
            || slot_states_[lease.job_slot]
                != SaveCommitSlotState::capturing
            || slot_tokens_[lease.job_slot] != lease.token) return nullptr;
    return &storage_->jobs_[lease.job_slot];
}

SaveCommitSubmission SaveCommitWorker::submit(
    const SaveCommitCaptureLease& lease) noexcept {
    std::lock_guard<std::mutex> lock{mutex_};
    if (!started_ || stopping_ || lease.epoch != epoch_) {
        return {SaveCommitSubmitState::stopped};
    }
    if (lease.job_slot >= kSlotCount
            || slot_states_[lease.job_slot]
                != SaveCommitSlotState::capturing
            || slot_tokens_[lease.job_slot] != lease.token
            || storage_->jobs_[lease.job_slot].revision != lease.revision
            || storage_->jobs_[lease.job_slot].intent != lease.intent
            || storage_->jobs_[lease.job_slot].kind != lease.kind
            || storage_->jobs_[lease.job_slot].payload != lease.payload
            || (lease.payload
                    == SaveCommitPayloadKind::captured_checkpoint
                && storage_->jobs_[lease.job_slot]
                    .checkpoint.persistence_revision != lease.revision)
            || queued_slot_ != kNoSlot) {
        return {SaveCommitSubmitState::busy};
    }
    queued_slot_ = lease.job_slot;
    slot_states_[lease.job_slot] = SaveCommitSlotState::queued;
    wake_.notify_all();
    return {lease.state,
        lease.state == SaveCommitSubmitState::superseded_background
            ? lease.job_slot : kNoSlot};
}

void SaveCommitWorker::cancel_capture(
    const SaveCommitCaptureLease& lease) noexcept {
    std::lock_guard<std::mutex> lock{mutex_};
    if (lease.epoch == epoch_ && lease.job_slot < kSlotCount
            && slot_states_[lease.job_slot]
                == SaveCommitSlotState::capturing
            && slot_tokens_[lease.job_slot] == lease.token) {
        slot_states_[lease.job_slot] = SaveCommitSlotState::free;
        slot_tokens_[lease.job_slot] = 0U;
    }
}

bool SaveCommitWorker::try_take_completion(
    SaveCommitCompletion& completion) noexcept {
    std::lock_guard<std::mutex> lock{mutex_};
    if (!completion_ready_) return false;
    completion = completion_;
    if (completion.job_slot < kSlotCount
            && slot_states_[completion.job_slot]
                == SaveCommitSlotState::completion_ready
            && slot_tokens_[completion.job_slot] == completion.token) {
        slot_states_[completion.job_slot] = SaveCommitSlotState::free;
        slot_tokens_[completion.job_slot] = 0U;
    }
    completion_ready_ = false;
    wake_.notify_all();
    return true;
}

void SaveCommitWorker::stop_and_join() noexcept {
    {
        std::lock_guard<std::mutex> lock{mutex_};
        if (!started_) return;
        stopping_ = true;
        if (queued_slot_ < kSlotCount) {
            slot_states_[queued_slot_] = SaveCommitSlotState::free;
            slot_tokens_[queued_slot_] = 0U;
        }
        queued_slot_ = kNoSlot;
        completion_ready_ = false;
        wake_.notify_all();
    }
    if (thread_.joinable()) thread_.join();
    std::lock_guard<std::mutex> lock{mutex_};
    started_ = false;
    active_slot_ = kNoSlot;
    queued_slot_ = kNoSlot;
    completion_ready_ = false;
    completion_ = {};
    slot_states_.fill(SaveCommitSlotState::free);
    slot_tokens_ = {};
}

bool SaveCommitWorker::idle() const noexcept {
    std::lock_guard<std::mutex> lock{mutex_};
    return started_ && active_slot_ == kNoSlot && queued_slot_ == kNoSlot
        && !completion_ready_;
}

void SaveCommitWorker::run() noexcept {
    std::unique_lock<std::mutex> lock{mutex_};
    for (;;) {
        wake_.wait(lock, [this] {
            return stopping_ || (queued_slot_ != kNoSlot
                && !completion_ready_);
        });
        if (stopping_) return;
        active_slot_ = queued_slot_;
        queued_slot_ = kNoSlot;
        const std::uint8_t slot = active_slot_;
        slot_states_[slot] = SaveCommitSlotState::active;
        const std::uint64_t revision = storage_->jobs_[slot].revision;
        const std::uint64_t intent = storage_->jobs_[slot].intent;
        const SaveCommitRequestKind kind = storage_->jobs_[slot].kind;
        const std::uint64_t token = slot_tokens_[slot];
        const std::uint64_t epoch = epoch_;
        lock.unlock();
        const SaveCommitExactResult result = commit(slot);
        lock.lock();
        if (!stopping_ && epoch == epoch_) {
            completion_ = {slot, revision, kind, intent, token, epoch, result};
            completion_ready_ = true;
            slot_states_[slot] = SaveCommitSlotState::completion_ready;
        } else {
            slot_states_[slot] = SaveCommitSlotState::free;
            slot_tokens_[slot] = 0U;
        }
        active_slot_ = kNoSlot;
        wake_.notify_all();
    }
}

SaveCommitExactResult SaveCommitWorker::commit(
    const std::uint8_t job_slot) noexcept {
    SaveCommitExactResult result{};
    result.state = SaveCommitState::not_committed;
    result.error = SaveError::write_failed;
    SaveCommitJobSlot& job = storage_->jobs_[job_slot];
    if (storage_->disk_states_[0U] == SaveCommitDiskState::unavailable
            || storage_->disk_states_[1U]
                == SaveCommitDiskState::unavailable) {
        result.error = SaveError::read_failed;
        return result;
    }
    const bool valid_a =
        storage_->disk_states_[0U] == SaveCommitDiskState::valid;
    const bool valid_b =
        storage_->disk_states_[1U] == SaveCommitDiskState::valid;
    const std::size_t active = !valid_a && !valid_b ? 1U
        : !valid_b || (valid_a && storage_->disk_revisions_[0U]
            >= storage_->disk_revisions_[1U]) ? 0U : 1U;
    const std::uint64_t highest_revision = !valid_a && !valid_b ? 0U
        : storage_->disk_revisions_[active];
    result.active_slot = !valid_a && !valid_b ? SaveSlot::none
        : active == 0U ? SaveSlot::a : SaveSlot::b;
    if (job.revision < highest_revision) {
        result.error = SaveError::invalid_checkpoint;
        return result;
    }

    // Between commits the two fixed buffers are byte-exact mirrors of the
    // last fully decoded/validated A/B images. Successful publication replaces
    // only the target mirror with the canonical current image. This preserves
    // a collision-free proof for legacy, V9, and V10 slots without a third
    // large slot.
    const auto record_elapsed = [](std::uint64_t& destination,
                                    const auto begin) noexcept {
        const auto elapsed = std::chrono::duration_cast<
            std::chrono::nanoseconds>(
                std::chrono::steady_clock::now() - begin).count();
        if (elapsed > 0) {
            destination += static_cast<std::uint64_t>(elapsed);
        }
    };
    const auto measured_compare_file = [&](const std::filesystem::path& path,
                                            const std::uint8_t* expected,
                                            const std::size_t expected_size)
                                            noexcept {
        const auto begin = std::chrono::steady_clock::now();
        const FileIoResult compared = compare_file(
            path, expected, expected_size);
        record_elapsed(result.cost.file_readback_ns, begin);
        ++result.cost.readback_operations;
        return compared;
    };
    const auto disk_matches_cache = [&](const std::size_t index,
                                        const bool replace_invalid) noexcept {
        const SaveCommitDiskState expected = storage_->disk_states_[index];
        if (expected == SaveCommitDiskState::invalid && replace_invalid) {
            return true;
        }
        const SaveCommitDiskState actual =
            disk_file_state(storage_->slot_paths_[index]);
        if (actual != expected) return false;
        if (expected == SaveCommitDiskState::missing) return true;
        return expected == SaveCommitDiskState::valid
            && measured_compare_file(storage_->slot_paths_[index],
                storage_->buffers_[index].data(),
                storage_->disk_sizes_[index]) == FileIoResult::ok;
    };

    const auto final_scan = [&](const std::array<SaveCommitDiskState,
            kSlotCount>& expected_states,
            const std::array<std::uint64_t, kSlotCount>& expected_revisions,
            const std::array<std::size_t, kSlotCount>& expected_sizes,
            SaveError& failure) noexcept {
        for (std::size_t index = 0U; index < kSlotCount; ++index) {
            const SaveFaultPoint point = index == 0U
                ? SaveFaultPoint::final_scan_a : SaveFaultPoint::final_scan_b;
            if (hook_failed(storage_->config_, point)) {
                failure = SaveError::final_scan_failed;
                return false;
            }
            const SaveCommitDiskState actual =
                disk_file_state(storage_->slot_paths_[index]);
            if (expected_states[index] == SaveCommitDiskState::missing) {
                if (actual != SaveCommitDiskState::missing) {
                    failure = SaveError::final_scan_failed;
                    return false;
                }
                continue;
            }
            if (expected_states[index] != SaveCommitDiskState::valid
                    || actual != SaveCommitDiskState::valid
                    || measured_compare_file(storage_->slot_paths_[index],
                        storage_->buffers_[index].data(),
                        expected_sizes[index]) != FileIoResult::ok) {
                failure = SaveError::final_scan_failed;
                return false;
            }
        }
        const bool final_a =
            expected_states[0U] == SaveCommitDiskState::valid;
        const bool final_b =
            expected_states[1U] == SaveCommitDiskState::valid;
        if ((final_a && expected_revisions[0U] > job.revision)
                || (final_b && expected_revisions[1U] > job.revision)
                || (final_a && final_b
                    && expected_revisions[0U] == expected_revisions[1U]
                    && (expected_sizes[0U] != expected_sizes[1U]
                        || !std::equal(storage_->buffers_[0U].begin(),
                            storage_->buffers_[0U].begin()
                                + expected_sizes[0U],
                            storage_->buffers_[1U].begin())))) {
            failure = SaveError::conflicting_slots;
            return false;
        }
        return true;
    };
    if (job.revision == highest_revision) {
        std::uint64_t verified_revision{};
        const bool payload_verified = job.payload
                == SaveCommitPayloadKind::verify_durable
            ? inspect_checkpoint_latest_envelope(
                storage_->buffers_[active].data(),
                storage_->disk_sizes_[active], verified_revision)
                    == CodecError::none
                && verified_revision == job.revision
            : verify_checkpoint_latest_readback(
                storage_->buffers_[active].data(),
                storage_->disk_sizes_[active], job.checkpoint,
                storage_->buffers_[active].data(),
                storage_->disk_sizes_[active]) == CodecError::none;
        if (job.kind != SaveCommitRequestKind::exact
                || !disk_matches_cache(0U, false)
                || !disk_matches_cache(1U, false)
                || (storage_->disk_formats_[active]
                        != kCheckpointFormatVersionV9
                    && storage_->disk_formats_[active]
                        != kCheckpointFormatVersionV10)
                || !payload_verified) {
            result.error = SaveError::invalid_checkpoint;
            return result;
        }
        SaveError scan_error{SaveError::none};
        if (!final_scan(storage_->disk_states_, storage_->disk_revisions_,
                storage_->disk_sizes_, scan_error)) {
            result.state = SaveCommitState::indeterminate;
            result.error = scan_error;
            return result;
        }
        result.state = SaveCommitState::committed;
        result.error = SaveError::none;
        return result;
    }
    if (job.payload == SaveCommitPayloadKind::verify_durable) {
        result.error = SaveError::invalid_checkpoint;
        return result;
    }
    const std::size_t target = 1U - active;
    if (!disk_matches_cache(active, false)
            || !disk_matches_cache(target, true)) {
        result.error = SaveError::read_failed;
        return result;
    }
    const SaveCommitDiskState previous_target_state =
        storage_->disk_states_[target];
    std::uint8_t* const encoded = storage_->buffers_[target].data();
    std::size_t encoded_size{};
    const auto encode_begin = std::chrono::steady_clock::now();
    const CodecError encoded_error = encode_checkpoint_v10_into(
        job.checkpoint, encoded, kMaximumEncodedCheckpointBytes, encoded_size);
    record_elapsed(result.cost.encode_ns, encode_begin);
    if (encoded_error != CodecError::none) {
        if (previous_target_state == SaveCommitDiskState::valid) {
            storage_->disk_states_[target] = SaveCommitDiskState::invalid;
            storage_->disk_revisions_[target] = 0U;
            storage_->disk_sizes_[target] = 0U;
            storage_->disk_formats_[target] = 0U;
            storage_->disk_checksums_[target] = 0U;
        }
        result.error = SaveError::invalid_checkpoint;
        return result;
    }
    result.cost.encoded_bytes = encoded_size;
    const std::uint32_t encoded_checksum =
        crc32_update(0U, encoded, encoded_size);
    const auto discard_replaced_cache = [&]() noexcept {
        if (previous_target_state != SaveCommitDiskState::valid) return;
        storage_->disk_states_[target] = SaveCommitDiskState::invalid;
        storage_->disk_revisions_[target] = 0U;
        storage_->disk_sizes_[target] = 0U;
        storage_->disk_formats_[target] = 0U;
        storage_->disk_checksums_[target] = 0U;
    };
    if (!remove_file(storage_->temp_paths_[target])) {
        discard_replaced_cache();
        result.error = SaveError::cleanup_failed;
        return result;
    }
    if (hook_failed(storage_->config_, SaveFaultPoint::before_temp_write)) {
        discard_replaced_cache();
        return result;
    }
    const auto write_begin = std::chrono::steady_clock::now();
    const FileIoResult write = write_file(storage_->temp_paths_[target],
        encoded, encoded_size, storage_->config_);
    record_elapsed(result.cost.durable_write_ns, write_begin);
    ++result.cost.write_operations;
    if (write != FileIoResult::ok) {
        discard_replaced_cache();
        result.error = write == FileIoResult::flush_failed
            ? SaveError::flush_failed
            : write == FileIoResult::close_failed
                ? SaveError::close_failed : SaveError::write_failed;
        return result;
    }
    if (hook_failed(storage_->config_, SaveFaultPoint::after_temp_write)) {
        if (!remove_file(storage_->temp_paths_[target])) {
            result.error = SaveError::cleanup_failed;
        }
        discard_replaced_cache();
        return result;
    }
    if (hook_failed(storage_->config_, SaveFaultPoint::temp_readback)
            || measured_compare_file(storage_->temp_paths_[target], encoded,
                encoded_size) != FileIoResult::ok) {
        if (!remove_file(storage_->temp_paths_[target])) {
            discard_replaced_cache();
            result.error = SaveError::cleanup_failed;
            return result;
        }
        discard_replaced_cache();
        result.error = SaveError::readback_failed;
        return result;
    }
    // The source checkpoint was structurally validated by the canonical
    // encoder.  Byte-for-byte readback therefore proves both the exact field
    // image and its decodability without mutating the immutable job slot.
    bool publish_failed = hook_failed(
        storage_->config_, SaveFaultPoint::after_temp_validation)
        || hook_failed(storage_->config_, SaveFaultPoint::before_publish);
    if (!publish_failed) {
        const auto publish_begin = std::chrono::steady_clock::now();
        const bool published = publish_file(storage_->temp_paths_[target],
            storage_->slot_paths_[target]);
        record_elapsed(result.cost.durable_write_ns, publish_begin);
        ++result.cost.write_operations;
        publish_failed = !published;
    }
    if (publish_failed) {
        if (!remove_file(storage_->temp_paths_[target])) {
            discard_replaced_cache();
            result.error = SaveError::cleanup_failed;
            return result;
        }
        discard_replaced_cache();
        result.error = SaveError::publish_failed;
        return result;
    }
    result.active_slot = target == 0U ? SaveSlot::a : SaveSlot::b;
    if (hook_failed(storage_->config_, SaveFaultPoint::after_publish)) {
        result.state = SaveCommitState::indeterminate;
        result.error = SaveError::publish_failed;
        return result;
    }
    if (measured_compare_file(storage_->slot_paths_[target], encoded,
            encoded_size) != FileIoResult::ok) {
        result.state = SaveCommitState::indeterminate;
        result.error = SaveError::final_scan_failed;
        return result;
    }
    std::array<SaveCommitDiskState, kSlotCount> final_states =
        storage_->disk_states_;
    std::array<std::uint64_t, kSlotCount> final_revisions =
        storage_->disk_revisions_;
    std::array<std::size_t, kSlotCount> final_sizes =
        storage_->disk_sizes_;
    std::array<std::uint32_t, kSlotCount> final_formats =
        storage_->disk_formats_;
    std::array<std::uint32_t, kSlotCount> final_checksums =
        storage_->disk_checksums_;
    final_states[target] = SaveCommitDiskState::valid;
    final_revisions[target] = job.revision;
    final_sizes[target] = encoded_size;
    final_formats[target] = kCheckpointFormatVersionV10;
    final_checksums[target] = encoded_checksum;
    SaveError scan_error{SaveError::none};
    if (!final_scan(final_states, final_revisions, final_sizes, scan_error)) {
        result.state = SaveCommitState::indeterminate;
        result.error = scan_error;
        return result;
    }
    storage_->disk_states_ = final_states;
    storage_->disk_revisions_ = final_revisions;
    storage_->disk_sizes_ = final_sizes;
    storage_->disk_formats_ = final_formats;
    storage_->disk_checksums_ = final_checksums;
    result.state = SaveCommitState::committed;
    result.error = SaveError::none;
    return result;
}

}  // namespace arpg::persistence
