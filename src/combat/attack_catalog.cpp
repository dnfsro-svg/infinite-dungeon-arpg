#include "combat/attack_catalog.hpp"

#include <array>

namespace arpg::combat {
namespace {

constexpr std::array<AttackDefinition, kAttackCount> kAttackDefinitions{{
    {AttackId::j1,       5, 3,  9, 28, 10, ImpactKind::light_hitstun,
     {{0.20F,-0.65F, 0.10F},{1.50F,0.65F,1.50F}},0.10F,0.0F,0.0F,FeedbackLevel::light},
    {AttackId::j2,       6, 3, 10, 34, 12, ImpactKind::medium_hitstun,
     {{0.20F,-0.65F, 0.10F},{1.65F,0.65F,1.55F}},0.14F,2.2F,0.0F,FeedbackLevel::medium},
    {AttackId::j3,       8, 4, 16, 52, 20, ImpactKind::knockdown,
     {{0.15F,-0.70F, 0.05F},{1.90F,0.70F,1.65F}},0.20F,5.0F,0.0F,FeedbackLevel::heavy},
    {AttackId::heavy,   14, 5, 22, 90, 40, ImpactKind::knockdown,
     {{0.10F,-0.75F, 0.00F},{2.20F,0.75F,1.75F}},0.24F,7.0F,0.0F,FeedbackLevel::heavy},
    {AttackId::launcher, 7, 4, 17, 38, 18, ImpactKind::launch,
     {{0.10F,-0.65F, 0.00F},{1.40F,0.65F,1.90F}},0.12F,1.2F,9.5F,FeedbackLevel::medium},
    {AttackId::air_j,    4, 5, 12, 42, 15, ImpactKind::medium_hitstun,
     {{0.10F,-0.65F,-0.40F},{1.60F,0.65F,1.20F}},0.08F,2.5F,0.0F,FeedbackLevel::medium},
}};

constexpr bool has_valid_feedback(FeedbackLevel feedback) noexcept {
    return feedback == FeedbackLevel::light
        || feedback == FeedbackLevel::medium
        || feedback == FeedbackLevel::heavy;
}

constexpr bool has_inverted_hitbox(const Aabb& hitbox) noexcept {
    return hitbox.minimum.x > hitbox.maximum.x
        || hitbox.minimum.y > hitbox.maximum.y
        || hitbox.minimum.z > hitbox.maximum.z;
}

constexpr bool catalog_is_valid() noexcept {
    for (std::size_t index = 0; index < kAttackDefinitions.size(); ++index) {
        const auto& definition = kAttackDefinitions[index];
        if (definition.startup_ticks == 0
                || definition.active_ticks == 0
                || definition.recovery_ticks == 0
                || definition.damage <= 0
                || definition.break_damage < 0
                || has_inverted_hitbox(definition.local_hitbox)
                || !has_valid_feedback(definition.feedback)) {
            return false;
        }

        for (std::size_t other = index + 1;
             other < kAttackDefinitions.size();
             ++other) {
            if (definition.id == kAttackDefinitions[other].id) {
                return false;
            }
        }
    }
    return true;
}

}  // namespace

const AttackDefinition* find_attack_definition(AttackId id) noexcept {
    for (const auto& definition : kAttackDefinitions) {
        if (definition.id == id) {
            return &definition;
        }
    }
    return nullptr;
}

AttackPhase attack_phase_at(
    const AttackDefinition& definition,
    std::uint32_t elapsed_ticks) noexcept {
    const auto active_start =
        static_cast<std::uint32_t>(definition.startup_ticks);
    const auto recovery_start =
        active_start + static_cast<std::uint32_t>(definition.active_ticks);
    const auto finished_start =
        recovery_start + static_cast<std::uint32_t>(definition.recovery_ticks);

    if (elapsed_ticks < active_start) {
        return AttackPhase::startup;
    }
    if (elapsed_ticks < recovery_start) {
        return AttackPhase::active;
    }
    if (elapsed_ticks < finished_start) {
        return AttackPhase::recovery;
    }
    return AttackPhase::finished;
}

bool validate_attack_catalog() noexcept {
    return catalog_is_valid();
}

}  // namespace arpg::combat
