#pragma once

#include "combat/combat_types.hpp"
#include "combat/room_monster_plan.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace arpg::dungeon {

inline constexpr std::size_t kRoomDropCellCount = 400U;
inline constexpr std::size_t kRoomDropCellCapacity = 9U;
inline constexpr std::size_t kRoomDropReserveCapacity = 16U;
inline constexpr std::size_t kRoomDropQueryCandidateCapacity =
    35U * kRoomDropCellCapacity + kRoomDropReserveCapacity;

enum class RoomDropKind : std::uint8_t {
    equipment,
    material,
    health_potion,
};

enum class RoomDropIndexFault : std::uint8_t {
    none,
    invalid_plan,
    invalid_ordinal,
    invalid_bounds,
    cell_capacity,
    reserve_capacity,
    query_capacity,
};

struct RoomDropIndexRecord final {
    RoomDropKind kind{RoomDropKind::equipment};
    std::uint16_t ordinal{0xFFFFU};
    std::uint16_t home_cell{0xFFFFU};
    combat::Vec3 position{};
    bool present{};
};

struct RoomDropCandidateSet final {
    std::array<RoomDropIndexRecord, kRoomDropQueryCandidateCapacity> records{};
    std::uint16_t count{};
    std::uint16_t candidates_examined{};
    RoomDropIndexFault fault{RoomDropIndexFault::none};
};

class RoomDropSpatialIndex final {
public:
    void clear() noexcept;
    [[nodiscard]] bool reset(const combat::RoomMonsterPlan& plan) noexcept;
    [[nodiscard]] bool insert_monster_drop(RoomDropKind kind,
        std::uint16_t ordinal, std::uint16_t spawn_ordinal,
        combat::Vec3 position) noexcept;
    [[nodiscard]] bool can_insert_monster_drop(RoomDropKind kind,
        std::uint16_t ordinal, std::uint16_t spawn_ordinal,
        combat::Vec3 position) const noexcept;
    [[nodiscard]] bool insert_abyss_reserve(RoomDropKind kind,
        std::uint16_t ordinal, combat::Vec3 position) noexcept;
    [[nodiscard]] bool can_insert_abyss_reserve(RoomDropKind kind,
        std::uint16_t ordinal, combat::Vec3 position) const noexcept;
    [[nodiscard]] bool set_present(RoomDropKind kind,
        std::uint16_t ordinal, bool present) noexcept;
    [[nodiscard]] bool has_present_record(RoomDropKind kind,
        std::uint16_t ordinal) const noexcept;
    void write_candidates(const combat::Aabb& world_bounds,
        RoomDropCandidateSet& output) const noexcept;

    [[nodiscard]] RoomDropIndexFault fault() const noexcept { return fault_; }
    [[nodiscard]] std::uint16_t monster_count() const noexcept {
        return monster_count_;
    }
    [[nodiscard]] std::uint8_t last_set_present_records_examined()
        const noexcept {
        return last_set_present_records_examined_;
    }

private:
    struct Bucket final {
        std::array<RoomDropIndexRecord, kRoomDropCellCapacity> records{};
        std::uint8_t count{};
    };

    std::array<Bucket, kRoomDropCellCount> cells_{};
    std::array<RoomDropIndexRecord, kRoomDropReserveCapacity> reserve_{};
    std::array<std::uint16_t, limits::kRoomMonsterCapacity> home_cells_{};
    std::uint8_t reserve_count_{};
    std::uint8_t last_set_present_records_examined_{};
    std::uint16_t monster_count_{};
    RoomDropIndexFault fault_{RoomDropIndexFault::none};
};

static_assert(kRoomDropCellCount == combat::kRoomMonsterCellCount);
static_assert(kRoomDropQueryCandidateCapacity == 331U);

}  // namespace arpg::dungeon
