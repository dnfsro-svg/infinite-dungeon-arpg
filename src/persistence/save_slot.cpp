#include "persistence/save_store_detail.hpp"
#include "persistence/room_progress_codec.hpp"

#include "persistence/checkpoint_codec.hpp"

#include <fstream>
#include <iterator>
#include <memory>
#include <new>
#include <system_error>
#include <utility>
#include <vector>

namespace arpg::persistence::detail {

namespace {

bool same_item(const items::ItemInstance& lhs,
    const items::ItemInstance& rhs) noexcept {
    if (lhs.id != rhs.id || lhs.base_id != rhs.base_id
        || lhs.rarity != rhs.rarity || lhs.item_level != rhs.item_level
        || lhs.required_level != rhs.required_level
        || lhs.affix_count != rhs.affix_count
        || lhs.reserved != rhs.reserved
        || lhs.reinforcement != rhs.reinforcement
        || lhs.extension_reserved != rhs.extension_reserved) {
        return false;
    }
    for (std::size_t index = 0U; index < lhs.affixes.size(); ++index) {
        if (lhs.affixes[index].affix_id != rhs.affixes[index].affix_id
            || lhs.affixes[index].tier != rhs.affixes[index].tier
            || lhs.affixes[index].variant != rhs.affixes[index].variant
            || lhs.affixes[index].value_roll_bp
                != rhs.affixes[index].value_roll_bp) {
            return false;
        }
    }
    return true;
}

bool same_ownership(const items::ItemOwnershipState& lhs,
    const items::ItemOwnershipState& rhs) noexcept {
    if (lhs.items.size() != rhs.items.size()
        || lhs.equipment.equipped_ids != rhs.equipment.equipped_ids
        || lhs.materials != rhs.materials
        || lhs.material_discovery_bits != rhs.material_discovery_bits
        || lhs.material_claimed_drop_bits != rhs.material_claimed_drop_bits
        || lhs.claimed_drop_bits != rhs.claimed_drop_bits
        || lhs.next_item_sequence != rhs.next_item_sequence) {
        return false;
    }
    for (std::size_t index = 0U; index < lhs.items.size(); ++index) {
        if (!same_item(lhs.items[index], rhs.items[index]))
            return false;
    }
    return true;
}

bool same_skill_loadout(const skills::SkillLoadoutState& lhs,
    const skills::SkillLoadoutState& rhs) noexcept {
    if (lhs.owned_active_bits != rhs.owned_active_bits)
        return false;
    for (std::size_t slot = 0U; slot < lhs.slots.size(); ++slot) {
        if (lhs.slots[slot].active != rhs.slots[slot].active
            || lhs.slots[slot].supports != rhs.slots[slot].supports) {
            return false;
        }
    }
    return true;
}

bool same_room(const checkpoint::RoomDescriptor& lhs,
    const checkpoint::RoomDescriptor& rhs) noexcept {
    return lhs.index == rhs.index && lhs.seed == rhs.seed
        && lhs.depth == rhs.depth
        && lhs.floor_room_index == rhs.floor_room_index
        && lhs.entry == rhs.entry && lhs.ecology == rhs.ecology
        && lhs.has_hole == rhs.has_hole && lhs.is_abyss == rhs.is_abyss;
}

bool same_death(const checkpoint::DeathCheckpoint& lhs,
    const checkpoint::DeathCheckpoint& rhs) noexcept {
    return lhs.lifecycle == rhs.lifecycle
        && lhs.data_version == rhs.data_version
        && lhs.death_depth == rhs.death_depth
        && lhs.death_floor_room_index == rhs.death_floor_room_index
        && lhs.death_ecology == rhs.death_ecology
        && lhs.death_was_abyss == rhs.death_was_abyss
        && lhs.source_kind == rhs.source_kind
        && lhs.source_monster_id == rhs.source_monster_id
        && lhs.source_detail_id == rhs.source_detail_id
        && lhs.damage_type == rhs.damage_type
        && lhs.raw_damage == rhs.raw_damage
        && lhs.barrier_loss == rhs.barrier_loss
        && lhs.health_loss == rhs.health_loss
        && lhs.final_damage == rhs.final_damage
        && lhs.recent_damage == rhs.recent_damage
        && lhs.hp == rhs.hp && lhs.max_hp == rhs.max_hp
        && lhs.barrier == rhs.barrier
        && lhs.max_barrier == rhs.max_barrier
        && lhs.armor == rhs.armor && lhs.evasion == rhs.evasion
        && lhs.armor_reduction_bp == rhs.armor_reduction_bp
        && lhs.evasion_rate_bp == rhs.evasion_rate_bp
        && lhs.damage_reduction == rhs.damage_reduction
        && lhs.damage_reduction_cap == rhs.damage_reduction_cap
        && same_room(lhs.target_room, rhs.target_room);
}

}  // namespace

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
        && lhs.abyss.lifecycle == rhs.abyss.lifecycle
        && lhs.abyss.danger == rhs.abyss.danger
        && lhs.abyss.rule == rhs.abyss.rule
        && lhs.abyss.rules_version == rhs.abyss.rules_version
        && lhs.abyss.reward_total == rhs.abyss.reward_total
        && lhs.abyss.generated_mask == rhs.abyss.generated_mask
        && lhs.abyss.claimed_mask == rhs.abyss.claimed_mask
        && lhs.abyss.abandoned_mask == rhs.abyss.abandoned_mask
        && lhs.abyss.reward_revision == rhs.abyss.reward_revision
        && lhs.last_abyss_resolution.valid == rhs.last_abyss_resolution.valid
        && lhs.last_abyss_resolution.room_seed
            == rhs.last_abyss_resolution.room_seed
        && lhs.last_abyss_resolution.rule == rhs.last_abyss_resolution.rule
        && lhs.last_abyss_resolution.total == rhs.last_abyss_resolution.total
        && lhs.last_abyss_resolution.generated
            == rhs.last_abyss_resolution.generated
        && lhs.last_abyss_resolution.claimed
            == rhs.last_abyss_resolution.claimed
        && lhs.last_abyss_resolution.abandoned
            == rhs.last_abyss_resolution.abandoned
        && lhs.last_abyss_resolution.lifecycle
            == rhs.last_abyss_resolution.lifecycle
        && lhs.death_sequence == rhs.death_sequence
        && same_death(lhs.death, rhs.death)
        && lhs.progression.level == rhs.progression.level
        && lhs.progression.experience == rhs.progression.experience
        && lhs.progression.earned_passive_points
            == rhs.progression.earned_passive_points
        && lhs.progression.unspent_passive_points
            == rhs.progression.unspent_passive_points
        && lhs.passive_tree.allocated_bits == rhs.passive_tree.allocated_bits
        && lhs.last_transition == rhs.last_transition
        && lhs.last_direction == rhs.last_direction
        && same_skill_loadout(lhs.skill_loadout, rhs.skill_loadout)
        && same_ownership(lhs.item_ownership, rhs.item_ownership);
}

