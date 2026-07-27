#pragma once

#include "core/bounded_queue.hpp"
#include "dungeon/encounter_director.hpp"
#include "dungeon/dungeon_types.hpp"
#include "items/item_crafting.hpp"
#include "progression/progression_rules.hpp"

#include "combat/combat_world.hpp"

#include <cstddef>
#include <cstdint>
#include <array>
#include <memory>
#include <optional>
#include <utility>

namespace arpg::test {
struct DungeonSessionTestAccess;
struct DungeonDeathStressFixture;
}

namespace arpg::dungeon {

namespace checkpoint {
struct SaveCheckpointSlot;
}

[[nodiscard]] std::uint16_t affix_drop_chance_bp(
    std::uint16_t score) noexcept;
[[nodiscard]] std::uint8_t affix_item_level(
    std::uint64_t depth, std::uint16_t score) noexcept;
[[nodiscard]] std::uint64_t affix_experience(
    std::uint64_t base_experience, std::uint16_t score) noexcept;
[[nodiscard]] bool item_id_in_use(
    const items::ItemOwnershipState& ownership,
    const std::array<GroundItem, kGroundDropCapacity>& ground_items,
    std::uint64_t item_id,
    std::uint16_t ignored_ground_index = 0xFFFFU) noexcept;

class ReusablePendingSave final {
public:
    [[nodiscard]] bool has_value() const noexcept { return engaged_; }
    [[nodiscard]] PendingSave& operator*() noexcept { return value_; }
    [[nodiscard]] const PendingSave& operator*() const noexcept {
        return value_;
    }
    [[nodiscard]] PendingSave* operator->() noexcept { return &value_; }
    [[nodiscard]] const PendingSave* operator->() const noexcept {
        return &value_;
    }
    void reset() noexcept { engaged_ = false; }
    [[nodiscard]] PendingSave& prepare() noexcept {
        engaged_ = true;
        return value_;
    }
    [[nodiscard]] bool reserve_items(std::size_t count) noexcept {
        try {
            value_.next_state.item_ownership.items.reserve(
                count == 0U ? 1U : count);
            return true;
        } catch (...) {
            return false;
        }
    }
    ReusablePendingSave& operator=(PendingSave&& pending) noexcept {
        value_ = std::move(pending);
        engaged_ = true;
        return *this;
    }
    PendingSave& emplace(PendingSave&& pending) noexcept {
        *this = std::move(pending);
        return value_;
    }
    [[nodiscard]] std::optional<PendingSave> copy() const noexcept {
        if (!engaged_) return std::nullopt;
        try {
            return value_;
        } catch (...) {
            return std::nullopt;
        }
    }

private:
    PendingSave value_{};
    bool engaged_{};
};

class DungeonSession final {
public:
    static constexpr std::size_t kDungeonEventCapacity = 32;
    static constexpr std::size_t kCombatRelayCapacity =
        combat::kCombatEventCapacity;

