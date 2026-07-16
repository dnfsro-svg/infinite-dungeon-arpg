#include "abyss/abyss_rules.hpp"
#include "dungeon/dungeon_progression.hpp"
#include "dungeon/dungeon_session.hpp"
#include "dungeon/room_generation.hpp"
#include "items/item_generation.hpp"
#include "persistence/save_store.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <optional>

namespace {

using arpg::combat::Action;
using arpg::combat::CombatSnapshot;
using arpg::combat::MovementInput;
using arpg::combat::MonsterSnapshot;
using arpg::combat::Vec3;
using arpg::dungeon::DungeonRules;
using arpg::dungeon::DungeonRunState;
using arpg::dungeon::DungeonSession;
using arpg::dungeon::ExitDirection;
using arpg::dungeon::PendingSaveKind;
using arpg::dungeon::RequestResult;
using arpg::dungeon::RoomPhase;

struct AbyssEntry final {
    DungeonRunState source{};
    DungeonRunState target{};
    ExitDirection direction{ExitDirection::none};
};

struct EnvironmentEvidence final {
    int initial_hp{};
    int minimum_hp{};
    bool warning_seen{};
    bool active_seen{};
    bool active_damage_seen{};
};

DungeonRules validation_rules() noexcept {
    return {};
}

bool install_validation_build(DungeonRunState& state) {
    state.progression = {100U, 0U, 99U, 99U};
    state.item_ownership = {};
    state.item_ownership.items.reserve(6U);
    for (std::uint8_t index = 0U; index < 6U; ++index) {
        const std::uint64_t id = static_cast<std::uint64_t>(index) + 1U;
        const auto item = arpg::items::generate_item({
            0xA8100000ULL + index,
            static_cast<arpg::items::ItemSlot>(index),
            100U,
            id,
            arpg::items::ItemRarity::rare,
        });
        if (!item.has_value()) return false;
        state.item_ownership.items.push_back(*item);
        state.item_ownership.equipment.equipped_ids[index] = id;
    }
    state.item_ownership.next_item_sequence = 7U;
    return true;
}

std::optional<AbyssEntry> find_entry(
    arpg::abyss::AbyssRuleId required_rule) noexcept {
    constexpr std::array<ExitDirection, 4> directions{{
        ExitDirection::up, ExitDirection::down,
        ExitDirection::left, ExitDirection::right,
    }};
    const DungeonRules rules = validation_rules();
    for (std::uint64_t root = 1U; root < 200000U; ++root) {
        const auto initial = arpg::dungeon::make_initial_run_state(root, rules);
        if (initial.fault != arpg::dungeon::DungeonFault::none) continue;
        const auto preview = arpg::dungeon::preview_abyss_doors(
            initial.state.current_room);
        for (std::size_t index = 0U; index < directions.size(); ++index) {
            if (!preview[index]) continue;
            const auto target = arpg::dungeon::make_door_transition(
                initial.state, directions[index], rules);
            if (target.fault == arpg::dungeon::DungeonFault::none
                    && target.state.current_room.is_abyss
                    && target.state.abyss.rule == required_rule) {
                AbyssEntry result{initial.state, target.state, directions[index]};
                if (!install_validation_build(result.target)) return std::nullopt;
                return result;
            }
        }
    }
    return std::nullopt;
}

bool service_pending(DungeonSession& session,
    arpg::persistence::SaveStore& store) noexcept {
    const auto* const pending = session.pending_save_view();
    if (pending == nullptr) return true;
    auto committed = store.commit(pending->next_state);
    arpg::dungeon::SaveDisposition disposition =
        arpg::dungeon::SaveDisposition::indeterminate;
    if (committed.state == arpg::persistence::SaveCommitState::committed) {
        disposition = arpg::dungeon::SaveDisposition::committed;
    } else if (committed.state
            == arpg::persistence::SaveCommitState::not_committed) {
        disposition = arpg::dungeon::SaveDisposition::not_committed;
    }
    session.resolve_pending_save({disposition,
        committed.verified_state.commit_generation,
        std::move(committed.verified_state)});
    return disposition == arpg::dungeon::SaveDisposition::committed
        && session.snapshot().phase != RoomPhase::faulted;
}

const MonsterSnapshot* nearest_monster(const CombatSnapshot& state) noexcept {
    const MonsterSnapshot* best = nullptr;
    float best_distance = 0.0F;
    for (const MonsterSnapshot& monster : state.monsters) {
        if (!monster.active || monster.hp <= 0) continue;
        const float dx = monster.position.x - state.player.position.x;
        const float dy = monster.position.y - state.player.position.y;
        const float distance = dx * dx + dy * dy;
        if (best == nullptr || distance < best_distance) {
            best = &monster;
            best_distance = distance;
        }
    }
    return best;
}

MovementInput movement_toward(Vec3 from, Vec3 to) noexcept {
    MovementInput movement{};
    const float dx = to.x - from.x;
    const float dy = to.y - from.y;
    if (dx > 0.45F) movement.x = 1;
    else if (dx < -0.45F) movement.x = -1;
    if (dy > 0.25F) movement.y = 1;
    else if (dy < -0.25F) movement.y = -1;
    return movement;
}

bool in_attack_lane(const CombatSnapshot& state,
    const MonsterSnapshot& target) noexcept {
    const float dx = target.position.x - state.player.position.x;
    const float dy = target.position.y - state.player.position.y;
    const bool facing = std::fabs(dx) <= 0.2F
        || (dx > 0.0F && state.player.facing == arpg::combat::Facing::right)
        || (dx < 0.0F && state.player.facing == arpg::combat::Facing::left);
    return facing && std::fabs(dx) <= 1.70F && std::fabs(dy) <= 0.55F;
}

bool drive_clear(DungeonSession& session,
    arpg::persistence::SaveStore& store,
    int& initial_hp, int& minimum_hp,
    std::uint64_t& clear_generation,
    arpg::abyss::AbyssRuleId expected_rule) noexcept {
    initial_hp = 0;
    minimum_hp = 0;
    for (int tick = 0; tick < 12000; ++tick) {
        if (!service_pending(session, store)) return false;
        const auto state = session.snapshot();
        if (state.combat.has_value()) {
            if (initial_hp == 0) {
                initial_hp = state.combat->player.hp;
                minimum_hp = initial_hp;
            }
            minimum_hp = (std::min)(minimum_hp, state.combat->player.hp);
        }
        if (state.phase == RoomPhase::cleared
                || state.phase == RoomPhase::awaiting_exit) {
            clear_generation = state.commit_generation;
            const bool expected = state.is_abyss
                && state.abyss_rule == expected_rule;
            if (!expected) {
                std::cerr << "cleared unexpected room=" << state.room_index
                          << " abyss=" << state.is_abyss
                          << " rule=" << static_cast<unsigned>(state.abyss_rule)
                          << " generation=" << state.commit_generation << '\n';
            }
            return expected;
        }
        if (state.phase == RoomPhase::faulted || !state.combat.has_value()) {
            std::cerr << "combat stopped phase="
                      << static_cast<unsigned>(state.phase)
                      << " room=" << state.room_index
                      << " abyss=" << state.is_abyss
                      << " generation=" << state.commit_generation
                      << " fault=" << static_cast<unsigned>(
                            state.diagnostics.fault) << '\n';
            return false;
        }
        MovementInput movement{};
        if (state.phase == RoomPhase::combat) {
            const auto* const target = nearest_monster(*state.combat);
            if (target != nullptr) {
                movement = movement_toward(
                    state.combat->player.position, target->position);
                if (state.combat->player.hurt_ticks == 0U
                        && state.combat->player.active_attack
                            == arpg::combat::AttackId::none
                        && state.combat->diagnostics.input_size == 0U
                        && in_attack_lane(*state.combat, *target)) {
                    static_cast<void>(session.queue_action(Action::light));
                }
            }
        }
        session.tick(movement);
        while (session.try_pop_event().has_value()) {}
        while (session.try_pop_combat_event().has_value()) {}
    }
    const auto failed = session.snapshot();
    std::cerr << "clear failed phase=" << static_cast<unsigned>(failed.phase)
              << " abyss=" << failed.is_abyss
              << " rule=" << static_cast<unsigned>(failed.abyss_rule)
              << " lifecycle_pending=" << failed.abyss_pending_rewards
              << " room=" << failed.room_index
              << " hp=" << (failed.combat.has_value()
                    ? failed.combat->player.hp : -1)
              << " targets=" << static_cast<unsigned>(failed.remaining_targets)
              << '\n';
    return false;
}

bool wait_for_rewards(DungeonSession& session,
    arpg::persistence::SaveStore& store,
    std::array<std::uint64_t, 3>& ids) noexcept {
    for (int tick = 0; tick < 64; ++tick) {
        if (!service_pending(session, store)) return false;
        const auto state = session.snapshot();
        std::size_t found = 0U;
        for (std::size_t index = 0U; index < state.ground_item_count; ++index) {
            const auto& ground = state.ground_items[index];
            if (ground.source != arpg::dungeon::GroundItemSource::abyss_chest
                    || ground.abyss_reward_ordinal >= ids.size()) {
                continue;
            }
            ids[ground.abyss_reward_ordinal] = ground.item_id;
        }
        for (const std::uint64_t id : ids) found += id != 0U;
        if (found == ids.size()) return true;
        session.tick({});
    }
    const auto failed = session.snapshot();
    std::cerr << "reward wait failed phase="
              << static_cast<unsigned>(failed.phase)
              << " pending=" << failed.abyss_pending_rewards
              << " unpicked=" << failed.abyss_unpicked_rewards
              << " ground=" << failed.ground_item_count
              << " fault=" << static_cast<unsigned>(failed.diagnostics.fault)
              << '\n';
    return false;
}

bool claim_reward(DungeonSession& session,
    arpg::persistence::SaveStore& store,
    std::uint8_t reward_ordinal) noexcept {
    for (int tick = 0; tick < 500; ++tick) {
        if (!service_pending(session, store)) return false;
        const auto state = session.snapshot();
        if (state.abyss_unpicked_rewards < 3U
                && state.abyss_pending_rewards == 0U) {
            return true;
        }
        if (!state.combat.has_value()) {
            std::cerr << "claim has no combat phase="
                      << static_cast<unsigned>(state.phase) << '\n';
            return false;
        }
        const arpg::dungeon::GroundItemSnapshot* target = nullptr;
        for (std::size_t index = 0U; index < state.ground_item_count; ++index) {
            if (state.ground_items[index].source
                    == arpg::dungeon::GroundItemSource::abyss_chest
                    && state.ground_items[index].abyss_reward_ordinal
                        == reward_ordinal) {
                target = &state.ground_items[index];
                break;
            }
        }
        if (target == nullptr) {
            std::cerr << "claim target missing inventory="
                      << state.inventory_count << " ground="
                      << state.ground_item_count << " pending="
                      << state.abyss_pending_rewards << " unpicked="
                      << state.abyss_unpicked_rewards << '\n';
            return false;
        }
        const float dx = target->position.x - state.combat->player.position.x;
        const float dy = target->position.y - state.combat->player.position.y;
        if (dx * dx + dy * dy
                <= arpg::dungeon::kPickupRadius * arpg::dungeon::kPickupRadius) {
            const RequestResult requested = session.request_pickup(target->ordinal);
            if (requested == RequestResult::faulted) return false;
        }
        session.tick(movement_toward(
            state.combat->player.position, target->position));
    }
    const auto failed = session.snapshot();
    std::cerr << "claim failed inventory=" << failed.inventory_count
              << " ground=" << failed.ground_item_count
              << " pending=" << failed.abyss_pending_rewards
              << " unpicked=" << failed.abyss_unpicked_rewards
              << " phase=" << static_cast<unsigned>(failed.phase)
              << " fault=" << static_cast<unsigned>(failed.diagnostics.fault)
              << '\n';
    return false;
}

Vec3 door_position(ExitDirection direction) noexcept {
    switch (direction) {
    case ExitDirection::up: return {0.0F, -5.5F, 0.0F};
    case ExitDirection::down: return {0.0F, 5.5F, 0.0F};
    case ExitDirection::left: return {-12.0F, 0.0F, 0.0F};
    case ExitDirection::right: return {12.0F, 0.0F, 0.0F};
    case ExitDirection::none: return {};
    }
    return {};
}

MovementInput exit_movement(Vec3 player, ExitDirection direction) noexcept {
    MovementInput movement = movement_toward(player, door_position(direction));
    switch (direction) {
    case ExitDirection::up: movement.y = -1; break;
    case ExitDirection::down: movement.y = 1; break;
    case ExitDirection::left: movement.x = -1; break;
    case ExitDirection::right: movement.x = 1; break;
    case ExitDirection::none: break;
    }
    return movement;
}

bool abandon_through_door(DungeonSession& session,
    arpg::persistence::SaveStore& store,
    ExitDirection direction) noexcept {
    bool warned = false;
    bool released = false;
    for (int tick = 0; tick < 2000; ++tick) {
        if (!service_pending(session, store)) return false;
        const auto state = session.snapshot();
        if (state.room_index > 1U) return true;
        if (state.phase == RoomPhase::faulted || !state.combat.has_value()) {
            std::cerr << "abandon stopped phase="
                      << static_cast<unsigned>(state.phase)
                      << " fault=" << static_cast<unsigned>(
                            state.diagnostics.fault)
                      << " pending=" << state.abyss_pending_rewards
                      << " unpicked=" << state.abyss_unpicked_rewards << '\n';
            return false;
        }
        if (state.abyss_exit_confirmation_armed) {
            warned = true;
            if (!released) {
                session.tick({});
                released = true;
                continue;
            }
        }
        session.tick(exit_movement(
            state.combat->player.position, direction));
    }
    const auto failed = session.snapshot();
    std::cerr << "abandon timed out phase="
              << static_cast<unsigned>(failed.phase)
              << " room=" << failed.room_index
              << " armed=" << failed.abyss_exit_confirmation_armed
              << " pending=" << failed.abyss_pending_rewards
              << " unpicked=" << failed.abyss_unpicked_rewards << '\n';
    return warned && false;
}

std::optional<EnvironmentEvidence> run_environment_probe(
    const std::filesystem::path& save_directory) noexcept {
    const auto entry = find_entry(arpg::abyss::AbyssRuleId::thunderstorm);
    if (!entry.has_value()) return std::nullopt;
    arpg::persistence::SaveStore store({save_directory});
    const auto seeded = store.commit(entry->target);
    if (seeded.state != arpg::persistence::SaveCommitState::committed) {
        return std::nullopt;
    }
    DungeonSession session{validation_rules(), seeded.verified_state};
    if (!service_pending(session, store)) return std::nullopt;
    EnvironmentEvidence evidence{};
    int previous_hp = 0;
    bool previous_warning = false;
    for (int tick = 0; tick < 1200; ++tick) {
        if (!service_pending(session, store)) return std::nullopt;
        const auto state = session.snapshot();
        if (!state.combat.has_value() || !state.is_abyss) break;
        if (evidence.initial_hp == 0) {
            evidence.initial_hp = state.combat->player.hp;
            evidence.minimum_hp = evidence.initial_hp;
        }
        evidence.minimum_hp = (std::min)(
            evidence.minimum_hp, state.combat->player.hp);
        bool active_this_tick = false;
        bool warning_this_tick = false;
        for (std::size_t index = 0U;
             index < state.combat->hazard_count; ++index) {
            const auto& hazard = state.combat->hazards[index];
            if (!hazard.active
                    || hazard.source
                        != arpg::combat::HazardSource::abyss_environment) {
                continue;
            }
            evidence.warning_seen = evidence.warning_seen
                || hazard.telegraph_ticks != 0U;
            warning_this_tick = warning_this_tick
                || hazard.telegraph_ticks != 0U;
            evidence.active_seen = evidence.active_seen
                || hazard.telegraph_ticks == 0U;
            active_this_tick = active_this_tick
                || hazard.telegraph_ticks == 0U;
        }
        evidence.active_damage_seen = evidence.active_damage_seen
            || (active_this_tick && previous_hp != 0
                && state.combat->player.hp < previous_hp)
            || (previous_warning && !warning_this_tick
                && !active_this_tick && previous_hp != 0
                && state.combat->player.hp < previous_hp);
        evidence.active_seen = evidence.active_seen
            || (previous_warning && !warning_this_tick);
        if (evidence.active_damage_seen) {
            return evidence;
        }
        previous_hp = state.combat->player.hp;
        previous_warning = warning_this_tick;
        MovementInput movement{};
        if (!evidence.warning_seen) {
            const int leg = tick % 240;
            movement = leg < 60 ? MovementInput{1, 0}
                : leg < 120 ? MovementInput{0, 1}
                : leg < 180 ? MovementInput{-1, 0}
                : MovementInput{0, -1};
            if (state.combat->player.active_attack
                    == arpg::combat::AttackId::none
                    && state.combat->diagnostics.input_size == 0U) {
                static_cast<void>(session.queue_action(Action::jump));
            }
        }
        session.tick(movement);
        while (session.try_pop_event().has_value()) {}
        while (session.try_pop_combat_event().has_value()) {}
    }
    const auto stopped = session.snapshot();
    std::cerr << "environment probe stopped phase="
              << static_cast<unsigned>(stopped.phase)
              << " room=" << stopped.room_index
              << " abyss=" << stopped.is_abyss
              << " warning=" << evidence.warning_seen
              << " active=" << evidence.active_seen
              << " active_damage=" << evidence.active_damage_seen
              << " initial_hp=" << evidence.initial_hp
              << " minimum_hp=" << evidence.minimum_hp << '\n';
    return evidence.warning_seen && evidence.active_seen
        && evidence.active_damage_seen
        ? std::optional<EnvironmentEvidence>{evidence} : std::nullopt;
}

std::filesystem::path clean_fixture_directory(
    const std::filesystem::path& executable) {
    const auto directory = std::filesystem::absolute(executable).parent_path()
        / "stage10-validation-fixture-save";
    std::error_code error;
    std::filesystem::remove_all(directory, error);
    if (error) return {};
    std::filesystem::create_directories(directory, error);
    return error ? std::filesystem::path{} : directory;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 1 || argv == nullptr || argv[0] == nullptr) return 2;
    const auto entry = find_entry(arpg::abyss::AbyssRuleId::life_sacrifice);
    if (!entry.has_value()) return 3;
    const auto save_directory = clean_fixture_directory(argv[0]);
    if (save_directory.empty()) return 4;
    const DungeonRules rules = validation_rules();

