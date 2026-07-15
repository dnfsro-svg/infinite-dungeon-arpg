#include "test_framework.hpp"

#include "persistence/checkpoint_codec.hpp"
#include "persistence/save_store.hpp"
#include "persistence/save_store_detail.hpp"

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <string>
#include <system_error>
#include <vector>

namespace {

namespace checkpoint = arpg::dungeon::checkpoint;
namespace persistence = arpg::persistence;
namespace items = arpg::items;

[[maybe_unused]] const persistence::SaveLoadResult kSaveLoadFieldOrderProbe{
    persistence::SaveLoadState::ready,
    persistence::SaveError::none,
    persistence::SaveSlot::a,
    true,
    {}};

struct TempDirectory final {
    std::filesystem::path path;

    TempDirectory() noexcept {
        std::error_code error;
        path = std::filesystem::temp_directory_path(error)
            / "arpg_task7_save_store_tests";
        path /= std::to_string(
            static_cast<unsigned long long>(
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
    state.root_seed = 0x1000U + marker;
    state.commit_generation = generation;
    state.biases = {{1U, 2U, 3U, 4U}};
    state.current_room.index = 10U + marker;
    state.current_room.seed = 20U + marker;
    state.current_room.depth = 1U + marker;
    state.current_room.floor_room_index = 1U;
    state.current_room.entry = checkpoint::EntrySide::initial;
    state.current_room.ecology = checkpoint::DungeonElement::fire;
    state.last_transition = checkpoint::TransitionKind::none;
    state.last_direction = checkpoint::ExitDirection::none;
    return state;
}

items::ItemInstance normal_item(std::uint64_t id,
    std::uint8_t base_id = 1U) noexcept {
    items::ItemInstance item{};
    item.id = id;
    item.base_id = base_id;
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
        && lhs.progression.level == rhs.progression.level
        && lhs.progression.experience == rhs.progression.experience
        && lhs.progression.earned_passive_points
            == rhs.progression.earned_passive_points
        && lhs.progression.unspent_passive_points
            == rhs.progression.unspent_passive_points
        && lhs.last_transition == rhs.last_transition
        && lhs.last_direction == rhs.last_direction;
}

persistence::SaveStore make_store(const std::filesystem::path& path) noexcept {
    persistence::SaveStoreConfig config{};
    config.directory = path;
    return persistence::SaveStore(config);
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

bool has_only_allowed_files(const std::filesystem::path& directory) noexcept {
    std::error_code error;
    for (const auto& entry : std::filesystem::directory_iterator(directory, error)) {
        if (error) {
            return false;
        }
        const auto name = entry.path().filename().string();
        if (name == "run_a.sav" || name == "run_b.sav") {
            continue;
        }
        if (name.find(".corrupt.") != std::string::npos) {
            continue;
        }
        if (name.find("run_a.tmp") == 0U || name.find("run_b.tmp") == 0U) {
            return false;
        }
        return false;
    }
    return true;
}

arpg::test::Failure empty_commit_and_reload() noexcept {
    TempDirectory directory;
    auto store = make_store(directory.path);
    const auto empty = store.load();
    ARPG_REQUIRE(empty.state == persistence::SaveLoadState::empty);

    const auto first = make_state(1U);
    const auto committed = store.commit(first);
    ARPG_REQUIRE(committed.state == persistence::SaveCommitState::committed);
    ARPG_REQUIRE(committed.active_slot == persistence::SaveSlot::a);

    const auto reloaded = store.load();
    ARPG_REQUIRE(reloaded.state == persistence::SaveLoadState::ready);
    ARPG_REQUIRE(reloaded.active_slot == persistence::SaveSlot::a);
    ARPG_REQUIRE(same_state(reloaded.checkpoint, first));
    ARPG_REQUIRE(has_only_allowed_files(directory.path));
    return {};
}

arpg::test::Failure highest_generation_wins_and_alternates() noexcept {
    TempDirectory directory;
    auto store = make_store(directory.path);
    const auto one = make_state(1U, 1U);
    const auto two = make_state(2U, 2U);
    const auto three = make_state(3U, 3U);
    ARPG_REQUIRE(store.commit(one).state == persistence::SaveCommitState::committed);
    ARPG_REQUIRE(store.commit(two).state == persistence::SaveCommitState::committed);
    ARPG_REQUIRE(store.commit(three).state == persistence::SaveCommitState::committed);
    const auto loaded = store.load();
    ARPG_REQUIRE(loaded.state == persistence::SaveLoadState::ready);
    ARPG_REQUIRE(loaded.active_slot == persistence::SaveSlot::a);
    ARPG_REQUIRE(same_state(loaded.checkpoint, three));
    ARPG_REQUIRE(has_only_allowed_files(directory.path));
    return {};
}

arpg::test::Failure equal_generation_same_payload_prefers_a_then_writes_b() noexcept {
    TempDirectory directory;
    auto store = make_store(directory.path);
    const auto state = make_state(1U, 5U);
    ARPG_REQUIRE(store.commit(state).active_slot == persistence::SaveSlot::a);

    std::error_code error;
    std::filesystem::copy_file(directory.path / "run_a.sav",
        directory.path / "run_b.sav",
        std::filesystem::copy_options::overwrite_existing, error);
    ARPG_REQUIRE(!error);
    const auto loaded = store.load();
    ARPG_REQUIRE(loaded.state == persistence::SaveLoadState::ready);
    ARPG_REQUIRE(loaded.active_slot == persistence::SaveSlot::a);

    const auto next = make_state(2U, 6U);
    const auto committed = store.commit(next);
    ARPG_REQUIRE(committed.state == persistence::SaveCommitState::committed);
    ARPG_REQUIRE(committed.active_slot == persistence::SaveSlot::b);
    ARPG_REQUIRE(has_only_allowed_files(directory.path));
    return {};
}

arpg::test::Failure missing_slot_is_created() noexcept {
    TempDirectory directory;
    auto store = make_store(directory.path);
    const auto first = make_state(1U, 7U);
    ARPG_REQUIRE(store.commit(first).state == persistence::SaveCommitState::committed);
    std::error_code error;
    std::filesystem::remove(directory.path / "run_b.sav", error);
    ARPG_REQUIRE(!error);
    const auto loaded = store.load();
    ARPG_REQUIRE(loaded.state == persistence::SaveLoadState::ready);
    ARPG_REQUIRE(loaded.active_slot == persistence::SaveSlot::a);
    const auto second = make_state(2U, 8U);
    ARPG_REQUIRE(store.commit(second).state == persistence::SaveCommitState::committed);
    ARPG_REQUIRE(std::filesystem::exists(directory.path / "run_b.sav"));
    ARPG_REQUIRE(has_only_allowed_files(directory.path));
    return {};
}

arpg::test::Failure damaged_slot_is_archived_before_two_saves() noexcept {
    TempDirectory directory;
    auto store = make_store(directory.path);
    const auto first = make_state(1U, 9U);
    ARPG_REQUIRE(store.commit(first).state == persistence::SaveCommitState::committed);
    std::error_code error;
    write_bytes(directory.path / "run_b.sav", {0x01U, 0x02U, 0x03U});
    const auto loaded = store.load();
    ARPG_REQUIRE(loaded.state == persistence::SaveLoadState::ready);
    bool archived = false;
    for (const auto& entry : std::filesystem::directory_iterator(directory.path, error)) {
        if (entry.path().filename().string().find(".corrupt.") != std::string::npos) {
            archived = true;
        }
    }
    ARPG_REQUIRE(archived);
    ARPG_REQUIRE(store.commit(make_state(2U, 10U)).state == persistence::SaveCommitState::committed);
    ARPG_REQUIRE(store.commit(make_state(3U, 11U)).state == persistence::SaveCommitState::committed);
    ARPG_REQUIRE(has_only_allowed_files(directory.path));
    return {};
}

arpg::test::Failure conflicting_equal_generation_requires_recovery() noexcept {
    TempDirectory directory;
    auto store = make_store(directory.path);
    ARPG_REQUIRE(store.commit(make_state(1U, 12U)).state == persistence::SaveCommitState::committed);
    std::error_code error;
    std::filesystem::copy_file(directory.path / "run_a.sav",
        directory.path / "run_b.sav",
        std::filesystem::copy_options::overwrite_existing, error);
    ARPG_REQUIRE(!error);
    auto altered = make_state(1U, 12U);
    altered.progression = {2U, 0U, 1U, 1U};
    const auto encoded_altered = persistence::encode_checkpoint(altered);
    ARPG_REQUIRE(encoded_altered.has_value());
    write_bytes(directory.path / "run_b.sav", *encoded_altered);
    const auto loaded = store.load();
    ARPG_REQUIRE(loaded.state == persistence::SaveLoadState::recovery_required);
    ARPG_REQUIRE(loaded.error == persistence::SaveError::conflicting_slots);
    ARPG_REQUIRE(has_only_allowed_files(directory.path));
    return {};
}

arpg::test::Failure temp_files_do_not_participate_in_load() noexcept {
    TempDirectory directory;
    auto store = make_store(directory.path);
    const auto first = make_state(4U, 14U);
    ARPG_REQUIRE(store.commit(first).state == persistence::SaveCommitState::committed);
    write_bytes(directory.path / "run_b.tmp", encoded(make_state(99U, 99U)));
    const auto loaded = store.load();
    ARPG_REQUIRE(loaded.state == persistence::SaveLoadState::ready);
    ARPG_REQUIRE(loaded.active_slot == persistence::SaveSlot::a);
    ARPG_REQUIRE(same_state(loaded.checkpoint, first));
    ARPG_REQUIRE(has_only_allowed_files(directory.path));
    return {};
}

arpg::test::Failure variable_length_slots_rotate_large_then_small() noexcept {
    TempDirectory directory;
    auto store = make_store(directory.path);
    const auto large = with_items(make_state(1U, 20U), 257U);
    ARPG_REQUIRE(store.commit(large).state
        == persistence::SaveCommitState::committed);
    ARPG_REQUIRE(std::filesystem::file_size(directory.path / "run_a.sav")
        == 204U + 40U * 257U);

    const auto small = with_items(make_state(2U, 21U), 1U);
    ARPG_REQUIRE(store.commit(small).state
        == persistence::SaveCommitState::committed);
    ARPG_REQUIRE(std::filesystem::file_size(directory.path / "run_b.sav")
        == 244U);
    const auto loaded = store.load();
    ARPG_REQUIRE(loaded.state == persistence::SaveLoadState::ready);
    ARPG_REQUIRE(loaded.active_slot == persistence::SaveSlot::b);
    ARPG_REQUIRE(loaded.checkpoint.commit_generation == 2U);
    ARPG_REQUIRE(loaded.checkpoint.item_ownership.items.size() == 1U);
    ARPG_REQUIRE(loaded.checkpoint.item_ownership.items[0].id == 1U);
    return {};
}

arpg::test::Failure same_state_includes_all_ownership_bytes_and_order() noexcept {
    auto lhs = with_items(make_state(9U, 22U), 2U);
    auto rhs = lhs;
    std::swap(rhs.item_ownership.items[0], rhs.item_ownership.items[1]);
    ARPG_REQUIRE(!persistence::detail::same_state(lhs, rhs));
    rhs = lhs;
    rhs.item_ownership.items[0].reserved[2] = 1U;
    ARPG_REQUIRE(!persistence::detail::same_state(lhs, rhs));
    rhs = lhs;
    rhs.item_ownership.items[0].affixes[5].variant = 1U;
    ARPG_REQUIRE(!persistence::detail::same_state(lhs, rhs));
    rhs = lhs;
    rhs.item_ownership.equipment.equipped_ids[0] = 1U;
    ARPG_REQUIRE(!persistence::detail::same_state(lhs, rhs));
    rhs = lhs;
    rhs.item_ownership.claimed_drop_bits[2] = 8U;
    ARPG_REQUIRE(!persistence::detail::same_state(lhs, rhs));
    rhs = lhs;
    ++rhs.item_ownership.next_item_sequence;
    ARPG_REQUIRE(!persistence::detail::same_state(lhs, rhs));

    TempDirectory directory;
    write_bytes(directory.path / "run_a.sav", encoded(lhs));
    rhs = lhs;
    rhs.item_ownership.items[0].id = 99U;
    rhs.item_ownership.next_item_sequence = 100U;
    write_bytes(directory.path / "run_b.sav", encoded(rhs));
    auto store = make_store(directory.path);
    const auto loaded = store.load();
    ARPG_REQUIRE(loaded.state == persistence::SaveLoadState::recovery_required);
    ARPG_REQUIRE(loaded.error == persistence::SaveError::conflicting_slots);
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"empty commit and reload", &empty_commit_and_reload},
    {"highest generation wins and alternates", &highest_generation_wins_and_alternates},
    {"equal generation same payload prefers a then writes b", &equal_generation_same_payload_prefers_a_then_writes_b},
    {"missing slot is created", &missing_slot_is_created},
    {"damaged slot is archived before two saves", &damaged_slot_is_archived_before_two_saves},
    {"conflicting equal generation requires recovery", &conflicting_equal_generation_requires_recovery},
    {"temp files do not participate in load", &temp_files_do_not_participate_in_load},
    {"variable length slots rotate large then small", &variable_length_slots_rotate_large_then_small},
    {"same state includes ownership bytes and order", &same_state_includes_all_ownership_bytes_and_order},
};

}  // namespace

arpg::test::TestSuite save_store_suite() noexcept {
    return arpg::test::make_suite("save_store", kCases);
}
