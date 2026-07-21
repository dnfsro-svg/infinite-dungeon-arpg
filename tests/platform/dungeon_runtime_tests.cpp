#include "test_framework.hpp"
#include "allocation_probe.hpp"

#include "../dungeon/dungeon_test_support.hpp"
#include "dungeon_runtime.hpp"
#include "combat_renderer.hpp"
#include "raylib_host.hpp"
#include "abyss/abyss_rules.hpp"
#include "abyss/abyss_rewards.hpp"
#include "dungeon/abyss_reward.hpp"
#include "persistence/checkpoint_codec.hpp"
#include "platform/settings/settings_types.hpp"

#include <array>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

namespace {

namespace dungeon = arpg::dungeon;
namespace persistence = arpg::persistence;
namespace platform = arpg::platform;
namespace combat = arpg::combat;
namespace items = arpg::items;

#if defined(_ITERATOR_DEBUG_LEVEL) && _ITERATOR_DEBUG_LEVEL != 0
constexpr std::uint64_t kRuntimeVectorProxyAllocations = 1U;
#else
constexpr std::uint64_t kRuntimeVectorProxyAllocations = 0U;
#endif

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

struct TempDirectory final {
    std::filesystem::path path{};

    TempDirectory() noexcept {
        std::error_code error;
        path = std::filesystem::temp_directory_path(error)
            / "arpg_task10_dungeon_runtime";
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

struct FaultContext final {
    persistence::SaveFaultPoint point{};
    bool enabled{};
};

struct GroundReplacementContext final {
    dungeon::DungeonSession* session{};
    items::ItemInstance replacement{};
    bool armed{};
    bool invoked{};
};

bool fail_when_enabled(persistence::SaveFaultPoint point,
    void* opaque) noexcept {
    const auto* const context = static_cast<const FaultContext*>(opaque);
    return context != nullptr && context->enabled && context->point == point;
}

std::optional<std::uint64_t> provider_seed(void* context) noexcept {
    return context == nullptr ? std::nullopt
                              : std::optional<std::uint64_t>{
                                    *static_cast<std::uint64_t*>(context)};
}

platform::DungeonRuntimeConfig config_for(const TempDirectory& directory,
    std::uint64_t seed = 8U) noexcept {
    platform::DungeonRuntimeConfig config{};
    config.save.directory = directory.path;
    config.new_run_seed = seed;
    return config;
}

dungeon::DungeonRunState runtime_available_state(
    std::uint64_t seed = 1U) noexcept {
    auto state = dungeon::make_initial_run_state(
        0xA811AB1EULL, dungeon::DungeonRules{}).state;
    while (!arpg::abyss::is_abyss_roll(seed)) ++seed;
    state.current_room.seed = seed;
    state.current_room.depth = 40U;
    state.current_room.entry = dungeon::EntrySide::left;
    state.current_room.is_abyss = true;
    state.last_transition = dungeon::TransitionKind::door;
    state.last_direction = dungeon::ExitDirection::right;
    const auto selected = arpg::abyss::select_abyss_rule(seed, 40U);
    if (selected.has_value()) {
        state.abyss.lifecycle = arpg::abyss::AbyssLifecycle::available;
        state.abyss.danger = selected->danger;
        state.abyss.rule = selected->rule;
        state.abyss.rules_version = selected->rules_version;
    }
    return state;
}

void write_u32(std::vector<std::uint8_t>& bytes, std::size_t offset,
    std::uint32_t value) noexcept {
    for (std::size_t index = 0U; index < 4U; ++index)
        bytes[offset + index] = static_cast<std::uint8_t>(value >> (index * 8U));
}

std::vector<std::uint8_t> encode_v6(
    const dungeon::DungeonRunState& state) {
    const auto encoded = persistence::encode_checkpoint(state);
    if (!encoded.has_value()
            || encoded->size() < persistence::kV7BaseEncodedCheckpointSize) {
        return {};
    }

    const std::size_t item_count = state.item_ownership.items.size();
    std::vector<std::uint8_t> v6(
        persistence::kV6BaseEncodedCheckpointSize
            + item_count * persistence::kV4ItemRecordSize,
        0U);
    std::copy_n(encoded->begin(), persistence::kV6BaseEncodedCheckpointSize,
        v6.begin());
    for (std::size_t item_index = 0U; item_index < item_count; ++item_index) {
        const std::size_t v7_record = persistence::kV7BaseEncodedCheckpointSize
            + item_index * persistence::kV7ItemRecordSize;
        const std::size_t v6_record = persistence::kV6BaseEncodedCheckpointSize
            + item_index * persistence::kV4ItemRecordSize;
        std::copy_n(encoded->begin() + v7_record, 16U,
            v6.begin() + v6_record);
        for (std::size_t roll_index = 0U; roll_index < 6U; ++roll_index) {
            std::copy_n(encoded->begin() + v7_record + 16U + roll_index * 6U,
                4U, v6.begin() + v6_record + 16U + roll_index * 4U);
        }
    }
    v6[0U] = 'A'; v6[1U] = 'R'; v6[2U] = 'P'; v6[3U] = 'G';
    v6[4U] = 'S'; v6[5U] = 'V'; v6[6U] = '6'; v6[7U] = '\0';
    write_u32(v6, 8U, persistence::kSixthCheckpointFormatVersion);
    write_u32(v6, 24U, static_cast<std::uint32_t>(v6.size() - 32U));
    auto checksum = persistence::crc32_update(0U, v6.data() + 8U, 20U);
    checksum = persistence::crc32_update(
        checksum, v6.data() + 32U, v6.size() - 32U);
    write_u32(v6, 28U, checksum);
    return v6;
}

std::vector<std::uint8_t> encode_v4(
    const dungeon::DungeonRunState& state) {
    auto encodable = state;
    encodable.current_room.is_abyss = false;
    encodable.abyss = {};
    auto v5 = encode_v6(encodable);
    if (v5.empty()) return {};
    v5.erase(v5.begin() + 236U, v5.begin() + 460U);
    v5[0U] = 'I'; v5[1U] = 'A'; v5[2U] = 'R'; v5[3U] = 'P';
    v5[4U] = 'G'; v5[5U] = 'S'; v5[6U] = '0'; v5[7U] = '6';
    std::vector<std::uint8_t> v4(v5.size() - 32U, 0U);
    std::copy_n(v5.begin(), 120U, v4.begin());
    std::copy(v5.begin() + 152U, v5.end(), v4.begin() + 120U);
    v4[7U] = '5';
    v4[88U] = static_cast<std::uint8_t>(state.current_room.entry);
    v4[91U] = state.current_room.is_abyss ? 1U : 0U;
    v4[92U] = static_cast<std::uint8_t>(state.last_transition);
    v4[93U] = static_cast<std::uint8_t>(state.last_direction);
    write_u32(v4, 8U, 4U);
    write_u32(v4, 24U, static_cast<std::uint32_t>(v4.size() - 32U));
    auto checksum = persistence::crc32_update(0U, v4.data() + 8U, 20U);
    checksum = persistence::crc32_update(
        checksum, v4.data() + 32U, v4.size() - 32U);
    write_u32(v4, 28U, checksum);
    return v4;
}

std::vector<std::uint8_t> encode_v5(
    const dungeon::DungeonRunState& state) {
    auto v5 = encode_v6(state);
    if (v5.empty()) return {};
    v5.erase(v5.begin() + 236U, v5.begin() + 460U);
    const std::array<std::uint8_t, 8U> magic{{
        'I', 'A', 'R', 'P', 'G', 'S', '0', '6'}};
    std::copy(magic.begin(), magic.end(), v5.begin());
    write_u32(v5, 8U, 5U);
    write_u32(v5, 24U, static_cast<std::uint32_t>(v5.size() - 32U));
    auto checksum = persistence::crc32_update(0U, v5.data() + 8U, 20U);
    checksum = persistence::crc32_update(
        checksum, v5.data() + 32U, v5.size() - 32U);
    write_u32(v5, 28U, checksum);
    return v5;
}

bool write_save(const std::filesystem::path& path,
    const std::vector<std::uint8_t>& bytes) noexcept {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out.write(reinterpret_cast<const char*>(bytes.data()),
        static_cast<std::streamsize>(bytes.size()));
    return out.good();
}

bool same_room_descriptor(
    const dungeon::checkpoint::RoomDescriptor& lhs,
    const dungeon::checkpoint::RoomDescriptor& rhs) noexcept {
    return lhs.index == rhs.index && lhs.seed == rhs.seed
        && lhs.depth == rhs.depth
        && lhs.floor_room_index == rhs.floor_room_index
        && lhs.entry == rhs.entry && lhs.ecology == rhs.ecology
        && lhs.has_hole == rhs.has_hole && lhs.is_abyss == rhs.is_abyss;
}

arpg::test::Failure load_available_keeps_start_as_second_transaction() noexcept {
    TempDirectory directory;
    auto config = config_for(directory);
    persistence::SaveStore store(config.save);
    const auto available = runtime_available_state();
    ARPG_REQUIRE(store.commit(available).state
        == persistence::SaveCommitState::committed);
    platform::DungeonRuntime runtime(config);
    ARPG_REQUIRE(runtime.initialize());
    ARPG_REQUIRE(runtime.session()->snapshot().phase
        == dungeon::RoomPhase::committing);
    ARPG_REQUIRE(runtime.session()->pending_save_view()->kind
        == dungeon::PendingSaveKind::abyss_start);
    const auto disk = store.load();
    ARPG_REQUIRE(disk.checkpoint.commit_generation == available.commit_generation);
    ARPG_REQUIRE(disk.checkpoint.abyss.lifecycle
        == arpg::abyss::AbyssLifecycle::available);
    return {};
}

arpg::test::Failure servicing_available_start_creates_started_combat() noexcept {
    TempDirectory directory;
    auto config = config_for(directory);
    persistence::SaveStore store(config.save);
    const auto available = runtime_available_state();
    ARPG_REQUIRE(store.commit(available).state
        == persistence::SaveCommitState::committed);
    platform::DungeonRuntime runtime(config);
    ARPG_REQUIRE(runtime.initialize());
    runtime.service_pending_save();
    ARPG_REQUIRE(runtime.state() == platform::DungeonRuntimeState::running);
    ARPG_REQUIRE(runtime.session()->snapshot().phase == dungeon::RoomPhase::locked);
    ARPG_REQUIRE(runtime.session()->snapshot().combat.has_value());
    ARPG_REQUIRE(store.load().checkpoint.abyss.lifecycle
        == arpg::abyss::AbyssLifecycle::started);
    return {};
}

arpg::test::Failure load_started_commits_failed_before_session() noexcept {
    TempDirectory directory;
    auto config = config_for(directory);
    persistence::SaveStore store(config.save);
    auto started = runtime_available_state();
    started.abyss.lifecycle = arpg::abyss::AbyssLifecycle::started;
    ARPG_REQUIRE(store.commit(started).state
        == persistence::SaveCommitState::committed);
    platform::DungeonRuntime runtime(config);
    ARPG_REQUIRE(runtime.initialize());
    const auto disk = store.load();
    ARPG_REQUIRE(disk.checkpoint.commit_generation
        == started.commit_generation + 1U);
    ARPG_REQUIRE(disk.checkpoint.abyss.lifecycle
        == arpg::abyss::AbyssLifecycle::failed);
    ARPG_REQUIRE(!disk.checkpoint.current_room.is_abyss);
    ARPG_REQUIRE(runtime.session()->snapshot().phase == dungeon::RoomPhase::locked);
    ARPG_REQUIRE(runtime.session()->snapshot().combat.has_value());
    return {};
}

arpg::test::Failure load_started_failure_to_publish_faults_runtime() noexcept {
    TempDirectory directory;
    FaultContext fault{persistence::SaveFaultPoint::before_publish, false};
    auto config = config_for(directory);
    config.save.fault_hook = &fail_when_enabled;
    config.save.fault_context = &fault;
    persistence::SaveStore store(config.save);
    auto started = runtime_available_state();
    started.abyss.lifecycle = arpg::abyss::AbyssLifecycle::started;
    ARPG_REQUIRE(store.commit(started).state
        == persistence::SaveCommitState::committed);
    fault.enabled = true;
    platform::DungeonRuntime runtime(config);
    ARPG_REQUIRE(!runtime.initialize());
    ARPG_REQUIRE(runtime.state() == platform::DungeonRuntimeState::faulted);
    ARPG_REQUIRE(runtime.session() == nullptr);
    return {};
}

arpg::test::Failure migrated_initial_abyss_marker_commits_v5_before_session() noexcept {
    for (const auto transition : std::array<dungeon::TransitionKind, 2U>{{
            dungeon::TransitionKind::none,
            dungeon::TransitionKind::descent}}) {
        TempDirectory directory;
        auto legacy = dungeon::make_initial_run_state(
            0x1E6AC7ULL, dungeon::DungeonRules{}).state;
        legacy.current_room.is_abyss = true;
        legacy.last_transition = transition;
        const auto bytes = encode_v4(legacy);
        ARPG_REQUIRE(!bytes.empty());
        ARPG_REQUIRE(write_save(directory.path / "run_a.sav", bytes));
        platform::DungeonRuntime runtime(config_for(directory));
        ARPG_REQUIRE(runtime.initialize());
        persistence::SaveStore store(config_for(directory).save);
        const auto disk = store.load();
        ARPG_REQUIRE(!disk.migrated);
        ARPG_REQUIRE(disk.checkpoint.commit_generation
            == legacy.commit_generation + 1U);
        ARPG_REQUIRE(!disk.checkpoint.current_room.is_abyss);
        ARPG_REQUIRE(disk.checkpoint.abyss.lifecycle
            == arpg::abyss::AbyssLifecycle::none);
        ARPG_REQUIRE(runtime.session()->snapshot().phase
            == dungeon::RoomPhase::locked);
    }
    return {};
}

arpg::test::Failure migrated_checkpoint_publish_failure_faults_runtime() noexcept {
    TempDirectory directory;
    auto legacy = dungeon::make_initial_run_state(
        0x1E6AC7ULL, dungeon::DungeonRules{}).state;
    legacy.current_room.is_abyss = true;
    const auto bytes = encode_v4(legacy);
    ARPG_REQUIRE(!bytes.empty());
    ARPG_REQUIRE(write_save(directory.path / "run_a.sav", bytes));
    FaultContext fault{persistence::SaveFaultPoint::before_publish, true};
    auto config = config_for(directory);
    config.save.fault_hook = &fail_when_enabled;
    config.save.fault_context = &fault;
    platform::DungeonRuntime runtime(config);
    ARPG_REQUIRE(!runtime.initialize());
    ARPG_REQUIRE(runtime.state() == platform::DungeonRuntimeState::faulted);
    ARPG_REQUIRE(runtime.session() == nullptr);
    return {};
}

arpg::test::Failure migrated_door_available_commits_before_start_pending() noexcept {
    TempDirectory directory;
    auto legacy = runtime_available_state();
    legacy.abyss = {};
    const auto bytes = encode_v4(legacy);
    ARPG_REQUIRE(!bytes.empty());
    ARPG_REQUIRE(write_save(directory.path / "run_a.sav", bytes));
    platform::DungeonRuntime runtime(config_for(directory));
    ARPG_REQUIRE(runtime.initialize());
    persistence::SaveStore store(config_for(directory).save);
    const auto disk = store.load();
    ARPG_REQUIRE(!disk.migrated);
    ARPG_REQUIRE(disk.checkpoint.commit_generation
        == legacy.commit_generation + 1U);
    ARPG_REQUIRE(disk.checkpoint.abyss.lifecycle
        == arpg::abyss::AbyssLifecycle::available);
    ARPG_REQUIRE(runtime.session()->pending_save_view()->kind
        == dungeon::PendingSaveKind::abyss_start);
    ARPG_REQUIRE(runtime.session()->pending_save_view()->expected_generation
        == disk.checkpoint.commit_generation + 1U);
    return {};
}

void drain(dungeon::DungeonSession& session) noexcept {
    while (session.try_pop_event().has_value()) {
    }
    while (session.try_pop_combat_event().has_value()) {
    }
}

bool clear_and_await(dungeon::DungeonSession& session) noexcept {
    for (int tick = 0; tick < 4096; ++tick) {
        const auto snapshot = session.snapshot();
        if (snapshot.phase == dungeon::RoomPhase::awaiting_exit) {
            return true;
        }
        if (snapshot.phase == dungeon::RoomPhase::cleared) {
            session.tick({});
            drain(session);
            continue;
        }
        if (snapshot.phase == dungeon::RoomPhase::combat) {
            arpg::test::force_defeat_current_wave(session);
        }
        session.tick({});
        drain(session);
    }
    return false;
}

bool drive_door_pending(dungeon::DungeonSession& session) noexcept {
    if (!clear_and_await(session)) {
        return false;
    }
    combat::MovementInput movement{1, 0};
    for (int tick = 0; tick < 512; ++tick) {
        const auto snapshot = session.snapshot();
        if (snapshot.phase == dungeon::RoomPhase::committing) {
            return session.pending_transition().has_value();
        }
        if (!snapshot.combat.has_value()) {
            return false;
        }
        movement.y = snapshot.combat->player.position.y > 0.1F ? -1
            : (snapshot.combat->player.position.y < -0.1F ? 1 : 0);
        session.tick(movement);
        drain(session);
    }
    return false;
}

arpg::test::Failure empty_directory_commits_seeded_generation_one_before_running() noexcept {
    TempDirectory directory;
    auto config = config_for(directory);
    platform::DungeonRuntime runtime(config);
    ARPG_REQUIRE(runtime.initialize());
    ARPG_REQUIRE(runtime.state() == platform::DungeonRuntimeState::running);
    ARPG_REQUIRE(runtime.session() != nullptr);
    ARPG_REQUIRE(runtime.session()->snapshot().root_seed == 8U);
    ARPG_REQUIRE(runtime.session()->snapshot().commit_generation == 1U);
    ARPG_REQUIRE(runtime.session()->snapshot().depth == 1U);
    ARPG_REQUIRE(runtime.session()->snapshot().floor_room_index == 1U);
    ARPG_REQUIRE(runtime.session()->snapshot().ecology
        == dungeon::DungeonElement::water);
    ARPG_REQUIRE((runtime.session()->snapshot().biases
        == std::array<std::uint32_t, 4>{{0U, 0U, 0U, 0U}}));
    ARPG_REQUIRE((runtime.session()->snapshot().exits_open
        == std::array<bool, 4>{{false, false, false, false}}));
    persistence::SaveStore store(config.save);
    const auto loaded = store.load();
    ARPG_REQUIRE(loaded.state == persistence::SaveLoadState::ready);
    ARPG_REQUIRE(loaded.checkpoint.root_seed == 8U);
    ARPG_REQUIRE(loaded.checkpoint.commit_generation == 1U);
    return {};
}

arpg::test::Failure valid_save_ignores_new_run_seed_override() noexcept {
    TempDirectory directory;
    auto first = config_for(directory, 8U);
    platform::DungeonRuntime original(first);
    ARPG_REQUIRE(original.initialize());
    const auto original_seed = original.session()->snapshot().root_seed;
    auto second = config_for(directory, 999U);
    platform::DungeonRuntime resumed(second);
    ARPG_REQUIRE(resumed.initialize());
    ARPG_REQUIRE(resumed.session() != nullptr);
    ARPG_REQUIRE(resumed.session()->snapshot().root_seed == original_seed);
    return {};
}

arpg::test::Failure committed_pending_transition_maps_verified_state_and_saved_indicator() noexcept {
    TempDirectory directory;
    platform::DungeonRuntime runtime(config_for(directory));
    ARPG_REQUIRE(runtime.initialize());
    ARPG_REQUIRE(drive_door_pending(*runtime.session()));
    const auto expected = runtime.session()->pending_transition();
    ARPG_REQUIRE(expected.has_value());
    runtime.service_pending_transition();
    const auto snapshot = runtime.session()->snapshot();
    ARPG_REQUIRE(snapshot.phase == dungeon::RoomPhase::transitioning);
    ARPG_REQUIRE(snapshot.commit_generation == expected->next_state.commit_generation);
    ARPG_REQUIRE(snapshot.room_seed == expected->next_state.current_room.seed);
    ARPG_REQUIRE(runtime.render_status().indicator == platform::SaveIndicator::saved);
    return {};
}

arpg::test::Failure committed_passive_save_survives_runtime_restart() noexcept {
    TempDirectory directory;
    platform::DungeonRuntime runtime(config_for(directory));
    ARPG_REQUIRE(runtime.initialize());
    ARPG_REQUIRE(clear_and_await(*runtime.session()));
    ARPG_REQUIRE(runtime.session()->request_passive_allocation(8U));
    runtime.service_pending_save();
    const auto saved = runtime.session()->snapshot();
    ARPG_REQUIRE(saved.passive_tree.allocated_bits == ((1ULL << 0U) | (1ULL << 8U)));
    ARPG_REQUIRE(!saved.passive_save_pending);
    const auto generation = saved.commit_generation;
    runtime.service_pending_save();
    ARPG_REQUIRE(runtime.session()->snapshot().commit_generation == generation);
    platform::DungeonRuntime resumed(config_for(directory, 999U));
    ARPG_REQUIRE(resumed.initialize());
    ARPG_REQUIRE(resumed.session()->snapshot().passive_tree.allocated_bits
        == saved.passive_tree.allocated_bits);
    return {};
}

arpg::test::Failure committed_route_and_refund_survive_runtime_restart() noexcept {
    TempDirectory directory;
    auto config = config_for(directory);
    persistence::SaveStore seed_store(config.save);
    auto initial = dungeon::make_initial_run_state(8U, config.rules).state;
    initial.progression = {4U, 0U, 3U, 3U};
    ARPG_REQUIRE(seed_store.commit(initial).state
        == persistence::SaveCommitState::committed);

    platform::DungeonRuntime runtime(config);
    ARPG_REQUIRE(runtime.initialize());
    ARPG_REQUIRE(clear_and_await(*runtime.session()));
    const auto before = runtime.session()->snapshot();
    for (const std::uint8_t node : {std::uint8_t{8U}, std::uint8_t{9U},
            std::uint8_t{10U}}) {
        ARPG_REQUIRE(runtime.session()->request_passive_allocation(node));
        runtime.service_pending_save();
        ARPG_REQUIRE(!runtime.session()->snapshot().passive_save_pending);
        ARPG_REQUIRE(runtime.render_status().indicator == platform::SaveIndicator::saved);
    }
    ARPG_REQUIRE(runtime.session()->request_passive_refund(10U));
    runtime.service_pending_save();
    const auto saved = runtime.session()->snapshot();
    constexpr std::uint64_t kExpectedBits = (1ULL << 0U) | (1ULL << 8U)
        | (1ULL << 9U);
    ARPG_REQUIRE(saved.passive_tree.allocated_bits == kExpectedBits);
    ARPG_REQUIRE(saved.progression.unspent_passive_points
        == before.progression.unspent_passive_points - 2U);
    ARPG_REQUIRE(!saved.passive_save_pending);

    platform::DungeonRuntime resumed(config_for(directory, 999U));
    ARPG_REQUIRE(resumed.initialize());
    const auto restored = resumed.session()->snapshot();
    ARPG_REQUIRE(restored.passive_tree.allocated_bits == kExpectedBits);
    ARPG_REQUIRE(restored.progression.unspent_passive_points
        == saved.progression.unspent_passive_points);
    ARPG_REQUIRE(!restored.passive_save_pending);
    return {};
}

arpg::test::Failure passive_pre_publish_failure_keeps_old_tree_and_retryable_runtime() noexcept {
    TempDirectory directory;
    FaultContext fault{persistence::SaveFaultPoint::before_publish, false};
    auto config = config_for(directory);
    config.save.fault_hook = &fail_when_enabled;
    config.save.fault_context = &fault;
    platform::DungeonRuntime runtime(config);
    ARPG_REQUIRE(runtime.initialize());
    ARPG_REQUIRE(clear_and_await(*runtime.session()));
    const auto before = runtime.session()->snapshot();
    ARPG_REQUIRE(runtime.session()->request_passive_allocation(8U));
    fault.enabled = true;
    runtime.service_pending_save();
    const auto after = runtime.session()->snapshot();
    ARPG_REQUIRE(after.passive_tree.allocated_bits == before.passive_tree.allocated_bits);
    ARPG_REQUIRE(after.commit_generation == before.commit_generation);
    ARPG_REQUIRE(after.phase == dungeon::RoomPhase::awaiting_exit);
    ARPG_REQUIRE(!after.passive_save_pending);
    ARPG_REQUIRE(runtime.state() == platform::DungeonRuntimeState::running);
    ARPG_REQUIRE(runtime.render_status().indicator == platform::SaveIndicator::error);
    platform::DungeonRuntime resumed(config_for(directory, 999U));
    ARPG_REQUIRE(resumed.initialize());
    const auto restored = resumed.session()->snapshot();
    ARPG_REQUIRE(restored.passive_tree.allocated_bits
        == before.passive_tree.allocated_bits);
    ARPG_REQUIRE(restored.commit_generation == before.commit_generation);
    return {};
}

arpg::test::Failure indeterminate_passive_save_faults_runtime() noexcept {
    TempDirectory directory;
    FaultContext fault{persistence::SaveFaultPoint::after_publish, false};
    auto config = config_for(directory);
    config.save.fault_hook = &fail_when_enabled;
    config.save.fault_context = &fault;
    platform::DungeonRuntime runtime(config);
    ARPG_REQUIRE(runtime.initialize());
    ARPG_REQUIRE(clear_and_await(*runtime.session()));
    ARPG_REQUIRE(runtime.session()->request_passive_allocation(8U));
    fault.enabled = true;
    runtime.service_pending_save();
    ARPG_REQUIRE(runtime.session()->snapshot().phase == dungeon::RoomPhase::faulted);
    ARPG_REQUIRE(runtime.state() == platform::DungeonRuntimeState::faulted);
    ARPG_REQUIRE(runtime.render_status().faulted);
    ARPG_REQUIRE(!runtime.render_status().recovery_required);
    return {};
}

arpg::test::Failure passive_pending_rejects_door_and_descent_requests() noexcept {
    TempDirectory directory;
    platform::DungeonRuntime runtime(config_for(directory));
    ARPG_REQUIRE(runtime.initialize());
    ARPG_REQUIRE(clear_and_await(*runtime.session()));
    ARPG_REQUIRE(runtime.session()->request_passive_allocation(8U));
    const auto pending = runtime.session()->pending_save();
    ARPG_REQUIRE(pending.has_value());
    ARPG_REQUIRE(pending->kind == dungeon::PendingSaveKind::passive_tree);
    ARPG_REQUIRE(!runtime.session()->request_descent(true));
    const auto before_door = runtime.session()->snapshot();
    arpg::test::attempt_exit(*runtime.session(), dungeon::ExitDirection::right);
    const auto snapshot = runtime.session()->snapshot();
    ARPG_REQUIRE(snapshot.phase == dungeon::RoomPhase::committing);
    ARPG_REQUIRE(snapshot.passive_save_pending);
    ARPG_REQUIRE(!snapshot.has_pending_transition);
    ARPG_REQUIRE(snapshot.diagnostics.rejected_exit_count
        == before_door.diagnostics.rejected_exit_count + 1U);
    return {};
}

arpg::test::Failure pre_publish_not_committed_maps_to_retryable_error() noexcept {
    TempDirectory directory;
    FaultContext fault{persistence::SaveFaultPoint::before_publish, false};
    auto config = config_for(directory);
    config.save.fault_hook = &fail_when_enabled;
    config.save.fault_context = &fault;
    platform::DungeonRuntime runtime(config);
    ARPG_REQUIRE(runtime.initialize());
    ARPG_REQUIRE(drive_door_pending(*runtime.session()));
    fault.enabled = true;
    runtime.service_pending_transition();
    ARPG_REQUIRE(runtime.session()->snapshot().phase
        == dungeon::RoomPhase::awaiting_exit);
    ARPG_REQUIRE(runtime.render_status().indicator == platform::SaveIndicator::error);
    fault.enabled = false;
    ARPG_REQUIRE(drive_door_pending(*runtime.session()));
    runtime.service_pending_transition();
    ARPG_REQUIRE(runtime.session()->snapshot().phase
        == dungeon::RoomPhase::transitioning);
    return {};
}

arpg::test::Failure indeterminate_maps_to_faulted_runtime_and_blocks_selection() noexcept {
    TempDirectory directory;
    FaultContext fault{persistence::SaveFaultPoint::after_publish, false};
    auto config = config_for(directory);
    config.save.fault_hook = &fail_when_enabled;
    config.save.fault_context = &fault;
    platform::DungeonRuntime runtime(config);
    ARPG_REQUIRE(runtime.initialize());
    ARPG_REQUIRE(drive_door_pending(*runtime.session()));
    fault.enabled = true;
    runtime.service_pending_transition();
    ARPG_REQUIRE(runtime.session()->snapshot().phase == dungeon::RoomPhase::faulted);
    ARPG_REQUIRE(runtime.state() == platform::DungeonRuntimeState::faulted);
    ARPG_REQUIRE(!runtime.session()->request_descent(true));
    return {};
}

arpg::test::Failure dual_slot_corruption_requires_recovery_then_archives_new_run() noexcept {
    TempDirectory directory;
    const char invalid[] = "bad";
    std::ofstream(directory.path / "run_a.sav", std::ios::binary).write(invalid, 3);
    std::ofstream(directory.path / "run_b.sav", std::ios::binary).write(invalid, 3);
    platform::DungeonRuntime runtime(config_for(directory));
    ARPG_REQUIRE(!runtime.initialize());
    ARPG_REQUIRE(runtime.state() == platform::DungeonRuntimeState::recovery_required);
    ARPG_REQUIRE(runtime.render_status().recovery_required);
    ARPG_REQUIRE(!runtime.render_status().faulted);
    ARPG_REQUIRE(runtime.session() == nullptr);
    ARPG_REQUIRE(runtime.item_state() == nullptr);
    ARPG_REQUIRE(runtime.request_pickup(0U) == dungeon::RequestResult::rejected);
    ARPG_REQUIRE(runtime.request_equip(1U) == dungeon::RequestResult::rejected);
    ARPG_REQUIRE(runtime.request_unequip(items::ItemSlot::weapon)
        == dungeon::RequestResult::rejected);
    ARPG_REQUIRE(runtime.request_recipe({{1U, 2U, 3U}})
        == dungeon::RequestResult::rejected);
    ARPG_REQUIRE(runtime.recover_with_new_run());
    ARPG_REQUIRE(runtime.state() == platform::DungeonRuntimeState::running);
    ARPG_REQUIRE(runtime.session() != nullptr);
    ARPG_REQUIRE(runtime.session()->snapshot().commit_generation == 1U);
    return {};
}

arpg::test::Failure single_slot_corruption_recovers_and_subsequent_saves_alternate() noexcept {
    TempDirectory directory;
    auto config = config_for(directory);
    persistence::SaveStore store(config.save);
    const auto initial = dungeon::make_initial_run_state(8U, config.rules).state;
    ARPG_REQUIRE(store.commit(initial).state == persistence::SaveCommitState::committed);
    const auto active = store.load().active_slot;
    const auto bad_slot = active == persistence::SaveSlot::a ? "run_b.sav" : "run_a.sav";
    const char invalid[] = "bad";
    std::ofstream(directory.path / bad_slot, std::ios::binary).write(invalid, 3);
    platform::DungeonRuntime runtime(config);
    ARPG_REQUIRE(runtime.initialize());
    ARPG_REQUIRE(runtime.render_status().indicator == platform::SaveIndicator::recovered);
    ARPG_REQUIRE(drive_door_pending(*runtime.session()));
    runtime.service_pending_transition();
    const auto first_slot = runtime.render_status().active_slot;
    runtime.session()->tick({});
    ARPG_REQUIRE(drive_door_pending(*runtime.session()));
    runtime.service_pending_transition();
    ARPG_REQUIRE(runtime.render_status().active_slot != first_slot);
    return {};
}

arpg::test::Failure invalid_rules_fault_without_creating_save_or_session() noexcept {
    for (const int variant : {0, 1, 2}) {
        TempDirectory directory;
        auto config = config_for(directory);
        if (variant == 0) {
            config.rules.rules_version = 2U;
        } else if (variant == 1) {
            config.rules.hole_threshold = 10001U;
        } else {
            config.rules.base_weights[0] = 0U;
        }
        std::uint64_t provided = 17U;
        config.seed_provider = &provider_seed;
        config.seed_context = &provided;
        platform::DungeonRuntime runtime(config);
        ARPG_REQUIRE(!runtime.initialize());
        ARPG_REQUIRE(runtime.state() == platform::DungeonRuntimeState::faulted);
        ARPG_REQUIRE(runtime.session() == nullptr);
        ARPG_REQUIRE(!std::filesystem::exists(directory.path / "run_a.sav"));
        ARPG_REQUIRE(!std::filesystem::exists(directory.path / "run_b.sav"));
    }
    return {};
}

bool same_item(const items::ItemInstance& left,
    const items::ItemInstance& right) noexcept {
    return std::memcmp(&left, &right, sizeof(items::ItemInstance)) == 0;
}

bool same_ownership(const items::ItemOwnershipState& left,
    const items::ItemOwnershipState& right) noexcept {
    if (left.items.size() != right.items.size()
            || left.equipment.equipped_ids != right.equipment.equipped_ids
            || left.materials != right.materials
            || left.material_discovery_bits != right.material_discovery_bits
            || left.claimed_drop_bits != right.claimed_drop_bits
            || left.next_item_sequence != right.next_item_sequence) {
        return false;
    }
    for (std::size_t index = 0U; index < left.items.size(); ++index) {
        if (!same_item(left.items[index], right.items[index])) return false;
    }
    return true;
}

bool same_build(const combat::PlayerCombatBuild& left,
    const combat::PlayerCombatBuild& right) noexcept {
    const auto& a = left.values;
    const auto& b = right.values;
    return left.weapon_physical == right.weapon_physical
        && left.local_attack_speed_bp == right.local_attack_speed_bp
        && a.flat_damage == b.flat_damage
        && a.damage_increased == b.damage_increased
        && a.damage_reduction == b.damage_reduction
        && a.damage_reduction_cap_bonus == b.damage_reduction_cap_bonus
        && a.armor == b.armor && a.evasion == b.evasion
        && a.melee_damage == b.melee_damage
        && a.max_health == b.max_health
        && a.max_health_more == b.max_health_more
        && a.max_barrier == b.max_barrier
        && a.damage_taken == b.damage_taken
        && a.movement_speed == b.movement_speed
        && a.attack_speed == b.attack_speed
        && a.impulse_scale == b.impulse_scale
        && a.jump_speed == b.jump_speed
        && a.air_control == b.air_control && a.valid == b.valid;
}

enum class ItemRequestKind : std::uint8_t {
    pickup,
    equip,
    unequip,
    recipe,
};

dungeon::DungeonRunState item_state_for(ItemRequestKind kind) {
    auto state = dungeon::make_initial_run_state(0x81818181U, {}).state;
    state.item_ownership.next_item_sequence = 9U;
    if (kind == ItemRequestKind::equip) {
        state.item_ownership.items.push_back(normal_item(101U));
    } else if (kind == ItemRequestKind::unequip) {
        state.item_ownership.items.push_back(normal_item(201U));
        state.item_ownership.equipment.equipped_ids[0] = 201U;
    } else if (kind == ItemRequestKind::recipe) {
        state.item_ownership.items.push_back(normal_item(301U));
        state.item_ownership.items.push_back(normal_item(302U));
        state.item_ownership.items.push_back(normal_item(303U));
    }
    return state;
}

dungeon::RequestResult issue_item_request(platform::DungeonRuntime& runtime,
    ItemRequestKind kind) noexcept {
    switch (kind) {
    case ItemRequestKind::pickup:
        return runtime.request_pickup(0U);
    case ItemRequestKind::equip:
        return runtime.request_equip(101U);
    case ItemRequestKind::unequip:
        return runtime.request_unequip(items::ItemSlot::weapon);
    case ItemRequestKind::recipe:
        return runtime.request_recipe({{301U, 302U, 303U}});
    }
    return dungeon::RequestResult::rejected;
}

bool install_pickup_if_needed(platform::DungeonRuntime& runtime,
    ItemRequestKind kind) noexcept {
    if (kind != ItemRequestKind::pickup) return true;
    const auto snapshot = runtime.session()->snapshot();
    if (!snapshot.combat.has_value()) return false;
    arpg::test::install_ground_item(*runtime.session(), 0U,
        normal_item(401U, 2U), snapshot.combat->player.position);
    return runtime.session()->snapshot().ground_item_count == 1U;
}

bool same_receipt(const platform::LootPickupReceipt& left,
    const platform::LootPickupReceipt& right) noexcept {
    return left.valid == right.valid
        && left.commit_generation == right.commit_generation
        && left.item_id == right.item_id
        && left.base_id == right.base_id
        && left.item_level == right.item_level
        && left.rarity == right.rarity
        && left.source == right.source;
}

bool replace_ground_before_publish(persistence::SaveFaultPoint point,
    void* opaque) noexcept {
    auto* const context = static_cast<GroundReplacementContext*>(opaque);
    if (context != nullptr && context->armed && context->session != nullptr
            && point == persistence::SaveFaultPoint::before_publish) {
        arpg::test::install_ground_item(*context->session, 0U,
            context->replacement, {0.0F, 0.0F, 0.0F});
        context->invoked = true;
    }
    return false;
}

dungeon::DungeonRunState runtime_cleared_abyss_state() noexcept {
    constexpr std::uint64_t kDepth = 20U;
    constexpr std::uint64_t kLowDangerAbyssSeed = 1446U;
    dungeon::DungeonRunState state = dungeon::make_initial_run_state(
        0xA9B9555EEDULL, dungeon::DungeonRules{}).state;
    const auto selection = arpg::abyss::select_abyss_rule(
        kLowDangerAbyssSeed, kDepth);
    if (arpg::abyss::is_abyss_roll(kLowDangerAbyssSeed)
            && selection.has_value()
            && selection->danger == arpg::abyss::AbyssDanger::low) {
        state.current_room.seed = kLowDangerAbyssSeed;
        state.current_room.depth = kDepth;
        state.current_room.entry = dungeon::EntrySide::left;
        state.current_room.is_abyss = true;
        state.current_room.has_hole = true;
        state.last_transition = dungeon::TransitionKind::door;
        state.last_direction = dungeon::ExitDirection::right;
        state.abyss.lifecycle = arpg::abyss::AbyssLifecycle::cleared;
        state.abyss.danger = selection->danger;
        state.abyss.rule = selection->rule;
        state.abyss.rules_version = selection->rules_version;
        state.abyss.reward_total = arpg::abyss::reward_profile_for(
            selection->danger, 1U).item_count;
        state.abyss.generated_mask = 1U;
        state.abyss.reward_revision = 1U;
        return state;
    }
    return {};
}

arpg::test::Failure loot_filter_modes_map_to_pickup_policy() noexcept {
    ARPG_REQUIRE(platform::loot_pickup_policy(
        arpg::settings::LootFilterMode::show_all).minimum_rarity
        == items::ItemRarity::normal);
    ARPG_REQUIRE(platform::loot_pickup_policy(
        arpg::settings::LootFilterMode::magic_or_better).minimum_rarity
        == items::ItemRarity::magic);
    ARPG_REQUIRE(platform::loot_pickup_policy(
        arpg::settings::LootFilterMode::rare_only).minimum_rarity
        == items::ItemRarity::rare);
    ARPG_REQUIRE(platform::loot_pickup_policy(
        static_cast<arpg::settings::LootFilterMode>(0xFFU)).minimum_rarity
        == items::ItemRarity::normal);
    return {};
}

arpg::test::Failure fixed_tick_forwards_pickup_policy_and_defaults_show_all() noexcept {
    TempDirectory filtered_directory;
    platform::DungeonRuntime filtered(config_for(filtered_directory, 0xF117E2U));
    ARPG_REQUIRE(filtered.initialize());
    const auto filtered_before = filtered.session()->snapshot();
    ARPG_REQUIRE(filtered_before.combat.has_value());
    arpg::test::install_ground_item(*filtered.session(), 0U,
        normal_item(0xF117E201U), filtered_before.combat->player.position);

    filtered.fixed_tick({}, {items::ItemRarity::rare});

    ARPG_REQUIRE(filtered.session()->snapshot().ground_item_count == 1U);
    ARPG_REQUIRE(filtered.session()->pending_save_view() == nullptr);

    TempDirectory default_directory;
    platform::DungeonRuntime default_runtime(
        config_for(default_directory, 0xDEF4017U));
    ARPG_REQUIRE(default_runtime.initialize());
    const auto default_before = default_runtime.session()->snapshot();
    ARPG_REQUIRE(default_before.combat.has_value());
    arpg::test::install_ground_item(*default_runtime.session(), 0U,
        normal_item(0xDEF401701U), default_before.combat->player.position);

    default_runtime.fixed_tick({});

    ARPG_REQUIRE(default_runtime.session()->snapshot().ground_item_count == 0U);
    ARPG_REQUIRE(default_runtime.session()->pending_save_view() == nullptr);
    return {};
}

arpg::test::Failure synchronous_pickup_publishes_exact_committed_receipt()
    noexcept {
    TempDirectory directory;
    platform::DungeonRuntime runtime(config_for(directory, 0x51A7E11U));
    ARPG_REQUIRE(runtime.initialize());
    const auto before = runtime.session()->snapshot();
    ARPG_REQUIRE(before.combat.has_value());
    const items::ItemInstance item = normal_item(0x51A7E1101U, 3U);
    arpg::test::install_ground_item(*runtime.session(), 0U, item,
        before.combat->player.position);

    runtime.fixed_tick({});

    const auto after = runtime.session()->snapshot();
    const auto receipt = runtime.render_status().loot_pickup;
    ARPG_REQUIRE(after.commit_generation == before.commit_generation + 1U);
    ARPG_REQUIRE(after.ground_item_count == 0U);
    ARPG_REQUIRE(receipt.valid);
    ARPG_REQUIRE(receipt.commit_generation == after.commit_generation);
    ARPG_REQUIRE(receipt.item_id == item.id);
    ARPG_REQUIRE(receipt.base_id == item.base_id);
    ARPG_REQUIRE(receipt.item_level == item.item_level);
    ARPG_REQUIRE(receipt.rarity == item.rarity);
    ARPG_REQUIRE(receipt.source == dungeon::GroundItemSource::monster_drop);
    return {};
}

arpg::test::Failure abyss_claim_publishes_committed_abyss_receipt() noexcept {
    TempDirectory directory;
    auto config = config_for(directory, 0xAB155U);
    persistence::SaveStore seed_store(config.save);
    const auto initial = runtime_cleared_abyss_state();
    ARPG_REQUIRE(initial.current_room.is_abyss);
    ARPG_REQUIRE(seed_store.commit(initial).state
        == persistence::SaveCommitState::committed);
    platform::DungeonRuntime runtime(config);
    ARPG_REQUIRE(runtime.initialize());
    const auto before = runtime.session()->snapshot();
    ARPG_REQUIRE(before.ground_item_count == 1U);
    const auto item = before.ground_items[0];
    ARPG_REQUIRE(item.source == dungeon::GroundItemSource::abyss_chest);
    arpg::test::set_player_position(*runtime.session(), item.position);
    ARPG_REQUIRE(runtime.request_pickup(item.ordinal)
        == dungeon::RequestResult::accepted);

    runtime.service_pending_save();

    const auto after = runtime.session()->snapshot();
    const auto receipt = runtime.render_status().loot_pickup;
    ARPG_REQUIRE(after.commit_generation == before.commit_generation + 1U);
    ARPG_REQUIRE(after.ground_item_count == 0U);
    ARPG_REQUIRE(receipt.valid);
    ARPG_REQUIRE(receipt.commit_generation == after.commit_generation);
    ARPG_REQUIRE(receipt.item_id == item.item_id);
    ARPG_REQUIRE(receipt.source == dungeon::GroundItemSource::abyss_chest);
    return {};
}

arpg::test::Failure failed_and_nonpickup_saves_do_not_replace_receipt()
    noexcept {
    TempDirectory directory;
    FaultContext fault{persistence::SaveFaultPoint::before_publish, false};
    auto config = config_for(directory, 0xFA17E11U);
    config.save.fault_hook = &fail_when_enabled;
    config.save.fault_context = &fault;
    platform::DungeonRuntime runtime(config);
    ARPG_REQUIRE(runtime.initialize());
    const auto start = runtime.session()->snapshot();
    ARPG_REQUIRE(start.combat.has_value());
    const items::ItemInstance first = normal_item(0xFA17E1101U, 2U);
    arpg::test::install_ground_item(*runtime.session(), 0U, first,
        start.combat->player.position);
    runtime.fixed_tick({});
    const auto confirmed = runtime.render_status().loot_pickup;
    ARPG_REQUIRE(confirmed.valid);

    ARPG_REQUIRE(runtime.request_equip(first.id)
        == dungeon::RequestResult::accepted);
    runtime.service_pending_save();
    ARPG_REQUIRE(runtime.render_status().loot_pickup.commit_generation
        == confirmed.commit_generation);
    ARPG_REQUIRE(runtime.render_status().loot_pickup.item_id
        == confirmed.item_id);

    const auto positioned = runtime.session()->snapshot();
    ARPG_REQUIRE(positioned.combat.has_value());
    const items::ItemInstance second = normal_item(0xFA17E1102U, 1U);
    arpg::test::install_ground_item(*runtime.session(), 0U, second,
        positioned.combat->player.position);
    ARPG_REQUIRE(runtime.request_pickup(0U) == dungeon::RequestResult::accepted);
    fault.enabled = true;
    runtime.service_pending_save();
    ARPG_REQUIRE(runtime.render_status().indicator == platform::SaveIndicator::error);
    ARPG_REQUIRE(runtime.render_status().loot_pickup.commit_generation
        == confirmed.commit_generation);
    ARPG_REQUIRE(runtime.render_status().loot_pickup.item_id
        == confirmed.item_id);
    ARPG_REQUIRE(runtime.session()->snapshot().ground_item_count == 1U);
    return {};
}

arpg::test::Failure wrong_pending_ordinal_fault_does_not_publish_receipt()
    noexcept {
    TempDirectory directory;
    platform::DungeonRuntime runtime(config_for(directory, 0xBAD0D1U));
    ARPG_REQUIRE(runtime.initialize());
    auto before = runtime.session()->snapshot();
    ARPG_REQUIRE(before.combat.has_value());
    arpg::test::install_ground_item(*runtime.session(), 0U,
        normal_item(0xBAD0D100U), before.combat->player.position);
    runtime.fixed_tick({});
    const auto confirmed = runtime.render_status().loot_pickup;
    ARPG_REQUIRE(confirmed.valid);

    before = runtime.session()->snapshot();
    ARPG_REQUIRE(before.combat.has_value());
    arpg::test::install_ground_item(*runtime.session(), 0U,
        normal_item(0xBAD0D101U), before.combat->player.position);
    ARPG_REQUIRE(runtime.request_pickup(0U) == dungeon::RequestResult::accepted);
    arpg::test::DungeonSessionTestAccess::set_pending_pickup_ordinal(
        *runtime.session(), 191U);

    runtime.service_pending_save();

    ARPG_REQUIRE(runtime.state() == platform::DungeonRuntimeState::faulted);
    ARPG_REQUIRE(runtime.render_status().faulted);
    ARPG_REQUIRE(same_receipt(runtime.render_status().loot_pickup, confirmed));
    return {};
}

arpg::test::Failure replaced_pickup_ordinal_does_not_publish_receipt()
    noexcept {
    TempDirectory directory;
    GroundReplacementContext replacement{};
    replacement.replacement = normal_item(0xA17E2202U);
    auto config = config_for(directory, 0xA17E22U);
    config.save.fault_hook = &replace_ground_before_publish;
    config.save.fault_context = &replacement;
    platform::DungeonRuntime runtime(config);
    ARPG_REQUIRE(runtime.initialize());
    replacement.session = runtime.session();
    auto before = runtime.session()->snapshot();
    ARPG_REQUIRE(before.combat.has_value());
    arpg::test::install_ground_item(*runtime.session(), 0U,
        normal_item(0xA17E2200U), before.combat->player.position);
    runtime.fixed_tick({});
    const auto confirmed = runtime.render_status().loot_pickup;
    ARPG_REQUIRE(confirmed.valid);

    before = runtime.session()->snapshot();
    ARPG_REQUIRE(before.combat.has_value());
    arpg::test::install_ground_item(*runtime.session(), 0U,
        normal_item(0xA17E2201U), before.combat->player.position);
    ARPG_REQUIRE(runtime.request_pickup(0U) == dungeon::RequestResult::accepted);
    replacement.armed = true;

    runtime.service_pending_save();

    ARPG_REQUIRE(replacement.invoked);
    ARPG_REQUIRE(runtime.state() == platform::DungeonRuntimeState::faulted);
    ARPG_REQUIRE(runtime.render_status().faulted);
    ARPG_REQUIRE(same_receipt(runtime.render_status().loot_pickup, confirmed));
    const auto after = runtime.session()->snapshot();
    ARPG_REQUIRE(after.ground_item_count == 1U);
    ARPG_REQUIRE(after.ground_items[0].ordinal == 0U);
    ARPG_REQUIRE(after.ground_items[0].item_id == replacement.replacement.id);
    return {};
}

arpg::test::Failure failed_pickup_retry_publishes_one_presented_hud_notice()
    noexcept {
    TempDirectory directory;
    FaultContext fault{persistence::SaveFaultPoint::before_publish, false};
    auto config = config_for(directory, 0xFEED771U);
    config.save.fault_hook = &fail_when_enabled;
    config.save.fault_context = &fault;
    platform::DungeonRuntime runtime(config);
    ARPG_REQUIRE(runtime.initialize());
    platform::CombatRenderer renderer{};
    platform::ControlHints hints{};
    auto previous = runtime.session()->snapshot();
    renderer.observe_presented_hud_frame(platform::HudPresentedFrame::normal,
        previous, previous, runtime.render_status(), hints, 0.0F, false);
    ARPG_REQUIRE(previous.combat.has_value());
    arpg::test::install_ground_item(*runtime.session(), 0U,
        normal_item(0xFEED77101U), previous.combat->player.position);

    fault.enabled = true;
    runtime.fixed_tick({});
    auto failed = runtime.session()->snapshot();
    ARPG_REQUIRE(runtime.render_status().indicator == platform::SaveIndicator::error);
    ARPG_REQUIRE(!runtime.render_status().loot_pickup.valid);
    renderer.observe_presented_hud_frame(platform::HudPresentedFrame::normal,
        previous, failed, runtime.render_status(), hints, 0.0F, false);

    fault.enabled = false;
    runtime.fixed_tick({});
    const auto committed = runtime.session()->snapshot();
    ARPG_REQUIRE(runtime.render_status().loot_pickup.valid);
    renderer.observe_presented_hud_frame(platform::HudPresentedFrame::normal,
        failed, committed, runtime.render_status(), hints, 0.0F, false);
    ARPG_REQUIRE(renderer.hud_notice_view().primary.kind
        == platform::HudNoticeKind::loot_pickup);
    ARPG_REQUIRE(renderer.hud_notice_view().primary.seconds_left == 3.0F);
    renderer.observe_presented_hud_frame(platform::HudPresentedFrame::normal,
        committed, committed, runtime.render_status(), hints, 1.0F, false);
    ARPG_REQUIRE(renderer.hud_notice_view().primary.kind
        == platform::HudNoticeKind::loot_pickup);
    ARPG_REQUIRE(renderer.hud_notice_view().primary.seconds_left == 2.0F);
    return {};
}

arpg::test::Failure runtime_exposes_narrow_item_requests_and_stable_item_view() noexcept {
    TempDirectory directory;
    auto config = config_for(directory);
    auto initial = dungeon::make_initial_run_state(8U, config.rules).state;
    initial.item_ownership.items.push_back(normal_item(101U));
    initial.item_ownership.next_item_sequence = 102U;
    persistence::SaveStore seed_store(config.save);
    ARPG_REQUIRE(seed_store.commit(initial).state
        == persistence::SaveCommitState::committed);

    platform::DungeonRuntime runtime(config);
    ARPG_REQUIRE(runtime.initialize());
    ARPG_REQUIRE(runtime.item_state() != nullptr);
    ARPG_REQUIRE(runtime.item_state() == &runtime.session()->item_state());
    ARPG_REQUIRE(runtime.item_state()->items.size() == 1U);
    ARPG_REQUIRE(runtime.request_equip(101U) == dungeon::RequestResult::accepted);
    ARPG_REQUIRE(runtime.request_pickup(0U) == dungeon::RequestResult::rejected);
    ARPG_REQUIRE(runtime.request_unequip(items::ItemSlot::weapon)
        == dungeon::RequestResult::rejected);
    ARPG_REQUIRE(runtime.request_recipe({{101U, 102U, 103U}})
        == dungeon::RequestResult::rejected);
    return {};
}

arpg::test::Failure large_inventory_snapshot_and_views_do_not_allocate() noexcept {
    constexpr std::size_t kItemCount = 65535U;
    auto state = dungeon::make_initial_run_state(0x65535U, {}).state;
    state.item_ownership.items.reserve(kItemCount);
    for (std::size_t index = 0U; index < kItemCount; ++index) {
        state.item_ownership.items.push_back(normal_item(index + 1U));
    }
    state.item_ownership.next_item_sequence = kItemCount + 1U;
    dungeon::DungeonSession session{{}, std::move(state)};
    platform::DungeonRuntime* no_runtime = nullptr;
    ARPG_REQUIRE(session.snapshot().inventory_count == kItemCount);
    ARPG_REQUIRE(session.pending_save_view() == nullptr);

    const std::uint64_t before = arpg::test::allocation_count();
    for (int repetition = 0; repetition < 8; ++repetition) {
        const auto snapshot = session.snapshot();
        ARPG_REQUIRE(snapshot.inventory_count == kItemCount);
        ARPG_REQUIRE(snapshot.ground_item_count == 0U);
        ARPG_REQUIRE(session.item_state().items.size() == kItemCount);
        ARPG_REQUIRE(session.pending_save_view() == nullptr);
        ARPG_REQUIRE(no_runtime == nullptr);
    }
    ARPG_REQUIRE(arpg::test::allocation_count() == before);

    ARPG_REQUIRE(session.request_equip(1U)
        == dungeon::RequestResult::accepted);
    ARPG_REQUIRE(session.pending_save_view() != nullptr);
    const std::uint64_t pending_before = arpg::test::allocation_count();
    for (int repetition = 0; repetition < 8; ++repetition) {
        const dungeon::PendingSave* const pending = session.pending_save_view();
        ARPG_REQUIRE(pending != nullptr);
        ARPG_REQUIRE(pending->next_state.item_ownership.items.size()
            == kItemCount);
    }
    ARPG_REQUIRE(arpg::test::allocation_count() == pending_before);
    return {};
}

arpg::test::Failure generic_service_adds_no_large_state_copies() noexcept {
    constexpr std::size_t kItemCount = 65535U;
    auto initial = dungeon::make_initial_run_state(0xC0FFEEU, {}).state;
    initial.item_ownership.items.reserve(kItemCount);
    for (std::size_t index = 0U; index < kItemCount; ++index) {
        initial.item_ownership.items.push_back(normal_item(index + 1U));
    }
    initial.item_ownership.next_item_sequence = kItemCount + 1U;

    TempDirectory runtime_directory;
    TempDirectory direct_directory;
    auto runtime_config = config_for(runtime_directory);
    auto direct_config = config_for(direct_directory);
    persistence::SaveStore runtime_seed(runtime_config.save);
    persistence::SaveStore direct_store(direct_config.save);
    ARPG_REQUIRE(runtime_seed.commit(initial).state
        == persistence::SaveCommitState::committed);
    ARPG_REQUIRE(direct_store.commit(initial).state
        == persistence::SaveCommitState::committed);

    platform::DungeonRuntime runtime(runtime_config);
    ARPG_REQUIRE(runtime.initialize());
    ARPG_REQUIRE(runtime.request_equip(1U) == dungeon::RequestResult::accepted);
    const dungeon::PendingSave* const pending =
        runtime.session()->pending_save_view();
    ARPG_REQUIRE(pending != nullptr);
    const dungeon::DungeonRunState expected = pending->next_state;

    const std::uint64_t direct_before = arpg::test::allocation_count();
    const persistence::SaveCommitResult direct = direct_store.commit(expected);
    const std::uint64_t direct_allocations =
        arpg::test::allocation_count() - direct_before;
    ARPG_REQUIRE(direct.state == persistence::SaveCommitState::committed);

    const std::uint64_t runtime_before = arpg::test::allocation_count();
    runtime.service_pending_save();
    const std::uint64_t runtime_allocations =
        arpg::test::allocation_count() - runtime_before;
    ARPG_REQUIRE(runtime.state() == platform::DungeonRuntimeState::running);
    ARPG_REQUIRE(runtime.render_status().indicator == platform::SaveIndicator::saved);
    const std::uint64_t allowed_allocations = direct_allocations
        + kRuntimeVectorProxyAllocations;
    if (runtime_allocations > allowed_allocations) {
        std::fprintf(stderr,
            "runtime save allocations=%llu allowed=%llu direct=%llu\n",
            static_cast<unsigned long long>(runtime_allocations),
            static_cast<unsigned long long>(allowed_allocations),
            static_cast<unsigned long long>(direct_allocations));
    }
    ARPG_REQUIRE(runtime_allocations <= allowed_allocations);
    ARPG_REQUIRE(same_ownership(*runtime.item_state(), expected.item_ownership));
    return {};
}

arpg::test::Failure runtime_echoes_death_pending_kind() noexcept {
    TempDirectory directory;
    auto config = config_for(directory, 0xD34D10U);
    platform::DungeonRuntime runtime(config);
    ARPG_REQUIRE(runtime.initialize());
    auto* const session = runtime.session();
    ARPG_REQUIRE(session != nullptr);
    session->tick({});
    ARPG_REQUIRE(arpg::test::kill_current_player_through_combat(*session));
    session->tick({});
    ARPG_REQUIRE(session->pending_save_view() != nullptr);
    ARPG_REQUIRE(session->pending_save_view()->kind
        == dungeon::PendingSaveKind::death_retreat);
    runtime.service_pending_save();
    ARPG_REQUIRE(runtime.state() == platform::DungeonRuntimeState::running);
    ARPG_REQUIRE(session->snapshot().phase == dungeon::RoomPhase::death_pending);
    ARPG_REQUIRE(session->snapshot().death.has_value());
    ARPG_REQUIRE(session->snapshot().death->can_continue);
    return {};
}

arpg::test::Failure v5_load_commits_migration_before_session() noexcept {
    TempDirectory directory;
    auto legacy = dungeon::make_initial_run_state(
        0xD34D05U, dungeon::DungeonRules{}).state;
    const auto bytes = encode_v5(legacy);
    ARPG_REQUIRE(!bytes.empty());
    ARPG_REQUIRE(write_save(directory.path / "run_a.sav", bytes));

    platform::DungeonRuntime runtime(config_for(directory));
    ARPG_REQUIRE(runtime.initialize());
    persistence::SaveStore store(config_for(directory).save);
    const auto disk = store.load();
    ARPG_REQUIRE(disk.state == persistence::SaveLoadState::ready);
    ARPG_REQUIRE(!disk.migrated);
    ARPG_REQUIRE(disk.checkpoint.commit_generation
        == legacy.commit_generation + 1U);
    ARPG_REQUIRE(disk.checkpoint.death_sequence == 0U);
    ARPG_REQUIRE(disk.checkpoint.death.lifecycle
        == dungeon::checkpoint::DeathLifecycle::none);
    ARPG_REQUIRE(disk.checkpoint.root_seed == legacy.root_seed);
    ARPG_REQUIRE(disk.checkpoint.current_room.seed
        == legacy.current_room.seed);
    ARPG_REQUIRE(runtime.session()->snapshot().commit_generation
        == disk.checkpoint.commit_generation);
    return {};
}

bool prime_runtime_combat_death(platform::DungeonRuntime& runtime) noexcept {
    auto* const session = runtime.session();
    if (session == nullptr) return false;
    runtime.fixed_tick({});
    return arpg::test::kill_current_player_through_combat(*session);
}

arpg::test::Failure fixed_tick_commits_death_before_returning_snapshot() noexcept {
    TempDirectory directory;
    platform::DungeonRuntime runtime(config_for(directory, 0xD34D11U));
    ARPG_REQUIRE(runtime.initialize());
    ARPG_REQUIRE(prime_runtime_combat_death(runtime));
    const auto generation = runtime.session()->snapshot().commit_generation;

    runtime.fixed_tick({1, 1});

    const auto after = runtime.session()->snapshot();
    ARPG_REQUIRE(after.phase == dungeon::RoomPhase::death_pending);
    ARPG_REQUIRE(after.commit_generation == generation + 1U);
    ARPG_REQUIRE(after.death.has_value());
    ARPG_REQUIRE(after.death->can_continue);
    ARPG_REQUIRE(runtime.session()->pending_save_view() == nullptr);
    ARPG_REQUIRE(runtime.request_pickup(0U) == dungeon::RequestResult::rejected);
    ARPG_REQUIRE(runtime.request_equip(1U) == dungeon::RequestResult::rejected);
    ARPG_REQUIRE(runtime.request_unequip(items::ItemSlot::weapon)
        == dungeon::RequestResult::rejected);
    ARPG_REQUIRE(runtime.request_recipe({{1U, 2U, 3U}})
        == dungeon::RequestResult::rejected);
    ARPG_REQUIRE(!runtime.session()->queue_action(combat::Action::light));
    ARPG_REQUIRE(!runtime.session()->request_descent(true));
    return {};
}

arpg::test::Failure fixed_tick_not_committed_keeps_same_death_retryable() noexcept {
    TempDirectory directory;
    FaultContext fault{persistence::SaveFaultPoint::before_publish, false};
    auto config = config_for(directory, 0xD34D12U);
    config.save.fault_hook = &fail_when_enabled;
    config.save.fault_context = &fault;
    platform::DungeonRuntime runtime(config);
    ARPG_REQUIRE(runtime.initialize());
    ARPG_REQUIRE(prime_runtime_combat_death(runtime));
    const auto before = runtime.session()->snapshot();
    ARPG_REQUIRE(before.death.has_value());
    const auto target = before.death->checkpoint.target_room;
    fault.enabled = true;

    runtime.fixed_tick({1, 0});

    const auto after = runtime.session()->snapshot();
    ARPG_REQUIRE(runtime.state() == platform::DungeonRuntimeState::running);
    ARPG_REQUIRE(after.commit_generation == before.commit_generation);
    ARPG_REQUIRE(after.death.has_value());
    ARPG_REQUIRE(after.death->saving);
    ARPG_REQUIRE(!after.death->can_continue);
    ARPG_REQUIRE(same_room_descriptor(
        after.death->checkpoint.target_room, target));
    ARPG_REQUIRE(runtime.render_status().indicator == platform::SaveIndicator::error);
    fault.enabled = false;
    runtime.fixed_tick({});
    ARPG_REQUIRE(runtime.session()->snapshot().phase
        == dungeon::RoomPhase::death_pending);
    ARPG_REQUIRE(same_room_descriptor(
        runtime.session()->snapshot().death->checkpoint.target_room, target));
    return {};
}

arpg::test::Failure fixed_tick_indeterminate_faults_synchronously() noexcept {
    TempDirectory directory;
    FaultContext fault{persistence::SaveFaultPoint::after_publish, false};
    auto config = config_for(directory, 0xD34D13U);
    config.save.fault_hook = &fail_when_enabled;
    config.save.fault_context = &fault;
    platform::DungeonRuntime runtime(config);
    ARPG_REQUIRE(runtime.initialize());
    ARPG_REQUIRE(prime_runtime_combat_death(runtime));
    fault.enabled = true;

    runtime.fixed_tick({});

    ARPG_REQUIRE(runtime.state() == platform::DungeonRuntimeState::faulted);
    ARPG_REQUIRE(runtime.session()->snapshot().phase
        == dungeon::RoomPhase::faulted);
    ARPG_REQUIRE(runtime.render_status().indicator == platform::SaveIndicator::error);
    return {};
}

arpg::test::Failure runtime_continue_is_narrow_and_fixed_tick_commits_it() noexcept {
    TempDirectory directory;
    platform::DungeonRuntime runtime(config_for(directory, 0xD34D14U));
    ARPG_REQUIRE(runtime.initialize());
    ARPG_REQUIRE(runtime.request_death_continue()
        == dungeon::RequestResult::rejected);
    ARPG_REQUIRE(prime_runtime_combat_death(runtime));
    runtime.fixed_tick({});
    const auto death = runtime.session()->snapshot();
    ARPG_REQUIRE(death.phase == dungeon::RoomPhase::death_pending);
    const auto target_seed = death.death->checkpoint.target_room.seed;

    ARPG_REQUIRE(runtime.request_death_continue()
        == dungeon::RequestResult::accepted);
    ARPG_REQUIRE(runtime.session()->pending_save_view() != nullptr);
    ARPG_REQUIRE(runtime.session()->pending_save_view()->kind
        == dungeon::PendingSaveKind::death_continue);
    const auto saving = runtime.session()->snapshot();
    ARPG_REQUIRE(saving.death.has_value());
    ARPG_REQUIRE(saving.death->saving);
    ARPG_REQUIRE(!saving.death->can_continue);
    runtime.fixed_tick({1, 1});
    ARPG_REQUIRE(runtime.session()->snapshot().phase
        == dungeon::RoomPhase::transitioning);
    runtime.fixed_tick({});
    const auto continued = runtime.session()->snapshot();
    ARPG_REQUIRE(continued.room_seed == target_seed);
    ARPG_REQUIRE(!continued.death.has_value());
    return {};
}

arpg::test::Failure v6_pending_death_load_preserves_generation_and_target() noexcept {
    for (const bool abyss_death : {false, true}) {
        TempDirectory directory;
        auto config = config_for(directory, abyss_death ? 0xD34D16U : 0xD34D15U);
        auto initial = abyss_death
            ? runtime_available_state()
            : dungeon::make_initial_run_state(*config.new_run_seed, config.rules).state;
        persistence::SaveStore store(config.save);
        ARPG_REQUIRE(store.commit(initial).state
            == persistence::SaveCommitState::committed);
        dungeon::DungeonSession source{config.rules, initial};
        if (abyss_death) {
            const dungeon::PendingSave* const start = source.pending_save_view();
            ARPG_REQUIRE(start != nullptr);
            ARPG_REQUIRE(start->kind == dungeon::PendingSaveKind::abyss_start);
            const auto saved = store.commit(start->next_state);
            ARPG_REQUIRE(saved.state == persistence::SaveCommitState::committed);
            source.resolve_pending_save({dungeon::SaveDisposition::committed,
                saved.verified_state.commit_generation, saved.verified_state,
                std::nullopt});
        }
        source.tick({});
        ARPG_REQUIRE(arpg::test::kill_current_player_through_combat(source));
        source.tick({});
        const dungeon::PendingSave* const pending = source.pending_save_view();
        ARPG_REQUIRE(pending != nullptr);
        ARPG_REQUIRE(pending->kind == dungeon::PendingSaveKind::death_retreat);
        const auto expected = pending->next_state;
        ARPG_REQUIRE(store.commit(expected).state
            == persistence::SaveCommitState::committed);

        platform::DungeonRuntime runtime(config);
        ARPG_REQUIRE(runtime.initialize());
        const auto disk = store.load();
        ARPG_REQUIRE(disk.state == persistence::SaveLoadState::ready);
        ARPG_REQUIRE(dungeon::same_run_state(disk.checkpoint, expected));
        const auto loaded = runtime.session()->snapshot();
        ARPG_REQUIRE(loaded.phase == dungeon::RoomPhase::death_pending);
        ARPG_REQUIRE(loaded.commit_generation == expected.commit_generation);
        ARPG_REQUIRE(loaded.death.has_value());
        ARPG_REQUIRE(loaded.death->checkpoint.target_room.seed
            == expected.death.target_room.seed);
        ARPG_REQUIRE(loaded.death->checkpoint.target_room.depth
            == expected.death.target_room.depth);
        ARPG_REQUIRE(loaded.death->checkpoint.death_was_abyss == abyss_death);
        ARPG_REQUIRE(runtime.session()->pending_save_view() == nullptr);
        if (abyss_death) {
            ARPG_REQUIRE(expected.abyss.lifecycle
                == arpg::abyss::AbyssLifecycle::failed);
            ARPG_REQUIRE(loaded.is_abyss == false);
        }
    }
    return {};
}

arpg::test::Failure item_request_fault_matrix_is_atomic_and_restart_consistent() noexcept {
    constexpr std::array<ItemRequestKind, 4> kKinds{{
        ItemRequestKind::pickup,
        ItemRequestKind::equip,
        ItemRequestKind::unequip,
        ItemRequestKind::recipe,
    }};
    constexpr std::array<persistence::SaveCommitState, 3> kDispositions{{
        persistence::SaveCommitState::committed,
        persistence::SaveCommitState::not_committed,
        persistence::SaveCommitState::indeterminate,
    }};
    for (const ItemRequestKind kind : kKinds) {
        for (const persistence::SaveCommitState disposition : kDispositions) {
            TempDirectory directory;
            FaultContext fault{
                disposition == persistence::SaveCommitState::not_committed
                    ? persistence::SaveFaultPoint::before_publish
                    : persistence::SaveFaultPoint::after_publish,
                false,
            };
            auto config = config_for(directory);
            config.save.fault_hook = &fail_when_enabled;
            config.save.fault_context = &fault;
            persistence::SaveStore seed_store(config.save);
            const auto initial = item_state_for(kind);
            ARPG_REQUIRE(seed_store.commit(initial).state
                == persistence::SaveCommitState::committed);

            auto runtime = std::make_unique<platform::DungeonRuntime>(config);
            ARPG_REQUIRE(runtime->initialize());
            ARPG_REQUIRE(install_pickup_if_needed(*runtime, kind));
            const auto before_snapshot = runtime->session()->snapshot();
            const items::ItemOwnershipState before_items = *runtime->item_state();
            const auto before_build = arpg::test::player_build(*runtime->session());
            ARPG_REQUIRE(issue_item_request(*runtime, kind)
                == dungeon::RequestResult::accepted);
            const dungeon::PendingSave* const pending =
                runtime->session()->pending_save_view();
            ARPG_REQUIRE(pending != nullptr);
            const auto expected_generation = pending->expected_generation;
            const items::ItemOwnershipState expected_items =
                pending->next_state.item_ownership;
            auto expected_session = std::make_unique<dungeon::DungeonSession>(
                dungeon::DungeonRules{}, pending->next_state);
            const auto expected_build = arpg::test::player_build(
                *expected_session);

            ARPG_REQUIRE(runtime->request_pickup(0U)
                == dungeon::RequestResult::rejected);
            ARPG_REQUIRE(runtime->request_equip(101U)
                == dungeon::RequestResult::rejected);
            ARPG_REQUIRE(runtime->request_unequip(items::ItemSlot::weapon)
                == dungeon::RequestResult::rejected);
            ARPG_REQUIRE(runtime->request_recipe({{301U, 302U, 303U}})
                == dungeon::RequestResult::rejected);
            ARPG_REQUIRE(!runtime->session()->request_descent(true));
            arpg::test::attempt_exit(
                *runtime->session(), dungeon::ExitDirection::right);
            static_cast<void>(runtime->session()->reset_current_room());
            ARPG_REQUIRE(!runtime->session()->queue_action(combat::Action::light));
            ARPG_REQUIRE(runtime->session()->snapshot().phase
                == dungeon::RoomPhase::committing);
            ARPG_REQUIRE(runtime->session()->snapshot().commit_generation
                == before_snapshot.commit_generation);
            ARPG_REQUIRE(runtime->session()->snapshot().ground_item_count
                == before_snapshot.ground_item_count);
            ARPG_REQUIRE(same_ownership(*runtime->item_state(), before_items));

            fault.enabled = disposition != persistence::SaveCommitState::committed;
            runtime->service_pending_save();
            const auto after_snapshot = runtime->session()->snapshot();
            const bool committed =
                disposition == persistence::SaveCommitState::committed;
            const bool indeterminate =
                disposition == persistence::SaveCommitState::indeterminate;
            ARPG_REQUIRE(runtime->render_status().indicator
                == (committed ? platform::SaveIndicator::saved
                              : platform::SaveIndicator::error));
            ARPG_REQUIRE(runtime->state()
                == (indeterminate ? platform::DungeonRuntimeState::faulted
                                  : platform::DungeonRuntimeState::running));
            ARPG_REQUIRE(after_snapshot.phase
                == (indeterminate ? dungeon::RoomPhase::faulted
                                  : dungeon::RoomPhase::locked));
            ARPG_REQUIRE(after_snapshot.commit_generation
                == (committed ? expected_generation
                              : before_snapshot.commit_generation));
            ARPG_REQUIRE(same_ownership(*runtime->item_state(),
                committed ? expected_items : before_items));
            ARPG_REQUIRE(after_snapshot.ground_item_count
                == (committed && kind == ItemRequestKind::pickup
                    ? 0U : before_snapshot.ground_item_count));
            ARPG_REQUIRE(same_build(
                arpg::test::player_build(*runtime->session()),
                committed ? expected_build : before_build));
            if (indeterminate) {
                ARPG_REQUIRE(issue_item_request(*runtime, kind)
                    == dungeon::RequestResult::rejected);
            }

            auto restart_config = config_for(directory, 999U);
            persistence::SaveStore disk_store(restart_config.save);
            const auto disk = disk_store.load();
            ARPG_REQUIRE(disk.state == persistence::SaveLoadState::ready);
            auto restarted = std::make_unique<platform::DungeonRuntime>(
                restart_config);
            ARPG_REQUIRE(restarted->initialize());
            ARPG_REQUIRE(restarted->session()->snapshot().commit_generation
                == disk.checkpoint.commit_generation);
            ARPG_REQUIRE(same_ownership(
                *restarted->item_state(), disk.checkpoint.item_ownership));
            ARPG_REQUIRE(same_build(arpg::test::player_build(*restarted->session()),
                arpg::test::player_build(dungeon::DungeonSession{
                    {}, disk.checkpoint})));
            if (committed) {
                ARPG_REQUIRE(same_ownership(*restarted->item_state(), expected_items));
            } else if (!indeterminate) {
                ARPG_REQUIRE(same_ownership(*restarted->item_state(), before_items));
            } else {
                ARPG_REQUIRE(same_ownership(*restarted->item_state(), before_items)
                    || same_ownership(*restarted->item_state(), expected_items));
            }
        }
    }
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"load available keeps start as second transaction", &load_available_keeps_start_as_second_transaction},
    {"service available start creates started combat", &servicing_available_start_creates_started_combat},
    {"load started commits failed before session", &load_started_commits_failed_before_session},
    {"load started publish failure faults runtime", &load_started_failure_to_publish_faults_runtime},
    {"migrated initial marker commits v5 before session", &migrated_initial_abyss_marker_commits_v5_before_session},
    {"migrated checkpoint publish failure faults runtime", &migrated_checkpoint_publish_failure_faults_runtime},
    {"v5 load commits migration before session", &v5_load_commits_migration_before_session},
    {"migrated door available commits before start pending", &migrated_door_available_commits_before_start_pending},
    {"empty directory commits seeded generation one before running", &empty_directory_commits_seeded_generation_one_before_running},
    {"valid save ignores new run seed override", &valid_save_ignores_new_run_seed_override},
    {"committed pending transition maps verified state and saved indicator", &committed_pending_transition_maps_verified_state_and_saved_indicator},
    {"committed passive save survives runtime restart", &committed_passive_save_survives_runtime_restart},
    {"committed route and refund survive runtime restart", &committed_route_and_refund_survive_runtime_restart},
    {"passive pre publish failure keeps old tree and retryable runtime", &passive_pre_publish_failure_keeps_old_tree_and_retryable_runtime},
    {"indeterminate passive save faults runtime", &indeterminate_passive_save_faults_runtime},
    {"passive pending rejects door and descent requests", &passive_pending_rejects_door_and_descent_requests},
    {"pre publish not committed maps to retryable error", &pre_publish_not_committed_maps_to_retryable_error},
    {"indeterminate maps to faulted runtime and blocks selection", &indeterminate_maps_to_faulted_runtime_and_blocks_selection},
    {"dual slot corruption requires recovery then archives new run", &dual_slot_corruption_requires_recovery_then_archives_new_run},
    {"single slot corruption recovers and subsequent saves alternate", &single_slot_corruption_recovers_and_subsequent_saves_alternate},
    {"invalid rules fault without creating save or session", &invalid_rules_fault_without_creating_save_or_session},
    {"runtime exposes narrow item requests and stable item view", &runtime_exposes_narrow_item_requests_and_stable_item_view},
    {"loot filter modes map to pickup policy",
        &loot_filter_modes_map_to_pickup_policy},
    {"fixed tick forwards pickup policy and defaults show all",
        &fixed_tick_forwards_pickup_policy_and_defaults_show_all},
    {"synchronous pickup publishes exact committed receipt",
        &synchronous_pickup_publishes_exact_committed_receipt},
    {"abyss claim publishes committed abyss receipt",
        &abyss_claim_publishes_committed_abyss_receipt},
    {"failed and nonpickup preserve pickup receipt",
        &failed_and_nonpickup_saves_do_not_replace_receipt},
    {"wrong pending ordinal does not publish receipt",
        &wrong_pending_ordinal_fault_does_not_publish_receipt},
    {"replaced pickup ordinal does not publish receipt",
        &replaced_pickup_ordinal_does_not_publish_receipt},
    {"failed pickup retry publishes one HUD notice",
        &failed_pickup_retry_publishes_one_presented_hud_notice},
    {"large inventory snapshot and views do not allocate", &large_inventory_snapshot_and_views_do_not_allocate},
    {"generic service adds no large state copies", &generic_service_adds_no_large_state_copies},
    {"runtime echoes death pending kind", &runtime_echoes_death_pending_kind},
    {"fixed tick commits death before returning snapshot", &fixed_tick_commits_death_before_returning_snapshot},
    {"fixed tick not committed keeps same death retryable", &fixed_tick_not_committed_keeps_same_death_retryable},
    {"fixed tick indeterminate faults synchronously", &fixed_tick_indeterminate_faults_synchronously},
    {"runtime continue is narrow and fixed tick commits it", &runtime_continue_is_narrow_and_fixed_tick_commits_it},
    {"v6 pending death load preserves generation and target", &v6_pending_death_load_preserves_generation_and_target},
    {"item request fault matrix is atomic and restart consistent", &item_request_fault_matrix_is_atomic_and_restart_consistent},
};

}  // namespace

arpg::test::TestSuite dungeon_runtime_suite() noexcept {
    return arpg::test::make_suite("dungeon_runtime", kCases);
}