    arpg::persistence::SaveStore store({save_directory});
    const auto seeded = store.commit(entry->target);
    if (seeded.state != arpg::persistence::SaveCommitState::committed) return 5;
    DungeonSession session{rules, seeded.verified_state};
    if (!service_pending(session, store)) return 6;
    const auto started = session.snapshot();
    if (started.pending_save_kind.has_value()
            || started.abyss_rule != arpg::abyss::AbyssRuleId::life_sacrifice) {
        return 7;
    }

    int initial_hp = 0;
    int minimum_hp = 0;
    std::uint64_t clear_generation = 0U;
    if (!drive_clear(session, store, initial_hp, minimum_hp, clear_generation,
            arpg::abyss::AbyssRuleId::life_sacrifice)) {
        return 8;
    }
    std::array<std::uint64_t, 3> reward_ids{};
    if (!wait_for_rewards(session, store, reward_ids)) return 9;
    if (!claim_reward(session, store, 1U)) return 10;
    const auto claimed = session.snapshot();
    const auto loaded = store.load();
    if (loaded.state != arpg::persistence::SaveLoadState::ready) return 11;

    arpg::persistence::SaveStore abandon_store({save_directory / "abandon"});
    const auto abandon_seeded = abandon_store.commit(entry->target);
    if (abandon_seeded.state
            != arpg::persistence::SaveCommitState::committed) return 12;
    DungeonSession abandon_session{rules, abandon_seeded.verified_state};
    if (!service_pending(abandon_session, abandon_store)) return 13;
    int abandon_initial_hp = 0;
    int abandon_minimum_hp = 0;
    std::uint64_t abandon_clear_generation = 0U;
    if (!drive_clear(abandon_session, abandon_store,
            abandon_initial_hp, abandon_minimum_hp,
            abandon_clear_generation,
            arpg::abyss::AbyssRuleId::life_sacrifice)) return 14;
    if (!abandon_through_door(
            abandon_session, abandon_store, ExitDirection::right)) return 15;
    const auto abandoned = abandon_store.load();
    if (abandoned.state != arpg::persistence::SaveLoadState::ready
            || !abandoned.checkpoint.last_abyss_resolution.valid) return 16;
    const auto environment = run_environment_probe(
        save_directory / "environment");
    if (!environment.has_value()) return 17;

