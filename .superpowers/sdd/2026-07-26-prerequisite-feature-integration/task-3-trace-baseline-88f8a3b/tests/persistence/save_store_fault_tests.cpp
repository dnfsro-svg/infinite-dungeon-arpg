#include "test_framework.hpp"

#include "persistence/checkpoint_codec.hpp"
#include "persistence/save_store.hpp"
#include "abyss/abyss_rewards.hpp"
#include "abyss/abyss_rules.hpp"
#include "dungeon/dungeon_progression.hpp"

#include <array>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace {

namespace checkpoint = arpg::dungeon::checkpoint;
namespace persistence = arpg::persistence;
namespace items = arpg::items;

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

items::ItemInstance normal_item(std::uint64_t id) noexcept {
    items::ItemInstance item{};
    item.id = id;
    item.base_id = 1U;
    item.rarity = items::ItemRarity::normal;
    item.item_level = 1U;
    item.required_level = 1U;
    return item;
}

checkpoint::DungeonRunState with_items(checkpoint::DungeonRunState state,
    std::size_t count) {
    state.item_ownership.items.reserve(count);
    for (std::size_t index = 0U; index < count; ++index)
        state.item_ownership.items.push_back(normal_item(index + 1U));
    state.item_ownership.next_item_sequence = count + 1U;
    if (count != 0U) {
        state.item_ownership.equipment.equipped_ids[0] = 1U;
        state.item_ownership.claimed_drop_bits = {{
            static_cast<std::uint64_t>(count),
            static_cast<std::uint64_t>(count << 1U),
            static_cast<std::uint64_t>(count << 2U),
        }};
    }
    return state;
}

bool same_ownership(const items::ItemOwnershipState& lhs,
    const items::ItemOwnershipState& rhs) noexcept {
    if (lhs.items.size() != rhs.items.size()
            || lhs.equipment.equipped_ids != rhs.equipment.equipped_ids
            || lhs.materials != rhs.materials
            || lhs.material_discovery_bits != rhs.material_discovery_bits
            || lhs.claimed_drop_bits != rhs.claimed_drop_bits
            || lhs.next_item_sequence != rhs.next_item_sequence) {
        return false;
    }
    for (std::size_t index = 0U; index < lhs.items.size(); ++index) {
        if (std::memcmp(&lhs.items[index], &rhs.items[index],
                sizeof(items::ItemInstance)) != 0) return false;
    }
    return true;
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
    const auto bytes = persistence::encode_checkpoint(state);
    return bytes.has_value() ? *bytes : std::vector<std::uint8_t>{};
}

bool is_corrupt_archive(const std::filesystem::path& path) noexcept {
    const auto name = path.filename().string();
    return name.find(".corrupt.") != std::string::npos;
}

struct FaultContext final {
    persistence::SaveFaultPoint point{};
    bool persistent{};
    bool triggered{};
};

