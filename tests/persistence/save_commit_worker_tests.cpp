#include "test_framework.hpp"

#include "abyss/abyss_rewards.hpp"
#include "abyss/abyss_rules.hpp"
#include "combat/combat_world.hpp"
#include "persistence/save_commit_worker.hpp"

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <iterator>
#include <limits>
#include <string>
#include <thread>
#include <type_traits>
#include <vector>

namespace arpg::test {
void set_persistence_allocation_failure_countdown(
    int successful_allocations) noexcept;
bool persistence_allocation_failure_is_armed() noexcept;
}

namespace {

using namespace arpg;

static_assert(!std::is_copy_constructible_v<persistence::SaveCommitStorage>);
static_assert(!std::is_move_constructible_v<persistence::SaveCommitStorage>);
static_assert(sizeof(persistence::SaveCommitWorker) <= 1024U);
static_assert(persistence::kSaveCommitStorageResidentBytes
    <= persistence::kSaveCommitResidentBudgetBytes);

items::ItemInstance normal_item(std::uint64_t id) noexcept {
    items::ItemInstance item{};
    item.id = id;
    item.base_id = 1U;
    item.rarity = items::ItemRarity::normal;
    item.item_level = 1U;
    item.required_level = 1U;
    return item;
}

class TempDirectory final {
public:
    TempDirectory() {
        path = std::filesystem::temp_directory_path()
            / ("arpg_v9_worker_" + std::to_string(
                std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::create_directories(path);
    }
    ~TempDirectory() {
        std::error_code error;
        std::filesystem::remove_all(path, error);
    }
    std::filesystem::path path{};
};

bool fixture(dungeon::checkpoint::SaveCheckpointSlot& slot,
    std::uint64_t revision) noexcept {
    dungeon::checkpoint::clear_save_checkpoint_slot(slot);
    slot.persistence_revision = revision;
    slot.state.root_seed = 0x9000U;
    slot.state.commit_generation = 3U;
    slot.state.current_room.index = 9U;
    slot.state.current_room.seed = 11U;
    const auto danger = abyss::danger_for_rule(
        abyss::AbyssRuleId::thunderstorm);
    if (!danger.has_value()) return false;
    const std::uint8_t total = abyss::reward_profile_for(
        *danger, 1U).item_count;
    slot.state.last_abyss_resolution = {
        true,
        0xAB155U,
        abyss::AbyssRuleId::thunderstorm,
        total,
        0U,
        0U,
        total,
        abyss::AbyssLifecycle::failed,
    };
    slot.room_progress.lifecycle =
        dungeon::checkpoint::RoomProgressLifecycle::active;
    slot.room_progress.room_index = 9U;
    slot.room_progress.room_seed = 11U;
    slot.room_progress.monster_generator_version = 1U;
    slot.room_progress.monster_blueprint_hash = 0xA11CEULL;
    slot.room_progress.environment_generator_version = 1U;
    slot.room_progress.environment_blueprint_hash = 0xE117ULL;
    slot.room_progress.generated_monsters = 3U;
    slot.room_progress.required_kills = 1U;
    combat::CombatWorld world{};
    return world.capture_room_checkpoint(slot.room_progress.combat);
}

bool wait_completion(persistence::SaveCommitWorker& worker,
    persistence::SaveCommitCompletion& completion) noexcept {
    const auto deadline = std::chrono::steady_clock::now()
        + std::chrono::seconds{10};
    while (std::chrono::steady_clock::now() < deadline) {
        if (worker.try_take_completion(completion)) return true;
        std::this_thread::yield();
    }
    return false;
}

bool write_v9(const std::filesystem::path& path,
    const dungeon::checkpoint::SaveCheckpointSlot& slot) {
    std::vector<std::uint8_t> bytes(
        persistence::kMaximumEncodedCheckpointBytes);
    std::size_t written{};
    if (persistence::encode_checkpoint_v9_into(
            slot, bytes.data(), bytes.size(), written)
            != persistence::CodecError::none) return false;
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output.write(reinterpret_cast<const char*>(bytes.data()),
        static_cast<std::streamsize>(written));
    return output.good();
}

bool write_bytes(const std::filesystem::path& path,
    const std::vector<std::uint8_t>& bytes) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output.write(reinterpret_cast<const char*>(bytes.data()),
        static_cast<std::streamsize>(bytes.size()));
    return output.good();
}

struct SingleFaultHook final {
    persistence::SaveFaultPoint point{};
};

bool fail_single_worker_point(persistence::SaveFaultPoint point,
    void* context) noexcept {
    const auto* const fault = static_cast<const SingleFaultHook*>(context);
    return fault != nullptr && point == fault->point;
}

test::Failure worker_commits_v9_and_readback_verifies_exact_bytes() noexcept {
    TempDirectory directory{};
    std::unique_ptr<persistence::SaveCommitStorage> storage{
        new (std::nothrow) persistence::SaveCommitStorage{}};
    ARPG_REQUIRE(storage != nullptr);
    ARPG_REQUIRE(storage->initialize({directory.path}));
    ARPG_REQUIRE(storage->resident_bytes() >= sizeof(*storage));
    ARPG_REQUIRE(storage->resident_bytes()
        <= persistence::kSaveCommitStorageResidentBytes);
    ARPG_REQUIRE(storage->resident_bytes()
        <= persistence::kSaveCommitResidentBudgetBytes);
    persistence::SaveCommitWorker worker{*storage};
    ARPG_REQUIRE(worker.start());
    const auto lease = worker.acquire_capture_slot(17U,
        persistence::SaveCommitRequestKind::exact, 101U);
    ARPG_REQUIRE(lease.state == persistence::SaveCommitSubmitState::accepted);
    auto* const job = worker.capture_job(lease);
    ARPG_REQUIRE(job != nullptr);
    ARPG_REQUIRE(fixture(job->checkpoint, 17U));
    ARPG_REQUIRE(worker.submit(lease).state
        == persistence::SaveCommitSubmitState::accepted);
    persistence::SaveCommitCompletion completion{};
    ARPG_REQUIRE(wait_completion(worker, completion));
    ARPG_REQUIRE(completion.job_slot == 0U);
    ARPG_REQUIRE(completion.revision == 17U);
    ARPG_REQUIRE(completion.intent == 101U);
    ARPG_REQUIRE(completion.token == lease.token);
    ARPG_REQUIRE(completion.epoch == lease.epoch);
    ARPG_REQUIRE(completion.result.state
        == persistence::SaveCommitState::committed);
    worker.stop_and_join();

    const std::filesystem::path* const saved = completion.result.active_slot
            == persistence::SaveSlot::a
        ? storage->slot_path(0U) : storage->slot_path(1U);
    ARPG_REQUIRE(saved != nullptr);
    ARPG_REQUIRE(storage->slot_path(2U) == nullptr);
    ARPG_REQUIRE(storage->temp_path(2U) == nullptr);
    std::ifstream input(*saved, std::ios::binary);
    std::vector<std::uint8_t> bytes{
        std::istreambuf_iterator<char>{input},
        std::istreambuf_iterator<char>{}};
    ARPG_REQUIRE(!bytes.empty());
    std::unique_ptr<dungeon::checkpoint::SaveCheckpointSlot> decoded{
        new (std::nothrow) dungeon::checkpoint::SaveCheckpointSlot{}};
    ARPG_REQUIRE(decoded != nullptr);
    bool migrated{};
    ARPG_REQUIRE(persistence::decode_checkpoint_v9_into(bytes.data(),
        bytes.size(), *decoded, migrated)
        == persistence::CodecError::none);
    ARPG_REQUIRE(!migrated);
    ARPG_REQUIRE(decoded->persistence_revision == 17U);
    ARPG_REQUIRE(decoded->state.last_abyss_resolution.lifecycle
        == abyss::AbyssLifecycle::failed);

    TempDirectory stale_directory{};
    ARPG_REQUIRE(std::filesystem::create_directory(
        stale_directory.path / "run_a.tmp"));
    std::unique_ptr<persistence::SaveCommitStorage> retry_storage{
        new (std::nothrow) persistence::SaveCommitStorage{}};
    ARPG_REQUIRE(retry_storage != nullptr);
    ARPG_REQUIRE(!retry_storage->initialize({stale_directory.path}));
    ARPG_REQUIRE(retry_storage->resident_bytes() == 0U);
    ARPG_REQUIRE(std::filesystem::remove(
        stale_directory.path / "run_a.tmp"));
    ARPG_REQUIRE(retry_storage->initialize({stale_directory.path}));

    TempDirectory start_directory{};
    SingleFaultHook start_fault{persistence::SaveFaultPoint::worker_start};
    persistence::SaveStoreConfig start_config{start_directory.path};
    start_config.fault_hook = &fail_single_worker_point;
    start_config.fault_context = &start_fault;
    std::unique_ptr<persistence::SaveCommitStorage> start_storage{
        new (std::nothrow) persistence::SaveCommitStorage{}};
    ARPG_REQUIRE(start_storage != nullptr);
    ARPG_REQUIRE(start_storage->initialize(start_config));
    const std::size_t start_resident = start_storage->resident_bytes();
    persistence::SaveCommitWorker start_worker{*start_storage};
    ARPG_REQUIRE(!start_worker.start());
    ARPG_REQUIRE(!start_worker.idle());
    ARPG_REQUIRE(start_worker.acquire_capture_slot(1U,
        persistence::SaveCommitRequestKind::exact).state
        == persistence::SaveCommitSubmitState::stopped);
    ARPG_REQUIRE(start_storage->resident_bytes() == start_resident);
    ARPG_REQUIRE(start_storage->load_state()
        == persistence::SaveLoadState::empty);
    start_fault.point = persistence::SaveFaultPoint::before_archive;
    ARPG_REQUIRE(start_worker.start());
    const auto start_lease = start_worker.acquire_capture_slot(1U,
        persistence::SaveCommitRequestKind::exact, 0x51U);
    ARPG_REQUIRE(start_lease.state
        == persistence::SaveCommitSubmitState::accepted);
    ARPG_REQUIRE(start_lease.epoch == 1U);
    auto* const start_job = start_worker.capture_job(start_lease);
    ARPG_REQUIRE(start_job != nullptr);
    ARPG_REQUIRE(fixture(start_job->checkpoint, 1U));
    ARPG_REQUIRE(start_worker.submit(start_lease).state
        == persistence::SaveCommitSubmitState::accepted);
    persistence::SaveCommitCompletion start_completion{};
    ARPG_REQUIRE(wait_completion(start_worker, start_completion));
    ARPG_REQUIRE(start_completion.result.state
        == persistence::SaveCommitState::committed);
    start_worker.stop_and_join();
    ARPG_REQUIRE(start_worker.start());
    const auto restarted_start = start_worker.acquire_capture_slot(2U,
        persistence::SaveCommitRequestKind::exact, 0x52U);
    ARPG_REQUIRE(restarted_start.epoch == 2U);
    start_worker.cancel_capture(restarted_start);
    start_worker.stop_and_join();

    struct FaultExpectation final {
        persistence::SaveFaultPoint point{};
        persistence::SaveCommitState state{};
        persistence::SaveError error{};
    };
    constexpr std::array<FaultExpectation, 11U> kIoFaults{{
        {persistence::SaveFaultPoint::before_temp_write,
            persistence::SaveCommitState::not_committed,
            persistence::SaveError::write_failed},
        {persistence::SaveFaultPoint::temp_flush,
            persistence::SaveCommitState::not_committed,
            persistence::SaveError::flush_failed},
        {persistence::SaveFaultPoint::temp_close,
            persistence::SaveCommitState::not_committed,
            persistence::SaveError::close_failed},
        {persistence::SaveFaultPoint::after_temp_write,
            persistence::SaveCommitState::not_committed,
            persistence::SaveError::write_failed},
        {persistence::SaveFaultPoint::temp_readback,
            persistence::SaveCommitState::not_committed,
            persistence::SaveError::readback_failed},
        {persistence::SaveFaultPoint::after_temp_validation,
            persistence::SaveCommitState::not_committed,
            persistence::SaveError::publish_failed},
        {persistence::SaveFaultPoint::before_publish,
            persistence::SaveCommitState::not_committed,
            persistence::SaveError::publish_failed},
        {persistence::SaveFaultPoint::after_publish,
            persistence::SaveCommitState::indeterminate,
            persistence::SaveError::publish_failed},
        {persistence::SaveFaultPoint::final_scan_a,
            persistence::SaveCommitState::indeterminate,
            persistence::SaveError::final_scan_failed},
        {persistence::SaveFaultPoint::final_scan_b,
            persistence::SaveCommitState::indeterminate,
            persistence::SaveError::final_scan_failed},
        {persistence::SaveFaultPoint::before_archive,
            persistence::SaveCommitState::committed,
            persistence::SaveError::none},
    }};
    for (const FaultExpectation& expected : kIoFaults) {
        TempDirectory fault_directory{};
        SingleFaultHook fault{expected.point};
        persistence::SaveStoreConfig fault_config{fault_directory.path};
        fault_config.fault_hook = &fail_single_worker_point;
        fault_config.fault_context = &fault;
        std::unique_ptr<persistence::SaveCommitStorage> fault_storage{
            new (std::nothrow) persistence::SaveCommitStorage{}};
        ARPG_REQUIRE(fault_storage != nullptr);
        ARPG_REQUIRE(fault_storage->initialize(fault_config));
        persistence::SaveCommitWorker fault_worker{*fault_storage};
        ARPG_REQUIRE(fault_worker.start());
        const auto fault_lease = fault_worker.acquire_capture_slot(1U,
            persistence::SaveCommitRequestKind::exact, 9U);
        auto* const fault_job = fault_worker.capture_job(fault_lease);
        ARPG_REQUIRE(fault_job != nullptr);
        ARPG_REQUIRE(fixture(fault_job->checkpoint, 1U));
        ARPG_REQUIRE(fault_worker.submit(fault_lease).state
            == persistence::SaveCommitSubmitState::accepted);
        persistence::SaveCommitCompletion fault_completion{};
        ARPG_REQUIRE(wait_completion(fault_worker, fault_completion));
        ARPG_REQUIRE(fault_completion.result.state == expected.state);
        ARPG_REQUIRE(fault_completion.result.error == expected.error);
        fault_worker.stop_and_join();
    }

    TempDirectory maximum_directory{};
    std::unique_ptr<persistence::SaveCommitStorage> maximum_storage{
        new (std::nothrow) persistence::SaveCommitStorage{}};
    ARPG_REQUIRE(maximum_storage != nullptr);
    ARPG_REQUIRE(maximum_storage->initialize({maximum_directory.path}));
    persistence::SaveCommitWorker maximum_worker{*maximum_storage};
    ARPG_REQUIRE(maximum_worker.start());
    const auto maximum_lease = maximum_worker.acquire_capture_slot(1U,
        persistence::SaveCommitRequestKind::exact, 77U);
    auto* const maximum_job = maximum_worker.capture_job(maximum_lease);
    ARPG_REQUIRE(maximum_job != nullptr);
    ARPG_REQUIRE(fixture(maximum_job->checkpoint, 1U));
    auto& maximum_items =
        maximum_job->checkpoint.state.item_ownership.items;
    ARPG_REQUIRE(maximum_items.capacity()
        == persistence::kMaximumCheckpointItemCount);
    for (std::uint64_t id = 1U;
            id <= persistence::kMaximumCheckpointItemCount; ++id) {
        maximum_items.push_back(normal_item(id));
    }
    maximum_job->checkpoint.state.item_ownership.next_item_sequence =
        persistence::kMaximumCheckpointItemCount + 1U;
    arpg::test::set_persistence_allocation_failure_countdown(0);
    const auto maximum_submission = maximum_worker.submit(maximum_lease);
    persistence::SaveCommitCompletion maximum_completion{};
    const bool maximum_completed =
        wait_completion(maximum_worker, maximum_completion);
    const bool allocation_failure_still_armed =
        arpg::test::persistence_allocation_failure_is_armed();
    arpg::test::set_persistence_allocation_failure_countdown(-1);
    ARPG_REQUIRE(maximum_submission.state
        == persistence::SaveCommitSubmitState::accepted);
    ARPG_REQUIRE(maximum_completed);
    ARPG_REQUIRE(allocation_failure_still_armed);
    ARPG_REQUIRE(maximum_completion.result.state
        == persistence::SaveCommitState::committed);
    maximum_worker.stop_and_join();
    return {};
}

struct BlockingHook final {
    std::atomic<bool> entered{};
    std::atomic<bool> release{};
};

bool block_first_write(persistence::SaveFaultPoint point,
    void* context) noexcept {
    auto& hook = *static_cast<BlockingHook*>(context);
    if (point != persistence::SaveFaultPoint::before_temp_write
            || hook.entered.exchange(true)) return false;
    while (!hook.release.load()) std::this_thread::yield();
    return false;
}

test::Failure one_pending_slot_coalesces_background_for_exact() noexcept {
    TempDirectory directory{};
    BlockingHook hook{};
    std::unique_ptr<persistence::SaveCommitStorage> storage{
        new (std::nothrow) persistence::SaveCommitStorage{}};
    ARPG_REQUIRE(storage != nullptr);
    persistence::SaveStoreConfig config{directory.path};
    config.fault_hook = &block_first_write;
    config.fault_context = &hook;
    ARPG_REQUIRE(storage->initialize(config));
    persistence::SaveCommitWorker worker{*storage};
    ARPG_REQUIRE(worker.start());
    const auto first_lease = worker.acquire_capture_slot(20U,
        persistence::SaveCommitRequestKind::background);
    auto* first_job = worker.capture_job(first_lease);
    ARPG_REQUIRE(first_job != nullptr);
    ARPG_REQUIRE(fixture(first_job->checkpoint, 20U));
    ARPG_REQUIRE(worker.submit(first_lease).state
        == persistence::SaveCommitSubmitState::accepted);
    const auto deadline = std::chrono::steady_clock::now()
        + std::chrono::seconds{10};
    while (!hook.entered.load()
            && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::yield();
    }
    ARPG_REQUIRE(hook.entered.load());
    const auto queued_lease = worker.acquire_capture_slot(21U,
        persistence::SaveCommitRequestKind::background);
    auto* queued_job = worker.capture_job(queued_lease);
    ARPG_REQUIRE(queued_job != nullptr);
    ARPG_REQUIRE(fixture(queued_job->checkpoint, 21U));
    ARPG_REQUIRE(worker.submit(queued_lease).state
        == persistence::SaveCommitSubmitState::accepted);
    const auto exact_lease = worker.acquire_capture_slot(22U,
        persistence::SaveCommitRequestKind::exact, 202U);
    ARPG_REQUIRE(exact_lease.state
        == persistence::SaveCommitSubmitState::superseded_background);
    ARPG_REQUIRE(exact_lease.job_slot == queued_lease.job_slot);
    auto* exact_job = worker.capture_job(exact_lease);
    ARPG_REQUIRE(exact_job != nullptr);
    ARPG_REQUIRE(fixture(exact_job->checkpoint, 22U));
    ARPG_REQUIRE(worker.submit(exact_lease).state
        == persistence::SaveCommitSubmitState::superseded_background);
    const auto overwrite_attempt = worker.acquire_capture_slot(23U,
        persistence::SaveCommitRequestKind::exact, 303U);
    ARPG_REQUIRE(overwrite_attempt.state
        == persistence::SaveCommitSubmitState::busy);
    ARPG_REQUIRE(worker.capture_job(first_lease) == nullptr);
    hook.release = true;
    persistence::SaveCommitCompletion first{};
    persistence::SaveCommitCompletion second{};
    ARPG_REQUIRE(wait_completion(worker, first));
    ARPG_REQUIRE(wait_completion(worker, second));
    ARPG_REQUIRE(first.revision == 20U);
    ARPG_REQUIRE(second.revision == 22U);
    ARPG_REQUIRE(second.kind == persistence::SaveCommitRequestKind::exact);
    ARPG_REQUIRE(second.intent == 202U);
    ARPG_REQUIRE(second.result.state
        == persistence::SaveCommitState::committed);
    ARPG_REQUIRE(!worker.try_take_completion(second));

    std::uint64_t previous_token = exact_lease.token;
    for (std::uint64_t revision = 23U; revision < 39U; ++revision) {
        const auto reuse = worker.acquire_capture_slot(revision,
            persistence::SaveCommitRequestKind::exact, revision + 1000U);
        ARPG_REQUIRE(reuse.state
            == persistence::SaveCommitSubmitState::accepted);
        ARPG_REQUIRE(reuse.token != 0U && reuse.token != previous_token);
        previous_token = reuse.token;
        auto* const reuse_job = worker.capture_job(reuse);
        ARPG_REQUIRE(reuse_job != nullptr);
        ARPG_REQUIRE(fixture(reuse_job->checkpoint, revision));
        ARPG_REQUIRE(worker.submit(reuse).state
            == persistence::SaveCommitSubmitState::accepted);
        persistence::SaveCommitCompletion reused{};
        ARPG_REQUIRE(wait_completion(worker, reused));
        ARPG_REQUIRE(reused.revision == revision);
        ARPG_REQUIRE(reused.result.state
            == persistence::SaveCommitState::committed);
        ARPG_REQUIRE(!worker.try_take_completion(reused));
    }
    const auto stale = worker.acquire_capture_slot(37U,
        persistence::SaveCommitRequestKind::exact, 400U);
    auto* const stale_job = worker.capture_job(stale);
    ARPG_REQUIRE(stale_job != nullptr);
    ARPG_REQUIRE(fixture(stale_job->checkpoint, 37U));
    ARPG_REQUIRE(worker.submit(stale).state
        == persistence::SaveCommitSubmitState::accepted);
    persistence::SaveCommitCompletion stale_completion{};
    ARPG_REQUIRE(wait_completion(worker, stale_completion));
    ARPG_REQUIRE(stale_completion.result.state
        == persistence::SaveCommitState::not_committed);
    ARPG_REQUIRE(stale_completion.result.error
        == persistence::SaveError::invalid_checkpoint);

    const auto equal = worker.acquire_capture_slot(38U,
        persistence::SaveCommitRequestKind::exact, 401U);
    auto* const equal_job = worker.capture_job(equal);
    ARPG_REQUIRE(equal_job != nullptr);
    ARPG_REQUIRE(fixture(equal_job->checkpoint, 38U));
    ARPG_REQUIRE(worker.submit(equal).state
        == persistence::SaveCommitSubmitState::accepted);
    persistence::SaveCommitCompletion equal_completion{};
    ARPG_REQUIRE(wait_completion(worker, equal_completion));
    ARPG_REQUIRE(equal_completion.result.state
        == persistence::SaveCommitState::committed);

    hook.entered = false;
    hook.release = false;
    const auto stop_lease = worker.acquire_capture_slot(39U,
        persistence::SaveCommitRequestKind::exact, 499U);
    auto* const stop_job = worker.capture_job(stop_lease);
    ARPG_REQUIRE(stop_job != nullptr);
    ARPG_REQUIRE(fixture(stop_job->checkpoint, 39U));
    ARPG_REQUIRE(worker.submit(stop_lease).state
        == persistence::SaveCommitSubmitState::accepted);
    const auto stop_deadline = std::chrono::steady_clock::now()
        + std::chrono::seconds{10};
    while (!hook.entered.load()
            && std::chrono::steady_clock::now() < stop_deadline) {
        std::this_thread::yield();
    }
    ARPG_REQUIRE(hook.entered.load());
    std::thread stopper{[&worker] { worker.stop_and_join(); }};
    hook.release = true;
    stopper.join();
    persistence::SaveCommitCompletion suppressed{};
    ARPG_REQUIRE(!worker.try_take_completion(suppressed));
    ARPG_REQUIRE(worker.start());

    const auto maximum = worker.acquire_capture_slot(
        (std::numeric_limits<std::uint64_t>::max)(),
        persistence::SaveCommitRequestKind::exact, 402U);
    auto* const maximum_job = worker.capture_job(maximum);
    ARPG_REQUIRE(maximum_job != nullptr);
    ARPG_REQUIRE(fixture(maximum_job->checkpoint,
        (std::numeric_limits<std::uint64_t>::max)()));
    ARPG_REQUIRE(worker.submit(maximum).state
        == persistence::SaveCommitSubmitState::accepted);
    persistence::SaveCommitCompletion maximum_completion{};
    ARPG_REQUIRE(wait_completion(worker, maximum_completion));
    ARPG_REQUIRE(maximum_completion.result.state
        == persistence::SaveCommitState::committed);
    const std::uint64_t prior_epoch = maximum_completion.epoch;
    worker.stop_and_join();
    ARPG_REQUIRE(worker.start());
    const auto restarted = worker.acquire_capture_slot(
        (std::numeric_limits<std::uint64_t>::max)(),
        persistence::SaveCommitRequestKind::exact, 403U);
    ARPG_REQUIRE(restarted.epoch != prior_epoch);
    ARPG_REQUIRE(worker.capture_job(maximum) == nullptr);
    auto* const restarted_job = worker.capture_job(restarted);
    ARPG_REQUIRE(restarted_job != nullptr);
    ARPG_REQUIRE(fixture(restarted_job->checkpoint,
        (std::numeric_limits<std::uint64_t>::max)()));
    ARPG_REQUIRE(worker.submit(restarted).state
        == persistence::SaveCommitSubmitState::accepted);
    persistence::SaveCommitCompletion restarted_completion{};
    ARPG_REQUIRE(wait_completion(worker, restarted_completion));
    ARPG_REQUIRE(restarted_completion.result.state
        == persistence::SaveCommitState::committed);
    worker.stop_and_join();
    return {};
}

test::Failure equal_revision_conflict_archives_both_slots() noexcept {
    TempDirectory directory{};
    std::unique_ptr<dungeon::checkpoint::SaveCheckpointSlot> first{
        new (std::nothrow) dungeon::checkpoint::SaveCheckpointSlot{}};
    std::unique_ptr<dungeon::checkpoint::SaveCheckpointSlot> second{
        new (std::nothrow) dungeon::checkpoint::SaveCheckpointSlot{}};
    ARPG_REQUIRE(first != nullptr && second != nullptr);
    ARPG_REQUIRE(fixture(*first, 41U));
    ARPG_REQUIRE(fixture(*second, 41U));
    second->state.root_seed ^= 0x55U;
    ARPG_REQUIRE(write_v9(directory.path / "run_a.sav", *first));
    ARPG_REQUIRE(write_v9(directory.path / "run_b.sav", *second));

    std::unique_ptr<persistence::SaveCommitStorage> storage{
        new (std::nothrow) persistence::SaveCommitStorage{}};
    ARPG_REQUIRE(storage != nullptr);
    ARPG_REQUIRE(storage->initialize({directory.path}));
    ARPG_REQUIRE(storage->load_state()
        == persistence::SaveLoadState::recovery_required);
    persistence::SaveCommitWorker blocked_worker{*storage};
    ARPG_REQUIRE(!blocked_worker.start());
    ARPG_REQUIRE(storage->archive_invalid_files());
    ARPG_REQUIRE(storage->load_state() == persistence::SaveLoadState::empty);
    ARPG_REQUIRE(!std::filesystem::exists(directory.path / "run_a.sav"));
    ARPG_REQUIRE(!std::filesystem::exists(directory.path / "run_b.sav"));
    std::size_t archived{};
    for (const auto& entry : std::filesystem::directory_iterator(directory.path)) {
        if (entry.path().filename().string().find(".corrupt.")
                != std::string::npos) ++archived;
    }
    ARPG_REQUIRE(archived == 2U);

    {
        TempDirectory corrupt_high_directory{};
        std::unique_ptr<dungeon::checkpoint::SaveCheckpointSlot> valid{
            new (std::nothrow) dungeon::checkpoint::SaveCheckpointSlot{}};
        ARPG_REQUIRE(valid != nullptr);
        ARPG_REQUIRE(fixture(*valid, 50U));
        ARPG_REQUIRE(write_v9(
            corrupt_high_directory.path / "run_a.sav", *valid));
        std::vector<std::uint8_t> corrupt(
            persistence::kMaximumEncodedCheckpointBytes);
        std::size_t corrupt_size{};
        ARPG_REQUIRE(persistence::encode_checkpoint_v9_into(*valid,
            corrupt.data(), corrupt.size(), corrupt_size)
            == persistence::CodecError::none);
        corrupt.resize(corrupt_size);
        for (std::size_t byte = 0U; byte < 8U; ++byte) {
            corrupt[16U + byte] = 0xFFU;
        }
        ARPG_REQUIRE(write_bytes(
            corrupt_high_directory.path / "run_b.sav", corrupt));
        std::unique_ptr<persistence::SaveCommitStorage> corrupt_storage{
            new (std::nothrow) persistence::SaveCommitStorage{}};
        ARPG_REQUIRE(corrupt_storage != nullptr);
        ARPG_REQUIRE(corrupt_storage->initialize(
            {corrupt_high_directory.path}));
        ARPG_REQUIRE(corrupt_storage->load_state()
            == persistence::SaveLoadState::ready);
        ARPG_REQUIRE(corrupt_storage->loaded_slot()
            == persistence::SaveSlot::a);
        ARPG_REQUIRE(corrupt_storage->loaded_recovered());
    }
    {
        TempDirectory legacy_directory{};
        auto legacy_state = first->state;
        legacy_state.last_abyss_resolution.lifecycle =
            abyss::AbyssLifecycle::none;
        const auto legacy = persistence::encode_checkpoint(legacy_state);
        ARPG_REQUIRE(legacy.has_value());
        ARPG_REQUIRE(write_bytes(
            legacy_directory.path / "run_a.sav", *legacy));
        std::unique_ptr<persistence::SaveCommitStorage> legacy_storage{
            new (std::nothrow) persistence::SaveCommitStorage{}};
        ARPG_REQUIRE(legacy_storage != nullptr);
        ARPG_REQUIRE(legacy_storage->initialize({legacy_directory.path}));
        ARPG_REQUIRE(legacy_storage->load_state()
            == persistence::SaveLoadState::ready);
        ARPG_REQUIRE(legacy_storage->loaded_migrated());
        ARPG_REQUIRE(legacy_storage->loaded_format()
            == persistence::kCheckpointFormatVersion);
    }
    {
        TempDirectory oversized_directory{};
        std::vector<std::uint8_t> oversized(
            persistence::kMaximumEncodedCheckpointBytes + 1U, 0xA5U);
        ARPG_REQUIRE(write_bytes(
            oversized_directory.path / "run_a.sav", oversized));
        std::unique_ptr<persistence::SaveCommitStorage> oversized_storage{
            new (std::nothrow) persistence::SaveCommitStorage{}};
        ARPG_REQUIRE(oversized_storage != nullptr);
        ARPG_REQUIRE(oversized_storage->initialize(
            {oversized_directory.path}));
        ARPG_REQUIRE(oversized_storage->load_state()
            == persistence::SaveLoadState::recovery_required);
        persistence::SaveCommitWorker oversized_worker{*oversized_storage};
        ARPG_REQUIRE(!oversized_worker.start());
    }
    {
        TempDirectory unavailable_directory{};
        ARPG_REQUIRE(std::filesystem::create_directory(
            unavailable_directory.path / "run_a.sav"));
        std::unique_ptr<persistence::SaveCommitStorage> unavailable_storage{
            new (std::nothrow) persistence::SaveCommitStorage{}};
        ARPG_REQUIRE(unavailable_storage != nullptr);
        ARPG_REQUIRE(unavailable_storage->initialize(
            {unavailable_directory.path}));
        ARPG_REQUIRE(unavailable_storage->load_state()
            == persistence::SaveLoadState::blocked);
        persistence::SaveCommitWorker unavailable_worker{
            *unavailable_storage};
        ARPG_REQUIRE(!unavailable_worker.start());
    }
    {
        TempDirectory drift_directory{};
        std::unique_ptr<dungeon::checkpoint::SaveCheckpointSlot> current{
            new (std::nothrow) dungeon::checkpoint::SaveCheckpointSlot{}};
        std::unique_ptr<dungeon::checkpoint::SaveCheckpointSlot> older{
            new (std::nothrow) dungeon::checkpoint::SaveCheckpointSlot{}};
        std::unique_ptr<dungeon::checkpoint::SaveCheckpointSlot> injected{
            new (std::nothrow) dungeon::checkpoint::SaveCheckpointSlot{}};
        ARPG_REQUIRE(current && older && injected);
        ARPG_REQUIRE(fixture(*current, 70U));
        ARPG_REQUIRE(fixture(*older, 69U));
        ARPG_REQUIRE(fixture(*injected, 71U));
        ARPG_REQUIRE(write_v9(drift_directory.path / "run_a.sav", *current));
        ARPG_REQUIRE(write_v9(drift_directory.path / "run_b.sav", *older));
        std::unique_ptr<persistence::SaveCommitStorage> drift_storage{
            new (std::nothrow) persistence::SaveCommitStorage{}};
        ARPG_REQUIRE(drift_storage != nullptr);
        ARPG_REQUIRE(drift_storage->initialize({drift_directory.path}));
        drift_storage->release_loaded_checkpoints();
        persistence::SaveCommitWorker drift_worker{*drift_storage};
        ARPG_REQUIRE(drift_worker.start());
        // Simulate a concurrent writer replacing the other slot after init.
        ARPG_REQUIRE(write_v9(
            drift_directory.path / "run_b.sav", *injected));
        const auto drift_lease = drift_worker.acquire_capture_slot(70U,
            persistence::SaveCommitRequestKind::exact, 700U);
        auto* const drift_job = drift_worker.capture_job(drift_lease);
        ARPG_REQUIRE(drift_job != nullptr);
        ARPG_REQUIRE(fixture(drift_job->checkpoint, 70U));
        ARPG_REQUIRE(drift_worker.submit(drift_lease).state
            == persistence::SaveCommitSubmitState::accepted);
        persistence::SaveCommitCompletion drift_completion{};
        ARPG_REQUIRE(wait_completion(drift_worker, drift_completion));
        ARPG_REQUIRE(drift_completion.result.state
            != persistence::SaveCommitState::committed);
        ARPG_REQUIRE(drift_completion.result.error
            == persistence::SaveError::invalid_checkpoint);
        drift_worker.stop_and_join();
    }
    {
        TempDirectory equal_scan_directory{};
        ARPG_REQUIRE(write_v9(
            equal_scan_directory.path / "run_a.sav", *first));
        SingleFaultHook final_scan_fault{
            persistence::SaveFaultPoint::final_scan_b};
        persistence::SaveStoreConfig scan_config{equal_scan_directory.path};
        scan_config.fault_hook = &fail_single_worker_point;
        scan_config.fault_context = &final_scan_fault;
        std::unique_ptr<persistence::SaveCommitStorage> scan_storage{
            new (std::nothrow) persistence::SaveCommitStorage{}};
        ARPG_REQUIRE(scan_storage != nullptr);
        ARPG_REQUIRE(scan_storage->initialize(scan_config));
        scan_storage->release_loaded_checkpoints();
        persistence::SaveCommitWorker scan_worker{*scan_storage};
        ARPG_REQUIRE(scan_worker.start());
        const auto scan_lease = scan_worker.acquire_capture_slot(41U,
            persistence::SaveCommitRequestKind::exact, 410U);
        auto* const scan_job = scan_worker.capture_job(scan_lease);
        ARPG_REQUIRE(scan_job != nullptr);
        ARPG_REQUIRE(fixture(scan_job->checkpoint, 41U));
        ARPG_REQUIRE(scan_worker.submit(scan_lease).state
            == persistence::SaveCommitSubmitState::accepted);
        persistence::SaveCommitCompletion scan_completion{};
        ARPG_REQUIRE(wait_completion(scan_worker, scan_completion));
        ARPG_REQUIRE(scan_completion.result.state
            == persistence::SaveCommitState::indeterminate);
        ARPG_REQUIRE(scan_completion.result.error
            == persistence::SaveError::final_scan_failed);
        scan_worker.stop_and_join();
    }
    return {};
}

constexpr test::TestCase kCases[] = {
    {"worker exact v9 commit", &worker_commits_v9_and_readback_verifies_exact_bytes},
    {"worker pending coalescing", &one_pending_slot_coalesces_background_for_exact},
    {"worker equal revision conflict archive",
        &equal_revision_conflict_archives_both_slots},
};

}  // namespace

arpg::test::TestSuite save_commit_worker_suite() noexcept {
    return arpg::test::make_suite("save_commit_worker", kCases);
}
