#include "combat/combat_world.hpp"

#include "combat/attack_catalog.hpp"
#include "combat/monster_catalog.hpp"

#include <array>

namespace arpg::combat {
namespace {

constexpr std::array<int, kDummyCount> kDummyHitPoints{{300, 450, 700}};
constexpr std::array<int, kDummyCount> kDummyBreakValues{{0, 0, 120}};
constexpr int kStage4PlayerMaxHp = 1000;
constexpr std::uint16_t kPlayerHurtTicks = 12;
constexpr std::uint16_t kPlayerInvulnerabilityTicks = 30;

}  // namespace

CombatWorld::CombatWorld(CombatLabConfig config) noexcept
    : legacy_config_(config), legacy_mode_(true) {
    encounter_config_.player_spawn = config.player_spawn;
    encounter_config_.initial_facing = config.initial_facing;
    initialize_runtime();
}

CombatWorld::CombatWorld(CombatEncounterConfig config) noexcept
    : encounter_config_(config), legacy_mode_(false) {
    initialize_runtime();
}

bool CombatWorld::queue_action(Action action) noexcept {
    return input_buffer_.push(action);
}

void CombatWorld::tick(MovementInput movement) noexcept {
    const bool player_frozen = player_.hit_stop_ticks != 0;
    const bool player_hurt = player_.hurt_ticks != 0;
    if (player_frozen) {
        --player_.hit_stop_ticks;
    }
    if (player_hurt) {
        --player_.hurt_ticks;
    }
    if (!player_frozen && !player_hurt) {
        simulate_player(movement);
    }

    if (player_.invulnerability_ticks != 0) {
        --player_.invulnerability_ticks;
    }

    for (MonsterRuntime& monster : monsters_.slots_) {
        if (!monster.active) {
            continue;
        }
        const bool dummy_frozen = monster.hit_stop_ticks != 0;
        if (dummy_frozen) {
            --monster.hit_stop_ticks;
        } else {
            const std::size_t index = static_cast<std::size_t>(
                &monster - monsters_.slots_.data());
            if (legacy_mode_) {
                simulate_target(index);
            } else {
                simulate_monster(index);
            }
        }
    }

    if (!player_frozen && !player_hurt && player_.hurt_ticks == 0) {
        resolve_attack_hits();
    }

    input_buffer_.age(player_frozen || player_hurt);
    ++tick_;
}

void CombatWorld::reset() noexcept {
    initialize_runtime();

    CombatEvent reset_event{};
    reset_event.kind = CombatEventKind::reset;
    reset_event.tick = 0;
    reset_event.position = player_.position;
    emit_event(reset_event);
}

void CombatWorld::initialize_runtime() noexcept {
    initialize_player();
    monsters_.clear();
    if (legacy_mode_) {
        initialize_legacy_monsters();
    } else {
        static_cast<void>(load_wave(
            encounter_config_.wave, encounter_config_.reset_player_health));
    }

    attack_ = AttackRuntime{};
    input_buffer_.clear();
    input_buffer_.reset_diagnostics();
    while (events_.try_pop().has_value()) {
    }
    tick_ = 0;
    event_overflow_count_ = 0;
}

void CombatWorld::initialize_player() noexcept {
    player_ = PlayerRuntime{};
    player_.position = encounter_config_.player_spawn;
    player_.facing = encounter_config_.initial_facing;
    player_.max_hp = kStage4PlayerMaxHp;
    player_.hp = player_.max_hp;
}

void CombatWorld::initialize_legacy_monsters() noexcept {
    for (std::size_t index = 0; index < kDummyCount; ++index) {
        const auto handle = monsters_.spawn(
            index == 0U ? MonsterId::fire_bomber
                        : index == 1U ? MonsterId::fire_charger
                                      : MonsterId::water_bulwark,
            legacy_config_.dummy_spawns[index]);
        if (!handle.has_value()) {
            continue;
        }
        MonsterRuntime* monster = monsters_.get(*handle);
        if (monster == nullptr) {
            continue;
        }
        monster->kind = static_cast<DummyKind>(index);
        monster->max_hp = kDummyHitPoints[index];
        monster->hp = monster->max_hp;
        monster->max_break = kDummyBreakValues[index];
        monster->break_value = monster->max_break;
        monster->armor = index == 2U ? ArmorState::armored : ArmorState::none;
    }
}

bool CombatWorld::load_wave(
    const EncounterWave& wave,
    bool reset_player_health) noexcept {
    if (wave.spawn_count > kEncounterSpawnCapacity) {
        return false;
    }

    for (std::size_t index = 0; index < wave.spawn_count; ++index) {
        if (monster_definition(wave.spawns[index].id) == nullptr) {
            return false;
        }
    }

    monsters_.clear();
    for (std::size_t index = 0; index < wave.spawn_count; ++index) {
        const auto handle = monsters_.spawn(
            wave.spawns[index].id, wave.spawns[index].position);
        if (!handle.has_value()) {
            monsters_.clear();
            return false;
        }
    }
    attack_ = AttackRuntime{};
    input_buffer_.clear();
    input_buffer_.reset_diagnostics();
    while (events_.try_pop().has_value()) {
    }
    event_overflow_count_ = 0U;
    encounter_config_.wave = wave;
    encounter_config_.reset_player_health = reset_player_health;
    legacy_mode_ = false;
    if (reset_player_health) {
        const bool health_changed = player_.hp != player_.max_hp
                                 || player_.hurt_ticks != 0
                                 || player_.invulnerability_ticks != 0;
        player_.hp = player_.max_hp;
        player_.hurt_ticks = 0;
        player_.invulnerability_ticks = 0;

        if (health_changed) {
            CombatEvent health_reset{};
            health_reset.kind = CombatEventKind::player_health_reset;
            health_reset.tick = tick_;
            health_reset.position = player_.position;
            health_reset.value = player_.max_hp;
            emit_event(health_reset);
        }
    }
    return true;
}

std::size_t CombatWorld::active_monster_count() const noexcept {
    return monsters_.active_count();
}

bool CombatWorld::destroy_monster(MonsterHandle handle) noexcept {
    if (!monsters_.destroy(handle)) {
        return false;
    }
    if (handle.index < attack_.hit_targets.size()) {
        attack_.hit_targets[handle.index] = false;
    }
    return true;
}

std::optional<CombatEvent> CombatWorld::try_pop_event() noexcept {
    return events_.try_pop();
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
        player_.hp,
        player_.max_hp,
        player_.hurt_ticks,
        player_.invulnerability_ticks,
    };

