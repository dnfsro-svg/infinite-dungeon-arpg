#include "monster_material_presenter.hpp"

#include "combat_view_math.hpp"

namespace arpg::platform {
namespace {

[[nodiscard]] bool has_complete_material_animation(
    combat::MonsterId monster) noexcept {
    return monster == combat::MonsterId::water_bulwark
        || monster == combat::MonsterId::water_support
        || monster == combat::MonsterId::lightning_shooter
        || monster == combat::MonsterId::lightning_dasher
        || monster == combat::MonsterId::chaos_chaser
        || monster == combat::MonsterId::chaos_hazard;
}

}  // namespace

MonsterMaterialDrawPlan MonsterMaterialPresenter::collect_draw_plan(
    std::size_t slot_index, const combat::MonsterSnapshot& monster,
    std::uint64_t world_tick, bool hurt) noexcept {
    if (slot_index >= slots_.size()) return {};
    SlotState& slot = slots_[slot_index];
    if (!monster.active) {
        slot = {};
        return {};
    }
    if (!has_complete_material_animation(monster.id)) {
        slot = {};
        return {monster_visible(monster), false};
    }

    const MonsterAnimationState desired = select_monster_animation_state(
        monster.ai_phase, hurt);
    const bool identity_changed = !slot.occupied
        || slot.generation != monster.generation || slot.monster != monster.id;
    if (identity_changed) {
        slot = {true, monster.generation, monster.id, desired, world_tick};
    } else {
        const bool death_interrupt = desired == MonsterAnimationState::death
            && slot.state != MonsterAnimationState::death;
        const bool hurt_interrupt = desired == MonsterAnimationState::hurt
            && slot.state != MonsterAnimationState::death
            && slot.state != MonsterAnimationState::hurt;
        bool hurt_complete = true;
        if (slot.state == MonsterAnimationState::hurt) {
            const auto* clip = monster_animation_clip(monster.id, slot.state);
            if (clip != nullptr && clip->frames_per_second != 0U) {
                const std::uint64_t duration =
                    (static_cast<std::uint64_t>(clip->frame_count) * 60U
                        + clip->frames_per_second - 1U)
                    / clip->frames_per_second;
                hurt_complete = world_tick - slot.state_started_tick >= duration;
            }
        }
        const bool regular_transition = slot.state != desired
            && slot.state != MonsterAnimationState::death
            && (slot.state != MonsterAnimationState::hurt || hurt_complete);
        if (death_interrupt || hurt_interrupt || regular_transition) {
            slot.state = desired;
            slot.state_started_tick = world_tick;
        }
    }

    const MonsterAnimationClipDefinition* const clip = monster_animation_clip(
        monster.id, slot.state);
    if (clip == nullptr) return {};
    const bool loop = slot.state == MonsterAnimationState::idle
        || slot.state == MonsterAnimationState::move;
    const std::uint16_t frame_index = monster_animation_frame_index(*clip,
        world_tick - slot.state_started_tick, loop);
    return {true, true, slot.state, frame_index,
        monster_animation_frame(*clip, frame_index)};
}

void MonsterMaterialPresenter::reset() noexcept {
    slots_.fill(SlotState{});
}

}  // namespace arpg::platform
