#include "test_framework.hpp"

#include "persistence/checkpoint_codec.hpp"
#include "persistence/save_store.hpp"

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace {

namespace checkpoint = arpg::dungeon::checkpoint;
namespace persistence = arpg::persistence;

struct TempDirectory final {
    std::filesystem::path path;

    TempDirectory() noexcept {
        std::error_code error;
        path = std::filesystem::temp_directory_path(error)
            / "arpg_task7_save_store_fault_tests";
        path /= std::to_string(static_cast<unsigned long long>(
            std::hash<std::string>{}(path.string() + std::to_string(
                reinterpret_cast<std::uintptr_t>(this)))));
        std::filesystem::remove_all(path, error);
        std::filesystem::create_directories(path, error);
    }

    ~TempDirectory() noexcept {
        std::error_code error;
        std::filesystem::remove_all(path, error);
    }
};

checkpoint::DungeonRunState make_state(std::uint64_t generation,
    std::uint64_t marker = 0U) noexcept {
    checkpoint::DungeonRunState state{};
    state.root_seed = 0x2000U + marker;
    state.commit_generation = generation;
    state.biases = {{5U, 6U, 7U, 8U}};
    state.current_room.index = 30U + marker;
    state.current_room.seed = 40U + marker;
    state.current_room.depth = 1U + marker;
    state.current_room.floor_room_index = 1U;
    state.current_room.entry = checkpoint::EntrySide::initial;
    state.current_room.ecology = checkpoint::DungeonElement::water;
    state.last_transition = checkpoint::TransitionKind::none;
    state.last_direction = checkpoint::ExitDirection::none;
    return state;
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

std::vector<std::uint8_t> read_bytes(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    return std::vector<std::uint8_t>(
        std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

void write_bytes(const std::filesystem::path& path,
    const std::vector<std::uint8_t>& bytes) noexcept {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output.write(reinterpret_cast<const char*>(bytes.data()),
        static_cast<std::streamsize>(bytes.size()));
}

std::vector<std::uint8_t> encoded(const checkpoint::DungeonRunState& state) {
    std::array<std::uint8_t, persistence::kEncodedCheckpointSize> bytes{};
    const auto ok = persistence::encode_checkpoint(state, bytes);
    (void)ok;
    return std::vector<std::uint8_t>(bytes.begin(), bytes.end());
}

struct FaultContext final {
    persistence::SaveFaultPoint point{};
    bool persistent{};
    bool triggered{};
};

struct ArchiveFaultContext final {
    std::size_t calls{};
    std::size_t fail_on_call{};
};

bool fail_archive_on_call(persistence::SaveFaultPoint point,
    void* opaque) noexcept {
    auto* context = static_cast<ArchiveFaultContext*>(opaque);
    if (point != persistence::SaveFaultPoint::before_archive) {
        return false;
    }
    ++context->calls;
    return context->calls == context->fail_on_call;
}

bool fail_at(persistence::SaveFaultPoint point, void* opaque) noexcept {
    auto* context = static_cast<FaultContext*>(opaque);
    if (point != context->point) {
        return false;
    }
    if (context->persistent || !context->triggered) {
        context->triggered = true;
        return true;
    }
    return false;
}

persistence::SaveStore make_store(const std::filesystem::path& path,
    FaultContext* fault = nullptr) noexcept {
    persistence::SaveStoreConfig config{};
    config.directory = path;
    if (fault != nullptr) {
        config.fault_hook = &fail_at;
        config.fault_context = fault;
    }
    return persistence::SaveStore(config);
}

persistence::SaveStore make_archive_fault_store(
    const std::filesystem::path& path, ArchiveFaultContext* fault) noexcept {
    persistence::SaveStoreConfig config{};
    config.directory = path;
    config.fault_hook = &fail_archive_on_call;
    config.fault_context = fault;
    return persistence::SaveStore(config);
}

arpg::test::Failure before_temp_write_is_not_committed_and_old_bytes_unchanged() noexcept {
    TempDirectory directory;
    auto store = make_store(directory.path);
    const auto first = make_state(1U, 1U);
    ARPG_REQUIRE(store.commit(first).state == persistence::SaveCommitState::committed);
    const auto old_a = read_bytes(directory.path / "run_a.sav");

    FaultContext fault{persistence::SaveFaultPoint::before_temp_write, false, false};
    auto faulty = make_store(directory.path, &fault);
    const auto result = faulty.commit(make_state(2U, 2U));
    ARPG_REQUIRE(result.state == persistence::SaveCommitState::not_committed);
    ARPG_REQUIRE(result.error == persistence::SaveError::write_failed);
    ARPG_REQUIRE(read_bytes(directory.path / "run_a.sav") == old_a);
    ARPG_REQUIRE(!std::filesystem::exists(directory.path / "run_b.sav"));
    return {};
}

arpg::test::Failure after_temp_validation_before_publish_is_not_committed() noexcept {
    TempDirectory directory;
    auto store = make_store(directory.path);
    const auto first = make_state(1U, 3U);
    ARPG_REQUIRE(store.commit(first).state == persistence::SaveCommitState::committed);
    const auto old_a = read_bytes(directory.path / "run_a.sav");

    FaultContext fault{persistence::SaveFaultPoint::before_publish, false, false};
    auto faulty = make_store(directory.path, &fault);
    const auto result = faulty.commit(make_state(2U, 4U));
    ARPG_REQUIRE(result.state == persistence::SaveCommitState::not_committed);
    ARPG_REQUIRE(result.error == persistence::SaveError::publish_failed);
    ARPG_REQUIRE(read_bytes(directory.path / "run_a.sav") == old_a);
    ARPG_REQUIRE(store.load().state == persistence::SaveLoadState::ready);
    return {};
}

arpg::test::Failure publish_final_scan_fault_is_committed_when_expected_proven() noexcept {
    TempDirectory directory;
    auto store = make_store(directory.path);
    ARPG_REQUIRE(store.commit(make_state(1U, 5U)).state == persistence::SaveCommitState::committed);

    FaultContext fault{persistence::SaveFaultPoint::final_scan_a, true, false};
    auto faulty = make_store(directory.path, &fault);
    const auto expected = make_state(2U, 6U);
    const auto result = faulty.commit(expected);
    ARPG_REQUIRE(result.state == persistence::SaveCommitState::committed);
    ARPG_REQUIRE(result.active_slot == persistence::SaveSlot::b);
    ARPG_REQUIRE(same_state(result.verified_state, expected));

    FaultContext indeterminate_fault{persistence::SaveFaultPoint::final_scan_a, true, false};
    auto indeterminate = make_store(directory.path, &indeterminate_fault);
    const auto second = make_state(3U, 7U);
    indeterminate_fault.point = persistence::SaveFaultPoint::final_scan_a;
    const auto uncertain = indeterminate.commit(second);
    ARPG_REQUIRE(uncertain.state == persistence::SaveCommitState::indeterminate);
    ARPG_REQUIRE(uncertain.state != persistence::SaveCommitState::not_committed);
    return {};
}

arpg::test::Failure truncated_temp_without_valid_slot_requires_recovery() noexcept {
    TempDirectory directory;
    write_bytes(directory.path / "run_a.tmp", {0x01U, 0x02U});
    auto store = make_store(directory.path);
    const auto loaded = store.load();
    ARPG_REQUIRE(loaded.state == persistence::SaveLoadState::recovery_required);
    ARPG_REQUIRE(loaded.error == persistence::SaveError::read_failed);
    return {};
}

arpg::test::Failure four_invalid_files_are_archived_before_new_generation() noexcept {
    TempDirectory directory;
    const std::vector<std::uint8_t> invalid{0x10U, 0x11U, 0x12U};
    write_bytes(directory.path / "run_a.sav", invalid);
    write_bytes(directory.path / "run_b.sav", invalid);
    write_bytes(directory.path / "run_a.tmp", invalid);
    write_bytes(directory.path / "run_b.tmp", invalid);
    auto store = make_store(directory.path);
    ARPG_REQUIRE(store.load().state == persistence::SaveLoadState::recovery_required);

    auto created = store.archive_invalid_and_create(make_state(1U, 8U));
    ARPG_REQUIRE(created.state == persistence::SaveLoadState::ready);
    ARPG_REQUIRE(created.active_slot == persistence::SaveSlot::a);
    ARPG_REQUIRE(created.checkpoint.commit_generation == 1U);
    std::size_t archive_count = 0U;
    std::error_code error;
    for (const auto& entry : std::filesystem::directory_iterator(directory.path, error)) {
        if (entry.path().filename().string().find("corrupt_") == 0U) {
            ++archive_count;
        }
    }
    ARPG_REQUIRE(archive_count == 4U);
    ARPG_REQUIRE(!std::filesystem::exists(directory.path / "run_a.tmp"));
    ARPG_REQUIRE(!std::filesystem::exists(directory.path / "run_b.tmp"));
    return {};
}

arpg::test::Failure archive_failure_blocks_and_preserves_corrupt_files() noexcept {
    TempDirectory directory;
    const std::vector<std::uint8_t> invalid_a{0x21U, 0x22U, 0x23U};
    const std::vector<std::uint8_t> invalid_b{0x31U, 0x32U, 0x33U};
    write_bytes(directory.path / "run_a.sav", invalid_a);
    write_bytes(directory.path / "run_b.sav", invalid_b);
    FaultContext fault{persistence::SaveFaultPoint::before_archive, true, false};
    auto store = make_store(directory.path, &fault);
    const auto result = store.archive_invalid_and_create(make_state(1U, 9U));
    ARPG_REQUIRE(result.state == persistence::SaveLoadState::blocked);
    ARPG_REQUIRE(result.error == persistence::SaveError::archive_failed);
    ARPG_REQUIRE(read_bytes(directory.path / "run_a.sav") == invalid_a);
    ARPG_REQUIRE(read_bytes(directory.path / "run_b.sav") == invalid_b);

    TempDirectory midway_directory;
    const std::vector<std::uint8_t> midway_a{0x41U, 0x42U, 0x43U};
    const std::vector<std::uint8_t> midway_b{0x51U, 0x52U, 0x53U};
    write_bytes(midway_directory.path / "run_a.sav", midway_a);
    write_bytes(midway_directory.path / "run_b.sav", midway_b);
    ArchiveFaultContext midway_fault{0U, 2U};
    auto midway_store = make_archive_fault_store(
        midway_directory.path, &midway_fault);
    const auto midway_result = midway_store.archive_invalid_and_create(
        make_state(1U, 10U));
    ARPG_REQUIRE(midway_result.state == persistence::SaveLoadState::blocked);
    ARPG_REQUIRE(midway_result.error == persistence::SaveError::archive_failed);
    ARPG_REQUIRE(read_bytes(midway_directory.path / "run_a.sav") == midway_a);
    ARPG_REQUIRE(read_bytes(midway_directory.path / "run_b.sav") == midway_b);
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"before temp write is not committed and old bytes unchanged", &before_temp_write_is_not_committed_and_old_bytes_unchanged},
    {"after temp validation before publish is not committed", &after_temp_validation_before_publish_is_not_committed},
    {"publish final scan fault is committed when expected proven", &publish_final_scan_fault_is_committed_when_expected_proven},
    {"truncated temp without valid slot requires recovery", &truncated_temp_without_valid_slot_requires_recovery},
    {"four invalid files are archived before new generation", &four_invalid_files_are_archived_before_new_generation},
    {"archive failure blocks and preserves corrupt files", &archive_failure_blocks_and_preserves_corrupt_files},
};

}  // namespace

arpg::test::TestSuite save_store_fault_suite() noexcept {
    return arpg::test::make_suite("save_store_faults", kCases);
}
