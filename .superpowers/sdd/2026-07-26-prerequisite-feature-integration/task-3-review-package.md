# Review package: 39bc3b125cd96984d431375b56e17797a2988863..17cbb0a01f4a4e039574e689986fb0a5256c42ed

## Commits
17cbb0a balance: suppress monster affixes through depth three
88f8a3b balance: sharply reduce monster base stats

## Files changed
 src/combat/monster_affix_generation.cpp            |  3 +-
 src/combat/monster_catalog.cpp                     | 30 +++++++++----------
 tests/combat/break_stress_tests.cpp                | 10 +++----
 tests/combat/combat_test_main.cpp                  |  2 +-
 tests/combat/monster_affix_generation_tests.cpp    | 33 ++++++++++++++++++--
 tests/combat/monster_affix_runtime_tests.cpp       | 14 ++++-----
 tests/combat/monster_affix_trigger_tests.cpp       |  8 ++---
 tests/combat/monster_catalog_tests.cpp             | 35 ++++++++++++++++++++++
 tests/combat/monster_melee_tests.cpp               | 28 ++++++++---------
 tests/combat/player_build_tests.cpp                |  4 +--
 tests/combat/player_death_snapshot_tests.cpp       | 14 +++++++--
 tests/dungeon/dungeon_abyss_stress_tests.cpp       |  2 +-
 .../stage16_loot_reinforcement_simulation.cpp      |  2 +-
 13 files changed, 129 insertions(+), 56 deletions(-)

