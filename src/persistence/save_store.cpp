#include "persistence/save_store.hpp"

#include "persistence/checkpoint_codec.hpp"
#include "persistence/save_paths.hpp"
#include "persistence/save_store_detail.hpp"

#include <system_error>
#include <utility>

namespace arpg::persistence {
namespace {

namespace checkpoint = arpg::dungeon::checkpoint;

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
    return detail::load_recovered(config_);
}

SaveCommitResult SaveStore::commit(
    const checkpoint::DungeonRunState& expected) noexcept {
    try {
        const auto encoded_state = encode_checkpoint(expected);
        if (!encoded_state.has_value()) {
            return detail::commit_failure(SaveCommitState::not_committed,
                SaveError::invalid_checkpoint);
        }
        if (config_.directory.empty()) {
            return detail::commit_failure(SaveCommitState::not_committed,
                SaveError::directory_unavailable);
        }
        std::error_code directory_error;
        std::filesystem::create_directories(config_.directory, directory_error);
        if (directory_error) {
            return detail::commit_failure(SaveCommitState::not_committed,
                SaveError::directory_unavailable);
        }

        const auto loaded = load();
        if (loaded.state == SaveLoadState::blocked
                || loaded.state == SaveLoadState::recovery_required) {
            return detail::commit_failure(SaveCommitState::not_committed,
                loaded.error == SaveError::none
                    ? SaveError::read_failed : loaded.error,
                loaded.active_slot);
        }
        const auto active = loaded.active_slot;
        const auto target = active == SaveSlot::a ? SaveSlot::b : SaveSlot::a;
        const auto transaction = detail::write_transaction(
            config_, active, expected, *encoded_state);
        if (!transaction.needs_final_scan) {
            return transaction.result;
        }
        return detail::verify_published_transaction(
            config_, active, target, expected);
    } catch (...) {
        return detail::commit_failure(SaveCommitState::not_committed,
            SaveError::publish_failed, SaveSlot::none);
    }
}

SaveLoadResult SaveStore::archive_invalid_and_create(
    const checkpoint::DungeonRunState& initial) noexcept {
    try {
        const auto bytes = encode_checkpoint(initial);
        if (!bytes.has_value()) {
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
        const auto scan = detail::scan_directory(config_, false);
        if (scan.directory_error) {
            return {SaveLoadState::blocked, SaveError::directory_unavailable,
                SaveSlot::none, false, {}};
        }
        const auto files = detail::invalid_files(config_, scan, true);
        auto files_to_archive = files;
        const bool conflicting_slots = scan.a.state == detail::SlotFileState::valid
            && scan.b.state == detail::SlotFileState::valid
            && scan.a.persistence_revision == scan.b.persistence_revision
            && !detail::same_slot_checkpoint(scan.a, scan.b);
        if (conflicting_slots) {
            files_to_archive.push_back(
                config_.directory / detail::slot_name(SaveSlot::a));
            files_to_archive.push_back(
                config_.directory / detail::slot_name(SaveSlot::b));
        }
        const auto archive = detail::archive_files(config_, files_to_archive);
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
