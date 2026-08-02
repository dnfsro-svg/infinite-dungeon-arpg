#include "test_framework.hpp"
#include "allocation_probe.hpp"

#include "../dungeon/dungeon_test_support.hpp"
#include "dungeon_runtime.hpp"
#include "combat_renderer.hpp"
#include "raylib_host.hpp"
#include "abyss/abyss_rules.hpp"
#include "abyss/abyss_rewards.hpp"
#include "checkpoint/room_checkpoint_validation.hpp"
#include "dungeon/abyss_reward.hpp"
#include "persistence/checkpoint_codec.hpp"
#include "persistence/room_progress_codec.hpp"
#include "platform/settings/settings_types.hpp"

#include <array>
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace arpg::test {

struct DungeonRuntimeTestAccess final {
    static persistence::SaveCommitCompletion matching_exact(
        const platform::DungeonRuntime& runtime) noexcept {
        persistence::SaveCommitCompletion completion{};
        completion.job_slot = 0U;
        completion.revision = runtime.exact_flight_.revision;
        completion.kind = persistence::SaveCommitRequestKind::exact;
        completion.intent = runtime.exact_flight_.intent;
        completion.token = runtime.exact_flight_.token;
        completion.epoch = runtime.exact_flight_.epoch;
        completion.result.state = persistence::SaveCommitState::committed;
        completion.result.active_slot = persistence::SaveSlot::a;
        return completion;
    }

    static void inject(platform::DungeonRuntime& runtime,
        const persistence::SaveCommitCompletion& completion) noexcept {
        runtime.apply_save_completion(completion);
    }

    static void force_background_due(
        platform::DungeonRuntime& runtime) noexcept {
        runtime.progress_dirty_ = true;
        runtime.background_due_ = true;
    }

    static std::uint64_t authority_revision(
        const platform::DungeonRuntime& runtime) noexcept {
        return runtime.authority_revision_;
    }

    static std::uint64_t durable_revision(
        const platform::DungeonRuntime& runtime) noexcept {
        return runtime.durable_revision_;
    }

    static bool exact_active(
        const platform::DungeonRuntime& runtime) noexcept {
        return runtime.exact_flight_.active;
    }

    static bool background_active(
        const platform::DungeonRuntime& runtime) noexcept {
        return runtime.background_flight_.active;
    }
};

}  // namespace arpg::test