## Diff
diff --git a/src/combat/monster_affix_generation.cpp b/src/combat/monster_affix_generation.cpp
index 34f81cc..041ac42 100644
--- a/src/combat/monster_affix_generation.cpp
+++ b/src/combat/monster_affix_generation.cpp
@@ -37,21 +37,21 @@ supplement_abyss_affixes_with_catalog(
 }  // namespace detail
 namespace {
 
 struct AffixDepthBand final {
     std::uint64_t maximum_depth{};
     std::array<std::uint16_t, 4> count_weights{};
     std::array<std::uint16_t, 3> tier_weights{};
 };
 
 constexpr std::array<AffixDepthBand, 5> kAffixDepthBands{{
-    {3U, {{80U, 20U, 0U, 0U}}, {{100U, 0U, 0U}}},
+    {3U, {{100U, 0U, 0U, 0U}}, {{100U, 0U, 0U}}},
     {9U, {{55U, 38U, 7U, 0U}}, {{80U, 20U, 0U}}},
     {19U, {{30U, 45U, 20U, 5U}}, {{50U, 40U, 10U}}},
     {39U, {{15U, 35U, 35U, 15U}}, {{25U, 50U, 25U}}},
     {(std::numeric_limits<std::uint64_t>::max)(),
         {{5U, 20U, 40U, 35U}}, {{10U, 35U, 55U}}},
 }};
 
 constexpr std::uint64_t kAffixCountDomain = 0x41464658434E5431ULL;
 constexpr std::uint64_t kAffixSelectionDomain = 0x4146465853454C31ULL;
 constexpr std::uint64_t kAffixTierDomain = 0x4146465854494552ULL;
@@ -309,20 +309,21 @@ std::optional<MonsterAffixSet> supplement_abyss_affixes_with_catalog(
     std::uint8_t spawn_index,
     const MonsterDefinition& monster,
     MonsterAffixSet normal,
     const MonsterAffixCatalog& catalog) noexcept {
     if (!detail::monster_affix_catalog_valid(catalog)) return std::nullopt;
     const MonsterDefinition* const canonical = monster_definition(monster.id);
     if (canonical == nullptr || canonical->tags != monster.tags
             || !affix_set_valid_for_monster(normal, monster, catalog)) {
         return std::nullopt;
     }
+    if (depth <= 3U) return MonsterAffixSet{};
     const std::uint8_t target_count = abyss::minimum_abyss_affixes(depth);
     if (normal.count >= target_count) return normal;
 
     const std::uint64_t seed = monster_affix_context_seed(room_seed, depth,
         wave_index, spawn_index);
     const auto tier_weights = affix_tier_weights(depth);
     for (std::size_t output_index = normal.count;
          output_index < target_count; ++output_index) {
         std::array<Candidate, static_cast<std::size_t>(MonsterAffixId::count)>
             candidates{};
diff --git a/src/combat/monster_catalog.cpp b/src/combat/monster_catalog.cpp
index b28c25d..7d0eea4 100644
--- a/src/combat/monster_catalog.cpp
+++ b/src/combat/monster_catalog.cpp
@@ -34,54 +34,54 @@ constexpr std::uint16_t tags(
 
 constexpr DamagePacket elemental_damage(
     modifiers::DamageType type, int amount) noexcept {
     DamagePacket packet{};
     packet.amount[modifiers::damage_index(type)] = amount;
     return packet;
 }
 
 constexpr std::array<MonsterDefinition, 8> kCatalog{{
     {MonsterId::fire_bomber, 0, tags(MonsterTag::high_priority,
-         MonsterTag::direct_target), 3, 220, 0, 0.040F, 1.2F,
-     60, 1, 0, 0, elemental_damage(modifiers::DamageType::fire, 120),
+         MonsterTag::direct_target), 3, 100, 0, 0.040F, 1.2F,
+     60, 1, 0, 0, elemental_damage(modifiers::DamageType::fire, 36),
      0.0F, 0, FeedbackLevel::heavy},
     {MonsterId::fire_charger, 0, tags(MonsterTag::melee,
          MonsterTag::high_priority, MonsterTag::direct_target),
-     4, 420, 60, 0.030F, 5.0F, 45, 18, 35, 100,
-     elemental_damage(modifiers::DamageType::fire, 90), 0.0F, 0,
+     4, 190, 60, 0.030F, 5.0F, 45, 18, 35, 100,
+     elemental_damage(modifiers::DamageType::fire, 27), 0.0F, 0,
      FeedbackLevel::heavy},
     {MonsterId::water_bulwark, 1, tags(MonsterTag::melee,
-         MonsterTag::direct_target), 4, 700, 120, 0.025F, 1.0F,
-     24, 6, 30, 72, elemental_damage(modifiers::DamageType::water, 70),
+         MonsterTag::direct_target), 4, 315, 120, 0.025F, 1.0F,
+     24, 6, 30, 72, elemental_damage(modifiers::DamageType::water, 21),
      0.0F, 0, FeedbackLevel::heavy},
     {MonsterId::water_support, 1, tag(MonsterTag::support),
-     3, 300, 0, 0.030F, 4.0F, 36, 1, 24, 120,
+     3, 135, 0, 0.030F, 4.0F, 36, 1, 24, 120,
      elemental_damage(modifiers::DamageType::water, 0), 0.0F, 0,
      FeedbackLevel::medium, 90, 120},
     {MonsterId::lightning_shooter, 2, tags(MonsterTag::ranged,
-         MonsterTag::direct_target, MonsterTag::projectile_capable), 3, 240, 0, 0.035F, 4.5F,
+         MonsterTag::direct_target, MonsterTag::projectile_capable), 3, 110, 0, 0.035F, 4.5F,
      30, 1, 20, 75,
-     elemental_damage(modifiers::DamageType::lightning, 40), 0.14F, 0,
+     elemental_damage(modifiers::DamageType::lightning, 12), 0.14F, 0,
      FeedbackLevel::medium},
     {MonsterId::lightning_dasher, 2, tags(MonsterTag::melee,
          MonsterTag::high_priority, MonsterTag::direct_target),
-     3, 280, 0, 0.050F, 3.0F, 24, 12, 30, 80,
-     elemental_damage(modifiers::DamageType::lightning, 60), 0.0F, 0,
+     3, 125, 0, 0.050F, 3.0F, 24, 12, 30, 80,
+     elemental_damage(modifiers::DamageType::lightning, 18), 0.0F, 0,
      FeedbackLevel::medium},
     {MonsterId::chaos_chaser, 3, tags(MonsterTag::melee,
-         MonsterTag::direct_target), 2, 260, 0, 0.045F, 0.9F,
-     12, 4, 18, 42, elemental_damage(modifiers::DamageType::chaos, 45),
+         MonsterTag::direct_target), 2, 120, 0, 0.045F, 0.9F,
+     12, 4, 18, 42, elemental_damage(modifiers::DamageType::chaos, 14),
      0.0F, 0, FeedbackLevel::light},
     {MonsterId::chaos_hazard, 3, tags(MonsterTag::ranged,
          MonsterTag::high_priority, MonsterTag::ground_hazard,
-         MonsterTag::direct_target), 4, 320, 0, 0.030F, 4.0F,
-     45, 1, 25, 120, elemental_damage(modifiers::DamageType::chaos, 35),
+         MonsterTag::direct_target), 4, 145, 0, 0.030F, 4.0F,
+     45, 1, 25, 120, elemental_damage(modifiers::DamageType::chaos, 11),
      0.0F, 180, FeedbackLevel::heavy},
 }};
 
 constexpr bool validate_catalog() noexcept {
     bool has_direct_target = false;
     for (std::size_t index = 0; index < kCatalog.size(); ++index) {
         const auto& definition = kCatalog[index];
         if (static_cast<std::size_t>(definition.id) != index ||
             definition.preferred_ecology > 3U ||
             definition.threat_cost == 0U || definition.max_hp <= 0) {
diff --git a/tests/combat/break_stress_tests.cpp b/tests/combat/break_stress_tests.cpp
index 70c31ad..8c60608 100644
--- a/tests/combat/break_stress_tests.cpp
+++ b/tests/combat/break_stress_tests.cpp
@@ -748,51 +748,51 @@ arpg::test::Failure mixed_role_golden_replay_preserves_public_behavior() noexcep
     ARPG_REQUIRE(early_events[0].kind == CombatEventKind::swing);
     ARPG_REQUIRE(early_events[0].tick == 0U);
     ARPG_REQUIRE(early_events[1].kind == CombatEventKind::hit);
     ARPG_REQUIRE(early_events[1].tick == 5U);
     ARPG_REQUIRE(early_events[1].target_index == 1U);
     ARPG_REQUIRE(early_events[1].value == 28);
     ARPG_REQUIRE(early_events[2].kind == CombatEventKind::impact_summary);
     ARPG_REQUIRE(early_events[2].tick == 5U);
     ARPG_REQUIRE(early_events[3].kind == CombatEventKind::player_hit);
     ARPG_REQUIRE(early_events[3].tick == 39U);
-    ARPG_REQUIRE(early_events[3].value == 45);
+    ARPG_REQUIRE(early_events[3].value == 14);
     ARPG_REQUIRE(early_events[4].kind == CombatEventKind::player_hurt_started);
     ARPG_REQUIRE(early_events[4].tick == 39U);
-    ARPG_REQUIRE(early_events[4].value == 45);
+    ARPG_REQUIRE(early_events[4].value == 14);
 
     ARPG_REQUIRE(tick_45.tick == 46U);
-    ARPG_REQUIRE(tick_45.player.hp == 955);
+    ARPG_REQUIRE(tick_45.player.hp == 986);
     ARPG_REQUIRE(tick_45.monster_count == 5U);
     ARPG_REQUIRE(tick_45.monsters[0].id == MonsterId::water_bulwark);
     ARPG_REQUIRE(tick_45.monsters[0].ai_phase == MonsterAiPhase::telegraph);
     ARPG_REQUIRE(tick_45.monsters[0].shield == 90);
     ARPG_REQUIRE(tick_45.monsters[0].shield_ticks == 119U);
     ARPG_REQUIRE(tick_45.monsters[1].id == MonsterId::chaos_chaser);
-    ARPG_REQUIRE(tick_45.monsters[1].hp == 232);
+    ARPG_REQUIRE(tick_45.monsters[1].hp == 92);
     ARPG_REQUIRE(tick_45.monsters[1].ai_phase == MonsterAiPhase::recovery);
     ARPG_REQUIRE(tick_45.monsters[2].id == MonsterId::lightning_shooter);
     ARPG_REQUIRE(tick_45.monsters[2].ai_phase == MonsterAiPhase::recovery);
     ARPG_REQUIRE(tick_45.monsters[3].id == MonsterId::water_support);
     ARPG_REQUIRE(tick_45.monsters[3].ai_phase == MonsterAiPhase::recovery);
     ARPG_REQUIRE(tick_45.monsters[4].id == MonsterId::chaos_hazard);
     ARPG_REQUIRE(tick_45.monsters[4].ai_phase == MonsterAiPhase::telegraph);
     ARPG_REQUIRE(tick_45.hazard_count == 1U);
     ARPG_REQUIRE(tick_45.hazards[0].active);
     ARPG_REQUIRE(tick_45.hazards[0].owner.index == 4U);
     ARPG_REQUIRE(tick_45.diagnostics.effect_owner_count == 1U);
     ARPG_REQUIRE(tick_45.diagnostics.active_effect_count == 1U);
     ARPG_REQUIRE(tick_45.diagnostics.effect_overflow_count == 0U);
     ARPG_REQUIRE(tick_45.diagnostics.effect_command_overflow_count == 0U);
 
     ARPG_REQUIRE(tick_180.tick == 181U);
-    ARPG_REQUIRE(tick_180.player.hp == 845);
+    ARPG_REQUIRE(tick_180.player.hp == 952);
     ARPG_REQUIRE(tick_180.monsters[0].shield == 0);
     ARPG_REQUIRE(tick_180.monsters[0].shield_ticks == 0U);
     ARPG_REQUIRE(tick_180.hazard_count == 1U);
     ARPG_REQUIRE(tick_180.diagnostics.effect_owner_count == 1U);
     ARPG_REQUIRE(tick_180.diagnostics.active_effect_count == 0U);
     ARPG_REQUIRE(tick_180.diagnostics.effect_overflow_count == 0U);
     ARPG_REQUIRE(tick_180.diagnostics.effect_command_overflow_count == 0U);
     ARPG_REQUIRE(tick_180.diagnostics.event_overflow_count == 0U);
     return {};
 }
diff --git a/tests/combat/combat_test_main.cpp b/tests/combat/combat_test_main.cpp
index 4e78045..4aad3f8 100644
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
 
-    return arpg::test::run_suites(suites, 241, "stage 18 combat queries");
+    return arpg::test::run_suites(suites, 243, "stage 18 combat queries");
 }
diff --git a/tests/combat/monster_affix_generation_tests.cpp b/tests/combat/monster_affix_generation_tests.cpp
index cfce6c2..dd25ee8 100644
--- a/tests/combat/monster_affix_generation_tests.cpp
+++ b/tests/combat/monster_affix_generation_tests.cpp
@@ -202,21 +202,21 @@ bool affixes_are_unique(const MonsterAffixSet& set) noexcept {
 
 arpg::test::Failure abyss_supplement_enforces_depth_minimums_and_preserves_prefix() noexcept {
     const MonsterDefinition* const monster =
         monster_definition(MonsterId::lightning_shooter);
     ARPG_REQUIRE(monster != nullptr);
     struct Case final {
         std::uint64_t depth{};
         std::uint8_t minimum{};
     };
     constexpr std::array<Case, 6> kCases{{
-        {1U, 1U}, {19U, 1U}, {20U, 2U},
+        {4U, 1U}, {19U, 1U}, {20U, 2U},
         {39U, 2U}, {40U, 3U}, {(std::numeric_limits<std::uint64_t>::max)(), 3U},
     }};
 
     for (const Case value : kCases) {
         for (std::uint64_t seed = 0U; seed < 256U; ++seed) {
             const auto normal = generate_monster_affixes(seed, value.depth,
                 1U, 7U, *monster);
             ARPG_REQUIRE(normal.has_value());
             const auto supplemented = supplement_abyss_affixes(seed,
                 value.depth, 1U, 7U, *monster, *normal);
@@ -285,23 +285,50 @@ arpg::test::Failure abyss_supplement_allocates_nothing() noexcept {
     for (std::uint64_t seed = 0U; seed < 4096U; ++seed) {
         const auto result = supplement_abyss_affixes(seed, 40U, 1U, 7U,
             *monster, MonsterAffixSet{});
         ARPG_REQUIRE(result.has_value());
         ARPG_REQUIRE(result->count == 3U);
     }
     ARPG_REQUIRE(arpg::test::allocation_count() == before);
     return {};
 }
 
+arpg::test::Failure first_three_depths_are_affix_free_in_normal_and_abyss_rooms() noexcept {
+    MonsterAffixSet nonempty{};
+    nonempty.values[0] = {MonsterAffixId::mighty, MonsterAffixTier::m1};
+    nonempty.count = 1U;
+    for (std::uint64_t depth = 1U; depth <= 3U; ++depth) {
+        for (std::uint8_t raw = 0U;
+             raw < static_cast<std::uint8_t>(MonsterId::count); ++raw) {
+            const MonsterDefinition* monster = monster_definition(static_cast<MonsterId>(raw));
+            ARPG_REQUIRE(monster != nullptr);
+            for (std::uint64_t seed = 0U; seed < 256U; ++seed) {
+                const auto normal = generate_monster_affixes(seed, depth, 1U, raw, *monster);
+                ARPG_REQUIRE(normal.has_value());
+                ARPG_REQUIRE(normal->count == 0U);
+                const auto abyss = supplement_abyss_affixes(seed, depth, 1U, raw,
+                    *monster, *normal);
+                ARPG_REQUIRE(abyss.has_value());
+                ARPG_REQUIRE(abyss->count == 0U);
+            }
+            const auto cleared = supplement_abyss_affixes(7U, depth, 0U, raw,
+                *monster, nonempty);
+            ARPG_REQUIRE(cleared.has_value());
+            ARPG_REQUIRE(cleared->count == 0U);
+        }
+    }
+    return {};
+}
+
 arpg::test::Failure depth_bands_match_frozen_weights() noexcept {
     ARPG_REQUIRE((affix_count_weights(3U)
-        == std::array<std::uint16_t, 4>{{80U, 20U, 0U, 0U}}));
+        == std::array<std::uint16_t, 4>{{100U, 0U, 0U, 0U}}));
     ARPG_REQUIRE((affix_count_weights(4U)
         == std::array<std::uint16_t, 4>{{55U, 38U, 7U, 0U}}));
     ARPG_REQUIRE((affix_count_weights(9U)
         == std::array<std::uint16_t, 4>{{55U, 38U, 7U, 0U}}));
     ARPG_REQUIRE((affix_count_weights(10U)
         == std::array<std::uint16_t, 4>{{30U, 45U, 20U, 5U}}));
     ARPG_REQUIRE((affix_count_weights(19U)
         == std::array<std::uint16_t, 4>{{30U, 45U, 20U, 5U}}));
     ARPG_REQUIRE((affix_count_weights(20U)
         == std::array<std::uint16_t, 4>{{15U, 35U, 35U, 15U}}));
@@ -521,20 +548,22 @@ arpg::test::Failure invalid_monster_definition_fails_explicitly() noexcept {
 }
 
 constexpr arpg::test::TestCase kCases[] = {
     {"abyss supplement depth minimums and prefix",
         &abyss_supplement_enforces_depth_minimums_and_preserves_prefix},
     {"abyss supplement stable and satisfied bytes unchanged",
         &abyss_supplement_is_stable_and_leaves_satisfied_bytes_unchanged},
     {"abyss supplement insufficient candidates fail",
         &abyss_supplement_fails_when_compatible_candidates_run_out},
     {"abyss supplement no allocations", &abyss_supplement_allocates_nothing},
+    {"first three depths are affix free in normal and abyss rooms",
+        &first_three_depths_are_affix_free_in_normal_and_abyss_rooms},
     {"frozen depth bands", &depth_bands_match_frozen_weights},
     {"deterministic unique generation and score",
         &generation_is_deterministic_unique_and_scored},
     {"applicability and three high risk", &applicability_rules_and_three_high_risk_are_preserved},
     {"malformed catalogs fail explicitly", &malformed_catalogs_are_rejected_explicitly},
     {"malformed catalog fails before zero affix sampling",
         &malformed_catalog_fails_before_zero_affix_sampling},
     {"insufficient candidates fail explicitly",
         &insufficient_candidates_fail_explicitly},
     {"context has no depth wave or spawn alias",
diff --git a/tests/combat/monster_affix_runtime_tests.cpp b/tests/combat/monster_affix_runtime_tests.cpp
index 16d1732..e41f1e8 100644
--- a/tests/combat/monster_affix_runtime_tests.cpp
+++ b/tests/combat/monster_affix_runtime_tests.cpp
@@ -265,85 +265,85 @@ arpg::test::Failure shielding_recharges_once_after_tier_delay_and_hit_resets_tim
     return {};
 }
 
 arpg::test::Failure frozen_catalog_projects_static_affix_values() noexcept {
     const MonsterDefinition* const bulwark =
         monster_definition(MonsterId::water_bulwark);
     ARPG_REQUIRE(bulwark != nullptr);
 
     const MonsterAffixProfile mighty = evaluate_monster_affixes(
         *bulwark, one_affix(MonsterAffixId::mighty, MonsterAffixTier::m3));
-    ARPG_REQUIRE(mighty.max_hp == 1400);
+    ARPG_REQUIRE(mighty.max_hp == 630);
     ARPG_REQUIRE(mighty.horizontal_impulse_bp == 5500);
 
     const MonsterAffixProfile frenzy = evaluate_monster_affixes(
         *bulwark, one_affix(MonsterAffixId::frenzy, MonsterAffixTier::m3));
     ARPG_REQUIRE(frenzy.damage_bp == 15000);
     ARPG_REQUIRE(frenzy.attack_timing_bp == 7000);
 
     const MonsterAffixProfile swift = evaluate_monster_affixes(
         *bulwark, one_affix(MonsterAffixId::swift, MonsterAffixTier::m3));
     ARPG_REQUIRE(swift.move_bp == 14500);
     ARPG_REQUIRE(swift.cooldown_bp == 7600);
 
     const MonsterAffixProfile armored = evaluate_monster_affixes(
         *bulwark, one_affix(MonsterAffixId::armored, MonsterAffixTier::m3));
     ARPG_REQUIRE(armored.armor_rating == 775);
 
     const MonsterAffixProfile shielding = evaluate_monster_affixes(
         *bulwark, one_affix(MonsterAffixId::shielding, MonsterAffixTier::m2));
-    ARPG_REQUIRE(shielding.max_shield == 245);
+    ARPG_REQUIRE(shielding.max_shield == 110);
     ARPG_REQUIRE(shielding.shield_recharge_delay_ticks == 150U);
     return {};
 }
 
 arpg::test::Failure spawn_spec_is_copied_to_runtime_and_snapshot() noexcept {
     MonsterSpawnSpec spec{};
     spec.id = MonsterId::water_bulwark;
     spec.position = Vec3{2.0F, 0.0F, 0.0F};
     spec.affixes = one_affix(MonsterAffixId::mighty, MonsterAffixTier::m3);
     spec.spawn_ordinal = 97U;
 
     MonsterPool pool;
     const auto handle = pool.spawn(spec);
     ARPG_REQUIRE(handle.has_value());
     const MonsterRuntime* const runtime = pool.get(*handle);
     ARPG_REQUIRE(runtime != nullptr);
     ARPG_REQUIRE(runtime->affixes == spec.affixes);
     ARPG_REQUIRE(runtime->spawn_ordinal == 97U);
-    ARPG_REQUIRE(runtime->affix_profile.max_hp == 1400);
+    ARPG_REQUIRE(runtime->affix_profile.max_hp == 630);
     ARPG_REQUIRE(runtime->affix_warning == MonsterAffixWarning::none);
     ARPG_REQUIRE(runtime->affix_warning_ticks == 0U);
 
     EncounterWave wave{};
     wave.spawn_count = 1U;
     wave.spawns[0] = spec;
     CombatEncounterConfig config{};
     config.wave = wave;
     CombatWorld world{config};
     const CombatSnapshot snapshot = world.snapshot();
     ARPG_REQUIRE(snapshot.monster_count == 1U);
     ARPG_REQUIRE(snapshot.monsters[0].affixes == spec.affixes);
     ARPG_REQUIRE(snapshot.monsters[0].spawn_ordinal == 97U);
     ARPG_REQUIRE(snapshot.monsters[0].affix_warning == MonsterAffixWarning::none);
     ARPG_REQUIRE(snapshot.monsters[0].affix_warning_ticks == 0U);
-    ARPG_REQUIRE(snapshot.monsters[0].max_hp == 1400);
+    ARPG_REQUIRE(snapshot.monsters[0].max_hp == 630);
     return {};
 }
 
 arpg::test::Failure chilling_direct_hit_adds_water_damage_and_slows_movement() noexcept {
     CombatWorld world{affixed_encounter(
         MonsterAffixId::chilling, MonsterAffixTier::m3,
         MonsterId::chaos_chaser, 0.90F)};
     const int maximum_hp = world.snapshot().player.max_hp;
     ARPG_REQUIRE(wait_for_player_hit(world, 30));
-    ARPG_REQUIRE(world.snapshot().player.hp == maximum_hp - 61);
+    ARPG_REQUIRE(world.snapshot().player.hp == maximum_hp - 19);
     ARPG_REQUIRE(world.snapshot().player.slow_bp == 3500);
     ARPG_REQUIRE(world.snapshot().player.slow_ticks == 120U);
 
     arpg::test::tick_n(world, 30);
     const float start_x = world.snapshot().player.position.x;
     world.tick(MovementInput{1, 0});
     ARPG_REQUIRE(arpg::test::near(
         world.snapshot().player.position.x - start_x, 0.0585F, 1.0e-4F));
 
     CombatWorld refresh{dual_affixed_encounter(
@@ -365,31 +365,31 @@ arpg::test::Failure chilling_direct_hit_adds_water_damage_and_slows_movement() n
     ARPG_REQUIRE(refresh.snapshot().player.slow_ticks == 90U);
     return {};
 }
 
 arpg::test::Failure corrosion_dot_bypasses_evasion_and_uses_chaos_reduction() noexcept {
     CombatWorld world{affixed_encounter(
         MonsterAffixId::chaos_corrosion, MonsterAffixTier::m3,
         MonsterId::chaos_chaser, 0.90F)};
     const int maximum_hp = world.snapshot().player.max_hp;
     ARPG_REQUIRE(wait_for_player_hit(world, 30));
-    ARPG_REQUIRE(world.snapshot().player.hp == maximum_hp - 45);
+    ARPG_REQUIRE(world.snapshot().player.hp == maximum_hp - 14);
     ARPG_REQUIRE(world.snapshot().player.corrosion_damage_per_second == 45);
     ARPG_REQUIRE(world.snapshot().player.corrosion_ticks == 240U);
 
     PlayerCombatBuild protected_build{};
     protected_build.values.evasion = 1000000;
     protected_build.values.damage_reduction[
         arpg::modifiers::element_index(arpg::modifiers::DamageType::chaos)] = 5000;
     world.apply_player_build(protected_build);
     arpg::test::tick_n(world, 60);
-    ARPG_REQUIRE(world.snapshot().player.hp == maximum_hp - 68);
+    ARPG_REQUIRE(world.snapshot().player.hp == maximum_hp - 37);
 
     CombatWorld refresh{dual_affixed_encounter(
         MonsterAffixId::chaos_corrosion, MonsterAffixTier::m1,
         MonsterAffixTier::m3)};
     arpg::test::CombatWorldTestAccess::apply_monster_direct_hit(
         refresh, 0U, DamagePacket{45}, Vec3{}, FeedbackLevel::light);
     ARPG_REQUIRE(refresh.snapshot().player.corrosion_damage_per_second == 20);
     ARPG_REQUIRE(refresh.snapshot().player.corrosion_ticks == 120U);
     arpg::test::tick_n(refresh, 30);
     arpg::test::CombatWorldTestAccess::apply_monster_direct_hit(
diff --git a/tests/combat/monster_affix_trigger_tests.cpp b/tests/combat/monster_affix_trigger_tests.cpp
index 722cd02..c3c1334 100644
--- a/tests/combat/monster_affix_trigger_tests.cpp
+++ b/tests/combat/monster_affix_trigger_tests.cpp
@@ -148,23 +148,23 @@ arpg::test::Failure multishot_spawns_four_fanned_projectiles() noexcept {
     CombatWorld world{affixed_encounter(
         MonsterId::lightning_shooter, MonsterAffixId::multishot,
         MonsterAffixTier::m3)};
     for (int tick = 0; tick < 100; ++tick) {
         world.tick(MovementInput{});
         if (world.snapshot().projectile_count != 0U) break;
     }
     const CombatSnapshot snapshot = world.snapshot();
     ARPG_REQUIRE(snapshot.projectile_count == 4U);
     ARPG_REQUIRE(snapshot.projectiles[0].damage.amount[
-        arpg::modifiers::damage_index(arpg::modifiers::DamageType::lightning)] == 20);
+        arpg::modifiers::damage_index(arpg::modifiers::DamageType::lightning)] == 6);
     ARPG_REQUIRE(snapshot.projectiles[1].damage.amount[
-        arpg::modifiers::damage_index(arpg::modifiers::DamageType::lightning)] == 20);
+        arpg::modifiers::damage_index(arpg::modifiers::DamageType::lightning)] == 6);
     ARPG_REQUIRE(snapshot.projectiles[0].velocity.y
                  != snapshot.projectiles[1].velocity.y);
     return {};
 }
 
 arpg::test::Failure burning_ground_spawns_periodic_hazard() noexcept {
     CombatWorld world{affixed_encounter(
         MonsterId::chaos_chaser, MonsterAffixId::burning_ground,
         MonsterAffixTier::m1, Vec3{0.8F, 0.0F, 0.0F})};
     arpg::test::tick_n(world, 180);
@@ -504,30 +504,30 @@ arpg::test::Failure blink_warning_pauses_ai_then_real_hit_consumes_frenzied_empo
     bool actual_hit = false;
     for (int tick = 0; tick < 60; ++tick) {
         world.tick(MovementInput{});
         if (world.snapshot().player.hp < hp_before_warning) {
             actual_hit = true;
             break;
         }
     }
     const CombatSnapshot after_hit = world.snapshot();
     ARPG_REQUIRE(actual_hit);
-    ARPG_REQUIRE(after_hit.player.hp == hp_before_warning - 100);
+    ARPG_REQUIRE(after_hit.player.hp == hp_before_warning - 31);
     ARPG_REQUIRE(!after_hit.monsters[0].blink_empowered);
     return {};
 }
 
 arpg::test::Failure tiered_multishot_and_burning_values_are_frozen() noexcept {
     constexpr MonsterAffixTier kTiers[] = {
         MonsterAffixTier::m1, MonsterAffixTier::m2, MonsterAffixTier::m3};
     constexpr std::size_t kProjectileCounts[] = {2U, 3U, 4U};
-    constexpr int kProjectileDamage[] = {30, 24, 20};
+    constexpr int kProjectileDamage[] = {9, 7, 6};
     constexpr int kBurnIntervals[] = {180, 150, 120};
     constexpr float kBurnRadii[] = {0.75F, 0.90F, 1.05F};
     constexpr int kBurnDamage[] = {25, 35, 45};
     for (std::size_t index = 0U; index < 3U; ++index) {
         CombatWorld multishot_world{affixed_encounter(
             MonsterId::lightning_shooter, MonsterAffixId::multishot,
             kTiers[index])};
         for (int tick = 0; tick < 100; ++tick) {
             multishot_world.tick(MovementInput{});
             if (multishot_world.snapshot().projectile_count != 0U) break;
diff --git a/tests/combat/monster_catalog_tests.cpp b/tests/combat/monster_catalog_tests.cpp
index bab9df1..76cde70 100644
--- a/tests/combat/monster_catalog_tests.cpp
+++ b/tests/combat/monster_catalog_tests.cpp
@@ -33,21 +33,56 @@ arpg::test::Failure catalog_has_two_preferred_roles_per_ecology() noexcept {
             monster_definition(static_cast<MonsterId>(raw));
         ARPG_REQUIRE(definition->preferred_ecology < counts.size());
         ++counts[definition->preferred_ecology];
         ARPG_REQUIRE(definition->threat_cost > 0U);
         ARPG_REQUIRE(definition->max_hp > 0);
     }
     ARPG_REQUIRE((counts == std::array<std::uint8_t, 4>{{2, 2, 2, 2}}));
     return {};
 }
 
+arpg::test::Failure catalog_matches_approved_health_and_damage_table() noexcept {
+    struct Expected final {
+        MonsterId id;
+        int max_hp;
+        arpg::modifiers::DamageType damage_type;
+        int damage;
+    };
+    constexpr std::array<Expected, 8U> kExpected{{
+        {MonsterId::fire_bomber, 100, arpg::modifiers::DamageType::fire, 36},
+        {MonsterId::fire_charger, 190, arpg::modifiers::DamageType::fire, 27},
+        {MonsterId::water_bulwark, 315, arpg::modifiers::DamageType::water, 21},
+        {MonsterId::water_support, 135, arpg::modifiers::DamageType::water, 0},
+        {MonsterId::lightning_shooter, 110,
+         arpg::modifiers::DamageType::lightning, 12},
+        {MonsterId::lightning_dasher, 125,
+         arpg::modifiers::DamageType::lightning, 18},
+        {MonsterId::chaos_chaser, 120, arpg::modifiers::DamageType::chaos, 14},
+        {MonsterId::chaos_hazard, 145, arpg::modifiers::DamageType::chaos, 11},
+    }};
+    for (const Expected expected : kExpected) {
+        const MonsterDefinition* definition = monster_definition(expected.id);
+        ARPG_REQUIRE(definition != nullptr);
+        ARPG_REQUIRE(definition->max_hp == expected.max_hp);
+        for (std::size_t index = 0U;
+             index < definition->contact_damage.amount.size(); ++index) {
+            const int amount = index == arpg::modifiers::damage_index(expected.damage_type)
+                ? expected.damage : 0;
+            ARPG_REQUIRE(definition->contact_damage.amount[index] == amount);
+        }
+    }
+    return {};
+}
+
 constexpr arpg::test::TestCase kCases[] = {
     {"eight unique stable ids", &catalog_contains_eight_unique_stable_ids},
     {"two preferred roles per ecology",
      &catalog_has_two_preferred_roles_per_ecology},
+    {"catalog matches approved health and damage table",
+     &catalog_matches_approved_health_and_damage_table},
 };
 
 }  // namespace
 
 arpg::test::TestSuite monster_catalog_suite() noexcept {
     return arpg::test::make_suite("monster_catalog", kCases);
 }
diff --git a/tests/combat/monster_melee_tests.cpp b/tests/combat/monster_melee_tests.cpp
index 45ddd90..c1c84e8 100644
--- a/tests/combat/monster_melee_tests.cpp
+++ b/tests/combat/monster_melee_tests.cpp
@@ -294,82 +294,82 @@ arpg::test::Failure abyss_bulwark_adds_to_stage9_profile() noexcept {
     CombatEncounterConfig config = encounter_for(
         MonsterId::chaos_chaser, 2.0F);
     config.abyss = arpg::abyss::combat_config_for(
         arpg::abyss::AbyssRuleId::abyss_bulwark);
     config.wave.spawns[0].affixes = one_affix(
         MonsterAffixId::armored, MonsterAffixTier::m3);
     CombatWorld armored{config};
     const MonsterAffixProfile armored_profile =
         arpg::test::CombatWorldTestAccess::monster_affix_profile(armored, 0U);
     ARPG_REQUIRE(armored_profile.armor_rating == 1007);
-    ARPG_REQUIRE(armored.snapshot().monsters[0].max_shield == 168);
-    ARPG_REQUIRE(armored.snapshot().monsters[0].shield == 78);
+    ARPG_REQUIRE(armored.snapshot().monsters[0].max_shield == 126);
+    ARPG_REQUIRE(armored.snapshot().monsters[0].shield == 36);
 
     config.wave.spawns[0].affixes = one_affix(
         MonsterAffixId::mighty, MonsterAffixTier::m3);
     CombatWorld mighty{config};
-    ARPG_REQUIRE(mighty.snapshot().monsters[0].max_hp == 520);
-    ARPG_REQUIRE(mighty.snapshot().monsters[0].max_shield == 246);
-    ARPG_REQUIRE(mighty.snapshot().monsters[0].shield == 156);
+    ARPG_REQUIRE(mighty.snapshot().monsters[0].max_hp == 240);
+    ARPG_REQUIRE(mighty.snapshot().monsters[0].max_shield == 162);
+    ARPG_REQUIRE(mighty.snapshot().monsters[0].shield == 72);
 
     config.wave.spawns[0].affixes = one_affix(
         MonsterAffixId::shielding, MonsterAffixTier::m2);
     CombatWorld shielding{config};
-    ARPG_REQUIRE(shielding.snapshot().monsters[0].max_shield == 169);
-    ARPG_REQUIRE(shielding.snapshot().monsters[0].shield == 78);
+    ARPG_REQUIRE(shielding.snapshot().monsters[0].max_shield == 78);
+    ARPG_REQUIRE(shielding.snapshot().monsters[0].shield == 36);
 
     config.wave.spawns[0].affixes = {};
     CombatWorld zero_armor{config};
     ARPG_REQUIRE(arpg::test::CombatWorldTestAccess::monster_affix_profile(
         zero_armor, 0U).armor_rating == 0);
     return {};
 }
 
 arpg::test::Failure abyss_fury_scales_contact_once() noexcept {
     CombatEncounterConfig melee = encounter_for(
         MonsterId::chaos_chaser, 0.0F);
     melee.abyss = arpg::abyss::combat_config_for(
         arpg::abyss::AbyssRuleId::abyss_fury);
     CombatWorld contact{melee};
     arpg::test::CombatWorldTestAccess::arm_monster_active_attack(contact, 0U);
     const int contact_before = contact.snapshot().player.hp;
     arpg::test::CombatWorldTestAccess::simulate_monster(contact, 0U);
-    ARPG_REQUIRE(contact_before - contact.snapshot().player.hp == 65);
+    ARPG_REQUIRE(contact_before - contact.snapshot().player.hp == 20);
     return {};
 }
 
 arpg::test::Failure abyss_fury_scales_projectile_once() noexcept {
     CombatEncounterConfig projectile = encounter_for(
         MonsterId::lightning_shooter, 4.0F);
     projectile.abyss = arpg::abyss::combat_config_for(
         arpg::abyss::AbyssRuleId::abyss_fury);
     CombatWorld shooter{projectile};
     arpg::test::CombatWorldTestAccess::arm_monster_active_attack(shooter, 0U);
     arpg::test::CombatWorldTestAccess::simulate_monster(shooter, 0U);
     ARPG_REQUIRE(shooter.snapshot().projectile_count == 1U);
     ARPG_REQUIRE(shooter.snapshot().projectiles[0].damage.amount[
         arpg::modifiers::damage_index(
-            arpg::modifiers::DamageType::lightning)] == 40);
+            arpg::modifiers::DamageType::lightning)] == 12);
     return {};
 }
 
 arpg::test::Failure abyss_fury_scales_hazard_once() noexcept {
     CombatEncounterConfig hazard = encounter_for(
         MonsterId::chaos_hazard, 0.0F);
     hazard.abyss = arpg::abyss::combat_config_for(
         arpg::abyss::AbyssRuleId::abyss_fury);
     CombatWorld hazard_world{hazard};
     hazard_world.tick({});
     ARPG_REQUIRE(hazard_world.snapshot().hazard_count == 1U);
     ARPG_REQUIRE(hazard_world.snapshot().hazards[0].damage.amount[
         arpg::modifiers::damage_index(
-            arpg::modifiers::DamageType::chaos)] == 50);
+            arpg::modifiers::DamageType::chaos)] == 15);
     return {};
 }
 
 arpg::test::Failure abyss_fury_scales_death_blast_once() noexcept {
     CombatEncounterConfig death = encounter_for(
         MonsterId::chaos_chaser, 2.0F);
     death.abyss = arpg::abyss::combat_config_for(
         arpg::abyss::AbyssRuleId::abyss_fury);
     death.wave.spawns[0].position = Vec3{2.0F, 0.0F, 0.0F};
     death.wave.spawns[0].affixes = one_affix(
@@ -388,58 +388,58 @@ arpg::test::Failure abyss_fury_composes_with_frenzy_once() noexcept {
         MonsterId::chaos_chaser, 0.0F);
     frenzy.abyss = arpg::abyss::combat_config_for(
         arpg::abyss::AbyssRuleId::abyss_fury);
     frenzy.wave.spawns[0].affixes = one_affix(
         MonsterAffixId::frenzy, MonsterAffixTier::m3);
     CombatWorld composed_damage{frenzy};
     arpg::test::CombatWorldTestAccess::arm_monster_active_attack(
         composed_damage, 0U);
     const int composed_before = composed_damage.snapshot().player.hp;
     arpg::test::CombatWorldTestAccess::simulate_monster(composed_damage, 0U);
-    ARPG_REQUIRE(composed_before - composed_damage.snapshot().player.hp == 97);
+    ARPG_REQUIRE(composed_before - composed_damage.snapshot().player.hp == 30);
     ARPG_REQUIRE(has_abyss_phase_timing(frenzy, 7U, 4U, 9U, 21U));
     return {};
 }
 
 arpg::test::Failure abyss_fury_scales_final_chilling_contact_and_projectile() noexcept {
     CombatEncounterConfig contact = encounter_for(
         MonsterId::chaos_chaser, 0.0F);
     contact.abyss = arpg::abyss::combat_config_for(
         arpg::abyss::AbyssRuleId::abyss_fury);
     contact.wave.spawns[0].affixes = two_affixes(
         MonsterAffixId::chilling, MonsterAffixTier::m1,
         MonsterAffixId::blink_assault, MonsterAffixTier::m1);
     CombatWorld contact_world{contact};
     arpg::test::CombatWorldTestAccess::set_blink_empowered(
         contact_world, 0U, true);
     arpg::test::CombatWorldTestAccess::arm_monster_active_attack(
         contact_world, 0U);
     const int contact_before = contact_world.snapshot().player.hp;
     arpg::test::CombatWorldTestAccess::simulate_monster(contact_world, 0U);
-    ARPG_REQUIRE(contact_before - contact_world.snapshot().player.hp == 91);
+    ARPG_REQUIRE(contact_before - contact_world.snapshot().player.hp == 27);
 
     CombatEncounterConfig projectile = encounter_for(
         MonsterId::lightning_shooter, 0.0F);
     projectile.abyss = contact.abyss;
     projectile.wave.spawns[0].affixes = one_affix(
         MonsterAffixId::chilling, MonsterAffixTier::m1);
     CombatWorld projectile_world{projectile};
     arpg::test::CombatWorldTestAccess::arm_monster_active_attack(
         projectile_world, 0U);
     arpg::test::CombatWorldTestAccess::simulate_monster(projectile_world, 0U);
     ARPG_REQUIRE(projectile_world.snapshot().projectile_count == 1U);
     ARPG_REQUIRE(projectile_world.snapshot().projectiles[0].damage.amount[
         arpg::modifiers::damage_index(
-            arpg::modifiers::DamageType::lightning)] == 40);
+            arpg::modifiers::DamageType::lightning)] == 12);
     const int projectile_before = projectile_world.snapshot().player.hp;
     projectile_world.tick({});
