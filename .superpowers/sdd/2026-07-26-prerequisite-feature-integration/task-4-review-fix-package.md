# Review package: 8a848b30fafe87135cf432be8d0cd236c45c2c49..5a0e117

## Commits
5a0e117 test: harden potion pending-state determinism

## Files changed
 tests/dungeon/dungeon_stress_tests.cpp | 69 ++++++++++++++++++++++++++++++++++
 1 file changed, 69 insertions(+)

## Diff
diff --git a/tests/dungeon/dungeon_stress_tests.cpp b/tests/dungeon/dungeon_stress_tests.cpp
index 73f9095..a6d6ea8 100644
--- a/tests/dungeon/dungeon_stress_tests.cpp
+++ b/tests/dungeon/dungeon_stress_tests.cpp
@@ -10,20 +10,21 @@
 #include "items/item_generation.hpp"
 #include "passives/passive_tree_catalog.hpp"
 
 #include <algorithm>
 #include <array>
 #include <cmath>
 #include <cstddef>
 #include <cstdint>
 #include <cstdio>
 #include <memory>
+#include <optional>
 
 namespace {
 
 using arpg::combat::CombatEvent;
 using arpg::combat::CombatSnapshot;
 using arpg::combat::MovementInput;
 using arpg::dungeon::DungeonEvent;
 using arpg::dungeon::DungeonRules;
 using arpg::dungeon::DungeonSession;
 using arpg::dungeon::DungeonSnapshot;
@@ -228,20 +229,33 @@ bool same_ground_material(
 }
 
 bool same_ground_health_potion(
     const arpg::dungeon::GroundHealthPotionSnapshot& lhs,
     const arpg::dungeon::GroundHealthPotionSnapshot& rhs) noexcept {
     return lhs.spawn_ordinal == rhs.spawn_ordinal
         && lhs.claim_ordinal == rhs.claim_ordinal
         && same_vec(lhs.position, rhs.position);
 }
 
+bool same_pending_health_potion_claim(
+    const std::optional<arpg::dungeon::PendingHealthPotionClaim>& lhs,
+    const std::optional<arpg::dungeon::PendingHealthPotionClaim>& rhs) noexcept {
+    if (lhs.has_value() != rhs.has_value()) {
+        return false;
+    }
+    return !lhs.has_value()
+        || (lhs->spawn_ordinals == rhs->spawn_ordinals
+            && lhs->count == rhs->count
+            && lhs->expected_hp == rhs->expected_hp
+            && lhs->expected_max_hp == rhs->expected_max_hp);
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
@@ -284,20 +298,22 @@ bool same_snapshot(const DungeonSnapshot& lhs, const DungeonSnapshot& rhs) noexc
             || lhs.health_potion_pickup_receipt.consumed_count
                 != rhs.health_potion_pickup_receipt.consumed_count
             || lhs.health_potion_pickup_receipt.commit_generation
                 != rhs.health_potion_pickup_receipt.commit_generation
             || lhs.health_potion_pickup_receipt.restored_hp
                 != rhs.health_potion_pickup_receipt.restored_hp
             || lhs.pending_save_kind != rhs.pending_save_kind
             || lhs.pending_pickup_ordinal != rhs.pending_pickup_ordinal
             || lhs.pending_material_pickup_ordinal
                 != rhs.pending_material_pickup_ordinal
+            || lhs.pending_health_potion_spawn_ordinal
+                != rhs.pending_health_potion_spawn_ordinal
             || lhs.encounter.total_budget != rhs.encounter.total_budget
             || lhs.encounter.current_wave_budget
                 != rhs.encounter.current_wave_budget
             || lhs.encounter.current_wave_spawn_count
                 != rhs.encounter.current_wave_spawn_count
             || lhs.encounter.plan_valid != rhs.encounter.plan_valid
             || lhs.diagnostics.event_overflow_count
                 != rhs.diagnostics.event_overflow_count
             || lhs.diagnostics.combat_relay_overflow_count
                 != rhs.diagnostics.combat_relay_overflow_count
@@ -492,20 +508,22 @@ bool commit_equal(
     DungeonSession& rhs) noexcept {
     const auto* const a = lhs.pending_save_view();
     const auto* const b = rhs.pending_save_view();
     if (a == nullptr || b == nullptr
             || a->kind != b->kind
             || a->transition != b->transition
             || a->direction != b->direction
             || a->resume_phase != b->resume_phase
             || a->pickup_ordinal != b->pickup_ordinal
             || a->expected_generation != b->expected_generation
+            || !same_pending_health_potion_claim(
+                a->health_potion_claim, b->health_potion_claim)
             || !arpg::dungeon::same_run_state(a->next_state, b->next_state)) {
         return false;
     }
     lhs.resolve_pending_save({
         arpg::dungeon::SaveDisposition::committed,
         a->expected_generation,
         a->next_state,
     });
     rhs.resolve_pending_save({
         arpg::dungeon::SaveDisposition::committed,
@@ -1344,20 +1362,71 @@ arpg::test::Failure ten_thousand_director_plans_are_legal_deterministic_and_allo
     return {};
 }
 
 arpg::test::Failure identical_seed_and_route_are_field_equal() noexcept {
     arpg::dungeon::DungeonSessionConfig config;
     config.root_seed = 0x1020304050607080ULL;
     auto lhs = std::make_unique<DungeonSession>(config);
     auto rhs = std::make_unique<DungeonSession>(config);
     ARPG_REQUIRE(same_snapshot(lhs->snapshot(), rhs->snapshot()));
 
+    auto snapshot_lhs = std::make_unique<DungeonSnapshot>(lhs->snapshot());
+    auto snapshot_rhs = std::make_unique<DungeonSnapshot>(rhs->snapshot());
+    snapshot_lhs->pending_health_potion_spawn_ordinal =
+        static_cast<std::uint16_t>(6U);
+    snapshot_rhs->pending_health_potion_spawn_ordinal =
+        static_cast<std::uint16_t>(7U);
+    ARPG_REQUIRE(!same_snapshot(*snapshot_lhs, *snapshot_rhs));
+
+    using PendingHealthPotionClaim =
+        arpg::dungeon::PendingHealthPotionClaim;
+    const std::optional<PendingHealthPotionClaim> no_claim;
+    const std::optional<PendingHealthPotionClaim> claim{
+        PendingHealthPotionClaim{{{2U, 5U, 8U, 13U}}, 4U, 250, 1000}};
+    ARPG_REQUIRE(!same_pending_health_potion_claim(no_claim, claim));
+    ARPG_REQUIRE(!same_pending_health_potion_claim(claim, no_claim));
+
+    auto changed_claim = claim;
+    changed_claim->count = 3U;
+    ARPG_REQUIRE(!same_pending_health_potion_claim(claim, changed_claim));
+    for (std::size_t index = 0U;
+            index < claim->spawn_ordinals.size(); ++index) {
+        changed_claim = claim;
+        ++changed_claim->spawn_ordinals[index];
+        ARPG_REQUIRE(!same_pending_health_potion_claim(claim, changed_claim));
+    }
+    changed_claim = claim;
+    ++changed_claim->expected_hp;
+    ARPG_REQUIRE(!same_pending_health_potion_claim(claim, changed_claim));
+    changed_claim = claim;
+    ++changed_claim->expected_max_hp;
+    ARPG_REQUIRE(!same_pending_health_potion_claim(claim, changed_claim));
+
+    auto pending_lhs = std::make_unique<DungeonSession>(config);
+    auto pending_rhs = std::make_unique<DungeonSession>(config);
+    arpg::test::set_phase(*pending_lhs, RoomPhase::awaiting_exit);
+    arpg::test::set_phase(*pending_rhs, RoomPhase::awaiting_exit);
+    arpg::test::set_player_health(*pending_lhs, 500, 1000);
+    arpg::test::set_player_health(*pending_rhs, 500, 1000);
+    arpg::test::install_ground_health_potion(
+        *pending_lhs, 6U, {0.0F, 0.0F, 0.0F});
+    arpg::test::install_ground_health_potion(
+        *pending_rhs, 6U, {0.0F, 0.0F, 0.0F});
+    pending_lhs->tick({});
+    pending_rhs->tick({});
+    ARPG_REQUIRE(pending_lhs->pending_save_view() != nullptr);
+    ARPG_REQUIRE(pending_rhs->pending_save_view() != nullptr);
+    arpg::test::set_pending_health_potion_claim_count(*pending_rhs, 2U);
+    ARPG_REQUIRE(!commit_equal(*pending_lhs, *pending_rhs));
+    ARPG_REQUIRE(pending_lhs->pending_save_view() != nullptr);
+    ARPG_REQUIRE(pending_rhs->pending_save_view() != nullptr);
+
     const auto player = lhs->snapshot().combat->player.position;
     const auto normal = arpg::items::generate_item({
         0xCC0101U,
         arpg::items::ItemSlot::gloves,
         20U,
         0xCC0101U,
         arpg::items::ItemRarity::normal,
     });
     const auto rare = arpg::items::generate_item({
         0xCC0303U,
