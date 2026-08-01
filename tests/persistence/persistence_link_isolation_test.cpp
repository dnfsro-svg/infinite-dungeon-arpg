#include "checkpoint/room_checkpoint_validation.hpp"
#include "persistence/room_progress_codec.hpp"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <new>

int main() {
    using arpg::checkpoint::SaveCheckpointSlot;
    using arpg::persistence::CodecError;

    std::unique_ptr<SaveCheckpointSlot> source{
        new (std::nothrow) SaveCheckpointSlot{}};
    std::unique_ptr<SaveCheckpointSlot> decoded{
        new (std::nothrow) SaveCheckpointSlot{}};
    std::unique_ptr<std::uint8_t[]> bytes{
        new (std::nothrow) std::uint8_t[
            arpg::persistence::kMaximumEncodedCheckpointBytes]};
    std::unique_ptr<std::uint8_t[]> round_trip{
        new (std::nothrow) std::uint8_t[
            arpg::persistence::kMaximumEncodedCheckpointBytes]};
    if (!source || !decoded || !bytes || !round_trip) return 1;

    arpg::checkpoint::clear_save_checkpoint_slot(*source);
    source->persistence_revision = source->state.commit_generation;
    std::size_t written{};
    if (arpg::persistence::encode_checkpoint_v9_into(*source, bytes.get(),
            arpg::persistence::kMaximumEncodedCheckpointBytes, written)
        != CodecError::none) {
        return 2;
    }

    bool migrated{true};
    if (arpg::persistence::decode_checkpoint_v9_into(
            bytes.get(), written, *decoded, migrated) != CodecError::none
        || migrated) {
        return 3;
    }

    std::size_t round_trip_size{};
    if (arpg::persistence::encode_checkpoint_v9_into(
            *decoded, round_trip.get(),
            arpg::persistence::kMaximumEncodedCheckpointBytes,
            round_trip_size) != CodecError::none
        || round_trip_size != written
        || !std::equal(bytes.get(), bytes.get() + written,
            round_trip.get())
        || decoded->persistence_revision != source->persistence_revision
        || decoded->state.root_seed != source->state.root_seed
        || decoded->state.commit_generation != source->state.commit_generation
        || !arpg::checkpoint::same_room_progress_checkpoint(
            decoded->room_progress, source->room_progress)) {
        return 4;
    }
    return 0;
}
