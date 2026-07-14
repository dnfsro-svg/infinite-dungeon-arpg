#include "persistence/save_store_detail.hpp"

#include <chrono>
#include <string>
#include <system_error>
#include <utility>

namespace arpg::persistence::detail {
namespace {

std::uint64_t archive_stamp(const SaveStoreConfig& config) noexcept {
    try {
        if (config.stamp_provider != nullptr) {
            return config.stamp_provider(config.stamp_context);
        }
        return static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count());
    } catch (...) {
        return 0U;
    }
}

}  // namespace

void remove_temps(const SaveStoreConfig& config, ScanResult& scan) noexcept {
    try {
        if (scan.temp_a) {
            std::error_code error;
            std::filesystem::remove(config.directory / temp_name(SaveSlot::a),
                error);
            if (error) {
                scan.directory_error = true;
            }
        }
        if (scan.temp_b) {
            std::error_code error;
            std::filesystem::remove(config.directory / temp_name(SaveSlot::b),
                error);
            if (error) {
                scan.directory_error = true;
            }
        }
    } catch (...) {
        scan.directory_error = true;
    }
}

SaveLoadResult ready_result(const ScanResult& scan, SaveSlot slot,
    bool recovered) noexcept {
    SaveLoadResult result{};
    result.state = SaveLoadState::ready;
    result.active_slot = slot;
    result.recovered = recovered;
    result.checkpoint = slot == SaveSlot::a ? scan.a.checkpoint : scan.b.checkpoint;
    return result;
}

SaveCommitResult commit_failure(SaveCommitState state, SaveError error,
    SaveSlot slot) noexcept {
    SaveCommitResult result{};
    result.state = state;
    result.error = error;
    result.active_slot = slot;
    return result;
}

std::vector<std::filesystem::path> invalid_files(
    const SaveStoreConfig& config, const ScanResult& scan,
    bool include_temps) {
    std::vector<std::filesystem::path> files;
    if (scan.a.state == SlotFileState::invalid
            || scan.a.state == SlotFileState::unavailable) {
        files.push_back(config.directory / slot_name(SaveSlot::a));
    }
    if (scan.b.state == SlotFileState::invalid
            || scan.b.state == SlotFileState::unavailable) {
        files.push_back(config.directory / slot_name(SaveSlot::b));
    }
    if (include_temps) {
        if (scan.temp_a) {
            files.push_back(config.directory / temp_name(SaveSlot::a));
        }
        if (scan.temp_b) {
            files.push_back(config.directory / temp_name(SaveSlot::b));
        }
    }
    return files;
}

ArchiveResult archive_files(const SaveStoreConfig& config,
    const std::vector<std::filesystem::path>& files) noexcept {
    if (files.empty()) {
        return {true, SaveError::none};
    }
    struct MovedFile final {
        std::filesystem::path source{};
        std::filesystem::path destination{};
    };
    std::vector<MovedFile> moved;
    try {
        moved.reserve(files.size());
        const auto stamp = archive_stamp(config);
        std::size_t suffix = 0U;
        for (const auto& source : files) {
            std::filesystem::path destination;
            do {
                destination = config.directory
                    / (source.filename().string() + ".corrupt."
                        + std::to_string(stamp)
                        + (suffix == 0U ? "" : "." + std::to_string(suffix)));
                ++suffix;
            } while (std::filesystem::exists(destination));
            if (hook_failed(config, SaveFaultPoint::before_archive)) {
                std::error_code rollback_error;
                for (auto it = moved.rbegin(); it != moved.rend(); ++it) {
                    std::filesystem::rename(it->destination, it->source,
                        rollback_error);
                }
                return {false, SaveError::archive_failed};
            }
            moved.push_back({source, destination});
            std::error_code error;
            std::filesystem::rename(source, destination, error);
            if (error) {
                moved.pop_back();
                std::error_code rollback_error;
                for (auto it = moved.rbegin(); it != moved.rend(); ++it) {
                    std::filesystem::rename(it->destination, it->source,
                        rollback_error);
                }
                return {false, SaveError::archive_failed};
            }
        }
    } catch (...) {
        std::error_code rollback_error;
        for (auto it = moved.rbegin(); it != moved.rend(); ++it) {
            std::filesystem::rename(it->destination, it->source,
                rollback_error);
        }
        return {false, SaveError::archive_failed};
    }
    return {true, SaveError::none};
}

