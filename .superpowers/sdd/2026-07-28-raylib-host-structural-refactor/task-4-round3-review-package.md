# Review package: 4127669..94acada

## Commits
94acada test: seal stage11c guard control flow
10b08fb docs: record task 4 third review

## Files changed
 .../progress.md                                    |  19 ++
 src/platform/raylib/host_validation_stage11c.cpp   |  98 ++++---
 .../platform/cmake_source_registration_scan.cmake  | 182 ++++++++++---
 .../host_validation_sequence_guard_self_test.cmake |  36 ++-
 ...stage11c_hud_architecture_guard_self_test.cmake |  22 ++
 .../stage11c_hud_evidence_guard_self_test.cmake    | 117 +++++++-
 .../stage11c_hud_evidence_guard_test.cmake         | 294 ++++++++++++++++-----
 7 files changed, 616 insertions(+), 152 deletions(-)

## Diff
diff --git a/.superpowers/sdd/2026-07-28-raylib-host-structural-refactor/progress.md b/.superpowers/sdd/2026-07-28-raylib-host-structural-refactor/progress.md
index 2c5eb6f..c61803b 100644
--- a/.superpowers/sdd/2026-07-28-raylib-host-structural-refactor/progress.md
+++ b/.superpowers/sdd/2026-07-28-raylib-host-structural-refactor/progress.md
@@ -60,10 +60,29 @@ ordering, brace depth, and full layout RHS remain underconstrained; sequence
 keeps a raw CMake source check and architecture accepts a semicolon-split
 `# decoy;host_validation_stage11c.cpp` registration. FIX_BASE 2486d32.
 Task 4: fix round 2/5 (all findings addressed) — production changes are only
 equivalent braces around two early returns; evidence guard rejects early
 top-level returns, binds code-state runtime seams, and enforces exact RHS plus
 capture scope/order; architecture and sequence share a quote-aware raw-line
 CMake source-registration parser. VS2022 serial four-target build passed in
 196.05s; exact Step3 selector passed 8/8 in 969.00s. Step4 formal intentionally
 not rerun: the same 102-file staged asset and BASE-proven formal baseline remain
 applicable. Fix commit: `test: close stage11c guard bypasses`.
