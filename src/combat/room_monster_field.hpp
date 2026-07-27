#pragma once

#include "combat/monster_persistent_state.hpp"
#include "combat/room_monster_plan.hpp"
#include "combat/room_spatial_grid.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace arpg::combat {

enum class RoomMonsterFieldFault : std::uint8_t {
    none,
    invalid_plan,
    monster_residency_capacity,
    active_pool_failure,
    invalid_ordinal,
};

struct RoomResidentOrdinals final {
    std::array<MonsterOrdinal, kMonsterCapacity> ordinals{};
    std::uint16_t count{};
    RoomMonsterFieldFault fault{RoomMonsterFieldFault::none};
};

class RoomMonsterField final {
public:
    RoomMonsterField() noexcept;
    explicit RoomMonsterField(MonsterPool& active_pool) noexcept;
    RoomMonsterField(const RoomMonsterField&) = delete;
    RoomMonsterField& operator=(const RoomMonsterField&) = delete;
    RoomMonsterField(RoomMonsterField&&) = delete;
    RoomMonsterField& operator=(RoomMonsterField&&) = delete;

    [[nodiscard]] RoomMonsterPlan& plan_storage_for_construction() noexcept;
    [[nodiscard]] RoomMonsterFieldFault seal_plan(
        std::uint16_t expected_count) noexcept;

    template <typename BuildResult>
    [[nodiscard]] RoomMonsterFieldFault seal_plan(
        const BuildResult& result) noexcept {
        if (static_cast<std::uint32_t>(result.fault) != 0U) {
            fault_ = RoomMonsterFieldFault::invalid_plan;
            return fault_;
        }
        return seal_plan(static_cast<std::uint16_t>(
            result.density.total_count));
    }

    [[nodiscard]] RoomResidentOrdinals required_residents(
        RoomStreamingRegion region) const noexcept;
    [[nodiscard]] bool synchronize_active_region(
        RoomStreamingRegion region) noexcept;
    [[nodiscard]] RoomMonsterFieldFault apply_abyss_config_for_construction(
        const abyss::AbyssCombatConfig& config) noexcept;
    void write_back_active() noexcept;
    [[nodiscard]] bool mark_defeated(MonsterOrdinal ordinal) noexcept;

    [[nodiscard]] std::uint32_t total_count() const noexcept;
    [[nodiscard]] std::uint32_t living_count() const noexcept;
    [[nodiscard]] std::uint32_t defeated_count() const noexcept;
    [[nodiscard]] RoomMonsterFieldFault fault() const noexcept;
    [[nodiscard]] const RoomMonsterPlan& plan() const noexcept;
    [[nodiscard]] const MonsterPersistentState* persistent_state(
        MonsterOrdinal ordinal) const noexcept;
    [[nodiscard]] MonsterPersistentState* persistent_state(
        MonsterOrdinal ordinal) noexcept;
    [[nodiscard]] MonsterRuntime* active_runtime(
        MonsterOrdinal ordinal) noexcept;
    [[nodiscard]] const MonsterRuntime* active_runtime(
        MonsterOrdinal ordinal) const noexcept;
    [[nodiscard]] modifiers::EffectSet* active_effects(
        MonsterOrdinal ordinal) noexcept;
    [[nodiscard]] const modifiers::EffectSet* active_effects(
        MonsterOrdinal ordinal) const noexcept;
    [[nodiscard]] std::optional<MonsterHandle> resident_handle(
        MonsterOrdinal ordinal) const noexcept;
    [[nodiscard]] RoomResidentOrdinals resident_ordinals() const noexcept;
    [[nodiscard]] bool clamp_to_home_leash(
        MonsterOrdinal ordinal, Vec3& position) const noexcept;
    void rebind_active_pool(MonsterPool& active_pool) noexcept;
    [[nodiscard]] MonsterPool& active_pool() noexcept;
    [[nodiscard]] const MonsterPool& active_pool() const noexcept;

private:
    [[nodiscard]] bool plan_shape_valid(
        std::uint16_t expected_count) const noexcept;
    [[nodiscard]] bool initialize_persistent_states() noexcept;
    void clear_residency() noexcept;

    MonsterPool owned_active_pool_{};
    MonsterPool* active_pool_{&owned_active_pool_};
    RoomMonsterPlan plan_{};
    std::array<MonsterPersistentState,
        limits::kRoomMonsterCapacity> states_{};
    std::array<MonsterHandle,
        limits::kRoomMonsterCapacity> ordinal_to_handle_{};
    RoomResidentOrdinals residents_{};
    RoomStreamingRegion active_region_{};
    abyss::AbyssCombatConfig abyss_config_{};
    std::uint32_t defeated_count_{};
    RoomMonsterFieldFault fault_{RoomMonsterFieldFault::none};
    bool sealed_{};
    bool region_active_{};
};

}  // namespace arpg::combat