struct CorruptAfterPublishContext final {
    std::filesystem::path directory{};
    bool corrupted{};
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

bool corrupt_target_after_publish(persistence::SaveFaultPoint point,
    void* opaque) noexcept {
    auto* context = static_cast<CorruptAfterPublishContext*>(opaque);
    if (point != persistence::SaveFaultPoint::after_publish
            || context->corrupted) {
        return false;
    }
    context->corrupted = true;
    try {
        std::ofstream output(context->directory / "run_b.sav",
            std::ios::binary | std::ios::trunc);
        const std::array<std::uint8_t, 3> invalid{{0x71U, 0x72U, 0x73U}};
        output.write(reinterpret_cast<const char*>(invalid.data()),
            static_cast<std::streamsize>(invalid.size()));
    } catch (...) {
        return false;
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

arpg::test::Failure publish_final_scan_fault_is_indeterminate() noexcept {
    TempDirectory directory;
    auto store = make_store(directory.path);
    ARPG_REQUIRE(store.commit(make_state(1U, 5U)).state == persistence::SaveCommitState::committed);

    FaultContext fault{persistence::SaveFaultPoint::final_scan_a, true, false};
    auto faulty = make_store(directory.path, &fault);
    auto expected = make_state(2U, 6U);
    expected.last_abyss_resolution.valid = true;
    expected.last_abyss_resolution.room_seed = expected.current_room.seed;
    expected.last_abyss_resolution.rule = arpg::abyss::AbyssRuleId::thunderstorm;
    expected.last_abyss_resolution.total = 1U;
    expected.last_abyss_resolution.generated = 1U;
    const auto result = faulty.commit(expected);
    ARPG_REQUIRE(result.state == persistence::SaveCommitState::indeterminate);
    ARPG_REQUIRE(result.state != persistence::SaveCommitState::not_committed);
    ARPG_REQUIRE(result.error == persistence::SaveError::final_scan_failed);

    FaultContext indeterminate_fault{persistence::SaveFaultPoint::final_scan_a, true, false};
    auto indeterminate = make_store(directory.path, &indeterminate_fault);
    const auto second = make_state(3U, 7U);
    indeterminate_fault.point = persistence::SaveFaultPoint::final_scan_a;
    const auto uncertain = indeterminate.commit(second);
    ARPG_REQUIRE(uncertain.state == persistence::SaveCommitState::indeterminate);
    ARPG_REQUIRE(uncertain.state != persistence::SaveCommitState::not_committed);

    TempDirectory invalid_target_directory;
    auto invalid_target_store = make_store(invalid_target_directory.path);
    ARPG_REQUIRE(invalid_target_store.commit(make_state(1U, 11U)).state
        == persistence::SaveCommitState::committed);
    CorruptAfterPublishContext corrupt_context{
        invalid_target_directory.path, false};
    persistence::SaveStoreConfig corrupt_config{};
    corrupt_config.directory = invalid_target_directory.path;
    corrupt_config.fault_hook = &corrupt_target_after_publish;
    corrupt_config.fault_context = &corrupt_context;
    persistence::SaveStore corrupt_store(corrupt_config);
    const auto invalid_target = corrupt_store.commit(make_state(2U, 12U));
    ARPG_REQUIRE(invalid_target.state == persistence::SaveCommitState::not_committed);
    ARPG_REQUIRE(invalid_target.active_slot == persistence::SaveSlot::a);
    return {};
}

arpg::test::Failure truncated_temp_without_valid_slot_requires_recovery() noexcept {
    TempDirectory directory;
    write_bytes(directory.path / "run_a.tmp", {0x01U, 0x02U});
    auto store = make_store(directory.path);
    const auto loaded = store.load();
    ARPG_REQUIRE(loaded.state == persistence::SaveLoadState::recovery_required);
    ARPG_REQUIRE(loaded.error == persistence::SaveError::read_failed);

    TempDirectory dangling_directory;
    std::error_code symlink_error;
    std::filesystem::create_symlink(
        std::filesystem::path(R"(\\.\NUL\child)"),
        dangling_directory.path / "run_a.sav", symlink_error);
    ARPG_REQUIRE(!symlink_error);
    auto dangling_store = make_store(dangling_directory.path);
    const auto dangling_loaded = dangling_store.load();
    ARPG_REQUIRE(dangling_loaded.state == persistence::SaveLoadState::blocked);
    ARPG_REQUIRE(dangling_loaded.error
        == persistence::SaveError::directory_unavailable);
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
        if (is_corrupt_archive(entry.path())) {
            ++archive_count;
        }
    }
    ARPG_REQUIRE(archive_count == 4U);
    ARPG_REQUIRE(!std::filesystem::exists(directory.path / "run_a.tmp"));
    ARPG_REQUIRE(!std::filesystem::exists(directory.path / "run_b.tmp"));
    return {};
}

arpg::test::Failure conflicting_slots_are_archived_before_new_generation() noexcept {
    TempDirectory directory;
    auto store = make_store(directory.path);
    ARPG_REQUIRE(store.commit(make_state(1U, 13U)).state
        == persistence::SaveCommitState::committed);

    std::error_code error;
    std::filesystem::copy_file(directory.path / "run_a.sav",
        directory.path / "run_b.sav",
        std::filesystem::copy_options::overwrite_existing, error);
    ARPG_REQUIRE(!error);
    write_bytes(directory.path / "run_b.sav", encoded(make_state(1U, 14U)));
    const auto blocked = store.load();
    ARPG_REQUIRE(blocked.state == persistence::SaveLoadState::recovery_required);
    ARPG_REQUIRE(blocked.error == persistence::SaveError::conflicting_slots);

    const auto initial = make_state(1U, 15U);
    const auto created = store.archive_invalid_and_create(initial);
    ARPG_REQUIRE(created.state == persistence::SaveLoadState::ready);
    ARPG_REQUIRE(created.active_slot == persistence::SaveSlot::a);
    ARPG_REQUIRE(created.recovered);
    ARPG_REQUIRE(same_state(created.checkpoint, initial));

    const auto reloaded = store.load();
    ARPG_REQUIRE(reloaded.state == persistence::SaveLoadState::ready);
    ARPG_REQUIRE(reloaded.active_slot == persistence::SaveSlot::a);
    ARPG_REQUIRE(same_state(reloaded.checkpoint, initial));
    ARPG_REQUIRE(std::filesystem::exists(directory.path / "run_a.sav"));
    ARPG_REQUIRE(!std::filesystem::exists(directory.path / "run_b.sav"));

    std::size_t archive_count = 0U;
    bool archived_a = false;
    bool archived_b = false;
    for (const auto& entry : std::filesystem::directory_iterator(directory.path, error)) {
        const auto name = entry.path().filename().string();
        if (is_corrupt_archive(entry.path())) {
            ++archive_count;
            archived_a = archived_a || name.find("run_a.sav.corrupt.") == 0U;
            archived_b = archived_b || name.find("run_b.sav.corrupt.") == 0U;
        }
    }
    ARPG_REQUIRE(!error);
    ARPG_REQUIRE(archive_count == 2U);
    ARPG_REQUIRE(archived_a);
    ARPG_REQUIRE(archived_b);
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

arpg::test::Failure baseline_fault_recovery_selects_recorded_slot_and_generation() noexcept {
    struct CommitRecoveryExpectation final {
        persistence::SaveFaultPoint point{};
        persistence::SaveSlot slot{persistence::SaveSlot::none};
        std::uint64_t generation{};
    };
    constexpr std::array<CommitRecoveryExpectation, 7U> kCommitExpectations{{
        {persistence::SaveFaultPoint::before_temp_write,
            persistence::SaveSlot::a, 17U},
        {persistence::SaveFaultPoint::after_temp_write,
            persistence::SaveSlot::a, 17U},
        {persistence::SaveFaultPoint::after_temp_validation,
            persistence::SaveSlot::a, 17U},
        {persistence::SaveFaultPoint::before_publish,
            persistence::SaveSlot::a, 17U},
        {persistence::SaveFaultPoint::after_publish,
            persistence::SaveSlot::b, 18U},
        {persistence::SaveFaultPoint::final_scan_a,
            persistence::SaveSlot::b, 18U},
        {persistence::SaveFaultPoint::final_scan_b,
            persistence::SaveSlot::b, 18U},
    }};

    for (const auto& expected : kCommitExpectations) {
        TempDirectory directory;
        auto healthy = make_store(directory.path);
        ARPG_REQUIRE(healthy.commit(make_state(17U, 21U)).state
            == persistence::SaveCommitState::committed);

        FaultContext fault{expected.point, false, false};
        auto faulty = make_store(directory.path, &fault);
        static_cast<void>(faulty.commit(make_state(18U, 22U)));
        const auto recovered = faulty.load();
        ARPG_REQUIRE(recovered.state == persistence::SaveLoadState::ready);
        ARPG_REQUIRE(recovered.active_slot == expected.slot);
        ARPG_REQUIRE(recovered.checkpoint.commit_generation == expected.generation);
    }

    TempDirectory directory;
    auto healthy = make_store(directory.path);
    ARPG_REQUIRE(healthy.commit(make_state(17U, 23U)).state
        == persistence::SaveCommitState::committed);
    write_bytes(directory.path / "run_b.sav", {0x01U, 0x02U, 0x03U});
    FaultContext fault{persistence::SaveFaultPoint::before_archive, false, false};
    auto faulty = make_store(directory.path, &fault);
    const auto blocked = faulty.load();
    ARPG_REQUIRE(blocked.state == persistence::SaveLoadState::blocked);
    const auto recovered = faulty.load();
    ARPG_REQUIRE(recovered.state == persistence::SaveLoadState::ready);
    ARPG_REQUIRE(recovered.active_slot == persistence::SaveSlot::a);
    ARPG_REQUIRE(recovered.checkpoint.commit_generation == 17U);
    return {};
}

arpg::test::Failure variable_length_faults_preserve_atomic_slot_semantics() noexcept {
    struct Expectation final {
        persistence::SaveFaultPoint point{};
        std::uint64_t generation{};
        std::size_t item_count{};
    };
    constexpr std::array<Expectation, 7U> kExpectations{{
        {persistence::SaveFaultPoint::before_temp_write, 31U, 33U},
        {persistence::SaveFaultPoint::after_temp_write, 31U, 33U},
        {persistence::SaveFaultPoint::after_temp_validation, 31U, 33U},
        {persistence::SaveFaultPoint::before_publish, 31U, 33U},
        {persistence::SaveFaultPoint::after_publish, 32U, 1U},
        {persistence::SaveFaultPoint::final_scan_a, 32U, 1U},
        {persistence::SaveFaultPoint::final_scan_b, 32U, 1U},
    }};

    for (const auto& expectation : kExpectations) {
        TempDirectory directory;
        auto healthy = make_store(directory.path);
        const auto large = with_items(make_state(31U, 31U), 33U);
        ARPG_REQUIRE(healthy.commit(large).state
            == persistence::SaveCommitState::committed);

        FaultContext fault{expectation.point, false, false};
        auto faulty = make_store(directory.path, &fault);
        const auto small = with_items(make_state(32U, 32U), 1U);
        static_cast<void>(faulty.commit(small));
        const auto loaded = faulty.load();
        ARPG_REQUIRE(loaded.state == persistence::SaveLoadState::ready);
        ARPG_REQUIRE(loaded.checkpoint.commit_generation
            == expectation.generation);
        ARPG_REQUIRE(loaded.checkpoint.item_ownership.items.size()
            == expectation.item_count);
        ARPG_REQUIRE(loaded.checkpoint.item_ownership.next_item_sequence
            == expectation.item_count + 1U);
        const auto& expected_state = expectation.generation == 31U
            ? large : small;
        ARPG_REQUIRE(same_ownership(
            loaded.checkpoint.item_ownership,
            expected_state.item_ownership));
    }
    return {};
}

checkpoint::DungeonRunState cleared_abyss_for_faults(
    arpg::abyss::AbyssDanger wanted) noexcept {
    checkpoint::DungeonRunState state = arpg::dungeon::make_initial_run_state(
        0xA10FA017ULL, arpg::dungeon::DungeonRules{}).state;
    constexpr std::uint64_t kDepth = 20U;
    for (std::uint64_t seed = 1U; seed != 0U; ++seed) {
        const auto selection = arpg::abyss::select_abyss_rule(seed, kDepth);
        if (!arpg::abyss::is_abyss_roll(seed)
                || !selection.has_value() || selection->danger != wanted) continue;
        state.current_room.seed = seed;
        state.current_room.depth = kDepth;
        state.current_room.entry = checkpoint::EntrySide::left;
        state.current_room.is_abyss = true;
        state.current_room.has_hole = true;
        state.last_transition = checkpoint::TransitionKind::door;
        state.last_direction = checkpoint::ExitDirection::right;
        state.abyss.lifecycle = arpg::abyss::AbyssLifecycle::cleared;
        state.abyss.danger = selection->danger;
        state.abyss.rule = selection->rule;
        state.abyss.rules_version = selection->rules_version;
        state.abyss.reward_total = arpg::abyss::reward_profile_for(
            wanted, 1U).item_count;
        return state;
    }
    return {};
}

arpg::test::Failure abyss_claim_and_abandon_fault_matrix_is_old_or_new() noexcept {
    struct Transaction final {
        checkpoint::DungeonRunState old_state{};
        checkpoint::DungeonRunState new_state{};
    };
    std::array<Transaction, 2U> transactions{};

    auto& claim = transactions[0U];
    claim.old_state = cleared_abyss_for_faults(
        arpg::abyss::AbyssDanger::low);
    claim.old_state.commit_generation = 41U;
    claim.old_state.abyss.generated_mask = 1U;
    claim.old_state.abyss.reward_revision = 1U;
    claim.old_state.item_ownership.next_item_sequence = 9001U;
    claim.new_state = claim.old_state;
    claim.new_state.commit_generation = 42U;
    claim.new_state.abyss.claimed_mask = 1U;
    claim.new_state.abyss.reward_revision = 2U;
    claim.new_state.item_ownership.items.push_back(normal_item(0xA811C1A1U));

    auto& abandon = transactions[1U];
    abandon.old_state = cleared_abyss_for_faults(
        arpg::abyss::AbyssDanger::high);
    abandon.old_state.commit_generation = 51U;
    abandon.old_state.abyss.generated_mask = 1U;
    abandon.old_state.abyss.reward_revision = 1U;
    abandon.new_state = abandon.old_state;
    abandon.new_state.commit_generation = 52U;
    abandon.new_state.current_room.index += 1U;
    abandon.new_state.current_room.seed ^= 0xA8A8U;
    abandon.new_state.current_room.is_abyss = false;
    abandon.new_state.abyss = {};
    abandon.new_state.last_transition = checkpoint::TransitionKind::door;
    abandon.new_state.last_direction = checkpoint::ExitDirection::left;
    abandon.new_state.last_abyss_resolution.valid = true;
    abandon.new_state.last_abyss_resolution.room_seed =
        abandon.old_state.current_room.seed;
    abandon.new_state.last_abyss_resolution.rule = abandon.old_state.abyss.rule;
    abandon.new_state.last_abyss_resolution.total = 3U;
    abandon.new_state.last_abyss_resolution.generated = 1U;
    abandon.new_state.last_abyss_resolution.claimed = 0U;
    abandon.new_state.last_abyss_resolution.abandoned = 2U;

    struct FaultExpectation final {
        persistence::SaveFaultPoint point{};
        bool new_state{};
    };
    constexpr std::array<FaultExpectation, 7U> kFaults{{
        {persistence::SaveFaultPoint::before_temp_write, false},
        {persistence::SaveFaultPoint::after_temp_write, false},
        {persistence::SaveFaultPoint::after_temp_validation, false},
        {persistence::SaveFaultPoint::before_publish, false},
        {persistence::SaveFaultPoint::after_publish, true},
        {persistence::SaveFaultPoint::final_scan_a, true},
        {persistence::SaveFaultPoint::final_scan_b, true},
    }};

    for (const Transaction& transaction : transactions) {
        for (const FaultExpectation& expectation : kFaults) {
            TempDirectory directory;
            auto healthy = make_store(directory.path);
            ARPG_REQUIRE(healthy.commit(transaction.old_state).state
                == persistence::SaveCommitState::committed);
            FaultContext fault{expectation.point, false, false};
            auto faulty = make_store(directory.path, &fault);
            static_cast<void>(faulty.commit(transaction.new_state));
            const auto loaded = faulty.load();
            ARPG_REQUIRE(loaded.state == persistence::SaveLoadState::ready);
            const auto& expected = expectation.new_state
                ? transaction.new_state : transaction.old_state;
            ARPG_REQUIRE(arpg::dungeon::same_run_state(
                loaded.checkpoint, expected));
            if (&transaction == &transactions[0U]) {
                ARPG_REQUIRE(loaded.checkpoint.item_ownership.items.size()
                    == (expectation.new_state ? 1U : 0U));
                ARPG_REQUIRE(loaded.checkpoint.item_ownership.next_item_sequence
                    == 9001U);
                ARPG_REQUIRE(loaded.checkpoint.abyss.claimed_mask
                    == (expectation.new_state ? 1U : 0U));
            } else {
                ARPG_REQUIRE(loaded.checkpoint.last_abyss_resolution.valid
                    == expectation.new_state);
                ARPG_REQUIRE(loaded.checkpoint.current_room.is_abyss
                    != expectation.new_state);
            }
        }
    }
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"before temp write is not committed and old bytes unchanged", &before_temp_write_is_not_committed_and_old_bytes_unchanged},
    {"after temp validation before publish is not committed", &after_temp_validation_before_publish_is_not_committed},
    {"publish final scan fault is indeterminate", &publish_final_scan_fault_is_indeterminate},
    {"truncated temp without valid slot requires recovery", &truncated_temp_without_valid_slot_requires_recovery},
    {"four invalid files are archived before new generation", &four_invalid_files_are_archived_before_new_generation},
    {"conflicting slots are archived before new generation", &conflicting_slots_are_archived_before_new_generation},
    {"archive failure blocks and preserves corrupt files", &archive_failure_blocks_and_preserves_corrupt_files},
    {"baseline fault recovery selects recorded slot and generation", &baseline_fault_recovery_selects_recorded_slot_and_generation},
    {"variable length faults preserve atomic slot semantics", &variable_length_faults_preserve_atomic_slot_semantics},
    {"abyss claim abandon faults are old or new",
        &abyss_claim_and_abandon_fault_matrix_is_old_or_new},
};

}  // namespace

arpg::test::TestSuite save_store_fault_suite() noexcept {
    return arpg::test::make_suite("save_store_faults", kCases);
}