    for (std::size_t index = 0; index < monsters_.slots().size(); ++index) {
        const MonsterRuntime& dummy = monsters_.slots()[index];
        result.monsters[index] = MonsterSnapshot{
            dummy.active,
            dummy.generation,
            dummy.id,
            dummy.spawn,
            dummy.position,
            dummy.velocity,
            dummy.kind,
            dummy.facing,
            dummy.reaction,
            dummy.armor,
            dummy.hp,
            dummy.max_hp,
            dummy.break_value,
            dummy.max_break,
            dummy.break_window_ticks,
            dummy.hit_stop_ticks,
            dummy.ai_phase,
        };
    }

    result.monster_count = monsters_.active_count();
    std::size_t compatibility_index = 0U;
    for (std::size_t index = 0;
         index < monsters_.slots().size()
             && compatibility_index < kDummyCount;
         ++index) {
        if (!result.monsters[index].active) {
            continue;
        }
        result.dummies[compatibility_index] = result.monsters[index];
        ++compatibility_index;
    }

    result.diagnostics = CombatDiagnostics{
        input_buffer_.size(),
        input_buffer_.expired_count(),
        input_buffer_.overflow_count(),
        event_overflow_count_,
    };
    return result;
}

void CombatWorld::apply_player_damage(
    int damage,
    Vec3 source_position,
    FeedbackLevel feedback) noexcept {
    if (damage <= 0 || player_.invulnerability_ticks != 0) {
        return;
    }

    player_.hp = damage >= player_.hp ? 1 : player_.hp - damage;
    player_.hurt_ticks = kPlayerHurtTicks;
    player_.invulnerability_ticks = kPlayerInvulnerabilityTicks;
    player_.velocity.x = 0.0F;
    player_.velocity.y = 0.0F;

    CombatEvent hit{};
    hit.kind = CombatEventKind::player_hit;
    hit.tick = tick_;
    hit.hit_count = 1;
    hit.feedback = feedback;
    hit.position = source_position;
    hit.value = damage;
    emit_event(hit);

    CombatEvent hurt_started{};
    hurt_started.kind = CombatEventKind::player_hurt_started;
    hurt_started.tick = tick_;
    hurt_started.hit_count = 1;
    hurt_started.feedback = feedback;
    hurt_started.position = source_position;
    hurt_started.value = damage;
    emit_event(hurt_started);
}

}  // namespace arpg::combat
