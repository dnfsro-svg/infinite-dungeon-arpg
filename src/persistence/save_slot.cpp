#include "persistence/save_store_detail.hpp"

#include "persistence/checkpoint_codec.hpp"

#include <fstream>
#include <iterator>
#include <system_error>
#include <vector>

namespace arpg::persistence::detail {

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
        && lhs.progression.level == rhs.progression.level
        && lhs.progression.experience == rhs.progression.experience
        && lhs.progression.earned_passive_points
            == rhs.progression.earned_passive_points
        && lhs.progression.unspent_passive_points
            == rhs.progression.unspent_passive_points
        && lhs.passive_tree.allocated_bits == rhs.passive_tree.allocated_bits
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
        std::error_code directory_exists_error;
        if (!std::filesystem::exists(config.directory,
                directory_exists_error)) {
            if (directory_exists_error) {
                result.directory_error = true;
            }
            return result;
        }
        std::error_code directory_type_error;
        if (!std::filesystem::is_directory(config.directory,
                directory_type_error)
            || directory_type_error) {
            result.directory_error = true;
            return result;
        }

        const auto a_path = config.directory / slot_name(SaveSlot::a);
        const auto b_path = config.directory / slot_name(SaveSlot::b);
        const auto a_tmp = config.directory / temp_name(SaveSlot::a);
        const auto b_tmp = config.directory / temp_name(SaveSlot::b);
        const auto probe_exists = [&result](
            const std::filesystem::path& path) {
            std::error_code path_error;
            const auto present = std::filesystem::exists(path, path_error);
            if (path_error) {
                result.directory_error = true;
            }
            return present;
        };
        const auto slot_a_exists = probe_exists(a_path);
        const auto slot_b_exists = probe_exists(b_path);
        result.temp_a = probe_exists(a_tmp);
        result.temp_b = probe_exists(b_tmp);
        result.any_file = slot_a_exists || slot_b_exists
            || result.temp_a || result.temp_b;
        if (result.directory_error) {
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

}  // namespace arpg::persistence::detail
