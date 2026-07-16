#include "persistence/checkpoint_codec.hpp"

#include "passives/passive_tree_rules.hpp"
#include "progression/progression_rules.hpp"
#include "items/item_catalog.hpp"

#include <algorithm>
#include <limits>
#include <new>

namespace arpg::persistence {
namespace {

namespace checkpoint = arpg::dungeon::checkpoint;
using checkpoint::DungeonElement;
using checkpoint::EntrySide;
using checkpoint::ExitDirection;
using checkpoint::TransitionKind;

constexpr std::array<std::uint8_t, 8> kCurrentMagic{{
    'I', 'A', 'R', 'P', 'G', 'S', '0', '5'}};
constexpr std::array<std::uint8_t, 8> kThirdMagic{{
    'I', 'A', 'R', 'P', 'G', 'S', '0', '4'}};
constexpr std::array<std::uint8_t, 8> kLegacyMagic{{
    'I', 'A', 'R', 'P', 'G', 'S', '0', '3'}};
constexpr std::size_t kCrcHeaderOffset = 8U;
constexpr std::size_t kCrcHeaderSize = 20U;
constexpr std::size_t kCrcPayloadOffset = 32U;

struct DecodeCursor final {
    const std::byte* data{};
    std::size_t size{};
    std::size_t offset{};

    bool read_u8(std::uint8_t& value) noexcept {
        if (offset >= size)
            return false;
        value = static_cast<std::uint8_t>(data[offset]);
        ++offset;
        return true;
    }

    bool read_u16(std::uint16_t& value) noexcept {
        if (offset > size || size - offset < 2U)
            return false;
        const auto* source = reinterpret_cast<const std::uint8_t*>(data) + offset;
        value = static_cast<std::uint16_t>(source[0])
            | static_cast<std::uint16_t>(source[1] << 8U);
        offset += 2U;
        return true;
    }

    bool read_u32(std::uint32_t& value) noexcept {
        if (offset > size || size - offset < 4U) {
            return false;
        }
        value = 0U;
        const auto* source = reinterpret_cast<const std::uint8_t*>(data) + offset;
        for (std::size_t index = 0; index < 4U; ++index) {
            value |= static_cast<std::uint32_t>(source[index]) << (index * 8U);
        }
        offset += 4U;
        return true;
    }

