#pragma once

#include "dungeon/dungeon_progression.hpp"
#include "dungeon/dungeon_session.hpp"
#include "dungeon_view_math.hpp"
#include "persistence/save_store.hpp"
#include "persistence/save_commit_worker.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <memory>

namespace arpg::test {
struct DungeonRuntimeTestAccess;
}

namespace arpg::platform {

enum class DungeonRuntimeState : std::uint8_t {
    uninitialized,
    running,
    recovery_required,
    faulted,
};

enum class CleanShutdownState : std::uint8_t {
    idle,
    closing,
    ready,
    canceled,
    faulted,
};

using RootSeedProvider = std::optional<std::uint64_t> (*)(void* context) noexcept;

struct DungeonRuntimeConfig final {
    dungeon::DungeonRules rules{};
    persistence::SaveStoreConfig save{};
    std::optional<std::uint64_t> new_run_seed{};
    RootSeedProvider seed_provider{};
    void* seed_context{};
    bool continue_pending_death_on_initialize{};
};

struct LootPickupReceipt final {
    bool valid{};
    std::uint64_t commit_generation{};
    std::uint64_t item_id{};
    std::uint8_t base_id{};
    std::uint8_t item_level{};
    items::ItemRarity rarity{items::ItemRarity::normal};
    dungeon::GroundItemSource source{dungeon::GroundItemSource::monster_drop};
};

struct DungeonRenderStatus final {
    SaveIndicator indicator{SaveIndicator::none};
    persistence::SaveSlot active_slot{persistence::SaveSlot::none};
    persistence::SaveError error{persistence::SaveError::none};
    bool recovery_required{};
    bool faulted{};
    LootPickupReceipt loot_pickup{};
};

class DungeonRuntime final {
public:
    explicit DungeonRuntime(DungeonRuntimeConfig config);
    ~DungeonRuntime();

    [[nodiscard]] bool initialize() noexcept;
    [[nodiscard]] DungeonRuntimeState state() const noexcept;
    [[nodiscard]] dungeon::DungeonSession* session() noexcept;
    [[nodiscard]] const dungeon::DungeonSession* session() const noexcept;
    [[nodiscard]] dungeon::RequestResult request_pickup(
        std::uint16_t drop_ordinal) noexcept;
    [[nodiscard]] dungeon::RequestResult request_equip(
        std::uint64_t item_id) noexcept;
    [[nodiscard]] dungeon::RequestResult request_unequip(
        items::ItemSlot slot) noexcept;
    [[nodiscard]] dungeon::RequestResult request_craft(
        items::MaterialId material, std::uint64_t item_id,
        std::optional<items::DirectedCategory> directed_category =
            std::nullopt) noexcept;
    [[nodiscard]] dungeon::RequestResult request_recipe(
        const std::array<std::uint64_t, 3>& item_ids) noexcept;
    [[nodiscard]] dungeon::RequestResult request_reinforcement(
        std::uint64_t item_id) noexcept;
    [[nodiscard]] dungeon::RequestResult request_coupon(
        items::MaterialId coupon, std::uint64_t item_id) noexcept;
    [[nodiscard]] dungeon::RequestResult request_death_continue() noexcept;
    [[nodiscard]] const items::ItemOwnershipState* item_state() const noexcept;
    [[nodiscard]] DungeonRenderStatus render_status() const noexcept;
    void fixed_tick(combat::MovementInput movement,
        dungeon::AutoPickupPolicy pickup_policy = {}) noexcept;
    void service_pending_save() noexcept;
    void pump_persistence_frame() noexcept;
    [[nodiscard]] bool request_clean_shutdown() noexcept;
    [[nodiscard]] CleanShutdownState clean_shutdown_state() const noexcept;
    [[nodiscard]] bool gameplay_rearm_required() const noexcept;
    [[nodiscard]] bool authority_requests_enabled() const noexcept;
    void acknowledge_gameplay_rearmed() noexcept;
    [[nodiscard]] bool recover_with_new_run() noexcept;

private:
    friend struct ::arpg::test::DungeonRuntimeTestAccess;
    [[nodiscard]] std::optional<std::uint64_t> select_new_run_seed() const noexcept;
    [[nodiscard]] bool repair_pending_death_target(
        dungeon::DungeonRunState& checkpoint) const noexcept;
    [[nodiscard]] bool continue_pending_death_on_initialize() noexcept;
    [[nodiscard]] bool start_worker() noexcept;
    [[nodiscard]] bool commit_initial_checkpoint() noexcept;
    void poll_save_completion() noexcept;
    void apply_save_completion(
        const persistence::SaveCommitCompletion& completion) noexcept;
    void submit_pending_exact() noexcept;
    void submit_shutdown_exact() noexcept;
    void submit_background_if_due() noexcept;
    void fault_persistence_runtime(persistence::SaveError error) noexcept;

    DungeonRuntimeConfig config_{};
    std::unique_ptr<persistence::SaveCommitStorage> save_storage_{};
    std::unique_ptr<persistence::SaveCommitWorker> save_worker_{};
    std::unique_ptr<dungeon::DungeonSession> session_{};
    struct ExactFlight final {
        bool active{};
        std::uint64_t revision{};
        std::uint64_t intent{};
        std::uint64_t token{};
        std::uint64_t epoch{};
        std::uint64_t expected_generation{};
        dungeon::PendingSaveKind kind{dungeon::PendingSaveKind::transition};
        std::optional<LootPickupReceipt> pickup{};
        std::uint16_t pickup_ordinal{0xFFFFU};
        bool shutdown{};
        bool recovery_initial{};
    } exact_flight_{};
    std::uint64_t authority_revision_{};
    std::uint64_t durable_revision_{};
    struct BackgroundFlight final {
        bool active{};
        std::uint64_t revision{};
        std::uint64_t token{};
        std::uint64_t epoch{};
    } background_flight_{};
    std::uint64_t fixed_tick_count_{};
    std::uint64_t next_background_tick_{300U};
    bool background_due_{};
    bool progress_dirty_{};
    bool gameplay_rearm_required_{};
    CleanShutdownState clean_shutdown_state_{CleanShutdownState::idle};
    DungeonRuntimeState state_{DungeonRuntimeState::uninitialized};
    DungeonRenderStatus status_{};
};

}  // namespace arpg::platform
