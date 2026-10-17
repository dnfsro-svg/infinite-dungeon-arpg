# Review package: 17cbb0a01f4a4e039574e689986fb0a5256c42ed..8a848b30fafe87135cf432be8d0cd236c45c2c49

## Commits
8a848b3 feat: add atomic health potion drops

## Files changed
 src/combat/combat_world.cpp                   |   14 +
 src/combat/combat_world.hpp                   |    2 +
 src/dungeon/CMakeLists.txt                    |    1 +
 src/dungeon/dungeon_session.cpp               |  162 ++-
 src/dungeon/dungeon_session.hpp               |   16 +
 src/dungeon/dungeon_snapshot.cpp              |   14 +
 src/dungeon/dungeon_transition.cpp            |  265 ++++-
 src/dungeon/dungeon_types.hpp                 |   10 +
 src/dungeon/health_potion_loot.cpp            |   29 +
 src/dungeon/health_potion_loot.hpp            |   56 ++
 tests/combat/combat_test_main.cpp             |    2 +-
 tests/combat/player_health_tests.cpp          |   46 +
 tests/dungeon/CMakeLists.txt                  |    1 +
 tests/dungeon/dungeon_health_potion_tests.cpp | 1309 +++++++++++++++++++++++++
 tests/dungeon/dungeon_stress_tests.cpp        |   81 ++
 tests/dungeon/dungeon_test_main.cpp           |   22 +-
 tests/dungeon/dungeon_test_support.hpp        |  218 +++-
 17 files changed, 2236 insertions(+), 12 deletions(-)

## Diff
diff --git a/src/combat/combat_world.cpp b/src/combat/combat_world.cpp
index 671be35..c3b315a 100644
--- a/src/combat/combat_world.cpp
+++ b/src/combat/combat_world.cpp
@@ -733,20 +733,34 @@ void CombatWorld::restore_player_resources(int hp, int barrier) noexcept {
         encounter_config_.abyss.player_resource_restore_bp,
         BasisPointRounding::ceil);
     player_.hp = restored_hp >= player_.max_hp - player_.hp
         ? player_.max_hp
         : player_.hp + restored_hp;
     player_.barrier = restored_barrier >= player_.max_barrier - player_.barrier
         ? player_.max_barrier
         : player_.barrier + restored_barrier;
 }
 
+int CombatWorld::restore_player_health_percent(
+    std::uint16_t maximum_health_basis_points) noexcept {
+    if (maximum_health_basis_points == 0U || player_.max_hp <= 0
+            || player_defeated()) {
+        return 0;
+    }
+    const int requested = scale_basis_points(player_.max_hp,
+        maximum_health_basis_points, BasisPointRounding::ceil);
+    const int missing = (std::max)(0, player_.max_hp - player_.hp);
+    const int actual = (std::min)(requested, missing);
+    player_.hp += actual;
+    return actual;
+}
+
 void CombatWorld::clear_abyss_rule_preserving_resources() noexcept {
     DerivedPlayerBuild derived{};
     if (!derive_player_build(encounter_config_.player_build, derived)) return;
 
     const int old_max_hp = player_.max_hp;
     const int old_hp = player_.hp;
     const bool alive = old_hp > 0;
     remove_environment_hazards();
     abyss_environment_ = AbyssEnvironmentRuntime{};
     encounter_config_.abyss = {};
diff --git a/src/combat/combat_world.hpp b/src/combat/combat_world.hpp
index ed21edb..ed37898 100644
--- a/src/combat/combat_world.hpp
+++ b/src/combat/combat_world.hpp
@@ -38,20 +38,22 @@ public:
     explicit CombatWorld(CombatLabConfig config = {}) noexcept;
     explicit CombatWorld(CombatEncounterConfig config) noexcept;
 
     [[nodiscard]] bool queue_action(Action action) noexcept;
     [[nodiscard]] SkillCastResult request_active_skill(
         skills::ActiveSkillId skill) noexcept;
     void tick(MovementInput movement) noexcept;
     void reset() noexcept;
     void apply_player_build(PlayerCombatBuild build) noexcept;
     void restore_player_resources(int hp, int barrier) noexcept;
+    [[nodiscard]] int restore_player_health_percent(
+        std::uint16_t maximum_health_basis_points) noexcept;
     void clear_abyss_rule_preserving_resources() noexcept;
     [[nodiscard]] bool load_wave(
         const EncounterWave& wave,
         bool reset_player_health = true) noexcept;
     [[nodiscard]] bool destroy_monster(MonsterHandle handle) noexcept;
     [[nodiscard]] Vec3 player_position() const noexcept;
     [[nodiscard]] std::size_t living_monster_count() const noexcept;
     [[nodiscard]] std::size_t active_monster_count() const noexcept;
     [[nodiscard]] std::size_t active_projectile_count() const noexcept;
     [[nodiscard]] CombatSnapshot snapshot() const noexcept;
diff --git a/src/dungeon/CMakeLists.txt b/src/dungeon/CMakeLists.txt
index 0307a0f..08f5c47 100644
--- a/src/dungeon/CMakeLists.txt
+++ b/src/dungeon/CMakeLists.txt
@@ -4,20 +4,21 @@ add_library(arpg_dungeon STATIC
     dungeon_rules.cpp
     encounter_director.cpp
     encounter_budget.cpp
     encounter_wave_builder.cpp
     dungeon_session.cpp
     dungeon_transition.cpp
     dungeon_snapshot.cpp
     death_checkpoint.cpp
     dungeon_progression.cpp
     material_loot.cpp
+    health_potion_loot.cpp
     room_generation.cpp
     room_combat_template.cpp
     room_navigation.cpp)
 
 target_include_directories(arpg_dungeon PUBLIC "${PROJECT_SOURCE_DIR}/src")
 target_compile_features(arpg_dungeon PUBLIC cxx_std_17)
 target_link_libraries(arpg_dungeon PUBLIC arpg_combat arpg_progression arpg_passives arpg_items arpg_skills PRIVATE arpg_core arpg_abyss)
 arpg_enable_project_warnings(arpg_dungeon)
 if(MSVC)
     target_compile_options(arpg_dungeon PUBLIC /EHsc)
diff --git a/src/dungeon/dungeon_session.cpp b/src/dungeon/dungeon_session.cpp
index 0ac75bc..fcfb97e 100644
--- a/src/dungeon/dungeon_session.cpp
+++ b/src/dungeon/dungeon_session.cpp
@@ -1,19 +1,20 @@
 #include "dungeon/dungeon_session.hpp"
 
 #include "dungeon/abyss_reward.hpp"
 #include "dungeon/death_checkpoint.hpp"
 #include "dungeon/dungeon_progression.hpp"
 #include "dungeon/room_combat_template.hpp"
 #include "dungeon/room_navigation.hpp"
 #include "abyss/abyss_rewards.hpp"
 #include "abyss/abyss_rules.hpp"
+#include "combat/combat_scaling.hpp"
 #include "core/deterministic_rng.hpp"
 #include "items/item_catalog.hpp"
 #include "items/item_generation.hpp"
 #include "items/item_modifiers.hpp"
 #include "modifiers/player_modifier_values.hpp"
 #include "passives/passive_tree_rules.hpp"
 
 #include <array>
 #include <cassert>
 #include <cstddef>
@@ -252,46 +253,66 @@ DungeonSession::DungeonSession(
         return;
     }
     if (validate_rules(rules_) != DungeonFault::none) {
         enter_fault(DungeonFault::invalid_rules);
         return;
     }
     construct_current_room();
 }
 
 bool DungeonSession::queue_action(combat::Action action) noexcept {
-    return phase_ == RoomPhase::combat && combat_.has_value()
+    return phase_ == RoomPhase::combat
+        && !health_potion_abyss_clear_retry_gate_active()
+        && combat_.has_value()
         ? combat_->queue_action(action)
         : false;
 }
 