    bool read_u64(std::uint64_t& value) noexcept {
        if (offset > size || size - offset < 8U) {
            return false;
        }
        value = 0U;
        const auto* source = reinterpret_cast<const std::uint8_t*>(data) + offset;
        for (std::size_t index = 0; index < 8U; ++index) {
            value |= static_cast<std::uint64_t>(source[index]) << (index * 8U);
        }
        offset += 8U;
        return true;
    }
};

void write_u32(std::uint8_t* destination, std::uint32_t value) noexcept {
    for (std::size_t index = 0; index < 4U; ++index) {
        destination[index] = static_cast<std::uint8_t>(value >> (index * 8U));
    }
}

void write_u16(std::uint8_t* destination, std::uint16_t value) noexcept {
    destination[0] = static_cast<std::uint8_t>(value);
    destination[1] = static_cast<std::uint8_t>(value >> 8U);
}

void write_u64(std::uint8_t* destination, std::uint64_t value) noexcept {
    for (std::size_t index = 0; index < 8U; ++index) {
        destination[index] = static_cast<std::uint8_t>(value >> (index * 8U));
    }
}

bool valid_entry(std::uint8_t value) noexcept {
    return value <= static_cast<std::uint8_t>(EntrySide::right);
}

bool valid_element(std::uint8_t value) noexcept {
    return value <= static_cast<std::uint8_t>(DungeonElement::chaos);
}

bool valid_transition(std::uint8_t value) noexcept {
    return value == static_cast<std::uint8_t>(TransitionKind::door)
        || value == static_cast<std::uint8_t>(TransitionKind::descent)
        || value == static_cast<std::uint8_t>(TransitionKind::none);
}

bool valid_direction(std::uint8_t value) noexcept {
    return value <= static_cast<std::uint8_t>(ExitDirection::right)
        || value == static_cast<std::uint8_t>(ExitDirection::none);
}

bool valid_checkpoint_fields(
    const dungeon::checkpoint::DungeonRunState& state) noexcept {
    return state.commit_generation != 0U
        && state.current_room.depth != 0U
        && state.current_room.floor_room_index != 0U
        && valid_entry(static_cast<std::uint8_t>(state.current_room.entry))
        && valid_element(static_cast<std::uint8_t>(state.current_room.ecology))
        && valid_transition(static_cast<std::uint8_t>(state.last_transition))
        && valid_direction(static_cast<std::uint8_t>(state.last_direction))
        && progression::valid_progression_state(
            state.progression, progression::default_progression_rules())
        && passives::valid_passive_tree_state(
            state.passive_tree, state.progression);
}

std::uint32_t checkpoint_crc(
    const std::uint8_t* bytes,
    std::size_t payload_size) noexcept {
    const auto header_crc = crc32_update(
        0U, bytes + kCrcHeaderOffset, kCrcHeaderSize);
    return crc32_update(
        header_crc, bytes + kCrcPayloadOffset, payload_size);
}

DecodeResult error_result(CodecError error) noexcept {
    DecodeResult result{};
    result.error = error;
    return result;
}

}  // namespace

std::optional<EncodedCheckpoint> encode_checkpoint(
    const dungeon::checkpoint::DungeonRunState& state) noexcept {
    if (!valid_checkpoint_fields(state)
        || items::validate_ownership_detailed(state.item_ownership)
            != items::OwnershipValidationResult::valid) {
        return std::nullopt;
    }
    const auto item_count = state.item_ownership.items.size();
    if (item_count > kMaximumCheckpointItemCount
        || item_count > (std::numeric_limits<std::size_t>::max()
                - kV4BasePayloadSize) / kV4ItemRecordSize) {
        return std::nullopt;
    }
    const auto payload_size = kV4BasePayloadSize
        + item_count * kV4ItemRecordSize;
    if (payload_size > std::numeric_limits<std::size_t>::max()
            - kCheckpointHeaderSize) {
        return std::nullopt;
    }
    const auto encoded_size = kCheckpointHeaderSize + payload_size;
    EncodedCheckpoint out;
    try {
        out.resize(encoded_size, 0U);
    } catch (const std::bad_alloc&) {
        return std::nullopt;
    } catch (...) {
        return std::nullopt;
    }

    std::copy(kCurrentMagic.begin(), kCurrentMagic.end(), out.begin());
    write_u32(out.data() + 8U, kCheckpointFormatVersion);
    write_u32(out.data() + 12U, kCheckpointRulesVersion);
    write_u64(out.data() + 16U, state.commit_generation);
    write_u32(out.data() + 24U, static_cast<std::uint32_t>(payload_size));

    write_u64(out.data() + 32U, state.root_seed);
    for (std::size_t index = 0; index < state.biases.size(); ++index) {
        write_u32(out.data() + 40U + index * 4U, state.biases[index]);
    }
    write_u64(out.data() + 56U, state.current_room.index);
    write_u64(out.data() + 64U, state.current_room.seed);
    write_u64(out.data() + 72U, state.current_room.depth);
    write_u64(out.data() + 80U, state.current_room.floor_room_index);
    out[88U] = static_cast<std::uint8_t>(state.current_room.entry);
    out[89U] = static_cast<std::uint8_t>(state.current_room.ecology);
    out[90U] = state.current_room.has_hole ? 1U : 0U;
    out[91U] = state.current_room.is_abyss ? 1U : 0U;
    out[92U] = static_cast<std::uint8_t>(state.last_transition);
    out[93U] = static_cast<std::uint8_t>(state.last_direction);
    out[94U] = state.progression.level;
    out[95U] = state.progression.earned_passive_points;
    out[96U] = state.progression.unspent_passive_points;
    write_u64(out.data() + 98U, state.progression.experience);
    write_u64(out.data() + 106U, state.passive_tree.allocated_bits);

    write_u32(out.data() + 120U, static_cast<std::uint32_t>(item_count));
    write_u64(out.data() + 124U, state.item_ownership.next_item_sequence);
    for (std::size_t index = 0U;
            index < state.item_ownership.claimed_drop_bits.size(); ++index) {
        write_u64(out.data() + 132U + index * 8U,
            state.item_ownership.claimed_drop_bits[index]);
    }
    for (std::size_t index = 0U;
            index < state.item_ownership.equipment.equipped_ids.size(); ++index) {
        write_u64(out.data() + 156U + index * 8U,
            state.item_ownership.equipment.equipped_ids[index]);
    }
    for (std::size_t item_index = 0U; item_index < item_count; ++item_index) {
        const auto& item = state.item_ownership.items[item_index];
        auto* record = out.data() + 204U + item_index * kV4ItemRecordSize;
        write_u64(record, item.id);
        record[8U] = item.base_id;
        record[9U] = static_cast<std::uint8_t>(item.rarity);
        record[10U] = item.item_level;
        record[11U] = item.required_level;
        record[12U] = item.affix_count;
        for (std::size_t roll_index = 0U;
                roll_index < item.affixes.size(); ++roll_index) {
            const auto& roll = item.affixes[roll_index];
            auto* encoded_roll = record + 16U + roll_index * 4U;
            write_u16(encoded_roll, roll.affix_id);
            encoded_roll[2U] = roll.tier;
            encoded_roll[3U] = roll.variant;
        }
    }

    write_u32(out.data() + 28U,
        checkpoint_crc(out.data(), payload_size));
    return out;
}

DecodeResult decode_checkpoint(
    const std::uint8_t* bytes, std::size_t size) noexcept {
    if (bytes == nullptr || size < kCheckpointHeaderSize) {
        return error_result(CodecError::wrong_size);
    }
    const bool current_magic = std::equal(
        kCurrentMagic.begin(), kCurrentMagic.end(), bytes);
    const bool third_magic = std::equal(
        kThirdMagic.begin(), kThirdMagic.end(), bytes);
    const bool legacy_magic = std::equal(
        kLegacyMagic.begin(), kLegacyMagic.end(), bytes);
    if (!current_magic && !third_magic && !legacy_magic) {
        return error_result(CodecError::bad_magic);
    }
    DecodeCursor header{reinterpret_cast<const std::byte*>(bytes), size, 8U};
    std::uint32_t format{};
    std::uint32_t rules{};
    std::uint64_t generation{};
    std::uint32_t encoded_payload_size{};
    std::uint32_t encoded_crc{};
    if (!header.read_u32(format) || !header.read_u32(rules)
            || !header.read_u64(generation)
            || !header.read_u32(encoded_payload_size)
            || !header.read_u32(encoded_crc)) {
        return error_result(CodecError::wrong_size);
    }
    if (format != kCheckpointFormatVersion
        && format != kThirdCheckpointFormatVersion
        && format != kPreviousCheckpointFormatVersion
        && format != kLegacyCheckpointFormatVersion) {
        return error_result(CodecError::unsupported_format);
    }
    const bool matching_magic =
        (format == kCheckpointFormatVersion && current_magic)
        || (format == kThirdCheckpointFormatVersion && third_magic)
        || ((format == kPreviousCheckpointFormatVersion
                || format == kLegacyCheckpointFormatVersion) && legacy_magic);
    if (!matching_magic) {
        return error_result(CodecError::unsupported_format);
    }
    if (rules != kCheckpointRulesVersion) {
        return error_result(CodecError::unsupported_rules);
    }
    std::size_t payload_size = encoded_payload_size;
    if (payload_size > std::numeric_limits<std::size_t>::max()
            - kCheckpointHeaderSize
        || size != kCheckpointHeaderSize + payload_size) {
        return error_result(CodecError::bad_payload_length);
    }
    if ((format == kLegacyCheckpointFormatVersion
            && payload_size != kLegacyCheckpointPayloadSize)
        || (format == kPreviousCheckpointFormatVersion
            && payload_size != kPreviousCheckpointPayloadSize)
        || (format == kThirdCheckpointFormatVersion
            && payload_size != kCheckpointPayloadSize)
        || (format == kCheckpointFormatVersion
            && payload_size < kV4BasePayloadSize)) {
        return error_result(CodecError::bad_payload_length);
    }
    if (encoded_crc != checkpoint_crc(bytes, payload_size)) {
        return error_result(CodecError::bad_crc);
    }

    std::uint32_t item_count{};
    if (format == kCheckpointFormatVersion) {
        DecodeCursor count_cursor{
            reinterpret_cast<const std::byte*>(bytes), size, 120U};
        if (!count_cursor.read_u32(item_count)
            || item_count > kMaximumCheckpointItemCount) {
            return error_result(CodecError::bad_payload_length);
        }
        if (item_count > (std::numeric_limits<std::size_t>::max()
                - kV4BasePayloadSize) / kV4ItemRecordSize) {
            return error_result(CodecError::bad_payload_length);
        }
        const auto expected_payload_size = kV4BasePayloadSize
            + static_cast<std::size_t>(item_count) * kV4ItemRecordSize;
        if (payload_size != expected_payload_size)
            return error_result(CodecError::bad_payload_length);
    }

    DecodeResult result{};
    auto& state = result.state;
    if (item_count != 0U) {
        try {
            state.item_ownership.items.resize(item_count);
        } catch (const std::bad_alloc&) {
            return error_result(CodecError::allocation_failure);
        } catch (...) {
            return error_result(CodecError::allocation_failure);
        }
    }

    const auto entry = bytes[88U];
    const auto ecology = bytes[89U];
    const auto has_hole = bytes[90U];
    const auto is_abyss = bytes[91U];
    const auto transition = bytes[92U];
    const auto direction = bytes[93U];
    if (!valid_entry(entry)
            || !valid_element(ecology)
            || !valid_transition(transition)
            || !valid_direction(direction)) {
        return error_result(CodecError::invalid_enum);
    }
    if (has_hole > 1U || is_abyss > 1U) {
        return error_result(CodecError::invalid_boolean);
    }

    DecodeCursor payload{reinterpret_cast<const std::byte*>(bytes), size, 32U};
    if (!payload.read_u64(state.root_seed)) {
        return error_result(CodecError::wrong_size);
    }
    for (std::size_t index = 0; index < state.biases.size(); ++index) {
        if (!payload.read_u32(state.biases[index])) {
            return error_result(CodecError::wrong_size);
        }
    }
    state.commit_generation = generation;
    if (!payload.read_u64(state.current_room.index)
            || !payload.read_u64(state.current_room.seed)
            || !payload.read_u64(state.current_room.depth)
            || !payload.read_u64(state.current_room.floor_room_index)) {
        return error_result(CodecError::wrong_size);
    }
    state.current_room.entry = static_cast<EntrySide>(entry);
    state.current_room.ecology = static_cast<DungeonElement>(ecology);
    state.current_room.has_hole = has_hole != 0U;
    state.current_room.is_abyss = is_abyss != 0U;
    state.last_transition = static_cast<TransitionKind>(transition);
    state.last_direction = static_cast<ExitDirection>(direction);
    if (format == kPreviousCheckpointFormatVersion) {
        if (bytes[97U] != 0U
            || !std::all_of(bytes + 106U, bytes + 112U,
                [](std::uint8_t value) noexcept { return value == 0U; })) {
            return error_result(CodecError::invalid_state);
        }
        state.passive_tree = passives::PassiveTreeState{1ULL};
        state.progression.level = bytes[94U];
        state.progression.earned_passive_points = bytes[95U];
        state.progression.unspent_passive_points = bytes[96U];
        DecodeCursor progression{reinterpret_cast<const std::byte*>(bytes),
            size, 98U};
        if (!progression.read_u64(state.progression.experience)) {
            return error_result(CodecError::wrong_size);
        }
    } else if (format == kThirdCheckpointFormatVersion
            || format == kCheckpointFormatVersion) {
        if (bytes[97U] != 0U
            || !std::all_of(bytes + 114U, bytes + 120U,
                [](std::uint8_t value) noexcept { return value == 0U; })) {
            return error_result(CodecError::invalid_state);
        }
        state.progression.level = bytes[94U];
        state.progression.earned_passive_points = bytes[95U];
        state.progression.unspent_passive_points = bytes[96U];
        DecodeCursor progression{reinterpret_cast<const std::byte*>(bytes),
            size, 98U};
        if (!progression.read_u64(state.progression.experience)) {
            return error_result(CodecError::wrong_size);
        }
        DecodeCursor passive_tree{reinterpret_cast<const std::byte*>(bytes),
            size, 106U};
        if (!passive_tree.read_u64(state.passive_tree.allocated_bits)) {
            return error_result(CodecError::wrong_size);
        }
    } else {
        state.passive_tree = passives::PassiveTreeState{1ULL};
    }
    if (format == kCheckpointFormatVersion) {
        DecodeCursor ownership{
            reinterpret_cast<const std::byte*>(bytes), size, 124U};
        if (!ownership.read_u64(state.item_ownership.next_item_sequence))
            return error_result(CodecError::wrong_size);
        for (auto& claimed : state.item_ownership.claimed_drop_bits) {
            if (!ownership.read_u64(claimed))
                return error_result(CodecError::wrong_size);
        }
        for (auto& equipped : state.item_ownership.equipment.equipped_ids) {
            if (!ownership.read_u64(equipped))
                return error_result(CodecError::wrong_size);
        }
        for (auto& item : state.item_ownership.items) {
            if (!ownership.read_u64(item.id)
                || !ownership.read_u8(item.base_id)) {
                return error_result(CodecError::wrong_size);
            }
            std::uint8_t rarity{};
            if (!ownership.read_u8(rarity)
                || !ownership.read_u8(item.item_level)
                || !ownership.read_u8(item.required_level)
                || !ownership.read_u8(item.affix_count)) {
                return error_result(CodecError::wrong_size);
            }
            item.rarity = static_cast<items::ItemRarity>(rarity);
            std::uint8_t record_reserved{};
            for (std::size_t index = 0U; index < 3U; ++index) {
                if (!ownership.read_u8(record_reserved))
                    return error_result(CodecError::wrong_size);
                if (record_reserved != 0U)
                    return error_result(CodecError::invalid_state);
            }
            for (auto& roll : item.affixes) {
                if (!ownership.read_u16(roll.affix_id)
                    || !ownership.read_u8(roll.tier)
                    || !ownership.read_u8(roll.variant)) {
                    return error_result(CodecError::wrong_size);
                }
            }
        }
    }
    if (!valid_checkpoint_fields(state)) {
        return error_result(CodecError::invalid_state);
    }
    const auto ownership_validation =
        items::validate_ownership_detailed(state.item_ownership);
    if (ownership_validation == items::OwnershipValidationResult::allocation_failure)
        return error_result(CodecError::allocation_failure);
    if (ownership_validation != items::OwnershipValidationResult::valid)
        return error_result(CodecError::invalid_state);
    return result;
}

}  // namespace arpg::persistence