namespace {

namespace dungeon = arpg::dungeon;
namespace persistence = arpg::persistence;
namespace platform = arpg::platform;
namespace combat = arpg::combat;
namespace items = arpg::items;
namespace checkpoint = arpg::checkpoint;

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

struct InstalledPickupGround final {
    bool valid{};
    std::uint16_t ordinal{0xFFFFU};
};

InstalledPickupGround install_pickup_ground_at_active_spawn(
    platform::DungeonRuntime& runtime,
    const dungeon::DungeonSnapshot& snapshot,
    const items::ItemInstance& item,
    const std::uint16_t excluded_ordinal = 0xFFFFU) noexcept {
    dungeon::DungeonSession* const session = runtime.session();
    if (session == nullptr || !snapshot.combat.has_value()) return {};
    for (std::uint16_t index = 0U;
            index < snapshot.combat->monster_count; ++index) {
        const combat::MonsterSnapshot& monster =
            snapshot.combat->monsters[index];
        if (!monster.active || monster.spawn_ordinal == excluded_ordinal) {
            continue;
        }
        const std::uint16_t ordinal = monster.spawn_ordinal;
        if (!arpg::test::install_authoritative_ground_item(
                *session, ordinal, item, monster.position)) {
            continue;
        }
        const auto& ground = arpg::test::ground_items(*session)[ordinal];
        if (!ground.active || ground.drop_ordinal != ordinal
                || ground.item.id != item.id) {
            return {};
        }
        arpg::test::set_player_position(*session, monster.position);
        return {true, ordinal};
    }
    return {};
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
    std::uint16_t ordinal{0xFFFFU};
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

struct ScheduleBlockingHook final {
    std::atomic<bool> armed{};
    std::atomic<bool> entered{};
    std::atomic<bool> release{};
};

bool block_scheduled_write(persistence::SaveFaultPoint point,
    void* context) noexcept {
    auto& hook = *static_cast<ScheduleBlockingHook*>(context);
    if (point != persistence::SaveFaultPoint::before_temp_write
            || !hook.armed.load() || hook.entered.exchange(true)) {
        return false;
    }
    while (!hook.release.load()) std::this_thread::yield();
    return false;
}

bool wait_for_schedule_block(ScheduleBlockingHook& hook) noexcept {
    const auto deadline = std::chrono::steady_clock::now()
        + std::chrono::seconds{10};
    while (!hook.entered.load()
            && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::yield();
    }
    return hook.entered.load();
}

std::vector<std::uint8_t> read_active_bytes(
    const TempDirectory& directory, persistence::SaveSlot slot) {
    const char* const name = slot == persistence::SaveSlot::a
        ? "run_a.sav" : slot == persistence::SaveSlot::b
            ? "run_b.sav" : nullptr;
    if (name == nullptr) return {};
    std::ifstream input(directory.path / name,
        std::ios::binary | std::ios::ate);
    if (!input) return {};
    const std::streamoff end = input.tellg();
    if (end <= 0 || static_cast<std::uint64_t>(end)
            > persistence::kMaximumEncodedCheckpointBytes) {
        return {};
    }
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(end));
    input.seekg(0, std::ios::beg);
    input.read(reinterpret_cast<char*>(bytes.data()), end);
    return input.good() ? bytes : std::vector<std::uint8_t>{};
}

void settle_runtime_save(platform::DungeonRuntime& runtime) noexcept {
    const auto deadline = std::chrono::steady_clock::now()
        + std::chrono::seconds{10};
    do {
        runtime.service_pending_save();
        if (runtime.state() != platform::DungeonRuntimeState::running) return;
        if (runtime.session() != nullptr
                && runtime.session()->pending_save_view() == nullptr
                && runtime.render_status().indicator
                    != platform::SaveIndicator::saving) {
            return;
        }
        std::this_thread::yield();
    } while (std::chrono::steady_clock::now() < deadline);
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

bool decode_active_v9(const TempDirectory& directory,
    persistence::SaveSlot slot,
    checkpoint::SaveCheckpointSlot& out) noexcept {
    const char* const name = slot == persistence::SaveSlot::a
        ? "run_a.sav" : slot == persistence::SaveSlot::b
            ? "run_b.sav" : nullptr;
    if (name == nullptr) return false;
    std::ifstream input(directory.path / name,
        std::ios::binary | std::ios::ate);
    if (!input) return false;
    const std::streamoff end = input.tellg();
    if (end <= 0 || static_cast<std::uint64_t>(end)
            > persistence::kMaximumEncodedCheckpointBytes) {
        return false;
    }
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(end));
    input.seekg(0, std::ios::beg);
    input.read(reinterpret_cast<char*>(bytes.data()), end);
    bool migrated{};
    return input.good()
        && persistence::decode_checkpoint_v9_into(
            bytes.data(), bytes.size(), out, migrated)
                == persistence::CodecError::none
        && !migrated;
}

bool same_room_descriptor(
    const checkpoint::RoomDescriptor& lhs,
    const checkpoint::RoomDescriptor& rhs) noexcept {
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
    {
        platform::DungeonRuntime runtime(config);
        ARPG_REQUIRE(runtime.initialize());
        settle_runtime_save(runtime);
        ARPG_REQUIRE(runtime.state() == platform::DungeonRuntimeState::running);
        ARPG_REQUIRE(runtime.session()->snapshot().phase
            == dungeon::RoomPhase::locked);
        ARPG_REQUIRE(runtime.session()->snapshot().is_abyss);
        ARPG_REQUIRE(runtime.session()->snapshot().combat.has_value());
    }
    ARPG_REQUIRE(store.load().checkpoint.abyss.lifecycle
        == arpg::abyss::AbyssLifecycle::started);
    platform::DungeonRuntime restarted(config_for(directory, 0xBADU));
    ARPG_REQUIRE(restarted.initialize());
    ARPG_REQUIRE(restarted.state() == platform::DungeonRuntimeState::running);
    ARPG_REQUIRE(restarted.session()->snapshot().phase
        == dungeon::RoomPhase::locked);
    ARPG_REQUIRE(restarted.session()->snapshot().is_abyss);
    ARPG_REQUIRE(restarted.session()->snapshot().combat.has_value());
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

bool clear_and_await(platform::DungeonRuntime& runtime) noexcept {
    dungeon::DungeonSession& session = *runtime.session();
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
        if (snapshot.phase == dungeon::RoomPhase::committing) {
            settle_runtime_save(runtime);
            drain(session);
            continue;
        }
        if (snapshot.phase == dungeon::RoomPhase::combat) {
            arpg::test::force_complete_current_room_without_visual_drops(
                session);
        }
        session.tick({});
        drain(session);
    }
    return false;
}

bool drive_door_pending(platform::DungeonRuntime& runtime) noexcept {
    dungeon::DungeonSession& session = *runtime.session();
    if (!clear_and_await(runtime)) {
        return false;
    }
    combat::MovementInput movement{1, 0};
    for (int tick = 0; tick < 4096; ++tick) {
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
    const bool initialized = runtime.initialize();
    ARPG_REQUIRE(initialized);
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
    ARPG_REQUIRE(drive_door_pending(runtime));
    const auto expected = runtime.session()->pending_transition();
    ARPG_REQUIRE(expected.has_value());
    settle_runtime_save(runtime);
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
    ARPG_REQUIRE(clear_and_await(runtime));
    const auto before = runtime.session()->snapshot();
    ARPG_REQUIRE(before.combat.has_value());
    ARPG_REQUIRE(runtime.session()->request_passive_allocation(1U));
    settle_runtime_save(runtime);
    const auto saved = runtime.session()->snapshot();
    ARPG_REQUIRE(saved.passive_tree.allocated_bits
        == ((1ULL << 0U) | (1ULL << 1U)));
    ARPG_REQUIRE(saved.combat.has_value());
    ARPG_REQUIRE(saved.combat->player.max_hp
        == before.combat->player.max_hp + 20);
    ARPG_REQUIRE(!saved.passive_save_pending);
    const auto generation = saved.commit_generation;
    settle_runtime_save(runtime);
    ARPG_REQUIRE(runtime.session()->snapshot().commit_generation == generation);
    platform::DungeonRuntime resumed(config_for(directory, 999U));
    ARPG_REQUIRE(resumed.initialize());
    ARPG_REQUIRE(resumed.session()->snapshot().passive_tree.allocated_bits
        == saved.passive_tree.allocated_bits);
    ARPG_REQUIRE(resumed.session()->snapshot().combat.has_value());
    ARPG_REQUIRE(resumed.session()->snapshot().combat->player.max_hp
        == saved.combat->player.max_hp);
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
    ARPG_REQUIRE(clear_and_await(runtime));
    const auto before = runtime.session()->snapshot();
    for (const std::uint8_t node : {std::uint8_t{8U}, std::uint8_t{9U},
            std::uint8_t{10U}}) {
        ARPG_REQUIRE(runtime.session()->request_passive_allocation(node));
        settle_runtime_save(runtime);
        ARPG_REQUIRE(!runtime.session()->snapshot().passive_save_pending);
        ARPG_REQUIRE(runtime.render_status().indicator == platform::SaveIndicator::saved);
    }
    ARPG_REQUIRE(runtime.session()->request_passive_refund(10U));
    settle_runtime_save(runtime);
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
    ARPG_REQUIRE(clear_and_await(runtime));
    const auto before = runtime.session()->snapshot();
    ARPG_REQUIRE(runtime.session()->request_passive_allocation(8U));
    fault.enabled = true;
    settle_runtime_save(runtime);
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
    ARPG_REQUIRE(clear_and_await(runtime));
    ARPG_REQUIRE(runtime.session()->request_passive_allocation(8U));
    fault.enabled = true;
    settle_runtime_save(runtime);
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
    ARPG_REQUIRE(clear_and_await(runtime));
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
    ARPG_REQUIRE(drive_door_pending(runtime));
    fault.enabled = true;
    settle_runtime_save(runtime);
    ARPG_REQUIRE(runtime.session()->snapshot().phase
        == dungeon::RoomPhase::awaiting_exit);
    ARPG_REQUIRE(runtime.render_status().indicator == platform::SaveIndicator::error);
    fault.enabled = false;
    ARPG_REQUIRE(drive_door_pending(runtime));
    settle_runtime_save(runtime);
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
    ARPG_REQUIRE(drive_door_pending(runtime));
    fault.enabled = true;
    settle_runtime_save(runtime);
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
    ARPG_REQUIRE(runtime.state()
        == platform::DungeonRuntimeState::recovery_required);
    ARPG_REQUIRE(runtime.gameplay_rearm_required());
    ARPG_REQUIRE(!runtime.authority_requests_enabled());
    const auto deadline = std::chrono::steady_clock::now()
        + std::chrono::seconds{10};
    while (runtime.state()
            == platform::DungeonRuntimeState::recovery_required
            && std::chrono::steady_clock::now() < deadline) {
        runtime.pump_persistence_frame();
        std::this_thread::yield();
    }
    ARPG_REQUIRE(runtime.state() == platform::DungeonRuntimeState::running);
    ARPG_REQUIRE(runtime.session() != nullptr);
    ARPG_REQUIRE(runtime.session()->snapshot().commit_generation == 1U);
    ARPG_REQUIRE(!runtime.authority_requests_enabled());
    runtime.acknowledge_gameplay_rearmed();
    ARPG_REQUIRE(runtime.authority_requests_enabled());
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
    ARPG_REQUIRE(drive_door_pending(runtime));
    const auto first_slot = runtime.render_status().active_slot;
    settle_runtime_save(runtime);
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
    if (!arpg::test::install_authoritative_ground_item(*runtime.session(), 0U,
            normal_item(401U, 2U), snapshot.combat->player.position)) {
        return false;
    }
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
        context->invoked =
            arpg::test::replace_indexed_ground_item_for_fault(
                *context->session, context->ordinal,
                context->replacement);
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
    const InstalledPickupGround filtered_ground =
        install_pickup_ground_at_active_spawn(
            filtered, filtered_before, normal_item(0xF117E201U));
    ARPG_REQUIRE(filtered_ground.valid);

    filtered.fixed_tick({}, {items::ItemRarity::rare});

    ARPG_REQUIRE(filtered.session()->snapshot().ground_item_count == 1U);
    ARPG_REQUIRE(filtered.session()->pending_save_view() == nullptr);

    TempDirectory default_directory;
    platform::DungeonRuntime default_runtime(
        config_for(default_directory, 0xDEF4017U));
    ARPG_REQUIRE(default_runtime.initialize());
    const auto default_before = default_runtime.session()->snapshot();
    ARPG_REQUIRE(default_before.combat.has_value());
    const InstalledPickupGround default_ground =
        install_pickup_ground_at_active_spawn(
            default_runtime, default_before, normal_item(0xDEF401701U));
    ARPG_REQUIRE(default_ground.valid);

    default_runtime.fixed_tick({});
    settle_runtime_save(default_runtime);

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
    const InstalledPickupGround ground =
        install_pickup_ground_at_active_spawn(runtime, before, item);
    ARPG_REQUIRE(ground.valid);

    runtime.fixed_tick({});
    settle_runtime_save(runtime);

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

    settle_runtime_save(runtime);

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
    const InstalledPickupGround first_ground =
        install_pickup_ground_at_active_spawn(runtime, start, first);
    ARPG_REQUIRE(first_ground.valid);
    runtime.fixed_tick({});
    settle_runtime_save(runtime);
    const auto confirmed = runtime.render_status().loot_pickup;
    ARPG_REQUIRE(confirmed.valid);
    runtime.acknowledge_gameplay_rearmed();

    ARPG_REQUIRE(runtime.request_equip(first.id)
        == dungeon::RequestResult::accepted);
    settle_runtime_save(runtime);
    ARPG_REQUIRE(runtime.render_status().loot_pickup.commit_generation
        == confirmed.commit_generation);
    ARPG_REQUIRE(runtime.render_status().loot_pickup.item_id
        == confirmed.item_id);
    runtime.acknowledge_gameplay_rearmed();

    const auto positioned = runtime.session()->snapshot();
    ARPG_REQUIRE(positioned.combat.has_value());
    const items::ItemInstance second = normal_item(0xFA17E1102U, 1U);
    const InstalledPickupGround second_ground =
        install_pickup_ground_at_active_spawn(
            runtime, positioned, second, first_ground.ordinal);
    ARPG_REQUIRE(second_ground.valid);
    ARPG_REQUIRE(runtime.request_pickup(second_ground.ordinal)
        == dungeon::RequestResult::accepted);
    fault.enabled = true;
    settle_runtime_save(runtime);
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
    const InstalledPickupGround first_ground =
        install_pickup_ground_at_active_spawn(
            runtime, before, normal_item(0xBAD0D100U));
    ARPG_REQUIRE(first_ground.valid);
    runtime.fixed_tick({});
    settle_runtime_save(runtime);
    const auto confirmed = runtime.render_status().loot_pickup;
    ARPG_REQUIRE(confirmed.valid);
    runtime.acknowledge_gameplay_rearmed();

    before = runtime.session()->snapshot();
    ARPG_REQUIRE(before.combat.has_value());
    const InstalledPickupGround second_ground =
        install_pickup_ground_at_active_spawn(
            runtime, before, normal_item(0xBAD0D101U),
            first_ground.ordinal);
    ARPG_REQUIRE(second_ground.valid);
    ARPG_REQUIRE(runtime.request_pickup(second_ground.ordinal)
        == dungeon::RequestResult::accepted);
    arpg::test::DungeonSessionTestAccess::set_pending_pickup_ordinal(
        *runtime.session(), 0xFFFFU);

    settle_runtime_save(runtime);

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
    const InstalledPickupGround first_ground =
        install_pickup_ground_at_active_spawn(
            runtime, before, normal_item(0xA17E2200U));
    ARPG_REQUIRE(first_ground.valid);
    runtime.fixed_tick({});
    settle_runtime_save(runtime);
    const auto confirmed = runtime.render_status().loot_pickup;
    ARPG_REQUIRE(confirmed.valid);
    runtime.acknowledge_gameplay_rearmed();

    before = runtime.session()->snapshot();
    ARPG_REQUIRE(before.combat.has_value());
    const InstalledPickupGround second_ground =
        install_pickup_ground_at_active_spawn(
            runtime, before, normal_item(0xA17E2201U),
            first_ground.ordinal);
    ARPG_REQUIRE(second_ground.valid);
    replacement.ordinal = second_ground.ordinal;
    ARPG_REQUIRE(runtime.request_pickup(second_ground.ordinal)
        == dungeon::RequestResult::accepted);
    replacement.armed = true;

    settle_runtime_save(runtime);

    ARPG_REQUIRE(replacement.invoked);
    ARPG_REQUIRE(runtime.state() == platform::DungeonRuntimeState::faulted);
    ARPG_REQUIRE(runtime.render_status().faulted);
    ARPG_REQUIRE(same_receipt(runtime.render_status().loot_pickup, confirmed));
    const auto after = runtime.session()->snapshot();
    ARPG_REQUIRE(after.ground_item_count == 1U);
    ARPG_REQUIRE(after.ground_items[0].ordinal == replacement.ordinal);
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
    const InstalledPickupGround ground =
        install_pickup_ground_at_active_spawn(
            runtime, previous, normal_item(0xFEED77101U));
    ARPG_REQUIRE(ground.valid);

    fault.enabled = true;
    runtime.fixed_tick({});
    settle_runtime_save(runtime);
    auto failed = runtime.session()->snapshot();
    ARPG_REQUIRE(runtime.render_status().indicator == platform::SaveIndicator::error);
    ARPG_REQUIRE(!runtime.render_status().loot_pickup.valid);
    renderer.observe_presented_hud_frame(platform::HudPresentedFrame::normal,
        previous, failed, runtime.render_status(), hints, 0.0F, false);

    fault.enabled = false;
    runtime.acknowledge_gameplay_rearmed();
    runtime.fixed_tick({});
    settle_runtime_save(runtime);
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
    settle_runtime_save(runtime);
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

    struct ScheduleSpec final {
        std::uint32_t presentation_frames{};
        bool stale_before_capture{};
        bool stale_after_capture{};
        bool active_background{};
        bool matching_background_before_capture{};
    };
    struct ScheduleOutcome final {
        std::vector<std::uint8_t> saved_bytes{};
        std::vector<std::uint8_t> next_tick_bytes{};
        std::vector<std::uint64_t> event_trace{};
        arpg::core::DeterministicRng::State next_tick_rng{};
        std::array<std::uint64_t, 6U> equipped_ids{};
        std::uint64_t transaction_generation{};
        std::uint64_t next_authority_revision{};
        std::uint64_t next_combat_tick{};
    };
    const auto drain_trace = [](dungeon::DungeonSession& session) {
        std::vector<std::uint64_t> trace{};
        while (const auto event = session.try_pop_event()) {
            trace.push_back(
                static_cast<std::uint64_t>(event->kind)
                | (event->session_tick << 8U));
        }
        while (const auto event = session.try_pop_combat_event()) {
            trace.push_back(
                (std::uint64_t{1U} << 63U)
                | static_cast<std::uint64_t>(event->kind)
                | (event->tick << 8U));
        }
        return trace;
    };
    const auto capture_bytes = [](platform::DungeonRuntime& candidate,
                                  ScheduleOutcome& outcome) {
        auto slot = std::make_unique<
            checkpoint::SaveCheckpointSlot>();
        slot->state.item_ownership.items.reserve(
            persistence::kMaximumCheckpointItemCount);
        std::vector<std::uint8_t> bytes(
            persistence::kMaximumEncodedCheckpointBytes);
        if (!candidate.session()->capture_save_checkpoint(*slot,
                arpg::test::DungeonRuntimeTestAccess::authority_revision(
                    candidate))) {
            return std::vector<std::uint8_t>{};
        }
        outcome.next_tick_rng = slot->room_progress.combat.evasion_rng_state;
        outcome.equipped_ids = slot->state.item_ownership.equipment.equipped_ids;
        outcome.transaction_generation = slot->state.commit_generation;
        outcome.next_authority_revision = slot->persistence_revision;
        outcome.next_combat_tick = slot->room_progress.combat.tick;
        std::size_t written{};
        if (persistence::encode_checkpoint_v9_into(*slot, bytes.data(),
                bytes.size(), written) != persistence::CodecError::none) {
            return std::vector<std::uint8_t>{};
        }
        bytes.resize(written);
        return bytes;
    };
    const auto run_schedule = [&](const ScheduleSpec spec,
                                  ScheduleOutcome& outcome) {
        const auto fail = [&](const int stage) {
            std::fprintf(stderr,
                "schedule failure stage=%d frames=%u before=%d stale=%d bg=%d\n",
                stage, spec.presentation_frames,
                spec.stale_before_capture ? 1 : 0,
                spec.stale_after_capture ? 1 : 0,
                spec.active_background ? 1 : 0);
            return false;
        };
        TempDirectory directory;
        auto state = dungeon::make_initial_run_state(0x515151U, {}).state;
        state.item_ownership.items.push_back(normal_item(1U));
        state.item_ownership.next_item_sequence = 2U;
        auto config = config_for(directory, 0x515151U);
        ScheduleBlockingHook hook{};
        config.save.fault_hook = &block_scheduled_write;
        config.save.fault_context = &hook;
        persistence::SaveStore seed{config.save};
        if (seed.commit(state).state
                != persistence::SaveCommitState::committed) return fail(1);
        platform::DungeonRuntime candidate{config};
        if (!candidate.initialize() || candidate.session() == nullptr) {
            return fail(2);
        }
        settle_runtime_save(candidate);
        if (candidate.state() != platform::DungeonRuntimeState::running) {
            return fail(3);
        }
        drain(*candidate.session());
        candidate.fixed_tick({});
        drain(*candidate.session());
        const std::uint64_t revision_before =
            arpg::test::DungeonRuntimeTestAccess::authority_revision(candidate);
        const auto status_before = candidate.render_status();
        if (spec.stale_before_capture) {
            persistence::SaveCommitCompletion stale{};
            stale.kind = persistence::SaveCommitRequestKind::exact;
            stale.revision = revision_before;
            stale.intent = 0xBADU;
            stale.token = 0xBADU;
            stale.epoch = 0xBADU;
            stale.result.state = persistence::SaveCommitState::committed;
            arpg::test::DungeonRuntimeTestAccess::inject(candidate, stale);
            const auto status_after = candidate.render_status();
            if (status_after.indicator != status_before.indicator
                    || status_after.active_slot != status_before.active_slot
                    || status_after.error != status_before.error
                    || arpg::test::DungeonRuntimeTestAccess::authority_revision(
                        candidate) != revision_before) return fail(4);
        }
        if (spec.matching_background_before_capture) {
            arpg::test::DungeonRuntimeTestAccess::force_background_due(
                candidate);
            candidate.pump_persistence_frame();
            const std::uint64_t background_revision =
                arpg::test::DungeonRuntimeTestAccess::authority_revision(
                    candidate);
            const auto deadline = std::chrono::steady_clock::now()
                + std::chrono::seconds{10};
            while (arpg::test::DungeonRuntimeTestAccess::background_active(
                        candidate)
                    && std::chrono::steady_clock::now() < deadline) {
                candidate.pump_persistence_frame();
                std::this_thread::yield();
            }
            if (arpg::test::DungeonRuntimeTestAccess::background_active(
                        candidate)
                    || arpg::test::DungeonRuntimeTestAccess::durable_revision(
                        candidate) != background_revision) return fail(13);
        }
        hook.armed = true;
        if (spec.active_background) {
            arpg::test::DungeonRuntimeTestAccess::force_background_due(
                candidate);
            candidate.pump_persistence_frame();
            if (!wait_for_schedule_block(hook)
                    || !arpg::test::DungeonRuntimeTestAccess::
                        background_active(candidate)) return fail(5);
        }
        if (candidate.request_equip(1U)
                != dungeon::RequestResult::accepted) return fail(6);
        candidate.pump_persistence_frame();
        if (!spec.active_background && !wait_for_schedule_block(hook)) {
            return fail(7);
        }
        if (!arpg::test::DungeonRuntimeTestAccess::exact_active(candidate)
                || candidate.session()->pending_save_view() == nullptr) {
            return fail(8);
        }
        if (spec.stale_after_capture) {
            const auto matching =
                arpg::test::DungeonRuntimeTestAccess::matching_exact(candidate);
            const std::uint64_t durable_before =
                arpg::test::DungeonRuntimeTestAccess::durable_revision(
                    candidate);
            for (std::uint8_t mutation = 0U; mutation < 3U; ++mutation) {
                auto stale = matching;
                if (mutation == 0U) ++stale.revision;
                if (mutation == 1U) ++stale.token;
                if (mutation == 2U) ++stale.epoch;
                arpg::test::DungeonRuntimeTestAccess::inject(candidate, stale);
                if (!arpg::test::DungeonRuntimeTestAccess::exact_active(
                            candidate)
                        || candidate.session()->pending_save_view() == nullptr
                        || arpg::test::DungeonRuntimeTestAccess::
                            durable_revision(candidate) != durable_before) {
                    return fail(9);
                }
            }
        }
        const auto frozen = candidate.session()->snapshot();
        const std::uint64_t frozen_tick = frozen.combat.has_value()
            ? frozen.combat->tick : 0U;
        const std::uint64_t frozen_revision =
            arpg::test::DungeonRuntimeTestAccess::authority_revision(candidate);
        for (std::uint32_t frame = 0U;
                frame < spec.presentation_frames; ++frame) {
            // This is the exact per-presented-frame path used while pause or
            // inventory owns the host frame: persistence pumps, zero fixed
            // authority ticks.
            candidate.pump_persistence_frame();
            const auto presented = candidate.session()->snapshot();
            if (!presented.combat.has_value()
                    || presented.combat->tick != frozen_tick
                    || arpg::test::DungeonRuntimeTestAccess::
                        authority_revision(candidate) != frozen_revision) {
                return fail(10);
            }
        }
        hook.release = true;
        settle_runtime_save(candidate);
        if (candidate.state() != platform::DungeonRuntimeState::running
                || candidate.session()->pending_save_view() != nullptr
                || arpg::test::DungeonRuntimeTestAccess::exact_active(
                    candidate)
                || candidate.render_status().indicator
                    != platform::SaveIndicator::saved) return fail(11);
        outcome.saved_bytes = read_active_bytes(
            directory, candidate.render_status().active_slot);
        candidate.acknowledge_gameplay_rearmed();
        candidate.fixed_tick({});
        outcome.next_tick_bytes = capture_bytes(candidate, outcome);
        outcome.event_trace = drain_trace(*candidate.session());
        return !outcome.saved_bytes.empty()
                && !outcome.next_tick_bytes.empty()
            ? true : fail(12);
    };

    constexpr std::array<ScheduleSpec, 9U> kSchedules{{
        {0U, true, false, false, false},
        {0U, false, true, false, false},
        {0U, false, false, false, true},
        {1U, false, false, false, false},
        {7U, false, false, false, false},
        {31U, false, false, false, false},
        {31U, false, true, false, false},
        {31U, true, false, false, false},
        {31U, false, true, true, false},
    }};
    ScheduleOutcome schedule_baseline{};
    ARPG_REQUIRE(run_schedule(kSchedules[0U], schedule_baseline));
    for (std::size_t index = 1U; index < kSchedules.size(); ++index) {
        ScheduleOutcome observed{};
        ARPG_REQUIRE(run_schedule(kSchedules[index], observed));
        ARPG_REQUIRE(observed.saved_bytes
            == schedule_baseline.saved_bytes);
        ARPG_REQUIRE(observed.next_tick_bytes
            == schedule_baseline.next_tick_bytes);
        ARPG_REQUIRE(observed.event_trace
            == schedule_baseline.event_trace);
        ARPG_REQUIRE(observed.next_tick_rng
            == schedule_baseline.next_tick_rng);
        ARPG_REQUIRE(observed.equipped_ids
            == schedule_baseline.equipped_ids);
        ARPG_REQUIRE(observed.transaction_generation
            == schedule_baseline.transaction_generation);
        ARPG_REQUIRE(observed.next_authority_revision
            == schedule_baseline.next_authority_revision);
        ARPG_REQUIRE(observed.next_combat_tick
            == schedule_baseline.next_combat_tick);
    }
    return {};
}

arpg::test::Failure runtime_echoes_death_pending_kind() noexcept {
    TempDirectory directory;
    auto config = config_for(directory, 0xD34D10U);
    {
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
        settle_runtime_save(runtime);
        ARPG_REQUIRE(runtime.state() == platform::DungeonRuntimeState::running);
        ARPG_REQUIRE(session->snapshot().phase
            == dungeon::RoomPhase::death_pending);
        ARPG_REQUIRE(session->snapshot().death.has_value());
        ARPG_REQUIRE(session->snapshot().death->can_continue);

        auto decoded = std::make_unique<
            checkpoint::SaveCheckpointSlot>();
        ARPG_REQUIRE(decode_active_v9(
            directory, runtime.render_status().active_slot, *decoded));
        ARPG_REQUIRE(decoded->room_progress.lifecycle
            == checkpoint::RoomProgressLifecycle::death_pending);
        ARPG_REQUIRE(decoded->room_progress.combat.has_death_snapshot);
        ARPG_REQUIRE(decoded->state.death.lifecycle
            == checkpoint::DeathLifecycle::pending_continue);
        const auto saved_armor =
            decoded->room_progress.combat.death_snapshot.defense.armor;
        ++decoded->room_progress.combat.death_snapshot.defense.armor;
        ++decoded->room_progress.combat.player.armor;
        ARPG_REQUIRE(!checkpoint::
            valid_room_progress_checkpoint_structural(
                decoded->room_progress, decoded->state));
        decoded->room_progress.combat.death_snapshot.defense.armor =
            saved_armor;
        --decoded->room_progress.combat.player.armor;
        ARPG_REQUIRE(checkpoint::
            valid_room_progress_checkpoint_structural(
                decoded->room_progress, decoded->state));
    }
    config.continue_pending_death_on_initialize = true;
    platform::DungeonRuntime resumed(config);
    ARPG_REQUIRE(resumed.initialize());
    ARPG_REQUIRE(resumed.session()->snapshot().phase
        == dungeon::RoomPhase::transitioning);
    ARPG_REQUIRE(!resumed.session()->snapshot().death.has_value());
    ARPG_REQUIRE(!resumed.session()->snapshot().combat.has_value());
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
        == checkpoint::DeathLifecycle::none);
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
    settle_runtime_save(runtime);

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
    settle_runtime_save(runtime);

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
    runtime.acknowledge_gameplay_rearmed();
    runtime.fixed_tick({});
    settle_runtime_save(runtime);
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
    settle_runtime_save(runtime);

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
    settle_runtime_save(runtime);
    const auto death = runtime.session()->snapshot();
    ARPG_REQUIRE(death.phase == dungeon::RoomPhase::death_pending);
    const auto target_seed = death.death->checkpoint.target_room.seed;

    runtime.acknowledge_gameplay_rearmed();
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
    settle_runtime_save(runtime);
    ARPG_REQUIRE(runtime.session()->snapshot().phase
        == dungeon::RoomPhase::transitioning);
    runtime.acknowledge_gameplay_rearmed();
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
        dungeon::DungeonRunState expected{};
        if (!abyss_death) {
            dungeon::DungeonSession source{config.rules, initial};
            source.tick({});
            ARPG_REQUIRE(arpg::test::kill_current_player_through_combat(source));
            source.tick({});
            const dungeon::PendingSave* const pending =
                source.pending_save_view();
            ARPG_REQUIRE(pending != nullptr);
            ARPG_REQUIRE(pending->kind
                == dungeon::PendingSaveKind::death_retreat);
            expected = pending->next_state;
            ARPG_REQUIRE(store.commit(expected).state
                == persistence::SaveCommitState::committed);
        } else {
            auto source = std::make_unique<platform::DungeonRuntime>(config);
            ARPG_REQUIRE(source->initialize());
            settle_runtime_save(*source);
            ARPG_REQUIRE(source->session()->snapshot().is_abyss);
            source->session()->tick({});
            ARPG_REQUIRE(arpg::test::kill_current_player_through_combat(
                *source->session()));
            source->session()->tick({});
            const dungeon::PendingSave* const pending =
                source->session()->pending_save_view();
            ARPG_REQUIRE(pending != nullptr);
            ARPG_REQUIRE(pending->kind
                == dungeon::PendingSaveKind::death_retreat);
            expected = pending->next_state;
            ARPG_REQUIRE(expected.abyss.lifecycle
                == arpg::abyss::AbyssLifecycle::failed);
            ARPG_REQUIRE(expected.last_abyss_resolution.lifecycle
                == arpg::abyss::AbyssLifecycle::failed);
            settle_runtime_save(*source);
            ARPG_REQUIRE(source->session()->snapshot().phase
                == dungeon::RoomPhase::death_pending);

            auto exact = std::make_unique<
                checkpoint::SaveCheckpointSlot>();
            ARPG_REQUIRE(decode_active_v9(directory,
                source->render_status().active_slot, *exact));
            ARPG_REQUIRE(dungeon::same_run_state(exact->state, expected));
            ARPG_REQUIRE(exact->state.last_abyss_resolution.lifecycle
                == arpg::abyss::AbyssLifecycle::failed);
            ARPG_REQUIRE(exact->room_progress.lifecycle
                == checkpoint::RoomProgressLifecycle::death_pending);
            source.reset();
        }

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
            ARPG_REQUIRE(disk.checkpoint.last_abyss_resolution.lifecycle
                == arpg::abyss::AbyssLifecycle::failed);
            ARPG_REQUIRE(loaded.is_abyss == false);
        }
    }
    return {};
}