    const auto preview = arpg::dungeon::preview_abyss_doors(
        entry->source.current_room);
    const std::size_t direction_index = static_cast<std::size_t>(entry->direction);
    std::cout << "door_preview source_seed=" << entry->source.current_room.seed
              << " direction=" << direction_index
              << " target_seed=" << entry->target.current_room.seed
              << " marked=" << preview[direction_index] << '\n'
              << "started generation=" << started.commit_generation
              << " rule=" << static_cast<unsigned>(started.abyss_rule)
              << " danger=" << static_cast<unsigned>(started.abyss_danger)
              << '\n'
              << "environment rule=" << static_cast<unsigned>(
                    arpg::abyss::AbyssRuleId::thunderstorm)
              << " initial_hp=" << environment->initial_hp
              << " minimum_hp=" << environment->minimum_hp
              << " warning=" << environment->warning_seen
              << " active=" << environment->active_seen
              << " active_damage=" << environment->active_damage_seen
              << " damaged="
              << (environment->minimum_hp < environment->initial_hp) << '\n'
              << "combat initial_hp=" << initial_hp
              << " minimum_hp=" << minimum_hp << '\n'
              << "clear generation=" << clear_generation << '\n'
              << "rewards ids=" << reward_ids[0] << ',' << reward_ids[1]
              << ',' << reward_ids[2] << '\n'
              << "reload generated="
              << static_cast<unsigned>(loaded.checkpoint.abyss.generated_mask)
              << " claimed="
              << static_cast<unsigned>(loaded.checkpoint.abyss.claimed_mask)
              << " abandoned="
              << static_cast<unsigned>(loaded.checkpoint.abyss.abandoned_mask)
              << " inventory=" << claimed.inventory_count << '\n'
              << "abandon total="
              << static_cast<unsigned>(
                    abandoned.checkpoint.last_abyss_resolution.total)
              << " generated="
              << static_cast<unsigned>(
                    abandoned.checkpoint.last_abyss_resolution.generated)
              << " claimed="
              << static_cast<unsigned>(
                    abandoned.checkpoint.last_abyss_resolution.claimed)
              << " abandoned="
              << static_cast<unsigned>(
                    abandoned.checkpoint.last_abyss_resolution.abandoned)
              << '\n';
    return preview[direction_index]
            && started.commit_generation > entry->target.commit_generation
            && reward_ids[0] != 0U && reward_ids[1] != 0U
            && reward_ids[2] != 0U
            && loaded.checkpoint.abyss.claimed_mask != 0U
        ? 0 : 18;
}
