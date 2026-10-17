# Review package: 33f544d..60a9188

## Commits
60a9188 test: strengthen pause state boundaries

## Files changed
 .../platform/pause_menu_state_boundary_test.cmake  | 144 +++++++++++++++----
 tests/platform/pause_menu_state_tests.cpp          | 153 ++++++++++++++++++++-
 2 files changed, 263 insertions(+), 34 deletions(-)

## Diff
diff --git a/tests/platform/pause_menu_state_boundary_test.cmake b/tests/platform/pause_menu_state_boundary_test.cmake
index ff32e76..9ca3219 100644
--- a/tests/platform/pause_menu_state_boundary_test.cmake
+++ b/tests/platform/pause_menu_state_boundary_test.cmake
@@ -1,59 +1,143 @@
 cmake_minimum_required(VERSION 3.25)
 
 if(NOT DEFINED PAUSE_STATE_HEADER OR NOT DEFINED PAUSE_STATE_SOURCE)
     message(FATAL_ERROR "Pause state source paths are required")
 endif()
 
-set(_arpg_forbidden_fragments
-    "raylib"
-    "settings_store"
-    "settingsstore"
-    "combat"
-    "dungeon"
-    "<filesystem>"
-    "std::filesystem"
-    "fopen("
-    "ifstream"
-    "ofstream")
-
-function(arpg_pause_source_has_forbidden_dependency SOURCE_TEXT OUT_FOUND OUT_FRAGMENT)
-    string(TOLOWER "${SOURCE_TEXT}" _arpg_source_lower)
-    foreach(_arpg_fragment IN LISTS _arpg_forbidden_fragments)
-        string(FIND "${_arpg_source_lower}" "${_arpg_fragment}" _arpg_position)
-        if(NOT _arpg_position EQUAL -1)
+function(arpg_pause_includes_are_allowed SOURCE_TEXT OUT_ALLOWED OUT_BAD_INCLUDE)
+    string(REPLACE "\r\n" "\n" _arpg_source "${SOURCE_TEXT}")
+    string(REPLACE "\r" "\n" _arpg_source "${_arpg_source}")
+    string(REPLACE "\n" ";" _arpg_lines "${_arpg_source}")
+    foreach(_arpg_line IN LISTS _arpg_lines)
+        if(_arpg_line MATCHES "^[ \t]*#[ \t]*include[ \t]+[<\"][^>\"]+[>\"]")
+            if(NOT _arpg_line MATCHES
+                    "^[ \t]*#[ \t]*include[ \t]+(\"pause_menu_state\\.hpp\"|\"platform/settings/settings_types\\.hpp\"|<cstddef>|<cstdint>|<optional>|<limits>)[ \t]*$")
+                set("${OUT_ALLOWED}" FALSE PARENT_SCOPE)
+                set("${OUT_BAD_INCLUDE}" "${_arpg_line}" PARENT_SCOPE)
+                return()
+            endif()
+        endif()
+    endforeach()
+    set("${OUT_ALLOWED}" TRUE PARENT_SCOPE)
+    set("${OUT_BAD_INCLUDE}" "" PARENT_SCOPE)
+endfunction()
+
+function(arpg_pause_source_has_forbidden_api SOURCE_TEXT OUT_FOUND OUT_LABEL)
+    set(_arpg_api_checks
+        "raylib device API|(^|[^A-Za-z0-9_])(IsKeyPressed|IsKeyPressedRepeat|IsKeyDown|IsKeyReleased|IsKeyUp|GetKeyPressed|GetCharPressed|IsMouseButtonPressed|IsMouseButtonDown|IsMouseButtonReleased|IsMouseButtonUp|GetMouseX|GetMouseY|GetMousePosition|GetMouseDelta|GetMouseWheelMove|GetMouseWheelMoveV)[ \t\r\n]*\\("
+        "raylib window API|(^|[^A-Za-z0-9_])(IsWindowFocused|IsWindowState|SetWindowState|ClearWindowState|ToggleFullscreen|ToggleBorderlessWindowed|SetWindowSize|SetWindowPosition|SetWindowMinSize|SetWindowMaxSize|SetWindowTitle|SetWindowMonitor|SetTargetFPS|SetMasterVolume|CloseWindow|WindowShouldClose)[ \t\r\n]*\\("
+        "SettingsStore type|(^|[^A-Za-z0-9_])SettingsStore([^A-Za-z0-9_]|$)"
+        "settings_store token|(^|[^A-Za-z0-9_])settings_store([^A-Za-z0-9_]|$)"
+        "combat namespace|(^|[^A-Za-z0-9_])combat[ \t\r\n]*::"
+        "dungeon namespace|(^|[^A-Za-z0-9_])dungeon[ \t\r\n]*::"
+        "filesystem namespace|(^|[^A-Za-z0-9_])(std[ \t\r\n]*::[ \t\r\n]*)?filesystem[ \t\r\n]*::"
+        "C++ stream type|(^|[^A-Za-z0-9_])(std[ \t\r\n]*::[ \t\r\n]*)?(fstream|ifstream|ofstream)([^A-Za-z0-9_]|$)"
+        "C FILE type|(^|[^A-Za-z0-9_])FILE([^A-Za-z0-9_]|$)"
+        "C file API|(^|[^A-Za-z0-9_])(fopen|fread|fwrite|fclose)[ \t\r\n]*\\("
+        "Windows file API|(^|[^A-Za-z0-9_])(CreateFile|ReadFile|WriteFile|MoveFile|MoveFileEx|ReplaceFile|DeleteFile|FlushFileBuffers)(A|W)?[ \t\r\n]*\\(")
+    foreach(_arpg_check IN LISTS _arpg_api_checks)
+        string(FIND "${_arpg_check}" "|" _arpg_separator)
+        string(SUBSTRING "${_arpg_check}" 0 ${_arpg_separator} _arpg_label)
+        math(EXPR _arpg_pattern_start "${_arpg_separator} + 1")
+        string(SUBSTRING "${_arpg_check}" ${_arpg_pattern_start} -1 _arpg_pattern)
+        if(SOURCE_TEXT MATCHES "${_arpg_pattern}")
             set("${OUT_FOUND}" TRUE PARENT_SCOPE)
