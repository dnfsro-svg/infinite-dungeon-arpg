#include "dungeon/room_environment.hpp"

#include "combat/combat_collision.hpp"
#include "combat/room_bounds.hpp"
#include "combat/room_spatial_grid.hpp"
#include "core/deterministic_rng.hpp"
#include "dungeon/room_monster_plan_builder.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>

namespace arpg::dungeon {
namespace {

using combat::Aabb;
using combat::RoomEnvironmentBlueprint;
using combat::RoomEnvironmentRecord;
using combat::RoomMonsterBlueprint;
using combat::RoomMonsterPlan;
using combat::RoomObstacleKind;
using combat::RoomPropKind;
using combat::Vec3;
using core::DeterministicRng;

constexpr std::uint16_t kDefaultEnvironmentRecordCount = 800U;
constexpr std::uint16_t kBreakableObstacleHp = 60U;
constexpr float kEntryAvoidanceRadius = 4.0F;
constexpr float kExitAvoidanceRadius = 3.0F;
constexpr float kObstacleHalfWidth = 0.70F;
constexpr float kObstacleHalfDepth = 0.55F;
constexpr float kObstacleHeight = 1.50F;
constexpr float kObstacleCellInset = 0.30F;
constexpr std::uint64_t kEnvironmentRootDomain = 0x454E56524F4F4D31ULL;
constexpr std::uint64_t kRecordDomain = 0x454E565245434F52ULL;
constexpr std::uint64_t kDepthDomain = 0x454E564445505448ULL;
constexpr std::uint64_t kDensityDomain = 0x454E5644454E5349ULL;
constexpr std::uint64_t kVersionDomain = 0x454E565645525331ULL;
constexpr std::uint64_t kEcologyDomain = 0x454E5645434F4C31ULL;
constexpr std::uint64_t kEntryDomain = 0x454E56454E545231ULL;
constexpr std::uint64_t kHoleDomain = 0x454E56484F4C4531ULL;
constexpr std::uint64_t kAbyssDomain = 0x454E564142595331ULL;
constexpr std::uint64_t kCompositionDomain = 0x454E56434F4D5031ULL;
constexpr std::uint64_t kPropDomain = 0x454E5650524F5031ULL;
constexpr std::uint64_t kPositionDomain = 0x454E56504F534954ULL;
constexpr std::uint64_t kTransformDomain = 0x454E5654524E5331ULL;
constexpr std::uint64_t kFNVOffset = 14695981039346656037ULL;
constexpr std::uint64_t kFNVPrime = 1099511628211ULL;
constexpr std::array<Vec3, 4U> kDoorCenters{{
    {0.0F, combat::room_bounds::min_y, 0.0F},
    {0.0F, combat::room_bounds::max_y, 0.0F},
    {combat::room_bounds::min_x, 0.0F, 0.0F},
    {combat::room_bounds::max_x, 0.0F, 0.0F},
}};
constexpr Vec3 kHoleCenter{0.0F, 3.5F, 0.0F};

[[nodiscard]] std::uint64_t bounded(
    DeterministicRng& rng, std::uint64_t limit) noexcept {
    const auto value = rng.next_bounded(limit);
    return value.has_value() ? *value : 0U;
}

void clear_environment(RoomEnvironmentBlueprint& environment) noexcept {
    for (RoomEnvironmentRecord& record : environment.records) {
        record = RoomEnvironmentRecord{};
    }
    environment.cell_offsets.fill(0U);
    environment.cell_counts.fill(0U);
    environment.record_count = 0U;
    environment.obstacle_count = 0U;
    environment.generator_version = 0U;
    environment.blueprint_hash = 0U;
}

[[nodiscard]] Vec3 entry_spawn(checkpoint::EntrySide entry) noexcept {
    switch (entry) {
    case checkpoint::EntrySide::initial:
        return {};
    case checkpoint::EntrySide::top:
        return {0.0F, combat::room_bounds::min_y + 0.75F, 0.0F};
    case checkpoint::EntrySide::bottom:
        return {0.0F, combat::room_bounds::max_y - 0.75F, 0.0F};
    case checkpoint::EntrySide::left:
        return {combat::room_bounds::min_x + 1.50F, 0.0F, 0.0F};
    case checkpoint::EntrySide::right:
        return {combat::room_bounds::max_x - 1.50F, 0.0F, 0.0F};
    }
    return {};
}

[[nodiscard]] Aabb initial_monster_bounds(
    const RoomMonsterBlueprint& monster) noexcept {
    return combat::room_monster_initial_bounds(monster);
}

[[nodiscard]] bool finite(Vec3 value) noexcept {
    return std::isfinite(value.x) && std::isfinite(value.y)
        && std::isfinite(value.z);
}

[[nodiscard]] bool finite(const Aabb& bounds) noexcept {
    return finite(bounds.minimum) && finite(bounds.maximum);
}

[[nodiscard]] bool circle_overlaps_aabb(
    Vec3 center, float radius, const Aabb& bounds) noexcept {
    const float nearest_x = std::clamp(
        center.x, bounds.minimum.x, bounds.maximum.x);
    const float nearest_y = std::clamp(
        center.y, bounds.minimum.y, bounds.maximum.y);
    const float x = center.x - nearest_x;
    const float y = center.y - nearest_y;
    return x * x + y * y <= radius * radius;
}

[[nodiscard]] bool is_central_cross(std::size_t cell) noexcept {
    const std::size_t column = cell % combat::room_spatial::columns;
    const std::size_t row = cell / combat::room_spatial::columns;
    return column == 9U || column == 10U || row == 9U || row == 10U;
}

[[nodiscard]] bool obstacle_is_clear(
    const checkpoint::RoomDescriptor& room,
    const RoomMonsterPlan& monsters,
    std::size_t home_cell,
    const Aabb& bounds) noexcept {
    if (circle_overlaps_aabb(
            entry_spawn(room.entry), kEntryAvoidanceRadius, bounds)) {
        return false;
    }
    for (const Vec3 door : kDoorCenters) {
        if (circle_overlaps_aabb(door, kExitAvoidanceRadius, bounds)) {
            return false;
        }
    }
    if (room.has_hole
            && circle_overlaps_aabb(
                kHoleCenter, kExitAvoidanceRadius, bounds)) {
        return false;
    }
    if (home_cell >= combat::kRoomMonsterCellCount) return false;
    const std::size_t begin = monsters.cell_offsets[home_cell];
    const std::size_t end = monsters.cell_offsets[home_cell + 1U];
    if (begin > end || end > monsters.monster_count) return false;
    for (std::size_t index = begin; index < end; ++index) {
        if (combat::overlaps_inclusive(
                bounds, initial_monster_bounds(monsters.monsters[index]))) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] std::uint64_t environment_root_seed(
    const checkpoint::RoomDescriptor& room,
    std::uint32_t generator_version,
    std::uint16_t monster_count) noexcept {
    const auto derive_field = [](std::uint64_t root,
                                  std::uint64_t domain,
                                  std::uint64_t value) noexcept {
        auto domain_stream = DeterministicRng::derive_stream(root, domain);
        return DeterministicRng::derive_stream(
            domain_stream.next_u64(), value).next_u64();
    };
    std::uint64_t seed = DeterministicRng::derive_stream(
        room.seed, kEnvironmentRootDomain).next_u64();
    seed = derive_field(seed, kDepthDomain, room.depth);
    seed = derive_field(seed, kVersionDomain, generator_version);
    seed = derive_field(
        seed, kEcologyDomain, static_cast<std::uint64_t>(room.ecology));
    seed = derive_field(
        seed, kEntryDomain, static_cast<std::uint64_t>(room.entry));
    seed = derive_field(seed, kHoleDomain, room.has_hole ? 1U : 0U);
    seed = derive_field(seed, kAbyssDomain, room.is_abyss ? 1U : 0U);
    return derive_field(seed, kDensityDomain, monster_count);
}

[[nodiscard]] DeterministicRng record_stream(
    std::uint64_t root_seed,
    std::uint16_t ordinal,
    std::uint64_t domain) noexcept {
    auto ordinal_stream = DeterministicRng::derive_stream(
        root_seed, kRecordDomain ^ static_cast<std::uint64_t>(ordinal));
    return DeterministicRng::derive_stream(
        ordinal_stream.next_u64(), domain);
}

[[nodiscard]] RoomPropKind decoration_prop(
    checkpoint::DungeonElement ecology,
    std::uint64_t choice) noexcept {
    switch (ecology) {
    case checkpoint::DungeonElement::fire: {
        constexpr std::array<RoomPropKind, 4U> props{{
            RoomPropKind::torch, RoomPropKind::banner,
            RoomPropKind::weapon_rack, RoomPropKind::bone_pile}};
        return props[choice % props.size()];
    }
    case checkpoint::DungeonElement::water: {
        constexpr std::array<RoomPropKind, 3U> props{{
            RoomPropKind::lantern, RoomPropKind::coral,
            RoomPropKind::grate}};
        return props[choice % props.size()];
    }
    case checkpoint::DungeonElement::lightning: {
        constexpr std::array<RoomPropKind, 3U> props{{
            RoomPropKind::arc_lamp, RoomPropKind::capacitor_bank,
            RoomPropKind::grounding_rod}};
        return props[choice % props.size()];
    }
    case checkpoint::DungeonElement::chaos: {
        constexpr std::array<RoomPropKind, 3U> props{{
            RoomPropKind::rift_lantern, RoomPropKind::anomaly_condenser,
            RoomPropKind::warning_obelisk}};
        return props[choice % props.size()];
    }
    }
    return RoomPropKind::torch;
}

[[nodiscard]] RoomPropKind solid_obstacle_prop(
    checkpoint::DungeonElement ecology) noexcept {
    switch (ecology) {
    case checkpoint::DungeonElement::fire:
        return RoomPropKind::brazier;
    case checkpoint::DungeonElement::water:
        return RoomPropKind::coral;
    case checkpoint::DungeonElement::lightning:
        return RoomPropKind::capacitor_bank;
    case checkpoint::DungeonElement::chaos:
        return RoomPropKind::warning_obelisk;
    }
    return RoomPropKind::brazier;
}

[[nodiscard]] Vec3 cell_anchor(
    std::size_t cell, float x_fraction, float y_fraction,
    float inset) noexcept {
    const std::size_t column = cell % combat::room_spatial::columns;
    const std::size_t row = cell / combat::room_spatial::columns;
    const float minimum_x = combat::room_bounds::min_x
        + static_cast<float>(column) * combat::room_spatial::cell_width;
    const float minimum_y = combat::room_bounds::min_y
        + static_cast<float>(row) * combat::room_spatial::cell_depth;
    const float usable_width = combat::room_spatial::cell_width - 2.0F * inset;
    const float usable_depth = combat::room_spatial::cell_depth - 2.0F * inset;
    return {
        minimum_x + inset + x_fraction * usable_width,
        minimum_y + inset + y_fraction * usable_depth,
        0.0F,
    };
}

[[nodiscard]] Aabb obstacle_bounds(Vec3 center) noexcept {
    return {
        {center.x - kObstacleHalfWidth,
            center.y - kObstacleHalfDepth, 0.0F},
        {center.x + kObstacleHalfWidth,
            center.y + kObstacleHalfDepth, kObstacleHeight},
    };
}

[[nodiscard]] bool place_obstacle(
    const checkpoint::RoomDescriptor& room,
    const RoomMonsterPlan& monsters,
    std::uint64_t root_seed,
    std::size_t cell,
    std::uint16_t ordinal,
    Vec3& out_anchor,
    Aabb& out_bounds) noexcept {
    constexpr std::uint32_t kRandomAttempts = 32U;
    constexpr std::uint32_t kScanSide = 17U;
    constexpr float kFractionScale = 1.0F / 65535.0F;
    constexpr float kInset = kObstacleHalfWidth + kObstacleCellInset;

    for (std::uint32_t attempt = 0U; attempt < kRandomAttempts; ++attempt) {
        auto rng = record_stream(root_seed, ordinal,
            kPositionDomain ^ static_cast<std::uint64_t>(attempt));
        const float x = static_cast<float>(bounded(rng, 65536U))
            * kFractionScale;
        const float y = static_cast<float>(bounded(rng, 65536U))
            * kFractionScale;
        const Vec3 anchor = cell_anchor(cell, x, y, kInset);
        const Aabb bounds = obstacle_bounds(anchor);
        if (obstacle_is_clear(room, monsters, cell, bounds)) {
            out_anchor = anchor;
            out_bounds = bounds;
            return true;
        }
    }

    for (std::uint32_t y = 0U; y < kScanSide; ++y) {
        for (std::uint32_t x = 0U; x < kScanSide; ++x) {
            const Vec3 anchor = cell_anchor(cell,
                static_cast<float>(x + 1U) / 18.0F,
                static_cast<float>(y + 1U) / 18.0F, kInset);
            const Aabb bounds = obstacle_bounds(anchor);
            if (obstacle_is_clear(room, monsters, cell, bounds)) {
                out_anchor = anchor;
                out_bounds = bounds;
                return true;
            }
        }
    }
    return false;
}

[[nodiscard]] std::uint16_t cell_for(Vec3 position) noexcept {
    const float column_value = (position.x - combat::room_bounds::min_x)
        / combat::room_spatial::cell_width;
    const float row_value = (position.y - combat::room_bounds::min_y)
        / combat::room_spatial::cell_depth;
    const std::size_t column = std::min<std::size_t>(
        combat::room_spatial::columns - 1U,
        column_value <= 0.0F ? 0U
            : static_cast<std::size_t>(column_value));
    const std::size_t row = std::min<std::size_t>(
        combat::room_spatial::rows - 1U,
        row_value <= 0.0F ? 0U
            : static_cast<std::size_t>(row_value));
    return static_cast<std::uint16_t>(
        row * combat::room_spatial::columns + column);
}

[[nodiscard]] std::size_t cell_axis(float coordinate, float minimum,
    float cell_size, std::size_t cell_count) noexcept {
    if (!std::isfinite(coordinate)) return 0U;
    const float normalized = (coordinate - minimum) / cell_size;
    if (normalized <= 0.0F) return 0U;
    if (normalized >= static_cast<float>(cell_count)) return cell_count - 1U;
    return static_cast<std::size_t>(normalized);
}

[[nodiscard]] bool environment_record_visible(const Aabb& bounds,
    const RoomEnvironmentRecord& record) noexcept {
    if (record.obstacle.kind != combat::RoomObstacleKind::none) {
        const Aabb& obstacle = record.obstacle.bounds;
        return obstacle.minimum.x <= bounds.maximum.x
            && obstacle.maximum.x >= bounds.minimum.x
            && obstacle.minimum.y <= bounds.maximum.y
            && obstacle.maximum.y >= bounds.minimum.y
            && obstacle.minimum.z <= bounds.maximum.z
            && obstacle.maximum.z >= bounds.minimum.z;
    }
    return record.anchor.x >= bounds.minimum.x
        && record.anchor.x <= bounds.maximum.x
        && record.anchor.y >= bounds.minimum.y
        && record.anchor.y <= bounds.maximum.y
        && record.anchor.z >= bounds.minimum.z
        && record.anchor.z <= bounds.maximum.z;
}

[[nodiscard]] bool aabb_within_cell(
    const Aabb& bounds, std::size_t cell) noexcept {
    const std::size_t column = cell % combat::room_spatial::columns;
    const std::size_t row = cell / combat::room_spatial::columns;
    const float minimum_x = combat::room_bounds::min_x
        + static_cast<float>(column) * combat::room_spatial::cell_width;
    const float maximum_x = minimum_x + combat::room_spatial::cell_width;
    const float minimum_y = combat::room_bounds::min_y
        + static_cast<float>(row) * combat::room_spatial::cell_depth;
    const float maximum_y = minimum_y + combat::room_spatial::cell_depth;
    return bounds.minimum.x >= minimum_x && bounds.maximum.x <= maximum_x
        && bounds.minimum.y >= minimum_y && bounds.maximum.y <= maximum_y;
}

[[nodiscard]] bool all_doors_reachable(
    const checkpoint::RoomDescriptor& room,
    const RoomEnvironmentBlueprint& environment) noexcept {
    std::array<bool, combat::kRoomEnvironmentCellCount> blocked{};
    for (std::size_t index = 0U; index < environment.record_count; ++index) {
        const RoomEnvironmentRecord& record = environment.records[index];
        if (record.obstacle.kind != RoomObstacleKind::none) {
            if (record.home_cell >= blocked.size()) return false;
            blocked[record.home_cell] = true;
        }
    }

    const std::uint16_t start = cell_for(entry_spawn(room.entry));
    if (blocked[start]) return false;
    std::array<bool, combat::kRoomEnvironmentCellCount> visited{};
    std::array<std::uint16_t,
        combat::kRoomEnvironmentCellCount> queue{};
    std::size_t head = 0U;
    std::size_t tail = 0U;
    queue[tail++] = start;
    visited[start] = true;
    while (head < tail) {
        const std::uint16_t current = queue[head++];
        const std::size_t row = current / combat::room_spatial::columns;
        const std::size_t column = current % combat::room_spatial::columns;
        const auto visit = [&blocked, &visited, &queue, &tail](
                               std::size_t next) noexcept {
            if (!blocked[next] && !visited[next]) {
                visited[next] = true;
                queue[tail++] = static_cast<std::uint16_t>(next);
            }
        };
        if (column > 0U) visit(current - 1U);
        if (column + 1U < combat::room_spatial::columns) {
            visit(current + 1U);
        }
        if (row > 0U) visit(current - combat::room_spatial::columns);
        if (row + 1U < combat::room_spatial::rows) {
            visit(current + combat::room_spatial::columns);
        }
    }
    for (const Vec3 door : kDoorCenters) {
        if (!visited[cell_for(door)]) return false;
    }
    return true;
}

void hash_byte(std::uint64_t& hash, std::uint8_t value) noexcept {
    hash ^= value;
    hash *= kFNVPrime;
}

template <typename Unsigned>
void hash_unsigned(std::uint64_t& hash, Unsigned value) noexcept {
    static_assert(std::is_unsigned_v<Unsigned>);
    for (std::size_t index = 0U; index < sizeof(Unsigned); ++index) {
        hash_byte(hash, static_cast<std::uint8_t>(
            value >> static_cast<unsigned int>(index * 8U)));
    }
}

void hash_float(std::uint64_t& hash, float value) noexcept {
    if (value == 0.0F) value = 0.0F;
    std::uint32_t bits{};
    std::memcpy(&bits, &value, sizeof(bits));
    hash_unsigned(hash, bits);
}

void hash_vec3(std::uint64_t& hash, Vec3 value) noexcept {
    hash_float(hash, value.x);
    hash_float(hash, value.y);
    hash_float(hash, value.z);
}

void hash_aabb(std::uint64_t& hash, const Aabb& bounds) noexcept {
    hash_vec3(hash, bounds.minimum);
    hash_vec3(hash, bounds.maximum);
}

[[nodiscard]] std::uint64_t environment_hash(
    const RoomEnvironmentBlueprint& environment) noexcept {
    std::uint64_t hash = kFNVOffset;
    hash_unsigned(hash, environment.generator_version);
    hash_unsigned(hash, environment.record_count);
    hash_unsigned(hash, environment.obstacle_count);
    for (const std::uint16_t offset : environment.cell_offsets) {
        hash_unsigned(hash, offset);
    }
    for (const std::uint8_t count : environment.cell_counts) {
        hash_unsigned(hash, count);
    }
    for (std::size_t index = 0U; index < environment.record_count; ++index) {
        const RoomEnvironmentRecord& record = environment.records[index];
        hash_unsigned(hash, record.ordinal);
        hash_unsigned(hash, record.home_cell);
        hash_unsigned(hash, static_cast<std::uint8_t>(record.prop));
        hash_vec3(hash, record.anchor);
        hash_unsigned(hash, record.scale_bp);
        hash_unsigned(hash, record.quarter_turns);
        hash_unsigned(hash, static_cast<std::uint8_t>(record.mirror_x));
        hash_unsigned(hash,
            static_cast<std::uint8_t>(record.obstacle.kind));
        hash_unsigned(hash, record.obstacle.max_hp);
        hash_aabb(hash, record.obstacle.bounds);
    }
    return hash;
}

[[nodiscard]] bool zero_aabb(const Aabb& bounds) noexcept {
    return bounds.minimum.x == 0.0F && bounds.minimum.y == 0.0F
        && bounds.minimum.z == 0.0F && bounds.maximum.x == 0.0F
        && bounds.maximum.y == 0.0F && bounds.maximum.z == 0.0F;
}

[[nodiscard]] bool monster_plan_sealed(
    const checkpoint::RoomDescriptor& room,
    const RoomMonsterPlan& monsters) noexcept {
    return monsters.generator_version != 0U
        && monsters.monster_count > 0U
        && monsters.monster_count <= monsters.monsters.size()
        && room_monster_plan_legal(room, monsters);
}

[[nodiscard]] RoomEnvironmentBuildResult build_with_count(
    const checkpoint::RoomDescriptor& room,
    const DungeonRules& rules,
    std::uint32_t generator_version,
    const RoomMonsterPlan& monsters,
    std::uint16_t requested_record_count,
    bool force_placement_failure,
    RoomEnvironmentBlueprint& out_environment) noexcept {
    clear_environment(out_environment);
    if (generator_version == 0U
            || validate_rules(rules) != DungeonFault::none) {
        return {DungeonFault::invalid_rules};
    }
    if (!monster_plan_sealed(room, monsters)) {
        return {DungeonFault::invalid_monster_plan};
    }
    if (requested_record_count
            > combat::kRoomEnvironmentRecordCapacity) {
        return {DungeonFault::environment_capacity};
    }

    const std::uint16_t base_count = static_cast<std::uint16_t>(
        requested_record_count / combat::kRoomEnvironmentCellCount);
    const std::uint16_t extra_cells = static_cast<std::uint16_t>(
        requested_record_count % combat::kRoomEnvironmentCellCount);
    if (base_count > 3U) {
        return {DungeonFault::environment_capacity};
    }

    const std::uint64_t root_seed = environment_root_seed(
        room, generator_version, monsters.monster_count);
    auto composition_rng = DeterministicRng::derive_stream(
        root_seed, kCompositionDomain);
    const std::uint8_t obstacle_phase = static_cast<std::uint8_t>(
        bounded(composition_rng, 5U));
    std::uint16_t ordinal = 0U;
    std::uint16_t obstacle_count = 0U;
    for (std::size_t cell = 0U;
         cell < combat::kRoomEnvironmentCellCount; ++cell) {
        out_environment.cell_offsets[cell] = ordinal;
        const std::uint8_t cell_count = static_cast<std::uint8_t>(
            base_count + (cell < extra_cells ? 1U : 0U));
        if (cell_count > 3U) {
            clear_environment(out_environment);
            return {DungeonFault::environment_capacity};
        }
        out_environment.cell_counts[cell] = cell_count;
        for (std::uint8_t local = 0U; local < cell_count; ++local) {
            RoomEnvironmentRecord& record = out_environment.records[ordinal];
            record = RoomEnvironmentRecord{};
            record.ordinal = ordinal;
            record.home_cell = static_cast<std::uint16_t>(cell);
            const bool obstacle = local == 0U && !is_central_cross(cell)
                && (cell + obstacle_phase) % 5U == 0U;
            if (obstacle) {
                record.obstacle.kind = ((cell / 5U) & 1U) == 0U
                    ? RoomObstacleKind::breakable
                    : RoomObstacleKind::solid;
                record.obstacle.max_hp =
                    record.obstacle.kind == RoomObstacleKind::breakable
                    ? kBreakableObstacleHp : 0U;
                record.prop = record.obstacle.kind
                        == RoomObstacleKind::breakable
                    ? RoomPropKind::crate
                    : solid_obstacle_prop(room.ecology);
                if (force_placement_failure
                        || !place_obstacle(room, monsters, root_seed, cell, ordinal,
                        record.anchor, record.obstacle.bounds)) {
                    clear_environment(out_environment);
                    return {DungeonFault::environment_placement};
                }
                ++obstacle_count;
            } else {
                auto prop_rng = record_stream(
                    root_seed, ordinal, kPropDomain);
                auto position_rng = record_stream(
                    root_seed, ordinal, kPositionDomain);
                record.prop = decoration_prop(
                    room.ecology, bounded(prop_rng, 16U));
                const float x = static_cast<float>(
                    bounded(position_rng, 65536U))
                    / 65535.0F;
                const float y = static_cast<float>(
                    bounded(position_rng, 65536U))
                    / 65535.0F;
                record.anchor = cell_anchor(cell, x, y, 0.25F);
            }
            auto transform_rng = record_stream(
                root_seed, ordinal, kTransformDomain);
            record.scale_bp = static_cast<std::uint16_t>(
                9000U + bounded(transform_rng, 2001U));
            record.quarter_turns = static_cast<std::uint8_t>(
                bounded(transform_rng, 4U));
            record.mirror_x = bounded(transform_rng, 2U) != 0U;
            ++ordinal;
        }
    }
    out_environment.cell_offsets[combat::kRoomEnvironmentCellCount] = ordinal;
    out_environment.record_count = ordinal;
    out_environment.obstacle_count = obstacle_count;
    out_environment.generator_version = generator_version;

    if (!all_doors_reachable(room, out_environment)) {
        clear_environment(out_environment);
        return {DungeonFault::environment_navigation};
    }
    out_environment.blueprint_hash = environment_hash(out_environment);
    if (!room_environment_legal(room, monsters, out_environment)) {
        clear_environment(out_environment);
        return {DungeonFault::environment_placement};
    }
    return {};
}

}  // namespace

namespace {

[[nodiscard]] bool visible_record_precedes(
    const RoomEnvironmentRecord& left,
    const RoomEnvironmentRecord& right) noexcept {
    if (left.home_cell != right.home_cell) {
        return left.home_cell < right.home_cell;
    }
    if (left.obstacle.kind != right.obstacle.kind) {
        return static_cast<std::uint8_t>(left.obstacle.kind)
            < static_cast<std::uint8_t>(right.obstacle.kind);
    }
    return left.ordinal < right.ordinal;
}

VisibleEnvironmentQueryResult query_visible_environment_bounded(
    const RoomEnvironmentBlueprint& blueprint,
    const Aabb& world_bounds, std::size_t output_capacity,
    VisibleEnvironmentSet& output) noexcept {
    output = {};
    if (!finite(world_bounds)
            || world_bounds.minimum.x > world_bounds.maximum.x
            || world_bounds.minimum.y > world_bounds.maximum.y
            || world_bounds.minimum.z > world_bounds.maximum.z) {
        return {VisibleEnvironmentQueryStatus::invalid_query,
            DungeonFault::none};
    }
    if (blueprint.record_count > blueprint.records.size()) {
        return {VisibleEnvironmentQueryStatus::hard_fault,
            DungeonFault::environment_capacity};
    }
    if (output_capacity > output.records.size()) {
        return {VisibleEnvironmentQueryStatus::hard_fault,
            DungeonFault::environment_capacity};
    }

    const std::size_t visible_first_column = cell_axis(
        world_bounds.minimum.x, combat::room_bounds::min_x,
        combat::room_spatial::cell_width, combat::room_spatial::columns);
    const std::size_t visible_last_column = cell_axis(
        world_bounds.maximum.x, combat::room_bounds::min_x,
        combat::room_spatial::cell_width, combat::room_spatial::columns);
    const std::size_t visible_first_row = cell_axis(
        world_bounds.minimum.y, combat::room_bounds::min_y,
        combat::room_spatial::cell_depth, combat::room_spatial::rows);
    const std::size_t visible_last_row = cell_axis(
        world_bounds.maximum.y, combat::room_bounds::min_y,
        combat::room_spatial::cell_depth, combat::room_spatial::rows);
    const std::size_t first_column = visible_first_column == 0U
        ? 0U : visible_first_column - 1U;
    const std::size_t last_column = (std::min)(
        visible_last_column + 1U, combat::room_spatial::columns - 1U);
    const std::size_t first_row = visible_first_row == 0U
        ? 0U : visible_first_row - 1U;
    const std::size_t last_row = (std::min)(
        visible_last_row + 1U, combat::room_spatial::rows - 1U);
    if (last_column - first_column + 1U > 7U
            || last_row - first_row + 1U > 5U) {
        return {VisibleEnvironmentQueryStatus::hard_fault,
            DungeonFault::environment_capacity};
    }

    for (std::size_t row = first_row; row <= last_row; ++row) {
        for (std::size_t column = first_column;
                column <= last_column; ++column) {
            const std::size_t cell = row * combat::room_spatial::columns
                + column;
            const std::size_t begin = blueprint.cell_offsets[cell];
            const std::size_t count = blueprint.cell_counts[cell];
            if (count > 3U || begin > blueprint.record_count
                    || count > blueprint.record_count - begin) {
                output = {};
                return {VisibleEnvironmentQueryStatus::hard_fault,
                    DungeonFault::environment_capacity};
            }
            const std::size_t end = begin + count;
            bool broken_previous_span = begin != 0U;
            if (cell != 0U) {
                const std::size_t previous_begin =
                    blueprint.cell_offsets[cell - 1U];
                const std::size_t previous_count =
                    blueprint.cell_counts[cell - 1U];
                broken_previous_span = previous_count > 3U
                    || previous_begin > blueprint.record_count
                    || previous_count
                        > blueprint.record_count - previous_begin
                    || previous_begin + previous_count != begin;
            }
            if (broken_previous_span
                    || blueprint.cell_offsets[cell + 1U] != end
                    || output.candidates_examined + count
                        > kEnvironmentQueryCandidateCapacity) {
                output = {};
                return {VisibleEnvironmentQueryStatus::hard_fault,
                    DungeonFault::environment_capacity};
            }
            for (std::size_t index = begin; index < end; ++index) {
                const RoomEnvironmentRecord& record = blueprint.records[index];
                ++output.candidates_examined;
                if (record.home_cell != cell || record.ordinal != index) {
                    output = {};
                    return {VisibleEnvironmentQueryStatus::hard_fault,
                        DungeonFault::environment_capacity};
                }
                if (!environment_record_visible(world_bounds, record)) continue;
                if (output.count >= output_capacity) {
                    output = {};
                    return {VisibleEnvironmentQueryStatus::hard_fault,
                        DungeonFault::environment_capacity};
                }
                output.records[output.count++] = record;
            }
        }
    }
    for (std::size_t index = 1U; index < output.count; ++index) {
        const RoomEnvironmentRecord value = output.records[index];
        std::size_t insertion = index;
        while (insertion != 0U && visible_record_precedes(
                value, output.records[insertion - 1U])) {
            output.records[insertion] = output.records[insertion - 1U];
            --insertion;
        }
        output.records[insertion] = value;
    }
    return {VisibleEnvironmentQueryStatus::ok, DungeonFault::none};
}

}  // namespace

VisibleEnvironmentQueryResult query_visible_environment(
    const RoomEnvironmentBlueprint& blueprint,
    const Aabb& world_bounds, VisibleEnvironmentSet& output) noexcept {
    return query_visible_environment_bounded(blueprint, world_bounds,
        output.records.size(), output);
}

bool write_visible_environment(const RoomEnvironmentBlueprint& blueprint,
    const Aabb& world_bounds, VisibleEnvironmentSet& output) noexcept {
    return query_visible_environment(blueprint, world_bounds, output).status
        == VisibleEnvironmentQueryStatus::ok;
}

RoomEnvironmentBuildResult build_room_environment(
    const checkpoint::RoomDescriptor& room,
    const DungeonRules& rules,
    std::uint32_t generator_version,
    const RoomMonsterPlan& monsters,
    RoomEnvironmentBlueprint& out_environment) noexcept {
    return build_with_count(room, rules, generator_version, monsters,
        kDefaultEnvironmentRecordCount, false, out_environment);
}

bool room_environment_legal(
    const checkpoint::RoomDescriptor& room,
    const RoomMonsterPlan& monsters,
    const RoomEnvironmentBlueprint& environment) noexcept {
    if (!monster_plan_sealed(room, monsters)
            || environment.generator_version == 0U
            || environment.record_count
                > combat::kRoomEnvironmentRecordCapacity
            || environment.cell_offsets[0] != 0U
            || environment.cell_offsets[combat::kRoomEnvironmentCellCount]
                != environment.record_count) {
        return false;
    }

    std::size_t counted_obstacles = 0U;
    for (std::size_t cell = 0U;
         cell < combat::kRoomEnvironmentCellCount; ++cell) {
        const std::size_t begin = environment.cell_offsets[cell];
        const std::size_t end = environment.cell_offsets[cell + 1U];
        if (environment.cell_counts[cell] > 3U || begin > end
                || end > environment.record_count
                || end - begin != environment.cell_counts[cell]) {
            return false;
        }
        std::size_t cell_obstacles = 0U;
        for (std::size_t index = begin; index < end; ++index) {
            const RoomEnvironmentRecord& record = environment.records[index];
            if (record.ordinal != index || record.home_cell != cell
                    || !finite(record.anchor) || record.scale_bp == 0U
                    || record.quarter_turns > 3U
                    || record.prop >= RoomPropKind::count
                    || cell_for(record.anchor) != cell) {
                return false;
            }
            if (record.obstacle.kind == RoomObstacleKind::none) {
                if (record.obstacle.max_hp != 0U
                        || !zero_aabb(record.obstacle.bounds)) {
                    return false;
                }
                continue;
            }
            if (record.obstacle.kind != RoomObstacleKind::solid
                    && record.obstacle.kind != RoomObstacleKind::breakable) {
                return false;
            }
            if (!finite(record.obstacle.bounds)
                    || record.obstacle.bounds.minimum.x
                        > record.obstacle.bounds.maximum.x
                    || record.obstacle.bounds.minimum.y
                        > record.obstacle.bounds.maximum.y
                    || record.obstacle.bounds.minimum.z
                        > record.obstacle.bounds.maximum.z
                    || (record.obstacle.kind == RoomObstacleKind::solid
                        && record.obstacle.max_hp != 0U)
                    || (record.obstacle.kind == RoomObstacleKind::breakable
                        && record.obstacle.max_hp == 0U)
                    || is_central_cross(cell)
                    || !aabb_within_cell(record.obstacle.bounds, cell)
                    || !obstacle_is_clear(
                        room, monsters, cell, record.obstacle.bounds)) {
                return false;
            }
            ++cell_obstacles;
            ++counted_obstacles;
        }
        if (cell_obstacles > 1U) return false;
    }
    return counted_obstacles == environment.obstacle_count
        && all_doors_reachable(room, environment)
        && environment_hash(environment) == environment.blueprint_hash;
}

namespace test_support {

VisibleEnvironmentQueryResult
query_visible_environment_with_output_capacity(
    const RoomEnvironmentBlueprint& blueprint,
    const Aabb& world_bounds,
    std::size_t output_capacity,
    VisibleEnvironmentSet& output) noexcept {
    return query_visible_environment_bounded(
        blueprint, world_bounds, output_capacity, output);
}

RoomEnvironmentBuildResult build_room_environment_with_record_count(
    const checkpoint::RoomDescriptor& room,
    const DungeonRules& rules,
    std::uint32_t generator_version,
    const RoomMonsterPlan& monsters,
    std::uint16_t requested_record_count,
    RoomEnvironmentBlueprint& out_environment) noexcept {
    return build_with_count(room, rules, generator_version, monsters,
        requested_record_count, false, out_environment);
}

RoomEnvironmentBuildResult
build_room_environment_with_forced_placement_failure(
    const checkpoint::RoomDescriptor& room,
    const DungeonRules& rules,
    std::uint32_t generator_version,
    const RoomMonsterPlan& monsters,
    RoomEnvironmentBlueprint& out_environment) noexcept {
    return build_with_count(room, rules, generator_version, monsters,
        kDefaultEnvironmentRecordCount, true, out_environment);
}

}  // namespace test_support
}  // namespace arpg::dungeon
