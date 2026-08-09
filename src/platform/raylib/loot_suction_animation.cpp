#include "loot_suction_animation.hpp"

#include <algorithm>
#include <cmath>

namespace arpg::platform {
namespace {

constexpr float kFlightDurationSeconds = 0.45F;
constexpr float kDestinationPulseSeconds = 0.16F;
constexpr float kArcHeightPixels = 46.0F;
constexpr float kWaistOffsetPixels = 44.0F;
constexpr float kBackOffsetPixels = 18.0F;
constexpr float kPi = 3.14159265358979323846F;

[[nodiscard]] bool committed(const DungeonRenderStatus& status) noexcept {
    return status.indicator == SaveIndicator::saved
        && !status.recovery_required && !status.faulted;
}

[[nodiscard]] bool contains_equipment(const dungeon::DungeonSnapshot& snapshot,
    std::uint64_t item_id) noexcept {
    const std::size_t count = (std::min)(
        static_cast<std::size_t>(snapshot.ground_item_count),
        snapshot.ground_items.size());
    for (std::size_t index{}; index < count; ++index) {
        if (snapshot.ground_items[index].item_id == item_id) return true;
    }
    return false;
}

[[nodiscard]] const dungeon::GroundItemSnapshot* find_equipment(
    const dungeon::DungeonSnapshot& snapshot, std::uint64_t item_id) noexcept {
    const std::size_t count = (std::min)(
        static_cast<std::size_t>(snapshot.ground_item_count),
        snapshot.ground_items.size());
    for (std::size_t index{}; index < count; ++index) {
        const dungeon::GroundItemSnapshot& item = snapshot.ground_items[index];
        if (item.item_id == item_id) return &item;
    }
    return nullptr;
}

[[nodiscard]] bool contains_material(const dungeon::DungeonSnapshot& snapshot,
    std::uint16_t ordinal) noexcept {
    const std::size_t count = (std::min)(
        static_cast<std::size_t>(snapshot.ground_material_count),
        snapshot.ground_materials.size());
    for (std::size_t index{}; index < count; ++index) {
        if (snapshot.ground_materials[index].ordinal == ordinal) return true;
    }
    return false;
}

[[nodiscard]] bool same_room(const dungeon::DungeonSnapshot& previous,
    const dungeon::DungeonSnapshot& current) noexcept {
    return previous.room_index == current.room_index
        && previous.room_instance_generation == current.room_instance_generation;
}

[[nodiscard]] float smoothstep(float value) noexcept {
    const float clamped = std::clamp(value, 0.0F, 1.0F);
    return clamped * clamped * (3.0F - 2.0F * clamped);
}

}  // namespace

void LootSuctionState::observe(const dungeon::DungeonSnapshot& previous,
    const dungeon::DungeonSnapshot& current,
    const DungeonRenderStatus& status) noexcept {
    const LootPickupReceipt& equipment_receipt = status.loot_pickup;
    const dungeon::MaterialPickupReceipt& material_receipt =
        current.material_pickup_receipt;
    if (!attached_) {
        attached_ = true;
        equipment_generation_ = equipment_receipt.commit_generation;
        equipment_item_id_ = equipment_receipt.item_id;
        material_generation_ = material_receipt.commit_generation;
        return;
    }

    if (!committed(status)) {
        if (equipment_receipt.commit_generation > equipment_generation_) {
            equipment_generation_ = equipment_receipt.commit_generation;
            equipment_item_id_ = equipment_receipt.item_id;
        }
        if (material_receipt.commit_generation > material_generation_) {
            material_generation_ = material_receipt.commit_generation;
        }
        return;
    }

    if (equipment_receipt.valid
            && equipment_receipt.commit_generation > equipment_generation_) {
        equipment_generation_ = equipment_receipt.commit_generation;
        equipment_item_id_ = equipment_receipt.item_id;
        const dungeon::GroundItemSnapshot* const item = find_equipment(
            previous, equipment_receipt.item_id);
        if (item != nullptr && !contains_equipment(current,
                equipment_receipt.item_id)) {
            for (LootSuctionFlight& flight : flights_) {
                if (flight.active) continue;
                flight = {item->position, {}, ground_loot_item_sprite(item->slot),
                    {255U, 255U, 255U, 255U}, 0.0F, true, true};
                break;
            }
        }
    }

    if (!material_receipt.valid
            || material_receipt.commit_generation <= material_generation_) {
        return;
    }
    material_generation_ = material_receipt.commit_generation;
    if (!same_room(previous, current)) return;

    std::array<std::uint64_t, items::kMaterialCount> remaining =
        material_receipt.counts;
    const std::size_t count = (std::min)(
        static_cast<std::size_t>(previous.ground_material_count),
        previous.ground_materials.size());
    for (std::size_t index{}; index < count; ++index) {
        const dungeon::GroundMaterialSnapshot& material =
            previous.ground_materials[index];
        const std::size_t material_index = items::material_index(material.material);
        if (material_index >= remaining.size() || remaining[material_index] == 0U
                || contains_material(current, material.ordinal)) {
            continue;
        }
        for (LootSuctionFlight& flight : flights_) {
            if (flight.active) continue;
            flight = {material.position, {}, material_loot_sprite(material.material),
                material_color(material.material), 0.0F, true, false};
            --remaining[material_index];
            break;
        }
    }
}

void LootSuctionState::update(float frame_seconds, bool paused) noexcept {
    if (paused || frame_seconds <= 0.0F) return;
    destination_pulse_seconds_ = (std::max)(0.0F,
        destination_pulse_seconds_ - frame_seconds);
    for (LootSuctionFlight& flight : flights_) {
        if (!flight.active) continue;
        flight.elapsed_seconds += frame_seconds;
        if (flight.elapsed_seconds < kFlightDurationSeconds) continue;
        flight.active = false;
        destination_pulse_seconds_ = kDestinationPulseSeconds;
    }
}

LootSuctionPlan LootSuctionState::build_plan(CombatCameraView camera,
    combat::Vec3 player_position, float width, float height) const noexcept {
    LootSuctionPlan plan{};
    const ScreenProjection player = project_combat_position(
        player_position, camera, width, height);
    plan.destination = {player.x - kBackOffsetPixels * player.scale,
        player.ground_y - kWaistOffsetPixels * player.scale};
    plan.destination_pulse = destination_pulse_seconds_ / kDestinationPulseSeconds;
    for (const LootSuctionFlight& source : flights_) {
        if (!source.active || plan.count == plan.flights.size()) continue;
        LootSuctionFlight flight = source;
        const ScreenProjection origin = project_combat_position(
            source.world_position, camera, width, height);
        const float elapsed = source.elapsed_seconds / kFlightDurationSeconds;
        const float progress = smoothstep(elapsed);
        const float arc = std::sin(kPi * std::clamp(elapsed, 0.0F, 1.0F))
            * kArcHeightPixels * origin.scale;
        flight.center = {origin.x + (plan.destination.x - origin.x) * progress,
            origin.y + (plan.destination.y - origin.y) * progress - arc};
        plan.flights[plan.count++] = flight;
    }
    return plan;
}

void LootSuctionState::clear() noexcept {
    flights_.fill({});
    equipment_generation_ = 0U;
    equipment_item_id_ = 0U;
    material_generation_ = 0U;
    destination_pulse_seconds_ = 0.0F;
    attached_ = false;
}

std::size_t LootSuctionState::active_count() const noexcept {
    std::size_t result{};
    for (const LootSuctionFlight& flight : flights_) {
        if (flight.active) ++result;
    }
    return result;
}

}  // namespace arpg::platform