arpg::test::Failure production_startup_auto_continues_pending_death() noexcept {
    TempDirectory directory;
    auto config = config_for(directory, 0xD34D17U);
    persistence::SaveStore store(config.save);
    const auto initial = dungeon::make_initial_run_state(
        *config.new_run_seed, config.rules).state;
    ARPG_REQUIRE(store.commit(initial).state
        == persistence::SaveCommitState::committed);

    dungeon::DungeonSession source{config.rules, initial};
    source.tick({});
    ARPG_REQUIRE(arpg::test::kill_current_player_through_combat(source));
    source.tick({});
    const dungeon::PendingSave* const pending = source.pending_save_view();
    ARPG_REQUIRE(pending != nullptr);
    ARPG_REQUIRE(pending->kind == dungeon::PendingSaveKind::death_retreat);
    const dungeon::DungeonRunState dead = pending->next_state;
    ARPG_REQUIRE(store.commit(dead).state
        == persistence::SaveCommitState::committed);

    config.continue_pending_death_on_initialize = true;
    platform::DungeonRuntime runtime(config);
    ARPG_REQUIRE(runtime.initialize());

    const auto resumed = runtime.session()->snapshot();
    ARPG_REQUIRE(resumed.phase == dungeon::RoomPhase::transitioning);
    ARPG_REQUIRE(!resumed.death.has_value());
    ARPG_REQUIRE(!resumed.combat.has_value());
    ARPG_REQUIRE(resumed.commit_generation == dead.commit_generation + 1U);
    ARPG_REQUIRE(resumed.room_seed == dead.death.target_room.seed);
    ARPG_REQUIRE(resumed.depth == dead.death.target_room.depth);

    dungeon::DungeonRunState expected = dead;
    ++expected.commit_generation;
    expected.current_room = dead.death.target_room;
    expected.abyss = {};
    expected.death = {};
    const auto saved = store.load();
    ARPG_REQUIRE(saved.state == persistence::SaveLoadState::ready);
    ARPG_REQUIRE(dungeon::same_run_state(saved.checkpoint, expected));

    platform::DungeonRuntime relaunched(config);
    ARPG_REQUIRE(relaunched.initialize());
    const auto already_clear = relaunched.session()->snapshot();
    ARPG_REQUIRE(!already_clear.death.has_value());
    ARPG_REQUIRE(already_clear.commit_generation == expected.commit_generation);

    const auto saved_after_relaunch = store.load();
    ARPG_REQUIRE(saved_after_relaunch.state
        == persistence::SaveLoadState::ready);
    ARPG_REQUIRE(saved_after_relaunch.checkpoint.death.lifecycle
        == checkpoint::DeathLifecycle::none);
    ARPG_REQUIRE(saved_after_relaunch.checkpoint.commit_generation
        == expected.commit_generation);

    runtime.acknowledge_gameplay_rearmed();
    runtime.fixed_tick({});
    const auto playable = runtime.session()->snapshot();
    ARPG_REQUIRE(playable.phase == dungeon::RoomPhase::locked);
    ARPG_REQUIRE(playable.combat.has_value());
    ARPG_REQUIRE(playable.combat->player.hp > 0);
    ARPG_REQUIRE(playable.combat->player.hp
        == playable.combat->player.max_hp);
    return {};
}