-            set("${OUT_FRAGMENT}" "${_arpg_fragment}" PARENT_SCOPE)
+            set("${OUT_LABEL}" "${_arpg_label}" PARENT_SCOPE)
             return()
         endif()
     endforeach()
     set("${OUT_FOUND}" FALSE PARENT_SCOPE)
-    set("${OUT_FRAGMENT}" "" PARENT_SCOPE)
+    set("${OUT_LABEL}" "" PARENT_SCOPE)
 endfunction()
 
-foreach(_arpg_mutation IN ITEMS
+foreach(_arpg_allowed_include IN ITEMS
+        "#include \"pause_menu_state.hpp\""
+        "  # include \"platform/settings/settings_types.hpp\"  "
+        "#include <cstddef>"
+        "#include <cstdint>"
+        "#include <optional>"
+        "#include <limits>"
+        "// #include <raylib.h>")
+    arpg_pause_includes_are_allowed(
+        "${_arpg_allowed_include}" _arpg_allowed _arpg_bad_include)
+    if(NOT _arpg_allowed)
+        message(FATAL_ERROR
+            "Pause state include guard rejected clean control: ${_arpg_allowed_include}")
+    endif()
+endforeach()
+
+foreach(_arpg_forbidden_include IN ITEMS
         "#include <raylib.h>"
-        "SettingsStore* store;"
-        "Combat* combat;"
-        "Dungeon* dungeon;"
+        "#include \"combat/combat.hpp\""
+        "#include \"dungeon/dungeon.hpp\""
         "#include <filesystem>"
-        "std::ifstream input;")
-    arpg_pause_source_has_forbidden_dependency(
-        "${_arpg_mutation}" _arpg_found _arpg_fragment)
+        "#include <fstream>"
+        "#include <cstdio>"
+        "#include <windows.h>")
+    arpg_pause_includes_are_allowed(
+        "${_arpg_forbidden_include}" _arpg_allowed _arpg_bad_include)
+    if(_arpg_allowed)
+        message(FATAL_ERROR
+            "Pause state include guard missed mutation: ${_arpg_forbidden_include}")
+    endif()
+endforeach()
+
+foreach(_arpg_mutation IN ITEMS
+        "IsKeyPressed(1);"
+        "SetWindowState(1);"
+        "SettingsStore* store;"
+        "auto settings_store = 0;"
+        "combat::CombatState combat_state;"
+        "dungeon::DungeonSession session;"
+        "filesystem::path save_path;"
+        "std::ifstream input;"
+        "FILE* handle;"
+        "fopen(path, mode);"
+        "CreateFileW(path, 0, 0, 0, 0, 0, 0);"
+        "ReadFile(handle, buffer, size, count, 0);")
+    arpg_pause_source_has_forbidden_api(
+        "${_arpg_mutation}" _arpg_found _arpg_label)
     if(NOT _arpg_found)
         message(FATAL_ERROR
-            "Pause state boundary self-test missed mutation: ${_arpg_mutation}")
+            "Pause state API guard missed mutation: ${_arpg_mutation}")
+    endif()
+endforeach()
+
+foreach(_arpg_clean_control IN ITEMS
+        "// raylib combat dungeon file are architecture words"
+        "constexpr char message[] = \"raylib combat dungeon file\";"
+        "int raylibish_combatant_dungeoned_file_id = 0;"
+        "SettingsStorefront settings_storefront;"
+        "combative::State dungeoned::State;"
+        "filesystemish::path profile;"
+        "int fstreaming_ifstreamed_ofstreamed = 0;"
+        "int FILESYSTEM_FILE_ID = 0;"
+        "auto fopenly_freader_fwriter_fcloser = 0;"
+        "auto CreateFileName_ReadFileName_WriteFileName = 0;"
+        "auto IsKeyPressedMessage_SetWindowStateCache = 0;")
+    arpg_pause_source_has_forbidden_api(
+        "${_arpg_clean_control}" _arpg_found _arpg_label)
+    if(_arpg_found)
+        message(FATAL_ERROR
+            "Pause state API guard rejected clean control '${_arpg_label}': ${_arpg_clean_control}")
     endif()
 endforeach()
 
 foreach(_arpg_source IN ITEMS "${PAUSE_STATE_HEADER}" "${PAUSE_STATE_SOURCE}")
     if(NOT EXISTS "${_arpg_source}")
         message(FATAL_ERROR "Pause state source missing: ${_arpg_source}")
     endif()
     file(READ "${_arpg_source}" _arpg_source_text)