+bool DungeonSession::health_potion_abyss_clear_retry_gate_active()
+    const noexcept {
+    return retry_health_potion_abyss_clear_before_combat_;
+}
+
 combat::SkillCastResult DungeonSession::request_active_skill_slot(
     std::uint8_t slot) noexcept {
     const std::size_t index = static_cast<std::size_t>(slot);
-    if (phase_ != RoomPhase::combat || !combat_.has_value()
+    if (phase_ != RoomPhase::combat
+            || health_potion_abyss_clear_retry_gate_active()
+            || !combat_.has_value()
             || index >= skills::kActiveSkillSlotCount) {
         return combat::SkillCastResult::none;
     }
     const skills::ActiveSkillId skill =
         stable_state_.skill_loadout.slots[index].active;
     return skill == skills::ActiveSkillId::none
         ? combat::SkillCastResult::none
         : combat_->request_active_skill(skill);
 }
 
 RoomPhase DungeonSession::phase() const noexcept {
     return phase_;
 }
 
 void DungeonSession::tick(
     combat::MovementInput movement,
     AutoPickupPolicy pickup_policy) noexcept {
+    if (retry_health_potion_abyss_clear_before_combat_) {
+        retry_health_potion_abyss_clear_before_combat_ = false;
+        if (phase_ != RoomPhase::combat || pending_save_.has_value()
+                || !combat_.has_value()) {
+            enter_fault(DungeonFault::save_receipt_mismatch);
+        } else {
+            prepare_room_clear();
+        }
+        ++session_tick_;
+        return;
+    }
     if (phase_ == RoomPhase::committing
             || phase_ == RoomPhase::death_pending
             || phase_ == RoomPhase::faulted) {
         ++session_tick_;
         return;
     }
     if (phase_ == RoomPhase::locked) {
         phase_ = RoomPhase::combat;
         if (emit(DungeonEventKind::room_entered)
                 && phase_ != RoomPhase::faulted) {
@@ -395,26 +416,28 @@ std::optional<PendingSave> DungeonSession::pending_save() const noexcept {
 
 const PendingSave* DungeonSession::pending_save_view() const noexcept {
     return pending_save_.has_value() ? &*pending_save_ : nullptr;
 }
 
 const items::ItemOwnershipState& DungeonSession::item_state() const noexcept {
     return stable_state_.item_ownership;
 }
 
 RequestResult DungeonSession::reset_current_room() noexcept {
-    if (phase_ == RoomPhase::transitioning
+    if (health_potion_abyss_clear_retry_gate_active()
+            || phase_ == RoomPhase::transitioning
             || phase_ == RoomPhase::committing
             || phase_ == RoomPhase::death_pending
             || phase_ == RoomPhase::faulted) {
         return RequestResult::rejected;
     }
+    retry_health_potion_abyss_clear_before_combat_ = false;
     if (stable_state_.current_room.is_abyss
             && stable_state_.abyss.lifecycle
                 == abyss::AbyssLifecycle::started) {
         return prepare_abyss_failure();
     }
     if (stable_state_.current_room.is_abyss
             && stable_state_.abyss.lifecycle
                 == abyss::AbyssLifecycle::cleared) {
         return RequestResult::rejected;
     }
@@ -453,40 +476,42 @@ bool item_id_in_use(
 std::optional<DungeonEvent> DungeonSession::try_pop_event() noexcept {
     return events_.try_pop();
 }
 
 std::optional<combat::CombatEvent>
 DungeonSession::try_pop_combat_event() noexcept {
     return combat_events_.try_pop();
 }
 
 void DungeonSession::construct_current_room() noexcept {
+    retry_health_potion_abyss_clear_before_combat_ = false;
     if (stable_state_.death.lifecycle
             == checkpoint::DeathLifecycle::pending_continue) {
         clear_transient_room_state();
         if (!validate_pending_death_state()) {
             enter_fault(DungeonFault::death_sequence_mismatch);
             return;
         }
         phase_ = RoomPhase::death_pending;
         return;
     }
     if (!checkpoint::valid_death_checkpoint_structural(
             stable_state_.death)) {
         clear_transient_room_state();
         enter_fault(DungeonFault::death_sequence_mismatch);
         return;
     }
     ground_items_ = {};
     rolled_drop_bits_ = {};
     ground_materials_ = {};
     rolled_material_bits_ = {};
+    ground_health_potions_ = {};
     if (stable_state_.current_room.depth == 0U) {
         enter_fault(DungeonFault::invalid_item_state);
         return;
     }
     room_progression_ = stable_state_.progression;
     pending_room_experience_ = 0U;
     last_room_experience_ = 0U;
     last_levels_gained_ = 0U;
     if (stable_state_.abyss.lifecycle == abyss::AbyssLifecycle::cleared) {
         construct_cleared_abyss_room();
@@ -548,25 +573,27 @@ bool DungeonSession::validate_pending_death_state() const noexcept {
         && state.last_abyss_resolution.room_seed
             == state.current_room.seed
         && state.last_abyss_resolution.rule == state.abyss.rule
         && state.last_abyss_resolution.total == total
         && state.last_abyss_resolution.generated == 0U
         && state.last_abyss_resolution.claimed == 0U
         && state.last_abyss_resolution.abandoned == total;
 }
 
 void DungeonSession::clear_transient_room_state() noexcept {
+    retry_health_potion_abyss_clear_before_combat_ = false;
     combat_.reset();
     ground_items_ = {};
     rolled_drop_bits_ = {};
     ground_materials_ = {};
     rolled_material_bits_ = {};
+    ground_health_potions_ = {};
     encounter_plan_ = {};
     wave_index_ = 0U;
     wave_delay_ticks_ = 0U;
     pending_save_.reset();
     pending_item_build_.reset();
     pending_abyss_combat_.reset();
     pending_abyss_reward_.reset();
     clear_abyss_exit_confirmation();
     room_progression_ = stable_state_.progression;
     pending_room_experience_ = 0U;
@@ -822,20 +849,21 @@ void DungeonSession::construct_normal_room() noexcept {
         return;
     }
     encounter_plan_ = plan.plan;
     wave_index_ = 0U;
     wave_delay_ticks_ = 0U;
     combat_.emplace(*config);
     phase_ = RoomPhase::locked;
 }
 
 void DungeonSession::reset_to_normal_room(bool clear_queues) noexcept {
+    retry_health_potion_abyss_clear_before_combat_ = false;
     if (clear_queues) {
         while (events_.try_pop().has_value()) {
         }
         while (combat_events_.try_pop().has_value()) {
         }
     }
     combat_.reset();
     pending_save_.reset();
     pending_item_build_.reset();
     pending_abyss_combat_.reset();
@@ -1248,20 +1276,23 @@ void DungeonSession::publish_run_state_reusing_items(
         source.item_ownership.material_claimed_drop_bits;
     destination.item_ownership.claimed_drop_bits =
         source.item_ownership.claimed_drop_bits;
     destination.item_ownership.next_item_sequence =
         source.item_ownership.next_item_sequence;
     destination.death_sequence = source.death_sequence;
     destination.death = source.death;
 }
 
 RequestResult DungeonSession::request_death_continue() noexcept {
+    if (health_potion_abyss_clear_retry_gate_active()) {
+        return RequestResult::rejected;
+    }
     if (phase_ == RoomPhase::faulted) return RequestResult::faulted;
     if (phase_ != RoomPhase::death_pending || pending_save_.has_value()
             || combat_.has_value()
             || stable_state_.death.lifecycle
                 != checkpoint::DeathLifecycle::pending_continue) {
         return RequestResult::rejected;
     }
     if (stable_state_.commit_generation
             == (std::numeric_limits<std::uint64_t>::max)()) {
         enter_fault(DungeonFault::commit_generation_overflow);
@@ -1452,20 +1483,106 @@ bool DungeonSession::place_ground_material(
     }
     if (ground_materials_[ordinal].active) {
         saturating_increment(diagnostics_.material_ground_saturation_count);
         return false;
     }
     ground_materials_[ordinal] = {
         true, ordinal, source, position, material};
     return true;
 }
 
+bool DungeonSession::place_ground_health_potion(
+    std::uint16_t spawn_ordinal, combat::Vec3 position) noexcept {
+    if (spawn_ordinal >= ground_health_potions_.size()) return false;
+    GroundHealthPotion& slot = ground_health_potions_[spawn_ordinal];
+    if (slot.active) {
+        saturating_increment(
+            diagnostics_.health_potion_ground_saturation_count);
+        return false;
+    }
+    slot = {true, spawn_ordinal,
+        health_potion_claim_ordinal(spawn_ordinal), position};
+    return true;
+}
+
+bool DungeonSession::has_claimable_health_potion() const noexcept {
+    if (!combat_.has_value() || combat_->death_snapshot().has_value()) {
+        return false;
+    }
+    const combat::CombatSnapshot snapshot = combat_->snapshot();
+    if (!health_potion_auto_use_eligible(
+            snapshot.player.hp, snapshot.player.max_hp)) {
+        return false;
+    }
+    for (const GroundHealthPotion& ground : ground_health_potions_) {
+        if (ground.active) return true;
+    }
+    return false;
+}
+
+bool DungeonSession::append_clear_health_potion_claims(
+    PendingSave& pending) noexcept {
+    if ((pending.kind != PendingSaveKind::room_clear
+            && pending.kind != PendingSaveKind::abyss_clear)
+            || !combat_.has_value()) {
+        return false;
+    }
+    pending.health_potion_claim.reset();
+    if (combat_->death_snapshot().has_value()) return true;
+    const combat::CombatSnapshot snapshot = combat_->snapshot();
+    if (!health_potion_auto_use_eligible(
+            snapshot.player.hp, snapshot.player.max_hp)) {
+        return true;
+    }
+
+    PendingHealthPotionClaim claim{};
+    claim.expected_hp = snapshot.player.hp;
+    claim.expected_max_hp = snapshot.player.max_hp;
+    int projected_hp = snapshot.player.hp;
+    const int restore = combat::scale_basis_points(
+        snapshot.player.max_hp, kHealthPotionRestoreBp,
+        combat::BasisPointRounding::ceil);
+    for (std::uint16_t spawn = 0U;
+         spawn < ground_health_potions_.size(); ++spawn) {
+        const GroundHealthPotion& ground = ground_health_potions_[spawn];
+        if (!ground.active) continue;
+        const std::uint16_t claim_ordinal =
+            health_potion_claim_ordinal(spawn);
+        if (ground.spawn_ordinal != spawn
+                || ground.claim_ordinal != claim_ordinal
+                || bit_is_set(stable_state_.item_ownership
+                        .material_claimed_drop_bits,
+                    claim_ordinal)
+                || bit_is_set(pending.next_state.item_ownership
+                        .material_claimed_drop_bits,
+                    claim_ordinal)) {
+            return false;
+        }
+        claim.spawn_ordinals[claim.count++] = spawn;
+        set_bit(pending.next_state.item_ownership
+                .material_claimed_drop_bits,
+            claim_ordinal);
+        projected_hp = (std::min)(
+            snapshot.player.max_hp, projected_hp + restore);
+        const bool strictly_above_threshold =
+            static_cast<std::int64_t>(projected_hp) * 10000
+                > static_cast<std::int64_t>(snapshot.player.max_hp)
+                    * kHealthPotionAutoUseThresholdBp;
+        if (strictly_above_threshold
+                || claim.count == kPendingHealthPotionClaimCapacity) {
+            break;
+        }
+    }
+    if (claim.count != 0U) pending.health_potion_claim = claim;
+    return true;
+}
+
 void DungeonSession::roll_ground_materials(
     const combat::CombatEvent& event) noexcept {
     const std::uint16_t spawn_ordinal = event.spawn_ordinal;
     if (spawn_ordinal >= kGroundDropCapacity) return;
     const std::uint16_t common_ordinal = static_cast<std::uint16_t>(
         spawn_ordinal * 2U);
     const std::uint16_t coupon_ordinal = static_cast<std::uint16_t>(
         common_ordinal + 1U);
     const combat::Vec3 position{event.position.x, event.position.y, 0.0F};
     if (claim_material_roll(common_ordinal)) {
@@ -1488,38 +1605,59 @@ void DungeonSession::roll_ground_materials(
         const auto coupon = roll_coupon_drop(
             stable_state_.current_room.seed,
             spawn_ordinal,
             stable_state_.current_room.depth,
             event.affix_score,
             abyss_monster);
         if (coupon.has_value()) {
             static_cast<void>(place_ground_material(
                 coupon_ordinal, GroundMaterialSource::monster_coupon,
                 position, *coupon));
+        } else if (roll_health_potion_drop(
+                stable_state_.current_room.seed, spawn_ordinal)) {
+            static_cast<void>(place_ground_health_potion(
+                spawn_ordinal, position));
         }
     }
 }
 
 bool DungeonSession::materialize_abyss_clear_materials() noexcept {
     const std::uint8_t count = abyss_material_reward_count(
         stable_state_.abyss.danger);
     if (count == 0U || count > 3U) return false;
     for (std::uint8_t index = 0U; index < count; ++index) {
         const std::uint16_t ordinal = static_cast<std::uint16_t>(
             kAbyssMaterialOrdinalBegin + index);
         if (!claim_material_roll(ordinal)) {
             if (bit_is_set(
                     stable_state_.item_ownership.material_claimed_drop_bits,
                     ordinal)) {
                 continue;
             }
-            return false;
+            const auto expected_material = roll_abyss_material(
+                stable_state_.current_room.seed,
+                stable_state_.current_room.depth,
+                index);
+            const auto expected_position = abyss_reward_position(index);
+            const GroundMaterial& ground = ground_materials_[ordinal];
+            if (!expected_material.has_value()
+                    || !expected_position.has_value()
+                    || !ground.active
+                    || ground.ordinal != ordinal
+                    || ground.source != GroundMaterialSource::abyss_reward
+                    || ground.material != *expected_material
+                    || ground.position.x != expected_position->x
+                    || ground.position.y != expected_position->y
+                    || ground.position.z != expected_position->z) {
+                return false;
+            }
+            continue;
         }
         const auto material = roll_abyss_material(
             stable_state_.current_room.seed,
             stable_state_.current_room.depth,
             index);
         const auto position = abyss_reward_position(index);
         if (!material.has_value() || !position.has_value()
                 || !place_ground_material(
                     ordinal, GroundMaterialSource::abyss_reward,
                     *position, *material)) {
@@ -1572,21 +1710,22 @@ void DungeonSession::settle_room_experience() noexcept {
     const progression::ProgressionAward award = progression::apply_experience(
         room_progression_, pending_room_experience_, progression_rules_);
     room_progression_ = award.state;
     last_levels_gained_ = award.levels_gained;
     pending_room_experience_ = 0U;
 }
 
 void DungeonSession::prepare_room_clear() noexcept {
     const bool started_abyss = stable_state_.current_room.is_abyss
         && stable_state_.abyss.lifecycle == abyss::AbyssLifecycle::started;
-    if (!started_abyss && !has_ground_materials()) {
+    if (!started_abyss && !has_ground_materials()
+            && !has_claimable_health_potion()) {
         settle_room_experience();
         publish_room_clear();
         return;
     }
     if (!combat_.has_value() || pending_save_.has_value()) {
         enter_fault(started_abyss
             ? DungeonFault::invalid_abyss_state
             : DungeonFault::invalid_item_state);
         return;
     }
@@ -1637,20 +1776,32 @@ void DungeonSession::prepare_room_clear() noexcept {
         pending_save_ = PendingSave{
             started_abyss ? PendingSaveKind::abyss_clear
                           : PendingSaveKind::room_clear,
             next.commit_generation,
             std::move(next),
             TransitionKind::none,
             ExitDirection::none,
             started_abyss ? RoomPhase::cleared : RoomPhase::combat,
         };
         vacuum_room_materials();
+        if (phase_ != RoomPhase::faulted && pending_save_.has_value()
+                && !append_clear_health_potion_claims(*pending_save_)) {
+            pending_save_.reset();
+            enter_fault(started_abyss
+                ? DungeonFault::invalid_abyss_state
+                : DungeonFault::invalid_item_state);
+            return;
+        }
+        if (started_abyss && pending_save_.has_value()
+                && pending_save_->health_potion_claim.has_value()) {
+            pending_save_->resume_phase = RoomPhase::combat;
+        }
     } catch (...) {
         pending_save_.reset();
         enter_fault(started_abyss
             ? DungeonFault::invalid_abyss_state
             : DungeonFault::invalid_item_state);
         return;
     }
     if (phase_ == RoomPhase::faulted || !pending_save_.has_value()) return;
     phase_ = RoomPhase::committing;
 }
@@ -1664,20 +1815,21 @@ void DungeonSession::publish_room_clear() noexcept {
 }
 
 bool DungeonSession::can_emit(std::size_t count) const noexcept {
     return count <= events_.capacity() - events_.size();
 }
 
 void DungeonSession::enter_fault(DungeonFault fault) noexcept {
     if (fault == DungeonFault::none) {
         return;
     }
+    retry_health_potion_abyss_clear_before_combat_ = false;
     diagnostics_.fault = fault;
     phase_ = RoomPhase::faulted;
     clear_abyss_exit_confirmation();
     pending_item_build_.reset();
     pending_abyss_combat_.reset();
 
     const DungeonEvent event{
         DungeonEventKind::faulted,
         session_tick_,
         stable_state_.current_room.index,
diff --git a/src/dungeon/dungeon_session.hpp b/src/dungeon/dungeon_session.hpp
index 62bb3d2..ee249cd 100644
--- a/src/dungeon/dungeon_session.hpp
+++ b/src/dungeon/dungeon_session.hpp
@@ -195,20 +195,25 @@ private:
     [[nodiscard]] bool claim_defeat_reward(
         const combat::CombatEvent& event) noexcept;
     void roll_ground_drop(const combat::CombatEvent& event) noexcept;
     void roll_ground_materials(const combat::CombatEvent& event) noexcept;
     [[nodiscard]] bool claim_material_roll(std::uint16_t ordinal) noexcept;
     [[nodiscard]] bool place_ground_material(
         std::uint16_t ordinal,
         GroundMaterialSource source,
         combat::Vec3 position,
         items::MaterialId material) noexcept;
+    [[nodiscard]] bool place_ground_health_potion(
+        std::uint16_t spawn_ordinal, combat::Vec3 position) noexcept;
+    [[nodiscard]] bool has_claimable_health_potion() const noexcept;
+    [[nodiscard]] bool append_clear_health_potion_claims(
+        PendingSave& pending) noexcept;
     [[nodiscard]] bool materialize_abyss_clear_materials() noexcept;
     void vacuum_room_materials() noexcept;
     [[nodiscard]] bool has_ground_materials() const noexcept;
     void prepare_room_clear() noexcept;
     void publish_room_clear() noexcept;
     void settle_room_experience() noexcept;
     [[nodiscard]] bool can_emit(std::size_t count) const noexcept;
     void attempt_exit(ExitDirection direction) noexcept;
     [[nodiscard]] bool prepare_transition(
         TransitionKind kind,
@@ -229,20 +234,27 @@ private:
         DungeonRunState&& next,
         PendingSaveKind kind,
         RoomPhase resume_phase,
         std::optional<ReinforcementReceipt> reinforcement_receipt =
             std::nullopt) noexcept;
     [[nodiscard]] RequestResult request_skill_loadout_mutation(
         SkillLoadoutMutation mutation,
         skills::ActiveSkillId skill,
         std::uint8_t left,
         std::uint8_t right) noexcept;
+    [[nodiscard]] RequestResult request_health_potion_pickup(
+        std::uint16_t spawn_ordinal) noexcept;
+    [[nodiscard]] bool health_potion_abyss_clear_retry_gate_active()
+        const noexcept;
+    [[nodiscard]] bool pending_health_potion_cache_consistent() const noexcept;
+    void apply_committed_health_potions(
+        const PendingHealthPotionClaim& claim, bool room_clear) noexcept;
     [[nodiscard]] PlayerBuildResult build_for(
         const checkpoint::DungeonRunState& state,
         const items::EquipmentState* equipment_override = nullptr) const noexcept;
     [[nodiscard]] bool pending_item_cache_consistent() const noexcept;
     [[nodiscard]] bool pending_abyss_cache_consistent() const noexcept;
     [[nodiscard]] bool pending_abyss_reward_cache_consistent() const noexcept;
     [[nodiscard]] bool pending_abyss_claim_cache_consistent() const noexcept;
     [[nodiscard]] bool pending_death_cache_consistent() const noexcept;
     [[nodiscard]] bool pending_material_cache_consistent() const noexcept;
     void commit_pending_save(const PendingSaveResult& result) noexcept;
@@ -266,20 +278,24 @@ private:
     std::optional<combat::PlayerCombatBuild> pending_item_build_{};
     std::optional<combat::CombatEncounterConfig> pending_abyss_combat_{};
     std::optional<PendingAbyssReward> pending_abyss_reward_{};
     AbyssExitConfirmation abyss_exit_confirmation_{};
     std::optional<combat::CombatWorld> combat_{};
     std::array<GroundItem, kGroundDropCapacity> ground_items_{};
     std::array<std::uint64_t, 3> rolled_drop_bits_{};
     std::array<GroundMaterial, kGroundMaterialCapacity> ground_materials_{};
     std::array<std::uint64_t, kMaterialDropBitWordCount>
         rolled_material_bits_{};
+    std::array<GroundHealthPotion, kGroundHealthPotionCapacity>
+        ground_health_potions_{};
+    HealthPotionPickupReceipt health_potion_pickup_receipt_{};
+    bool retry_health_potion_abyss_clear_before_combat_{};
     MaterialPickupReceipt material_pickup_receipt_{};
     ReinforcementReceipt reinforcement_receipt_{};
     RoomEncounterPlan encounter_plan_{};
     std::uint8_t wave_index_{};
     std::uint16_t wave_delay_ticks_{};
     core::BoundedQueue<DungeonEvent, kDungeonEventCapacity> events_{};
     core::BoundedQueue<combat::CombatEvent, kCombatRelayCapacity>
         combat_events_{};
     RoomPhase phase_{RoomPhase::locked};
     ExitDirection last_exit_{ExitDirection::none};
diff --git a/src/dungeon/dungeon_snapshot.cpp b/src/dungeon/dungeon_snapshot.cpp
index d5398ca..1f8cd41 100644
--- a/src/dungeon/dungeon_snapshot.cpp
+++ b/src/dungeon/dungeon_snapshot.cpp
@@ -137,33 +137,47 @@ DungeonSnapshot DungeonSession::build_dungeon_snapshot() const noexcept {
     }
     for (const GroundMaterial& ground : ground_materials_) {
         if (!ground.active) continue;
         GroundMaterialSnapshot& packed =
             result.ground_materials[result.ground_material_count++];
         packed.ordinal = ground.ordinal;
         packed.source = ground.source;
         packed.position = ground.position;
         packed.material = ground.material;
     }
+    for (const GroundHealthPotion& ground : ground_health_potions_) {
+        if (!ground.active) continue;
+        GroundHealthPotionSnapshot& packed =
+            result.ground_health_potions[
+                result.ground_health_potion_count++];
+        packed.spawn_ordinal = ground.spawn_ordinal;
+        packed.claim_ordinal = ground.claim_ordinal;
+        packed.position = ground.position;
+    }
+    result.health_potion_pickup_receipt = health_potion_pickup_receipt_;
     result.material_pickup_receipt = material_pickup_receipt_;
     result.reinforcement_receipt = reinforcement_receipt_;
     if (pending_save_.has_value()) {
         result.pending_save_kind = pending_save_->kind;
         if (pending_save_->kind == PendingSaveKind::loot_pickup
                 || pending_save_->kind
                     == PendingSaveKind::abyss_reward_claim) {
             result.pending_pickup_ordinal = pending_save_->pickup_ordinal;
         }
         if (pending_save_->kind == PendingSaveKind::material_pickup) {
             result.pending_material_pickup_ordinal =
                 pending_save_->pickup_ordinal;
         }
+        if (pending_save_->kind == PendingSaveKind::health_potion_pickup) {
+            result.pending_health_potion_spawn_ordinal =
+                pending_save_->pickup_ordinal;
+        }
     }
     if (combat_.has_value()) {
         result.combat.emplace(combat_->snapshot());
     }
     result.encounter.total_budget = encounter_plan_.total_budget;
     if (stable_state_.current_room.is_abyss) {
         const auto legality = abyss_encounter_legality_config(rules_.encounter);
         result.encounter.plan_valid = legality.has_value()
             && encounter_plan_legal(encounter_plan_, *legality);
     } else {
diff --git a/src/dungeon/dungeon_transition.cpp b/src/dungeon/dungeon_transition.cpp
index 2085f7c..bfd4897 100644
--- a/src/dungeon/dungeon_transition.cpp
+++ b/src/dungeon/dungeon_transition.cpp
@@ -1,13 +1,14 @@
 #include "dungeon/dungeon_session.hpp"
 
 #include "combat/room_bounds.hpp"
+#include "combat/combat_scaling.hpp"
 #include "core/deterministic_rng.hpp"
 #include "dungeon/abyss_reward.hpp"
 #include "dungeon/dungeon_progression.hpp"
 #include "items/item_catalog.hpp"
 #include "items/item_crafting.hpp"
 #include "items/item_generation.hpp"
 #include "passives/passive_tree_rules.hpp"
 
 #include <array>
 #include <cstddef>
@@ -130,49 +131,52 @@ std::uint8_t popcount8(std::uint8_t value) noexcept {
     while (value != 0U) {
         count = static_cast<std::uint8_t>(count + (value & 1U));
         value = static_cast<std::uint8_t>(value >> 1U);
     }
     return count;
 }
 
 }  // namespace
 
 bool DungeonSession::request_descent(bool player_in_range) noexcept {
+    if (health_potion_abyss_clear_retry_gate_active()) return false;
     if (phase_ != RoomPhase::awaiting_exit || !combat_.has_value()
             || !player_in_range || pending_save_.has_value()
             || !stable_state_.current_room.has_hole) {
         saturating_increment(diagnostics_.rejected_exit_count);
         return false;
     }
     const bool abandon = abyss_pending_reward_count() != 0U
         || abyss_unpicked_reward_count() != 0U;
     if (!confirm_abyss_exit(
             TransitionKind::descent, ExitDirection::none)) {
         return false;
     }
     return prepare_transition(
         TransitionKind::descent, ExitDirection::none, abandon);
 }
 
 void DungeonSession::resolve_pending_transition(
     const TransitionSaveResult& result) noexcept {
+    if (health_potion_abyss_clear_retry_gate_active()) return;
     if (!pending_save_.has_value()
             || (pending_save_->kind != PendingSaveKind::transition
                 && pending_save_->kind != PendingSaveKind::abyss_abandon)) {
         enter_fault(DungeonFault::save_receipt_mismatch);
         return;
     }
     resolve_pending_save(result);
 }
 
 void DungeonSession::resolve_pending_save(
     const PendingSaveResult& result) noexcept {
+    if (health_potion_abyss_clear_retry_gate_active()) return;
     commit_pending_save(result);
 }
 
 void DungeonSession::attempt_exit(ExitDirection direction) noexcept {
     if (phase_ != RoomPhase::awaiting_exit || !combat_.has_value()
             || direction == ExitDirection::none || pending_save_.has_value()) {
         saturating_increment(diagnostics_.rejected_exit_count);
         return;
     }
 
@@ -402,20 +406,21 @@ bool DungeonSession::request_passive_allocation(
     return prepare_passive_mutation(node, false);
 }
 
 bool DungeonSession::request_passive_refund(
     passives::PassiveNodeId node) noexcept {
     return prepare_passive_mutation(node, true);
 }
 
 bool DungeonSession::prepare_passive_mutation(
     passives::PassiveNodeId node, bool refund) noexcept {
+    if (health_potion_abyss_clear_retry_gate_active()) return false;
     if (!pending_item_cache_consistent()) {
         enter_fault(DungeonFault::save_receipt_mismatch);
         return false;
     }
     if (phase_ != RoomPhase::awaiting_exit || !combat_.has_value()
             || pending_save_.has_value()) {
         return false;
     }
 
     DungeonRunState next = stable_state_;
@@ -694,26 +699,137 @@ bool DungeonSession::pending_material_cache_consistent() const noexcept {
         const std::uint16_t ordinal = pending_save_->pickup_ordinal;
         if (ordinal >= ground_materials_.size()
                 || !append_ground(ground_materials_[ordinal])) {
             return false;
         }
     } else {
         for (const GroundMaterial& ground : ground_materials_) {
             if (ground.active && !append_ground(ground)) return false;
         }
     }
+    if (vacuum && pending_save_->health_potion_claim.has_value()) {
+        const PendingHealthPotionClaim& claim =
+            *pending_save_->health_potion_claim;
+        if (claim.count == 0U
+                || claim.count > kPendingHealthPotionClaimCapacity) {
+            return false;
+        }
+        for (std::uint8_t index = 0U; index < claim.count; ++index) {
+            const std::uint16_t ordinal = health_potion_claim_ordinal(
+                claim.spawn_ordinals[index]);
+            if (bit_is_set(expected_claims, ordinal)) return false;
+            set_bit(expected_claims, ordinal);
+        }
+    }
     return next.materials == expected_counts
         && next.material_discovery_bits == expected_discovery
         && next.material_claimed_drop_bits == expected_claims;
 }
 
+bool DungeonSession::pending_health_potion_cache_consistent() const noexcept {
+    if (!pending_save_.has_value()) return true;
+    const PendingSaveKind kind = pending_save_->kind;
+    const bool pickup = kind == PendingSaveKind::health_potion_pickup;
+    const bool clear = kind == PendingSaveKind::room_clear
+        || kind == PendingSaveKind::abyss_clear;
+    const bool has_claim = pending_save_->health_potion_claim.has_value();
+    if (pickup != has_claim && !clear) return false;
+    if (!pickup && !clear) return !has_claim;
+    if (!has_claim) {
+        return !clear || !has_claimable_health_potion();
+    }
+    if (!combat_.has_value()) return false;
+
+    const PendingHealthPotionClaim& claim =
+        *pending_save_->health_potion_claim;
+    if (claim.count == 0U
+            || claim.count > kPendingHealthPotionClaimCapacity) {
+        return false;
+    }
+    const combat::CombatSnapshot snapshot = combat_->snapshot();
+    if (claim.expected_hp != snapshot.player.hp
+            || claim.expected_max_hp != snapshot.player.max_hp) {
+        return false;
+    }
+    for (std::uint8_t index = 0U; index < claim.count; ++index) {
+        const std::uint16_t spawn = claim.spawn_ordinals[index];
+        if (spawn >= ground_health_potions_.size()
+                || (index != 0U
+                    && claim.spawn_ordinals[index - 1U] >= spawn)) {
+            return false;
+        }
+        const GroundHealthPotion& ground = ground_health_potions_[spawn];
+        const std::uint16_t claim_ordinal =
+            health_potion_claim_ordinal(spawn);
+        if (!ground.active || ground.spawn_ordinal != spawn
+                || ground.claim_ordinal != claim_ordinal
+                || bit_is_set(
+                    stable_state_.item_ownership.material_claimed_drop_bits,
+                    claim_ordinal)
+                || !bit_is_set(pending_save_->next_state.item_ownership
+                        .material_claimed_drop_bits,
+                    claim_ordinal)) {
+            return false;
+        }
+    }
+    if (clear) {
+        std::array<std::uint16_t, kPendingHealthPotionClaimCapacity>
+            expected_spawns{};
+        std::uint8_t expected_count = 0U;
+        int projected_hp = claim.expected_hp;
+        const int restore = combat::scale_basis_points(
+            claim.expected_max_hp, kHealthPotionRestoreBp,
+            combat::BasisPointRounding::ceil);
+        for (std::uint16_t spawn = 0U;
+             spawn < ground_health_potions_.size(); ++spawn) {
+            if (!ground_health_potions_[spawn].active) continue;
+            expected_spawns[expected_count++] = spawn;
+            projected_hp = (std::min)(
+                claim.expected_max_hp, projected_hp + restore);
+            const bool strictly_above_threshold =
+                static_cast<std::int64_t>(projected_hp) * 10000
+                    > static_cast<std::int64_t>(claim.expected_max_hp)
+                        * kHealthPotionAutoUseThresholdBp;
+            if (strictly_above_threshold
+                    || expected_count == kPendingHealthPotionClaimCapacity) {
+                break;
+            }
+        }
+        if (expected_count != claim.count) return false;
+        for (std::uint8_t index = 0U; index < expected_count; ++index) {
+            if (expected_spawns[index] != claim.spawn_ordinals[index]) {
+                return false;
+            }
+        }
+        return true;
+    }
+    if (claim.count != 1U
+            || pending_save_->pickup_ordinal != claim.spawn_ordinals[0]
+            || pending_save_->expected_generation
+                != stable_state_.commit_generation + 1U
+            || pending_save_->next_state.commit_generation
+                != pending_save_->expected_generation) {
+        return false;
+    }
+
+    DungeonRunState& expected = death_validation_scratch_;
+    if (!copy_run_state_reusing_items(expected, stable_state_)) return false;
+    expected.commit_generation = pending_save_->expected_generation;
+    set_bit(expected.item_ownership.material_claimed_drop_bits,
+        health_potion_claim_ordinal(claim.spawn_ordinals[0]));
+    return same_run_state(expected, pending_save_->next_state);
+}
+
 RequestResult DungeonSession::request_equip(std::uint64_t item_id) noexcept {
+    if (health_potion_abyss_clear_retry_gate_active()) {
+        return RequestResult::rejected;
+    }
     if (!pending_item_cache_consistent()) {
         enter_fault(DungeonFault::save_receipt_mismatch);
         return RequestResult::faulted;
     }
     if (!item_request_phase(phase_) || !combat_.has_value()
             || pending_save_.has_value() || item_id == 0U) {
         return RequestResult::rejected;
     }
     const items::OwnershipValidationResult stable_validation =
         items::validate_ownership_detailed(stable_state_.item_ownership);
@@ -746,20 +862,23 @@ RequestResult DungeonSession::request_equip(std::uint64_t item_id) noexcept {
         return prepare_item_save(
             std::move(next), PendingSaveKind::equipment, phase_);
     } catch (const std::bad_alloc&) {
         return RequestResult::rejected;
     } catch (...) {
         return RequestResult::rejected;
     }
 }
 
 RequestResult DungeonSession::request_unequip(items::ItemSlot slot) noexcept {
+    if (health_potion_abyss_clear_retry_gate_active()) {
+        return RequestResult::rejected;
+    }
     if (!pending_item_cache_consistent()) {
         enter_fault(DungeonFault::save_receipt_mismatch);
         return RequestResult::faulted;
     }
     const std::size_t slot_index = static_cast<std::size_t>(slot);
     if (!item_request_phase(phase_) || !combat_.has_value()
             || pending_save_.has_value()
             || slot_index >= static_cast<std::size_t>(items::ItemSlot::count)) {
         return RequestResult::rejected;
     }
@@ -786,20 +905,23 @@ RequestResult DungeonSession::request_unequip(items::ItemSlot slot) noexcept {
         return RequestResult::rejected;
     } catch (...) {
         return RequestResult::rejected;
     }
 }
 
 RequestResult DungeonSession::request_craft(
     items::MaterialId material,
     std::uint64_t item_id,
     std::optional<items::DirectedCategory> directed_category) noexcept {
+    if (health_potion_abyss_clear_retry_gate_active()) {
+        return RequestResult::rejected;
+    }
     if (!pending_item_cache_consistent()) {
         enter_fault(DungeonFault::save_receipt_mismatch);
         return RequestResult::faulted;
     }
     const std::size_t material_index = items::material_index(material);
     if (!item_request_phase(phase_) || !combat_.has_value()
             || pending_save_.has_value() || item_id == 0U
             || material_index >= items::kMaterialCount
             || !items::material_is_crafting_currency(material)) {
         return RequestResult::rejected;
@@ -842,20 +964,23 @@ RequestResult DungeonSession::request_craft(
             std::move(next), PendingSaveKind::craft, phase_);
     } catch (const std::bad_alloc&) {
         return RequestResult::rejected;
     } catch (...) {
         return RequestResult::rejected;
     }
 }
 
 RequestResult DungeonSession::request_reinforcement(
     std::uint64_t item_id) noexcept {
+    if (health_potion_abyss_clear_retry_gate_active()) {
+        return RequestResult::rejected;
+    }
     if (!pending_item_cache_consistent()) {
         enter_fault(DungeonFault::save_receipt_mismatch);
         return RequestResult::faulted;
     }
     const std::size_t stone = items::material_index(
         items::MaterialId::reinforcement_stone);
     if (!item_request_phase(phase_) || !combat_.has_value()
             || pending_save_.has_value() || item_id == 0U
             || stone >= items::kMaterialCount
             || stable_state_.item_ownership.materials[stone] == 0U) {
@@ -931,20 +1056,23 @@ RequestResult DungeonSession::request_reinforcement(
             PendingSaveKind::reinforcement, phase_, receipt);
     } catch (const std::bad_alloc&) {
         return RequestResult::rejected;
     } catch (...) {
         return RequestResult::rejected;
     }
 }
 
 RequestResult DungeonSession::request_coupon(
     items::MaterialId coupon, std::uint64_t item_id) noexcept {
+    if (health_potion_abyss_clear_retry_gate_active()) {
+        return RequestResult::rejected;
+    }
     if (!pending_item_cache_consistent()) {
         enter_fault(DungeonFault::save_receipt_mismatch);
         return RequestResult::faulted;
     }
     const std::size_t coupon_index = items::material_index(coupon);
     if (!item_request_phase(phase_) || !combat_.has_value()
             || pending_save_.has_value() || item_id == 0U
             || coupon_index >= items::kMaterialCount
             || !items::material_is_coupon(coupon)
             || stable_state_.item_ownership.materials[coupon_index] == 0U) {
@@ -983,20 +1111,23 @@ RequestResult DungeonSession::request_coupon(
             PendingSaveKind::reinforcement, phase_, receipt);
     } catch (const std::bad_alloc&) {
         return RequestResult::rejected;
     } catch (...) {
         return RequestResult::rejected;
     }
 }
 
 RequestResult DungeonSession::request_recipe(
     const std::array<std::uint64_t, 3>& item_ids) noexcept {
+    if (health_potion_abyss_clear_retry_gate_active()) {
+        return RequestResult::rejected;
+    }
     if (!pending_item_cache_consistent()) {
         enter_fault(DungeonFault::save_receipt_mismatch);
         return RequestResult::faulted;
     }
     if (!item_request_phase(phase_) || !combat_.has_value()
             || pending_save_.has_value() || item_ids[0] == 0U
             || item_ids[1] == 0U || item_ids[2] == 0U
             || item_ids[0] == item_ids[1] || item_ids[0] == item_ids[2]
             || item_ids[1] == item_ids[2]) {
         return RequestResult::rejected;
@@ -1067,20 +1198,23 @@ RequestResult DungeonSession::request_recipe(
             std::move(next), PendingSaveKind::recipe, phase_);
     } catch (const std::bad_alloc&) {
         return RequestResult::rejected;
     } catch (...) {
         return RequestResult::rejected;
     }
 }
 
 RequestResult DungeonSession::request_pickup(
     std::uint16_t drop_ordinal) noexcept {
+    if (health_potion_abyss_clear_retry_gate_active()) {
+        return RequestResult::rejected;
+    }
     if (!pending_item_cache_consistent()) {
         enter_fault(DungeonFault::save_receipt_mismatch);
         return RequestResult::faulted;
     }
     if (!item_request_phase(phase_) || pending_save_.has_value()
             || drop_ordinal >= ground_items_.size()) {
         return RequestResult::rejected;
     }
     const GroundItem& ground = ground_items_[drop_ordinal];
     if (!ground.active || ground.drop_ordinal != drop_ordinal) {
@@ -1196,20 +1330,23 @@ RequestResult DungeonSession::request_pickup(
         return RequestResult::accepted;
     } catch (const std::bad_alloc&) {
         return RequestResult::rejected;
     } catch (...) {
         return RequestResult::rejected;
     }
 }
 
 RequestResult DungeonSession::request_material_pickup(
     std::uint16_t ordinal) noexcept {
+    if (health_potion_abyss_clear_retry_gate_active()) {
+        return RequestResult::rejected;
+    }
     if (!pending_item_cache_consistent()
             || !pending_material_cache_consistent()) {
         enter_fault(DungeonFault::save_receipt_mismatch);
         return RequestResult::faulted;
     }
     if (!item_request_phase(phase_) || pending_save_.has_value()
             || ordinal >= ground_materials_.size()) {
         return RequestResult::rejected;
     }
     const GroundMaterial& ground = ground_materials_[ordinal];
@@ -1267,20 +1404,94 @@ RequestResult DungeonSession::request_material_pickup(
         });
         phase_ = RoomPhase::committing;
         return RequestResult::accepted;
     } catch (const std::bad_alloc&) {
         return RequestResult::rejected;
     } catch (...) {
         return RequestResult::rejected;
     }
 }
 
+RequestResult DungeonSession::request_health_potion_pickup(
+    std::uint16_t spawn_ordinal) noexcept {
+    if (health_potion_abyss_clear_retry_gate_active()) {
+        return RequestResult::rejected;
+    }
+    if (!pending_item_cache_consistent()
+            || !pending_material_cache_consistent()
+            || !pending_health_potion_cache_consistent()) {
+        enter_fault(DungeonFault::save_receipt_mismatch);
+        return RequestResult::faulted;
+    }
+    if (!item_request_phase(phase_) || pending_save_.has_value()
+            || !combat_.has_value()
+            || spawn_ordinal >= ground_health_potions_.size()) {
+        return RequestResult::rejected;
+    }
+    const GroundHealthPotion& ground = ground_health_potions_[spawn_ordinal];
+    const combat::CombatSnapshot snapshot = combat_->snapshot();
+    if (!ground.active || ground.spawn_ordinal != spawn_ordinal
+            || ground.claim_ordinal != health_potion_claim_ordinal(spawn_ordinal)
+            || combat_->death_snapshot().has_value()
+            || !health_potion_auto_use_eligible(
+                snapshot.player.hp, snapshot.player.max_hp)) {
+        return RequestResult::rejected;
+    }
+    if (bit_is_set(stable_state_.item_ownership.material_claimed_drop_bits,
+            ground.claim_ordinal)) {
+        enter_fault(DungeonFault::invalid_item_state);
+        return RequestResult::faulted;
+    }
+    if (stable_state_.commit_generation
+            == (std::numeric_limits<std::uint64_t>::max)()) {
+        enter_fault(DungeonFault::commit_generation_overflow);
+        return RequestResult::faulted;
+    }
+    PendingSave& pending = pending_save_.prepare();
+    if (!copy_run_state_reusing_items(pending.next_state, stable_state_)) {
+        pending_save_.reset();
+        return RequestResult::rejected;
+    }
+    ++pending.next_state.commit_generation;
+    set_bit(pending.next_state.item_ownership.material_claimed_drop_bits,
+        ground.claim_ordinal);
+    pending.kind = PendingSaveKind::health_potion_pickup;
+    pending.expected_generation = pending.next_state.commit_generation;
+    pending.transition = TransitionKind::none;
+    pending.direction = ExitDirection::none;
+    pending.resume_phase = phase_;
+    pending.pickup_ordinal = spawn_ordinal;
+    pending.death_snapshot.reset();
+    pending.reinforcement_receipt.reset();
+    pending.health_potion_claim = PendingHealthPotionClaim{
+        {{spawn_ordinal, 0U, 0U, 0U}}, 1U,
+        snapshot.player.hp, snapshot.player.max_hp};
+    phase_ = RoomPhase::committing;
+    return RequestResult::accepted;
+}
+
+void DungeonSession::apply_committed_health_potions(
+    const PendingHealthPotionClaim& claim, bool room_clear) noexcept {
+    HealthPotionPickupReceipt receipt{};
+    receipt.valid = true;
+    receipt.room_clear = room_clear;
+    receipt.commit_generation = stable_state_.commit_generation;
+    for (std::uint8_t index = 0U; index < claim.count; ++index) {
+        const std::uint16_t spawn = claim.spawn_ordinals[index];
+        receipt.restored_hp += combat_->restore_player_health_percent(
+            kHealthPotionRestoreBp);
+        ground_health_potions_[spawn] = {};
+        ++receipt.consumed_count;
+    }
+    health_potion_pickup_receipt_ = receipt;
+}
+
 RequestResult DungeonSession::request_remove_active_skill(
     std::uint8_t slot) noexcept {
     return request_skill_loadout_mutation(
         SkillLoadoutMutation::remove,
         skills::ActiveSkillId::none, slot, 0U);
 }
 
 RequestResult DungeonSession::request_equip_active_skill(
     skills::ActiveSkillId skill, std::uint8_t slot) noexcept {
     return request_skill_loadout_mutation(
@@ -1292,20 +1503,23 @@ RequestResult DungeonSession::request_swap_active_skill_slots(
     return request_skill_loadout_mutation(
         SkillLoadoutMutation::swap,
         skills::ActiveSkillId::none, left, right);
 }
 
 RequestResult DungeonSession::request_skill_loadout_mutation(
     SkillLoadoutMutation mutation,
     skills::ActiveSkillId skill,
     std::uint8_t left,
     std::uint8_t right) noexcept {
+    if (health_potion_abyss_clear_retry_gate_active()) {
+        return RequestResult::rejected;
+    }
     if (!pending_item_cache_consistent()) {
         enter_fault(DungeonFault::save_receipt_mismatch);
         return RequestResult::faulted;
     }
     const std::size_t left_index = static_cast<std::size_t>(left);
     const std::size_t right_index = static_cast<std::size_t>(right);
     if (!item_request_phase(phase_) || pending_save_.has_value()) {
         return RequestResult::rejected;
     }
     const bool invalid_remove = mutation == SkillLoadoutMutation::remove
@@ -1363,24 +1577,31 @@ bool auto_pickup_eligible(
     if (!ground.active) return false;
     if (ground.source == GroundItemSource::abyss_chest) return true;
     return ground.source == GroundItemSource::monster_drop
         && static_cast<std::uint8_t>(ground.item.rarity)
             >= static_cast<std::uint8_t>(policy.minimum_rarity);
 }
 
 void DungeonSession::request_nearby_pickups(
     combat::Vec3 player_position,
     AutoPickupPolicy pickup_policy) noexcept {
+    if (health_potion_abyss_clear_retry_gate_active()) return;
     if (!item_request_phase(phase_) || !combat_.has_value()
             || pending_save_.has_value()) {
         return;
     }
+    for (std::uint16_t spawn = 0U;
+         spawn < ground_health_potions_.size(); ++spawn) {
+        if (!ground_health_potions_[spawn].active) continue;
+        const RequestResult result = request_health_potion_pickup(spawn);
+        if (result != RequestResult::rejected) return;
+    }
     for (std::uint16_t ordinal = 0U;
          ordinal < ground_items_.size(); ++ordinal) {
         const GroundItem& ground = ground_items_[ordinal];
         if (!auto_pickup_eligible(ground, pickup_policy)
                 || !pickup_distance_ok(player_position, ground.position)) {
             continue;
         }
         const RequestResult result = request_pickup(ordinal);
         if (result != RequestResult::rejected) return;
     }
@@ -1418,99 +1639,131 @@ void DungeonSession::commit_pending_save(
         return;
     }
     if (!pending_abyss_claim_cache_consistent()) {
         enter_fault(DungeonFault::save_receipt_mismatch);
         return;
     }
     if (!pending_material_cache_consistent()) {
         enter_fault(DungeonFault::save_receipt_mismatch);
         return;
     }
+    if (!pending_health_potion_cache_consistent()) {
+        enter_fault(DungeonFault::save_receipt_mismatch);
+        return;
+    }
     if (!pending_death_cache_consistent()) {
         enter_fault(DungeonFault::save_receipt_mismatch);
         return;
     }
     const bool death_pending = pending_save_->kind
             == PendingSaveKind::death_retreat
         || pending_save_->kind == PendingSaveKind::death_continue;
     if (death_pending && (!result.kind.has_value()
             || *result.kind != pending_save_->kind)) {
         enter_fault(DungeonFault::save_receipt_mismatch);
         return;
     }
+    if (pending_save_->health_potion_claim.has_value()
+            && result.kind.has_value()
+            && *result.kind != pending_save_->kind) {
+        enter_fault(DungeonFault::save_receipt_mismatch);
+        return;
+    }
     if (result.disposition == SaveDisposition::indeterminate) {
         enter_fault(DungeonFault::save_commit_indeterminate);
         return;
     }
     const bool abyss_commit = pending_save_->kind == PendingSaveKind::abyss_start
         || pending_save_->kind == PendingSaveKind::abyss_fail
         || pending_save_->kind == PendingSaveKind::abyss_clear
         || pending_save_->kind
             == PendingSaveKind::abyss_reward_materialized;
     const bool claim_pending = pending_save_->kind
         == PendingSaveKind::abyss_reward_claim;
     const bool abandon_pending = pending_save_->kind
         == PendingSaveKind::abyss_abandon;
     const bool protected_abyss_commit = abyss_commit || claim_pending
         || abandon_pending;
+    const bool retryable_health_potion_abyss_clear =
+        pending_save_->kind == PendingSaveKind::abyss_clear
+        && pending_save_->health_potion_claim.has_value();
     if (result.disposition == SaveDisposition::not_committed
-            && protected_abyss_commit) {
+            && protected_abyss_commit
+            && !retryable_health_potion_abyss_clear) {
         enter_fault(DungeonFault::save_receipt_mismatch);
         return;
     }
     if (result.disposition == SaveDisposition::not_committed) {
         const RoomPhase resume_phase = pending_save_->resume_phase;
         death_continue_failed_ = pending_save_->kind
             == PendingSaveKind::death_continue;
         phase_ = resume_phase;
         saturating_increment(diagnostics_.save_failure_count);
         static_cast<void>(emit(
             DungeonEventKind::save_failed,
             &stable_state_,
             &pending_save_->next_state,
             pending_save_->next_state.last_transition,
             pending_save_->next_state.last_direction));
         pending_save_.reset();
         pending_item_build_.reset();
+        retry_health_potion_abyss_clear_before_combat_ =
+            retryable_health_potion_abyss_clear
+            && phase_ != RoomPhase::faulted;
         return;
     }
     if (result.generation != pending_save_->expected_generation
             || pending_save_->expected_generation
                 != pending_save_->next_state.commit_generation
             || !same_run_state(
                 result.verified_state, pending_save_->next_state)) {
         enter_fault(DungeonFault::save_receipt_mismatch);
         return;
     }
 
+    retry_health_potion_abyss_clear_before_combat_ = false;
     const PendingSaveKind kind = pending_save_->kind;
     const bool item_commit = kind == PendingSaveKind::equipment
         || kind == PendingSaveKind::craft
         || kind == PendingSaveKind::recipe
         || kind == PendingSaveKind::reinforcement;
     const bool reinforcement_commit = kind == PendingSaveKind::reinforcement;
     const bool skill_loadout_commit = kind == PendingSaveKind::skill_loadout;
     const bool pickup_commit = kind == PendingSaveKind::loot_pickup;
     const bool material_pickup_commit =
         kind == PendingSaveKind::material_pickup;
+    const bool health_potion_pickup_commit =
+        kind == PendingSaveKind::health_potion_pickup;
     const bool claim_commit = kind == PendingSaveKind::abyss_reward_claim;
     const bool start_commit = kind == PendingSaveKind::abyss_start;
     const bool fail_commit = kind == PendingSaveKind::abyss_fail;
     const bool abyss_clear_commit = kind == PendingSaveKind::abyss_clear;
     const bool room_clear_commit = kind == PendingSaveKind::room_clear;
     const bool clear_commit = abyss_clear_commit || room_clear_commit;
     const bool reward_commit = kind
         == PendingSaveKind::abyss_reward_materialized;
     const bool death_retreat_commit = kind == PendingSaveKind::death_retreat;
     const bool death_continue_commit = kind == PendingSaveKind::death_continue;
     const bool death_commit = death_retreat_commit || death_continue_commit;
     const std::uint16_t pickup_ordinal = pending_save_->pickup_ordinal;
+    std::optional<PendingHealthPotionClaim> committed_health_claim =
+        pending_save_->health_potion_claim;
+    if (committed_health_claim.has_value() && !combat_.has_value()) {
+        enter_fault(DungeonFault::save_receipt_mismatch);
+        return;
+    }
+    if (health_potion_pickup_commit) {
+        if (!committed_health_claim.has_value()) {
+            enter_fault(DungeonFault::save_receipt_mismatch);
+            return;
+        }
+    }
     if (pickup_commit) {
         if (pickup_ordinal >= ground_items_.size()) {
             enter_fault(DungeonFault::save_receipt_mismatch);
             return;
         }
         const GroundItem& ground = ground_items_[pickup_ordinal];
         const items::ItemInstance* published = find_item(
             pending_save_->next_state.item_ownership, ground.item.id);
         if (!ground.active || ground.drop_ordinal != pickup_ordinal
                 || published == nullptr
@@ -1632,37 +1885,42 @@ void DungeonSession::commit_pending_save(
         return;
     }
     if (clear_commit) {
         settle_room_experience();
         room_progression_ = stable_state_.progression;
         ground_materials_ = {};
         material_pickup_receipt_ = published_material_receipt;
         if (abyss_clear_commit) {
             combat_->clear_abyss_rule_preserving_resources();
         }
+        if (committed_health_claim.has_value()) {
+            apply_committed_health_potions(
+                *committed_health_claim, true);
+        }
         // prepare_room_clear reserved both publication slots. While committing,
         // tick() is frozen and no other dungeon-event producer can consume them.
         publish_room_clear();
         return;
     }
     if (reward_commit) {
         ground_items_[published_reward.ground_index] = published_reward.ground;
         phase_ = resume_phase;
         return;
     }
     if (kind == PendingSaveKind::transition
             || kind == PendingSaveKind::abyss_abandon) {
         clear_abyss_exit_confirmation();
         ground_items_ = {};
         rolled_drop_bits_ = {};
         ground_materials_ = {};
         rolled_material_bits_ = {};
+        ground_health_potions_ = {};
         combat_.reset();
         phase_ = RoomPhase::transitioning;
         emit_committed(previous_room, stable_state_);
         return;
     }
     if (skill_loadout_commit) {
         phase_ = resume_phase;
         return;
     }
     room_progression_ = stable_state_.progression;
@@ -1678,14 +1936,19 @@ void DungeonSession::commit_pending_save(
         ground_items_[pickup_ordinal] = GroundItem{};
         phase_ = resume_phase;
         return;
     }
     if (material_pickup_commit) {
         ground_materials_[pickup_ordinal] = GroundMaterial{};
         material_pickup_receipt_ = published_material_receipt;
         phase_ = resume_phase;
         return;
     }
+    if (health_potion_pickup_commit) {
+        apply_committed_health_potions(*committed_health_claim, false);
+        phase_ = resume_phase;
+        return;
+    }
     phase_ = RoomPhase::awaiting_exit;
 }
 
 }  // namespace arpg::dungeon
diff --git a/src/dungeon/dungeon_types.hpp b/src/dungeon/dungeon_types.hpp
index a45b505..7f8d45d 100644
--- a/src/dungeon/dungeon_types.hpp
+++ b/src/dungeon/dungeon_types.hpp
@@ -1,15 +1,16 @@
 #pragma once
 
 #include "combat/combat_types.hpp"
 #include "dungeon/dungeon_checkpoint.hpp"
 #include "dungeon/dungeon_rules.hpp"
+#include "dungeon/health_potion_loot.hpp"
 #include "dungeon/material_loot.hpp"
 
 #include <array>
 #include <cstddef>
 #include <cstdint>
 #include <optional>
 
 namespace arpg::dungeon {
 
 using ExitDirection = checkpoint::ExitDirection;
@@ -87,20 +88,21 @@ struct AbyssExitConfirmation final {
 
 struct DungeonDiagnostics final {
     std::uint32_t event_overflow_count{};
     std::uint32_t combat_relay_overflow_count{};
     std::uint32_t rejected_exit_count{};
     std::uint32_t save_failure_count{};
     std::uint32_t ground_saturation_count{};
     std::uint32_t material_ground_saturation_count{};
     DungeonFault fault{DungeonFault::none};
     bool room_index_overflow{};
+    std::uint32_t health_potion_ground_saturation_count{};
 };
 
 struct DungeonEncounterDiagnostics final {
     std::uint8_t total_budget{};
     std::uint8_t current_wave_budget{};
     std::uint8_t current_wave_spawn_count{};
     bool plan_valid{};
 };
 
 struct DungeonSessionConfig final {
@@ -109,20 +111,21 @@ struct DungeonSessionConfig final {
 };
 
 struct RoomDescriptor final {
     std::uint64_t index{};
     std::uint64_t seed{};
     EntrySide entry{EntrySide::initial};
     combat::CombatLabConfig combat{};
 };
 
 inline constexpr std::size_t kGroundDropCapacity = 192U;
+static_assert(kGroundHealthPotionCapacity == kGroundDropCapacity);
 inline constexpr float kPickupRadius = 1.5F;
 
 enum class GroundItemSource : std::uint8_t {
     monster_drop,
     abyss_chest,
 };
 
 struct GroundItem final {
     bool active{};
     std::uint16_t drop_ordinal{};
@@ -228,20 +231,25 @@ struct DungeonSnapshot final {
     std::optional<std::uint16_t> pending_pickup_ordinal{};
     std::optional<std::uint16_t> pending_material_pickup_ordinal{};
     std::optional<DeathSnapshot> death{};
     std::optional<combat::CombatSnapshot> combat{};
     DungeonEncounterDiagnostics encounter{};
     DungeonDiagnostics diagnostics{};
     progression::ProgressionState progression{};
     std::uint64_t pending_room_experience{};
     std::uint64_t last_room_experience{};
     std::uint8_t last_levels_gained{};
+    std::uint16_t ground_health_potion_count{};
+    std::array<GroundHealthPotionSnapshot, kGroundHealthPotionCapacity>
+        ground_health_potions{};
+    HealthPotionPickupReceipt health_potion_pickup_receipt{};
+    std::optional<std::uint16_t> pending_health_potion_spawn_ordinal{};
 };
 
 struct PendingTransition final {
     TransitionKind kind{TransitionKind::none};
     ExitDirection direction{ExitDirection::none};
     std::uint64_t expected_generation{};
     DungeonRunState next_state{};
 };
 
 enum class PendingSaveKind : std::uint8_t {
@@ -256,32 +264,34 @@ enum class PendingSaveKind : std::uint8_t {
     reinforcement,
     skill_loadout,
     abyss_start,
     abyss_fail,
     abyss_clear,
     abyss_reward_materialized,
     abyss_reward_claim,
     abyss_abandon,
     death_retreat,
     death_continue,
+    health_potion_pickup,
 };
 
 struct PendingSave final {
     PendingSaveKind kind{PendingSaveKind::transition};
     std::uint64_t expected_generation{};
     DungeonRunState next_state{};
     TransitionKind transition{TransitionKind::none};
     ExitDirection direction{ExitDirection::none};
     RoomPhase resume_phase{RoomPhase::awaiting_exit};
     std::uint16_t pickup_ordinal{0xFFFFU};
     std::optional<combat::CombatDeathSnapshot> death_snapshot{};
     std::optional<ReinforcementReceipt> reinforcement_receipt{};
+    std::optional<PendingHealthPotionClaim> health_potion_claim{};
 };
 
 struct PendingSaveResult final {
     SaveDisposition disposition{SaveDisposition::indeterminate};
     std::uint64_t generation{};
     DungeonRunState verified_state{};
     std::optional<PendingSaveKind> kind{};
 };
 
 using TransitionSaveResult = PendingSaveResult;
diff --git a/src/dungeon/health_potion_loot.cpp b/src/dungeon/health_potion_loot.cpp
new file mode 100644
index 0000000..706189e
--- /dev/null
+++ b/src/dungeon/health_potion_loot.cpp
@@ -0,0 +1,29 @@
+#include "dungeon/health_potion_loot.hpp"
+
+#include "core/deterministic_rng.hpp"
+
+namespace arpg::dungeon {
+namespace {
+
+constexpr std::uint64_t kHealthPotionChanceDomain = 0x48505F504F544E31ULL;
+
+}  // namespace
+
+bool roll_health_potion_drop(
+    std::uint64_t room_seed, std::uint16_t spawn_ordinal) noexcept {
+    auto ordinal = core::DeterministicRng::derive_stream(
+        room_seed, static_cast<std::uint64_t>(spawn_ordinal));
+    auto chance = core::DeterministicRng::derive_stream(
+        ordinal.next_u64(), kHealthPotionChanceDomain);
+    return chance.next_bounded(10000U).value_or(9999U)
+        < kHealthPotionDropChanceBp;
+}
+
+bool health_potion_auto_use_eligible(int hp, int max_hp) noexcept {
+    if (hp <= 0 || max_hp <= 0 || hp > max_hp) return false;
+    return static_cast<std::int64_t>(hp) * 10000
+        <= static_cast<std::int64_t>(max_hp)
+            * kHealthPotionAutoUseThresholdBp;
+}
+
+}  // namespace arpg::dungeon
diff --git a/src/dungeon/health_potion_loot.hpp b/src/dungeon/health_potion_loot.hpp
new file mode 100644
index 0000000..c9cde8a
--- /dev/null
+++ b/src/dungeon/health_potion_loot.hpp
@@ -0,0 +1,56 @@
+#pragma once
+
+#include "combat/combat_types.hpp"
+
+#include <array>
+#include <cstddef>
+#include <cstdint>
+#include <optional>
+
+namespace arpg::dungeon {
+
+inline constexpr std::uint16_t kHealthPotionDropChanceBp = 3000U;
+inline constexpr std::uint16_t kHealthPotionRestoreBp = 2500U;
+inline constexpr std::uint16_t kHealthPotionAutoUseThresholdBp = 7500U;
+inline constexpr std::size_t kGroundHealthPotionCapacity = 192U;
+inline constexpr std::size_t kPendingHealthPotionClaimCapacity = 4U;
+
+[[nodiscard]] constexpr std::uint16_t health_potion_claim_ordinal(
+    std::uint16_t spawn_ordinal) noexcept {
+    return static_cast<std::uint16_t>(spawn_ordinal * 2U + 1U);
+}
+
+struct GroundHealthPotion final {
+    bool active{};
+    std::uint16_t spawn_ordinal{};
+    std::uint16_t claim_ordinal{};
+    combat::Vec3 position{};
+};
+
+struct GroundHealthPotionSnapshot final {
+    std::uint16_t spawn_ordinal{};
+    std::uint16_t claim_ordinal{};
+    combat::Vec3 position{};
+};
+
+struct HealthPotionPickupReceipt final {
+    bool valid{};
+    bool room_clear{};
+    std::uint16_t consumed_count{};
+    std::uint64_t commit_generation{};
+    int restored_hp{};
+};
+
+struct PendingHealthPotionClaim final {
+    std::array<std::uint16_t, kPendingHealthPotionClaimCapacity> spawn_ordinals{};
+    std::uint8_t count{};
+    int expected_hp{};
+    int expected_max_hp{};
+};
+
+[[nodiscard]] bool roll_health_potion_drop(
+    std::uint64_t room_seed, std::uint16_t spawn_ordinal) noexcept;
+[[nodiscard]] bool health_potion_auto_use_eligible(
+    int hp, int max_hp) noexcept;
+
+}  // namespace arpg::dungeon
diff --git a/tests/combat/combat_test_main.cpp b/tests/combat/combat_test_main.cpp
index 4aad3f8..85e7f44 100644
--- a/tests/combat/combat_test_main.cpp
+++ b/tests/combat/combat_test_main.cpp
@@ -124,12 +124,12 @@ int main() {
         monster_special_suite(),
         monster_pool_suite(),
         player_health_suite(),
         player_build_suite(),
         player_damage_history_suite(),
         player_death_snapshot_suite(),
         player_defense_suite(),
         storm_swords_skill_suite(),
     };
 
-    return arpg::test::run_suites(suites, 243, "stage 18 combat queries");
+    return arpg::test::run_suites(suites, 245, "stage 18 combat queries");
 }
diff --git a/tests/combat/player_health_tests.cpp b/tests/combat/player_health_tests.cpp
index d91dbad..29f267f 100644
--- a/tests/combat/player_health_tests.cpp
+++ b/tests/combat/player_health_tests.cpp
@@ -1,19 +1,20 @@
 #include "test_framework.hpp"
 
 #include "combat_test_support.hpp"
 
 #include "combat/combat_world.hpp"
 
 #include "abyss/abyss_rules.hpp"
 
 #include <cstddef>
+#include <cstdint>
 
 namespace {
 
 using namespace arpg::combat;
 using arpg::test::tick_n;
 
 CombatEncounterConfig single_chaser_encounter() noexcept {
     CombatEncounterConfig config{};
     config.player_spawn = Vec3{0.0F, 0.0F, 0.0F};
     config.initial_facing = Facing::right;
@@ -242,31 +243,76 @@ arpg::test::Failure life_sacrifice_maps_ratio_and_clear_does_not_refill() noexce
     world.clear_abyss_rule_preserving_resources();
     ARPG_REQUIRE(world.snapshot().player.max_hp == 1001);
     ARPG_REQUIRE(world.snapshot().player.hp == 501);
     ARPG_REQUIRE(world.snapshot().player.max_barrier == 101);
     ARPG_REQUIRE(world.snapshot().player.barrier == 37);
     ARPG_REQUIRE(arpg::test::CombatWorldTestAccess::abyss_config(world).rule
                  == arpg::abyss::AbyssRuleId::none);
     return {};
 }
 
+arpg::test::Failure percent_health_restore_uses_actual_max_and_bypasses_abyss_multiplier() noexcept {
+    CombatEncounterConfig config = single_chaser_encounter();
+    config.player_build.values.max_health = 10000;  // actual max becomes 1001
+    CombatWorld world{config};
+    arpg::test::CombatWorldTestAccess::set_player_resources(world, 500, 0);
+    ARPG_REQUIRE(world.restore_player_health_percent(2500U) == 251);
+    ARPG_REQUIRE(world.snapshot().player.hp == 751);
+    arpg::test::CombatWorldTestAccess::set_player_resources(world, 900, 0);
+    ARPG_REQUIRE(world.restore_player_health_percent(2500U) == 101);
+    ARPG_REQUIRE(world.snapshot().player.hp == 1001);
+    ARPG_REQUIRE(world.restore_player_health_percent(2500U) == 0);
+
+    config = single_chaser_encounter();
+    config.abyss = arpg::abyss::combat_config_for(
+        arpg::abyss::AbyssRuleId::exhausted_recovery);
+    CombatWorld abyss{config};
+    arpg::test::CombatWorldTestAccess::set_player_resources(abyss, 600, 0);
+    ARPG_REQUIRE(abyss.restore_player_health_percent(2500U) == 250);
+    ARPG_REQUIRE(abyss.snapshot().player.hp == 850);
+    ARPG_REQUIRE(abyss.restore_player_health_percent(0U) == 0);
+    return {};
+}
+
+arpg::test::Failure percent_health_restore_never_revives_defeated_player() noexcept {
+    CombatEncounterConfig config = single_chaser_encounter();
+    config.wave = {};
+    CombatWorld world{config};
+    arpg::test::CombatWorldTestAccess::apply_damage(world,
+        world.snapshot().player.max_hp, Vec3{}, FeedbackLevel::heavy);
+    const auto death = world.death_snapshot();
+    ARPG_REQUIRE(death.has_value());
+    const std::uint64_t death_tick = death->tick;
+    const std::uint64_t final_damage = death->final_damage;
+    ARPG_REQUIRE(world.restore_player_health_percent(2500U) == 0);
+    ARPG_REQUIRE(world.snapshot().player.hp == 0);
+    ARPG_REQUIRE(world.death_snapshot().has_value());
+    ARPG_REQUIRE(world.death_snapshot()->tick == death_tick);
+    ARPG_REQUIRE(world.death_snapshot()->final_damage == final_damage);
+    return {};
+}
+
 constexpr arpg::test::TestCase kCases[] = {
     {"damage reaches zero", &player_damage_reaches_zero},
     {"accepted hit payload and hurt event", &accepted_hit_emits_payload_and_hurt_started},
     {"player defeat event emits once", &player_defeat_event_emits_once},
     {"player defeat latch survives event overflow", &player_defeat_latch_survives_event_overflow},
     {"defeated player rejects and discards input", &defeated_player_rejects_and_discards_input},
     {"hurt movement lock", &hurt_ticks_lock_movement_until_expired},
     {"wave load restores full health", &load_wave_reset_restores_full_health},
     {"exhausted initial cap applied once",
      &exhausted_recovery_caps_initial_resources_once},
     {"exhausted explicit and wave restore",
      &exhausted_recovery_scales_explicit_and_wave_restore},
     {"life sacrifice ratio and clear",
      &life_sacrifice_maps_ratio_and_clear_does_not_refill},
+    {"percent health restore uses actual max and bypasses abyss multiplier",
+     &percent_health_restore_uses_actual_max_and_bypasses_abyss_multiplier},
+    {"percent health restore never revives defeated player",
+     &percent_health_restore_never_revives_defeated_player},
 };
 
 }  // namespace
 
 arpg::test::TestSuite player_health_suite() noexcept {
     return arpg::test::make_suite("player_health", kCases);
 }
diff --git a/tests/dungeon/CMakeLists.txt b/tests/dungeon/CMakeLists.txt
index 29fb77e..186ab87 100644
--- a/tests/dungeon/CMakeLists.txt
+++ b/tests/dungeon/CMakeLists.txt
@@ -12,20 +12,21 @@ add_executable(arpg_dungeon_tests
     dungeon_progression_tests.cpp
     dungeon_query_tests.cpp
     dungeon_passive_tree_tests.cpp
     dungeon_item_transaction_tests.cpp
     dungeon_crafting_transaction_tests.cpp
     dungeon_skill_loadout_transaction_tests.cpp
     dungeon_skill_cast_tests.cpp
     dungeon_equipment_stress_tests.cpp
     dungeon_loot_drop_tests.cpp
     dungeon_material_loot_tests.cpp
+    dungeon_health_potion_tests.cpp
     dungeon_transaction_tests.cpp
     dungeon_abyss_reward_tests.cpp
     dungeon_abyss_stress_tests.cpp
     encounter_director_tests.cpp
     dungeon_wave_tests.cpp)
 target_sources(arpg_dungeon_tests PRIVATE
     dungeon_progression_reward_tests.cpp
     dungeon_affix_reward_tests.cpp
     dungeon_affix_stress_tests.cpp)
 
diff --git a/tests/dungeon/dungeon_health_potion_tests.cpp b/tests/dungeon/dungeon_health_potion_tests.cpp
new file mode 100644
index 0000000..a73dcbe
--- /dev/null
+++ b/tests/dungeon/dungeon_health_potion_tests.cpp
@@ -0,0 +1,1309 @@
+#include "test_framework.hpp"
+
+#include "allocation_probe.hpp"
+#include "dungeon_test_support.hpp"
+
+#include "dungeon/health_potion_loot.hpp"
+#include "dungeon/material_loot.hpp"
+#include "dungeon/dungeon_progression.hpp"
+#include "combat/combat_scaling.hpp"
+#include "persistence/save_store.hpp"
+
+#include <array>
+#include <cstring>
+#include <cstdint>
+#include <filesystem>
+#include <functional>
+#include <optional>
+#include <string>
+#include <utility>
+
+namespace {
+
+using arpg::dungeon::DungeonRules;
+using arpg::dungeon::DungeonRunState;
+using arpg::dungeon::DungeonSession;
+using arpg::dungeon::GroundMaterialSource;
+
+struct PotionTempDirectory final {
+    std::filesystem::path path{};
+
+    PotionTempDirectory() noexcept {
+        std::error_code error;
+        path = std::filesystem::temp_directory_path(error)
+            / "arpg_health_potion_transaction"
+            / std::to_string(static_cast<unsigned long long>(
+                std::hash<std::string>{}(std::to_string(
+                    reinterpret_cast<std::uintptr_t>(this)))));
+        std::filesystem::remove_all(path, error);
+        std::filesystem::create_directories(path, error);
+        if (error) path.clear();
+    }
+
+    ~PotionTempDirectory() noexcept {
+        std::error_code error;
+        std::filesystem::remove_all(path, error);
+    }
+};
+
+struct DropSeedRequirements final {
+    bool common{};
+    bool coupon{};
+    bool potion{};
+};
+
+std::uint64_t find_drop_seed(
+    std::uint16_t spawn_ordinal,
+    std::uint64_t depth,
+    std::uint16_t affix_score,
+    DropSeedRequirements required) noexcept {
+    for (std::uint64_t seed = 1U; seed < 1000000U; ++seed) {
+        const bool common = arpg::dungeon::roll_material_drop(
+            seed, spawn_ordinal, depth, affix_score).has_value();
+        const bool coupon = arpg::dungeon::roll_coupon_drop(
+            seed, spawn_ordinal, depth, affix_score, false).has_value();
+        const bool potion = arpg::dungeon::roll_health_potion_drop(
+            seed, spawn_ordinal);
+        if (common == required.common && coupon == required.coupon
+                && potion == required.potion) {
+            return seed;
+        }
+    }
+    return 0U;
+}
+
+DungeonRunState potion_state(
+    std::uint64_t seed, std::uint64_t depth = 90U) noexcept {
+    DungeonRunState state = arpg::dungeon::make_initial_run_state(
+        0x48505F504F54494FULL, DungeonRules{}).state;
+    state.current_room.seed = seed;
+    state.current_room.depth = depth;
+    return state;
+}
+
+DungeonRunState retry_gate_state(std::uint64_t seed) {
+    DungeonRunState state = potion_state(seed);
+    arpg::items::ItemInstance item{};
+    item.id = 7001U;
+    item.base_id = 1U;
+    item.rarity = arpg::items::ItemRarity::normal;
+    item.item_level = 95U;
+    item.required_level = 1U;
+    state.item_ownership.items.push_back(item);
+    state.item_ownership.next_item_sequence = 9U;
+    state.item_ownership.materials[arpg::items::material_index(
+        arpg::items::MaterialId::transmute)] = 1U;
+    return state;
+}
+
+bool enter_health_potion_abyss_clear_retry(
+    DungeonSession& session, std::uint16_t potion_spawn = 4U) noexcept {
+    arpg::test::set_phase(session, arpg::dungeon::RoomPhase::combat);
+    arpg::test::set_started_abyss_room(
+        session, arpg::abyss::AbyssDanger::low);
+    arpg::test::set_player_health(session, 500, 1000);
+    arpg::test::quiesce_current_room_for_clear_retry(session);
+    arpg::test::install_ground_health_potion(
+        session, potion_spawn, {100.0F, 0.0F, 0.0F});
+    session.tick({});
+    const arpg::dungeon::PendingSave* const pending =
+        session.pending_save_view();
+    if (pending == nullptr
+            || pending->kind != arpg::dungeon::PendingSaveKind::abyss_clear
+            || !pending->health_potion_claim.has_value()) {
+        return false;
+    }
+    session.resolve_pending_save({
+        arpg::dungeon::SaveDisposition::not_committed,
+        pending->expected_generation,
+        pending->next_state,
+        pending->kind,
+    });
+    return session.phase() == arpg::dungeon::RoomPhase::combat
+        && session.pending_save_view() == nullptr
+        && arpg::test::DungeonSessionTestAccess::
+            health_potion_abyss_clear_retry_pending(session);
+}
+
+bool relay_drop(
+    DungeonSession& session,
+    std::uint64_t seed,
+    std::uint16_t spawn_ordinal,
+    std::uint16_t affix_score,
+    arpg::combat::Vec3 position) noexcept {
+    arpg::test::set_current_room_seed(session, seed);
+    const bool relayed = arpg::test::relay_defeated(
+        session,
+        static_cast<std::uint8_t>(spawn_ordinal / 96U),
+        static_cast<std::uint8_t>(spawn_ordinal % 96U),
+        position,
+        true,
+        arpg::combat::MonsterId::fire_bomber,
+        spawn_ordinal,
+        affix_score,
+        false);
+    static_cast<void>(session.try_pop_combat_event());
+    return relayed;
+}
+
+const arpg::dungeon::GroundMaterialSnapshot* find_ground_material_snapshot(
+    const arpg::dungeon::DungeonSnapshot& snapshot,
+    std::uint16_t ordinal) noexcept {
+    for (std::uint16_t index = 0U;
+         index < snapshot.ground_material_count; ++index) {
+        if (snapshot.ground_materials[index].ordinal == ordinal) {
+            return &snapshot.ground_materials[index];
+        }
+    }
+    return nullptr;
+}
+
+bool claim_bit_is_set(
+    const arpg::dungeon::DungeonRunState& state,
+    std::uint16_t ordinal) noexcept {
+    const std::size_t word = static_cast<std::size_t>(ordinal / 64U);
+    const std::uint64_t mask = std::uint64_t{1U} << (ordinal % 64U);
+    return word < state.item_ownership.material_claimed_drop_bits.size()
+        && (state.item_ownership.material_claimed_drop_bits[word] & mask) != 0U;
+}
+
+void install_potion(DungeonSession& session, std::uint16_t spawn,
+    arpg::combat::Vec3 position = {100.0F, 0.0F, 0.0F}) noexcept {
+    arpg::test::install_ground_health_potion(session, spawn, position);
+}
+
+arpg::test::Failure health_potion_rules_are_frozen() noexcept {
+    static_assert(arpg::dungeon::kHealthPotionDropChanceBp == 3000U);
+    static_assert(arpg::dungeon::kHealthPotionRestoreBp == 2500U);
+    static_assert(arpg::dungeon::kHealthPotionAutoUseThresholdBp == 7500U);
+    static_assert(arpg::dungeon::kGroundHealthPotionCapacity == 192U);
+    ARPG_REQUIRE(arpg::dungeon::health_potion_claim_ordinal(0U) == 1U);
+    ARPG_REQUIRE(arpg::dungeon::health_potion_claim_ordinal(191U) == 383U);
+    return {};
+}
+
+arpg::test::Failure health_potion_roll_is_deterministic_and_has_both_outcomes()
+    noexcept {
+    // These seeds freeze both the independent HP-potion stream and the exact
+    // 3000-basis-point comparison boundary for spawn ordinal 37.
+    ARPG_REQUIRE(arpg::dungeon::roll_health_potion_drop(2725U, 37U));
+    ARPG_REQUIRE(!arpg::dungeon::roll_health_potion_drop(1902U, 37U));
+    constexpr std::array<bool, 4U> kFrozenDomainOutcomes{{
+        true, false, true, false,
+    }};
+    for (std::uint64_t seed = 0U; seed < kFrozenDomainOutcomes.size(); ++seed) {
+        ARPG_REQUIRE(arpg::dungeon::roll_health_potion_drop(seed, 37U)
+            == kFrozenDomainOutcomes[seed]);
+    }
+    bool found_drop = false;
+    bool found_miss = false;
+    for (std::uint64_t seed = 0U; seed < 4096U; ++seed) {
+        const bool first = arpg::dungeon::roll_health_potion_drop(seed, 37U);
+        const bool second = arpg::dungeon::roll_health_potion_drop(seed, 37U);
+        ARPG_REQUIRE(first == second);
+        found_drop = found_drop || first;
+        found_miss = found_miss || !first;
+    }
+    ARPG_REQUIRE(found_drop);
+    ARPG_REQUIRE(found_miss);
+    return {};
+}
+
+arpg::test::Failure health_potion_threshold_is_integer_exact() noexcept {
+    using arpg::dungeon::health_potion_auto_use_eligible;
+    ARPG_REQUIRE(!health_potion_auto_use_eligible(751, 1000));
+    ARPG_REQUIRE(health_potion_auto_use_eligible(750, 1000));
+    ARPG_REQUIRE(health_potion_auto_use_eligible(749, 1000));
+    ARPG_REQUIRE(!health_potion_auto_use_eligible(0, 1000));
+    ARPG_REQUIRE(!health_potion_auto_use_eligible(1, 0));
+    ARPG_REQUIRE(health_potion_auto_use_eligible(3, 4));
+    ARPG_REQUIRE(!health_potion_auto_use_eligible(4, 4));
+    return {};
+}
+
+arpg::test::Failure
+coupon_has_priority_and_common_material_can_coexist_with_potion() noexcept {
+    constexpr std::uint16_t kSpawn = 23U;
+    constexpr std::uint64_t kDepth = 90U;
+    constexpr std::uint16_t kScore = 18U;
+    const std::uint64_t coupon_seed = find_drop_seed(
+        kSpawn, kDepth, kScore, {false, true, true});
+    const std::uint64_t common_potion_seed = find_drop_seed(
+        kSpawn, kDepth, kScore, {true, false, true});
+    ARPG_REQUIRE(coupon_seed != 0U);
+    ARPG_REQUIRE(common_potion_seed != 0U);
+
+    DungeonSession coupon_session{
+        DungeonRules{}, potion_state(coupon_seed, kDepth)};
+    ARPG_REQUIRE(relay_drop(coupon_session, coupon_seed, kSpawn, kScore,
+        {2.0F, 3.0F, 9.0F}));
+    const auto coupon_snapshot = coupon_session.snapshot();
+    ARPG_REQUIRE(coupon_snapshot.ground_health_potion_count == 0U);
+    const auto* secondary = find_ground_material_snapshot(
+        coupon_snapshot, arpg::dungeon::health_potion_claim_ordinal(kSpawn));
+    ARPG_REQUIRE(secondary != nullptr);
+    ARPG_REQUIRE(secondary->source == GroundMaterialSource::monster_coupon);
+
+    DungeonSession coexist_session{
+        DungeonRules{}, potion_state(common_potion_seed, kDepth)};
+    ARPG_REQUIRE(relay_drop(coexist_session, common_potion_seed, kSpawn,
+        kScore, {-2.0F, 7.0F, 5.0F}));
+    const auto coexist_snapshot = coexist_session.snapshot();
+    ARPG_REQUIRE(coexist_snapshot.ground_health_potion_count == 1U);
+    ARPG_REQUIRE(coexist_snapshot.ground_health_potions[0].claim_ordinal
+        == arpg::dungeon::health_potion_claim_ordinal(kSpawn));
+    const auto* common = find_ground_material_snapshot(
+        coexist_snapshot, static_cast<std::uint16_t>(kSpawn * 2U));
+    ARPG_REQUIRE(common != nullptr);
+    ARPG_REQUIRE(common->source == GroundMaterialSource::monster_common);
+    ARPG_REQUIRE(find_ground_material_snapshot(coexist_snapshot,
+        arpg::dungeon::health_potion_claim_ordinal(kSpawn)) == nullptr);
+    return {};
+}
+
+arpg::test::Failure
+adding_potions_does_not_change_existing_material_or_coupon_rolls() noexcept {
+    constexpr std::size_t kBatch = 48U;
+    constexpr std::uint64_t kDepth = 90U;
+    constexpr std::uint16_t kScore = 18U;
+    const std::uint64_t seed = find_drop_seed(
+        0U, kDepth, kScore, {false, false, true});
+    ARPG_REQUIRE(seed != 0U);
+    std::array<std::optional<arpg::items::MaterialId>, kBatch> common{};
+    std::array<std::optional<arpg::items::MaterialId>, kBatch> coupons{};
+    for (std::uint16_t spawn = 0U; spawn < kBatch; ++spawn) {
+        common[spawn] = arpg::dungeon::roll_material_drop(
+            seed, spawn, kDepth, kScore);
+        coupons[spawn] = arpg::dungeon::roll_coupon_drop(
+            seed, spawn, kDepth, kScore, false);
+    }
+
+    DungeonSession session{DungeonRules{}, potion_state(seed, kDepth)};
+    for (std::uint16_t spawn = 0U; spawn < kBatch; ++spawn) {
+        ARPG_REQUIRE(relay_drop(session, seed, spawn, kScore,
+            {static_cast<float>(spawn), -static_cast<float>(spawn), 8.0F}));
+    }
+    const auto snapshot = session.snapshot();
+    ARPG_REQUIRE(snapshot.ground_health_potion_count != 0U);
+    for (std::uint16_t spawn = 0U; spawn < kBatch; ++spawn) {
+        ARPG_REQUIRE(common[spawn] == arpg::dungeon::roll_material_drop(
+            seed, spawn, kDepth, kScore));
+        ARPG_REQUIRE(coupons[spawn] == arpg::dungeon::roll_coupon_drop(
+            seed, spawn, kDepth, kScore, false));
+        const auto* actual_common = find_ground_material_snapshot(
+            snapshot, static_cast<std::uint16_t>(spawn * 2U));
+        ARPG_REQUIRE((actual_common != nullptr) == common[spawn].has_value());
+        if (actual_common != nullptr) {
+            ARPG_REQUIRE(actual_common->material == *common[spawn]);
+            ARPG_REQUIRE(actual_common->source
+                == GroundMaterialSource::monster_common);
+        }
+        const auto* actual_coupon = find_ground_material_snapshot(
+            snapshot, arpg::dungeon::health_potion_claim_ordinal(spawn));
+        ARPG_REQUIRE((actual_coupon != nullptr)
+            == coupons[spawn].has_value());
+        if (actual_coupon != nullptr) {
+            ARPG_REQUIRE(actual_coupon->material == *coupons[spawn]);
+            ARPG_REQUIRE(actual_coupon->source
+                == GroundMaterialSource::monster_coupon);
+        }
+    }
+    return {};
+}
+
+arpg::test::Failure
+ground_potion_pool_accepts_spawn_zero_and_191_without_overwrite() noexcept {
+    constexpr std::uint64_t kDepth = 90U;
+    constexpr std::uint16_t kScore = 18U;
+    const std::uint64_t initial_seed = find_drop_seed(
+        0U, kDepth, kScore, {false, false, true});
+    ARPG_REQUIRE(initial_seed != 0U);
+    DungeonSession session{
+        DungeonRules{}, potion_state(initial_seed, kDepth)};
+    const std::uint64_t allocations_before = arpg::test::allocation_count();
+    arpg::dungeon::DungeonSnapshot full{};
+    {
+        arpg::test::ScopedAllocationFailure fail{0U};
+        for (std::uint16_t spawn = 0U;
+             spawn < arpg::dungeon::kGroundHealthPotionCapacity; ++spawn) {
+            const std::uint64_t seed = find_drop_seed(
+                spawn, kDepth, kScore, {false, false, true});
+            ARPG_REQUIRE(seed != 0U);
+            ARPG_REQUIRE(relay_drop(session, seed, spawn, kScore,
+                {static_cast<float>(spawn),
+                    static_cast<float>(spawn + 1U), 99.0F}));
+        }
+        full = session.snapshot();
+    }
+    ARPG_REQUIRE(arpg::test::allocation_count() == allocations_before);
+    ARPG_REQUIRE(full.ground_health_potion_count
+        == arpg::dungeon::kGroundHealthPotionCapacity);
+    ARPG_REQUIRE(full.ground_health_potions[0].spawn_ordinal == 0U);
+    ARPG_REQUIRE(full.ground_health_potions[191].spawn_ordinal == 191U);
+    ARPG_REQUIRE(full.ground_health_potions[0].claim_ordinal == 1U);
+    ARPG_REQUIRE(full.ground_health_potions[191].claim_ordinal == 383U);
+    const auto original = full.ground_health_potions[0];
+    const std::uint32_t saturation_before =
+        full.diagnostics.health_potion_ground_saturation_count;
+
+    arpg::test::clear_rolled_material_claims(session);
+    ARPG_REQUIRE(relay_drop(session, initial_seed, 0U, kScore,
+        {-999.0F, -888.0F, 77.0F}));
+    const auto after = session.snapshot();
+    ARPG_REQUIRE(after.ground_health_potion_count
+        == arpg::dungeon::kGroundHealthPotionCapacity);
+    ARPG_REQUIRE(std::memcmp(&original, &after.ground_health_potions[0],
+        sizeof(original)) == 0);
+    ARPG_REQUIRE(after.diagnostics.health_potion_ground_saturation_count
+        == saturation_before + 1U);
+    return {};
+}
+
+arpg::test::Failure
+potion_snapshot_is_sorted_by_spawn_ordinal_and_clears_on_room_exit() noexcept {
+    constexpr std::uint64_t kDepth = 90U;
+    constexpr std::uint16_t kScore = 18U;
+    constexpr std::array<std::uint16_t, 3U> kInstallOrder{{191U, 77U, 0U}};
+    const std::uint64_t initial_seed = find_drop_seed(
+        kInstallOrder[0], kDepth, kScore, {false, false, true});
+    ARPG_REQUIRE(initial_seed != 0U);
+    DungeonSession session{
+        DungeonRules{}, potion_state(initial_seed, kDepth)};
+    for (const std::uint16_t spawn : kInstallOrder) {
+        const std::uint64_t seed = find_drop_seed(
+            spawn, kDepth, kScore, {false, false, true});
+        ARPG_REQUIRE(seed != 0U);
+        ARPG_REQUIRE(relay_drop(session, seed, spawn, kScore,
+            {static_cast<float>(spawn), 4.0F, 17.0F}));
+    }
+    const auto installed = session.snapshot();
+    ARPG_REQUIRE(installed.ground_health_potion_count == 3U);
+    ARPG_REQUIRE(installed.ground_health_potions[0].spawn_ordinal == 0U);
+    ARPG_REQUIRE(installed.ground_health_potions[1].spawn_ordinal == 77U);
+    ARPG_REQUIRE(installed.ground_health_potions[2].spawn_ordinal == 191U);
+    ARPG_REQUIRE(!installed.health_potion_pickup_receipt.valid);
+
+    arpg::test::set_phase(session, arpg::dungeon::RoomPhase::combat);
+    arpg::test::prepare_room_clear(session);
+    ARPG_REQUIRE(session.snapshot().phase
+        == arpg::dungeon::RoomPhase::cleared);
+    ARPG_REQUIRE(session.snapshot().ground_health_potion_count == 3U);
+    session.tick({});
+    ARPG_REQUIRE(session.snapshot().phase
+        == arpg::dungeon::RoomPhase::awaiting_exit);
+    ARPG_REQUIRE(session.snapshot().ground_health_potion_count == 3U);
+
+    arpg::test::set_current_room_hole(session, true);
+    ARPG_REQUIRE(session.request_descent(true));
+    ARPG_REQUIRE(session.snapshot().ground_health_potion_count == 3U);
+    const auto pending = session.pending_transition();
+    ARPG_REQUIRE(pending.has_value());
+    session.resolve_pending_transition({
+        arpg::dungeon::SaveDisposition::committed,
+        pending->expected_generation,
+        pending->next_state,
+    });
+    ARPG_REQUIRE(session.snapshot().phase
+        == arpg::dungeon::RoomPhase::transitioning);
+    ARPG_REQUIRE(session.snapshot().ground_health_potion_count == 0U);
+    return {};
+}
+
+arpg::test::Failure
+potion_above_threshold_stays_grounded_but_75_percent_ignores_distance()
+    noexcept {
+    DungeonSession session{DungeonRules{}, potion_state(101U)};
+    arpg::test::set_phase(session, arpg::dungeon::RoomPhase::awaiting_exit);
+    arpg::test::set_player_position(session, {0.0F, 0.0F, 0.0F});
+    arpg::test::set_player_health(session, 751, 1000);
+    install_potion(session, 7U, {100.0F, 0.0F, 0.0F});
+
+    session.tick({});
+    auto visible = session.snapshot();
+    ARPG_REQUIRE(!session.pending_save().has_value());
+    ARPG_REQUIRE(visible.combat.has_value());
+    ARPG_REQUIRE(visible.combat->player.hp == 751);
+    ARPG_REQUIRE(visible.ground_health_potion_count == 1U);
+    ARPG_REQUIRE(!visible.health_potion_pickup_receipt.valid);
+
+    arpg::test::set_player_health(session, 750, 1000);
+    const std::uint64_t allocations_before = arpg::test::allocation_count();
+    session.tick({});
+    ARPG_REQUIRE(arpg::test::allocation_count() == allocations_before);
+    const auto pending = session.pending_save();
+    visible = session.snapshot();
+    ARPG_REQUIRE(pending.has_value());
+    ARPG_REQUIRE(pending->kind
+        == arpg::dungeon::PendingSaveKind::health_potion_pickup);
+    ARPG_REQUIRE(pending->health_potion_claim.has_value());
+    ARPG_REQUIRE(pending->health_potion_claim->count == 1U);
+    ARPG_REQUIRE(pending->health_potion_claim->spawn_ordinals[0] == 7U);
+    ARPG_REQUIRE(visible.pending_health_potion_spawn_ordinal == 7U);
+    ARPG_REQUIRE(visible.combat->player.hp == 750);
+    ARPG_REQUIRE(visible.ground_health_potion_count == 1U);
+    ARPG_REQUIRE(!visible.health_potion_pickup_receipt.valid);
+    ARPG_REQUIRE(!claim_bit_is_set(arpg::test::stable_state(session),
+        arpg::dungeon::health_potion_claim_ordinal(7U)));
+    ARPG_REQUIRE(claim_bit_is_set(pending->next_state,
+        arpg::dungeon::health_potion_claim_ordinal(7U)));
+    return {};
+}
+
+arpg::test::Failure
+potion_auto_use_scans_stable_spawn_order_and_rechecks_after_commit() noexcept {
+    DungeonSession session{DungeonRules{}, potion_state(102U)};
+    arpg::test::set_phase(session, arpg::dungeon::RoomPhase::awaiting_exit);
+    arpg::test::set_player_health(session, 749, 1000);
+    install_potion(session, 9U);
+    install_potion(session, 6U);
+    install_potion(session, 2U);
+
+    session.request_nearby_pickups({-500.0F, 0.0F, 0.0F});
+    ARPG_REQUIRE(session.pending_save().has_value());
+    ARPG_REQUIRE(session.pending_save()->health_potion_claim.has_value());
+    ARPG_REQUIRE(session.pending_save()->health_potion_claim
+        ->spawn_ordinals[0] == 2U);
+    ARPG_REQUIRE(arpg::test::commit_pending(session));
+    ARPG_REQUIRE(session.snapshot().combat->player.hp == 999);
+
+    session.request_nearby_pickups({500.0F, 0.0F, 0.0F});
+    ARPG_REQUIRE(!session.pending_save().has_value());
+    ARPG_REQUIRE(session.snapshot().ground_health_potion_count == 2U);
+    arpg::test::set_player_health(session, 750, 1000);
+    session.request_nearby_pickups({500.0F, 0.0F, 0.0F});
+    ARPG_REQUIRE(session.pending_save().has_value());
+    ARPG_REQUIRE(session.pending_save()->health_potion_claim
+        ->spawn_ordinals[0] == 6U);
+    return {};
+}
+
+arpg::test::Failure
+dead_player_or_death_snapshot_never_requests_or_consumes_potion() noexcept {
+    DungeonSession dead{DungeonRules{}, potion_state(103U)};
+    arpg::test::set_phase(dead, arpg::dungeon::RoomPhase::awaiting_exit);
+    arpg::test::set_player_health(dead, 0, 1000);
+    install_potion(dead, 3U);
+    dead.request_nearby_pickups({});
+    ARPG_REQUIRE(!dead.pending_save().has_value());
+    ARPG_REQUIRE(dead.snapshot().combat->player.hp == 0);
+    ARPG_REQUIRE(dead.snapshot().ground_health_potion_count == 1U);
+    ARPG_REQUIRE(!dead.snapshot().health_potion_pickup_receipt.valid);
+
+    DungeonSession death_snapshot{DungeonRules{}, potion_state(104U)};
+    ARPG_REQUIRE(arpg::test::kill_current_player_through_combat(death_snapshot));
+    arpg::test::set_player_health(death_snapshot, 750, 1000);
+    arpg::test::set_phase(
+        death_snapshot, arpg::dungeon::RoomPhase::awaiting_exit);
+    install_potion(death_snapshot, 3U);
+    death_snapshot.request_nearby_pickups({});
+    ARPG_REQUIRE(!death_snapshot.pending_save().has_value());
+    ARPG_REQUIRE(death_snapshot.snapshot().combat->player.hp == 750);
+    ARPG_REQUIRE(death_snapshot.snapshot().ground_health_potion_count == 1U);
+    ARPG_REQUIRE(!death_snapshot.snapshot().health_potion_pickup_receipt.valid);
+
+    DungeonSession no_combat{DungeonRules{}, potion_state(105U)};
+    arpg::test::set_phase(no_combat, arpg::dungeon::RoomPhase::awaiting_exit);
+    install_potion(no_combat, 3U);
+    arpg::test::clear_combat_world(no_combat);
+    no_combat.request_nearby_pickups({});
+    ARPG_REQUIRE(!no_combat.pending_save().has_value());
+    ARPG_REQUIRE(no_combat.snapshot().ground_health_potion_count == 1U);
+    ARPG_REQUIRE(!no_combat.snapshot().health_potion_pickup_receipt.valid);
+
+    DungeonSession wrong_phase{DungeonRules{}, potion_state(106U)};
+    arpg::test::set_player_health(wrong_phase, 750, 1000);
+    arpg::test::set_phase(
+        wrong_phase, arpg::dungeon::RoomPhase::transitioning);
+    install_potion(wrong_phase, 3U);
+    wrong_phase.request_nearby_pickups({});
+    ARPG_REQUIRE(!wrong_phase.pending_save().has_value());
+    ARPG_REQUIRE(wrong_phase.snapshot().combat->player.hp == 750);
+    ARPG_REQUIRE(wrong_phase.snapshot().ground_health_potion_count == 1U);
+    ARPG_REQUIRE(!wrong_phase.snapshot().health_potion_pickup_receipt.valid);
+    return {};
+}
+
+arpg::test::Failure
+committed_pickup_heals_caps_removes_and_publishes_exact_receipt() noexcept {
+    DungeonSession session{DungeonRules{}, potion_state(107U)};
+    arpg::test::set_phase(session, arpg::dungeon::RoomPhase::awaiting_exit);
+    arpg::test::set_player_health(session, 750, 1000);
+    install_potion(session, 9U);
+    install_potion(session, 2U);
+    const std::uint64_t generation_before =
+        session.snapshot().commit_generation;
+    const std::uint64_t allocations_before_request =
+        arpg::test::allocation_count();
+
+    session.request_nearby_pickups({-1000.0F, -1000.0F, 0.0F});
+    ARPG_REQUIRE(arpg::test::allocation_count()
+        == allocations_before_request);
+    const auto pending = session.pending_save();
+    ARPG_REQUIRE(pending.has_value());
+    ARPG_REQUIRE(session.snapshot().combat->player.hp == 750);
+    ARPG_REQUIRE(session.snapshot().ground_health_potion_count == 2U);
+    ARPG_REQUIRE(!session.snapshot().health_potion_pickup_receipt.valid);
+    const arpg::dungeon::PendingSaveResult receipt{
+        arpg::dungeon::SaveDisposition::committed,
+        pending->expected_generation,
+        pending->next_state,
+    };
+    const std::uint64_t allocations_before_resolve =
+        arpg::test::allocation_count();
+    {
+        arpg::test::ScopedAllocationFailure fail{0U};
+        session.resolve_pending_save(receipt);
+    }
+    ARPG_REQUIRE(arpg::test::allocation_count() == allocations_before_resolve);
+
+    const auto committed = session.snapshot();
+    ARPG_REQUIRE(committed.phase
+        == arpg::dungeon::RoomPhase::awaiting_exit);
+    ARPG_REQUIRE(committed.combat->player.hp == 1000);
+    ARPG_REQUIRE(committed.ground_health_potion_count == 1U);
+    ARPG_REQUIRE(committed.ground_health_potions[0].spawn_ordinal == 9U);
+    ARPG_REQUIRE(committed.health_potion_pickup_receipt.valid);
+    ARPG_REQUIRE(!committed.health_potion_pickup_receipt.room_clear);
+    ARPG_REQUIRE(committed.health_potion_pickup_receipt.restored_hp == 250);
+    ARPG_REQUIRE(committed.health_potion_pickup_receipt.consumed_count == 1U);
+    ARPG_REQUIRE(committed.health_potion_pickup_receipt.commit_generation
+        == generation_before + 1U);
+    ARPG_REQUIRE(claim_bit_is_set(arpg::test::stable_state(session),
+        arpg::dungeon::health_potion_claim_ordinal(2U)));
+    ARPG_REQUIRE(!claim_bit_is_set(arpg::test::stable_state(session),
+        arpg::dungeon::health_potion_claim_ordinal(9U)));
+    return {};
+}
+
+arpg::test::Failure
+failed_or_indeterminate_save_never_heals_removes_or_reports_success()
+    noexcept {
+    DungeonSession rollback{DungeonRules{}, potion_state(201U)};
+    arpg::test::set_phase(
+        rollback, arpg::dungeon::RoomPhase::awaiting_exit);
+    arpg::test::set_player_health(rollback, 500, 1000);
+    install_potion(rollback, 4U);
+    rollback.request_nearby_pickups({});
+    const auto first = rollback.pending_save();
+    ARPG_REQUIRE(first.has_value());
+    const DungeonRunState stable_before = arpg::test::stable_state(rollback);
+    rollback.resolve_pending_save({
+        arpg::dungeon::SaveDisposition::not_committed,
+        first->expected_generation,
+        first->next_state,
+        first->kind,
+    });
+    auto visible = rollback.snapshot();
+    ARPG_REQUIRE(visible.phase
+        == arpg::dungeon::RoomPhase::awaiting_exit);
+    ARPG_REQUIRE(visible.combat->player.hp == 500);
+    ARPG_REQUIRE(visible.ground_health_potion_count == 1U);
+    ARPG_REQUIRE(!visible.health_potion_pickup_receipt.valid);
+    ARPG_REQUIRE(arpg::dungeon::same_run_state(
+        arpg::test::stable_state(rollback), stable_before));
+    ARPG_REQUIRE(!claim_bit_is_set(arpg::test::stable_state(rollback),
+        arpg::dungeon::health_potion_claim_ordinal(4U)));
+
+    rollback.request_nearby_pickups({999.0F, 999.0F, 0.0F});
+    const auto retry = rollback.pending_save();
+    ARPG_REQUIRE(retry.has_value());
+    ARPG_REQUIRE(arpg::dungeon::same_run_state(
+        retry->next_state, first->next_state));
+    ARPG_REQUIRE(retry->health_potion_claim.has_value());
+    ARPG_REQUIRE(retry->health_potion_claim->spawn_ordinals
+        == first->health_potion_claim->spawn_ordinals);
+    ARPG_REQUIRE(retry->health_potion_claim->count
+        == first->health_potion_claim->count);
+    ARPG_REQUIRE(retry->health_potion_claim->expected_hp
+        == first->health_potion_claim->expected_hp);
+    ARPG_REQUIRE(retry->health_potion_claim->expected_max_hp
+        == first->health_potion_claim->expected_max_hp);
+
+    DungeonSession indeterminate{DungeonRules{}, potion_state(202U)};
+    arpg::test::set_phase(
+        indeterminate, arpg::dungeon::RoomPhase::awaiting_exit);
+    arpg::test::set_player_health(indeterminate, 500, 1000);
+    install_potion(indeterminate, 4U);
+    indeterminate.request_nearby_pickups({});
+    indeterminate.resolve_pending_save({
+        arpg::dungeon::SaveDisposition::indeterminate, 0U, {}});
+    visible = indeterminate.snapshot();
+    ARPG_REQUIRE(visible.phase == arpg::dungeon::RoomPhase::faulted);
+    ARPG_REQUIRE(visible.diagnostics.fault
+        == arpg::dungeon::DungeonFault::save_commit_indeterminate);
+    ARPG_REQUIRE(visible.combat->player.hp == 500);
+    ARPG_REQUIRE(visible.ground_health_potion_count == 1U);
+    ARPG_REQUIRE(!visible.health_potion_pickup_receipt.valid);
+    ARPG_REQUIRE(!claim_bit_is_set(arpg::test::stable_state(indeterminate),
+        arpg::dungeon::health_potion_claim_ordinal(4U)));
+
+    DungeonSession oversized{DungeonRules{}, potion_state(206U)};
+    arpg::test::set_phase(oversized, arpg::dungeon::RoomPhase::combat);
+    arpg::test::set_player_health(oversized, 1, 1000);
+    install_potion(oversized, 2U);
+    install_potion(oversized, 3U);
+    install_potion(oversized, 6U);
+    arpg::test::prepare_room_clear(oversized);
+    ARPG_REQUIRE(oversized.pending_save().has_value());
+    ARPG_REQUIRE(oversized.pending_save()->health_potion_claim.has_value());
+    ARPG_REQUIRE(oversized.pending_save()->health_potion_claim->count == 3U);
+    const DungeonRunState oversized_stable_before =
+        arpg::test::stable_state(oversized);
+    arpg::test::set_pending_health_potion_claim_count(oversized, 5U);
+    arpg::test::set_pending_next_material_claim_bit(oversized,
+        arpg::dungeon::health_potion_claim_ordinal(0U));
+    arpg::test::set_pending_next_material_claim_bit(oversized,
+        arpg::dungeon::health_potion_claim_ordinal(5U));
+    ARPG_REQUIRE(!arpg::test::pending_material_cache_consistent(oversized));
+    const auto oversized_pending = oversized.pending_save();
+    ARPG_REQUIRE(oversized_pending.has_value());
+    oversized.resolve_pending_save({
+        arpg::dungeon::SaveDisposition::committed,
+        oversized_pending->expected_generation,
+        oversized_pending->next_state,
+        oversized_pending->kind,
+    });
+    visible = oversized.snapshot();
+    ARPG_REQUIRE(visible.phase == arpg::dungeon::RoomPhase::faulted);
+    ARPG_REQUIRE(visible.diagnostics.fault
+        == arpg::dungeon::DungeonFault::save_receipt_mismatch);
+    ARPG_REQUIRE(visible.combat->player.hp == 1);
+    ARPG_REQUIRE(visible.ground_health_potion_count == 3U);
+    ARPG_REQUIRE(!visible.health_potion_pickup_receipt.valid);
+    ARPG_REQUIRE(arpg::dungeon::same_run_state(
+        arpg::test::stable_state(oversized), oversized_stable_before));
+    return {};
+}
+
+arpg::test::Failure
+mismatched_pending_or_replaced_ground_faults_before_health_side_effects()
+    noexcept {
+    const auto assert_unchanged_fault = [](const DungeonSession& session,
+        int expected_hp, std::uint16_t expected_ground) noexcept {
+        const auto visible = session.snapshot();
+        return visible.phase == arpg::dungeon::RoomPhase::faulted
+            && visible.diagnostics.fault
+                == arpg::dungeon::DungeonFault::save_receipt_mismatch
+            && visible.combat.has_value()
+            && visible.combat->player.hp == expected_hp
+            && visible.ground_health_potion_count == expected_ground
+            && !visible.health_potion_pickup_receipt.valid;
+    };
+
+    DungeonSession wrong_generation{DungeonRules{}, potion_state(203U)};
+    arpg::test::set_phase(
+        wrong_generation, arpg::dungeon::RoomPhase::awaiting_exit);
+    arpg::test::set_player_health(wrong_generation, 500, 1000);
+    install_potion(wrong_generation, 5U);
+    wrong_generation.request_nearby_pickups({});
+    const auto generation_pending = wrong_generation.pending_save();
+    ARPG_REQUIRE(generation_pending.has_value());
+    wrong_generation.resolve_pending_save({
+        arpg::dungeon::SaveDisposition::committed,
+        generation_pending->expected_generation + 1U,
+        generation_pending->next_state,
+        generation_pending->kind,
+    });
+    ARPG_REQUIRE(assert_unchanged_fault(wrong_generation, 500, 1U));
+    ARPG_REQUIRE(!claim_bit_is_set(
+        arpg::test::stable_state(wrong_generation),
+        arpg::dungeon::health_potion_claim_ordinal(5U)));
+
+    DungeonSession wrong_kind{DungeonRules{}, potion_state(204U)};
+    arpg::test::set_phase(
+        wrong_kind, arpg::dungeon::RoomPhase::awaiting_exit);
+    arpg::test::set_player_health(wrong_kind, 500, 1000);
+    install_potion(wrong_kind, 5U);
+    wrong_kind.request_nearby_pickups({});
+    const auto kind_pending = wrong_kind.pending_save();
+    ARPG_REQUIRE(kind_pending.has_value());
+    wrong_kind.resolve_pending_save({
+        arpg::dungeon::SaveDisposition::committed,
+        kind_pending->expected_generation,
+        kind_pending->next_state,
+        arpg::dungeon::PendingSaveKind::material_pickup,
+    });
+    ARPG_REQUIRE(assert_unchanged_fault(wrong_kind, 500, 1U));
+    ARPG_REQUIRE(!claim_bit_is_set(arpg::test::stable_state(wrong_kind),
+        arpg::dungeon::health_potion_claim_ordinal(5U)));
+
+    DungeonSession replaced{DungeonRules{}, potion_state(205U)};
+    arpg::test::set_phase(
+        replaced, arpg::dungeon::RoomPhase::awaiting_exit);
+    arpg::test::set_player_health(replaced, 500, 1000);
+    install_potion(replaced, 5U);
+    replaced.request_nearby_pickups({});
+    const auto replaced_pending = replaced.pending_save();
+    ARPG_REQUIRE(replaced_pending.has_value());
+    arpg::test::replace_ground_health_potion(replaced, 5U, {
+        true, 6U, arpg::dungeon::health_potion_claim_ordinal(6U),
+        {17.0F, 18.0F, 0.0F}});
+    replaced.resolve_pending_save({
+        arpg::dungeon::SaveDisposition::committed,
+        replaced_pending->expected_generation,
+        replaced_pending->next_state,
+        replaced_pending->kind,
+    });
+    ARPG_REQUIRE(assert_unchanged_fault(replaced, 500, 1U));
+    ARPG_REQUIRE(!claim_bit_is_set(arpg::test::stable_state(replaced),
+        arpg::dungeon::health_potion_claim_ordinal(5U)));
+
+    DungeonSession polluted{DungeonRules{}, potion_state(207U)};
+    arpg::test::set_phase(
+        polluted, arpg::dungeon::RoomPhase::awaiting_exit);
+    arpg::test::set_player_health(polluted, 500, 1000);
+    install_potion(polluted, 5U);
+    polluted.request_nearby_pickups({});
+    ARPG_REQUIRE(polluted.pending_save().has_value());
+    const DungeonRunState polluted_stable_before =
+        arpg::test::stable_state(polluted);
+    arpg::test::offset_pending_next_room_depth(polluted, 1U);
+    const auto polluted_pending = polluted.pending_save();
+    ARPG_REQUIRE(polluted_pending.has_value());
+    polluted.resolve_pending_save({
+        arpg::dungeon::SaveDisposition::committed,
+        polluted_pending->expected_generation,
+        polluted_pending->next_state,
+        polluted_pending->kind,
+    });
+    ARPG_REQUIRE(assert_unchanged_fault(polluted, 500, 1U));
+    ARPG_REQUIRE(arpg::dungeon::same_run_state(
+        arpg::test::stable_state(polluted), polluted_stable_before));
+    return {};
+}
+
+arpg::test::Failure
+committed_claim_without_runtime_resolve_does_not_replay_after_reload()
+    noexcept {
+    constexpr std::uint16_t kSpawn = 8U;
+    constexpr std::uint64_t kDepth = 90U;
+    constexpr std::uint16_t kScore = 18U;
+    const std::uint64_t seed = find_drop_seed(
+        kSpawn, kDepth, kScore, {false, false, true});
+    ARPG_REQUIRE(seed != 0U);
+    PotionTempDirectory directory;
+    ARPG_REQUIRE(!directory.path.empty());
+    arpg::persistence::SaveStore store({directory.path});
+    auto seeded = store.commit(potion_state(seed, kDepth));
+    ARPG_REQUIRE(seeded.state
+        == arpg::persistence::SaveCommitState::committed);
+
+    DungeonSession before_crash{
+        DungeonRules{}, std::move(seeded.verified_state)};
+    ARPG_REQUIRE(relay_drop(before_crash, seed, kSpawn, kScore,
+        {2.0F, 3.0F, 0.0F}));
+    ARPG_REQUIRE(before_crash.snapshot().ground_health_potion_count == 1U);
+    arpg::test::set_phase(
+        before_crash, arpg::dungeon::RoomPhase::awaiting_exit);
+    arpg::test::set_player_health(before_crash, 500, 1000);
+    before_crash.request_nearby_pickups({});
+    const arpg::dungeon::PendingSave* const pending =
+        before_crash.pending_save_view();
+    ARPG_REQUIRE(pending != nullptr);
+    auto durable = store.commit(pending->next_state);
+    ARPG_REQUIRE(durable.state
+        == arpg::persistence::SaveCommitState::committed);
+
+    auto loaded = store.load();
+    ARPG_REQUIRE(loaded.state == arpg::persistence::SaveLoadState::ready);
+    ARPG_REQUIRE(claim_bit_is_set(loaded.checkpoint,
+        arpg::dungeon::health_potion_claim_ordinal(kSpawn)));
+    DungeonSession reloaded{
+        DungeonRules{}, std::move(loaded.checkpoint)};
+    arpg::test::set_phase(
+        reloaded, arpg::dungeon::RoomPhase::awaiting_exit);
+    arpg::test::set_player_health(reloaded, 500, 1000);
+    ARPG_REQUIRE(relay_drop(reloaded, seed, kSpawn, kScore,
+        {2.0F, 3.0F, 0.0F}));
+    reloaded.tick({});
+    const auto visible = reloaded.snapshot();
+    ARPG_REQUIRE(visible.combat->player.hp == 500);
+    ARPG_REQUIRE(visible.ground_health_potion_count == 0U);
+    ARPG_REQUIRE(!visible.pending_health_potion_spawn_ordinal.has_value());
+    ARPG_REQUIRE(!visible.health_potion_pickup_receipt.valid);
+    ARPG_REQUIRE(claim_bit_is_set(arpg::test::stable_state(reloaded),
+        arpg::dungeon::health_potion_claim_ordinal(kSpawn)));
+    return {};
+}
+
+arpg::test::Failure
+final_kill_low_health_folds_sorted_potions_into_clear_transaction() noexcept {
+    constexpr std::uint16_t kMaterialOrdinal = 40U;
+    const std::size_t chaos_index = arpg::items::material_index(
+        arpg::items::MaterialId::chaos);
+    DungeonSession normal{DungeonRules{}, potion_state(301U)};
+    arpg::test::set_phase(normal, arpg::dungeon::RoomPhase::combat);
+    arpg::test::set_player_health(normal, 500, 1000);
+    install_potion(normal, 8U);
+    install_potion(normal, 3U);
+    arpg::test::install_ground_material(normal, kMaterialOrdinal,
+        arpg::items::MaterialId::chaos, {4.0F, 5.0F, 0.0F});
+    arpg::test::prepare_room_clear(normal);
+    const auto first = normal.pending_save();
+    ARPG_REQUIRE(first.has_value());
+    ARPG_REQUIRE(first->kind == arpg::dungeon::PendingSaveKind::room_clear);
+    ARPG_REQUIRE(first->health_potion_claim.has_value());
+    ARPG_REQUIRE(first->health_potion_claim->count == 2U);
+    ARPG_REQUIRE(first->health_potion_claim->spawn_ordinals[0] == 3U);
+    ARPG_REQUIRE(first->health_potion_claim->spawn_ordinals[1] == 8U);
+    ARPG_REQUIRE(first->next_state.item_ownership.materials[chaos_index]
+        == arpg::test::stable_state(normal)
+            .item_ownership.materials[chaos_index] + 1U);
+    ARPG_REQUIRE(claim_bit_is_set(first->next_state, kMaterialOrdinal));
+    ARPG_REQUIRE(claim_bit_is_set(first->next_state,
+        arpg::dungeon::health_potion_claim_ordinal(3U)));
+    ARPG_REQUIRE(claim_bit_is_set(first->next_state,
+        arpg::dungeon::health_potion_claim_ordinal(8U)));
+    ARPG_REQUIRE(normal.snapshot().combat->player.hp == 500);
+    ARPG_REQUIRE(normal.snapshot().ground_health_potion_count == 2U);
+    ARPG_REQUIRE(normal.snapshot().ground_material_count == 1U);
+    ARPG_REQUIRE(!normal.snapshot().health_potion_pickup_receipt.valid);
+
+    normal.resolve_pending_save({
+        arpg::dungeon::SaveDisposition::not_committed,
+        first->expected_generation,
+        first->next_state,
+        first->kind,
+    });
+    ARPG_REQUIRE(normal.snapshot().phase
+        == arpg::dungeon::RoomPhase::combat);
+    ARPG_REQUIRE(normal.snapshot().combat->player.hp == 500);
+    ARPG_REQUIRE(normal.snapshot().ground_health_potion_count == 2U);
+    ARPG_REQUIRE(normal.snapshot().ground_material_count == 1U);
+    ARPG_REQUIRE(!normal.snapshot().health_potion_pickup_receipt.valid);
+    arpg::test::prepare_room_clear(normal);
+    const auto retry = normal.pending_save();
+    ARPG_REQUIRE(retry.has_value());
+    ARPG_REQUIRE(arpg::dungeon::same_run_state(
+        retry->next_state, first->next_state));
+    ARPG_REQUIRE(retry->health_potion_claim->spawn_ordinals
+        == first->health_potion_claim->spawn_ordinals);
+
+    DungeonSession abyss{DungeonRules{}, potion_state(302U)};
+    arpg::test::set_phase(abyss, arpg::dungeon::RoomPhase::combat);
+    arpg::test::set_started_abyss_room(
+        abyss, arpg::abyss::AbyssDanger::low);
+    arpg::test::set_player_health(abyss, 500, 1000);
+    ARPG_REQUIRE(arpg::test::install_delayed_abyss_environment_hazard(abyss));
+    arpg::test::quiesce_current_room_for_clear_retry(abyss);
+    install_potion(abyss, 4U);
+    arpg::test::install_ground_material(abyss, kMaterialOrdinal,
+        arpg::items::MaterialId::chaos, {4.0F, 5.0F, 0.0F});
+    abyss.tick({});
+    const auto abyss_pending = abyss.pending_save();
+    ARPG_REQUIRE(abyss_pending.has_value());
+    ARPG_REQUIRE(abyss_pending->kind
+        == arpg::dungeon::PendingSaveKind::abyss_clear);
+    ARPG_REQUIRE(abyss_pending->health_potion_claim.has_value());
+    ARPG_REQUIRE(abyss_pending->health_potion_claim->count == 1U);
+    ARPG_REQUIRE(abyss_pending->health_potion_claim->spawn_ordinals[0] == 4U);
+    ARPG_REQUIRE(claim_bit_is_set(
+        abyss_pending->next_state, kMaterialOrdinal));
+    ARPG_REQUIRE(claim_bit_is_set(abyss_pending->next_state,
+        arpg::dungeon::health_potion_claim_ordinal(4U)));
+    ARPG_REQUIRE(abyss_pending->next_state.item_ownership
+        .materials[chaos_index]
+        > arpg::test::stable_state(abyss)
+            .item_ownership.materials[chaos_index]);
+    ARPG_REQUIRE(abyss.snapshot().combat->player.hp == 500);
+    ARPG_REQUIRE(abyss.snapshot().ground_health_potion_count == 1U);
+    ARPG_REQUIRE(!abyss.snapshot().health_potion_pickup_receipt.valid);
+    const DungeonRunState abyss_stable_before =
+        arpg::test::stable_state(abyss);
+    const auto abyss_before_failure = abyss.snapshot();
+    abyss.resolve_pending_save({
+        arpg::dungeon::SaveDisposition::not_committed,
+        abyss_pending->expected_generation,
+        abyss_pending->next_state,
+        abyss_pending->kind,
+    });
+    const auto abyss_after_failure = abyss.snapshot();
+    ARPG_REQUIRE(abyss_after_failure.phase
+        == arpg::dungeon::RoomPhase::combat);
+    ARPG_REQUIRE(abyss_after_failure.combat->player.hp
+        == abyss_before_failure.combat->player.hp);
+    ARPG_REQUIRE(abyss_after_failure.ground_health_potion_count
+        == abyss_before_failure.ground_health_potion_count);
+    ARPG_REQUIRE(abyss_after_failure.ground_material_count
+        == abyss_before_failure.ground_material_count);
+    for (std::uint16_t index = 0U;
+            index < abyss_after_failure.ground_health_potion_count; ++index) {
+        const auto& before = abyss_before_failure.ground_health_potions[index];
+        const auto& after = abyss_after_failure.ground_health_potions[index];
+        ARPG_REQUIRE(after.spawn_ordinal == before.spawn_ordinal);
+        ARPG_REQUIRE(after.claim_ordinal == before.claim_ordinal);
+        ARPG_REQUIRE(after.position.x == before.position.x);
+        ARPG_REQUIRE(after.position.y == before.position.y);
+        ARPG_REQUIRE(after.position.z == before.position.z);
+    }
+    for (std::uint16_t index = 0U;
+            index < abyss_after_failure.ground_material_count; ++index) {
+        const auto& before = abyss_before_failure.ground_materials[index];
+        const auto& after = abyss_after_failure.ground_materials[index];
+        ARPG_REQUIRE(after.ordinal == before.ordinal);
+        ARPG_REQUIRE(after.source == before.source);
+        ARPG_REQUIRE(after.material == before.material);
+        ARPG_REQUIRE(after.position.x == before.position.x);
+        ARPG_REQUIRE(after.position.y == before.position.y);
+        ARPG_REQUIRE(after.position.z == before.position.z);
+    }
+    const auto& receipt_before =
+        abyss_before_failure.health_potion_pickup_receipt;
+    const auto& receipt_after =
+        abyss_after_failure.health_potion_pickup_receipt;
+    ARPG_REQUIRE(receipt_after.valid == receipt_before.valid);
+    ARPG_REQUIRE(receipt_after.room_clear == receipt_before.room_clear);
+    ARPG_REQUIRE(receipt_after.consumed_count == receipt_before.consumed_count);
+    ARPG_REQUIRE(receipt_after.commit_generation
+        == receipt_before.commit_generation);
+    ARPG_REQUIRE(receipt_after.restored_hp == receipt_before.restored_hp);
+    for (const bool exit_open : abyss_after_failure.exits_open) {
+        ARPG_REQUIRE(!exit_open);
+    }
+    ARPG_REQUIRE(arpg::dungeon::same_run_state(
+        arpg::test::stable_state(abyss), abyss_stable_before));
+    ARPG_REQUIRE(!claim_bit_is_set(arpg::test::stable_state(abyss),
+        arpg::dungeon::health_potion_claim_ordinal(4U)));
+    ARPG_REQUIRE(abyss_before_failure.combat->hazard_count == 1U);
+    const auto hazard_before_retry =
+        abyss_before_failure.combat->hazards[0];
+    ARPG_REQUIRE(hazard_before_retry.active);
+    ARPG_REQUIRE(hazard_before_retry.telegraph_ticks == 1U);
+    const bool accepted_combat_action =
+        abyss.queue_action(arpg::combat::Action::light);
+    abyss.tick({});
+    const auto abyss_after_retry = abyss.snapshot();
+    ARPG_REQUIRE(abyss_after_retry.combat->player.hp
+        == abyss_before_failure.combat->player.hp);
+    ARPG_REQUIRE(abyss_after_retry.combat->hazard_count == 1U);
+    const auto hazard_after_retry = abyss_after_retry.combat->hazards[0];
+    ARPG_REQUIRE(hazard_after_retry.telegraph_ticks
+        == hazard_before_retry.telegraph_ticks);
+    ARPG_REQUIRE(hazard_after_retry.active_ticks
+        == hazard_before_retry.active_ticks);
+    ARPG_REQUIRE(hazard_after_retry.lifetime_ticks
+        == hazard_before_retry.lifetime_ticks);
+    ARPG_REQUIRE(hazard_after_retry.player_latched
+        == hazard_before_retry.player_latched);
+    ARPG_REQUIRE(!accepted_combat_action);
+    const auto abyss_retry = abyss.pending_save();
+    ARPG_REQUIRE(abyss_retry.has_value());
+    ARPG_REQUIRE(abyss_retry->kind == abyss_pending->kind);
+    ARPG_REQUIRE(abyss_retry->expected_generation
+        == abyss_pending->expected_generation);
+    ARPG_REQUIRE(arpg::dungeon::same_run_state(
+        abyss_retry->next_state, abyss_pending->next_state));
+    ARPG_REQUIRE(abyss_retry->health_potion_claim.has_value());
+    ARPG_REQUIRE(abyss_retry->health_potion_claim->spawn_ordinals
+        == abyss_pending->health_potion_claim->spawn_ordinals);
+    ARPG_REQUIRE(abyss_retry->health_potion_claim->count
+        == abyss_pending->health_potion_claim->count);
+    ARPG_REQUIRE(abyss_retry->health_potion_claim->expected_hp
+        == abyss_pending->health_potion_claim->expected_hp);
+    ARPG_REQUIRE(abyss_retry->health_potion_claim->expected_max_hp
+        == abyss_pending->health_potion_claim->expected_max_hp);
+    return {};
+}
+
+arpg::test::Failure
+abyss_clear_health_retry_gates_all_public_mutation_entries() noexcept {
+    const auto retry_is_intact = [](const DungeonSession& session) noexcept {
+        return session.phase() == arpg::dungeon::RoomPhase::combat
+            && session.pending_save_view() == nullptr
+            && session.snapshot().diagnostics.fault
+                == arpg::dungeon::DungeonFault::none
+            && arpg::test::DungeonSessionTestAccess::
+                health_potion_abyss_clear_retry_pending(session);
+    };
+
+    DungeonSession nearby{DungeonRules{}, retry_gate_state(306U)};
+    ARPG_REQUIRE(enter_health_potion_abyss_clear_retry(nearby));
+    const auto nearby_before = nearby.snapshot();
+    const DungeonRunState nearby_stable_before =
+        arpg::test::stable_state(nearby);
+    nearby.request_nearby_pickups({100.0F, 0.0F, 0.0F});
+    ARPG_REQUIRE(retry_is_intact(nearby));
+    ARPG_REQUIRE(nearby.snapshot().ground_health_potion_count
+        == nearby_before.ground_health_potion_count);
+    ARPG_REQUIRE(arpg::dungeon::same_run_state(
+        arpg::test::stable_state(nearby), nearby_stable_before));
+
+    DungeonSession equipment{DungeonRules{}, retry_gate_state(307U)};
+    ARPG_REQUIRE(enter_health_potion_abyss_clear_retry(equipment));
+    ARPG_REQUIRE(equipment.request_equip(7001U)
+        == arpg::dungeon::RequestResult::rejected);
+    ARPG_REQUIRE(retry_is_intact(equipment));
+
+    DungeonSession crafting{DungeonRules{}, retry_gate_state(308U)};
+    ARPG_REQUIRE(enter_health_potion_abyss_clear_retry(crafting));
+    ARPG_REQUIRE(crafting.request_craft(
+        arpg::items::MaterialId::transmute, 7001U)
+        == arpg::dungeon::RequestResult::rejected);
+    ARPG_REQUIRE(retry_is_intact(crafting));
+
+    DungeonSession loadout{DungeonRules{}, retry_gate_state(309U)};
+    ARPG_REQUIRE(enter_health_potion_abyss_clear_retry(loadout));
+    ARPG_REQUIRE(loadout.request_remove_active_skill(0U)
+        == arpg::dungeon::RequestResult::rejected);
+    ARPG_REQUIRE(retry_is_intact(loadout));
+
+    DungeonSession reset{DungeonRules{}, retry_gate_state(310U)};
+    ARPG_REQUIRE(enter_health_potion_abyss_clear_retry(reset));
+    ARPG_REQUIRE(reset.reset_current_room()
+        == arpg::dungeon::RequestResult::rejected);
+    ARPG_REQUIRE(retry_is_intact(reset));
+
+    const arpg::dungeon::PendingSaveResult stray_result{
+        arpg::dungeon::SaveDisposition::not_committed,
+        0U,
+        {},
+        arpg::dungeon::PendingSaveKind::transition,
+    };
+    DungeonSession save_resolve{DungeonRules{}, retry_gate_state(311U)};
+    ARPG_REQUIRE(enter_health_potion_abyss_clear_retry(save_resolve));
+    save_resolve.resolve_pending_save(stray_result);
+    ARPG_REQUIRE(retry_is_intact(save_resolve));
+
+    DungeonSession transition_resolve{
+        DungeonRules{}, retry_gate_state(312U)};
+    ARPG_REQUIRE(enter_health_potion_abyss_clear_retry(transition_resolve));
+    transition_resolve.resolve_pending_transition(stray_result);
+    ARPG_REQUIRE(retry_is_intact(transition_resolve));
+
+    nearby.tick({});
+    const auto rebuilt = nearby.pending_save();
+    ARPG_REQUIRE(rebuilt.has_value());
+    ARPG_REQUIRE(rebuilt->kind
+        == arpg::dungeon::PendingSaveKind::abyss_clear);
+    ARPG_REQUIRE(rebuilt->health_potion_claim.has_value());
+    ARPG_REQUIRE(rebuilt->health_potion_claim->count == 1U);
+    ARPG_REQUIRE(rebuilt->health_potion_claim->spawn_ordinals[0] == 4U);
+    return {};
+}
+
+arpg::test::Failure
+clear_batch_selects_only_until_health_is_strictly_above_75_percent()
+    noexcept {
+    DungeonSession session{DungeonRules{}, potion_state(303U)};
+    arpg::test::set_phase(session, arpg::dungeon::RoomPhase::combat);
+    arpg::test::set_player_health(session, 1, 1000);
+    install_potion(session, 9U);
+    install_potion(session, 2U);
+    install_potion(session, 6U);
+    install_potion(session, 3U);
+    arpg::test::prepare_room_clear(session);
+    const auto pending = session.pending_save();
+    ARPG_REQUIRE(pending.has_value());
+    ARPG_REQUIRE(pending->kind == arpg::dungeon::PendingSaveKind::room_clear);
+    ARPG_REQUIRE(pending->health_potion_claim.has_value());
+    ARPG_REQUIRE(pending->health_potion_claim->count == 3U);
+    ARPG_REQUIRE(pending->health_potion_claim->spawn_ordinals[0] == 2U);
+    ARPG_REQUIRE(pending->health_potion_claim->spawn_ordinals[1] == 3U);
+    ARPG_REQUIRE(pending->health_potion_claim->spawn_ordinals[2] == 6U);
+    ARPG_REQUIRE(session.snapshot().combat->player.hp == 1);
+    ARPG_REQUIRE(session.snapshot().ground_health_potion_count == 4U);
+    ARPG_REQUIRE(!session.snapshot().health_potion_pickup_receipt.valid);
+    session.resolve_pending_save({
+        arpg::dungeon::SaveDisposition::committed,
+        pending->expected_generation,
+        pending->next_state,
+        pending->kind,
+    });
+    const auto committed = session.snapshot();
+    ARPG_REQUIRE(committed.phase == arpg::dungeon::RoomPhase::cleared);
+    ARPG_REQUIRE(committed.combat->player.hp == 751);
+    ARPG_REQUIRE(committed.ground_health_potion_count == 1U);
+    ARPG_REQUIRE(committed.ground_health_potions[0].spawn_ordinal == 9U);
+    ARPG_REQUIRE(committed.health_potion_pickup_receipt.valid);
+    ARPG_REQUIRE(committed.health_potion_pickup_receipt.room_clear);
+    ARPG_REQUIRE(committed.health_potion_pickup_receipt.consumed_count == 3U);
+    ARPG_REQUIRE(committed.health_potion_pickup_receipt.restored_hp == 750);
+    ARPG_REQUIRE(claim_bit_is_set(arpg::test::stable_state(session),
+        arpg::dungeon::health_potion_claim_ordinal(2U)));
+    ARPG_REQUIRE(claim_bit_is_set(arpg::test::stable_state(session),
+        arpg::dungeon::health_potion_claim_ordinal(3U)));
+    ARPG_REQUIRE(claim_bit_is_set(arpg::test::stable_state(session),
+        arpg::dungeon::health_potion_claim_ordinal(6U)));
+    ARPG_REQUIRE(!claim_bit_is_set(arpg::test::stable_state(session),
+        arpg::dungeon::health_potion_claim_ordinal(9U)));
+
+    DungeonSession wrong_kind_normal{DungeonRules{}, potion_state(306U)};
+    arpg::test::set_phase(
+        wrong_kind_normal, arpg::dungeon::RoomPhase::combat);
+    arpg::test::set_player_health(wrong_kind_normal, 500, 1000);
+    install_potion(wrong_kind_normal, 4U);
+    arpg::test::prepare_room_clear(wrong_kind_normal);
+    const auto wrong_kind_pending = wrong_kind_normal.pending_save();
+    ARPG_REQUIRE(wrong_kind_pending.has_value());
+    const DungeonRunState wrong_kind_stable_before =
+        arpg::test::stable_state(wrong_kind_normal);
+    wrong_kind_normal.resolve_pending_save({
+        arpg::dungeon::SaveDisposition::committed,
+        wrong_kind_pending->expected_generation,
+        wrong_kind_pending->next_state,
+        arpg::dungeon::PendingSaveKind::material_pickup,
+    });
+    const auto wrong_kind_visible = wrong_kind_normal.snapshot();
+    ARPG_REQUIRE(wrong_kind_visible.phase
+        == arpg::dungeon::RoomPhase::faulted);
+    ARPG_REQUIRE(wrong_kind_visible.diagnostics.fault
+        == arpg::dungeon::DungeonFault::save_receipt_mismatch);
+    ARPG_REQUIRE(wrong_kind_visible.combat->player.hp == 500);
+    ARPG_REQUIRE(wrong_kind_visible.ground_health_potion_count == 1U);
+    ARPG_REQUIRE(!wrong_kind_visible.health_potion_pickup_receipt.valid);
+    ARPG_REQUIRE(arpg::dungeon::same_run_state(
+        arpg::test::stable_state(wrong_kind_normal),
+        wrong_kind_stable_before));
+    return {};
+}
+
+arpg::test::Failure
+final_kill_high_health_keeps_potion_until_room_transition() noexcept {
+    DungeonSession session{DungeonRules{}, potion_state(304U)};
+    arpg::test::set_phase(session, arpg::dungeon::RoomPhase::combat);
+    arpg::test::set_player_health(session, 751, 1000);
+    install_potion(session, 5U);
+    arpg::test::prepare_room_clear(session);
+    ARPG_REQUIRE(!session.pending_save().has_value());
+    ARPG_REQUIRE(session.snapshot().phase == arpg::dungeon::RoomPhase::cleared);
+    ARPG_REQUIRE(session.snapshot().ground_health_potion_count == 1U);
+    ARPG_REQUIRE(session.snapshot().combat->player.hp == 751);
+    ARPG_REQUIRE(!session.snapshot().health_potion_pickup_receipt.valid);
+    session.tick({});
+    ARPG_REQUIRE(session.snapshot().phase
+        == arpg::dungeon::RoomPhase::awaiting_exit);
+    ARPG_REQUIRE(session.snapshot().ground_health_potion_count == 1U);
+    arpg::test::set_current_room_hole(session, true);
+    ARPG_REQUIRE(session.request_descent(true));
+    const auto pending = session.pending_transition();
+    ARPG_REQUIRE(pending.has_value());
+    ARPG_REQUIRE(session.snapshot().ground_health_potion_count == 1U);
+    session.resolve_pending_transition({
+        arpg::dungeon::SaveDisposition::committed,
+        pending->expected_generation,
+        pending->next_state,
+    });
+    ARPG_REQUIRE(session.snapshot().phase
+        == arpg::dungeon::RoomPhase::transitioning);
+    ARPG_REQUIRE(session.snapshot().ground_health_potion_count == 0U);
+    return {};
+}
+
+arpg::test::Failure
+abyss_clear_potion_uses_post_clear_actual_max_health() noexcept {
+    DungeonSession session{DungeonRules{}, potion_state(305U)};
+    arpg::test::set_phase(session, arpg::dungeon::RoomPhase::combat);
+    const int base_max = session.snapshot().combat->player.max_hp;
+    arpg::test::set_started_life_sacrifice_abyss_room(session);
+    const int sacrificed_max = session.snapshot().combat->player.max_hp;
+    ARPG_REQUIRE(sacrificed_max > 0);
+    ARPG_REQUIRE(sacrificed_max < base_max);
+    arpg::test::set_player_health(session, 1, sacrificed_max);
+    install_potion(session, 7U);
+    arpg::test::prepare_room_clear(session);
+    const auto pending = session.pending_save();
+    ARPG_REQUIRE(pending.has_value());
+    ARPG_REQUIRE(pending->kind == arpg::dungeon::PendingSaveKind::abyss_clear);
+    ARPG_REQUIRE(pending->health_potion_claim.has_value());
+    ARPG_REQUIRE(pending->health_potion_claim->expected_max_hp
+        == sacrificed_max);
+    ARPG_REQUIRE(session.snapshot().combat->player.hp == 1);
+    ARPG_REQUIRE(session.snapshot().ground_health_potion_count == 1U);
+    session.resolve_pending_save({
+        arpg::dungeon::SaveDisposition::committed,
+        pending->expected_generation,
+        pending->next_state,
+        pending->kind,
+    });
+    const auto committed = session.snapshot();
+    const int mapped = arpg::abyss::map_resource_ratio(
+        1, sacrificed_max, base_max, true).value_or(0);
+    const int expected_restore = arpg::combat::scale_basis_points(
+        base_max, arpg::dungeon::kHealthPotionRestoreBp,
+        arpg::combat::BasisPointRounding::ceil);
+    ARPG_REQUIRE(committed.phase == arpg::dungeon::RoomPhase::cleared);
+    ARPG_REQUIRE(committed.combat->player.max_hp == base_max);
+    ARPG_REQUIRE(committed.combat->player.hp == mapped + expected_restore);
+    ARPG_REQUIRE(committed.health_potion_pickup_receipt.valid);
+    ARPG_REQUIRE(committed.health_potion_pickup_receipt.room_clear);
+    ARPG_REQUIRE(committed.health_potion_pickup_receipt.consumed_count == 1U);
+    ARPG_REQUIRE(committed.health_potion_pickup_receipt.restored_hp
+        == expected_restore);
+    ARPG_REQUIRE(committed.ground_health_potion_count == 0U);
+
+    DungeonSession wrong_kind_abyss{DungeonRules{}, potion_state(307U)};
+    arpg::test::set_phase(
+        wrong_kind_abyss, arpg::dungeon::RoomPhase::combat);
+    arpg::test::set_started_abyss_room(
+        wrong_kind_abyss, arpg::abyss::AbyssDanger::low);
+    arpg::test::set_player_health(wrong_kind_abyss, 500, 1000);
+    install_potion(wrong_kind_abyss, 4U);
+    arpg::test::prepare_room_clear(wrong_kind_abyss);
+    const auto wrong_kind_pending = wrong_kind_abyss.pending_save();
+    ARPG_REQUIRE(wrong_kind_pending.has_value());
+    const DungeonRunState wrong_kind_stable_before =
+        arpg::test::stable_state(wrong_kind_abyss);
+    wrong_kind_abyss.resolve_pending_save({
+        arpg::dungeon::SaveDisposition::committed,
+        wrong_kind_pending->expected_generation,
+        wrong_kind_pending->next_state,
+        arpg::dungeon::PendingSaveKind::room_clear,
+    });
+    const auto wrong_kind_visible = wrong_kind_abyss.snapshot();
+    ARPG_REQUIRE(wrong_kind_visible.phase
+        == arpg::dungeon::RoomPhase::faulted);
+    ARPG_REQUIRE(wrong_kind_visible.diagnostics.fault
+        == arpg::dungeon::DungeonFault::save_receipt_mismatch);
+    ARPG_REQUIRE(wrong_kind_visible.combat->player.hp == 500);
+    ARPG_REQUIRE(wrong_kind_visible.ground_health_potion_count == 1U);
+    ARPG_REQUIRE(!wrong_kind_visible.health_potion_pickup_receipt.valid);
+    ARPG_REQUIRE(arpg::dungeon::same_run_state(
+        arpg::test::stable_state(wrong_kind_abyss),
+        wrong_kind_stable_before));
+    return {};
+}
+
+constexpr arpg::test::TestCase kCases[] = {
+    {"health potion rules frozen", &health_potion_rules_are_frozen},
+    {"health potion deterministic roll",
+        &health_potion_roll_is_deterministic_and_has_both_outcomes},
+    {"health potion exact threshold", &health_potion_threshold_is_integer_exact},
+    {"coupon priority and common potion coexistence",
+        &coupon_has_priority_and_common_material_can_coexist_with_potion},
+    {"potions preserve material and coupon rolls",
+        &adding_potions_does_not_change_existing_material_or_coupon_rolls},
+    {"fixed potion pool boundaries and no overwrite",
+        &ground_potion_pool_accepts_spawn_zero_and_191_without_overwrite},
+    {"potion snapshot sorted and room exit clears",
+        &potion_snapshot_is_sorted_by_spawn_ordinal_and_clears_on_room_exit},
+    {"potion threshold and distance-free auto-use",
+        &potion_above_threshold_stays_grounded_but_75_percent_ignores_distance},
+    {"potion stable scan and post-commit recheck",
+        &potion_auto_use_scans_stable_spawn_order_and_rechecks_after_commit},
+    {"dead player and death snapshot reject potion",
+        &dead_player_or_death_snapshot_never_requests_or_consumes_potion},
+    {"committed potion publishes exact receipt",
+        &committed_pickup_heals_caps_removes_and_publishes_exact_receipt},
+    {"failed potion save outcomes stay atomic",
+        &failed_or_indeterminate_save_never_heals_removes_or_reports_success},
+    {"potion cache mismatches fault before side effects",
+        &mismatched_pending_or_replaced_ground_faults_before_health_side_effects},
+    {"durable potion claim never replays after reload",
+        &committed_claim_without_runtime_resolve_does_not_replay_after_reload},
+    {"final kill folds sorted potions into clear save",
+        &final_kill_low_health_folds_sorted_potions_into_clear_transaction},
+    {"abyss clear health retry gates public mutations",
+        &abyss_clear_health_retry_gates_all_public_mutation_entries},
+    {"clear potion batch stops strictly above threshold",
+        &clear_batch_selects_only_until_health_is_strictly_above_75_percent},
+    {"high health clear preserves potion until transition",
+        &final_kill_high_health_keeps_potion_until_room_transition},
+    {"abyss clear potion uses post-clear max health",
+        &abyss_clear_potion_uses_post_clear_actual_max_health},
+};
+
+}  // namespace
+
+arpg::test::TestSuite dungeon_health_potion_suite() noexcept {
+    return arpg::test::make_suite("dungeon_health_potion", kCases);
+}
diff --git a/tests/dungeon/dungeon_stress_tests.cpp b/tests/dungeon/dungeon_stress_tests.cpp
index cdcf226..73f9095 100644
--- a/tests/dungeon/dungeon_stress_tests.cpp
+++ b/tests/dungeon/dungeon_stress_tests.cpp
@@ -140,20 +140,21 @@ std::uint64_t golden_room_trace_hash(
 }
 
 struct StressSummary final {
     std::uint32_t dungeon_events{};
     std::uint32_t combat_events{};
     std::uint32_t dungeon_overflow{};
     std::uint32_t relay_overflow{};
     std::uint32_t combat_overflow{};
     std::uint32_t input_overflow{};
     std::uint64_t save_boundaries{};
+    std::uint64_t health_potion_save_boundaries{};
     std::uint64_t save_boundary_allocations{};
     std::uint64_t room_load_boundaries{};
     std::uint64_t room_load_allocations{};
     std::uint64_t unexpected_hot_path_allocations{};
 };
 
 bool same_vec(const arpg::combat::Vec3& lhs, const arpg::combat::Vec3& rhs) noexcept {
     return lhs.x == rhs.x && lhs.y == rhs.y && lhs.z == rhs.z;
 }
 
@@ -219,20 +220,28 @@ bool same_ground_item(
 
 bool same_ground_material(
     const arpg::dungeon::GroundMaterialSnapshot& lhs,
     const arpg::dungeon::GroundMaterialSnapshot& rhs) noexcept {
     return lhs.ordinal == rhs.ordinal
         && lhs.source == rhs.source
         && same_vec(lhs.position, rhs.position)
         && lhs.material == rhs.material;
 }
 
+bool same_ground_health_potion(
+    const arpg::dungeon::GroundHealthPotionSnapshot& lhs,
+    const arpg::dungeon::GroundHealthPotionSnapshot& rhs) noexcept {
+    return lhs.spawn_ordinal == rhs.spawn_ordinal
+        && lhs.claim_ordinal == rhs.claim_ordinal
+        && same_vec(lhs.position, rhs.position);
+}
+
 bool same_snapshot(const DungeonSnapshot& lhs, const DungeonSnapshot& rhs) noexcept {
     if (lhs.session_tick != rhs.session_tick || lhs.root_seed != rhs.root_seed
             || lhs.commit_generation != rhs.commit_generation
             || lhs.room_index != rhs.room_index
             || lhs.room_seed != rhs.room_seed || lhs.phase != rhs.phase
             || lhs.depth != rhs.depth
             || lhs.floor_room_index != rhs.floor_room_index
             || lhs.biases != rhs.biases
             || lhs.has_active_room != rhs.has_active_room
             || lhs.exits_open != rhs.exits_open
@@ -251,28 +260,40 @@ bool same_snapshot(const DungeonSnapshot& lhs, const DungeonSnapshot& rhs) noexc
                 != rhs.abyss_exit_confirmation_armed
             || lhs.abyss_exit_confirmation_transition
                 != rhs.abyss_exit_confirmation_transition
             || lhs.abyss_exit_confirmation_direction
                 != rhs.abyss_exit_confirmation_direction
             || lhs.has_pending_transition != rhs.has_pending_transition
             || lhs.inventory_count != rhs.inventory_count
             || lhs.equipped_ids != rhs.equipped_ids
             || lhs.ground_item_count != rhs.ground_item_count
             || lhs.ground_material_count != rhs.ground_material_count
+            || lhs.ground_health_potion_count
+                != rhs.ground_health_potion_count
             || lhs.material_pickup_receipt.valid
                 != rhs.material_pickup_receipt.valid
             || lhs.material_pickup_receipt.room_vacuum
                 != rhs.material_pickup_receipt.room_vacuum
             || lhs.material_pickup_receipt.commit_generation
                 != rhs.material_pickup_receipt.commit_generation
             || lhs.material_pickup_receipt.counts
                 != rhs.material_pickup_receipt.counts
+            || lhs.health_potion_pickup_receipt.valid
+                != rhs.health_potion_pickup_receipt.valid
+            || lhs.health_potion_pickup_receipt.room_clear
+                != rhs.health_potion_pickup_receipt.room_clear
+            || lhs.health_potion_pickup_receipt.consumed_count
+                != rhs.health_potion_pickup_receipt.consumed_count
+            || lhs.health_potion_pickup_receipt.commit_generation
+                != rhs.health_potion_pickup_receipt.commit_generation
+            || lhs.health_potion_pickup_receipt.restored_hp
+                != rhs.health_potion_pickup_receipt.restored_hp
             || lhs.pending_save_kind != rhs.pending_save_kind
             || lhs.pending_pickup_ordinal != rhs.pending_pickup_ordinal
             || lhs.pending_material_pickup_ordinal
                 != rhs.pending_material_pickup_ordinal
             || lhs.encounter.total_budget != rhs.encounter.total_budget
             || lhs.encounter.current_wave_budget
                 != rhs.encounter.current_wave_budget
             || lhs.encounter.current_wave_spawn_count
                 != rhs.encounter.current_wave_spawn_count
             || lhs.encounter.plan_valid != rhs.encounter.plan_valid
@@ -281,38 +302,48 @@ bool same_snapshot(const DungeonSnapshot& lhs, const DungeonSnapshot& rhs) noexc
             || lhs.diagnostics.combat_relay_overflow_count
                 != rhs.diagnostics.combat_relay_overflow_count
             || lhs.diagnostics.rejected_exit_count
                 != rhs.diagnostics.rejected_exit_count
             || lhs.diagnostics.save_failure_count
                 != rhs.diagnostics.save_failure_count
             || lhs.diagnostics.ground_saturation_count
                 != rhs.diagnostics.ground_saturation_count
             || lhs.diagnostics.material_ground_saturation_count
                 != rhs.diagnostics.material_ground_saturation_count
+            || lhs.diagnostics.health_potion_ground_saturation_count
+                != rhs.diagnostics.health_potion_ground_saturation_count
             || lhs.diagnostics.fault != rhs.diagnostics.fault
             || lhs.diagnostics.room_index_overflow
                 != rhs.diagnostics.room_index_overflow
             || lhs.combat.has_value() != rhs.combat.has_value()) {
         return false;
     }
     for (std::size_t index = 0U; index < lhs.ground_items.size(); ++index) {
         if (!same_ground_item(lhs.ground_items[index], rhs.ground_items[index])) {
             return false;
         }
     }
     for (std::size_t index = 0U;
             index < lhs.ground_materials.size(); ++index) {
         if (!same_ground_material(
                 lhs.ground_materials[index], rhs.ground_materials[index])) {
             return false;
         }
     }
+    for (std::size_t index = 0U;
+            index < lhs.ground_health_potion_count; ++index) {
+        if (!same_ground_health_potion(
+                lhs.ground_health_potions[index],
+                rhs.ground_health_potions[index])) {
+            return false;
+        }
+    }
     return !lhs.combat.has_value() || same_combat(*lhs.combat, *rhs.combat);
 }
 
 bool same_event(const DungeonEvent& lhs, const DungeonEvent& rhs) noexcept {
     return lhs.kind == rhs.kind && lhs.session_tick == rhs.session_tick
         && lhs.room_index == rhs.room_index && lhs.room_seed == rhs.room_seed
         && lhs.destination_room_index == rhs.destination_room_index
         && lhs.destination_room_seed == rhs.destination_room_seed
         && lhs.transition == rhs.transition
         && lhs.direction == rhs.direction
@@ -361,28 +392,34 @@ void record_allocations(
 }
 
 void tracked_tick(
     DungeonSession& session,
     MovementInput movement,
     StressSummary& summary) noexcept {
     const std::uint64_t before = arpg::test::allocation_count();
     const RoomPhase phase_before = session.snapshot().phase;
     session.tick(movement);
     const DungeonSnapshot state = session.snapshot();
+    const bool health_potion_save_boundary =
+        state.phase == RoomPhase::committing
+        && state.pending_save_kind.has_value()
+        && *state.pending_save_kind
+            == arpg::dungeon::PendingSaveKind::health_potion_pickup;
     const bool save_boundary = state.phase == RoomPhase::committing
         && state.pending_save_kind.has_value()
         && (*state.pending_save_kind
                 == arpg::dungeon::PendingSaveKind::transition
             || *state.pending_save_kind
                 == arpg::dungeon::PendingSaveKind::loot_pickup
             || *state.pending_save_kind
                 == arpg::dungeon::PendingSaveKind::material_pickup
+            || health_potion_save_boundary
             || *state.pending_save_kind
                 == arpg::dungeon::PendingSaveKind::abyss_start
             || *state.pending_save_kind
                 == arpg::dungeon::PendingSaveKind::abyss_clear
             || *state.pending_save_kind
                 == arpg::dungeon::PendingSaveKind::room_clear
             || *state.pending_save_kind
                 == arpg::dungeon::PendingSaveKind::abyss_reward_materialized);
     const bool protected_abyss_boundary = state.phase == RoomPhase::committing
         && state.pending_save_kind.has_value()
@@ -392,20 +429,23 @@ void tracked_tick(
                 == arpg::dungeon::PendingSaveKind::abyss_abandon);
     const bool room_load = phase_before == RoomPhase::transitioning
         && (state.phase == RoomPhase::locked
             || (state.phase == RoomPhase::committing
                 && state.pending_save_kind.has_value()
                 && *state.pending_save_kind
                     == arpg::dungeon::PendingSaveKind::abyss_start));
     const std::uint64_t delta = arpg::test::allocation_count() - before;
     if (save_boundary || protected_abyss_boundary) {
         ++summary.save_boundaries;
+        if (health_potion_save_boundary) {
+            ++summary.health_potion_save_boundaries;
+        }
         summary.save_boundary_allocations += delta;
     } else if (room_load) {
         ++summary.room_load_boundaries;
         summary.room_load_allocations += delta;
     } else {
         summary.unexpected_hot_path_allocations += delta;
     }
 }
 
 bool tick_equal(
@@ -507,20 +547,21 @@ bool confirm_pending_save(
     const auto next_room_seed = pending->next_state.current_room.seed;
     session.resolve_pending_save({
         arpg::dungeon::SaveDisposition::committed,
         expected_generation,
         pending->next_state,
     });
     const DungeonSnapshot saved = session.snapshot();
     record_allocations(summary, before, true);
     if (kind == arpg::dungeon::PendingSaveKind::loot_pickup
             || kind == arpg::dungeon::PendingSaveKind::material_pickup
+            || kind == arpg::dungeon::PendingSaveKind::health_potion_pickup
             || kind == arpg::dungeon::PendingSaveKind::abyss_reward_claim) {
         return saved.phase == resume_phase
             && !saved.pending_save_kind.has_value()
             && saved.commit_generation == expected_generation;
     }
     if (kind == arpg::dungeon::PendingSaveKind::abyss_start) {
         return saved.phase == RoomPhase::locked
             && saved.has_active_room && saved.combat.has_value()
             && !saved.pending_save_kind.has_value()
             && saved.commit_generation == expected_generation;
@@ -541,20 +582,53 @@ bool confirm_pending_save(
     return (kind == arpg::dungeon::PendingSaveKind::transition
             || kind == arpg::dungeon::PendingSaveKind::abyss_abandon)
         && saved.phase == RoomPhase::transitioning
         && !saved.has_pending_transition
         && saved.commit_generation == expected_generation
         && saved.room_index == next_room_index
         && saved.room_seed == next_room_seed
         && !saved.has_active_room && !saved.combat.has_value();
 }
 
+bool exercise_health_potion_save_boundary(
+    DungeonSession& session,
+    StressSummary& summary) noexcept {
+    const DungeonSnapshot initial = session.snapshot();
+    if (!initial.combat.has_value()) return false;
+    arpg::test::DungeonSessionTestAccess::damage_current_player(
+        session, initial.combat->player.max_hp / 2);
+    const DungeonSnapshot damaged = session.snapshot();
+    if (!damaged.combat.has_value()
+            || damaged.combat->player.hp <= 0
+            || damaged.combat->player.hp * 4
+                > damaged.combat->player.max_hp * 3) {
+        return false;
+    }
+    arpg::test::install_ground_health_potion(
+        session, 0U, damaged.combat->player.position);
+    const std::uint64_t boundary_before =
+        summary.health_potion_save_boundaries;
+    tracked_tick(session, {}, summary);
+    const auto* pending = session.pending_save_view();
+    if (pending == nullptr
+            || pending->kind
+                != arpg::dungeon::PendingSaveKind::health_potion_pickup
+            || !confirm_pending_save(session, summary)) {
+        return false;
+    }
+    const DungeonSnapshot committed = session.snapshot();
+    return summary.health_potion_save_boundaries == boundary_before + 1U
+        && committed.health_potion_pickup_receipt.valid
+        && committed.health_potion_pickup_receipt.consumed_count == 1U
+        && committed.health_potion_pickup_receipt.restored_hp > 0;
+}
+
 bool drive_clear(DungeonSession& session, StressSummary& summary) noexcept {
     for (int tick = 0; tick < 8192; ++tick) {
         const DungeonSnapshot state = session.snapshot();
         if (state.phase == RoomPhase::committing) {
             if (!confirm_pending_save(session, summary)) return false;
             drain(session, summary);
             continue;
         }
         if (state.phase == RoomPhase::cleared) {
             tracked_tick(session, MovementInput{}, summary);
@@ -1059,20 +1133,27 @@ void print_real_input_trace(
         static_cast<unsigned long long>(trace.first_whiff_tick),
         trace.first_whiff_dx, trace.first_whiff_dy,
         static_cast<int>(trace.first_whiff_facing),
         static_cast<unsigned>(trace.first_whiff_ai),
         static_cast<unsigned>(trace.first_whiff_reaction),
         trace.player_position.x, trace.player_position.y,
         trace.target_position.x, trace.target_position.y);
 }
 
 arpg::test::Failure launcher_input_robot_clears_ten_minimal_committed_rooms() noexcept {
+    {
+        DungeonSession potion_probe;
+        StressSummary potion_summary{};
+        ARPG_REQUIRE(exercise_health_potion_save_boundary(
+            potion_probe, potion_summary));
+        ARPG_REQUIRE(potion_summary.health_potion_save_boundaries == 1U);
+    }
     constexpr std::uint64_t kSeed = 0x5245414C494E5055ULL;
     const DungeonRules rules = single_chaser_rules();
     const auto initial = arpg::dungeon::make_initial_run_state(kSeed, rules);
     ARPG_REQUIRE(initial.fault == arpg::dungeon::DungeonFault::none);
     DungeonSession session{rules, initial.state};
     const DungeonSnapshot first = session.snapshot();
     std::printf("[real-input] seed=%llu initial-room=%llu plan=%u/%u/%u\n",
         static_cast<unsigned long long>(kSeed),
         static_cast<unsigned long long>(first.room_index),
         static_cast<unsigned>(first.wave_count),
diff --git a/tests/dungeon/dungeon_test_main.cpp b/tests/dungeon/dungeon_test_main.cpp
index 7f4eeca..47acf84 100644
--- a/tests/dungeon/dungeon_test_main.cpp
+++ b/tests/dungeon/dungeon_test_main.cpp
@@ -18,20 +18,21 @@ arpg::test::TestSuite dungeon_rules_suite() noexcept;
 arpg::test::TestSuite dungeon_progression_suite() noexcept;
 arpg::test::TestSuite dungeon_query_suite() noexcept;
 arpg::test::TestSuite dungeon_passive_tree_suite() noexcept;
 arpg::test::TestSuite dungeon_item_transaction_suite() noexcept;
 arpg::test::TestSuite dungeon_crafting_transaction_suite() noexcept;
 arpg::test::TestSuite dungeon_skill_loadout_transaction_suite() noexcept;
 arpg::test::TestSuite dungeon_skill_cast_suite() noexcept;
 arpg::test::TestSuite dungeon_equipment_stress_suite() noexcept;
 arpg::test::TestSuite dungeon_loot_drop_suite() noexcept;
 arpg::test::TestSuite dungeon_material_loot_suite() noexcept;
+arpg::test::TestSuite dungeon_health_potion_suite() noexcept;
 arpg::test::TestSuite dungeon_transaction_suite() noexcept;
 arpg::test::TestSuite dungeon_abyss_reward_suite() noexcept;
 arpg::test::TestSuite dungeon_abyss_stress_suite() noexcept;
 arpg::test::TestSuite encounter_director_suite() noexcept;
 arpg::test::TestSuite dungeon_wave_suite() noexcept;
 arpg::test::TestSuite dungeon_progression_reward_suite() noexcept;
 arpg::test::TestSuite dungeon_affix_reward_suite() noexcept;
 arpg::test::TestSuite dungeon_affix_stress_suite() noexcept;
 
 namespace {
@@ -109,20 +110,30 @@ bool stage16_material_loot_only() noexcept {
 bool stage16_crafting_transaction_only() noexcept {
     char* value = nullptr;
     std::size_t length = 0U;
     const errno_t error = _dupenv_s(&value, &length,
         "ARPG_STAGE16_CRAFTING_TRANSACTION_ONLY");
     const bool enabled = error == 0 && value != nullptr;
     std::free(value);
     return enabled;
 }
 
+bool health_potion_only() noexcept {
+    char* value = nullptr;
+    std::size_t length = 0U;
+    const errno_t error = _dupenv_s(&value, &length,
+        "ARPG_HEALTH_POTION_ONLY");
+    const bool enabled = error == 0 && value != nullptr;
+    std::free(value);
+    return enabled;
+}
+
 bool stage17_skill_loadout_only() noexcept {
     char* value = nullptr;
     std::size_t length = 0U;
     const errno_t error = _dupenv_s(&value, &length,
         "ARPG_STAGE17_SKILL_LOADOUT_ONLY");
     const bool enabled = error == 0 && value != nullptr;
     std::free(value);
     return enabled;
 }
 
@@ -149,20 +160,21 @@ int main() {
         dungeon_progression_suite(),
         dungeon_query_suite(),
         dungeon_passive_tree_suite(),
         dungeon_item_transaction_suite(),
         dungeon_crafting_transaction_suite(),
         dungeon_skill_loadout_transaction_suite(),
         dungeon_skill_cast_suite(),
         dungeon_equipment_stress_suite(),
         dungeon_loot_drop_suite(),
         dungeon_material_loot_suite(),
+        dungeon_health_potion_suite(),
         dungeon_transaction_suite(),
         dungeon_abyss_reward_suite(),
         dungeon_abyss_stress_suite(),
         encounter_director_suite(),
         dungeon_wave_suite(),
         dungeon_progression_reward_suite(),
         dungeon_affix_reward_suite(),
         dungeon_affix_stress_suite(),
     };
 
@@ -224,21 +236,29 @@ int main() {
     }
 
     if (stage16_crafting_transaction_only()) {
         const arpg::test::TestSuite crafting_only[] = {
             dungeon_crafting_transaction_suite(),
         };
         return arpg::test::run_suites(crafting_only, 6,
             "stage 16 task 7 crafting and reinforcement transactions");
     }
 
+    if (health_potion_only()) {
+        const arpg::test::TestSuite potion_only[] = {
+            dungeon_health_potion_suite(),
+        };
+        return arpg::test::run_suites(potion_only, 19,
+            "task 6 health potion atomic auto-use");
+    }
+
     if (stage17_skill_loadout_only()) {
         const arpg::test::TestSuite loadout_only[] = {
             dungeon_skill_loadout_transaction_suite(),
         };
         return arpg::test::run_suites(loadout_only, 9,
             "stage 17 task 3 skill loadout transactions");
     }
 
-    return arpg::test::run_suites(suites, 287,
+    return arpg::test::run_suites(suites, 306,
         "stage 18 dungeon queries");
 }
diff --git a/tests/dungeon/dungeon_test_support.hpp b/tests/dungeon/dungeon_test_support.hpp
index 78f4919..1fc5b9e 100644
--- a/tests/dungeon/dungeon_test_support.hpp
+++ b/tests/dungeon/dungeon_test_support.hpp
@@ -1,24 +1,48 @@
 #pragma once
 
 #include "combat/fire_room_obstacle.hpp"
+#include "abyss/abyss_rules.hpp"
 #include "dungeon/dungeon_session.hpp"
 
 #include <algorithm>
 #include <array>
 #include <cmath>
 #include <cstddef>
 #include <cstdint>
 
 namespace arpg::test {
 
 struct DungeonSessionTestAccess final {
+    static bool commit_fixture_health_potion_if_eligible(
+        dungeon::DungeonSession& session) noexcept {
+        for (std::uint16_t spawn = 0U;
+             spawn < session.ground_health_potions_.size(); ++spawn) {
+            if (!session.ground_health_potions_[spawn].active) continue;
+            const dungeon::RequestResult requested =
+                session.request_health_potion_pickup(spawn);
+            if (requested == dungeon::RequestResult::rejected) continue;
+            if (requested == dungeon::RequestResult::faulted
+                    || !session.pending_save_.has_value()) {
+                return false;
+            }
+            const dungeon::PendingSaveResult committed{
+                dungeon::SaveDisposition::committed,
+                session.pending_save_->expected_generation,
+                session.pending_save_->next_state,
+                session.pending_save_->kind,
+            };
+            session.commit_pending_save(committed);
+            return session.phase_ != dungeon::RoomPhase::faulted;
+        }
+        return true;
+    }
     static const dungeon::RoomEncounterPlan& encounter_plan(
         const dungeon::DungeonSession& session) noexcept {
         return session.encounter_plan_;
     }
     static const combat::PlayerCombatBuild& player_build(
         const dungeon::DungeonSession& session) noexcept {
         return session.combat_->encounter_config_.player_build;
     }
     static const combat::CombatWorld* combat_world_address(
         const dungeon::DungeonSession& session) noexcept {
@@ -101,20 +125,27 @@ struct DungeonSessionTestAccess final {
         for (std::size_t index = 0; index < world.monsters_.slots_.size(); ++index) {
             combat::MonsterRuntime& monster = world.monsters_.slots_[index];
             if (!monster.active || monster.hp <= 0) {
                 continue;
             }
             monster.hp = 0;
             combat::AttackDefinition defeat{};
             defeat.feedback = combat::FeedbackLevel::light;
             world.apply_dummy_impact(index, defeat);
         }
+        session.relay_combat_events();
+        const bool has_later_wave = session.wave_index_ + 1U
+            < session.encounter_plan_.wave_count;
+        if (has_later_wave) {
+            static_cast<void>(
+                commit_fixture_health_potion_if_eligible(session));
+        }
     }
     static bool defeat_next_live_monster(
         dungeon::DungeonSession& session) noexcept {
         if (!session.combat_.has_value()) {
             return false;
         }
         combat::CombatWorld& world = *session.combat_;
         for (std::size_t index = 0U; index < world.monsters_.slots_.size(); ++index) {
             combat::MonsterRuntime& monster = world.monsters_.slots_[index];
             if (!monster.active || monster.hp <= 0
@@ -128,21 +159,22 @@ struct DungeonSessionTestAccess final {
         return false;
     }
     static bool relay_defeated(
         dungeon::DungeonSession& session,
         std::uint8_t wave_index,
         std::uint8_t target_index,
         combat::Vec3 position,
         bool reward_eligible = true,
         combat::MonsterId monster_id = combat::MonsterId::fire_bomber,
         std::uint16_t spawn_ordinal = 0xFFFFU,
-        std::uint16_t affix_score = 0U) noexcept {
+        std::uint16_t affix_score = 0U,
+        bool commit_health_potion = true) noexcept {
         if (!session.combat_.has_value() || wave_index >= 2U) {
             return false;
         }
         session.encounter_plan_.wave_count = 2U;
         session.encounter_plan_.waves[wave_index].spawn_count = 96U;
         session.wave_index_ = wave_index;
         combat::CombatEvent event{};
         event.kind = combat::CombatEventKind::defeated;
         event.target_index = target_index;
         event.position = position;
@@ -150,21 +182,22 @@ struct DungeonSessionTestAccess final {
         event.spawn_ordinal = spawn_ordinal == 0xFFFFU
             ? static_cast<std::uint16_t>(
                 static_cast<std::uint16_t>(wave_index) * 96U + target_index)
             : spawn_ordinal;
         event.affix_score = affix_score;
         event.reward_eligible = reward_eligible;
         if (!session.combat_->events_.try_push(event)) {
             return false;
         }
         session.relay_combat_events();
-        return true;
+        return !commit_health_potion
+            || commit_fixture_health_potion_if_eligible(session);
     }
     static void set_current_room_seed(
         dungeon::DungeonSession& session,
         std::uint64_t seed) noexcept {
         session.stable_state_.current_room.seed = seed;
     }
     static void set_current_room_depth(
         dungeon::DungeonSession& session,
         std::uint64_t depth) noexcept {
         session.stable_state_.current_room.depth = depth;
@@ -174,20 +207,31 @@ struct DungeonSessionTestAccess final {
         dungeon::RoomPhase phase) noexcept {
         session.phase_ = phase;
     }
     static void set_player_position(
         dungeon::DungeonSession& session,
         combat::Vec3 position) noexcept {
         if (session.combat_.has_value()) {
             session.combat_->player_.position = position;
         }
     }
+    static void set_player_health(
+        dungeon::DungeonSession& session, int hp, int max_hp) noexcept {
+        if (session.combat_.has_value()) {
+            session.combat_->player_.max_hp = max_hp;
+            session.combat_->player_.hp = hp;
+        }
+    }
+    static void clear_combat_world(
+        dungeon::DungeonSession& session) noexcept {
+        session.combat_.reset();
+    }
     static void clear_ground_item(
         dungeon::DungeonSession& session,
         std::uint16_t ordinal) noexcept {
         if (ordinal < session.ground_items_.size()) {
             session.ground_items_[ordinal] = {};
         }
     }
     static void install_ground_item(
         dungeon::DungeonSession& session,
         std::uint16_t ordinal,
@@ -246,36 +290,127 @@ struct DungeonSessionTestAccess final {
         session.stable_state_.current_room.is_abyss = true;
         session.stable_state_.abyss.lifecycle = abyss::AbyssLifecycle::started;
         session.stable_state_.abyss.danger = danger;
         session.stable_state_.abyss.rule = danger == abyss::AbyssDanger::low
             ? abyss::AbyssRuleId::thunderstorm
             : (danger == abyss::AbyssDanger::medium
                 ? abyss::AbyssRuleId::hunting_flames
                 : abyss::AbyssRuleId::chaos_expansion);
         session.stable_state_.abyss.rules_version = abyss::kAbyssRulesVersion;
     }
+    static void set_started_life_sacrifice_abyss_room(
+        dungeon::DungeonSession& session) noexcept {
+        session.stable_state_.current_room.is_abyss = true;
+        session.stable_state_.abyss.lifecycle = abyss::AbyssLifecycle::started;
+        session.stable_state_.abyss.danger = abyss::AbyssDanger::high;
+        session.stable_state_.abyss.rule = abyss::AbyssRuleId::life_sacrifice;
+        session.stable_state_.abyss.rules_version = abyss::kAbyssRulesVersion;
+        if (session.combat_.has_value()) {
+            session.combat_->encounter_config_.abyss =
+                abyss::combat_config_for(abyss::AbyssRuleId::life_sacrifice);
+            session.combat_->apply_player_build(
+                session.combat_->encounter_config_.player_build);
+        }
+    }
     static void clear_all_ground_items(
         dungeon::DungeonSession& session) noexcept {
         session.ground_items_ = {};
     }
     static void install_ground_material(
         dungeon::DungeonSession& session,
         std::uint16_t ordinal,
         items::MaterialId material,
         combat::Vec3 position,
         dungeon::GroundMaterialSource source =
             dungeon::GroundMaterialSource::monster_common) noexcept {
         if (ordinal < session.ground_materials_.size()) {
             session.ground_materials_[ordinal] = {
                 true, ordinal, source, position, material};
         }
     }
+    static void install_ground_health_potion(
+        dungeon::DungeonSession& session,
+        std::uint16_t spawn_ordinal,
+        combat::Vec3 position) noexcept {
+        if (spawn_ordinal < session.ground_health_potions_.size()) {
+            session.ground_health_potions_[spawn_ordinal] = {
+                true, spawn_ordinal,
+                dungeon::health_potion_claim_ordinal(spawn_ordinal),
+                position};
+        }
+    }
+    static void replace_ground_health_potion(
+        dungeon::DungeonSession& session,
+        std::uint16_t slot,
+        dungeon::GroundHealthPotion replacement) noexcept {
+        if (slot < session.ground_health_potions_.size()) {
+            session.ground_health_potions_[slot] = replacement;
+        }
+    }
+    static void quiesce_current_room_for_clear_retry(
+        dungeon::DungeonSession& session) noexcept {
+        session.encounter_plan_.wave_count = static_cast<std::uint8_t>(
+            session.wave_index_ + 1U);
+        if (!session.combat_.has_value()) return;
+        for (combat::MonsterRuntime& monster
+                : session.combat_->monsters_.slots_) {
+            monster.active = false;
+            monster.hp = 0;
+        }
+    }
+    static bool install_delayed_abyss_environment_hazard(
+        dungeon::DungeonSession& session) noexcept {
+        if (!session.combat_.has_value()) return false;
+        combat::CombatWorld& world = *session.combat_;
+        return world.spawn_environment_hazard(
+            combat::HazardKind::thunderstorm,
+            world.player_.position,
+            1.0F,
+            2U,
+            8U,
+            1U,
+            combat::DamagePacket{1},
+            500U,
+            modifiers::DamageType::lightning);
+    }
+    static bool health_potion_abyss_clear_retry_pending(
+        const dungeon::DungeonSession& session) noexcept {
+        return session.retry_health_potion_abyss_clear_before_combat_;
+    }
+    static void offset_pending_next_room_depth(
+        dungeon::DungeonSession& session, std::uint64_t offset) noexcept {
+        if (session.pending_save_.has_value()) {
+            session.pending_save_->next_state.current_room.depth += offset;
+        }
+    }
+    static void set_pending_health_potion_claim_count(
+        dungeon::DungeonSession& session, std::uint8_t count) noexcept {
+        if (session.pending_save_.has_value()
+                && session.pending_save_->health_potion_claim.has_value()) {
+            session.pending_save_->health_potion_claim->count = count;
+        }
+    }
+    static void set_pending_next_material_claim_bit(
+        dungeon::DungeonSession& session, std::uint16_t ordinal) noexcept {
+        if (!session.pending_save_.has_value()) return;
+        auto& bits = session.pending_save_->next_state.item_ownership
+            .material_claimed_drop_bits;
+        const std::size_t word = ordinal / 64U;
+        const std::uint8_t bit = static_cast<std::uint8_t>(ordinal % 64U);
+        if (word < bits.size()) {
+            bits[word] |= std::uint64_t{1U} << bit;
+        }
+    }
+    static bool pending_material_cache_consistent(
+        const dungeon::DungeonSession& session) noexcept {
+        return session.pending_material_cache_consistent();
+    }
     static void prepare_room_clear(
         dungeon::DungeonSession& session) noexcept {
         session.prepare_room_clear();
     }
     static void offset_pending_abyss_reward_position(
         dungeon::DungeonSession& session,
         combat::Vec3 offset) noexcept {
         if (session.pending_abyss_reward_.has_value()) {
             auto& position = session.pending_abyss_reward_->ground.position;
             position.x += offset.x;
@@ -311,20 +446,24 @@ struct DungeonSessionTestAccess final {
     static const std::array<dungeon::GroundItem,
         dungeon::kGroundDropCapacity>& ground_items(
         const dungeon::DungeonSession& session) noexcept {
         return session.ground_items_;
     }
     static const std::array<dungeon::GroundMaterial,
         dungeon::kGroundMaterialCapacity>& ground_materials(
         const dungeon::DungeonSession& session) noexcept {
         return session.ground_materials_;
     }
+    static void clear_rolled_material_claims(
+        dungeon::DungeonSession& session) noexcept {
+        session.rolled_material_bits_ = {};
+    }
 };
 
 inline void force_defeat_current_wave(dungeon::DungeonSession& session) noexcept {
     DungeonSessionTestAccess::force_defeat_current_wave(session);
 }
 
 inline bool defeat_next_live_monster(dungeon::DungeonSession& session) noexcept {
     return DungeonSessionTestAccess::defeat_next_live_monster(session);
 }
 
@@ -370,24 +509,25 @@ inline void attempt_exit(dungeon::DungeonSession& session,
 }
 
 inline bool relay_defeated(
     dungeon::DungeonSession& session,
     std::uint8_t wave_index,
     std::uint8_t target_index,
     combat::Vec3 position,
     bool reward_eligible = true,
     combat::MonsterId monster_id = combat::MonsterId::fire_bomber,
     std::uint16_t spawn_ordinal = 0xFFFFU,
-    std::uint16_t affix_score = 0U) noexcept {
+    std::uint16_t affix_score = 0U,
+    bool commit_health_potion = true) noexcept {
     return DungeonSessionTestAccess::relay_defeated(
         session, wave_index, target_index, position, reward_eligible,
-        monster_id, spawn_ordinal, affix_score);
+        monster_id, spawn_ordinal, affix_score, commit_health_potion);
 }
 
 inline void set_current_room_seed(
     dungeon::DungeonSession& session,
     std::uint64_t seed) noexcept {
     DungeonSessionTestAccess::set_current_room_seed(session, seed);
 }
 
 inline void set_current_room_depth(
     dungeon::DungeonSession& session,
@@ -400,20 +540,30 @@ inline void set_phase(
     dungeon::RoomPhase phase) noexcept {
     DungeonSessionTestAccess::set_phase(session, phase);
 }
 
 inline void set_player_position(
     dungeon::DungeonSession& session,
     combat::Vec3 position) noexcept {
     DungeonSessionTestAccess::set_player_position(session, position);
 }
 
+inline void set_player_health(
+    dungeon::DungeonSession& session, int hp, int max_hp) noexcept {
+    DungeonSessionTestAccess::set_player_health(session, hp, max_hp);
+}
+
+inline void clear_combat_world(
+    dungeon::DungeonSession& session) noexcept {
+    DungeonSessionTestAccess::clear_combat_world(session);
+}
+
 inline void clear_ground_item(
     dungeon::DungeonSession& session,
     std::uint16_t ordinal) noexcept {
     DungeonSessionTestAccess::clear_ground_item(session, ordinal);
 }
 
 inline void install_ground_item(
     dungeon::DungeonSession& session,
     std::uint16_t ordinal,
     const items::ItemInstance& item,
@@ -455,36 +605,91 @@ inline void set_pending_pickup_ordinal(
     std::uint16_t ordinal) noexcept {
     DungeonSessionTestAccess::set_pending_pickup_ordinal(session, ordinal);
 }
 
 inline void set_started_abyss_room(
     dungeon::DungeonSession& session,
     abyss::AbyssDanger danger) noexcept {
     DungeonSessionTestAccess::set_started_abyss_room(session, danger);
 }
 
+inline void set_started_life_sacrifice_abyss_room(
+    dungeon::DungeonSession& session) noexcept {
+    DungeonSessionTestAccess::set_started_life_sacrifice_abyss_room(session);
+}
+
 inline void clear_all_ground_items(
     dungeon::DungeonSession& session) noexcept {
     DungeonSessionTestAccess::clear_all_ground_items(session);
 }
 
 inline void install_ground_material(
     dungeon::DungeonSession& session,
     std::uint16_t ordinal,
     items::MaterialId material,
     combat::Vec3 position,
     dungeon::GroundMaterialSource source =
         dungeon::GroundMaterialSource::monster_common) noexcept {
     DungeonSessionTestAccess::install_ground_material(
         session, ordinal, material, position, source);
 }
 
+inline void install_ground_health_potion(
+    dungeon::DungeonSession& session,
+    std::uint16_t spawn_ordinal,
+    combat::Vec3 position) noexcept {
+    DungeonSessionTestAccess::install_ground_health_potion(
+        session, spawn_ordinal, position);
+}
+
+inline void replace_ground_health_potion(
+    dungeon::DungeonSession& session,
+    std::uint16_t slot,
+    dungeon::GroundHealthPotion replacement) noexcept {
+    DungeonSessionTestAccess::replace_ground_health_potion(
+        session, slot, replacement);
+}
+
+inline void quiesce_current_room_for_clear_retry(
+    dungeon::DungeonSession& session) noexcept {
+    DungeonSessionTestAccess::quiesce_current_room_for_clear_retry(session);
+}
+
+inline bool install_delayed_abyss_environment_hazard(
+    dungeon::DungeonSession& session) noexcept {
+    return DungeonSessionTestAccess::install_delayed_abyss_environment_hazard(
+        session);
+}
+
+inline void offset_pending_next_room_depth(
+    dungeon::DungeonSession& session, std::uint64_t offset) noexcept {
+    DungeonSessionTestAccess::offset_pending_next_room_depth(session, offset);
+}
+
+inline void set_pending_health_potion_claim_count(
+    dungeon::DungeonSession& session, std::uint8_t count) noexcept {
+    DungeonSessionTestAccess::set_pending_health_potion_claim_count(
+        session, count);
+}
+
+inline void set_pending_next_material_claim_bit(
+    dungeon::DungeonSession& session, std::uint16_t ordinal) noexcept {
+    DungeonSessionTestAccess::set_pending_next_material_claim_bit(
+        session, ordinal);
+}
+
+inline bool pending_material_cache_consistent(
+    const dungeon::DungeonSession& session) noexcept {
+    return DungeonSessionTestAccess::pending_material_cache_consistent(
+        session);
+}
+
 inline void prepare_room_clear(
     dungeon::DungeonSession& session) noexcept {
     DungeonSessionTestAccess::prepare_room_clear(session);
 }
 
 inline void offset_pending_abyss_reward_position(
     dungeon::DungeonSession& session,
     combat::Vec3 offset) noexcept {
     DungeonSessionTestAccess::offset_pending_abyss_reward_position(
         session, offset);
@@ -519,20 +724,25 @@ inline void publish_run_state_reusing_items(
     DungeonSessionTestAccess::publish_run_state_reusing_items(
         destination, source);
 }
 
 inline const std::array<dungeon::GroundItem,
     dungeon::kGroundDropCapacity>& ground_items(
     const dungeon::DungeonSession& session) noexcept {
     return DungeonSessionTestAccess::ground_items(session);
 }
 
+inline void clear_rolled_material_claims(
+    dungeon::DungeonSession& session) noexcept {
+    DungeonSessionTestAccess::clear_rolled_material_claims(session);
+}
+
 inline bool same_encounter_plan(const dungeon::RoomEncounterPlan& left,
     const dungeon::RoomEncounterPlan& right) noexcept {
     if (left.wave_count != right.wave_count
             || left.total_budget != right.total_budget) return false;
     for (std::size_t wave = 0; wave < left.wave_count; ++wave) {
         const auto& a = left.waves[wave];
         const auto& b = right.waves[wave];
         if (a.spawn_count != b.spawn_count || a.spent_budget != b.spent_budget) return false;
         for (std::size_t spawn = 0; spawn < a.spawn_count; ++spawn) {
             if (a.spawns[spawn].id != b.spawns[spawn].id
