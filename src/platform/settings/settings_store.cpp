#include "platform/settings/settings_store.hpp"

#include "platform/settings/settings_codec.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <mutex>
#include <new>
#include <string>
#include <utility>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
#endif

namespace arpg::settings {
namespace {

constexpr const char* slot_a_name = "settings-a.bin";
constexpr const char* slot_b_name = "settings-b.bin";
constexpr const char* write_lock_name = "settings.lock";

std::mutex settings_write_mutex{};

enum class SlotState : std::uint8_t { missing, invalid, valid };

struct SlotRecord final {
    SlotState state{SlotState::missing};
    SettingsData settings{};
};

[[nodiscard]] bool same_settings(
    const SettingsData& lhs, const SettingsData& rhs) noexcept {
    return lhs.master_sfx_percent == rhs.master_sfx_percent &&
        lhs.sfx_percent == rhs.sfx_percent &&
        lhs.music_percent == rhs.music_percent &&
        lhs.ambience_percent == rhs.ambience_percent &&
        lhs.ui_percent == rhs.ui_percent &&
        lhs.window_mode == rhs.window_mode &&
        lhs.vsync_enabled == rhs.vsync_enabled &&
        lhs.loot_filter_mode == rhs.loot_filter_mode &&
        lhs.bindings == rhs.bindings &&
        lhs.revision == rhs.revision;
}

[[nodiscard]] SlotRecord read_slot(
    const SettingsFileOps& ops,
    const std::filesystem::path& path) {
    if (ops.read == nullptr) {
        return {};
    }
    std::vector<std::uint8_t> bytes{};
    if (!ops.read(ops.context, path, bytes)) {
        return {};
    }
    const auto decoded = decode_settings(bytes.data(), bytes.size());
    if (decoded.error != SettingsCodecError::none) {
        return {SlotState::invalid, {}};
    }
    return {SlotState::valid, decoded.settings};
}

[[nodiscard]] bool is_valid(const SlotRecord& slot) noexcept {
    return slot.state == SlotState::valid;
}

[[nodiscard]] SettingsLoadResult select_loaded(
    const SlotRecord& a, const SlotRecord& b) noexcept {
    const bool valid_a = is_valid(a);
    const bool valid_b = is_valid(b);
    if (valid_a && valid_b) {
        if (a.settings.revision == b.settings.revision) {
            if (!same_settings(a.settings, b.settings)) {
                return {SettingsLoadStatus::defaults_corrupt, default_settings()};
            }
            return {SettingsLoadStatus::loaded, a.settings};
        }
        return {SettingsLoadStatus::loaded,
            a.settings.revision > b.settings.revision ? a.settings : b.settings};
    }
    if (valid_a || valid_b) {
        return {SettingsLoadStatus::recovered_single_slot,
            valid_a ? a.settings : b.settings};
    }
    if (a.state == SlotState::missing && b.state == SlotState::missing) {
        return {SettingsLoadStatus::defaults_missing, default_settings()};
    }
    return {SettingsLoadStatus::defaults_corrupt, default_settings()};
}

[[nodiscard]] const char* target_slot_name(
    const SlotRecord& a, const SlotRecord& b) noexcept {
    if (!is_valid(a)) {
        return slot_a_name;
    }
    if (!is_valid(b)) {
        return slot_b_name;
    }
    if (a.settings.revision <= b.settings.revision) {
        return slot_a_name;
    }
    return slot_b_name;
}

class ScopedWriteLock final {
public:
    explicit ScopedWriteLock(const SettingsFileOps& ops) noexcept : ops_(ops) {}

    [[nodiscard]] SettingsWriteLockStatus acquire(
        const std::filesystem::path& directory) {
        if (ops_.acquire_write_lock == nullptr ||
                ops_.release_write_lock == nullptr) {
            return SettingsWriteLockStatus::failed;
        }
        const SettingsWriteLockResult result =
            ops_.acquire_write_lock(ops_.context, directory);
        if (result.status != SettingsWriteLockStatus::acquired) {
            return result.status;
        }
        token_ = result.token;
        return token_ == nullptr
            ? SettingsWriteLockStatus::failed
            : SettingsWriteLockStatus::acquired;
    }

    ~ScopedWriteLock() noexcept {
        if (token_ == nullptr || ops_.release_write_lock == nullptr) {
            return;
        }
        try {
            ops_.release_write_lock(ops_.context, token_);
        } catch (...) {
        }
    }