    explicit DungeonSession(DungeonSessionConfig config = {}) noexcept;
    explicit DungeonSession(
        DungeonRules rules,
        DungeonRunState stable_state) noexcept;
    [[nodiscard]] bool queue_action(combat::Action action) noexcept;
    [[nodiscard]] combat::SkillCastResult request_active_skill_slot(
        std::uint8_t slot) noexcept;
    void tick(
        combat::MovementInput movement,
        AutoPickupPolicy pickup_policy = {}) noexcept;
    [[nodiscard]] bool request_descent(bool player_in_range) noexcept;
    [[nodiscard]] bool request_passive_allocation(
        passives::PassiveNodeId node) noexcept;
    [[nodiscard]] bool request_passive_refund(
        passives::PassiveNodeId node) noexcept;
    [[nodiscard]] RequestResult request_equip(
        std::uint64_t item_id) noexcept;
    [[nodiscard]] RequestResult request_unequip(
        items::ItemSlot slot) noexcept;
    [[nodiscard]] RequestResult request_craft(
        items::MaterialId material,
        std::uint64_t item_id,
        std::optional<items::DirectedCategory> directed_category =
            std::nullopt) noexcept;
    [[nodiscard]] RequestResult request_recipe(
        const std::array<std::uint64_t, 3>& item_ids) noexcept;
    [[nodiscard]] RequestResult request_reinforcement(
        std::uint64_t item_id) noexcept;
    [[nodiscard]] RequestResult request_coupon(
        items::MaterialId coupon, std::uint64_t item_id) noexcept;
    [[nodiscard]] RequestResult request_pickup(
        std::uint16_t drop_ordinal) noexcept;
    [[nodiscard]] RequestResult request_material_pickup(
        std::uint16_t ordinal) noexcept;
    [[nodiscard]] RequestResult request_remove_active_skill(
        std::uint8_t slot) noexcept;
    [[nodiscard]] RequestResult request_equip_active_skill(
        skills::ActiveSkillId skill, std::uint8_t slot) noexcept;
    [[nodiscard]] RequestResult request_swap_active_skill_slots(
        std::uint8_t left, std::uint8_t right) noexcept;
    [[nodiscard]] RequestResult request_death_continue() noexcept;
    void request_nearby_pickups(
        combat::Vec3 player_position,
        AutoPickupPolicy pickup_policy = {}) noexcept;
    [[nodiscard]] const items::ItemOwnershipState& item_state() const noexcept;
    [[nodiscard]] std::optional<combat::PlayerCombatBuild>
    preview_equipment_build(
        const items::EquipmentState& equipment) const noexcept;
    [[nodiscard]] const PendingSave* pending_save_view() const noexcept;
    [[nodiscard]] std::optional<PendingSave> pending_save() const noexcept;
    void resolve_pending_save(const PendingSaveResult& result) noexcept;
    [[nodiscard]] std::optional<PendingTransition>
    pending_transition() const noexcept;
    void resolve_pending_transition(
        const TransitionSaveResult& result) noexcept;
    [[nodiscard]] RequestResult reset_current_room() noexcept;
    [[nodiscard]] RoomPhase phase() const noexcept;
    [[nodiscard]] DungeonSnapshot snapshot() const noexcept;
    void snapshot(DungeonSnapshot& destination) const noexcept;
    [[nodiscard]] bool capture_save_checkpoint(
        checkpoint::SaveCheckpointSlot& destination,
        std::uint64_t persistence_revision,
        const DungeonRunState* durable_override = nullptr) const noexcept;
    void clear_buffered_gameplay_input() noexcept;
    [[nodiscard]] bool restore_room_progress_checkpoint(
        const checkpoint::SaveCheckpointSlot& source) noexcept;
    [[nodiscard]] std::optional<DungeonEvent> try_pop_event() noexcept;
    [[nodiscard]] std::optional<combat::CombatEvent>
    try_pop_combat_event() noexcept;

private:
    friend struct ::arpg::test::DungeonSessionTestAccess;
    friend struct ::arpg::test::DungeonDeathStressFixture;
    enum class PlayerBuildStatus : std::uint8_t {
        valid,
        invalid_state,
        allocation_failure,
    };
    struct PlayerBuildResult final {
        combat::PlayerCombatBuild build{};
        PlayerBuildStatus status{PlayerBuildStatus::invalid_state};
    };
    enum class SkillLoadoutMutation : std::uint8_t {
        remove,
        equip,
        swap,
    };
    struct PendingAbyssReward final {
        std::uint16_t ground_index{0xFFFFU};
        std::uint8_t reward_ordinal{0xFFU};
        GroundItem ground{};
    };
    void construct_current_room() noexcept;
    [[nodiscard]] bool validate_pending_death_state() const noexcept;
    void clear_transient_room_state() noexcept;
    void construct_cleared_abyss_room() noexcept;
    void rebuild_committed_abyss_rewards() noexcept;
    void attempt_abyss_reward_materialization() noexcept;
    void construct_normal_room() noexcept;
    void construct_started_abyss_room() noexcept;
    [[nodiscard]] bool stage_current_room_population(
        combat::CombatEncounterConfig config) noexcept;
    [[nodiscard]] bool activate_staged_room_population(
        bool publish_population_event = true) noexcept;
    void clear_staged_room_population() noexcept;
    void reset_to_normal_room(bool clear_queues) noexcept;
    [[nodiscard]] bool prepare_abyss_start() noexcept;
    [[nodiscard]] RequestResult prepare_abyss_failure() noexcept;
    void relay_combat_defeats() noexcept;
    void relay_combat_events() noexcept;
    void handle_player_defeat() noexcept;
    [[nodiscard]] bool prepare_death_retreat() noexcept;
    [[nodiscard]] bool build_death_continue_next(
        DungeonRunState& next) const noexcept;
    [[nodiscard]] static bool copy_run_state_reusing_items(
        DungeonRunState& destination,
        const DungeonRunState& source) noexcept;
    static void publish_run_state_reusing_items(
        DungeonRunState& destination,
        DungeonRunState& source) noexcept;
    [[nodiscard]] bool claim_defeat_reward(
        const combat::CombatEvent& event) noexcept;
    void roll_ground_drop(const combat::CombatEvent& event) noexcept;
    void roll_ground_materials(const combat::CombatEvent& event) noexcept;
    [[nodiscard]] bool claim_material_roll(std::uint16_t ordinal) noexcept;
    [[nodiscard]] bool place_ground_material(
        std::uint16_t ordinal,
        GroundMaterialSource source,
        combat::Vec3 position,
        items::MaterialId material) noexcept;
    [[nodiscard]] bool place_ground_health_potion(
        std::uint16_t spawn_ordinal, combat::Vec3 position) noexcept;
    [[nodiscard]] bool has_claimable_health_potion() const noexcept;
    [[nodiscard]] bool append_clear_health_potion_claims(
        PendingSave& pending) noexcept;
    [[nodiscard]] bool materialize_abyss_clear_materials() noexcept;
    void vacuum_room_materials() noexcept;
    [[nodiscard]] bool has_ground_materials() const noexcept;
    void prepare_room_clear() noexcept;
    void publish_room_clear() noexcept;
    void settle_room_experience() noexcept;
    [[nodiscard]] bool can_emit(std::size_t count) const noexcept;
    void attempt_exit(ExitDirection direction) noexcept;
    [[nodiscard]] bool prepare_transition(
        TransitionKind kind,
        ExitDirection direction,
        bool abandon_abyss = false) noexcept;
    [[nodiscard]] bool confirm_abyss_exit(
        TransitionKind kind, ExitDirection direction) noexcept;
    [[nodiscard]] bool finalize_abyss_exit(
        DungeonRunState& next, bool abandon) const noexcept;
    void clear_abyss_exit_confirmation() noexcept;
    void update_abyss_exit_confirmation_range(
        combat::Vec3 player_position) noexcept;
    [[nodiscard]] std::uint8_t abyss_pending_reward_count() const noexcept;
    [[nodiscard]] std::uint8_t abyss_unpicked_reward_count() const noexcept;
    [[nodiscard]] bool prepare_passive_mutation(
        passives::PassiveNodeId node, bool refund) noexcept;
    [[nodiscard]] RequestResult prepare_item_save(
        DungeonRunState&& next,
        PendingSaveKind kind,
        RoomPhase resume_phase,
        std::optional<ReinforcementReceipt> reinforcement_receipt =
            std::nullopt) noexcept;
    [[nodiscard]] RequestResult request_skill_loadout_mutation(
        SkillLoadoutMutation mutation,
        skills::ActiveSkillId skill,
        std::uint8_t left,
        std::uint8_t right) noexcept;
    [[nodiscard]] RequestResult request_health_potion_pickup(
        std::uint16_t spawn_ordinal) noexcept;
    [[nodiscard]] bool health_potion_abyss_clear_retry_gate_active()
        const noexcept;
    [[nodiscard]] bool pending_health_potion_cache_consistent() const noexcept;
    void apply_committed_health_potions(
        const PendingHealthPotionClaim& claim, bool room_clear) noexcept;
    [[nodiscard]] PlayerBuildResult build_for(
        const checkpoint::DungeonRunState& state,
        const items::EquipmentState* equipment_override = nullptr) const noexcept;
    [[nodiscard]] bool pending_item_cache_consistent() const noexcept;
    [[nodiscard]] bool pending_abyss_cache_consistent() const noexcept;
    [[nodiscard]] bool pending_abyss_reward_cache_consistent() const noexcept;
    [[nodiscard]] bool pending_abyss_claim_cache_consistent() const noexcept;
    [[nodiscard]] bool pending_death_cache_consistent() const noexcept;
    [[nodiscard]] bool pending_material_cache_consistent() const noexcept;
    void commit_pending_save(const PendingSaveResult& result) noexcept;
    void build_dungeon_snapshot(DungeonSnapshot& destination) const noexcept;
    void enter_fault(DungeonFault fault) noexcept;
    void emit_committed(
        const checkpoint::RoomDescriptor& previous_room,
        const DungeonRunState& current) noexcept;
    bool emit(
        DungeonEventKind kind,
        const DungeonRunState* subject = nullptr,
        const DungeonRunState* destination = nullptr,
        TransitionKind transition = TransitionKind::none,
        ExitDirection direction = ExitDirection::none) noexcept;
    [[nodiscard]] std::uint32_t remaining_targets() const noexcept;

