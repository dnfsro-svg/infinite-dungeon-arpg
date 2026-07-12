#include "persistence/checkpoint_codec.hpp"

#include <algorithm>

namespace arpg::persistence {
namespace {

namespace checkpoint = arpg::dungeon::checkpoint;
using checkpoint::DungeonElement;
using checkpoint::EntrySide;
using checkpoint::ExitDirection;
using checkpoint::TransitionKind;

constexpr std::array<std::uint8_t, 8> kMagic{{
    'I', 'A', 'R', 'P', 'G', 'S', '0', '3'}};
constexpr std::size_t kCrcCoverageSize = 84U;
constexpr std::size_t kCrcHeaderOffset = 8U;
constexpr std::size_t kCrcHeaderSize = 20U;
constexpr std::size_t kCrcPayloadOffset = 32U;
constexpr std::size_t kCrcPayloadSize = 64U;

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

std::uint32_t read_u32(const std::uint8_t* source) noexcept {
    std::uint32_t value = 0U;
    for (std::size_t index = 0; index < 4U; ++index) {
        value |= static_cast<std::uint32_t>(source[index]) << (index * 8U);
    }
    return value;
}

std::uint64_t read_u64(const std::uint8_t* source) noexcept {
    std::uint64_t value = 0U;
    for (std::size_t index = 0; index < 8U; ++index) {
        value |= static_cast<std::uint64_t>(source[index]) << (index * 8U);
    }
    return value;
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
        && valid_direction(static_cast<std::uint8_t>(state.last_direction));
}

std::uint32_t checkpoint_crc(
    const std::uint8_t* bytes) noexcept {
    std::array<std::uint8_t, kCrcCoverageSize> covered{};
    std::copy_n(
        bytes + kCrcHeaderOffset,
        kCrcHeaderSize,
        covered.begin());
    std::copy_n(
        bytes + kCrcPayloadOffset,
        kCrcPayloadSize,
        covered.begin() + kCrcHeaderSize);
    return crc32(covered.data(), covered.size());
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
    std::copy(kMagic.begin(), kMagic.end(), out.begin());
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

    write_u32(out.data() + 28U, checkpoint_crc(out.data()));
    return true;
}

DecodeResult decode_checkpoint(
    const std::uint8_t* bytes, std::size_t size) noexcept {
    if (bytes == nullptr || size != kEncodedCheckpointSize) {
        return error_result(CodecError::wrong_size);
    }
    if (!std::equal(kMagic.begin(), kMagic.end(), bytes)) {
        return error_result(CodecError::bad_magic);
    }
    if (read_u32(bytes + 8U) != kCheckpointFormatVersion) {
        return error_result(CodecError::unsupported_format);
    }
    if (read_u32(bytes + 12U) != kCheckpointRulesVersion) {
        return error_result(CodecError::unsupported_rules);
    }
    if (read_u32(bytes + 24U) != kCheckpointPayloadSize) {
        return error_result(CodecError::bad_payload_length);
    }
    if (read_u32(bytes + 28U) != checkpoint_crc(bytes)) {
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
    if (bytes[94U] != 0U || bytes[95U] != 0U) {
        return error_result(CodecError::invalid_state);
    }

    DecodeResult result{};
    auto& state = result.state;
    state.root_seed = read_u64(bytes + 32U);
    for (std::size_t index = 0; index < state.biases.size(); ++index) {
        state.biases[index] = read_u32(bytes + 40U + index * 4U);
    }
    state.commit_generation = read_u64(bytes + 16U);
    state.current_room.index = read_u64(bytes + 56U);
    state.current_room.seed = read_u64(bytes + 64U);
    state.current_room.depth = read_u64(bytes + 72U);
    state.current_room.floor_room_index = read_u64(bytes + 80U);
    state.current_room.entry = static_cast<EntrySide>(entry);
    state.current_room.ecology = static_cast<DungeonElement>(ecology);
    state.current_room.has_hole = has_hole != 0U;
    state.current_room.is_abyss = is_abyss != 0U;
    state.last_transition = static_cast<TransitionKind>(transition);
    state.last_direction = static_cast<ExitDirection>(direction);
    if (!valid_state(state)) {
        return error_result(CodecError::invalid_state);
    }
    return result;
}

}  // namespace arpg::persistence