+Task 4: scoped re-review findings (round 3 required) — core guards still count
+returns only at top-level function depth, so nested unconditional returns and a
+late direct hash overwrite can leave required tokens unreachable while passing;
+runtime seam checks preserve relative depth but do not bind marker pairs to the
+absolute executable depth of `run_raylib_host`; the shared CMake scanner does
+not persist bracket-comment state or distinguish bracket-quoted arguments, so
+disabled or quoted source text can be counted as a real registration. FIX_BASE
+4127669. Production behavior and the round-2 build/exact-eight evidence remain
+accepted; only these validation-guard gaps are open.
+Task 4: fix round 3/5 (all findings addressed) — Stage11C driver and summary
+use behavior-equivalent single-exit/nested control flow; guards now enforce
+all-depth return inventories, the executable FNV core and ordered mix chain,
+and one-pass absolute runtime seam depths with compile-valid relocation
+mutations. The shared CMake scanner preserves cross-line quote/comment state
+and rejects arbitrary-equals bracket comments and bracket arguments. VS2022
+x64 serial four-target build passed in 194.54s; the exact eight-test selector
+passed 8/8 in 1118.22s. Step4 formal was intentionally not rerun because the
+unchanged 102-file staged-asset BASE comparison still proves the known formal
+failures are pre-existing. Fix commit: `test: seal stage11c guard control flow`.
diff --git a/src/platform/raylib/host_validation_stage11c.cpp b/src/platform/raylib/host_validation_stage11c.cpp
index 0b7b173..2a05b0b 100644
--- a/src/platform/raylib/host_validation_stage11c.cpp
+++ b/src/platform/raylib/host_validation_stage11c.cpp
@@ -35,63 +35,61 @@ void write_stage11c_rect(std::ostream& stream, const char* name,
 }
 
 }  // namespace
 
 PhysicalKeySnapshot inject_stage11c_physical_edges(
     PhysicalKeySnapshot snapshot, const RaylibHostConfig& config,
     const settings::SettingsData& settings_data,
     const dungeon::DungeonSnapshot& current,
     Stage11CHudValidationState& state) noexcept {
     using Scenario = Stage11CHudValidationScenario;
-    if (config.stage11c_hud_validation == Scenario::none
-            || snapshot.focus_lost) {
-        return snapshot;
-    }
-    ++state.injected_frames;
-    if (config.stage11c_hud_validation == Scenario::debug_overlay) {
-        if (!state.debug_visible) snapshot.f1 = true;
-        return snapshot;
-    }
-    if (!current.combat.has_value()) {
-        return snapshot;
-    }
-    const combat::CombatSnapshot& combat_snapshot = *current.combat;
-    if (current.phase == dungeon::RoomPhase::combat) {
-        const combat::MonsterSnapshot* const target =
-            nearest_living_monster(combat_snapshot);
-        if (target == nullptr) return snapshot;
-        const bool clears_room = config.stage11c_hud_validation
-                == Scenario::cleared_exit
-            || config.stage11c_hud_validation == Scenario::level_up_points
-            || config.stage11c_hud_validation == Scenario::abyss_abandon;
-        combat::Vec3 destination = target->position;
-        if (clears_room) {
-            destination.x += combat_snapshot.player.position.x
-                    <= target->position.x ? -1.0F : 1.0F;
-        }
-        inject_validation_movement(snapshot, settings_data,
-            validation_movement_toward(
-                combat_snapshot.player.position, destination));
-        if (clears_room && validation_attack_lane(combat_snapshot, *target)) {
-            inject_validation_action(snapshot, settings_data,
-                settings::SettingAction::light_attack, true);
+    if (config.stage11c_hud_validation != Scenario::none
+            && !snapshot.focus_lost) {
+        ++state.injected_frames;
+        if (config.stage11c_hud_validation == Scenario::debug_overlay) {
+            if (!state.debug_visible) snapshot.f1 = true;
+        } else if (current.combat.has_value()) {
+            const combat::CombatSnapshot& combat_snapshot = *current.combat;
+            if (current.phase == dungeon::RoomPhase::combat) {
+                const combat::MonsterSnapshot* const target =
+                    nearest_living_monster(combat_snapshot);
+                if (target != nullptr) {
+                    const bool clears_room = config.stage11c_hud_validation
+                            == Scenario::cleared_exit
+                        || config.stage11c_hud_validation
+                            == Scenario::level_up_points
+                        || config.stage11c_hud_validation
+                            == Scenario::abyss_abandon;
+                    combat::Vec3 destination = target->position;
+                    if (clears_room) {
+                        destination.x += combat_snapshot.player.position.x
+                                <= target->position.x ? -1.0F : 1.0F;
+                    }
+                    inject_validation_movement(snapshot, settings_data,
+                        validation_movement_toward(
+                            combat_snapshot.player.position, destination));
+                    if (clears_room
+                            && validation_attack_lane(combat_snapshot, *target)) {
+                        inject_validation_action(snapshot, settings_data,
+                            settings::SettingAction::light_attack, true);
+                    }
+                }
+            } else if (config.stage11c_hud_validation == Scenario::abyss_abandon
+                    && current.phase == dungeon::RoomPhase::awaiting_exit) {
+                const dungeon::ExitDirection direction = current.is_abyss
+                    ? dungeon::ExitDirection::right : validation_direction(config);
+                inject_validation_movement(snapshot, settings_data,
+                    validation_exit_movement(
+                        combat_snapshot.player.position, direction));
+            }
         }
-        return snapshot;
     }
-    if (config.stage11c_hud_validation != Scenario::abyss_abandon
-            || current.phase != dungeon::RoomPhase::awaiting_exit) {
-        return snapshot;
-    }
-    const dungeon::ExitDirection direction = current.is_abyss
-        ? dungeon::ExitDirection::right : validation_direction(config);
-    inject_validation_movement(snapshot, settings_data,
-        validation_exit_movement(combat_snapshot.player.position, direction));
     return snapshot;
 }
 
 std::uint64_t stage11c_production_snapshot_hash(
     const dungeon::DungeonSnapshot& snapshot) noexcept {
     std::uint64_t hash = 1469598103934665603ULL;
     const auto mix = [&hash](std::uint64_t value) noexcept {
         hash ^= value;
         hash *= 1099511628211ULL;
     };
@@ -160,29 +158,27 @@ bool stage11c_hud_validation_reached(
             && snapshot.last_levels_gained != 0U
             && snapshot.progression.unspent_passive_points != 0U;
     case Scenario::debug_overlay:
         return state.injected_frames >= 4U && draw_debug;
     }
     return false;
 }
 
 void write_stage11c_hud_validation_summary(const RaylibHostConfig& config,
     const Stage11CHudValidationState& state) noexcept {
-    if (!config.validation_summary_file.has_value()
-            || config.stage11c_hud_validation
-                == Stage11CHudValidationScenario::none) {
-        return;
-    }
-    try {
+    if (config.validation_summary_file.has_value()
+            && config.stage11c_hud_validation
+                != Stage11CHudValidationScenario::none) {
+        try {
         std::ofstream stream(*config.validation_summary_file,
             std::ios::out | std::ios::trunc);
-        if (!stream) return;
+        if (stream) {
         stream << "scenario="
                << stage11c_scenario_name(config.stage11c_hud_validation) << '\n';
         write_stage11c_rect(stream, "safe_rect", state.layout.safe_area);
         write_stage11c_rect(stream, "player_rect", state.layout.player_panel);
         write_stage11c_rect(stream, "objective_rect", state.layout.objective_panel);
         write_stage11c_rect(stream, "navigation_rect", state.layout.navigation_panel);
         write_stage11c_rect(stream, "primary_notice_rect", state.layout.primary_notice);
         write_stage11c_rect(stream, "secondary_notice_rect", state.layout.secondary_notice);
         write_stage11c_rect(stream, "debug_rect", state.layout.debug_panel);
         const PlayerHudModel& player = state.model.player;
@@ -212,16 +208,18 @@ void write_stage11c_hud_validation_summary(const RaylibHostConfig& config,
         }
         const bool stage11c_validation_result = state.captured
             && state.cjk_font_ready && state.production_snapshot_hash != 0U;
         stream << '\n'
                << "font_ready=" << (state.cjk_font_ready ? 1 : 0) << '\n'
                << "f1=" << (state.debug_visible ? 1 : 0) << '\n'
                << "production_snapshot_hash="
                << state.production_snapshot_hash << '\n'
                << "result="
                << (stage11c_validation_result ? "pass" : "fail") << '\n';
-    } catch (...) {
-        TraceLog(LOG_WARNING, "failed to write stage11c HUD validation summary");
+        }
+        } catch (...) {
+            TraceLog(LOG_WARNING, "failed to write stage11c HUD validation summary");
+        }
     }
 }
 
 }  // namespace arpg::platform::host_validation
diff --git a/tests/platform/cmake_source_registration_scan.cmake b/tests/platform/cmake_source_registration_scan.cmake
index 3307958..34278c4 100644
--- a/tests/platform/cmake_source_registration_scan.cmake
+++ b/tests/platform/cmake_source_registration_scan.cmake
@@ -1,83 +1,201 @@
-# Counts an exact, unquoted source argument only while parsing the raw lines of
-# add_library(arpg_raylib ...) or target_sources(arpg_raylib ...).  In
-# particular, comments are removed before semicolons can be interpreted as
-# list separators.
-function(arpg_cmake_code_line_and_paren_delta LINE OUT_CODE OUT_DELTA)
-    string(LENGTH "${LINE}" _length)
+# Counts an exact, unquoted source argument only while parsing the code-state
+# lines of add_library(arpg_raylib ...) or target_sources(arpg_raylib ...).
+# CMake bracket comments and bracket arguments are lexical regions: their
+# contents never contribute source names, comments, semicolons, or parentheses.
+function(arpg_cmake_bracket_opener SOURCE POSITION OUT_FOUND OUT_CLOSE OUT_AFTER)
+    string(LENGTH "${SOURCE}" _length)
+    if(POSITION GREATER_EQUAL _length)
+        set(${OUT_FOUND} FALSE PARENT_SCOPE)
+        return()
+    endif()
+    string(SUBSTRING "${SOURCE}" ${POSITION} 1 _open)
+    if(NOT _open STREQUAL "[")
+        set(${OUT_FOUND} FALSE PARENT_SCOPE)
+        return()
+    endif()
+    math(EXPR _scan "${POSITION} + 1")
+    set(_equals "")
+    while(_scan LESS _length)
+        string(SUBSTRING "${SOURCE}" ${_scan} 1 _character)
+        if(NOT _character STREQUAL "=")
+            break()
+        endif()
+        string(APPEND _equals "=")
+        math(EXPR _scan "${_scan} + 1")
+    endwhile()
+    if(_scan LESS _length)
+        string(SUBSTRING "${SOURCE}" ${_scan} 1 _close_open)
+        if(_close_open STREQUAL "[")
+            math(EXPR _after "${_scan} + 1")
+            set(${OUT_FOUND} TRUE PARENT_SCOPE)
+            set(${OUT_CLOSE} "]${_equals}]" PARENT_SCOPE)
+            set(${OUT_AFTER} ${_after} PARENT_SCOPE)
+            return()
+        endif()
+    endif()
+    set(${OUT_FOUND} FALSE PARENT_SCOPE)
+endfunction()
+
+function(arpg_cmake_append_masked COUNT OUT)
+    set(_masked "")
+    if(COUNT GREATER 0)
+        math(EXPR _last "${COUNT} - 1")
+        foreach(_index RANGE 0 ${_last})
+            string(APPEND _masked " ")
+        endforeach()
+    endif()
+    set(${OUT} "${_masked}" PARENT_SCOPE)
+endfunction()
+
+function(arpg_cmake_code_surface CMAKE_TEXT OUT_SURFACE)
+    string(REPLACE "\r\n" "\n" _text "${CMAKE_TEXT}")
+    string(REPLACE "\r" "\n" _text "${_text}")
+    string(LENGTH "${_text}" _length)
+    string(ASCII 10 _newline)
+    string(ASCII 34 _double_quote)
+    string(ASCII 92 _backslash)
     set(_scan 0)
-    set(_quoted FALSE)
+    set(_state normal)
     set(_escaped FALSE)
-    set(_code "")
-    set(_delta 0)
+    set(_bracket_close "")
+    set(_surface "")
     while(_scan LESS _length)
-        string(SUBSTRING "${LINE}" ${_scan} 1 _character)
-        if(_quoted)
-            string(APPEND _code "${_character}")
+        string(SUBSTRING "${_text}" ${_scan} 1 _character)
+        if(_state STREQUAL "normal")
+            if(_character STREQUAL "${_double_quote}")
+                string(APPEND _surface " ")
+                set(_state double_quote)
+                set(_escaped FALSE)
+                math(EXPR _scan "${_scan} + 1")
+                continue()
+            endif()
+            if(_character STREQUAL "#")
+                math(EXPR _after_hash "${_scan} + 1")
+                arpg_cmake_bracket_opener("${_text}" ${_after_hash}
+                    _bracket_found _bracket_close _after_open)
+                if(_bracket_found)
+                    math(EXPR _opener_length "${_after_open} - ${_scan}")
+                    arpg_cmake_append_masked(${_opener_length} _masked)
+                    string(APPEND _surface "${_masked}")
+                    set(_state bracket_comment)
+                    set(_scan ${_after_open})
+                    continue()
+                endif()
+                string(APPEND _surface " ")
+                set(_state line_comment)
+                math(EXPR _scan "${_scan} + 1")
+                continue()
+            endif()
+            if(_character STREQUAL "[")
+                arpg_cmake_bracket_opener("${_text}" ${_scan}
+                    _bracket_found _bracket_close _after_open)
+                if(_bracket_found)
+                    math(EXPR _opener_length "${_after_open} - ${_scan}")
+                    arpg_cmake_append_masked(${_opener_length} _masked)
+                    string(APPEND _surface "${_masked}")
+                    set(_state bracket_argument)
+                    set(_scan ${_after_open})
+                    continue()
+                endif()
+            endif()
+            string(APPEND _surface "${_character}")
+        elseif(_state STREQUAL "double_quote")
+            if(_character STREQUAL "${_newline}")
+                string(APPEND _surface "${_newline}")
+            else()
+                string(APPEND _surface " ")
+            endif()
             if(_escaped)
                 set(_escaped FALSE)
-            elseif(_character STREQUAL "\\")
+            elseif(_character STREQUAL "${_backslash}")
                 set(_escaped TRUE)
-            elseif(_character STREQUAL "\"")
-                set(_quoted FALSE)
+            elseif(_character STREQUAL "${_double_quote}")
+                set(_state normal)
+            endif()
+        elseif(_state STREQUAL "line_comment")
+            if(_character STREQUAL "${_newline}")
+                string(APPEND _surface "${_newline}")
+                set(_state normal)
+            else()
+                string(APPEND _surface " ")
             endif()
         else()
-            if(_character STREQUAL "#")
-                break()
+            string(LENGTH "${_bracket_close}" _close_length)
+            string(SUBSTRING "${_text}" ${_scan} ${_close_length} _candidate)
+            if(_candidate STREQUAL "${_bracket_close}")
+                arpg_cmake_append_masked(${_close_length} _masked)
+                string(APPEND _surface "${_masked}")
+                math(EXPR _scan "${_scan} + ${_close_length}")
+                set(_state normal)
+                continue()
             endif()
-            string(APPEND _code "${_character}")
-            if(_character STREQUAL "\"")
-                set(_quoted TRUE)
-            elseif(_character STREQUAL "(")
-                math(EXPR _delta "${_delta} + 1")
-            elseif(_character STREQUAL ")")
-                math(EXPR _delta "${_delta} - 1")
+            if(_character STREQUAL "${_newline}")
+                string(APPEND _surface "${_newline}")
+            else()
+                string(APPEND _surface " ")
             endif()
         endif()
         math(EXPR _scan "${_scan} + 1")
     endwhile()
-    set(${OUT_CODE} "${_code}" PARENT_SCOPE)
+    set(${OUT_SURFACE} "${_surface}" PARENT_SCOPE)
+endfunction()
+
+function(arpg_cmake_code_line_and_paren_delta LINE OUT_DELTA)
+    string(REGEX REPLACE "[^()]" "" _parentheses "${LINE}")
+    string(LENGTH "${_parentheses}" _length)
+    set(_delta 0)
+    if(_length GREATER 0)
+        math(EXPR _last "${_length} - 1")
+        foreach(_index RANGE 0 ${_last})
+            string(SUBSTRING "${_parentheses}" ${_index} 1 _character)
+            if(_character STREQUAL "(")
+                math(EXPR _delta "${_delta} + 1")
+            else()
+                math(EXPR _delta "${_delta} - 1")
+            endif()
+        endforeach()
+    endif()
     set(${OUT_DELTA} ${_delta} PARENT_SCOPE)
 endfunction()
 
 function(arpg_cmake_count_arpg_raylib_source CMAKE_TEXT SOURCE OUT_COUNT)
-    string(REPLACE "\r\n" "\n" _text "${CMAKE_TEXT}")
-    string(REPLACE "\r" "\n" _text "${_text}")
-    string(LENGTH "${_text}" _length)
+    arpg_cmake_code_surface("${CMAKE_TEXT}" _surface)
+    string(LENGTH "${_surface}" _length)
     set(_scan 0)
     set(_inside FALSE)
     set(_paren_depth 0)
     set(_count 0)
     while(_scan LESS_EQUAL _length)
         if(_scan EQUAL _length)
             set(_line "")
             set(_next -1)
         else()
-            string(SUBSTRING "${_text}" ${_scan} -1 _tail)
+            string(SUBSTRING "${_surface}" ${_scan} -1 _tail)
             string(FIND "${_tail}" "\n" _relative_newline)
             if(_relative_newline EQUAL -1)
                 set(_line "${_tail}")
                 set(_next -1)
             else()
                 string(SUBSTRING "${_tail}" 0 ${_relative_newline} _line)
                 math(EXPR _next "${_scan} + ${_relative_newline}")
             endif()
         endif()
-        arpg_cmake_code_line_and_paren_delta("${_line}" _code _delta)
+        arpg_cmake_code_line_and_paren_delta("${_line}" _delta)
         if(NOT _inside)
-            if(_code MATCHES
+            if(_line MATCHES
                     "^[ \t]*(add_library|target_sources)[ \t]*[(][ \t]*arpg_raylib([ \t]|[)])")
                 set(_inside TRUE)
                 set(_paren_depth ${_delta})
             endif()
         else()
-            if(_code MATCHES "^[ \t]*${SOURCE}[ \t]*$")
+            if(_line MATCHES "^[ \t]*${SOURCE}[ \t]*$")
                 math(EXPR _count "${_count} + 1")
             endif()
             math(EXPR _paren_depth "${_paren_depth} + ${_delta}")
             if(_paren_depth LESS_EQUAL 0)
                 set(_inside FALSE)
             endif()
         endif()
         if(_next EQUAL -1)
             break()
         endif()
diff --git a/tests/platform/host_validation_sequence_guard_self_test.cmake b/tests/platform/host_validation_sequence_guard_self_test.cmake
index c750548..5a3e2ea 100644
--- a/tests/platform/host_validation_sequence_guard_self_test.cmake
+++ b/tests/platform/host_validation_sequence_guard_self_test.cmake
@@ -21,22 +21,56 @@ execute_process(
         "-DCMAKE_OVERRIDE=${_sequence_cmake_mutation}" -P "${_guard}"
     RESULT_VARIABLE _sequence_cmake_result
     OUTPUT_VARIABLE _sequence_cmake_stdout ERROR_VARIABLE _sequence_cmake_stderr)
 if(_sequence_cmake_result EQUAL 0)
     message(FATAL_ERROR "Host validation sequence guard accepted CMake comment/quoted decoy")
 endif()
 if(NOT "${_sequence_cmake_stdout}${_sequence_cmake_stderr}" MATCHES
         "arpg_raylib does not register host_validation_stage11c.cpp")
     message(FATAL_ERROR "sequence CMake comment mutation failed for wrong reason: ${_sequence_cmake_stdout}${_sequence_cmake_stderr}")
 endif()
+
+function(arpg_expect_sequence_cmake_rejection NAME REPLACEMENT)
+    set(_mutation "${GUARD_TEST_ROOT}/sequence-${NAME}.cmake")
+    string(REPLACE "    host_validation_stage11c.cpp" "${REPLACEMENT}"
+        _mutated "${_sequence_cmake_text}")
+    if(_mutated STREQUAL _sequence_cmake_text)
+        message(FATAL_ERROR "sequence ${NAME} mutation anchor is missing")
+    endif()
+    file(WRITE "${_mutation}" "${_mutated}")
+    execute_process(
+        COMMAND "${CMAKE_COMMAND}" "-DSOURCE_ROOT=${SOURCE_ROOT}"
+            "-DCMAKE_OVERRIDE=${_mutation}" -P "${_guard}"
+        RESULT_VARIABLE _result OUTPUT_VARIABLE _stdout ERROR_VARIABLE _stderr)
+    if(_result EQUAL 0)
+        message(FATAL_ERROR "Host validation sequence guard accepted ${NAME}")
+    endif()
+    if(NOT "${_stdout}${_stderr}" MATCHES
+            "arpg_raylib does not register host_validation_stage11c.cpp")
+        message(FATAL_ERROR "sequence ${NAME} failed for wrong reason: ${_stdout}${_stderr}")
+    endif()
+endfunction()
+
+arpg_expect_sequence_cmake_rejection(sequence_cmake_bracket_comment_decoy [==[
+    #[=[
+    host_validation_stage11c.cpp
+    ]]
+    ]=]
+]==])
+arpg_expect_sequence_cmake_rejection(sequence_cmake_bracket_argument_decoy [===[
+    [==[
+    host_validation_stage11c.cpp
+    # ; (target_sources(arpg_raylib fake_target))
+    ]==]
+]===])
 if(DEFINED SEQUENCE_CMAKE_ONLY)