-    ARPG_REQUIRE(projectile_before - projectile_world.snapshot().player.hp == 66);
+    ARPG_REQUIRE(projectile_before - projectile_world.snapshot().player.hp == 19);
     return {};
 }
 
 constexpr arpg::test::TestCase kCases[] = {
     {"chaser move and telegraph stop", &chaos_chaser_moves_then_stops_for_telegraph},
     {"chaser active serial cooldown", &chaos_chaser_damages_only_once_per_active_serial},
     {"bulwark slow armored profile", &water_bulwark_is_slow_and_has_front_armor},
     {"bulwark rear bypasses armor", &bulwark_back_hit_bypasses_front_armor},
     {"bulwark break reaction", &bulwark_accepts_normal_reaction_after_break},
     {"launcher minimum airtime", &launcher_gives_stage4_monster_minimum_airtime},
diff --git a/tests/combat/player_build_tests.cpp b/tests/combat/player_build_tests.cpp
index a27bd49..3e99d8e 100644
--- a/tests/combat/player_build_tests.cpp
+++ b/tests/combat/player_build_tests.cpp
@@ -145,21 +145,21 @@ arpg::test::Failure hit_event_keeps_packet_total_when_monster_shield_absorbs() n
     }
 
     bool saw_hit = false;
     while (const auto event = world.try_pop_event()) {
         if (event->kind != CombatEventKind::hit) continue;
         saw_hit = true;
         ARPG_REQUIRE(event->value == 31);
         break;
     }
     ARPG_REQUIRE(saw_hit);
