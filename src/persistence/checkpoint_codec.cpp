#include "persistence/checkpoint_codec.hpp"

#include "passives/passive_tree_rules.hpp"
#include "progression/progression_rules.hpp"

#include <algorithm>

namespace arpg::persistence {
namespace {

namespace checkpoint = arpg::dungeon::checkpoint;
using checkpoint::DungeonElement;
using checkpoint::EntrySide;
using checkpoint::ExitDirection;
using checkpoint::TransitionKind;

constexpr std::array<std::uint8_t, 8> kCurrentMagic{{
    'I', 'A', 'R', 'P', 'G', 'S', '0', '4'}};
constexpr std::array<std::uint8_t, 8> kLegacyMagic{{
    'I', 'A', 'R', 'P', 'G', 'S', '0', '3'}};
constexpr std::size_t kMaximumCrcCoverageSize = 108U;
constexpr std::size_t kCrcHeaderOffset = 8U;
constexpr std::size_t kCrcHeaderSize = 20U;
constexpr std::size_t kCrcPayloadOffset = 32U;

struct DecodeCursor final {
    const std::byte* data{};
    std::size_t size{};
    std::size_t offset{};

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

bool valid_state(const dungeon::checkpoint::DungeonRunState& state) noexcept {
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
    std::array<std::uint8_t, kMaximumCrcCoverageSize> covered{};
    std::copy_n(
        bytes + kCrcHeaderOffset,
        kCrcHeaderSize,
        covered.begin());
    std::copy_n(
        bytes + kCrcPayloadOffset,
        payload_size,
        covered.begin() + kCrcHeaderSize);
    return crc32(covered.data(), kCrcHeaderSize + payload_size);
}

DecodeResult error_result(CodecError error) noexcept {
    DecodeResult result{};
    result.error = error;
    return result;
}

}  // namespace

bool encode_checkpoint(
    const dungeon::checkpoint::DungeonRunState& state,
    std::array<std::uint8_t, kEncodedCheckpointSize>& out) noexcept {
    if (!valid_state(state)) {
        return false;
    }

    out.fill(0U);
    std::copy(kCurrentMagic.begin(), kCurrentMagic.end(), out.begin());
    write_u32(out.data() + 8U, kCheckpointFormatVersion);
    write_u32(out.data() + 12U, kCheckpointRulesVersion);
    write_u64(out.data() + 16U, state.commit_generation);
    write_u32(out.data() + 24U, static_cast<std::uint32_t>(kCheckpointPayloadSize));

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

    write_u32(out.data() + 28U,
        checkpoint_crc(out.data(), kCheckpointPayloadSize));
    return true;
}

DecodeResult decode_checkpoint(
    const std::uint8_t* bytes, std::size_t size) noexcept {
    if (bytes == nullptr
        || (size != kEncodedCheckpointSize
            && size != kPreviousEncodedCheckpointSize
            && size != kLegacyEncodedCheckpointSize)) {
        return error_result(CodecError::wrong_size);
    }
    const bool current_magic = std::equal(
        kCurrentMagic.begin(), kCurrentMagic.end(), bytes);
    const bool legacy_magic = std::equal(
        kLegacyMagic.begin(), kLegacyMagic.end(), bytes);
    if (!current_magic && !legacy_magic) {
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
        && format != kPreviousCheckpointFormatVersion
        && format != kLegacyCheckpointFormatVersion) {
        return error_result(CodecError::unsupported_format);
    }
    if ((format == kCheckpointFormatVersion) != current_magic
        || (format != kCheckpointFormatVersion) != legacy_magic) {
        return error_result(CodecError::unsupported_format);
    }
    if (rules != kCheckpointRulesVersion) {
        return error_result(CodecError::unsupported_rules);
    }
    const std::size_t payload_size = format == kLegacyCheckpointFormatVersion
        ? kLegacyCheckpointPayloadSize
        : format == kPreviousCheckpointFormatVersion
            ? kPreviousCheckpointPayloadSize : kCheckpointPayloadSize;
    const std::size_t encoded_size = kCheckpointHeaderSize + payload_size;
    if (size != encoded_size || encoded_payload_size != payload_size) {
        return error_result(CodecError::bad_payload_length);
    }
    if (encoded_crc != checkpoint_crc(bytes, payload_size)) {
        return error_result(CodecError::bad_crc);
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

    DecodeResult result{};
    auto& state = result.state;
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
    } else if (format == kCheckpointFormatVersion) {
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
    if (!valid_state(state)) {
        return error_result(CodecError::invalid_state);
    }
    return result;
}

}  // namespace arpg::persistence
