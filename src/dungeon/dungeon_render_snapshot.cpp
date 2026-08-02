#include "dungeon/dungeon_session.hpp"

#include "combat/room_bounds.hpp"
#include "dungeon/room_generation.hpp"
#include "items/item_catalog.hpp"

#include <array>
#include <cmath>
#include <cstddef>

namespace arpg::dungeon {
namespace {

constexpr std::array<combat::Vec3, 4U> kDoorPositions{{
    {0.0F, combat::room_bounds::min_y, 0.0F},
    {0.0F, combat::room_bounds::max_y, 0.0F},
    {combat::room_bounds::min_x, 0.0F, 0.0F},
    {combat::room_bounds::max_x, 0.0F, 0.0F},
}};

constexpr std::array<ExitDirection, 4U> kDoorDirections{{
    ExitDirection::up,
    ExitDirection::down,
    ExitDirection::left,
    ExitDirection::right,
}};

[[nodiscard]] bool valid_query(const WorldViewQuery& query) noexcept {
    const combat::Aabb& bounds = query.world_bounds;
    return query.screen_width > 0 && query.screen_height > 0
        && std::isfinite(bounds.minimum.x)
        && std::isfinite(bounds.minimum.y)
        && std::isfinite(bounds.minimum.z)
        && std::isfinite(bounds.maximum.x)
        && std::isfinite(bounds.maximum.y)
        && std::isfinite(bounds.maximum.z)
        && bounds.minimum.x <= bounds.maximum.x
        && bounds.minimum.y <= bounds.maximum.y
        && bounds.minimum.z <= bounds.maximum.z;
}

[[nodiscard]] bool equipment_precedes(const GroundItemSnapshot& left,
    const GroundItemSnapshot& right,
    const combat::Vec3 center) noexcept {
    if (left.rarity != right.rarity) {
        return static_cast<std::uint8_t>(left.rarity)
            > static_cast<std::uint8_t>(right.rarity);
    }
    const float left_x = left.position.x - center.x;
    const float left_y = left.position.y - center.y;
    const float right_x = right.position.x - center.x;
    const float right_y = right.position.y - center.y;
    const float left_distance = left_x * left_x + left_y * left_y;
    const float right_distance = right_x * right_x + right_y * right_y;
    if (left_distance != right_distance) {
        return left_distance < right_distance;
    }
    return left.ordinal < right.ordinal;
}

void retain_equipment(GroundItemSnapshot packed,
    const combat::Vec3 center,
    DungeonRenderSnapshot& output) noexcept {
    std::size_t insert{};
    if (output.equipment_count < output.equipment.size()) {
        insert = output.equipment_count++;
        output.equipment[insert] = packed;
    } else {
        insert = output.equipment.size() - 1U;
        if (!equipment_precedes(
                packed, output.equipment[insert], center)) return;
        output.equipment[insert] = packed;
    }
    while (insert != 0U && equipment_precedes(
            output.equipment[insert], output.equipment[insert - 1U], center)) {
        const GroundItemSnapshot previous = output.equipment[insert - 1U];
        output.equipment[insert - 1U] = output.equipment[insert];
        output.equipment[insert] = previous;
        --insert;
    }
}

}  // namespace

bool DungeonSession::write_render_snapshot(const WorldViewQuery& query,
    DungeonRenderSnapshot& output) const noexcept {
    output = {};
    output.query = query;
    if (!valid_query(query)) return false;

    output.phase = phase_;
    output.ecology = stable_state_.current_room.ecology;
    output.has_active_room = combat_.has_value();
    output.has_combat = combat_.has_value();
    if (combat_.has_value()) output.combat = combat_->snapshot();

    if (room_environment_ != nullptr
            && !write_visible_environment(
                *room_environment_, query.world_bounds,
                output.environment)) {
        return false;
    }
    if (output.environment.count != 0U) {
        const combat::RoomObstacleRuntime* const obstacles =
            combat_.has_value() ? combat_->room_obstacles() : nullptr;
        for (std::uint16_t index = 0U;
                index < output.environment.count; ++index) {
            const combat::RoomEnvironmentRecord& record =
                output.environment.records[index];
            if (record.obstacle.kind == combat::RoomObstacleKind::none) {
                continue;
            }
            if (obstacles == nullptr) return false;
            const combat::RoomObstacleState* const state =
                obstacles->state(record.ordinal);
            if (state == nullptr || state->ordinal != record.ordinal
                    || state->kind != record.obstacle.kind
                    || state->max_hp != record.obstacle.max_hp) {
                return false;
            }
            output.environment_obstacles[index] = {
                state->ordinal,
                state->kind,
                state->hp,
                state->max_hp,
                state->broken_tick,
                true,
                state->intact,
            };
        }
    }

    RoomDropCandidateSet candidates{};
    room_drop_state_.spatial_index().write_candidates(
        query.world_bounds, candidates);
    if (candidates.fault != RoomDropIndexFault::none) return false;
    output.drop_candidates_examined = candidates.candidates_examined;
    const combat::Vec3 query_center{
        (query.world_bounds.minimum.x + query.world_bounds.maximum.x) * 0.5F,
        (query.world_bounds.minimum.y + query.world_bounds.maximum.y) * 0.5F,
        (query.world_bounds.minimum.z + query.world_bounds.maximum.z) * 0.5F,
    };
    for (std::size_t index = 0U; index < candidates.count; ++index) {
        const RoomDropIndexRecord& candidate = candidates.records[index];
        if (candidate.kind == RoomDropKind::equipment) {
            if (candidate.ordinal >= ground_items_.size()) return false;
            const GroundItem& ground = ground_items_[candidate.ordinal];
            if (!ground.active) continue;
            GroundItemSnapshot packed{};
            packed.ordinal = ground.drop_ordinal;
            packed.source = ground.source;
            packed.abyss_reward_ordinal = ground.abyss_reward_ordinal;
            packed.position = ground.position;
            packed.item_id = ground.item.id;
            packed.base_id = ground.item.base_id;
            packed.item_level = ground.item.item_level;
            const items::BaseDefinition* const base =
                items::base_definition(ground.item.base_id);
            if (base != nullptr) packed.slot = base->slot;
            packed.rarity = ground.item.rarity;
            retain_equipment(packed, query_center, output);
            continue;
        }
        if (candidate.kind == RoomDropKind::material) {
            if (candidate.ordinal >= ground_materials_.size()) return false;
            const GroundMaterial& ground =
                ground_materials_[candidate.ordinal];
            if (!ground.active) continue;
            if (output.material_count >= output.materials.size()) return false;
            output.materials[output.material_count++] = {
                ground.ordinal, ground.source, ground.position,
                ground.material};
            continue;
        }
        if (candidate.kind == RoomDropKind::health_potion) {
            const std::uint16_t spawn = static_cast<std::uint16_t>(
                candidate.ordinal / 2U);
            if (spawn >= ground_health_potions_.size()) return false;
            const GroundHealthPotion& ground = ground_health_potions_[spawn];
            if (!ground.active) continue;
            if (output.health_potion_count
                    >= output.health_potions.size()) return false;
            output.health_potions[output.health_potion_count++] = {
                ground.spawn_ordinal, ground.claim_ordinal, ground.position};
            continue;
        }
        return false;
    }

    const std::array<bool, 4U> abyss_doors =
        phase_ == RoomPhase::death_pending
        ? std::array<bool, 4U>{}
        : preview_abyss_doors(stable_state_.current_room);
    for (std::size_t index = 0U; index < output.doors.size(); ++index) {
        output.doors[index] = {
            kDoorPositions[index], kDoorDirections[index],
            room_progress_.exits_unlocked,
            abyss_doors[index],
        };
    }
    output.hole = {
        {0.0F, 3.5F, 0.0F},
        stable_state_.current_room.has_hole,
        room_progress_.exits_unlocked,
    };
    return true;
}

}  // namespace arpg::dungeon