-    ARPG_REQUIRE(world.snapshot().monsters[0].hp == 689);
+    ARPG_REQUIRE(world.snapshot().monsters[0].hp == 304);
     ARPG_REQUIRE(world.snapshot().monsters[0].shield == 0);
     return {};
 }
 
 bool same_player_state(
     const PlayerSnapshot& left, const PlayerSnapshot& right) noexcept {
     return left.position.x == right.position.x
         && left.position.y == right.position.y
         && left.position.z == right.position.z
         && left.velocity.x == right.velocity.x
@@ -385,21 +385,21 @@ arpg::test::Failure weapon_physical_is_applied_by_real_melee_hit() noexcept {
     build.weapon_physical = 50;
     CombatEncounterConfig config{};
     config.player_build = build;
     config.wave.spawn_count = 1U;
     config.wave.spawns[0] = MonsterSpawnSpec{
         MonsterId::chaos_chaser, Vec3{1.0F, 0.0F, 0.0F}};
     CombatWorld world{config};
     arpg::test::drain_events(world);
     ARPG_REQUIRE(world.queue_action(Action::light));
     arpg::test::tick_n(world, 8);
-    ARPG_REQUIRE(world.snapshot().monsters[0].hp == 182);
+    ARPG_REQUIRE(world.snapshot().monsters[0].hp == 42);
     bool saw_weapon_hit = false;
     while (const auto event = world.try_pop_event()) {
         if (event->kind == CombatEventKind::hit) {
             saw_weapon_hit = true;
             ARPG_REQUIRE(event->value == 78);
         }
     }
     ARPG_REQUIRE(saw_weapon_hit);
     return {};
 }
diff --git a/tests/combat/player_death_snapshot_tests.cpp b/tests/combat/player_death_snapshot_tests.cpp
index 8d82b90..3198597 100644
--- a/tests/combat/player_death_snapshot_tests.cpp
+++ b/tests/combat/player_death_snapshot_tests.cpp
@@ -198,23 +198,25 @@ arpg::test::Failure chain_lightning_reports_real_affix_source() noexcept {
                  == PlayerDamageSourceKind::monster_affix);
     ARPG_REQUIRE(world.death_snapshot()->source.monster
                  == MonsterId::chaos_chaser);
     ARPG_REQUIRE(world.death_snapshot()->source.detail_id
                  == static_cast<std::uint16_t>(
                      MonsterAffixId::chain_lightning));
     return {};
 }
 
 arpg::test::Failure death_blast_reports_defeated_owner_source() noexcept {
+    MonsterAffixSet death_blast = one_affix(MonsterAffixId::death_blast);
+    death_blast.values[0].tier = MonsterAffixTier::m2;
     CombatWorld world{one_monster(
         MonsterId::fire_bomber, Vec3{0.8F, 0.0F, 0.0F},
-        one_affix(MonsterAffixId::death_blast), 2000)};
+        death_blast, 2000)};
     ARPG_REQUIRE(tick_until_defeated(world, 180));
     ARPG_REQUIRE(world.death_snapshot()->source.kind
                  == PlayerDamageSourceKind::monster_affix);
     ARPG_REQUIRE(world.death_snapshot()->source.monster
                  == MonsterId::fire_bomber);
     ARPG_REQUIRE(world.death_snapshot()->source.detail_id
                  == static_cast<std::uint16_t>(MonsterAffixId::death_blast));
     return {};
 }
 