    ScopedWriteLock(const ScopedWriteLock&) = delete;
    ScopedWriteLock& operator=(const ScopedWriteLock&) = delete;

private:
    SettingsFileOps ops_{};
    void* token_{};
};

bool native_read(
    void*,
    const std::filesystem::path& path,
    std::vector<std::uint8_t>& bytes) {
    std::ifstream stream(path, std::ios::binary | std::ios::ate);
    if (!stream) {
        return false;
    }
    const std::streampos end = stream.tellg();
    if (end < 0) {
        return false;
    }
    bytes.resize(static_cast<std::size_t>(end));
    stream.seekg(0, std::ios::beg);
    if (!bytes.empty()) {
        stream.read(
            reinterpret_cast<char*>(bytes.data()),
            static_cast<std::streamsize>(bytes.size()));
    }
    return static_cast<bool>(stream);
}

[[nodiscard]] bool allowed_slot_path(const std::filesystem::path& path) {
    const auto name = path.filename();
    return name == slot_a_name || name == slot_b_name;
}

#ifdef _WIN32
bool write_and_flush_temp(
    const std::filesystem::path& temp,
    const std::uint8_t* bytes,
    std::size_t size) {
    HANDLE file = CreateFileW(temp.c_str(), GENERIC_WRITE, 0, nullptr,
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return false;
    }
    bool ok = true;
    std::size_t offset = 0U;
    while (ok && offset < size) {
        const std::size_t remaining = size - offset;
        const DWORD request = remaining > static_cast<std::size_t>(MAXDWORD)
            ? MAXDWORD : static_cast<DWORD>(remaining);
        DWORD written = 0U;
        ok = WriteFile(file, bytes + offset, request, &written, nullptr) != FALSE &&
            written == request;
        offset += static_cast<std::size_t>(written);
    }
    if (ok) {
        ok = FlushFileBuffers(file) != FALSE;
    }
    if (CloseHandle(file) == FALSE) {
        ok = false;
    }
    return ok;
}
#endif

bool native_replace(
    void*,
    const std::filesystem::path& path,
    const std::uint8_t* bytes,
    std::size_t size) {
    if (!allowed_slot_path(path) || bytes == nullptr) {
        return false;
    }
    std::error_code directory_error{};
    std::filesystem::create_directories(path.parent_path(), directory_error);
    if (directory_error) {
        return false;
    }
    std::filesystem::path temp = path;
    temp += ".tmp";
#ifdef _WIN32
    if (!write_and_flush_temp(temp, bytes, size)) {
        return false;
    }
    return MoveFileExW(temp.c_str(), path.c_str(),
        MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != FALSE;
#else
    std::ofstream stream(temp, std::ios::binary | std::ios::trunc);
    if (!stream) {
        return false;
    }
    stream.write(reinterpret_cast<const char*>(bytes),
        static_cast<std::streamsize>(size));
    stream.flush();
    if (!stream) {
        return false;
    }
    stream.close();
    if (!stream) {
        return false;
    }
    std::error_code replace_error{};
    std::filesystem::rename(temp, path, replace_error);
    return !replace_error;
#endif
}

SettingsWriteLockResult native_acquire_write_lock(
    void*, const std::filesystem::path& directory) {
    std::error_code directory_error{};
    std::filesystem::create_directories(directory, directory_error);
    if (directory_error) {
        return {SettingsWriteLockStatus::failed, nullptr};
    }
    const std::filesystem::path path = directory / write_lock_name;
#ifdef _WIN32
    HANDLE file = CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return {SettingsWriteLockStatus::failed, nullptr};
    }
    OVERLAPPED overlapped{};
    if (LockFileEx(file,
            LOCKFILE_EXCLUSIVE_LOCK | LOCKFILE_FAIL_IMMEDIATELY,
            0U, 1U, 0U,
            &overlapped) == FALSE) {
        const DWORD error = GetLastError();
        CloseHandle(file);
        return {error == ERROR_LOCK_VIOLATION
                ? SettingsWriteLockStatus::busy
                : SettingsWriteLockStatus::failed,
            nullptr};
    }
    return {SettingsWriteLockStatus::acquired, file};
#else
    const int descriptor = ::open(path.c_str(), O_CREAT | O_RDWR, 0600);
    if (descriptor < 0) {
        return {SettingsWriteLockStatus::failed, nullptr};
    }
    struct flock lock {};
    lock.l_type = F_WRLCK;
    lock.l_whence = SEEK_SET;
    int result = 0;
    do {
        result = ::fcntl(descriptor, F_SETLK, &lock);
    } while (result != 0 && errno == EINTR);
    if (result != 0) {
        const int error = errno;
        static_cast<void>(::close(descriptor));
        return {error == EACCES || error == EAGAIN
                ? SettingsWriteLockStatus::busy
                : SettingsWriteLockStatus::failed,
            nullptr};
    }
    auto* const token = new (std::nothrow) int{descriptor};
    if (token == nullptr) {
        lock.l_type = F_UNLCK;
        static_cast<void>(::fcntl(descriptor, F_SETLK, &lock));
        static_cast<void>(::close(descriptor));
        return {SettingsWriteLockStatus::failed, nullptr};
    }
    return {SettingsWriteLockStatus::acquired, token};
#endif
}

void native_release_write_lock(void*, void* token) {
    if (token == nullptr) {
        return;
    }
#ifdef _WIN32
    HANDLE file = static_cast<HANDLE>(token);
    OVERLAPPED overlapped{};
    static_cast<void>(UnlockFileEx(file, 0U, 1U, 0U, &overlapped));
    static_cast<void>(CloseHandle(file));
#else
    auto* const descriptor = static_cast<int*>(token);
    struct flock lock {};
    lock.l_type = F_UNLCK;
    lock.l_whence = SEEK_SET;
    static_cast<void>(::fcntl(*descriptor, F_SETLK, &lock));
    static_cast<void>(::close(*descriptor));
    delete descriptor;
#endif
}

}  // namespace

SettingsFileOps native_settings_file_ops() noexcept {
    return {nullptr, native_read, native_replace,
        native_acquire_write_lock, native_release_write_lock};
}

SettingsStore::SettingsStore(
    std::filesystem::path directory,
    SettingsFileOps file_ops)
    : directory_(std::move(directory)), file_ops_(file_ops) {}

SettingsLoadResult SettingsStore::load() const {
    if (directory_.empty()) {
        return {SettingsLoadStatus::defaults_corrupt, default_settings()};
    }
    try {
        const SlotRecord a = read_slot(file_ops_, directory_ / slot_a_name);
        const SlotRecord b = read_slot(file_ops_, directory_ / slot_b_name);
        return select_loaded(a, b);
    } catch (...) {
        return {SettingsLoadStatus::defaults_corrupt, default_settings()};
    }
}

SettingsSaveResult SettingsStore::save(
    const SettingsData& committed, SettingsData draft) const {
    if (directory_.empty()) {
        return {SettingsSaveStatus::write_failed, committed};
    }
    if (draft.revision != committed.revision) {
        return {SettingsSaveStatus::stale_revision, committed};
    }
    if (committed.revision == std::numeric_limits<std::uint64_t>::max()) {
        return {SettingsSaveStatus::revision_overflow, committed};
    }
    draft.revision = committed.revision + 1U;
    if (validate_settings(draft) != SettingsValidationError::none) {
        return {SettingsSaveStatus::invalid_settings, committed};
    }

    try {
        std::unique_lock<std::mutex> process_lock(
            settings_write_mutex, std::try_to_lock);
        if (!process_lock.owns_lock()) {
            return {SettingsSaveStatus::busy, committed};
        }
        ScopedWriteLock write_lock{file_ops_};
        const SettingsWriteLockStatus lock_status =
            write_lock.acquire(directory_);
        if (lock_status == SettingsWriteLockStatus::busy) {
            return {SettingsSaveStatus::busy, committed};
        }
        if (lock_status != SettingsWriteLockStatus::acquired) {
            return {SettingsSaveStatus::write_failed, committed};
        }

        std::filesystem::path target{};
        try {
            const SlotRecord a = read_slot(file_ops_, directory_ / slot_a_name);
            const SlotRecord b = read_slot(file_ops_, directory_ / slot_b_name);
            const SettingsLoadResult current = select_loaded(a, b);
            if (current.status == SettingsLoadStatus::defaults_corrupt) {
                return {SettingsSaveStatus::storage_conflict, committed};
            }
            if (!same_settings(current.settings, committed)) {
                return {SettingsSaveStatus::stale_revision, committed};
            }
            target = directory_ / target_slot_name(a, b);
            const auto encoded = encode_settings(draft);
            if (file_ops_.replace == nullptr ||
                    !file_ops_.replace(file_ops_.context, target,
                        encoded.data(), encoded.size())) {
                return {SettingsSaveStatus::write_failed, committed};
            }
        } catch (...) {
            return {SettingsSaveStatus::write_failed, committed};
        }

        try {
            const SlotRecord published = read_slot(file_ops_, target);
            if (!is_valid(published) ||
                    !same_settings(published.settings, draft)) {
                return {SettingsSaveStatus::readback_failed, committed};
            }
            return {SettingsSaveStatus::committed, published.settings};
        } catch (...) {
            return {SettingsSaveStatus::readback_failed, committed};
        }
    } catch (...) {
        return {SettingsSaveStatus::write_failed, committed};
    }
}

}  // namespace arpg::settings