-    message(STATUS "Host validation sequence guard rejected CMake comment/quoted decoy")
+    message(STATUS "Host validation sequence guard rejected all CMake decoys")
     return()
 endif()
 
 function(arpg_assert_lexical_token_equivalence LABEL SOURCE TOKEN EXPECTED_CODE)
     evidence_sanitize_cpp_for_scan("${SOURCE}" _sanitized_fixture)
     string(FIND "${_sanitized_fixture}" "${TOKEN}" _sanitized_position)
     evidence_find_cpp_code_token("${SOURCE}" "${TOKEN}" _fast_position)
     if(EXPECTED_CODE)
         if(_sanitized_position EQUAL -1 OR _fast_position EQUAL -1
                 OR NOT _sanitized_position EQUAL _fast_position)
diff --git a/tests/platform/stage11c_hud_architecture_guard_self_test.cmake b/tests/platform/stage11c_hud_architecture_guard_self_test.cmake
index 80eca61..3305f06 100644
--- a/tests/platform/stage11c_hud_architecture_guard_self_test.cmake
+++ b/tests/platform/stage11c_hud_architecture_guard_self_test.cmake
@@ -188,13 +188,35 @@ arpg_expect_hud_guard_rejects_after_replace(stage_source_forward_decoy
     "bool stage11c_hud_validation_reached(\n    const dungeon::DungeonSnapshot& snapshot,\n    Stage11CHudValidationScenario scenario,\n    const Stage11CHudValidationState& state, bool draw_debug) noexcept {"
     "bool stage11c_hud_validation_reached(\n    const dungeon::DungeonSnapshot& snapshot,\n    Stage11CHudValidationScenario scenario,\n    const Stage11CHudValidationState& state, bool draw_debug) noexcept;\nbool stage11c_hud_validation_reached_moved(\n    const dungeon::DungeonSnapshot& snapshot,\n    Stage11CHudValidationScenario scenario,\n    const Stage11CHudValidationState& state, bool draw_debug) noexcept {"
     ""
     "Stage11C HUD definition is missing from Stage source")
 arpg_expect_hud_guard_rejects_after_replace(stage_cmake_comment_quoted_decoy
     CMakeLists.txt
     "    host_validation_stage11c.cpp"
     "    # decoy;host_validation_stage11c.cpp\nmessage(STATUS \"host_validation_stage11c.cpp\")"
     ""
     "arpg_raylib does not register host_validation_stage11c.cpp exactly once")
