#include "dungeon/room_drop_state.hpp"

#include <cstddef>

namespace arpg::dungeon {
namespace {

template <std::size_t Size>
[[nodiscard]] bool bit_is_set(
    const std::array<std::uint64_t, Size>& bits,
    const std::uint16_t ordinal) noexcept {
    const std::size_t word = ordinal / 64U;
    return word < bits.size()
        && (bits[word] & (std::uint64_t{1U} << (ordinal % 64U))) != 0U;
}

template <std::size_t Size>
[[nodiscard]] bool set_once(std::array<std::uint64_t, Size>& bits,
    const std::uint16_t ordinal) noexcept {
    const std::size_t word = ordinal / 64U;
    if (word >= bits.size()) return false;
    const std::uint64_t mask = std::uint64_t{1U} << (ordinal % 64U);
    if ((bits[word] & mask) != 0U) return false;
    bits[word] |= mask;
    return true;
}

[[nodiscard]] bool valid_material_slot(
    const GroundMaterial& material) noexcept {
    const bool reserve = material.ordinal >= kAbyssSecondaryOrdinalBegin;
    switch (material.source) {
    case GroundMaterialSource::monster_common:
        return !reserve && (material.ordinal & 1U) == 0U;
    case GroundMaterialSource::monster_coupon:
        return !reserve && (material.ordinal & 1U) != 0U;
    case GroundMaterialSource::abyss_reward:
        return reserve
            && material.ordinal < kAuthoritativeSecondaryDropCapacity;
    }
    return false;
}

}  // namespace

bool RoomDropState::reset(const combat::RoomMonsterPlan& plan) noexcept {
    clear();
    return spatial_index_.reset(plan);
}

void RoomDropState::clear() noexcept {
    equipment_ = {};
    materials_ = {};
    health_potions_ = {};
    equipment_claim_bits_ = {};
    secondary_claim_bits_ = {};
    health_potion_presence_bits_ = {};
    active_equipment_count_ = 0U;
    active_material_count_ = 0U;
    active_health_potion_count_ = 0U;
    spatial_index_.clear();
}

bool RoomDropState::place_equipment(const GroundItem& item) noexcept {
    const std::uint16_t ordinal = item.drop_ordinal;
    if (!item.active || ordinal >= equipment_.size()
            || equipment_[ordinal].active
            || equipment_claimed(ordinal)) {
        return false;
    }
    const bool inserted = item.source == GroundItemSource::abyss_chest
        ? spatial_index_.insert_abyss_reserve(
            RoomDropKind::equipment, ordinal, item.position)
        : ordinal < spatial_index_.monster_count()
            && spatial_index_.insert_monster_drop(RoomDropKind::equipment,
                ordinal, ordinal, item.position);
    if (!inserted) return false;
    equipment_[ordinal] = item;
    ++active_equipment_count_;
    return true;
}

bool RoomDropState::place_material(const GroundMaterial& material) noexcept {
    const std::uint16_t ordinal = material.ordinal;
    const bool ordinary_secondary = ordinal < kAbyssSecondaryOrdinalBegin
        && (ordinal & 1U) != 0U;
    const std::uint16_t secondary_spawn = static_cast<std::uint16_t>(
        ordinal / 2U);
    if (!material.active || ordinal >= materials_.size()
            || !valid_material_slot(material)
            || materials_[ordinal].active || secondary_claimed(ordinal)
            || (ordinary_secondary
                && secondary_spawn < health_potions_.size()
                && health_potions_[secondary_spawn].active)) {
        return false;
    }
    const bool reserve = ordinal >= kAbyssSecondaryOrdinalBegin;
    const bool inserted = reserve
        ? spatial_index_.insert_abyss_reserve(
            RoomDropKind::material, ordinal, material.position)
        : spatial_index_.insert_monster_drop(RoomDropKind::material,
            ordinal, static_cast<std::uint16_t>(ordinal / 2U),
            material.position);
    if (!inserted) return false;
    materials_[ordinal] = material;
    ++active_material_count_;
    return true;
}

bool RoomDropState::place_health_potion(
    const GroundHealthPotion& potion) noexcept {
    const std::uint16_t spawn = potion.spawn_ordinal;
    if (!potion.active || spawn >= health_potions_.size()
            || spawn >= spatial_index_.monster_count()
            || potion.claim_ordinal != secondary_drop_ordinal(spawn)
            || health_potions_[spawn].active
            || materials_[potion.claim_ordinal].active
            || secondary_claimed(potion.claim_ordinal)
            || !spatial_index_.insert_monster_drop(
                RoomDropKind::health_potion, potion.claim_ordinal,
                spawn, potion.position)) {
        return false;
    }
    health_potions_[spawn] = potion;
    health_potion_presence_bits_[spawn / 64U] |=
        std::uint64_t{1U} << (spawn % 64U);
    ++active_health_potion_count_;
    return true;
}

bool RoomDropState::can_place_equipment(
    const GroundItem& item) const noexcept {
    const std::uint16_t ordinal = item.drop_ordinal;
    if (!item.active || ordinal >= equipment_.size()
            || equipment_[ordinal].active
            || equipment_claimed(ordinal)) {
        return false;
    }
    return item.source == GroundItemSource::abyss_chest
        ? spatial_index_.can_insert_abyss_reserve(
            RoomDropKind::equipment, ordinal, item.position)
        : ordinal < spatial_index_.monster_count()
            && spatial_index_.can_insert_monster_drop(
                RoomDropKind::equipment, ordinal, ordinal, item.position);
}

bool RoomDropState::can_mark_equipment_claimed(
    const std::uint16_t ordinal) const noexcept {
    return ordinal < equipment_.size() && equipment_[ordinal].active
        && !equipment_claimed(ordinal)
        && spatial_index_.has_present_record(
            RoomDropKind::equipment, ordinal);
}

bool RoomDropState::can_mark_secondary_claimed(
    const std::uint16_t ordinal) const noexcept {
    if (ordinal >= materials_.size() || secondary_claimed(ordinal)) {
        return false;
    }
    if (materials_[ordinal].active) {
        return spatial_index_.has_present_record(
            RoomDropKind::material, ordinal);
    }
    if (ordinal >= kAbyssSecondaryOrdinalBegin
            || (ordinal & 1U) == 0U) return false;
    const std::uint16_t spawn = static_cast<std::uint16_t>(ordinal / 2U);
    return spawn < health_potions_.size()
        && health_potions_[spawn].active
        && health_potions_[spawn].claim_ordinal == ordinal
        && spatial_index_.has_present_record(
            RoomDropKind::health_potion, ordinal);
}

bool RoomDropState::mark_equipment_claimed(
    const std::uint16_t ordinal) noexcept {
    if (!can_mark_equipment_claimed(ordinal)
            || !set_once(equipment_claim_bits_, ordinal)) {
        return false;
    }
    if (!spatial_index_.set_present(
            RoomDropKind::equipment, ordinal, false)) {
        const std::size_t word = ordinal / 64U;
        equipment_claim_bits_[word] &=
            ~(std::uint64_t{1U} << (ordinal % 64U));
        return false;
    }
    equipment_[ordinal] = {};
    --active_equipment_count_;
    return true;
}

bool RoomDropState::mark_secondary_claimed(
    const std::uint16_t ordinal) noexcept {
    if (!can_mark_secondary_claimed(ordinal)
            || !set_once(secondary_claim_bits_, ordinal)) return false;
    RoomDropKind kind{};
    std::uint16_t potion_spawn = 0xFFFFU;
    if (materials_[ordinal].active) {
        kind = RoomDropKind::material;
    } else if (ordinal < kAbyssSecondaryOrdinalBegin
            && (ordinal & 1U) != 0U) {
        const std::uint16_t spawn = static_cast<std::uint16_t>(ordinal / 2U);
        if (spawn < health_potions_.size()
                && health_potions_[spawn].active
                && health_potions_[spawn].claim_ordinal == ordinal) {
            kind = RoomDropKind::health_potion;
            potion_spawn = spawn;
        }
    }
    if ((kind != RoomDropKind::material
            && kind != RoomDropKind::health_potion)
            || !spatial_index_.set_present(kind, ordinal, false)) {
        const std::size_t word = ordinal / 64U;
        secondary_claim_bits_[word] &=
            ~(std::uint64_t{1U} << (ordinal % 64U));
        return false;
    }
    if (kind == RoomDropKind::material) {
        materials_[ordinal] = {};
        --active_material_count_;
    } else {
        health_potions_[potion_spawn] = {};
        health_potion_presence_bits_[potion_spawn / 64U] &=
            ~(std::uint64_t{1U} << (potion_spawn % 64U));
        --active_health_potion_count_;
    }
    return true;
}

bool RoomDropState::equipment_claimed(
    const std::uint16_t ordinal) const noexcept {
    return bit_is_set(equipment_claim_bits_, ordinal);
}

bool RoomDropState::secondary_claimed(
    const std::uint16_t ordinal) const noexcept {
    return bit_is_set(secondary_claim_bits_, ordinal);
}

std::uint16_t RoomDropState::first_health_potion_spawn() const noexcept {
    for (std::size_t word = 0U;
            word < health_potion_presence_bits_.size(); ++word) {
        const std::uint64_t bits = health_potion_presence_bits_[word];
        if (bits == 0U) continue;
        for (std::uint16_t bit = 0U; bit < 64U; ++bit) {
            if ((bits & (std::uint64_t{1U} << bit)) == 0U) continue;
            const std::size_t spawn = word * 64U + bit;
            return spawn < health_potions_.size()
                ? static_cast<std::uint16_t>(spawn) : std::uint16_t{0xFFFFU};
        }
    }
    return 0xFFFFU;
}

void RoomDropState::restore_claim_bits(
    const std::array<std::uint64_t, limits::kRoomEquipmentClaimWords>& equipment,
    const std::array<std::uint64_t, limits::kRoomSecondaryClaimWords>& secondary)
    noexcept {
    equipment_claim_bits_ = equipment;
    secondary_claim_bits_ = secondary;
}

}  // namespace arpg::dungeon