    DungeonRules rules_{};
    DungeonRunState stable_state_{};
    ReusablePendingSave pending_save_{};
    mutable DungeonRunState death_validation_scratch_{};
    std::optional<combat::PlayerCombatBuild> pending_item_build_{};
    std::optional<combat::CombatEncounterConfig> pending_abyss_combat_{};
    std::optional<combat::CombatEncounterConfig> staged_room_combat_{};
    std::unique_ptr<combat::RoomMonsterField> staged_room_monster_field_{};
    std::unique_ptr<combat::RoomEnvironmentBlueprint>
        staged_room_environment_{};
    RoomProgressState staged_room_progress_{};
    std::optional<PendingAbyssReward> pending_abyss_reward_{};
    AbyssExitConfirmation abyss_exit_confirmation_{};
    std::unique_ptr<combat::RoomEnvironmentBlueprint> room_environment_{};
    std::optional<combat::CombatWorld> combat_{};
    std::array<GroundItem, kGroundDropCapacity> ground_items_{};
    std::array<std::uint64_t, 3> rolled_drop_bits_{};
    std::array<GroundMaterial, kGroundMaterialCapacity> ground_materials_{};
    std::array<std::uint64_t, kMaterialDropBitWordCount>
        rolled_material_bits_{};
    std::array<GroundHealthPotion, kGroundHealthPotionCapacity>
        ground_health_potions_{};
    HealthPotionPickupReceipt health_potion_pickup_receipt_{};
    bool retry_health_potion_abyss_clear_before_combat_{};
    MaterialPickupReceipt material_pickup_receipt_{};
    ReinforcementReceipt reinforcement_receipt_{};
    RoomEncounterPlan encounter_plan_{};
    std::uint8_t wave_index_{};
    std::uint16_t wave_delay_ticks_{};
    RoomProgressState room_progress_{};
    core::BoundedQueue<DungeonEvent, kDungeonEventCapacity> events_{};
    core::BoundedQueue<combat::CombatEvent, kCombatRelayCapacity>
        combat_events_{};
    RoomPhase phase_{RoomPhase::locked};
    ExitDirection last_exit_{ExitDirection::none};
    std::uint64_t session_tick_{};
    DungeonDiagnostics diagnostics_{};
    progression::ProgressionRules progression_rules_{
        progression::default_progression_rules()};
    progression::ProgressionState room_progression_{};
    std::uint64_t pending_room_experience_{};
    std::uint64_t last_room_experience_{};
    std::uint8_t last_levels_gained_{};
    bool death_detected_emitted_{};
    bool death_continue_failed_{};
    passives::PassiveTreeError last_passive_tree_error_{
        passives::PassiveTreeError::none};
};

}  // namespace arpg::dungeon
