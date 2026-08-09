#pragma once

#include "platform/settings/settings_types.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <vector>

namespace arpg::settings {

enum class SettingsLoadStatus : std::uint8_t {
    loaded,
    defaults_missing,
    recovered_single_slot,
    defaults_corrupt
};

enum class SettingsSaveStatus : std::uint8_t {
    committed,
    invalid_settings,
    stale_revision,
    busy,
    storage_conflict,
    revision_overflow,
    write_failed,
    readback_failed
};

enum class SettingsWriteLockStatus : std::uint8_t {
    acquired,
    busy,
    failed
};

struct SettingsWriteLockResult final {
    SettingsWriteLockStatus status{SettingsWriteLockStatus::failed};
    void* token{};
};

struct SettingsLoadResult final {
    SettingsLoadStatus status{SettingsLoadStatus::defaults_missing};
    SettingsData settings{};
};

struct SettingsSaveResult final {
    SettingsSaveStatus status{SettingsSaveStatus::write_failed};
    SettingsData settings{};
};

struct SettingsFileOps final {
    void* context{};
    bool (*read)(void*, const std::filesystem::path&, std::vector<std::uint8_t>&){};
    bool (*replace)(void*, const std::filesystem::path&,
        const std::uint8_t*, std::size_t){};
    SettingsWriteLockResult (*acquire_write_lock)(
        void*, const std::filesystem::path&){};
    void (*release_write_lock)(void*, void*){};
};

[[nodiscard]] SettingsFileOps native_settings_file_ops() noexcept;

class SettingsStore final {
public:
    // An empty directory disables all file I/O: load reports corrupt defaults
    // and save reports write_failed while preserving the caller's committed value.
    explicit SettingsStore(
        std::filesystem::path directory,
        SettingsFileOps file_ops = native_settings_file_ops());

    [[nodiscard]] SettingsLoadResult load() const;
    [[nodiscard]] SettingsSaveResult save(
        const SettingsData& committed, SettingsData draft) const;

private:
    std::filesystem::path directory_{};
    SettingsFileOps file_ops_{};
};

}  // namespace arpg::settings
