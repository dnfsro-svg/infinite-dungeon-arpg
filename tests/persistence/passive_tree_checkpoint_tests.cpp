#include "test_framework.hpp"

#include "persistence/checkpoint_codec.hpp"
#include "persistence/save_store.hpp"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace {

namespace checkpoint = arpg::dungeon::checkpoint;
namespace persistence = arpg::persistence;

bool fail_final_scan_a(persistence::SaveFaultPoint point,
    void*) noexcept {
    return point == persistence::SaveFaultPoint::final_scan_a;
}

checkpoint::DungeonRunState make_fixture() noexcept {
    checkpoint::DungeonRunState state{};
    state.root_seed = 0x0102030405060708ULL;
    state.commit_generation = 7U;
    state.biases = {{1U, 2U, 3U, 4U}};
    state.current_room.index = 10U;
    state.current_room.seed = 20U;
    state.current_room.depth = 2U;
    state.current_room.floor_room_index = 3U;
    state.current_room.entry = checkpoint::EntrySide::right;
    state.current_room.ecology = checkpoint::DungeonElement::chaos;
    state.current_room.has_hole = true;
    state.current_room.is_abyss = true;
    state.last_transition = checkpoint::TransitionKind::descent;
    state.last_direction = checkpoint::ExitDirection::left;
    state.progression = {10U, 0U, 9U, 6U};
    state.passive_tree.allocated_bits = (1ULL << 0U) | (1ULL << 8U)
        | (1ULL << 9U) | (1ULL << 10U);
    return state;
}

void refresh_crc(std::uint8_t* bytes, std::size_t payload_size) noexcept {
    auto checksum = persistence::crc32_update(0U, bytes + 8U, 20U);
    checksum = persistence::crc32_update(checksum, bytes + 32U, payload_size);
    for (std::size_t index = 0U; index < 4U; ++index) {
        bytes[28U + index] = static_cast<std::uint8_t>(
            checksum >> (index * 8U));
    }
}

std::array<std::uint8_t, persistence::kPreviousEncodedCheckpointSize>
legacy_format_two_fixture() noexcept {
    checkpoint::DungeonRunState state = make_fixture();
    state.progression = {7U, 0U, 6U, 6U};
    state.passive_tree.allocated_bits = 1ULL;
    const auto current = persistence::encode_checkpoint(state);
    if (!current.has_value())
        return {};
    std::array<std::uint8_t, persistence::kPreviousEncodedCheckpointSize> legacy{};
    std::copy_n(current->begin(), legacy.size(), legacy.begin());
    legacy[0U] = 'I'; legacy[1U] = 'A'; legacy[2U] = 'R'; legacy[3U] = 'P';
    legacy[4U] = 'G'; legacy[5U] = 'S'; legacy[6U] = '0'; legacy[7U] = '3';
    legacy[8U] = 2U;
    legacy[9U] = legacy[10U] = legacy[11U] = 0U;
    legacy[24U] = 80U;
    legacy[25U] = legacy[26U] = legacy[27U] = 0U;
    std::fill(legacy.begin() + 106U, legacy.begin() + 112U,
        static_cast<std::uint8_t>(0U));
    refresh_crc(legacy.data(), 80U);
    return legacy;
}

arpg::test::Failure passive_bits_round_trip_at_little_endian_offset_106() noexcept {
    const auto state = make_fixture();
    const auto bytes = persistence::encode_checkpoint(state);
    ARPG_REQUIRE(bytes.has_value());
    ARPG_REQUIRE(bytes->size() == 204U);
    ARPG_REQUIRE((*bytes)[0U] == 'I' && (*bytes)[7U] == '5');
    ARPG_REQUIRE((*bytes)[106U] == 0x01U && (*bytes)[107U] == 0x07U);
    const auto decoded = persistence::decode_checkpoint(bytes->data(), bytes->size());
    ARPG_REQUIRE(decoded.error == persistence::CodecError::none);
    ARPG_REQUIRE(decoded.state.passive_tree.allocated_bits
        == state.passive_tree.allocated_bits);
    return {};
}