@@ -336,32 +338,38 @@ arpg::test::Failure evasion_does_not_enter_recent_damage() noexcept {
         world, DamagePacket{20}, DamageDelivery::ground_or_environment,
         source(PlayerDamageSourceKind::unknown),
         Vec3{}, FeedbackLevel::heavy);
     ARPG_REQUIRE(world.death_snapshot()->recent_damage[kPhysical] == 20U);
     return {};
 }
 
 arpg::test::Failure delayed_corrosion_keeps_the_affix_owner_source() noexcept {
     MonsterAffixSet corrosion{};
     corrosion.values[0] = {
-        MonsterAffixId::chaos_corrosion, MonsterAffixTier::m1};
+        MonsterAffixId::chaos_corrosion, MonsterAffixTier::m2};
     corrosion.count = 1U;
     CombatWorld world{one_monster(
         MonsterId::chaos_chaser, Vec3{0.65F, 0.0F, 0.0F}, corrosion)};
     arpg::test::CombatWorldTestAccess::set_player_resources(world, 65, 0);
+    for (int tick = 0; tick < 120
+         && world.snapshot().player.corrosion_damage_per_second == 0; ++tick) {
+        world.tick(MovementInput{});
+    }
+    ARPG_REQUIRE(world.snapshot().player.corrosion_damage_per_second == 30);
+    arpg::test::CombatWorldTestAccess::freeze_monster_ai(world, 0U, 240U);
     ARPG_REQUIRE(tick_until_defeated(world));
     const auto& death = *world.death_snapshot();
     ARPG_REQUIRE(death.source.kind == PlayerDamageSourceKind::monster_affix);
     ARPG_REQUIRE(death.source.monster == MonsterId::chaos_chaser);
     ARPG_REQUIRE(death.source.detail_id == static_cast<std::uint16_t>(
         MonsterAffixId::chaos_corrosion));
