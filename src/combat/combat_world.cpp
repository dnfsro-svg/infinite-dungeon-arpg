#include "combat/combat_world.hpp"

#include "combat/attack_catalog.hpp"

#include <array>

namespace arpg::combat {
namespace {

constexpr std::array<int, kDummyCount> kDummyHitPoints{{300, 450, 700}};
constexpr std::array<int, kDummyCount> kDummyBreakValues{{0, 0, 120}};

}  // namespace

CombatWorld::CombatWorld(CombatLabConfig config) noexcept : config_(config) {
    reset();
}

bool CombatWorld::queue_action(Action action) noexcept {
    return input_buffer_.push(action);
}

void CombatWorld::tick(MovementInput movement) noexcept {
    ++tick_;
    if (player_.hit_stop_ticks != 0) {
        --player_.hit_stop_ticks;
        input_buffer_.age(true);
        return;
    }

    simulate_player(movement);
    input_buffer_.age(player_.hit_stop_ticks != 0);
}

void CombatWorld::reset() noexcept {
    player_ = PlayerRuntime{};
    player_.position = config_.player_spawn;

    for (std::size_t index = 0; index < dummies_.size(); ++index) {
        DummyRuntime dummy{};
        dummy.kind = static_cast<DummyKind>(index);
        dummy.spawn = config_.dummy_spawns[index];
        dummy.position = dummy.spawn;
        dummy.max_hp = kDummyHitPoints[index];
        dummy.hp = dummy.max_hp;
        dummy.max_break = kDummyBreakValues[index];
        dummy.break_value = dummy.max_break;
        dummy.armor = index == 2 ? ArmorState::armored : ArmorState::none;
        dummies_[index] = dummy;
    }

    attack_ = AttackRuntime{};
    input_buffer_.clear();
    input_buffer_.reset_diagnostics();
    tick_ = 0;
    event_overflow_count_ = 0;
}

CombatSnapshot CombatWorld::snapshot() const noexcept {
    CombatSnapshot result{};
    result.tick = tick_;
    result.player = PlayerSnapshot{
        player_.position,
        player_.velocity,
        player_.facing,
        player_.state,
        attack_.id,
        attack_.id == AttackId::none
            ? AttackPhase::finished
            : attack_phase_at(
                  *find_attack_definition(attack_.id), attack_.elapsed_ticks),
        attack_.elapsed_ticks,
        player_.combo_stage,
        player_.hit_stop_ticks,
        player_.air_attack_available,
    };

    for (std::size_t index = 0; index < dummies_.size(); ++index) {
        const DummyRuntime& dummy = dummies_[index];
        result.dummies[index] = DummySnapshot{
            dummy.position,
            dummy.velocity,
            dummy.kind,
            dummy.reaction,
            dummy.armor,
            dummy.hp,
            dummy.max_hp,
            dummy.break_value,
            dummy.max_break,
            dummy.break_window_ticks,
            dummy.hit_stop_ticks,
        };
    }

    result.diagnostics = CombatDiagnostics{
        input_buffer_.size(),
        input_buffer_.expired_count(),
        input_buffer_.overflow_count(),
        event_overflow_count_,
    };
    return result;
}

}  // namespace arpg::combat
