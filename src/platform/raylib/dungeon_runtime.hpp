#pragma once

#include "dungeon/dungeon_progression.hpp"
#include "dungeon/dungeon_session.hpp"
#include "dungeon_view_math.hpp"
#include "persistence/save_store.hpp"

#include <array>
#include <cstdint>
#include <optional>

namespace arpg::platform {

enum class DungeonRuntimeState : std::uint8_t {
    uninitialized,
    running,
    recovery_required,
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
    // Kept until the host is migrated to the generic pending-save entry point.
    void service_pending_transition() noexcept;
    [[nodiscard]] bool recover_with_new_run() noexcept;

private:
    [[nodiscard]] std::optional<std::uint64_t> select_new_run_seed() const noexcept;
    [[nodiscard]] bool repair_pending_death_target(
        dungeon::DungeonRunState& checkpoint) const noexcept;
    [[nodiscard]] bool continue_pending_death_on_initialize() noexcept;
    void sync_load_status(const persistence::SaveLoadResult& result) noexcept;
    void sync_commit_status(const persistence::SaveCommitResult& result) noexcept;
    [[nodiscard]] bool commit_and_resolve_pending(
        const dungeon::PendingSave&,
        dungeon::PendingSaveKind,
        std::uint64_t expected_generation) noexcept;
    [[nodiscard]] static dungeon::PendingSaveResult to_session_result(
        persistence::SaveCommitResult&& saved) noexcept;

    DungeonRuntimeConfig config_{};
    persistence::SaveStore store_;
    std::optional<dungeon::DungeonSession> session_{};
    DungeonRuntimeState state_{DungeonRuntimeState::uninitialized};
    DungeonRenderStatus status_{};
};

}  // namespace arpg::platform