-    ARPG_REQUIRE(death.raw_damage == 20U);
+    ARPG_REQUIRE(death.raw_damage == 30U);
     ARPG_REQUIRE(death.primary_type == arpg::modifiers::DamageType::chaos);
     return {};
 }
 
 arpg::test::Failure barrier_absorption_and_lethal_split_are_exact() noexcept {
     CombatWorld absorbed{CombatEncounterConfig{}};
     arpg::test::CombatWorldTestAccess::set_player_resources(absorbed, 20, 30);
     arpg::test::CombatWorldTestAccess::apply_damage(
         absorbed, DamagePacket{10}, DamageDelivery::direct,
         source(PlayerDamageSourceKind::unknown), Vec3{}, FeedbackLevel::light);
diff --git a/tests/dungeon/dungeon_abyss_stress_tests.cpp b/tests/dungeon/dungeon_abyss_stress_tests.cpp
index db53c0d..2eb3ba8 100644
--- a/tests/dungeon/dungeon_abyss_stress_tests.cpp
+++ b/tests/dungeon/dungeon_abyss_stress_tests.cpp
@@ -526,21 +526,21 @@ arpg::test::Failure thousand_room_abyss_trace_matches_golden_and_reload() noexce
         first->door_hits[3], second->reload_count,
         static_cast<unsigned long long>(first->hash));
 
     ARPG_REQUIRE(total_trials == 4000U);
     ARPG_REQUIRE(total_hits >= 20U && total_hits <= 70U);
     constexpr std::array<std::uint32_t, 4> kGoldenTrials{{
         1000U, 1000U, 1000U, 1000U}};
     constexpr std::array<std::uint32_t, 4> kGoldenHits{{9U, 11U, 9U, 7U}};
     ARPG_REQUIRE(first->door_trials == kGoldenTrials);
     ARPG_REQUIRE(first->door_hits == kGoldenHits);