SaveLoadResult load_recovered(const SaveStoreConfig& config) noexcept {
    try {
        if (config.directory.empty()) {
            return {SaveLoadState::blocked, SaveError::directory_unavailable,
                SaveSlot::none, false, {}};
        }
        auto scan = scan_directory(config, false);
        if (scan.directory_error) {
            return {SaveLoadState::blocked, SaveError::directory_unavailable,
                SaveSlot::none, false, {}};
        }
        if (scan.a.state == SlotFileState::valid
                && scan.b.state == SlotFileState::valid) {
            if (scan.a.checkpoint.commit_generation
                    == scan.b.checkpoint.commit_generation
                && !same_state(scan.a.checkpoint, scan.b.checkpoint)) {
                return {SaveLoadState::recovery_required,
                    SaveError::conflicting_slots, SaveSlot::none, false, {}};
            }
            remove_temps(config, scan);
            if (scan.directory_error) {
                return {SaveLoadState::blocked,
                    SaveError::directory_unavailable, SaveSlot::none, false, {}};
            }
            return ready_result(scan, highest_slot(scan));
        }

        const auto valid_count = (scan.a.state == SlotFileState::valid ? 1 : 0)
            + (scan.b.state == SlotFileState::valid ? 1 : 0);
        if (valid_count == 1) {
            const auto invalid = invalid_files(config, scan, false);
            if (!invalid.empty()) {
                const auto archive = archive_files(config, invalid);
                if (!archive.ok) {
                    return {SaveLoadState::blocked, archive.error,
                        SaveSlot::none, false, {}};
                }
                scan = scan_directory(config, false);
                if (scan.directory_error) {
                    return {SaveLoadState::blocked,
                        SaveError::directory_unavailable, SaveSlot::none, false, {}};
                }
                remove_temps(config, scan);
                if (scan.directory_error) {
                    return {SaveLoadState::blocked,
                        SaveError::directory_unavailable, SaveSlot::none, false, {}};
                }
                return ready_result(scan, highest_slot(scan), true);
            }
            remove_temps(config, scan);
            if (scan.directory_error) {
                return {SaveLoadState::blocked,
                    SaveError::directory_unavailable, SaveSlot::none, false, {}};
            }
            return ready_result(scan, highest_slot(scan));
        }
        if (!scan.any_file) {
            return {SaveLoadState::empty, SaveError::none,
                SaveSlot::none, false, {}};
        }
        return {SaveLoadState::recovery_required, SaveError::read_failed,
            SaveSlot::none, false, {}};
    } catch (...) {
        return {SaveLoadState::blocked, SaveError::read_failed,
            SaveSlot::none, false, {}};
    }
}

SaveCommitResult verify_published_transaction(const SaveStoreConfig& config,
    SaveSlot active, SaveSlot target,
    const checkpoint::DungeonRunState& expected) noexcept {
    const auto final_scan = scan_directory(config, true);
    const auto& target_info = target == SaveSlot::a
        ? final_scan.a : final_scan.b;
    const auto& other_info = target == SaveSlot::a
        ? final_scan.b : final_scan.a;
    const auto other_slot = target == SaveSlot::a
        ? SaveSlot::b : SaveSlot::a;
    if (final_scan.directory_error || final_scan.faulted
            || target_info.state == SlotFileState::unavailable
            || other_info.state == SlotFileState::unavailable) {
        return commit_failure(SaveCommitState::indeterminate,
            SaveError::final_scan_failed, target);
    }
    if (target_info.state == SlotFileState::valid
            && same_state(target_info.checkpoint, expected)) {
        if (other_info.state == SlotFileState::valid) {
            if (other_info.checkpoint.commit_generation
                    > expected.commit_generation
                || (other_info.checkpoint.commit_generation
                        == expected.commit_generation
                    && !same_state(other_info.checkpoint, expected))) {
                return commit_failure(SaveCommitState::indeterminate,
                    SaveError::final_scan_failed, target);
            }
        }
        SaveCommitResult result{};
        result.state = SaveCommitState::committed;
        result.active_slot = target;
        result.verified_state = target_info.checkpoint;
        return result;
    }
    if (target_info.state == SlotFileState::invalid
            && other_info.state == SlotFileState::valid
            && active == other_slot) {
        return commit_failure(SaveCommitState::not_committed,
            SaveError::final_scan_failed, active);
    }
    return commit_failure(SaveCommitState::indeterminate,
        SaveError::final_scan_failed, target);
}

}  // namespace arpg::persistence::detail
