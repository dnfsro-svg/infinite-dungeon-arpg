#include "combat/combat_world.hpp"
#include "combat/room_bounds.hpp"
#include "modifiers/modifier_math.hpp"

#include <algorithm>
#include <cstdint>

namespace arpg::combat {
namespace {

constexpr float kTickSeconds = 1.0F / 60.0F;
constexpr float kGravity = 24.0F;
constexpr std::uint16_t kLightHitstunTicks = 10;
constexpr std::uint16_t kMediumHitstunTicks = 16;
constexpr std::uint16_t kKnockdownTicks = 45;
constexpr std::uint16_t kRisingTicks = 30;
constexpr std::uint16_t kRespawnTicks = 90;
constexpr std::uint16_t kLauncherHoverTicks = 30;

float impulse_scale(DummyKind kind) noexcept {
    modifiers::Modifier light_scale{1U, modifiers::StatId::impulse_scale,
        modifiers::ModifierOperation::more, 12500};
    light_scale.required_tags = modifiers::tag(
        modifiers::ModifierTag::light_target);
    modifiers::ModifierContext context{};
    if (kind == DummyKind::light) {
        context.tags = modifiers::tag(modifiers::ModifierTag::light_target);
    }
    const std::array<modifiers::Modifier, 1> values{{light_scale}};
    const auto result = modifiers::evaluate_stat(
        modifiers::kFixedOne, modifiers::StatId::impulse_scale,
        values, context, {0, 100000});
    return static_cast<float>(result.value)
        / static_cast<float>(modifiers::kFixedOne);
}

std::uint16_t reaction_ticks(
    DummyKind kind,
    std::uint16_t base_ticks) noexcept {
    if (kind != DummyKind::light) {
        return base_ticks;
    }
    const std::uint32_t scaled =
        static_cast<std::uint32_t>(base_ticks) * 5U;
    return static_cast<std::uint16_t>((scaled + 3U) / 4U);
}

}  // namespace

void CombatWorld::apply_dummy_impact(
    std::size_t index,
    const AttackDefinition& definition) noexcept {
    MonsterRuntime& dummy = monsters_.slots_[index];
    if (dummy.hp == 0) {
        dummy.reaction = ReactionState::defeated;
        dummy.ai_phase = MonsterAiPhase::defeated;
        dummy.reaction_ticks = kRespawnTicks;
        dummy.break_window_ticks = 0;
        dummy.velocity = Vec3{};

        CombatEvent defeated{};
        defeated.kind = CombatEventKind::defeated;
        defeated.tick = tick_;
        defeated.attack = definition.id;
        defeated.target_index = static_cast<std::uint8_t>(index);
        defeated.feedback = definition.feedback;
        defeated.position = dummy.position;
        emit_event(defeated);
        return;
    }

    const bool melee_ai = dummy.id == MonsterId::chaos_chaser
                       || dummy.id == MonsterId::water_bulwark;
    dummy.ai_phase = !legacy_mode_ && !melee_ai
                       ? MonsterAiPhase::idle
                       : MonsterAiPhase::move;
    dummy.ai_ticks = 0;
    dummy.contact_attack_resolved = true;

    const float scale = impulse_scale(dummy.kind);
    const float facing = player_.facing == Facing::right ? 1.0F : -1.0F;
    const bool already_airborne =
        dummy.reaction == ReactionState::airborne || dummy.position.z > 0.0F;

    switch (definition.impact) {
    case ImpactKind::light_hitstun:
        dummy.velocity.x = 0.0F;
        if (already_airborne) {
            dummy.reaction = ReactionState::airborne;
            dummy.reaction_ticks = 0;
        } else {
            dummy.velocity.z = 0.0F;
            dummy.reaction = ReactionState::hitstun;
            dummy.reaction_ticks = reaction_ticks(
                dummy.kind, kLightHitstunTicks);
        }
        break;
    case ImpactKind::medium_hitstun:
        dummy.velocity.x =
            facing * definition.knockback_speed * scale;
        if (already_airborne) {
            dummy.reaction = ReactionState::airborne;
            dummy.reaction_ticks = 0;
        } else {
            dummy.velocity.z = 0.0F;
            dummy.reaction = ReactionState::hitstun;
            dummy.reaction_ticks = reaction_ticks(
                dummy.kind, kMediumHitstunTicks);
        }
        break;
    case ImpactKind::knockdown:
        dummy.velocity.x =
            facing * definition.knockback_speed * scale;
        if (already_airborne) {
            dummy.reaction = ReactionState::airborne;
            dummy.reaction_ticks = 0;
        } else {
            dummy.velocity.z = 0.0F;
            dummy.reaction = ReactionState::knockdown;
            dummy.reaction_ticks = reaction_ticks(
                dummy.kind, kKnockdownTicks);
        }
        break;
    case ImpactKind::launch:
        dummy.velocity.x =
            facing * definition.knockback_speed * scale;
        dummy.velocity.z = definition.launch_speed * scale;
        dummy.reaction = ReactionState::airborne;
        dummy.reaction_ticks = kLauncherHoverTicks;
        break;
    }
}

void CombatWorld::respawn_dummy(std::size_t index) noexcept {
    MonsterRuntime& dummy = monsters_.slots_[index];
    dummy.position = dummy.spawn;
    dummy.velocity = Vec3{};
    dummy.reaction = ReactionState::respawning;
    dummy.armor = dummy.kind == DummyKind::heavy
                      ? ArmorState::armored
                      : ArmorState::none;
    dummy.reaction_ticks = 0;
    dummy.break_window_ticks = 0;
    dummy.hit_stop_ticks = 0;
    dummy.hp = dummy.max_hp;
    dummy.break_value = dummy.max_break;

    CombatEvent respawned{};
    respawned.kind = CombatEventKind::respawned;
    respawned.tick = tick_;
    respawned.target_index = static_cast<std::uint8_t>(index);
    respawned.position = dummy.position;
    emit_event(respawned);
}

void CombatWorld::simulate_target(std::size_t index) noexcept {
    MonsterRuntime& dummy = monsters_.slots_[index];

    if (dummy.armor == ArmorState::broken
        && dummy.reaction != ReactionState::defeated
        && dummy.reaction != ReactionState::respawning
        && dummy.break_window_ticks != 0) {
        --dummy.break_window_ticks;
        if (dummy.break_window_ticks == 0) {
            dummy.armor = ArmorState::armored;
            dummy.break_value = dummy.max_break;
        }
    }

    const auto integrate_horizontal = [&dummy]() noexcept {
        dummy.position.x += dummy.velocity.x * kTickSeconds;
        if (dummy.position.x <= room_bounds::min_x) {
            dummy.position.x = room_bounds::min_x;
            if (dummy.velocity.x < 0.0F) {
                dummy.velocity.x = 0.0F;
            }
        } else if (dummy.position.x >= room_bounds::max_x) {
            dummy.position.x = room_bounds::max_x;
            if (dummy.velocity.x > 0.0F) {
                dummy.velocity.x = 0.0F;
            }
        }
    };

    switch (dummy.reaction) {
    case ReactionState::idle:
        dummy.velocity = Vec3{};
        return;
    case ReactionState::hitstun:
        integrate_horizontal();
        if (dummy.reaction_ticks != 0) {
            --dummy.reaction_ticks;
        }
        if (dummy.reaction_ticks == 0) {
            dummy.reaction = ReactionState::idle;
            dummy.velocity = Vec3{};
        }
        return;
    case ReactionState::airborne:
        integrate_horizontal();
        if (dummy.velocity.z <= 0.0F && dummy.reaction_ticks != 0) {
            --dummy.reaction_ticks;
            dummy.velocity.z = 0.0F;
        } else {
            dummy.position.z += dummy.velocity.z * kTickSeconds;
            dummy.velocity.z -= kGravity * kTickSeconds;
        }
        if (dummy.position.z <= 0.0F) {
            dummy.position.z = 0.0F;
            dummy.velocity.z = 0.0F;
            dummy.reaction = ReactionState::knockdown;
            dummy.reaction_ticks = reaction_ticks(
                dummy.kind, kKnockdownTicks);

            CombatEvent landing{};
            landing.kind = CombatEventKind::landing;
            landing.tick = tick_;
            landing.target_index = static_cast<std::uint8_t>(index);
            landing.position = dummy.position;
            emit_event(landing);
        }
        return;
    case ReactionState::knockdown:
        integrate_horizontal();
        if (dummy.reaction_ticks != 0) {
            --dummy.reaction_ticks;
        }
        if (dummy.reaction_ticks == 0) {
            dummy.reaction = ReactionState::rising;
            dummy.reaction_ticks = reaction_ticks(
                dummy.kind, kRisingTicks);
            dummy.velocity = Vec3{};
        }
        return;
    case ReactionState::rising:
        if (dummy.reaction_ticks != 0) {
            --dummy.reaction_ticks;
        }
        if (dummy.reaction_ticks == 0) {
            dummy.reaction = ReactionState::idle;
            dummy.velocity = Vec3{};
        }
        return;
    case ReactionState::defeated:
        if (!legacy_mode_ || !legacy_config_.respawn_defeated_dummies) {
            return;
        }
        if (dummy.reaction_ticks != 0) {
            --dummy.reaction_ticks;
        }
        if (dummy.reaction_ticks == 0) {
            respawn_dummy(index);
        }
        return;
    case ReactionState::respawning:
        dummy.reaction = ReactionState::idle;
        return;
    }
}

}  // namespace arpg::combat