-    arpg_pause_source_has_forbidden_dependency(
-        "${_arpg_source_text}" _arpg_found _arpg_fragment)
+    arpg_pause_includes_are_allowed(
+        "${_arpg_source_text}" _arpg_allowed _arpg_bad_include)
+    if(NOT _arpg_allowed)
+        message(FATAL_ERROR
+            "Pause state source has forbidden include '${_arpg_bad_include}': ${_arpg_source}")
+    endif()
+    arpg_pause_source_has_forbidden_api(
+        "${_arpg_source_text}" _arpg_found _arpg_label)
     if(_arpg_found)
         message(FATAL_ERROR
-            "Pause state source contains forbidden dependency '${_arpg_fragment}': ${_arpg_source}")
+            "Pause state source contains forbidden ${_arpg_label}: ${_arpg_source}")
     endif()
 endforeach()
diff --git a/tests/platform/pause_menu_state_tests.cpp b/tests/platform/pause_menu_state_tests.cpp
index 402c6dd..4fa8f0b 100644
--- a/tests/platform/pause_menu_state_tests.cpp
+++ b/tests/platform/pause_menu_state_tests.cpp
@@ -468,30 +468,175 @@ test::Failure non_action_inputs_do_not_mutate_settings() noexcept {
     const auto original = state.draft;
     ARPG_REQUIRE(update(state, capture_input(settings::StableKey::q)) ==
         platform::PauseCommand::none);
     ARPG_REQUIRE(state.screen == platform::PauseScreen::settings);
     ARPG_REQUIRE(state.draft.bindings == original.bindings);
     return {};
 }
 
 test::Failure state_machine_performs_no_heap_allocations() noexcept {
     auto state = make_state();
+    constexpr ContextOwner kBlockingOwners[] = {
+        ContextOwner::death,
+        ContextOwner::recovery,
+        ContextOwner::inventory,
+        ContextOwner::passive,
+        ContextOwner::pending_save,
+    };
+    constexpr char kRetryMessage[] = "retry save";
     const std::uint64_t before = test::allocation_count();
-    for (int iteration = 0; iteration < 1000; ++iteration) {
-        state.screen = platform::PauseScreen::closed;
+
+    for (std::uint64_t iteration = 0U; iteration < 64U; ++iteration) {
+        for (const ContextOwner owner : kBlockingOwners) {
+            state = make_state();
+            ARPG_REQUIRE(update(
+                state, input_for(InputKind::escape), context_for(owner)) ==
+                platform::PauseCommand::none);
+            ARPG_REQUIRE(state.screen == platform::PauseScreen::closed);
+        }
+
+        state = make_state();
         ARPG_REQUIRE(update(state, input_for(InputKind::escape)) == platform::PauseCommand::none);
+        ARPG_REQUIRE(state.screen == platform::PauseScreen::root);
+
+        state = make_state();
+        state.screen = platform::PauseScreen::root;
+        ARPG_REQUIRE(update(state, input_for(InputKind::up)) == platform::PauseCommand::none);
+        ARPG_REQUIRE(state.selected_row == 2U);
+        ARPG_REQUIRE(update(state, input_for(InputKind::down)) == platform::PauseCommand::none);
+        ARPG_REQUIRE(state.selected_row == 0U);
+        ARPG_REQUIRE(update(state, input_for(InputKind::enter)) == platform::PauseCommand::resume);
+
+        state = make_state();
+        state.screen = platform::PauseScreen::root;
         state.selected_row = 1U;
         ARPG_REQUIRE(update(state, input_for(InputKind::activate)) == platform::PauseCommand::none);
+        ARPG_REQUIRE(state.screen == platform::PauseScreen::settings);
+
+        state = make_state();
+        state.screen = platform::PauseScreen::root;
+        state.selected_row = 2U;
+        ARPG_REQUIRE(update(state, input_for(InputKind::activate)) == platform::PauseCommand::none);
+        ARPG_REQUIRE(state.screen == platform::PauseScreen::quit_confirm);
+        ARPG_REQUIRE(state.selected_row == 1U);
+
+        state = make_state();
+        state.screen = platform::PauseScreen::settings;
+        state.selected_row = 0U;
+        ARPG_REQUIRE(update(state, input_for(InputKind::left)) == platform::PauseCommand::preview);
+
+        state = make_state();
+        state.screen = platform::PauseScreen::settings;
+        state.selected_row = 1U;
+        ARPG_REQUIRE(update(state, input_for(InputKind::right)) == platform::PauseCommand::preview);
+
+        state = make_state();
+        state.screen = platform::PauseScreen::settings;
+        state.selected_row = 2U;
+        ARPG_REQUIRE(update(state, input_for(InputKind::activate)) == platform::PauseCommand::preview);
+
+        state = make_state();
+        state.screen = platform::PauseScreen::settings;
+        state.selected_row = 13U;
+        state.draft.master_sfx_percent = 50U;
+        state.draft.revision = iteration;
+        ARPG_REQUIRE(update(state, input_for(InputKind::activate)) == platform::PauseCommand::preview);
+        ARPG_REQUIRE(state.draft.revision == iteration);
+
+        state = make_state();
+        state.screen = platform::PauseScreen::settings;
+        state.selected_row = 14U;
+        state.committed.revision = iteration;
+        state.draft = state.committed;
+        state.message = kRetryMessage;
+        ARPG_REQUIRE(update(state, input_for(InputKind::activate)) == platform::PauseCommand::apply);
+        ARPG_REQUIRE(state.draft.revision == iteration + 1U);
+        ARPG_REQUIRE(state.message == kRetryMessage);
+
+        state = make_state();
+        state.screen = platform::PauseScreen::settings;
+        state.selected_row = 14U;
+        state.draft.master_sfx_percent = 53U;
+        ARPG_REQUIRE(update(state, input_for(InputKind::activate)) == platform::PauseCommand::none);
+        ARPG_REQUIRE(state.message != nullptr);
+
+        state = make_state();
+        state.screen = platform::PauseScreen::settings;
+        state.selected_row = 14U;
+        state.committed.revision = std::numeric_limits<std::uint64_t>::max();
+        ARPG_REQUIRE(update(state, input_for(InputKind::activate)) == platform::PauseCommand::none);
+        ARPG_REQUIRE(state.message != nullptr);
+
+        state = make_state();
+        state.screen = platform::PauseScreen::settings;
+        state.selected_row = 15U;
+        state.draft.master_sfx_percent = 25U;
+        ARPG_REQUIRE(update(state, input_for(InputKind::activate)) == platform::PauseCommand::rollback);
+        ARPG_REQUIRE(state.screen == platform::PauseScreen::root);
+
+        for (std::size_t action = 0U;
+             action < static_cast<std::size_t>(settings::SettingAction::count);
+             ++action) {
+            state = make_state();
+            state.screen = platform::PauseScreen::capture_binding;
+            state.capture_action = static_cast<settings::SettingAction>(action);
+            ARPG_REQUIRE(update(state, capture_input(settings::StableKey::q)) ==
+                platform::PauseCommand::none);
+            ARPG_REQUIRE(state.screen == platform::PauseScreen::settings);
+        }
+
+        state = make_state();
+        state.screen = platform::PauseScreen::capture_binding;
+        state.capture_action = settings::SettingAction::light_attack;
+        ARPG_REQUIRE(update(state, capture_input(settings::StableKey::k)) ==
+            platform::PauseCommand::none);
+        ARPG_REQUIRE(settings::binding_for(
+            state.draft, settings::SettingAction::jump) == settings::StableKey::j);
+
+        state = make_state();
+        state.screen = platform::PauseScreen::capture_binding;
+        state.capture_action = settings::SettingAction::light_attack;
+        ARPG_REQUIRE(update(state, capture_input(settings::StableKey::v)) ==
+            platform::PauseCommand::none);
+        ARPG_REQUIRE(state.screen == platform::PauseScreen::capture_binding);
+
+        state = make_state();
+        state.screen = platform::PauseScreen::capture_binding;
+        state.capture_action = settings::SettingAction::light_attack;
+        ARPG_REQUIRE(update(state, capture_input(settings::StableKey::count)) ==
+            platform::PauseCommand::none);
+        ARPG_REQUIRE(state.screen == platform::PauseScreen::capture_binding);
+
+        state = make_state();
+        state.screen = platform::PauseScreen::capture_binding;
+        state.capture_action = settings::SettingAction::light_attack;
+        ARPG_REQUIRE(update(state, {}, unfocused_context()) == platform::PauseCommand::none);
+        ARPG_REQUIRE(state.screen == platform::PauseScreen::settings);
+
+        state = make_state();
+        state.screen = platform::PauseScreen::capture_binding;
+        state.capture_action = settings::SettingAction::light_attack;
+        ARPG_REQUIRE(update(state, input_for(InputKind::escape)) == platform::PauseCommand::none);
+        ARPG_REQUIRE(state.screen == platform::PauseScreen::settings);
+
+        state = make_state();
+        state.screen = platform::PauseScreen::quit_confirm;
         state.selected_row = 0U;
-        ARPG_REQUIRE(update(state, input_for(InputKind::left)) != platform::PauseCommand::apply);
-        ARPG_REQUIRE(update(state, input_for(InputKind::escape)) == platform::PauseCommand::rollback);
+        ARPG_REQUIRE(update(state, input_for(InputKind::enter)) == platform::PauseCommand::quit);
+
+        state = make_state();
+        state.screen = platform::PauseScreen::quit_confirm;
+        state.selected_row = 1U;
+        ARPG_REQUIRE(update(state, input_for(InputKind::activate)) == platform::PauseCommand::none);
+        ARPG_REQUIRE(state.screen == platform::PauseScreen::root);
     }
+
     ARPG_REQUIRE(test::allocation_count() == before);
     return {};
 }
 
 constexpr test::TestCase kCases[] = {
     {"closed is idle without escape", &closed_is_idle_without_escape},
     {"higher priority contexts block pause", &higher_priority_contexts_block_pause_opening},
     {"opening drops same-frame inputs", &opening_pause_drops_same_frame_inputs},
     {"root navigation wraps", &root_navigation_wraps_three_rows},
     {"root continue uses enter and escape", &root_continue_supports_enter_and_escape},