+arpg_expect_hud_guard_rejects_after_replace(stage_cmake_bracket_comment_decoy
+    CMakeLists.txt
+    "    host_validation_stage11c.cpp"
+    [==[
+    #[=[
+    host_validation_stage11c.cpp
+    ]]
+    ]=]
+]==]
+    ""
+    "arpg_raylib does not register host_validation_stage11c.cpp exactly once")
+arpg_expect_hud_guard_rejects_after_replace(stage_cmake_bracket_argument_decoy
+    CMakeLists.txt
+    "    host_validation_stage11c.cpp"
+    [===[
+    [==[
+    host_validation_stage11c.cpp
+    # ; (add_library(arpg_raylib fake_target))
+    ]==]
+]===]
+    ""
+    "arpg_raylib does not register host_validation_stage11c.cpp exactly once")
 
 message(STATUS
     "Stage 11C HUD architecture guard rejected all twelve production-source mutations")
diff --git a/tests/platform/stage11c_hud_evidence_guard_self_test.cmake b/tests/platform/stage11c_hud_evidence_guard_self_test.cmake
index 1ff682f..7d08a36 100644
--- a/tests/platform/stage11c_hud_evidence_guard_self_test.cmake
+++ b/tests/platform/stage11c_hud_evidence_guard_self_test.cmake
@@ -195,20 +195,38 @@ function(stage11c_expect_stage_rejection LABEL NEEDLE REPLACEMENT EXPECTED)
             "-DSTAGE11C_SOURCE_OVERRIDE=${_mutation}" -P "${_guard}"
         RESULT_VARIABLE _result OUTPUT_VARIABLE _stdout ERROR_VARIABLE _stderr)
     if(_result EQUAL 0)
         message(FATAL_ERROR "Stage11C evidence guard accepted named Stage mutation: ${LABEL}")
     endif()
     if(NOT "${_stdout}${_stderr}" MATCHES "${EXPECTED}")
         message(FATAL_ERROR "named Stage mutation ${LABEL} failed for wrong reason: ${_stdout}${_stderr}")
     endif()
 endfunction()
 
