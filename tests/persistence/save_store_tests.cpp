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
namespace skills = arpg::skills;

[[maybe_unused]] const persistence::SaveLoadResult kSaveLoadFieldOrderProbe{
    persistence::SaveLoadState::ready,
    persistence::SaveError::none,
    persistence::SaveSlot::a,
    true,
    false,
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

checkpoint::DungeonRunState with_pending_death(
    checkpoint::DungeonRunState state) noexcept {
    state.death_sequence = 1U;
    state.biases = {};
    state.current_room.is_abyss = false;
    state.last_transition = checkpoint::TransitionKind::death_retreat;
    state.last_direction = checkpoint::ExitDirection::none;
    auto& death = state.death;
    death.lifecycle = checkpoint::DeathLifecycle::pending_continue;
    death.data_version = checkpoint::kDeathCheckpointDataVersion;
    death.death_depth = state.current_room.depth;
    death.death_floor_room_index = state.current_room.floor_room_index;
    death.death_ecology = state.current_room.ecology;
    death.source_kind = checkpoint::DeathSourceKind::unknown;
    death.source_monster_id = 0xFFU;
    death.damage_type = checkpoint::DeathDamageType::physical;
    death.raw_damage = 10U;
    death.health_loss = 10U;
    death.final_damage = 10U;
    death.recent_damage[0] = 10U;
    death.max_hp = 10;
    death.damage_reduction_cap = {{7500, 7500, 7500, 7500}};
    death.target_room = {state.current_room.index + 1U, 0xD34DULL,
        state.current_room.depth > 1U ? state.current_room.depth - 1U : 1U,
        0U, checkpoint::EntrySide::initial,
        checkpoint::DungeonElement::water, false, false};
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

void write_u32(std::vector<std::uint8_t>& bytes, std::size_t offset,
    std::uint32_t value) noexcept {
    for (std::size_t index = 0U; index < 4U; ++index)
        bytes[offset + index] = static_cast<std::uint8_t>(value >> (index * 8U));
}

std::vector<std::uint8_t> encoded_v4(
    const checkpoint::DungeonRunState& state) {
    const auto v8 = encoded(state);
    if (v8.size() < persistence::kV8BaseEncodedCheckpointSize)
        return {};
    std::vector<std::uint8_t> v5(
        persistence::kV6BaseEncodedCheckpointSize
            + state.item_ownership.items.size()
                * persistence::kV4ItemRecordSize,
        0U);
    std::copy_n(v8.begin(), persistence::kV6BaseEncodedCheckpointSize,
        v5.begin());
    for (std::size_t item_index = 0U;
            item_index < state.item_ownership.items.size(); ++item_index) {
        const std::size_t v8_record = persistence::kV8BaseEncodedCheckpointSize
            + item_index * persistence::kV7ItemRecordSize;
        const std::size_t v6_record = persistence::kV6BaseEncodedCheckpointSize
            + item_index * persistence::kV4ItemRecordSize;
        std::copy_n(v8.begin() + v8_record, 16U, v5.begin() + v6_record);
        for (std::size_t roll = 0U; roll < 6U; ++roll) {
            std::copy_n(v8.begin() + v8_record + 16U + roll * 6U, 4U,
                v5.begin() + v6_record + 16U + roll * 4U);
        }
    }
    v5.erase(v5.begin() + 236U, v5.begin() + 460U);
    v5[0U] = 'I'; v5[1U] = 'A'; v5[2U] = 'R'; v5[3U] = 'P';
    v5[4U] = 'G'; v5[5U] = 'S'; v5[6U] = '0'; v5[7U] = '6';
    write_u32(v5, 8U, 5U);
    write_u32(v5, 24U, static_cast<std::uint32_t>(v5.size() - 32U));
    if (v5.size() < 152U)
        return {};
    std::vector<std::uint8_t> v4(v5.size() - 32U, 0U);
    std::copy_n(v5.begin(), 120U, v4.begin());
    std::copy(v5.begin() + 152U, v5.end(), v4.begin() + 120U);
    v4[7U] = '5';
    write_u32(v4, 8U, 4U);
    write_u32(v4, 24U, static_cast<std::uint32_t>(v4.size() - 32U));
    auto checksum = persistence::crc32_update(0U, v4.data() + 8U, 20U);
    checksum = persistence::crc32_update(
        checksum, v4.data() + 32U, v4.size() - 32U);
    write_u32(v4, 28U, checksum);
    return v4;
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
        == persistence::kV8BaseEncodedCheckpointSize
            + persistence::kV7ItemRecordSize * 257U);

    const auto small = with_items(make_state(2U, 21U), 1U);
    ARPG_REQUIRE(store.commit(small).state
        == persistence::SaveCommitState::committed);
    ARPG_REQUIRE(std::filesystem::file_size(directory.path / "run_b.sav")
        == persistence::kV8BaseEncodedCheckpointSize
            + persistence::kV7ItemRecordSize);
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
    rhs.item_ownership.materials[0] = 1U;
    ARPG_REQUIRE(!persistence::detail::same_state(lhs, rhs));
    rhs = lhs;
    rhs.item_ownership.material_discovery_bits = 1U;
    ARPG_REQUIRE(!persistence::detail::same_state(lhs, rhs));
    rhs = lhs;
    rhs.item_ownership.items[0].reinforcement = 1U;
    ARPG_REQUIRE(!persistence::detail::same_state(lhs, rhs));
    rhs = lhs;
    rhs.item_ownership.claimed_drop_bits[2] = 8U;
    ARPG_REQUIRE(!persistence::detail::same_state(lhs, rhs));
    rhs = lhs;
    ++rhs.item_ownership.next_item_sequence;
    ARPG_REQUIRE(!persistence::detail::same_state(lhs, rhs));
    rhs = lhs;
    ++rhs.abyss.reward_revision;
    ARPG_REQUIRE(!persistence::detail::same_state(lhs, rhs));
    rhs = lhs;
    rhs.last_abyss_resolution.room_seed = 1U;
    ARPG_REQUIRE(!persistence::detail::same_state(lhs, rhs));
    rhs = lhs;
    rhs.last_abyss_resolution.lifecycle =
        arpg::abyss::AbyssLifecycle::failed;
    ARPG_REQUIRE(!persistence::detail::same_state(lhs, rhs));
    rhs = lhs;
    ++rhs.death_sequence;
    ARPG_REQUIRE(!persistence::detail::same_state(lhs, rhs));
    lhs = with_pending_death(lhs);
    rhs = lhs;
    ++rhs.death.raw_damage;
    ARPG_REQUIRE(!persistence::detail::same_state(lhs, rhs));

    const auto ownership_state = lhs;
    lhs = make_state(9U, 22U);
    rhs = lhs;
    rhs.skill_loadout.owned_active_bits ^= 0x2U;
    ARPG_REQUIRE(!persistence::detail::same_state(lhs, rhs));
    for (std::size_t slot = 0U;
            slot < skills::kActiveSkillSlotCount; ++slot) {
        rhs = lhs;
        rhs.skill_loadout.slots[slot].active =
            lhs.skill_loadout.slots[slot].active
                == skills::ActiveSkillId::none
            ? skills::ActiveSkillId::draw_slash
            : skills::ActiveSkillId::none;
        ARPG_REQUIRE(!persistence::detail::same_state(lhs, rhs));
    }
    for (std::size_t slot = 0U;
            slot < skills::kActiveSkillSlotCount; ++slot) {
        for (std::size_t support = 0U;
                support < skills::kSupportSlotsPerActive; ++support) {
            rhs = lhs;
            rhs.skill_loadout.slots[slot].supports[support] =
                static_cast<skills::SupportSkillId>(0U);
            ARPG_REQUIRE(!persistence::detail::same_state(lhs, rhs));
        }
    }
    lhs = ownership_state;

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

arpg::test::Failure equal_generation_active_slot_difference_conflicts()
    noexcept {
    TempDirectory directory;
    const auto lhs = make_state(8U, 80U);
    auto rhs = lhs;
    std::swap(rhs.skill_loadout.slots[1U], rhs.skill_loadout.slots[4U]);
    ARPG_REQUIRE(skills::validate_skill_loadout(lhs.skill_loadout)
        == skills::SkillLoadoutError::none);
    ARPG_REQUIRE(skills::validate_skill_loadout(rhs.skill_loadout)
        == skills::SkillLoadoutError::none);
    const auto encoded_lhs = encoded(lhs);
    const auto encoded_rhs = encoded(rhs);
    ARPG_REQUIRE(encoded_lhs.size()
        == persistence::kV8BaseEncodedCheckpointSize);
    ARPG_REQUIRE(encoded_rhs.size()
        == persistence::kV8BaseEncodedCheckpointSize);
    write_bytes(directory.path / "run_a.sav", encoded_lhs);
    write_bytes(directory.path / "run_b.sav", encoded_rhs);

    const auto loaded = make_store(directory.path).load();
    ARPG_REQUIRE(loaded.state == persistence::SaveLoadState::recovery_required);
    ARPG_REQUIRE(loaded.error == persistence::SaveError::conflicting_slots);
    return {};
}

arpg::test::Failure equal_generation_owned_skill_difference_conflicts()
    noexcept {
    TempDirectory directory;
    const auto lhs = make_state(8U, 81U);
    auto rhs = lhs;
    rhs.skill_loadout = skills::SkillLoadoutState{};
    ARPG_REQUIRE(skills::validate_skill_loadout(lhs.skill_loadout)
        == skills::SkillLoadoutError::none);
    ARPG_REQUIRE(skills::validate_skill_loadout(rhs.skill_loadout)
        == skills::SkillLoadoutError::none);
    const auto encoded_lhs = encoded(lhs);
    const auto encoded_rhs = encoded(rhs);
    ARPG_REQUIRE(encoded_lhs.size()
        == persistence::kV8BaseEncodedCheckpointSize);
    ARPG_REQUIRE(encoded_rhs.size()
        == persistence::kV8BaseEncodedCheckpointSize);
    write_bytes(directory.path / "run_a.sav", encoded_lhs);
    write_bytes(directory.path / "run_b.sav", encoded_rhs);

    const auto loaded = make_store(directory.path).load();
    ARPG_REQUIRE(loaded.state == persistence::SaveLoadState::recovery_required);
    ARPG_REQUIRE(loaded.error == persistence::SaveError::conflicting_slots);
    return {};
}

arpg::test::Failure equal_generation_death_sequence_difference_conflicts() noexcept {
    TempDirectory directory;
    const auto lhs = with_pending_death(make_state(7U, 77U));
    auto rhs = lhs;
    ++rhs.death_sequence;
    write_bytes(directory.path / "run_a.sav", encoded(lhs));
    write_bytes(directory.path / "run_b.sav", encoded(rhs));
    auto store = make_store(directory.path);
    const auto loaded = store.load();
    ARPG_REQUIRE(loaded.state == persistence::SaveLoadState::recovery_required);
    ARPG_REQUIRE(loaded.error == persistence::SaveError::conflicting_slots);
    return {};
}

arpg::test::Failure equal_generation_death_field_difference_conflicts() noexcept {
    TempDirectory directory;
    const auto lhs = with_pending_death(make_state(7U, 78U));
    auto rhs = lhs;
    ++rhs.death.raw_damage;
    write_bytes(directory.path / "run_a.sav", encoded(lhs));
    write_bytes(directory.path / "run_b.sav", encoded(rhs));
    auto store = make_store(directory.path);
    const auto loaded = store.load();
    ARPG_REQUIRE(loaded.state == persistence::SaveLoadState::recovery_required);
    ARPG_REQUIRE(loaded.error == persistence::SaveError::conflicting_slots);
    return {};
}

arpg::test::Failure migrated_flag_follows_the_selected_ab_slot() noexcept {
    TempDirectory directory;
    write_bytes(directory.path / "run_a.sav", encoded_v4(make_state(1U, 40U)));
    write_bytes(directory.path / "run_b.sav", encoded(make_state(2U, 41U)));
    auto store = make_store(directory.path);
    auto loaded = store.load();
    ARPG_REQUIRE(loaded.state == persistence::SaveLoadState::ready);
    ARPG_REQUIRE(loaded.active_slot == persistence::SaveSlot::b);
    ARPG_REQUIRE(!loaded.migrated);

    write_bytes(directory.path / "run_b.sav", encoded(make_state(1U, 41U)));
    write_bytes(directory.path / "run_a.sav", encoded_v4(make_state(2U, 40U)));
    loaded = store.load();
    ARPG_REQUIRE(loaded.state == persistence::SaveLoadState::ready);
    ARPG_REQUIRE(loaded.active_slot == persistence::SaveSlot::a);
    ARPG_REQUIRE(loaded.migrated);
    return {};
}

arpg::test::Failure migrated_flag_survives_invalid_slot_recovery() noexcept {
    TempDirectory directory;
    write_bytes(directory.path / "run_a.sav", encoded_v4(make_state(7U, 42U)));
    write_bytes(directory.path / "run_b.sav", {0x01U, 0x02U, 0x03U});
    auto store = make_store(directory.path);
    const auto loaded = store.load();
    ARPG_REQUIRE(loaded.state == persistence::SaveLoadState::ready);
    ARPG_REQUIRE(loaded.active_slot == persistence::SaveSlot::a);
    ARPG_REQUIRE(loaded.recovered);
    ARPG_REQUIRE(loaded.migrated);
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
    {"equal generation active slot difference conflicts",
        &equal_generation_active_slot_difference_conflicts},
    {"equal generation owned skill difference conflicts",
        &equal_generation_owned_skill_difference_conflicts},
    {"equal generation death sequence difference conflicts", &equal_generation_death_sequence_difference_conflicts},
    {"equal generation death field difference conflicts", &equal_generation_death_field_difference_conflicts},
    {"migrated flag follows selected ab slot", &migrated_flag_follows_the_selected_ab_slot},
    {"migrated flag survives invalid slot recovery", &migrated_flag_survives_invalid_slot_recovery},
};

}  // namespace

arpg::test::TestSuite save_store_suite() noexcept {
    return arpg::test::make_suite("save_store", kCases);
}