bool same_slot_checkpoint(const SlotInfo& lhs, const SlotInfo& rhs) noexcept {
    if (lhs.state != SlotFileState::valid
            || rhs.state != SlotFileState::valid
            || lhs.persistence_revision != rhs.persistence_revision
            || lhs.v9 != rhs.v9) {
        return false;
    }
    return lhs.v9 ? lhs.encoded_bytes == rhs.encoded_bytes
                  : same_state(lhs.checkpoint, rhs.checkpoint);
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
    constexpr std::array<std::uint8_t, 8U> kV9Magic{{
        'A', 'R', 'P', 'G', 'S', 'V', '9', '\0'}};
    constexpr std::array<std::uint8_t, 8U> kV10Magic{{
        'A', 'R', 'P', 'G', 'S', 'V', '1', '0'}};
    const bool is_v9 = bytes.size() >= kV9Magic.size()
        && std::equal(kV9Magic.begin(), kV9Magic.end(), bytes.begin());
    const bool is_v10 = bytes.size() >= kV10Magic.size()
        && std::equal(kV10Magic.begin(), kV10Magic.end(), bytes.begin());
    if (is_v9 || is_v10) {
        std::unique_ptr<checkpoint::SaveCheckpointSlot> decoded{
            new (std::nothrow) checkpoint::SaveCheckpointSlot{}};
        bool migrated{};
        const CodecError decode_error = decoded == nullptr
            ? CodecError::allocation_failure
            : is_v10
                ? decode_checkpoint_v10_into(bytes.data(), bytes.size(),
                    *decoded, migrated)
                : decode_checkpoint_v9_into(bytes.data(), bytes.size(),
                    *decoded, migrated);
        if (decode_error != CodecError::none
                || migrated) {
            info.state = SlotFileState::invalid;
            info.error = SaveError::read_failed;
            return info;
        }
        info.state = SlotFileState::valid;
        info.checkpoint = decoded->state;
        info.persistence_revision = decoded->persistence_revision;
        info.v9 = true;
        info.encoded_bytes = std::move(bytes);
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
    info.persistence_revision = decoded.state.commit_generation;
    info.migrated = decoded.migrated;
    info.encoded_bytes = std::move(bytes);
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
    if (scan.a.persistence_revision > scan.b.persistence_revision) {
        return SaveSlot::a;
    }
    if (scan.b.persistence_revision > scan.a.persistence_revision) {
        return SaveSlot::b;
    }
    return same_slot_checkpoint(scan.a, scan.b)
        ? SaveSlot::a
        : SaveSlot::none;
}

}  // namespace arpg::persistence::detail