+function(stage11c_expect_stage_source_rejection LABEL MUTATED EXPECTED)
+    if(MUTATED STREQUAL _stage_source_text)
+        message(FATAL_ERROR "Stage source mutation ${LABEL} did not change production source")
+    endif()
+    set(_mutation "${GUARD_TEST_ROOT}/stage-${LABEL}.cpp")
+    file(WRITE "${_mutation}" "${MUTATED}")
+    execute_process(
+        COMMAND "${CMAKE_COMMAND}" "-DSOURCE_ROOT=${SOURCE_ROOT}"
+            "-DSTAGE11C_SOURCE_OVERRIDE=${_mutation}" -P "${_guard}"
+        RESULT_VARIABLE _result OUTPUT_VARIABLE _stdout ERROR_VARIABLE _stderr)
+    if(_result EQUAL 0)
+        message(FATAL_ERROR "Stage11C evidence guard accepted named Stage mutation: ${LABEL}")
+    endif()
+    if(NOT "${_stdout}${_stderr}" MATCHES "${EXPECTED}")
+        message(FATAL_ERROR "named Stage mutation ${LABEL} failed for wrong reason: ${_stdout}${_stderr}")
+    endif()
+endfunction()
+
 stage11c_expect_input_rejection(input_test_access
     "snapshot.down[index] = true;"
     "snapshot.down[index] = true;\n    TestAccess stage11c_test_access{};"
     "non-physical shared input")
 stage11c_expect_input_rejection(input_direct_viewmodel
     "snapshot.down[index] = true;"
     "snapshot.down[index] = true;\n    HudViewModel direct_model{};"
     "non-physical shared input")
 stage11c_expect_input_rejection(input_direct_logical_action
     "snapshot.down[index] = true;"
@@ -255,20 +273,56 @@ stage11c_expect_host_text_rejection(host_capture_success_removed
     ";"
     "capture ordering")
 
 set(_run_signature "HostExitCode run_raylib_host(const RaylibHostConfig& config) noexcept {")
 string(REPLACE "${_run_signature}"
     "// STAGE11C_HUD_VALIDATION_SEAM_BEGIN observation\nconstexpr const char* stage11c_marker_decoy = \"// STAGE11C_HUD_VALIDATION_SEAM_BEGIN observation\";\n${_run_signature}"
     _marker_decoy_source "${_host_source}")
 stage11c_expect_host_source_rejection(host_marker_comment_string_decoy
     "${_marker_decoy_source}" "cannot bind observation seam marker")
 
+function(stage11c_extract_host_seam SOURCE NAME OUT_SEAM)
+    set(_begin "// STAGE11C_HUD_VALIDATION_SEAM_BEGIN ${NAME}")
+    set(_end "// STAGE11C_HUD_VALIDATION_SEAM_END ${NAME}")
+    string(FIND "${SOURCE}" "${_begin}" _begin_position)
+    string(FIND "${SOURCE}" "${_end}" _end_position)
+    if(_begin_position EQUAL -1 OR _end_position EQUAL -1
+            OR _end_position LESS _begin_position)
+        message(FATAL_ERROR "cannot extract Stage11C ${NAME} seam mutation")
+    endif()
+    string(LENGTH "${_end}" _end_length)
+    math(EXPR _seam_length "${_end_position} - ${_begin_position} + ${_end_length}")
+    string(SUBSTRING "${SOURCE}" ${_begin_position} ${_seam_length} _seam)
+    set(${OUT_SEAM} "${_seam}" PARENT_SCOPE)
+endfunction()
+
+stage11c_extract_host_seam("${_host_source}" observation _observation_seam)
+string(REPLACE "${_observation_seam}"
+    "const auto stage11c_observation_decoy = [&] {\n${_observation_seam}\n            };\n            const bool stage11c_target_visible = false;"
+    _observation_lambda_source "${_host_source}")
+stage11c_expect_host_source_rejection(host_observation_seam_in_lambda
+    "${_observation_lambda_source}" "observation seam scope")
+
+stage11c_extract_host_seam("${_host_source}" presented_capture _presented_capture_seam)
+string(REPLACE "${_presented_capture_seam}"
+    "const auto stage11c_presented_capture_decoy = [&] {\n${_presented_capture_seam}\n            };\n            static_cast<void>(present_frame_and_maybe_capture(nullptr));\n            ++presented_frame_count;\n            const bool capture_succeeded = false;\n            const bool captured_stage10_frame = false;"
+    _presented_capture_lambda_source "${_host_source}")
+stage11c_expect_host_source_rejection(host_presented_capture_seam_in_lambda
+    "${_presented_capture_lambda_source}" "presented capture seam scope")
+
+stage11c_extract_host_seam("${_host_source}" reached _reached_seam)
+string(REPLACE "${_reached_seam}"
+    "if (false) {\n${_reached_seam}\n            }\n            const bool stage11c_reached = false;"
+    _reached_if_false_source "${_host_source}")
+stage11c_expect_host_source_rejection(host_reached_seam_in_if_false
+    "${_reached_if_false_source}" "reached seam scope")
+
 set(_captured_block "if (captured_stage10_frame && stage11c_reached) {\n                stage11c_validation_state.captured = true;\n            }")
 string(REPLACE "${_captured_block}" "if (captured_stage10_frame && stage11c_reached) {\n            }"
     _captured_early_source "${_host_source}")
 string(REPLACE "const bool capture_succeeded =\n                present_frame_and_maybe_capture("
     "if (stage11c_reached) {\n                stage11c_validation_state.captured = true;\n            }\n            const bool capture_succeeded =\n                present_frame_and_maybe_capture("
     _captured_early_source "${_captured_early_source}")
 stage11c_expect_host_source_rejection(host_captured_moved_before_present
     "${_captured_early_source}" "rejected capture ordering")
 
 string(REPLACE "${_captured_block}"
@@ -306,33 +360,88 @@ stage11c_expect_stage_rejection(stage_reached_constant
     "case Scenario::cleared_exit:"
     "return true;"
     "reached predicate token")
 stage11c_expect_stage_rejection(stage_summary_notice_removed
     "state.notices.primary.kind"
     "state.notices_primary_removed"
     "summary token")
 stage11c_expect_stage_rejection(stage_driver_early_return
     "Stage11CHudValidationState& state) noexcept {"
     "Stage11CHudValidationState& state) noexcept {\n    return snapshot;"
-    "physical driver early return")
+    "physical driver return inventory")
 stage11c_expect_stage_rejection(stage_hash_early_return
     "const dungeon::DungeonSnapshot& snapshot) noexcept {"
     "const dungeon::DungeonSnapshot& snapshot) noexcept {\n    return 1U;"
-    "production hash early return")
+    "production hash return inventory")
 stage11c_expect_stage_rejection(stage_reached_early_return
     "const Stage11CHudValidationState& state, bool draw_debug) noexcept {"
     "const Stage11CHudValidationState& state, bool draw_debug) noexcept {\n    return true;"
-    "reached predicate early return")
+    "reached predicate return inventory")
 stage11c_expect_stage_rejection(stage_summary_early_return
     "const Stage11CHudValidationState& state) noexcept {"
     "const Stage11CHudValidationState& state) noexcept {\n    return;"
-    "summary early return")
+    "summary return inventory")
+stage11c_expect_stage_rejection(stage_driver_unreachable_return
+    "Stage11CHudValidationState& state) noexcept {"
+    "Stage11CHudValidationState& state) noexcept {\n    if (true) {\n        return snapshot;\n    }"
+    "physical driver return inventory")
+stage11c_expect_stage_rejection(stage_hash_unreachable_return
+    "const dungeon::DungeonSnapshot& snapshot) noexcept {"
+    "const dungeon::DungeonSnapshot& snapshot) noexcept {\n    if (true) {\n        return 1U;\n    }"
+    "production hash return inventory")
+stage11c_expect_stage_rejection(stage_reached_unreachable_return
+    "const Stage11CHudValidationState& state, bool draw_debug) noexcept {"
+    "const Stage11CHudValidationState& state, bool draw_debug) noexcept {\n    if (true) {\n        return true;\n    }"
+    "reached predicate return inventory")
+stage11c_expect_stage_rejection(stage_summary_unreachable_return
+    "const Stage11CHudValidationState& state) noexcept {"
+    "const Stage11CHudValidationState& state) noexcept {\n    if (true) {\n        return;\n    }"
+    "summary return inventory")
+stage11c_expect_stage_rejection(stage_hash_direct_overwrite
+    "mix(snapshot.root_seed);"
+    "mix(snapshot.root_seed);\n    hash = 1U;"
+    "production hash direct overwrite")
+stage11c_expect_stage_rejection(stage_hash_offset_basis_changed
+    "1469598103934665603ULL"
+    "1469598103934665604ULL"
+    "production hash core")
+stage11c_expect_stage_rejection(stage_hash_xor_changed
+    "hash ^= value;"
+    "hash ^= value + 1U;"
+    "production hash core")
+stage11c_expect_stage_rejection(stage_hash_prime_changed
+    "hash *= 1099511628211ULL;"
+    "hash *= 1099511628213ULL;"
+    "production hash core")
+
+set(_hash_core_begin "    std::uint64_t hash = 1469598103934665603ULL;")
+set(_hash_core_return "    return hash;")
+string(FIND "${_stage_source_text}" "${_hash_core_begin}" _hash_core_begin_at)
+string(FIND "${_stage_source_text}" "${_hash_core_return}" _hash_core_return_at)
+if(_hash_core_begin_at EQUAL -1 OR _hash_core_return_at EQUAL -1
+        OR NOT _hash_core_begin_at LESS _hash_core_return_at)
+    message(FATAL_ERROR "hash relocation mutation anchor is missing")
+endif()
+string(SUBSTRING "${_stage_source_text}" 0 ${_hash_core_begin_at}
+    _hash_relocation_prefix)
+math(EXPR _hash_core_length "${_hash_core_return_at} - ${_hash_core_begin_at}")
+string(SUBSTRING "${_stage_source_text}" ${_hash_core_begin_at}
+    ${_hash_core_length} _hash_relocation_body)
+string(LENGTH "${_hash_core_return}" _hash_core_return_length)
+math(EXPR _hash_relocation_suffix_at
+    "${_hash_core_return_at} + ${_hash_core_return_length}")
+string(SUBSTRING "${_stage_source_text}" ${_hash_relocation_suffix_at} -1
+    _hash_relocation_suffix)
+set(_hash_relocation_source "${_hash_relocation_prefix}    [[maybe_unused]] const auto stage11c_hash_core_decoy = [&] {\n${_hash_relocation_body}    };\n    std::uint64_t hash{1U};\n    return hash;${_hash_relocation_suffix}")
+stage11c_expect_stage_source_rejection(stage_hash_core_in_uncalled_lambda
+    "${_hash_relocation_source}"
+    "production hash core")
 
 stage11c_expect_host_rejection(host_bypassed_session_progression
     "${_model_copy}"
     "${_model_copy}\n                current.progression.level = 99U;"
     "bypassed Session progression")
 
 set(_state_alias
     "host_validation::Stage11CHudValidationState& stage11c_validation_state =\n            validation_states->stage11c;")
 string(REPLACE "${_state_alias}"
     "host_validation::Stage11CHudValidationState stage11c_validation_state{};"
diff --git a/tests/platform/stage11c_hud_evidence_guard_test.cmake b/tests/platform/stage11c_hud_evidence_guard_test.cmake
index db0e78c..f38aa0d 100644
--- a/tests/platform/stage11c_hud_evidence_guard_test.cmake
+++ b/tests/platform/stage11c_hud_evidence_guard_test.cmake
@@ -51,61 +51,73 @@ function(stage11c_count_raw_token SOURCE TOKEN OUT_COUNT)
         math(EXPR _position "${_scan} + ${_relative}")
         math(EXPR _count "${_count} + 1")
         string(LENGTH "${TOKEN}" _token_length)
         math(EXPR _scan "${_position} + ${_token_length}")
     endwhile()
     set(${OUT_COUNT} ${_count} PARENT_SCOPE)
 endfunction()
 
 # The input starts at a code-state-verified function signature. This streams
 # lexical delimiters only; it never builds a sanitized copy of the host.
-function(stage11c_find_line_comment_markers SOURCE MARKER OUT_POSITIONS)
+function(stage11c_find_stage11c_seam_markers SOURCE OUT_ENTRIES)
     string(ASCII 10 _newline)
     string(ASCII 13 _carriage_return)
     string(ASCII 34 _double_quote)
     string(ASCII 39 _single_quote)
     string(ASCII 92 _backslash)
     string(LENGTH "${SOURCE}" _source_length)
     set(_scan 0)
     set(_state code)
-    set(_positions)
+    set(_brace_depth 0)
+    set(_entries)
     while(_scan LESS _source_length)
         if(_state STREQUAL "code")
             set(_next -1)
-            foreach(_delimiter IN ITEMS "/" "${_double_quote}" "${_single_quote}")
+            foreach(_delimiter IN ITEMS "/" "${_double_quote}" "${_single_quote}" "{" "}")
                 evidence_find_cpp_from_offset("${SOURCE}" ${_scan}
                     "${_delimiter}" _candidate)
                 if(NOT _candidate EQUAL -1
                         AND (_next EQUAL -1 OR _candidate LESS _next))
                     set(_next ${_candidate})
                 endif()
             endforeach()
             if(_next EQUAL -1)
                 break()
             endif()
             string(SUBSTRING "${SOURCE}" ${_next} 1 _character)
             if(_character STREQUAL "/")
                 math(EXPR _after_slash "${_next} + 1")
                 evidence_cpp_logical_next_character("${SOURCE}" ${_after_slash}
                     _next_character _next_found)
                 if(_next_found AND _next_character STREQUAL "/")
                     string(SUBSTRING "${SOURCE}" ${_next} -1 _comment_tail)
-                    string(FIND "${_comment_tail}" "${MARKER}" _marker_at)
-                    if(_marker_at EQUAL 0)
-                        list(APPEND _positions ${_next})
-                    endif()
+                    foreach(_name IN ITEMS observation reached presented_capture)
+                        foreach(_kind IN ITEMS BEGIN END)
+                            set(_marker
+                                "// STAGE11C_HUD_VALIDATION_SEAM_${_kind} ${_name}")
+                            string(FIND "${_comment_tail}" "${_marker}" _marker_at)
+                            if(_marker_at EQUAL 0)
+                                list(APPEND _entries
+                                    "${_name}|${_kind}|${_next}|${_brace_depth}")
+                            endif()
+                        endforeach()
+                    endforeach()
                     set(_state line_comment)
                 elseif(_next_found AND _next_character STREQUAL "*")
                     set(_state block_comment)
                 endif()
             elseif(_character STREQUAL "${_double_quote}")
                 set(_state string_literal)
+            elseif(_character STREQUAL "{")
+                math(EXPR _brace_depth "${_brace_depth} + 1")
+            elseif(_character STREQUAL "}")
+                math(EXPR _brace_depth "${_brace_depth} - 1")
             else()
                 set(_state character_literal)
             endif()
             math(EXPR _scan "${_next} + 1")
         elseif(_state STREQUAL "line_comment")
             set(_next -1)
             foreach(_delimiter IN ITEMS "${_backslash}" "${_newline}" "${_carriage_return}")
                 evidence_find_cpp_from_offset("${SOURCE}" ${_scan}
                     "${_delimiter}" _candidate)
                 if(NOT _candidate EQUAL -1
@@ -177,71 +189,93 @@ function(stage11c_find_line_comment_markers SOURCE MARKER OUT_POSITIONS)
                     endif()
                 else()
                     math(EXPR _scan "${_next} + 2")
                 endif()
             else()
                 set(_state code)
                 math(EXPR _scan "${_next} + 1")
             endif()
         endif()
     endwhile()
-    set(${OUT_POSITIONS} "${_positions}" PARENT_SCOPE)
+    set(${OUT_ENTRIES} "${_entries}" PARENT_SCOPE)
 endfunction()
 
 evidence_find_cpp_code_token("${_host_text}"
     "HostExitCode run_raylib_host(" _stage11c_runtime_begin)
 if(_stage11c_runtime_begin EQUAL -1)
     message(FATAL_ERROR "Stage11C evidence guard cannot bind run_raylib_host")
 endif()
 string(SUBSTRING "${_host_text}" ${_stage11c_runtime_begin} -1
     _stage11c_runtime_tail)
 evidence_find_cpp_code_token("${_stage11c_runtime_tail}" "audio.shutdown();"
     _stage11c_runtime_shutdown_relative)
 if(_stage11c_runtime_shutdown_relative EQUAL -1)
     message(FATAL_ERROR "Stage11C evidence guard cannot bind run_raylib_host")
 endif()
 math(EXPR _stage11c_runtime_length
     "${_stage11c_runtime_shutdown_relative} + 17")
 string(SUBSTRING "${_stage11c_runtime_tail}" 0 ${_stage11c_runtime_length}
     _stage11c_runtime_crop)
+stage11c_find_stage11c_seam_markers("${_stage11c_runtime_crop}"
+    _stage11c_runtime_seam_markers)
 
-function(stage11c_extract_seam LABEL OUT)
-    set(_begin "// STAGE11C_HUD_VALIDATION_SEAM_BEGIN ${LABEL}")
-    set(_end "// STAGE11C_HUD_VALIDATION_SEAM_END ${LABEL}")
+function(stage11c_lookup_runtime_seam_marker NAME KIND OUT_POSITION OUT_DEPTH)
+    set(_matches)
+    foreach(_entry IN LISTS _stage11c_runtime_seam_markers)
+        string(REPLACE "|" ";" _parts "${_entry}")
+        list(GET _parts 0 _entry_name)
+        list(GET _parts 1 _entry_kind)
+        if(_entry_name STREQUAL "${NAME}" AND _entry_kind STREQUAL "${KIND}")
+            list(APPEND _matches "${_entry}")
+        endif()
+    endforeach()
+    list(LENGTH _matches _match_count)
+    if(NOT _match_count EQUAL 1)
+        message(FATAL_ERROR "Stage11C evidence guard cannot bind ${NAME} seam marker")
+    endif()
+    list(GET _matches 0 _match)
+    string(REPLACE "|" ";" _match_parts "${_match}")
+    list(GET _match_parts 2 _position)
+    list(GET _match_parts 3 _depth)
+    set(${OUT_POSITION} ${_position} PARENT_SCOPE)
+    set(${OUT_DEPTH} ${_depth} PARENT_SCOPE)
+endfunction()
+
+function(stage11c_extract_seam NAME SCOPE_LABEL EXPECTED_DEPTH OUT)
+    set(_begin "// STAGE11C_HUD_VALIDATION_SEAM_BEGIN ${NAME}")
+    set(_end "// STAGE11C_HUD_VALIDATION_SEAM_END ${NAME}")
     stage11c_count_raw_token("${_host_text}" "${_begin}" _raw_begin_count)
     stage11c_count_raw_token("${_host_text}" "${_end}" _raw_end_count)
-    stage11c_find_line_comment_markers("${_stage11c_runtime_crop}" "${_begin}"
-        _begins)
-    stage11c_find_line_comment_markers("${_stage11c_runtime_crop}" "${_end}"
-        _ends)
-    list(LENGTH _begins _begin_count)
-    list(LENGTH _ends _end_count)
-    if(NOT _raw_begin_count EQUAL 1 OR NOT _raw_end_count EQUAL 1
-            OR NOT _begin_count EQUAL 1 OR NOT _end_count EQUAL 1)
-        message(FATAL_ERROR "Stage11C evidence guard cannot bind ${LABEL} seam marker")
-    endif()
-    list(GET _begins 0 _begin_at)
-    list(GET _ends 0 _end_at)
+    if(NOT _raw_begin_count EQUAL 1 OR NOT _raw_end_count EQUAL 1)
+        message(FATAL_ERROR "Stage11C evidence guard cannot bind ${NAME} seam marker")
+    endif()
+    stage11c_lookup_runtime_seam_marker("${NAME}" BEGIN _begin_at _begin_depth)
+    stage11c_lookup_runtime_seam_marker("${NAME}" END _end_at _end_depth)
     if(NOT _begin_at LESS _end_at)
-        message(FATAL_ERROR "Stage11C evidence guard cannot isolate ${LABEL} seam")
+        message(FATAL_ERROR "Stage11C evidence guard cannot isolate ${NAME} seam")
+    endif()
+    if(NOT _begin_depth EQUAL _end_depth
+            OR NOT _begin_depth EQUAL EXPECTED_DEPTH)
+        message(FATAL_ERROR "Stage11C evidence guard rejected ${SCOPE_LABEL} seam scope: expected ${EXPECTED_DEPTH}, begin ${_begin_depth}, end ${_end_depth}")
     endif()
     string(LENGTH "${_end}" _end_length)
     math(EXPR _length "${_end_at} - ${_begin_at} + ${_end_length}")
     string(SUBSTRING "${_stage11c_runtime_crop}" ${_begin_at} ${_length} _raw)
     evidence_sanitize_cpp_for_scan("${_raw}" _code)
     set(${OUT} "${_code}" PARENT_SCOPE)
-    set("_stage11c_${LABEL}_begin" ${_begin_at} PARENT_SCOPE)
-    set("_stage11c_${LABEL}_end" ${_end_at} PARENT_SCOPE)
+    set("_stage11c_${NAME}_begin" ${_begin_at} PARENT_SCOPE)
+    set("_stage11c_${NAME}_end" ${_end_at} PARENT_SCOPE)
 endfunction()
-stage11c_extract_seam(observation _stage11c_observation)
-stage11c_extract_seam(reached _stage11c_reached_seam)
-stage11c_extract_seam(presented_capture _stage11c_presented_capture)
+stage11c_extract_seam(observation "observation" 3 _stage11c_observation)
+stage11c_extract_seam(reached "reached" 3 _stage11c_reached_seam)
+stage11c_extract_seam(presented_capture "presented capture" 3
+    _stage11c_presented_capture)
 if(NOT _stage11c_observation_end LESS _stage11c_reached_begin
         OR NOT _stage11c_reached_end LESS _stage11c_presented_capture_begin)
     message(FATAL_ERROR "Stage11C evidence guard rejected seam marker order")
 endif()
 file(READ "${_header}" _header_text)
 file(READ "${_input_header}" _input_header_text)
 file(READ "${_input_source}" _input_source_text)
 file(READ "${_stage_header}" _stage_header_text)
 file(READ "${_stage_source}" _stage_source_text)
 file(READ "${_formal}" _formal_text)
@@ -346,20 +380,107 @@ function(stage11c_code_brace_depth SURFACE POSITION OUT_DEPTH)
             if(_brace STREQUAL "{")
                 math(EXPR _depth "${_depth} + 1")
             else()
                 math(EXPR _depth "${_depth} - 1")
             endif()
         endforeach()
     endif()
     set(${OUT_DEPTH} ${_depth} PARENT_SCOPE)
 endfunction()
 
+function(stage11c_require_unique_hash_core_token SURFACE TOKEN OUT_POSITION)
+    string(FIND "${SURFACE}" "${TOKEN}" _position)
+    if(_position EQUAL -1)
+        message(FATAL_ERROR "Stage11C evidence guard rejected production hash core")
+    endif()
+    math(EXPR _after "${_position} + 1")
+    string(SUBSTRING "${SURFACE}" ${_after} -1 _tail)
+    string(FIND "${_tail}" "${TOKEN}" _duplicate)
+    if(NOT _duplicate EQUAL -1)
+        message(FATAL_ERROR "Stage11C evidence guard rejected production hash core")
+    endif()
+    set(${OUT_POSITION} ${_position} PARENT_SCOPE)
+endfunction()
+
+function(stage11c_matching_brace_position SURFACE OPEN_POSITION OUT_POSITION)
+    string(LENGTH "${SURFACE}" _length)
+    set(_depth 0)
+    set(_close -1)
+    while(OPEN_POSITION LESS _length)
+        string(SUBSTRING "${SURFACE}" ${OPEN_POSITION} 1 _character)
+        if(_character STREQUAL "{")
+            math(EXPR _depth "${_depth} + 1")
+        elseif(_character STREQUAL "}")
+            math(EXPR _depth "${_depth} - 1")
+            if(_depth EQUAL 0)
+                set(_close ${OPEN_POSITION})
+                break()
+            endif()
+        endif()
+        math(EXPR OPEN_POSITION "${OPEN_POSITION} + 1")
+    endwhile()
+    if(_close EQUAL -1)
+        message(FATAL_ERROR "Stage11C evidence guard rejected production hash core")
+    endif()
+    set(${OUT_POSITION} ${_close} PARENT_SCOPE)
+endfunction()
+
+function(stage11c_require_hash_core SURFACE)
+    stage11c_require_unique_hash_core_token("${SURFACE}"
+        "std::uint64_t hash = 1469598103934665603ULL;" _initialization)
+    stage11c_require_unique_hash_core_token("${SURFACE}" "hash ^= value;" _xor)
+    stage11c_require_unique_hash_core_token("${SURFACE}"
+        "hash *= 1099511628211ULL;" _prime)
+    set(_mix_signature "const auto mix = [&hash](std::uint64_t value) noexcept {")
+    stage11c_require_unique_hash_core_token("${SURFACE}" "${_mix_signature}"
+        _mix_begin)
+    stage11c_require_unique_hash_core_token("${SURFACE}" "return hash;"
+        _final_return)
+    string(SUBSTRING "${SURFACE}" ${_mix_begin} -1 _mix_tail)
+    string(FIND "${_mix_tail}" "{" _mix_open_relative)
+    math(EXPR _mix_open "${_mix_begin} + ${_mix_open_relative}")
+    stage11c_matching_brace_position("${SURFACE}" ${_mix_open} _mix_close)
+    stage11c_code_brace_depth("${SURFACE}" ${_initialization} _initialization_depth)
+    stage11c_code_brace_depth("${SURFACE}" ${_mix_open} _mix_depth)
+    stage11c_code_brace_depth("${SURFACE}" ${_xor} _xor_depth)
+    stage11c_code_brace_depth("${SURFACE}" ${_prime} _prime_depth)
+    stage11c_code_brace_depth("${SURFACE}" ${_final_return} _return_depth)
+    if(NOT _initialization LESS _mix_begin OR NOT _mix_begin LESS _xor
+            OR NOT _xor LESS _prime OR NOT _prime LESS _mix_close
+            OR NOT _initialization_depth EQUAL 1 OR NOT _mix_depth EQUAL 1
+            OR NOT _xor_depth EQUAL 2 OR NOT _prime_depth EQUAL 2
+            OR NOT _return_depth EQUAL 1)
+        message(FATAL_ERROR "Stage11C evidence guard rejected production hash core")
+    endif()
+    set(_previous_chain_position -1)
+    foreach(_token IN ITEMS
+            "mix(snapshot.session_tick);" "mix(snapshot.root_seed);"
+            "mix(snapshot.commit_generation);" "mix(snapshot.room_index);"
+            "mix(snapshot.room_seed);" "mix(snapshot.depth);"
+            "mix(snapshot.floor_room_index);"
+            "mix(static_cast<std::uint64_t>(snapshot.phase));"
+            "mix(snapshot.is_abyss ? 1U : 0U);"
+            "mix(snapshot.abyss_exit_confirmation_armed ? 1U : 0U);"
+            "mix(snapshot.remaining_targets);" "mix(snapshot.progression.level);"
+            "mix(snapshot.progression.experience);"
+            "mix(snapshot.progression.unspent_passive_points);")
+        stage11c_require_unique_hash_core_token("${SURFACE}" "${_token}"
+            _chain_position)
+        stage11c_code_brace_depth("${SURFACE}" ${_chain_position} _chain_depth)
+        if(NOT _chain_depth EQUAL 1
+                OR NOT _previous_chain_position LESS _chain_position)
+            message(FATAL_ERROR "Stage11C evidence guard rejected production hash core")
+        endif()
+        set(_previous_chain_position ${_chain_position})
+    endforeach()
+endfunction()
+
 function(stage11c_require_unique_seam_token LABEL SURFACE TOKEN EXPECTED_DEPTH)
     string(FIND "${SURFACE}" "${TOKEN}" _position)
     if(_position EQUAL -1)
         message(FATAL_ERROR "Stage11C evidence guard rejected ${LABEL}")
     endif()
     math(EXPR _after "${_position} + 1")
     string(SUBSTRING "${SURFACE}" ${_after} -1 _tail)
     string(FIND "${_tail}" "${TOKEN}" _duplicate)
     if(NOT _duplicate EQUAL -1)
         message(FATAL_ERROR "Stage11C evidence guard rejected ${LABEL}")
@@ -415,128 +536,171 @@ stage11c_require_ordered_seam_tokens("capture ordering"
     "const bool captured_stage10_frame = captured_stage10_target|0"
     "if (captured_stage10_frame && stage11c_reached) {|0"
     "stage11c_validation_state.captured = true|1")
 string(FIND "${_stage11c_presented_capture}"
     "const bool captured_stage10_frame = captured_stage10_target\n                && capture_succeeded;"
     _stage11c_capture_dependency)
 if(_stage11c_capture_dependency EQUAL -1)
     message(FATAL_ERROR "Stage11C evidence guard rejected capture ordering")
 endif()
 
-function(stage11c_top_level_return_positions SURFACE OUT_POSITIONS)
+function(stage11c_return_positions SURFACE OUT_POSITIONS)
     string(LENGTH "${SURFACE}" _length)
     set(_scan 0)
     set(_positions)
     while(_scan LESS _length)
         string(SUBSTRING "${SURFACE}" ${_scan} -1 _tail)
         string(FIND "${_tail}" "return" _relative)
         if(_relative EQUAL -1)
             break()
         endif()
         math(EXPR _position "${_scan} + ${_relative}")
+        set(_code_token TRUE)
+        if(_position GREATER 0)
+            math(EXPR _previous_position "${_position} - 1")
+            string(SUBSTRING "${SURFACE}" ${_previous_position} 1 _previous)
+            if(_previous MATCHES "[A-Za-z0-9_]")
+                set(_code_token FALSE)
+            endif()
+        endif()
         math(EXPR _after_return "${_position} + 6")
         if(_after_return LESS _length)
             string(SUBSTRING "${SURFACE}" ${_after_return} 1 _next)
-            if(NOT _next MATCHES "[A-Za-z0-9_]")
-                stage11c_code_brace_depth("${SURFACE}" ${_position} _depth)
-                if(_depth EQUAL 1)
-                    list(APPEND _positions ${_position})
-                endif()
+            if(_code_token AND NOT _next MATCHES "[A-Za-z0-9_]")
+                list(APPEND _positions ${_position})
             endif()
         endif()
         math(EXPR _scan "${_position} + 6")
     endwhile()
     set(${OUT_POSITIONS} "${_positions}" PARENT_SCOPE)
 endfunction()
 
-function(stage11c_require_single_final_top_level_return LABEL SURFACE EXPECTED)
-    stage11c_top_level_return_positions("${SURFACE}" _returns)
+function(stage11c_require_single_final_return LABEL SURFACE EXPECTED)
+    stage11c_return_positions("${SURFACE}" _returns)
     list(LENGTH _returns _return_count)
     if(NOT _return_count EQUAL 1)
-        message(FATAL_ERROR "Stage11C evidence guard rejected ${LABEL} early return")
+        message(FATAL_ERROR "Stage11C evidence guard rejected ${LABEL} return inventory")
     endif()
     list(GET _returns 0 _return_position)
     string(LENGTH "${EXPECTED}" _expected_length)
     string(SUBSTRING "${SURFACE}" ${_return_position} ${_expected_length}
         _actual_return)
     if(NOT _actual_return STREQUAL "${EXPECTED}")
-        message(FATAL_ERROR "Stage11C evidence guard rejected ${LABEL} early return")
+        message(FATAL_ERROR "Stage11C evidence guard rejected ${LABEL} return inventory")
     endif()
     foreach(_required IN LISTS ARGN)
         string(FIND "${SURFACE}" "${_required}" _required_position)
         if(_required_position EQUAL -1 OR _return_position LESS _required_position)
-            message(FATAL_ERROR "Stage11C evidence guard rejected ${LABEL} early return")
+            message(FATAL_ERROR "Stage11C evidence guard rejected ${LABEL} return inventory")
         endif()
     endforeach()
 endfunction()
 
-function(stage11c_require_switch_before_top_level_return SURFACE)
+function(stage11c_require_switch_return_inventory SURFACE)
     string(FIND "${SURFACE}" "switch (scenario) {" _switch_position)
     if(_switch_position EQUAL -1)
-        message(FATAL_ERROR "Stage11C evidence guard rejected reached predicate early return")
+        message(FATAL_ERROR "Stage11C evidence guard rejected reached predicate return inventory")
     endif()
     stage11c_code_brace_depth("${SURFACE}" ${_switch_position} _switch_depth)
     if(NOT _switch_depth EQUAL 1)
-        message(FATAL_ERROR "Stage11C evidence guard rejected reached predicate early return")
+        message(FATAL_ERROR "Stage11C evidence guard rejected reached predicate return inventory")
+    endif()
+    stage11c_return_positions("${SURFACE}" _returns)
+    list(LENGTH _returns _return_count)
+    if(NOT _return_count EQUAL 9)
+        message(FATAL_ERROR "Stage11C evidence guard rejected reached predicate return inventory")
+    endif()
+    list(GET _returns 0 _pre_switch_return)
+    string(SUBSTRING "${SURFACE}" ${_pre_switch_return} 13 _pre_switch_value)
+    if(NOT _pre_switch_value STREQUAL "return false;")
+        message(FATAL_ERROR "Stage11C evidence guard rejected reached predicate return inventory")
     endif()
-    stage11c_top_level_return_positions("${SURFACE}" _returns)
-    foreach(_return_position IN LISTS _returns)
+    math(EXPR _last_return_index "${_return_count} - 1")
+    foreach(_return_index RANGE 1 ${_last_return_index})
+        list(GET _returns ${_return_index} _return_position)
         if(_return_position LESS _switch_position)
-            message(FATAL_ERROR "Stage11C evidence guard rejected reached predicate early return")
+            message(FATAL_ERROR "Stage11C evidence guard rejected reached predicate return inventory")
         endif()
     endforeach()
 endfunction()
 
-function(stage11c_reject_summary_top_level_return_before_fields SURFACE)
-    stage11c_top_level_return_positions("${SURFACE}" _returns)
-    foreach(_required IN ITEMS
-            "state.model.player" "state.notices.primary.kind"
-            "state.model.navigation.depth" "stage11c_validation_result")
-        string(FIND "${SURFACE}" "${_required}" _required_position)
-        if(_required_position EQUAL -1)
-            message(FATAL_ERROR "Stage11C evidence guard rejected summary early return")
+function(stage11c_reject_summary_returns SURFACE)
+    stage11c_return_positions("${SURFACE}" _returns)
+    if(_returns)
+        message(FATAL_ERROR "Stage11C evidence guard rejected summary return inventory")
+    endif()
+endfunction()
+
+function(stage11c_reject_hash_direct_overwrite SURFACE)
+    string(FIND "${SURFACE}" "std::uint64_t hash =" _initialization)
+    if(_initialization EQUAL -1)
+        message(FATAL_ERROR "Stage11C evidence guard rejected production hash direct overwrite")
+    endif()
+    string(SUBSTRING "${SURFACE}" ${_initialization} -1 _initialization_tail)
+    string(FIND "${_initialization_tail}" ";" _initialization_end_relative)
+    if(_initialization_end_relative EQUAL -1)
+        message(FATAL_ERROR "Stage11C evidence guard rejected production hash direct overwrite")
+    endif()
+    math(EXPR _initialization_end
+        "${_initialization} + ${_initialization_end_relative}")
+    string(LENGTH "${SURFACE}" _length)
+    set(_scan 0)
+    while(_scan LESS _length)
+        string(SUBSTRING "${SURFACE}" ${_scan} -1 _tail)
+        string(FIND "${_tail}" "hash" _relative)
+        if(_relative EQUAL -1)
+            break()
         endif()
-        foreach(_return_position IN LISTS _returns)
-            if(_return_position LESS _required_position)
-                message(FATAL_ERROR "Stage11C evidence guard rejected summary early return")
-            endif()
-        endforeach()
-    endforeach()
+        math(EXPR _position "${_scan} + ${_relative}")
+        math(EXPR _after_hash "${_position} + 4")
+        string(SUBSTRING "${SURFACE}" ${_after_hash} -1 _after_hash_text)
+        string(REGEX MATCH "^[ \t\r\n]*=" _direct_assignment
+            "${_after_hash_text}")
+        string(REGEX MATCH "^[ \t\r\n]*==" _comparison
+            "${_after_hash_text}")
+        if(_direct_assignment AND NOT _comparison
+                AND _position GREATER _initialization_end)
+            message(FATAL_ERROR
+                "Stage11C evidence guard rejected production hash direct overwrite")
+        endif()
+        math(EXPR _scan "${_after_hash}")
+    endwhile()
 endfunction()
 stage11c_require_function_tokens("physical driver" "${_stage11c_driver}"
     "++state.injected_frames;" "nearest_living_monster(combat_snapshot)"
     "inject_validation_movement(" "inject_validation_action(")
 stage11c_require_function_tokens("production hash" "${_stage11c_hash_function}"
     "mix(snapshot.session_tick);" "mix(snapshot.root_seed);"
     "mix(snapshot.progression.unspent_passive_points);"
     "mix(monster.spawn_ordinal);" "return hash;")
+stage11c_require_hash_core("${_stage11c_hash_function}")
 stage11c_require_function_tokens("reached predicate" "${_stage11c_reached_function}"
     "case Scenario::low_health_status:" "player.hp > 0"
     "case Scenario::cleared_exit:" "case Scenario::abyss_abandon:"
     "case Scenario::level_up_points:" "case Scenario::debug_overlay:")
 stage11c_require_function_tokens("summary" "${_stage11c_summary_function}"
     "state.model.player" "player.status_tag_count"
     "state.notices.primary.kind" "state.notices.secondary.kind"
     "state.model.navigation.depth" "state.cjk_font_ready"
     "state.production_snapshot_hash" "stage11c_validation_result")
-stage11c_require_single_final_top_level_return("physical driver" "${_stage11c_driver}"
+stage11c_require_single_final_return("physical driver" "${_stage11c_driver}"
     "return snapshot;"
     "++state.injected_frames;" "nearest_living_monster(combat_snapshot)"
     "inject_validation_movement(" "inject_validation_action(")
-stage11c_require_single_final_top_level_return("production hash"
+stage11c_require_single_final_return("production hash"
     "${_stage11c_hash_function}" "return hash;"
     "mix(snapshot.session_tick);" "mix(snapshot.root_seed);"
     "mix(snapshot.progression.unspent_passive_points);"
     "mix(monster.spawn_ordinal);")
-stage11c_require_switch_before_top_level_return("${_stage11c_reached_function}")
-stage11c_reject_summary_top_level_return_before_fields(
-    "${_stage11c_summary_function}")
+stage11c_require_switch_return_inventory("${_stage11c_reached_function}")
+stage11c_reject_summary_returns("${_stage11c_summary_function}")
+stage11c_reject_hash_direct_overwrite("${_stage11c_hash_function}")
 foreach(_forbidden IN LISTS _physical_input_forbidden)
     string(FIND "${_stage11c_driver}" "${_forbidden}" _found)
     if(NOT _found EQUAL -1)
         message(FATAL_ERROR "Stage11C evidence guard rejected non-physical scenario driver: ${_forbidden}")
     endif()
 endforeach()
 
 set(_stage11c_runtime_alias
     "host_validation::Stage11CHudValidationState& stage11c_validation_state =\n            validation_states->stage11c;")
 string(REGEX MATCHALL
