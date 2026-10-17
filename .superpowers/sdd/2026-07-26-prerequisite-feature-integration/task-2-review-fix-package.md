# Review package: c7a281af8bf56c36b9f52cd92c6255fae3802383..39bc3b125cd96984d431375b56e17797a2988863

## Commits
39bc3b1 test: cover idempotent death recovery initialization

## Files changed
 tests/platform/dungeon_runtime_tests.cpp | 14 ++++++++++++++
 1 file changed, 14 insertions(+)

## Diff
diff --git a/tests/platform/dungeon_runtime_tests.cpp b/tests/platform/dungeon_runtime_tests.cpp
index 4557058..d096d48 100644
--- a/tests/platform/dungeon_runtime_tests.cpp
+++ b/tests/platform/dungeon_runtime_tests.cpp
@@ -1490,20 +1490,34 @@ arpg::test::Failure production_startup_auto_continues_pending_death() noexcept {
 
     dungeon::DungeonRunState expected = dead;
     ++expected.commit_generation;
     expected.current_room = dead.death.target_room;
     expected.abyss = {};
     expected.death = {};
     const auto saved = store.load();
     ARPG_REQUIRE(saved.state == persistence::SaveLoadState::ready);
     ARPG_REQUIRE(dungeon::same_run_state(saved.checkpoint, expected));
 
+    platform::DungeonRuntime relaunched(config);
+    ARPG_REQUIRE(relaunched.initialize());
+    const auto already_clear = relaunched.session()->snapshot();
+    ARPG_REQUIRE(!already_clear.death.has_value());
+    ARPG_REQUIRE(already_clear.commit_generation == expected.commit_generation);
+
+    const auto saved_after_relaunch = store.load();
+    ARPG_REQUIRE(saved_after_relaunch.state
+        == persistence::SaveLoadState::ready);
+    ARPG_REQUIRE(saved_after_relaunch.checkpoint.death.lifecycle
+        == dungeon::checkpoint::DeathLifecycle::none);
+    ARPG_REQUIRE(saved_after_relaunch.checkpoint.commit_generation
+        == expected.commit_generation);
+
     runtime.fixed_tick({});
     const auto playable = runtime.session()->snapshot();
     ARPG_REQUIRE(playable.phase == dungeon::RoomPhase::locked);
     ARPG_REQUIRE(playable.combat.has_value());
     ARPG_REQUIRE(playable.combat->player.hp > 0);
     ARPG_REQUIRE(playable.combat->player.hp
         == playable.combat->player.max_hp);
     return {};
 }
 