arpg::test::Failure format_two_migrates_to_start_only_passive_tree() noexcept {
    const auto bytes = legacy_format_two_fixture();
    const auto decoded = persistence::decode_checkpoint(bytes.data(), bytes.size());
    ARPG_REQUIRE(decoded.error == persistence::CodecError::none);
    ARPG_REQUIRE(decoded.state.passive_tree.allocated_bits == 1ULL);
    ARPG_REQUIRE(decoded.state.item_ownership.items.empty());
    ARPG_REQUIRE(decoded.state.item_ownership.equipment.equipped_ids
        == arpg::items::EquipmentState{}.equipped_ids);
    const std::array<std::uint64_t, 3U> no_claimed_drops{};
    ARPG_REQUIRE(decoded.state.item_ownership.claimed_drop_bits
        == no_claimed_drops);
    ARPG_REQUIRE(decoded.state.item_ownership.next_item_sequence == 1U);
    return {};
}

arpg::test::Failure invalid_passive_bits_are_rejected() noexcept {
    for (const auto mutate : std::array<void(*)(std::uint8_t*), 3U>{
             [](std::uint8_t* bytes) noexcept { bytes[106U] &= 0xFEU; },
             [](std::uint8_t* bytes) noexcept { bytes[114U] = 0x01U; },
             [](std::uint8_t* bytes) noexcept {
                 bytes[106U] = 0x01U; bytes[107U] = 0x08U;
             }}) {
        auto bytes = persistence::encode_checkpoint(make_fixture());
        ARPG_REQUIRE(bytes.has_value());
        mutate(bytes->data());
        refresh_crc(bytes->data(), 172U);
        ARPG_REQUIRE(persistence::decode_checkpoint(bytes->data(), bytes->size()).error
            == persistence::CodecError::invalid_state);
    }
    return {};
}

arpg::test::Failure crc_covers_passive_bits() noexcept {
    auto bytes = persistence::encode_checkpoint(make_fixture());
    ARPG_REQUIRE(bytes.has_value());
    (*bytes)[106U] ^= 0x08U;
    ARPG_REQUIRE(persistence::decode_checkpoint(bytes->data(), bytes->size()).error
        == persistence::CodecError::bad_crc);
    return {};
}

arpg::test::Failure save_store_fault_recovery_preserves_passive_bits() noexcept {
    const auto directory = std::filesystem::temp_directory_path()
        / ("arpg_task4_passive_tree_" + std::to_string(
            static_cast<unsigned long long>(std::rand())));
    std::error_code error;
    std::filesystem::remove_all(directory, error);
    std::filesystem::create_directories(directory, error);
    persistence::SaveStoreConfig config{};
    config.directory = directory;
    persistence::SaveStore store(config);
    const auto initial = make_fixture();
    ARPG_REQUIRE(store.commit(initial).state == persistence::SaveCommitState::committed);

    auto next = make_fixture();
    next.commit_generation = 8U;
    next.passive_tree.allocated_bits = (1ULL << 0U) | (1ULL << 1U)
        | (1ULL << 2U) | (1ULL << 3U);
    ARPG_REQUIRE(next.passive_tree.allocated_bits
        != initial.passive_tree.allocated_bits);
    persistence::SaveStoreConfig faulty_config{};
    faulty_config.directory = directory;
    faulty_config.fault_hook = &fail_final_scan_a;
    persistence::SaveStore faulty_store(faulty_config);
    ARPG_REQUIRE(faulty_store.commit(next).state
        == persistence::SaveCommitState::indeterminate);

    const auto loaded = faulty_store.load();
    ARPG_REQUIRE(loaded.state == persistence::SaveLoadState::ready);
    ARPG_REQUIRE(loaded.checkpoint.passive_tree.allocated_bits
        == next.passive_tree.allocated_bits);
    std::filesystem::remove_all(directory, error);
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"passive bits round trip at little endian offset 106",
        &passive_bits_round_trip_at_little_endian_offset_106},
    {"format two migrates to start only passive tree",
        &format_two_migrates_to_start_only_passive_tree},
    {"invalid passive bits are rejected", &invalid_passive_bits_are_rejected},
    {"crc covers passive bits", &crc_covers_passive_bits},
    {"save store fault recovery preserves passive bits",
        &save_store_fault_recovery_preserves_passive_bits},
};

}  // namespace

arpg::test::TestSuite passive_tree_checkpoint_suite() noexcept {
    return arpg::test::make_suite("passive_tree_checkpoint", kCases);
}