-    ARPG_REQUIRE(first->hash == 0xe102b17e6423351bULL);
+    ARPG_REQUIRE(first->hash == 0x0d55b41ebb0b1128ULL);
     return {};
 }
 
 arpg::test::Failure extreme_abyss_pools_run_600_ticks_without_allocation() noexcept {
     arpg::dungeon::DungeonSession session{
         arpg::dungeon::DungeonRules{}, extreme_cleared_state()};
     arpg::test::install_combat_world(session, extreme_config());
     arpg::combat::CombatWorld* const world =
         arpg::test::mutable_combat_world(session);
     ARPG_REQUIRE(world != nullptr);
diff --git a/tests/dungeon/stage16_loot_reinforcement_simulation.cpp b/tests/dungeon/stage16_loot_reinforcement_simulation.cpp
index 747a394..340af6d 100644
--- a/tests/dungeon/stage16_loot_reinforcement_simulation.cpp
+++ b/tests/dungeon/stage16_loot_reinforcement_simulation.cpp
@@ -156,21 +156,21 @@ std::optional<std::uint64_t> frozen_existing_stream_fingerprint(
             static_cast<void>(dungeon::roll_material_drop(seed, 17U, 90U, 27U));
             static_cast<void>(dungeon::roll_coupon_drop(seed, 17U, 90U, 27U, false));
             static_cast<void>(dungeon::roll_abyss_material(seed, 90U, 2U));
         }
     }
     return hash;
 }
 
 bool replay_keeps_existing_items_stable() noexcept {
     constexpr std::uint64_t kFrozenTask1RoomEquipmentAndAbyss =
-        15972885342817959559ULL;
+        10882612280735857555ULL;
     const auto baseline = frozen_existing_stream_fingerprint(false);
     const auto interleaved = frozen_existing_stream_fingerprint(true);
     std::cout << "frozen_existing_stream=" << baseline.value_or(0U) << '\n';
     return require(baseline.has_value() && interleaved.has_value()
             && *baseline == *interleaved,
             "material RNG does not perturb Task1 room/equipment/abyss streams")
         && require(*baseline == kFrozenTask1RoomEquipmentAndAbyss,
             "Task1 room/equipment/abyss golden fingerprint");
 }
 
