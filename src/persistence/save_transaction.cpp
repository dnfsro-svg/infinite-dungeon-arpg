#include "persistence/save_store_detail.hpp"

#include <fstream>
#include <system_error>

namespace arpg::persistence::detail {

TransactionResult write_transaction(const SaveStoreConfig& config,
    SaveSlot active, const checkpoint::DungeonRunState& expected,
    const std::array<std::uint8_t, kEncodedCheckpointSize>& encoded_state) noexcept {
    bool published = false;
    const auto target = active == SaveSlot::a ? SaveSlot::b : SaveSlot::a;
    try {
        const auto target_path = config.directory / slot_name(target);
        const auto temp_path = config.directory / temp_name(target);

        std::error_code error;
        std::filesystem::remove(temp_path, error);
        if (hook_failed(config, SaveFaultPoint::before_temp_write)) {
            return {false, commit_failure(SaveCommitState::not_committed,
                SaveError::write_failed, active)};
        }

        {
            std::ofstream output(temp_path, std::ios::binary | std::ios::trunc);
            if (!output) {
                return {false, commit_failure(SaveCommitState::not_committed,
                    SaveError::write_failed, active)};
            }
            output.write(reinterpret_cast<const char*>(encoded_state.data()),
                static_cast<std::streamsize>(encoded_state.size()));
            if (!output) {
                std::filesystem::remove(temp_path, error);
                return {false, commit_failure(SaveCommitState::not_committed,
                    SaveError::write_failed, active)};
            }
            output.flush();
            if (!output) {
                std::filesystem::remove(temp_path, error);
                return {false, commit_failure(SaveCommitState::not_committed,
                    SaveError::flush_failed, active)};
            }
        }
        if (hook_failed(config, SaveFaultPoint::after_temp_write)) {
            std::filesystem::remove(temp_path, error);
            return {false, commit_failure(SaveCommitState::not_committed,
                SaveError::write_failed, active)};
        }

        const auto validated = read_slot(temp_path);
        if (validated.state != SlotFileState::valid
                || !same_state(validated.checkpoint, expected)) {
            std::filesystem::remove(temp_path, error);
            return {false, commit_failure(SaveCommitState::not_committed,
                SaveError::read_failed, active)};
        }
        if (hook_failed(config, SaveFaultPoint::after_temp_validation)
                || hook_failed(config, SaveFaultPoint::before_publish)) {
            std::filesystem::remove(temp_path, error);
            return {false, commit_failure(SaveCommitState::not_committed,
                SaveError::publish_failed, active)};
        }

        std::filesystem::remove(target_path, error);
        if (error) {
            std::filesystem::remove(temp_path, error);
            return {false, commit_failure(SaveCommitState::not_committed,
                SaveError::publish_failed, active)};
        }
        std::filesystem::rename(temp_path, target_path, error);
        if (error) {
            std::filesystem::remove(temp_path, error);
            return {false, commit_failure(SaveCommitState::not_committed,
                SaveError::publish_failed, active)};
        }
        published = true;
        if (hook_failed(config, SaveFaultPoint::after_publish)) {
            return {false, commit_failure(SaveCommitState::indeterminate,
                SaveError::publish_failed, target)};
        }
        return {true, {}};
    } catch (...) {
        return {false, commit_failure(
            published ? SaveCommitState::indeterminate
                      : SaveCommitState::not_committed,
            published ? SaveError::final_scan_failed
                      : SaveError::publish_failed,
            published ? target : SaveSlot::none)};
    }
}

}  // namespace arpg::persistence::detail