arpg::test::Failure production_startup_repairs_stale_death_target() noexcept {
    TempDirectory directory;
    auto config = config_for(directory, 0xD34D18U);
    persistence::SaveStore store(config.save);
    const auto initial = dungeon::make_initial_run_state(
        *config.new_run_seed, config.rules).state;
    ARPG_REQUIRE(store.commit(initial).state
        == persistence::SaveCommitState::committed);

    dungeon::DungeonSession source{config.rules, initial};
    source.tick({});
    ARPG_REQUIRE(arpg::test::kill_current_player_through_combat(source));
    source.tick({});
    const dungeon::PendingSave* const pending = source.pending_save_view();
    ARPG_REQUIRE(pending != nullptr);
    dungeon::DungeonRunState stale = pending->next_state;
    const auto expected_target = stale.death.target_room;
    stale.death.target_room.seed ^= 0x5A5A5A5A5A5A5A5AULL;
    ARPG_REQUIRE(store.commit(stale).state
        == persistence::SaveCommitState::committed);

    config.continue_pending_death_on_initialize = true;
    platform::DungeonRuntime runtime(config);
    ARPG_REQUIRE(runtime.initialize());
    const auto resumed = runtime.session()->snapshot();
    ARPG_REQUIRE(runtime.state() == platform::DungeonRuntimeState::running);
    ARPG_REQUIRE(resumed.phase == dungeon::RoomPhase::transitioning);
    ARPG_REQUIRE(!resumed.death.has_value());
    ARPG_REQUIRE(resumed.room_seed == expected_target.seed);
    ARPG_REQUIRE(resumed.depth == expected_target.depth);
    ARPG_REQUIRE(resumed.commit_generation == stale.commit_generation + 1U);

    const auto saved = store.load();
    ARPG_REQUIRE(saved.state == persistence::SaveLoadState::ready);
    ARPG_REQUIRE(saved.checkpoint.death.lifecycle
        == checkpoint::DeathLifecycle::none);
    ARPG_REQUIRE(saved.checkpoint.current_room.seed == expected_target.seed);
    ARPG_REQUIRE(saved.checkpoint.current_room.depth == expected_target.depth);
    return {};
}

