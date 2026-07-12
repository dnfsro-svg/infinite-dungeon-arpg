#include "persistence/save_store.hpp"

#include "persistence/checkpoint_codec.hpp"
#include "persistence/save_paths.hpp"

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <string>
#include <system_error>
#include <vector>

namespace arpg::persistence {
namespace {

namespace checkpoint = arpg::dungeon::checkpoint;

enum class SlotFileState : std::uint8_t { missing, valid, invalid, unavailable };

struct SlotInfo final {
    SlotFileState state{SlotFileState::missing};
    SaveError error{SaveError::none};
    checkpoint::DungeonRunState checkpoint{};
};

struct ScanResult final {
    SlotInfo a{};
    SlotInfo b{};
    bool temp_a{};
    bool temp_b{};
    bool any_file{};
    bool directory_error{};
    bool faulted{};
};

struct ArchiveResult final {
    bool ok{};
    SaveError error{SaveError::archive_failed};
};

const std::filesystem::path& slot_name(SaveSlot slot) {
    static const std::filesystem::path a{"run_a.sav"};
    static const std::filesystem::path b{"run_b.sav"};
    return slot == SaveSlot::a ? a : b;
}

const std::filesystem::path& temp_name(SaveSlot slot) {
    static const std::filesystem::path a{"run_a.tmp"};
    static const std::filesystem::path b{"run_b.tmp"};
    return slot == SaveSlot::a ? a : b;
}

SaveFaultPoint final_scan_point(SaveSlot slot) noexcept {
    return slot == SaveSlot::a
        ? SaveFaultPoint::final_scan_a
        : SaveFaultPoint::final_scan_b;
}

bool same_state(const checkpoint::DungeonRunState& lhs,
    const checkpoint::DungeonRunState& rhs) noexcept {
    return lhs.root_seed == rhs.root_seed
        && lhs.commit_generation == rhs.commit_generation
        && lhs.biases == rhs.biases
        && lhs.current_room.index == rhs.current_room.index
        && lhs.current_room.seed == rhs.current_room.seed
        && lhs.current_room.depth == rhs.current_room.depth
        && lhs.current_room.floor_room_index == rhs.current_room.floor_room_index
        && lhs.current_room.entry == rhs.current_room.entry
        && lhs.current_room.ecology == rhs.current_room.ecology
        && lhs.current_room.has_hole == rhs.current_room.has_hole
        && lhs.current_room.is_abyss == rhs.current_room.is_abyss
        && lhs.last_transition == rhs.last_transition
        && lhs.last_direction == rhs.last_direction;
}

SlotInfo read_slot(const std::filesystem::path& path) {
    SlotInfo info{};
    std::error_code error;
    if (!std::filesystem::exists(path, error)) {
        if (error) {
            info.state = SlotFileState::unavailable;
            info.error = SaveError::read_failed;
        }
        return info;
    }

    std::ifstream input(path, std::ios::binary);
    if (!input) {
        info.state = SlotFileState::unavailable;
        info.error = SaveError::read_failed;
        return info;
    }
    std::vector<std::uint8_t> bytes(
        (std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    if (!input.eof() && input.fail()) {
        info.state = SlotFileState::unavailable;
        info.error = SaveError::read_failed;
        return info;
    }
    const auto decoded = decode_checkpoint(bytes.data(), bytes.size());
    if (decoded.error != CodecError::none) {
        info.state = SlotFileState::invalid;
        info.error = SaveError::read_failed;
        return info;
    }
    info.state = SlotFileState::valid;
    info.checkpoint = decoded.state;
    return info;
}

bool hook_failed(const SaveStoreConfig& config,
    SaveFaultPoint point) noexcept {
    return config.fault_hook != nullptr
        && config.fault_hook(point, config.fault_context);
}

ScanResult scan_directory(const SaveStoreConfig& config,
    bool final_scan) noexcept {
    ScanResult result{};
    try {
        std::error_code error;
        if (!std::filesystem::exists(config.directory, error)) {
            if (error) {
                result.directory_error = true;
            }
            return result;
        }
        if (!std::filesystem::is_directory(config.directory, error) || error) {
            result.directory_error = true;
            return result;
        }

        const auto a_path = config.directory / slot_name(SaveSlot::a);
        const auto b_path = config.directory / slot_name(SaveSlot::b);
        const auto a_tmp = config.directory / temp_name(SaveSlot::a);
        const auto b_tmp = config.directory / temp_name(SaveSlot::b);
        result.any_file = std::filesystem::exists(a_path, error)
            || std::filesystem::exists(b_path, error)
            || std::filesystem::exists(a_tmp, error)
            || std::filesystem::exists(b_tmp, error);
        if (error) {
            result.directory_error = true;
            return result;
        }
        result.temp_a = std::filesystem::exists(a_tmp, error);
        result.temp_b = std::filesystem::exists(b_tmp, error);
        if (error) {
            result.directory_error = true;
            return result;
        }

        if (final_scan && hook_failed(config, SaveFaultPoint::final_scan_a)) {
            result.a.state = SlotFileState::unavailable;
            result.a.error = SaveError::final_scan_failed;
            result.faulted = true;
        } else {
            result.a = read_slot(a_path);
        }
        if (final_scan && hook_failed(config, SaveFaultPoint::final_scan_b)) {
            result.b.state = SlotFileState::unavailable;
            result.b.error = SaveError::final_scan_failed;
            result.faulted = true;
        } else {
            result.b = read_slot(b_path);
        }
    } catch (...) {
        result.directory_error = true;
    }
    return result;
}

SaveSlot highest_slot(const ScanResult& scan) noexcept {
    if (scan.a.state != SlotFileState::valid
            && scan.b.state != SlotFileState::valid) {
        return SaveSlot::none;
    }
    if (scan.a.state == SlotFileState::valid
            && scan.b.state != SlotFileState::valid) {
        return SaveSlot::a;
    }
    if (scan.b.state == SlotFileState::valid
            && scan.a.state != SlotFileState::valid) {
        return SaveSlot::b;
    }
    if (scan.a.checkpoint.commit_generation
            > scan.b.checkpoint.commit_generation) {
        return SaveSlot::a;
    }
    if (scan.b.checkpoint.commit_generation
            > scan.a.checkpoint.commit_generation) {
        return SaveSlot::b;
    }
    return same_state(scan.a.checkpoint, scan.b.checkpoint)
        ? SaveSlot::a
        : SaveSlot::none;
}

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
                    / ("corrupt_" + std::to_string(stamp) + "_"
                        + source.filename().string()
                        + (suffix == 0U ? "" : "_" + std::to_string(suffix)));
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

void remove_temps(const SaveStoreConfig& config, const ScanResult& scan) {
    std::error_code error;
    if (scan.temp_a) {
        std::filesystem::remove(config.directory / temp_name(SaveSlot::a), error);
    }
    if (scan.temp_b) {
        std::filesystem::remove(config.directory / temp_name(SaveSlot::b), error);
    }
}

SaveLoadResult ready_result(const ScanResult& scan, SaveSlot slot,
    bool recovered = false) noexcept {
    SaveLoadResult result{};
    result.state = SaveLoadState::ready;
    result.active_slot = slot;
    result.recovered = recovered;
    result.checkpoint = slot == SaveSlot::a ? scan.a.checkpoint : scan.b.checkpoint;
    return result;
}

SaveCommitResult commit_failure(SaveCommitState state, SaveError error,
    SaveSlot slot = SaveSlot::none) noexcept {
    SaveCommitResult result{};
    result.state = state;
    result.error = error;
    result.active_slot = slot;
    return result;
}

}  // namespace

SaveStore::SaveStore(SaveStoreConfig config)
    : config_(std::move(config)) {
    if (config_.directory.empty()) {
        const auto fallback = default_save_directory();
        if (fallback.has_value()) {
            config_.directory = *fallback;
        }
    }
}

SaveLoadResult SaveStore::load() noexcept {
    try {
        if (config_.directory.empty()) {
            return {SaveLoadState::blocked, SaveError::directory_unavailable,
                SaveSlot::none, false, {}};
        }
        auto scan = scan_directory(config_, false);
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
            remove_temps(config_, scan);
            return ready_result(scan, highest_slot(scan));
        }

        const auto valid_count = (scan.a.state == SlotFileState::valid ? 1 : 0)
            + (scan.b.state == SlotFileState::valid ? 1 : 0);
        if (valid_count == 1) {
            const auto invalid = invalid_files(config_, scan, false);
            if (!invalid.empty()) {
                const auto archive = archive_files(config_, invalid);
                if (!archive.ok) {
                    return {SaveLoadState::blocked, archive.error,
                        SaveSlot::none, false, {}};
                }
                scan = scan_directory(config_, false);
                if (scan.directory_error) {
                    return {SaveLoadState::blocked,
                        SaveError::directory_unavailable, SaveSlot::none, false, {}};
                }
                remove_temps(config_, scan);
                return ready_result(scan, highest_slot(scan), true);
            }
            remove_temps(config_, scan);
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

SaveCommitResult SaveStore::commit(
    const checkpoint::DungeonRunState& expected) noexcept {
    bool published = false;
    SaveSlot published_slot = SaveSlot::none;
    try {
        std::array<std::uint8_t, kEncodedCheckpointSize> encoded_state{};
        if (!encode_checkpoint(expected, encoded_state)) {
            return commit_failure(SaveCommitState::not_committed,
                SaveError::invalid_checkpoint);
        }
        if (config_.directory.empty()) {
            return commit_failure(SaveCommitState::not_committed,
                SaveError::directory_unavailable);
        }
        std::error_code directory_error;
        std::filesystem::create_directories(config_.directory, directory_error);
        if (directory_error) {
            return commit_failure(SaveCommitState::not_committed,
                SaveError::directory_unavailable);
        }

        const auto loaded = load();
        if (loaded.state == SaveLoadState::blocked
                || loaded.state == SaveLoadState::recovery_required) {
            return commit_failure(SaveCommitState::not_committed,
                loaded.error == SaveError::none
                    ? SaveError::read_failed : loaded.error,
                loaded.active_slot);
        }
        const auto active = loaded.active_slot;
        const auto target = active == SaveSlot::a ? SaveSlot::b : SaveSlot::a;
        const auto target_path = config_.directory / slot_name(target);
        const auto temp_path = config_.directory / temp_name(target);

        std::error_code error;
        std::filesystem::remove(temp_path, error);
        if (hook_failed(config_, SaveFaultPoint::before_temp_write)) {
            return commit_failure(SaveCommitState::not_committed,
                SaveError::write_failed, active);
        }

        {
            std::ofstream output(temp_path, std::ios::binary | std::ios::trunc);
            if (!output) {
                return commit_failure(SaveCommitState::not_committed,
                    SaveError::write_failed, active);
            }
            output.write(reinterpret_cast<const char*>(encoded_state.data()),
                static_cast<std::streamsize>(encoded_state.size()));
            if (!output) {
                std::filesystem::remove(temp_path, error);
                return commit_failure(SaveCommitState::not_committed,
                    SaveError::write_failed, active);
            }
            output.flush();
            if (!output) {
                std::filesystem::remove(temp_path, error);
                return commit_failure(SaveCommitState::not_committed,
                    SaveError::flush_failed, active);
            }
        }
        if (hook_failed(config_, SaveFaultPoint::after_temp_write)) {
            std::filesystem::remove(temp_path, error);
            return commit_failure(SaveCommitState::not_committed,
                SaveError::write_failed, active);
        }

        const auto validated = read_slot(temp_path);
        if (validated.state != SlotFileState::valid
                || !same_state(validated.checkpoint, expected)) {
            std::filesystem::remove(temp_path, error);
            return commit_failure(SaveCommitState::not_committed,
                SaveError::read_failed, active);
        }
        if (hook_failed(config_, SaveFaultPoint::after_temp_validation)
                || hook_failed(config_, SaveFaultPoint::before_publish)) {
            std::filesystem::remove(temp_path, error);
            return commit_failure(SaveCommitState::not_committed,
                SaveError::publish_failed, active);
        }

        std::filesystem::remove(target_path, error);
        if (error) {
            std::filesystem::remove(temp_path, error);
            return commit_failure(SaveCommitState::not_committed,
                SaveError::publish_failed, active);
        }
        std::filesystem::rename(temp_path, target_path, error);
        if (error) {
            std::filesystem::remove(temp_path, error);
            return commit_failure(SaveCommitState::not_committed,
                SaveError::publish_failed, active);
        }
        published = true;
        published_slot = target;
        if (hook_failed(config_, SaveFaultPoint::after_publish)) {
            return commit_failure(SaveCommitState::indeterminate,
                SaveError::publish_failed, target);
        }

        const auto final_scan = scan_directory(config_, true);
        const auto& target_info = target == SaveSlot::a
            ? final_scan.a : final_scan.b;
        if (target_info.state == SlotFileState::valid
                && same_state(target_info.checkpoint, expected)) {
            const auto& other_info = target == SaveSlot::a
                ? final_scan.b : final_scan.a;
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
        return commit_failure(SaveCommitState::indeterminate,
            SaveError::final_scan_failed, target);
    } catch (...) {
        return commit_failure(
            published ? SaveCommitState::indeterminate
                      : SaveCommitState::not_committed,
            published ? SaveError::final_scan_failed
                      : SaveError::publish_failed,
            published_slot);
    }
}

SaveLoadResult SaveStore::archive_invalid_and_create(
    const checkpoint::DungeonRunState& initial) noexcept {
    try {
        std::array<std::uint8_t, kEncodedCheckpointSize> bytes{};
        if (!encode_checkpoint(initial, bytes)) {
            return {SaveLoadState::blocked, SaveError::invalid_checkpoint,
                SaveSlot::none, false, {}};
        }
        if (config_.directory.empty()) {
            return {SaveLoadState::blocked, SaveError::directory_unavailable,
                SaveSlot::none, false, {}};
        }
        std::error_code error;
        std::filesystem::create_directories(config_.directory, error);
        if (error) {
            return {SaveLoadState::blocked, SaveError::directory_unavailable,
                SaveSlot::none, false, {}};
        }
        const auto scan = scan_directory(config_, false);
        if (scan.directory_error) {
            return {SaveLoadState::blocked, SaveError::directory_unavailable,
                SaveSlot::none, false, {}};
        }
        const auto files = invalid_files(config_, scan, true);
        const auto archive = archive_files(config_, files);
        if (!archive.ok) {
            return {SaveLoadState::blocked, archive.error,
                SaveSlot::none, false, {}};
        }
        const auto commit_result = commit(initial);
        if (commit_result.state == SaveCommitState::committed) {
            SaveLoadResult result{};
            result.state = SaveLoadState::ready;
            result.active_slot = commit_result.active_slot;
            result.checkpoint = commit_result.verified_state;
            result.recovered = true;
            return result;
        }
        return {SaveLoadState::blocked, commit_result.error,
            commit_result.active_slot, true, {}};
    } catch (...) {
        return {SaveLoadState::blocked, SaveError::archive_failed,
            SaveSlot::none, false, {}};
    }
}

}  // namespace arpg::persistence