arpg::test::Failure production_startup_death_continue_commit_failures_fault()
    noexcept {
    constexpr std::array<persistence::SaveFaultPoint, 2U> kFaultPoints{{
        persistence::SaveFaultPoint::before_publish,
        persistence::SaveFaultPoint::after_publish,
    }};
    std::uint64_t seed = 0xD34D20U;
    for (const auto point : kFaultPoints) {
        TempDirectory directory;
        auto config = config_for(directory, seed++);
        FaultContext fault{point, false};
        config.save.fault_hook = &fail_when_enabled;
        config.save.fault_context = &fault;
        persistence::SaveStore store(config.save);
        const auto initial = dungeon::make_initial_run_state(
            *config.new_run_seed, config.rules).state;
        ARPG_REQUIRE(store.commit(initial).state
            == persistence::SaveCommitState::committed);

        dungeon::DungeonSession source{config.rules, initial};
        source.tick({});
        ARPG_REQUIRE(arpg::test::kill_current_player_through_combat(source));
        source.tick({});
        const dungeon::PendingSave* const pending = source.pending_save_view();
        ARPG_REQUIRE(pending != nullptr);
        ARPG_REQUIRE(store.commit(pending->next_state).state
            == persistence::SaveCommitState::committed);

        fault.enabled = true;
        config.continue_pending_death_on_initialize = true;
        platform::DungeonRuntime runtime(config);
        ARPG_REQUIRE(!runtime.initialize());
        ARPG_REQUIRE(runtime.state() == platform::DungeonRuntimeState::faulted);
        ARPG_REQUIRE(runtime.render_status().faulted);
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
            settle_runtime_save(*runtime);
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

void settle_clean_shutdown(platform::DungeonRuntime& runtime) noexcept {
    const auto deadline = std::chrono::steady_clock::now()
        + std::chrono::seconds{10};
    while (runtime.clean_shutdown_state()
                == platform::CleanShutdownState::closing
            && std::chrono::steady_clock::now() < deadline) {
        runtime.pump_persistence_frame();
        std::this_thread::yield();
    }
}

arpg::test::Failure clean_shutdown_commits_latest_authority_and_rearms()
    noexcept {
    TempDirectory directory;
    platform::DungeonRuntime runtime(config_for(directory, 0xC105E1U));
    ARPG_REQUIRE(runtime.initialize());
    runtime.fixed_tick({1, 0});
    ARPG_REQUIRE(runtime.request_clean_shutdown());
    ARPG_REQUIRE(runtime.gameplay_rearm_required());
    settle_clean_shutdown(runtime);
    ARPG_REQUIRE(runtime.clean_shutdown_state()
        == platform::CleanShutdownState::ready);
    ARPG_REQUIRE(runtime.state() == platform::DungeonRuntimeState::running);
    runtime.acknowledge_gameplay_rearmed();
    ARPG_REQUIRE(!runtime.gameplay_rearm_required());

    platform::DungeonRuntime resumed(config_for(directory, 0xBADU));
    ARPG_REQUIRE(resumed.initialize());
    ARPG_REQUIRE(resumed.session()->snapshot().commit_generation
        == runtime.session()->snapshot().commit_generation);
    return {};
}

arpg::test::Failure death_pending_clean_shutdown_reuses_durable_exact_save()
    noexcept {
    TempDirectory directory;
    const auto config = config_for(directory, 0xD34DC105U);
    std::uint64_t durable_death_revision{};
    {
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

        settle_runtime_save(runtime);
        ARPG_REQUIRE(runtime.state()
            == platform::DungeonRuntimeState::running);
        ARPG_REQUIRE(session->pending_save_view() == nullptr);
        const auto death = session->snapshot();
        ARPG_REQUIRE(death.phase == dungeon::RoomPhase::death_pending);
        ARPG_REQUIRE(death.death.has_value());
        ARPG_REQUIRE(death.death->can_continue);
        ARPG_REQUIRE(!arpg::test::DungeonRuntimeTestAccess::exact_active(
            runtime));
        ARPG_REQUIRE(!arpg::test::DungeonRuntimeTestAccess::background_active(
            runtime));
        durable_death_revision =
            arpg::test::DungeonRuntimeTestAccess::durable_revision(runtime);
        ARPG_REQUIRE(durable_death_revision
            == arpg::test::DungeonRuntimeTestAccess::authority_revision(
                runtime));

        auto exact = std::make_unique<checkpoint::SaveCheckpointSlot>();
        ARPG_REQUIRE(decode_active_v9(
            directory, runtime.render_status().active_slot, *exact));
        ARPG_REQUIRE(exact->persistence_revision == durable_death_revision);
        ARPG_REQUIRE(exact->state.death.lifecycle
            == checkpoint::DeathLifecycle::pending_continue);
        ARPG_REQUIRE(exact->room_progress.lifecycle
            == checkpoint::RoomProgressLifecycle::death_pending);

        ARPG_REQUIRE(runtime.request_clean_shutdown());
        settle_clean_shutdown(runtime);
        ARPG_REQUIRE(runtime.clean_shutdown_state()
            == platform::CleanShutdownState::ready);
    }

    platform::DungeonRuntime resumed(config);
    ARPG_REQUIRE(resumed.initialize());
    const auto loaded = resumed.session()->snapshot();
    ARPG_REQUIRE(loaded.phase == dungeon::RoomPhase::death_pending);
    ARPG_REQUIRE(loaded.death.has_value());
    ARPG_REQUIRE(loaded.death->can_continue);
    auto exact = std::make_unique<checkpoint::SaveCheckpointSlot>();
    ARPG_REQUIRE(decode_active_v9(
        directory, resumed.render_status().active_slot, *exact));
    ARPG_REQUIRE(exact->persistence_revision == durable_death_revision);
    ARPG_REQUIRE(exact->state.death.lifecycle
        == checkpoint::DeathLifecycle::pending_continue);
    ARPG_REQUIRE(exact->room_progress.lifecycle
        == checkpoint::RoomProgressLifecycle::death_pending);
    return {};
}

arpg::test::Failure background_durable_shutdown_still_runs_exact_final_scan()
    noexcept {
    TempDirectory directory;
    FaultContext fault{persistence::SaveFaultPoint::final_scan_a, false};
    auto config = config_for(directory, 0xC105E4U);
    config.save.fault_hook = &fail_when_enabled;
    config.save.fault_context = &fault;
    platform::DungeonRuntime runtime(config);
    ARPG_REQUIRE(runtime.initialize());

    runtime.fixed_tick({1, 0});
    ARPG_REQUIRE(arpg::test::DungeonRuntimeTestAccess::authority_revision(
        runtime) > arpg::test::DungeonRuntimeTestAccess::durable_revision(
        runtime));
    arpg::test::DungeonRuntimeTestAccess::force_background_due(runtime);
    const auto deadline = std::chrono::steady_clock::now()
        + std::chrono::seconds{10};
    do {
        runtime.pump_persistence_frame();
        if (!arpg::test::DungeonRuntimeTestAccess::background_active(runtime)
                && arpg::test::DungeonRuntimeTestAccess::durable_revision(
                    runtime)
                    == arpg::test::DungeonRuntimeTestAccess::authority_revision(
                        runtime)) {
            break;
        }
        std::this_thread::yield();
    } while (std::chrono::steady_clock::now() < deadline);
    ARPG_REQUIRE(!arpg::test::DungeonRuntimeTestAccess::background_active(
        runtime));
    ARPG_REQUIRE(arpg::test::DungeonRuntimeTestAccess::durable_revision(runtime)
        == arpg::test::DungeonRuntimeTestAccess::authority_revision(runtime));

    fault.enabled = true;
    ARPG_REQUIRE(runtime.request_clean_shutdown());
    settle_clean_shutdown(runtime);
    ARPG_REQUIRE(runtime.clean_shutdown_state()
        == platform::CleanShutdownState::faulted);
    ARPG_REQUIRE(runtime.state() == platform::DungeonRuntimeState::faulted);
    ARPG_REQUIRE(runtime.render_status().error
        == persistence::SaveError::final_scan_failed);
    return {};
}

arpg::test::Failure clean_shutdown_not_committed_cancels_close() noexcept {
    TempDirectory directory;
    FaultContext fault{persistence::SaveFaultPoint::before_publish, false};
    auto config = config_for(directory, 0xC105E2U);
    config.save.fault_hook = &fail_when_enabled;
    config.save.fault_context = &fault;
    platform::DungeonRuntime runtime(config);
    ARPG_REQUIRE(runtime.initialize());
    ARPG_REQUIRE(clear_and_await(runtime));
    ARPG_REQUIRE(runtime.session()->request_passive_allocation(1U));
    fault.enabled = true;
    ARPG_REQUIRE(runtime.request_clean_shutdown());
    settle_clean_shutdown(runtime);
    ARPG_REQUIRE(runtime.clean_shutdown_state()
        == platform::CleanShutdownState::canceled);
    ARPG_REQUIRE(runtime.state() == platform::DungeonRuntimeState::running);
    ARPG_REQUIRE(runtime.render_status().indicator
        == platform::SaveIndicator::error);
    return {};
}

arpg::test::Failure clean_shutdown_indeterminate_faults() noexcept {
    TempDirectory directory;
    FaultContext fault{persistence::SaveFaultPoint::after_publish, false};
    auto config = config_for(directory, 0xC105E3U);
    config.save.fault_hook = &fail_when_enabled;
    config.save.fault_context = &fault;
    platform::DungeonRuntime runtime(config);
    ARPG_REQUIRE(runtime.initialize());
    ARPG_REQUIRE(clear_and_await(runtime));
    ARPG_REQUIRE(runtime.session()->request_passive_allocation(1U));
    fault.enabled = true;
    ARPG_REQUIRE(runtime.request_clean_shutdown());
    settle_clean_shutdown(runtime);
    ARPG_REQUIRE(runtime.clean_shutdown_state()
        == platform::CleanShutdownState::faulted);
    ARPG_REQUIRE(runtime.state() == platform::DungeonRuntimeState::faulted);
    return {};
}

arpg::test::Failure health_potion_exact_persists_post_heal_room_state()
    noexcept {
    TempDirectory directory;
    auto runtime = std::make_unique<platform::DungeonRuntime>(
        config_for(directory, 0xEA1101U));
    ARPG_REQUIRE(runtime->initialize());
    const auto before = runtime->session()->snapshot();
    ARPG_REQUIRE(before.combat.has_value());
    const int injured_hp = (std::max)(1, before.combat->player.max_hp / 4);
    arpg::test::set_player_health(*runtime->session(), injured_hp,
        before.combat->player.max_hp);
    arpg::test::install_ground_health_potion(*runtime->session(), 0U,
        before.combat->player.position);

    runtime->fixed_tick({});
    ARPG_REQUIRE(runtime->session()->pending_save_view() != nullptr);
    ARPG_REQUIRE(runtime->session()->pending_save_view()->kind
        == dungeon::PendingSaveKind::health_potion_pickup);
    settle_runtime_save(*runtime);
    const auto healed = runtime->session()->snapshot();
    ARPG_REQUIRE(healed.combat.has_value());
    ARPG_REQUIRE(healed.combat->player.hp > injured_hp);

    auto decoded = std::make_unique<
        checkpoint::SaveCheckpointSlot>();
    ARPG_REQUIRE(decode_active_v9(
        directory, runtime->render_status().active_slot, *decoded));
    ARPG_REQUIRE(decoded->room_progress.combat.player.hp
        == healed.combat->player.hp);
    ARPG_REQUIRE((decoded->room_progress.secondary_claim_bits[0U]
        & (std::uint64_t{1U} << dungeon::health_potion_claim_ordinal(0U)))
        != 0U);
    for (std::uint16_t index = 0U;
            index < decoded->room_progress.secondary_ground_count; ++index) {
        ARPG_REQUIRE(decoded->room_progress.secondary_ground[index].tag
            != checkpoint::SecondaryGroundTag::health_potion);
    }
    const int expected_hp = healed.combat->player.hp;
    runtime.reset();

    platform::DungeonRuntime restarted(config_for(directory, 0xBADU));
    ARPG_REQUIRE(restarted.initialize());
    const auto restored = restarted.session()->snapshot();
    ARPG_REQUIRE(restored.combat.has_value());
    ARPG_REQUIRE(restored.combat->player.hp == expected_hp);
    const auto& potions = arpg::test::DungeonSessionTestAccess::
        ground_health_potions(*restarted.session());
    ARPG_REQUIRE(std::none_of(potions.begin(), potions.end(),
        [](const dungeon::GroundHealthPotion& potion) noexcept {
            return potion.active;
        }));
    return {};
}

arpg::test::Failure normal_full_clear_is_exact_and_reloads_awaiting_exit()
    noexcept {
    TempDirectory directory;
    auto runtime = std::make_unique<platform::DungeonRuntime>(
        config_for(directory, 0xEA1102U));
    ARPG_REQUIRE(runtime->initialize());
    ARPG_REQUIRE(clear_and_await(*runtime));
    ARPG_REQUIRE(runtime->session()->snapshot().phase
        == dungeon::RoomPhase::awaiting_exit);

    auto decoded = std::make_unique<
        checkpoint::SaveCheckpointSlot>();
    ARPG_REQUIRE(decode_active_v9(
        directory, runtime->render_status().active_slot, *decoded));
    ARPG_REQUIRE(decoded->room_progress.lifecycle
        == checkpoint::RoomProgressLifecycle::active);
    ARPG_REQUIRE(decoded->room_progress.full_clear);
    ARPG_REQUIRE(decoded->room_progress.exits_unlocked);
    ARPG_REQUIRE(decoded->room_progress.reward_committed);
    runtime.reset();

    platform::DungeonRuntime restarted(config_for(directory, 0xBADU));
    ARPG_REQUIRE(restarted.initialize());
    ARPG_REQUIRE(restarted.session()->snapshot().phase
        == dungeon::RoomPhase::awaiting_exit);
    return {};
}

arpg::test::Failure abyss_full_clear_persists_cleared_environment_and_reload()
    noexcept {
    TempDirectory directory;
    auto config = config_for(directory, 0xEA1103U);
    persistence::SaveStore store(config.save);
    const auto available = runtime_available_state();
    ARPG_REQUIRE(store.commit(available).state
        == persistence::SaveCommitState::committed);
    auto runtime = std::make_unique<platform::DungeonRuntime>(config);
    ARPG_REQUIRE(runtime->initialize());
    settle_runtime_save(*runtime);
    ARPG_REQUIRE(runtime->session()->snapshot().is_abyss);
    ARPG_REQUIRE(clear_and_await(*runtime));

    auto decoded = std::make_unique<
        checkpoint::SaveCheckpointSlot>();
    ARPG_REQUIRE(decode_active_v9(
        directory, runtime->render_status().active_slot, *decoded));
    ARPG_REQUIRE(decoded->state.abyss.lifecycle
        == arpg::abyss::AbyssLifecycle::cleared);
    ARPG_REQUIRE(decoded->room_progress.full_clear);
    ARPG_REQUIRE(decoded->room_progress.exits_unlocked);
    ARPG_REQUIRE(decoded->room_progress.reward_committed);
    ARPG_REQUIRE(decoded->room_progress.combat.abyss_environment.rule
        == arpg::abyss::AbyssRuleId::none);
    ARPG_REQUIRE(!decoded->room_progress.combat.abyss_environment.active);
    ARPG_REQUIRE(!decoded->room_progress.combat.abyss_environment.warning);
    runtime.reset();

    platform::DungeonRuntime restarted(config_for(directory, 0xBADU));
    ARPG_REQUIRE(restarted.initialize());
    ARPG_REQUIRE(restarted.session()->snapshot().phase
        == dungeon::RoomPhase::awaiting_exit);
    return {};
}

arpg::test::Failure render_snapshot_has_one_stable_heap_slot() noexcept {
    static_assert(sizeof(platform::DungeonRuntime)
        < sizeof(dungeon::DungeonRenderSnapshot));
    TempDirectory failed_directory;
    platform::DungeonRuntime failed(config_for(failed_directory, 0xC4A001U));
    {
        arpg::test::ScopedAllocationFailure fail_first_allocation{0U};
        ARPG_REQUIRE(!failed.initialize());
    }
    ARPG_REQUIRE(failed.state() == platform::DungeonRuntimeState::faulted);
    ARPG_REQUIRE(failed.render_snapshot_storage() == nullptr);

    TempDirectory directory;
    platform::DungeonRuntime runtime(config_for(directory, 0xC4A002U));
    ARPG_REQUIRE(runtime.initialize());
    dungeon::DungeonRenderSnapshot* const storage =
        runtime.render_snapshot_storage();
    ARPG_REQUIRE(storage != nullptr);
    ARPG_REQUIRE(runtime.session() != nullptr);
    const auto current = std::make_unique<dungeon::DungeonSnapshot>(
        runtime.session()->snapshot());
    ARPG_REQUIRE(current != nullptr && current->combat.has_value());
    const combat::Vec3 player = current->combat->player.position;
    const dungeon::WorldViewQuery query{
        {{player.x - 12.0F, player.y - 5.5F, -1.0F},
            {player.x + 12.0F, player.y + 5.5F, 32.0F}},
        1920, 1080, 1U};
    ARPG_REQUIRE(runtime.session()->write_render_snapshot(query, *storage));
    ARPG_REQUIRE(storage->query.camera_version == 1U);
    runtime.fixed_tick({});
    ARPG_REQUIRE(runtime.render_snapshot_storage() == storage);
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
    {"production startup auto continues pending death", &production_startup_auto_continues_pending_death},
    {"production startup repairs stale death target", &production_startup_repairs_stale_death_target},
    {"production startup death continue commit failures fault",
        &production_startup_death_continue_commit_failures_fault},
    {"item request fault matrix is atomic and restart consistent", &item_request_fault_matrix_is_atomic_and_restart_consistent},
    {"clean shutdown commits latest authority and rearms",
        &clean_shutdown_commits_latest_authority_and_rearms},
    {"death pending clean shutdown reuses durable exact save",
        &death_pending_clean_shutdown_reuses_durable_exact_save},
    {"background durable shutdown still runs exact final scan",
        &background_durable_shutdown_still_runs_exact_final_scan},
    {"clean shutdown not committed cancels close",
        &clean_shutdown_not_committed_cancels_close},
    {"clean shutdown indeterminate faults",
        &clean_shutdown_indeterminate_faults},
    {"health potion exact persists post heal room state",
        &health_potion_exact_persists_post_heal_room_state},
    {"normal full clear exact reloads awaiting exit",
        &normal_full_clear_is_exact_and_reloads_awaiting_exit},
    {"abyss full clear persists cleared environment",
        &abyss_full_clear_persists_cleared_environment_and_reload},
    {"render snapshot uses one stable heap slot",
        &render_snapshot_has_one_stable_heap_slot},
};

}  // namespace

arpg::test::TestSuite dungeon_runtime_suite() noexcept {
    return arpg::test::make_suite("dungeon_runtime", kCases);
}
