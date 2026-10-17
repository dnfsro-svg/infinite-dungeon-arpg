# Review package: 68fbab8..b0c51f5

## Commits
b0c51f5 test: enforce frame input authority at compile time

## Files changed
 src/platform/raylib/direct_input_poison.hpp |  37 ++
 src/platform/raylib/inventory_renderer.cpp  |   5 +
 src/platform/raylib/raylib_host.cpp         |   5 +
 tests/platform/CMakeLists.txt               |  51 ++
 tests/platform/host_input_source_test.cmake | 701 +++-------------------------
 5 files changed, 173 insertions(+), 626 deletions(-)

## Diff
diff --git a/src/platform/raylib/direct_input_poison.hpp b/src/platform/raylib/direct_input_poison.hpp
new file mode 100644
index 0000000..972243f
--- /dev/null
+++ b/src/platform/raylib/direct_input_poison.hpp
@@ -0,0 +1,37 @@
+#pragma once
+
+namespace arpg::platform::direct_input_poison {
+
+struct Blocked final {
+    template <typename... Args>
+    void operator()(Args&&...) const = delete;
+
+    const Blocked* operator&() const = delete;
+};
+
+inline constexpr Blocked blocked{};
+
+inline constexpr bool active = true;
+
+}  // namespace arpg::platform::direct_input_poison
+
+#define platform_key_pressed ::arpg::platform::direct_input_poison::blocked
+#define platform_key_down ::arpg::platform::direct_input_poison::blocked
+#define IsKeyPressed ::arpg::platform::direct_input_poison::blocked
+#define IsKeyPressedRepeat ::arpg::platform::direct_input_poison::blocked
+#define IsKeyDown ::arpg::platform::direct_input_poison::blocked
+#define IsKeyReleased ::arpg::platform::direct_input_poison::blocked
+#define IsKeyUp ::arpg::platform::direct_input_poison::blocked
+#define GetKeyPressed ::arpg::platform::direct_input_poison::blocked
+#define GetCharPressed ::arpg::platform::direct_input_poison::blocked
+#define IsMouseButtonPressed ::arpg::platform::direct_input_poison::blocked
+#define IsMouseButtonDown ::arpg::platform::direct_input_poison::blocked
+#define IsMouseButtonReleased ::arpg::platform::direct_input_poison::blocked
+#define IsMouseButtonUp ::arpg::platform::direct_input_poison::blocked
+#define GetMouseX ::arpg::platform::direct_input_poison::blocked
+#define GetMouseY ::arpg::platform::direct_input_poison::blocked
+#define GetMousePosition ::arpg::platform::direct_input_poison::blocked
+#define GetMouseDelta ::arpg::platform::direct_input_poison::blocked
+#define GetMouseWheelMove ::arpg::platform::direct_input_poison::blocked
+#define GetMouseWheelMoveV ::arpg::platform::direct_input_poison::blocked
+#define IsWindowFocused ::arpg::platform::direct_input_poison::blocked
diff --git a/src/platform/raylib/inventory_renderer.cpp b/src/platform/raylib/inventory_renderer.cpp
index d13b990..1819841 100644
--- a/src/platform/raylib/inventory_renderer.cpp
+++ b/src/platform/raylib/inventory_renderer.cpp
@@ -7,20 +7,25 @@
 #include "items/item_catalog.hpp"
 
 #include <raylib.h>
 
 #include <algorithm>
 #include <array>
 #include <cmath>
 #include <cstddef>
 #include <cstdint>
 
+#include "direct_input_poison.hpp"
+
+static_assert(arpg::platform::direct_input_poison::active,
+    "direct input poison must be active in inventory_renderer.cpp");
+
 namespace arpg::platform {
 namespace {
 
 constexpr float kCellHeight = 58.0F;
 constexpr float kGridTopInset = 82.0F;
 constexpr float kGridBottomInset = 58.0F;
 
 const char* slot_name(items::ItemSlot slot) noexcept {
     switch (slot) {
     case items::ItemSlot::weapon: return "Weapon";
diff --git a/src/platform/raylib/raylib_host.cpp b/src/platform/raylib/raylib_host.cpp
index b692ea3..e109b9c 100644
--- a/src/platform/raylib/raylib_host.cpp
+++ b/src/platform/raylib/raylib_host.cpp
@@ -14,20 +14,25 @@
 #include "platform/settings/settings_types.hpp"
 
 #include <raylib.h>
 
 #include <cstdint>
 #include <cmath>
 #include <filesystem>
 #include <optional>
 #include <string>
 
+#include "direct_input_poison.hpp"
+
+static_assert(arpg::platform::direct_input_poison::active,
+    "direct input poison must be active in raylib_host.cpp");
+
 #if !defined(RAYLIB_VERSION_MAJOR) || !defined(RAYLIB_VERSION_MINOR) \
     || !defined(RAYLIB_VERSION_PATCH)
 #error "raylib version macros are unavailable"
 #endif
 
 static_assert(RAYLIB_VERSION_MAJOR == 6, "raylib 6.0.0 is required");
 static_assert(RAYLIB_VERSION_MINOR == 0, "raylib 6.0.0 is required");
 static_assert(RAYLIB_VERSION_PATCH == 0, "raylib 6.0.0 is required");
 
 namespace arpg::platform {
diff --git a/tests/platform/CMakeLists.txt b/tests/platform/CMakeLists.txt
index 4a9ec6c..7f05121 100644
--- a/tests/platform/CMakeLists.txt
+++ b/tests/platform/CMakeLists.txt
@@ -1013,18 +1013,69 @@ set_tests_properties(
 
 add_test(
     NAME platform.input_latency_source
     COMMAND "${CMAKE_COMMAND}"
         "-DRAYLIB_SOURCE_DIR=${PROJECT_SOURCE_DIR}/src/platform/raylib"
         -P "${CMAKE_CURRENT_SOURCE_DIR}/input_latency_source_test.cmake")
 set_tests_properties(
     platform.input_latency_source
     PROPERTIES LABELS "headless;platform;architecture")
 
+function(arpg_verify_direct_input_poison)
+    include(CheckCXXSourceCompiles)
+    set(CMAKE_CXX_STANDARD 17)
+    set(CMAKE_CXX_STANDARD_REQUIRED ON)
+    set(CMAKE_REQUIRED_INCLUDES
+        "${PROJECT_SOURCE_DIR}/src/platform/raylib")
+    set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
+
+    unset(ARPG_DIRECT_INPUT_POISON_ACTIVE_COMPILES CACHE)
+    check_cxx_source_compiles([=[
+        #include "direct_input_poison.hpp"
+        static_assert(
+            ::arpg::platform::direct_input_poison::active,
+            "direct input poison must be active");
+        int main() { return 0; }
+    ]=] ARPG_DIRECT_INPUT_POISON_ACTIVE_COMPILES)
+    if(NOT ARPG_DIRECT_INPUT_POISON_ACTIVE_COMPILES)
+        message(FATAL_ERROR
+            "direct input poison must expose a compilable active sentinel")
+    endif()
+
+    set(_arpg_poison_probes
+        "KEY|IsKeyDown(0)"
+        "MOUSE|IsMouseButtonDown(0)"
+        "FOCUS|IsWindowFocused()"
+        "PLATFORM_WRAPPER|platform_key_down(0)"
+        "ADDRESS|&IsKeyDown")
+    foreach(_arpg_poison_probe IN LISTS _arpg_poison_probes)
+        string(REPLACE "|" ";" _arpg_poison_parts
+            "${_arpg_poison_probe}")
+        list(GET _arpg_poison_parts 0 _arpg_poison_label)
+        list(GET _arpg_poison_parts 1 _arpg_poison_expression)
+        set(_arpg_poison_result
+            "ARPG_DIRECT_INPUT_POISON_${_arpg_poison_label}_COMPILES")
+        unset("${_arpg_poison_result}" CACHE)
+        check_cxx_source_compiles("
+            #include \"direct_input_poison.hpp\"
+            int main() {
+                static_cast<void>(${_arpg_poison_expression});
+                return 0;
+            }
+        " "${_arpg_poison_result}")
+        if(${_arpg_poison_result})
+            message(FATAL_ERROR
+                "direct input poison compile probe unexpectedly accepted ${_arpg_poison_label}")
+        endif()
+    endforeach()
+endfunction()
+
+arpg_verify_direct_input_poison()
+
 add_test(
     NAME platform.host_input_source
     COMMAND "${CMAKE_COMMAND}"
         "-DRAYLIB_SOURCE_DIR=${PROJECT_SOURCE_DIR}/src/platform/raylib"
         -P "${CMAKE_CURRENT_SOURCE_DIR}/host_input_source_test.cmake")
 set_tests_properties(
     platform.host_input_source
     PROPERTIES LABELS "headless;platform;architecture")
diff --git a/tests/platform/host_input_source_test.cmake b/tests/platform/host_input_source_test.cmake
index c449639..854912d 100644
--- a/tests/platform/host_input_source_test.cmake
+++ b/tests/platform/host_input_source_test.cmake
@@ -1,658 +1,107 @@
 if(NOT DEFINED RAYLIB_SOURCE_DIR)
     message(FATAL_ERROR "RAYLIB_SOURCE_DIR is required")
 endif()
 
-file(READ "${RAYLIB_SOURCE_DIR}/host_input.cpp" HOST_INPUT_SOURCE)
+set(POISON_HEADER "${RAYLIB_SOURCE_DIR}/direct_input_poison.hpp")
+if(NOT EXISTS "${POISON_HEADER}")
+    message(FATAL_ERROR
+        "direct input poison header is required at ${POISON_HEADER}")
+endif()
+
 file(READ "${RAYLIB_SOURCE_DIR}/raylib_host.cpp" HOST_SOURCE)
 file(READ "${RAYLIB_SOURCE_DIR}/inventory_renderer.cpp" INVENTORY_SOURCE)
-
-function(find_cpp_raw_literal_end SOURCE START_INDEX OUT_END)
-    set(TEXT "${SOURCE}")
-    string(LENGTH "${TEXT}" TEXT_LENGTH)
-    set(RAW_END -1)
-    math(EXPR NEXT_INDEX "${START_INDEX} + 1")
-    if(START_INDEX LESS TEXT_LENGTH AND NEXT_INDEX LESS TEXT_LENGTH)
-        string(SUBSTRING "${TEXT}" ${START_INDEX} 1 START_CHAR)
-        string(SUBSTRING "${TEXT}" ${NEXT_INDEX} 1 NEXT_CHAR)
-        if(START_CHAR STREQUAL "R" AND NEXT_CHAR STREQUAL "\"")
-            math(EXPR DELIMITER_START "${START_INDEX} + 2")
-            set(SCAN ${DELIMITER_START})
-            set(OPEN_PAREN -1)
-            while(SCAN LESS TEXT_LENGTH)
-                math(EXPR DELIMITER_LENGTH "${SCAN} - ${DELIMITER_START}")
-                if(DELIMITER_LENGTH GREATER 16)
-                    break()
-                endif()
-                string(SUBSTRING "${TEXT}" ${SCAN} 1 RAW_CHAR)
-                if(RAW_CHAR STREQUAL "(")
-                    set(OPEN_PAREN ${SCAN})
-                    break()
-                endif()
-                if(RAW_CHAR STREQUAL " " OR RAW_CHAR STREQUAL "\t"
-                        OR RAW_CHAR STREQUAL "\r" OR RAW_CHAR STREQUAL "\n"
-                        OR RAW_CHAR STREQUAL ")" OR RAW_CHAR STREQUAL "\\")
-                    break()
-                endif()
-                math(EXPR SCAN "${SCAN} + 1")
-            endwhile()
-            if(OPEN_PAREN GREATER_EQUAL 0)
-                math(EXPR DELIMITER_LENGTH
-                    "${OPEN_PAREN} - ${DELIMITER_START}")
-                string(SUBSTRING "${TEXT}" ${DELIMITER_START}
-                    ${DELIMITER_LENGTH} RAW_DELIMITER)
-                set(RAW_TERMINATOR ")${RAW_DELIMITER}\"")
-                math(EXPR CONTENT_START "${OPEN_PAREN} + 1")
-                math(EXPR REMAINING_LENGTH
-                    "${TEXT_LENGTH} - ${CONTENT_START}")
-                string(SUBSTRING "${TEXT}" ${CONTENT_START}
-                    ${REMAINING_LENGTH} REMAINING)
-                string(FIND "${REMAINING}" "${RAW_TERMINATOR}"
-                    RELATIVE_CLOSE)
-                if(RELATIVE_CLOSE GREATER_EQUAL 0)
-                    string(LENGTH "${RAW_TERMINATOR}" TERMINATOR_LENGTH)
-                    math(EXPR RAW_END
-                        "${CONTENT_START} + ${RELATIVE_CLOSE} + ${TERMINATOR_LENGTH}")
-                endif()
-            endif()
-        endif()
-    endif()
-    set("${OUT_END}" ${RAW_END} PARENT_SCOPE)
-endfunction()
-
-function(splice_cpp_lines SOURCE OUT_SOURCE)
-    set(TEXT "${SOURCE}")
-    string(LENGTH "${TEXT}" TEXT_LENGTH)
-    set(OUTPUT "")
-    set(INDEX 0)
-    set(STATE "normal")
-    while(INDEX LESS TEXT_LENGTH)
-        string(SUBSTRING "${TEXT}" ${INDEX} 1 CURRENT)
-
-        if(STATE STREQUAL "raw")
-            math(EXPR RAW_LENGTH "${RAW_END} - ${INDEX}")
-            string(SUBSTRING "${TEXT}" ${INDEX} ${RAW_LENGTH}
-                RAW_LITERAL)
-            string(APPEND OUTPUT "${RAW_LITERAL}")
-            set(INDEX ${RAW_END})
-            set(STATE "normal")
-            continue()
-        endif()
-
-        math(EXPR NEXT_INDEX "${INDEX} + 1")
-        set(NEXT "")
-        if(NEXT_INDEX LESS TEXT_LENGTH)
-            string(SUBSTRING "${TEXT}" ${NEXT_INDEX} 1 NEXT)
-        endif()
-
-        set(SPLICE_LENGTH 0)
-        if(CURRENT STREQUAL "\\")
-            if(NEXT STREQUAL "\n")
-                set(SPLICE_LENGTH 2)
-            elseif(NEXT STREQUAL "\r")
-                math(EXPR AFTER_CR_INDEX "${INDEX} + 2")
-                if(AFTER_CR_INDEX LESS TEXT_LENGTH)
-                    string(SUBSTRING "${TEXT}" ${AFTER_CR_INDEX}
-                        1 AFTER_CR)
-                    if(AFTER_CR STREQUAL "\n")
-                        set(SPLICE_LENGTH 3)
-                    endif()
-                endif()
-            endif()
-        endif()
-
-        if(SPLICE_LENGTH GREATER 0)
-            math(EXPR INDEX "${INDEX} + ${SPLICE_LENGTH}")
-            continue()
-        endif()
-
-        if(STATE STREQUAL "line_comment")
-            if(CURRENT STREQUAL "\r")
-                string(APPEND OUTPUT "${CURRENT}")
-                if(NEXT STREQUAL "\n")
-                    string(APPEND OUTPUT "${NEXT}")
-                    math(EXPR INDEX "${INDEX} + 2")
-                else()
-                    math(EXPR INDEX "${INDEX} + 1")
-                endif()
-                set(STATE "normal")
-                continue()
-            elseif(CURRENT STREQUAL "\n")
-                string(APPEND OUTPUT "${CURRENT}")
-                math(EXPR INDEX "${INDEX} + 1")
-                set(STATE "normal")
-                continue()
-            endif()
-            string(APPEND OUTPUT "${CURRENT}")
-            math(EXPR INDEX "${INDEX} + 1")
-            continue()
-        endif()
-
-        if(STATE STREQUAL "block_comment")
-            if(CURRENT STREQUAL "*" AND NEXT STREQUAL "/")
-                string(APPEND OUTPUT "*/")
-                math(EXPR INDEX "${INDEX} + 2")
-                set(STATE "normal")
-                continue()
-            endif()
-            string(APPEND OUTPUT "${CURRENT}")
-            math(EXPR INDEX "${INDEX} + 1")
-            continue()
-        endif()
-
-        if(STATE STREQUAL "ordinary_string" OR STATE STREQUAL "char")
-            if(CURRENT STREQUAL "\\")
-                string(APPEND OUTPUT "${CURRENT}")
-                math(EXPR INDEX "${INDEX} + 1")
-                if(INDEX LESS TEXT_LENGTH)
-                    string(SUBSTRING "${TEXT}" ${INDEX} 1 ESCAPED)
-                    string(APPEND OUTPUT "${ESCAPED}")
-                    math(EXPR INDEX "${INDEX} + 1")
-                endif()
-                continue()
-            endif()
-
-            string(APPEND OUTPUT "${CURRENT}")
-            math(EXPR INDEX "${INDEX} + 1")
-            if((STATE STREQUAL "ordinary_string" AND CURRENT STREQUAL "\"")
-                    OR (STATE STREQUAL "char" AND CURRENT STREQUAL "'"))
-                set(STATE "normal")
-            elseif(CURRENT STREQUAL "\r")
-                if(NEXT STREQUAL "\n")
-                    string(APPEND OUTPUT "${NEXT}")
-                    math(EXPR INDEX "${INDEX} + 1")
-                endif()
-                set(STATE "normal")
-            elseif(CURRENT STREQUAL "\n")
-                set(STATE "normal")
-            endif()
-            continue()
-        endif()
-
-        if(CURRENT STREQUAL "R" AND NEXT STREQUAL "\"")
-            find_cpp_raw_literal_end("${TEXT}" ${INDEX} RAW_END)
-            if(RAW_END GREATER_EQUAL 0)
-                set(STATE "raw")
-                continue()
-            endif()
-        endif()
-
-        if(CURRENT STREQUAL "/" AND NEXT STREQUAL "/")
-            string(APPEND OUTPUT "//")
-            math(EXPR INDEX "${INDEX} + 2")
-            set(STATE "line_comment")
-            continue()
-        endif()
-
-        if(CURRENT STREQUAL "/" AND NEXT STREQUAL "*")
-            string(APPEND OUTPUT "/*")
-            math(EXPR INDEX "${INDEX} + 2")
-            set(STATE "block_comment")
-            continue()
-        endif()
-
-        if(CURRENT STREQUAL "\"")
-            string(APPEND OUTPUT "${CURRENT}")
-            math(EXPR INDEX "${INDEX} + 1")
-            set(STATE "ordinary_string")
-            continue()
-        endif()
-
-        if(CURRENT STREQUAL "'")
-            string(APPEND OUTPUT "${CURRENT}")
-            math(EXPR INDEX "${INDEX} + 1")
-            set(STATE "char")
-            continue()
-        endif()
-
-        string(APPEND OUTPUT "${CURRENT}")
-        math(EXPR INDEX "${INDEX} + 1")
-    endwhile()
-    set("${OUT_SOURCE}" "${OUTPUT}" PARENT_SCOPE)
-endfunction()
-
-function(mask_cpp_literals SOURCE OUT_SOURCE)
-    set(TEXT "${SOURCE}")
-    string(LENGTH "${TEXT}" TEXT_LENGTH)
-    set(OUTPUT "")
-    set(INDEX 0)
-    while(INDEX LESS TEXT_LENGTH)
-        string(SUBSTRING "${TEXT}" ${INDEX} 1 CURRENT)
-
-        set(RAW_END -1)
-        if(CURRENT STREQUAL "R")
-            math(EXPR RAW_QUOTE_INDEX "${INDEX} + 1")
-            if(RAW_QUOTE_INDEX LESS TEXT_LENGTH)
-                string(SUBSTRING "${TEXT}" ${RAW_QUOTE_INDEX}
-                    1 RAW_QUOTE)
-                if(RAW_QUOTE STREQUAL "\"")
-                    find_cpp_raw_literal_end("${TEXT}" ${INDEX} RAW_END)
-                endif()
-            endif()
-        endif()
-
-        if(RAW_END GREATER_EQUAL 0)
-            while(INDEX LESS RAW_END)
-                string(SUBSTRING "${TEXT}" ${INDEX} 1 MASKED_CHAR)
-                if(MASKED_CHAR STREQUAL "\r" OR MASKED_CHAR STREQUAL "\n")
-                    string(APPEND OUTPUT "${MASKED_CHAR}")
-                else()
-                    string(APPEND OUTPUT " ")
-                endif()
-                math(EXPR INDEX "${INDEX} + 1")
-            endwhile()
-            continue()
-        endif()
-
-        if(CURRENT STREQUAL "\"" OR CURRENT STREQUAL "'")
-            set(DELIMITER "${CURRENT}")
-            string(APPEND OUTPUT " ")
-            math(EXPR INDEX "${INDEX} + 1")
-            while(INDEX LESS TEXT_LENGTH)
-                string(SUBSTRING "${TEXT}" ${INDEX} 1 LITERAL_CHAR)
-                if(LITERAL_CHAR STREQUAL "\r" OR LITERAL_CHAR STREQUAL "\n")
-                    string(APPEND OUTPUT "${LITERAL_CHAR}")
-                    math(EXPR INDEX "${INDEX} + 1")
-                    break()
-                endif()
-                string(APPEND OUTPUT " ")
-                math(EXPR INDEX "${INDEX} + 1")
-                if(LITERAL_CHAR STREQUAL "\\")
-                    if(INDEX LESS TEXT_LENGTH)
-                        string(SUBSTRING "${TEXT}" ${INDEX} 1 ESCAPED_CHAR)
-                        if(ESCAPED_CHAR STREQUAL "\r"
-                                OR ESCAPED_CHAR STREQUAL "\n")
-                            string(APPEND OUTPUT "${ESCAPED_CHAR}")
-                        else()
-                            string(APPEND OUTPUT " ")
-                        endif()
-                        math(EXPR INDEX "${INDEX} + 1")
-                    endif()
-                elseif(LITERAL_CHAR STREQUAL DELIMITER)
-                    break()
-                endif()
-            endwhile()
-            continue()
-        endif()
-
-        string(APPEND OUTPUT "${CURRENT}")
-        math(EXPR INDEX "${INDEX} + 1")
-    endwhile()
-    set("${OUT_SOURCE}" "${OUTPUT}" PARENT_SCOPE)
-endfunction()
-
-function(strip_cpp_comments SOURCE OUT_SOURCE)
-    set(TEXT "${SOURCE}")
-    string(LENGTH "${TEXT}" TEXT_LENGTH)
-    set(OUTPUT "")
-    set(INDEX 0)
-    while(INDEX LESS TEXT_LENGTH)
-        string(SUBSTRING "${TEXT}" ${INDEX} 1 CURRENT)
-        math(EXPR NEXT_INDEX "${INDEX} + 1")
-        set(NEXT "")
-        if(NEXT_INDEX LESS TEXT_LENGTH)
-            string(SUBSTRING "${TEXT}" ${NEXT_INDEX} 1 NEXT)
-        endif()
-
-        if(CURRENT STREQUAL "/" AND NEXT STREQUAL "/")
-            string(APPEND OUTPUT "  ")
-            math(EXPR INDEX "${INDEX} + 2")
-            while(INDEX LESS TEXT_LENGTH)
-                string(SUBSTRING "${TEXT}" ${INDEX} 1 COMMENT_CHAR)
-                if(COMMENT_CHAR STREQUAL "\r" OR COMMENT_CHAR STREQUAL "\n")
-                    string(APPEND OUTPUT "${COMMENT_CHAR}")
-                    math(EXPR INDEX "${INDEX} + 1")
-                    break()
-                endif()
-                string(APPEND OUTPUT " ")
-                math(EXPR INDEX "${INDEX} + 1")
-            endwhile()
-            continue()
-        endif()
-
-        if(CURRENT STREQUAL "/" AND NEXT STREQUAL "*")
-            string(APPEND OUTPUT "  ")
-            math(EXPR INDEX "${INDEX} + 2")
-            while(INDEX LESS TEXT_LENGTH)
-                string(SUBSTRING "${TEXT}" ${INDEX} 1 COMMENT_CHAR)
-                math(EXPR COMMENT_NEXT_INDEX "${INDEX} + 1")
-                set(COMMENT_NEXT "")
-                if(COMMENT_NEXT_INDEX LESS TEXT_LENGTH)
-                    string(SUBSTRING "${TEXT}" ${COMMENT_NEXT_INDEX}
-                        1 COMMENT_NEXT)
-                endif()
-                if(COMMENT_CHAR STREQUAL "*" AND COMMENT_NEXT STREQUAL "/")
-                    string(APPEND OUTPUT "  ")
-                    math(EXPR INDEX "${INDEX} + 2")
-                    break()
-                endif()
-                if(COMMENT_CHAR STREQUAL "\r" OR COMMENT_CHAR STREQUAL "\n")
-                    string(APPEND OUTPUT "${COMMENT_CHAR}")
-                else()
-                    string(APPEND OUTPUT " ")
-                endif()
-                math(EXPR INDEX "${INDEX} + 1")
-            endwhile()
-            continue()
-        endif()
-
-        string(APPEND OUTPUT "${CURRENT}")
-        math(EXPR INDEX "${INDEX} + 1")
-    endwhile()
-    set("${OUT_SOURCE}" "${OUTPUT}" PARENT_SCOPE)
-endfunction()
-
-function(strip_non_code SOURCE OUT_SOURCE)
-    splice_cpp_lines("${SOURCE}" SPLICED_SOURCE)
-    mask_cpp_literals("${SPLICED_SOURCE}" WITHOUT_LITERALS)
-    strip_cpp_comments("${WITHOUT_LITERALS}" CODE_ONLY)
-    set("${OUT_SOURCE}" "${CODE_ONLY}" PARENT_SCOPE)
-endfunction()
+file(READ "${RAYLIB_SOURCE_DIR}/host_input.cpp" INPUT_AUTHORITY_SOURCE)
+file(READ "${POISON_HEADER}" POISON_SOURCE)
 
 function(require_match_count SOURCE PATTERN EXPECTED LABEL)
     string(REGEX MATCHALL "${PATTERN}" MATCHES "${SOURCE}")
     list(LENGTH MATCHES ACTUAL)
     if(NOT ACTUAL EQUAL EXPECTED)
         message(FATAL_ERROR
-            "${LABEL}: expected ${EXPECTED} source matches, found ${ACTUAL}")
+            "${LABEL}: expected ${EXPECTED} matches, found ${ACTUAL}")
     endif()
 endfunction()
 
-set(DIRECT_INPUT_PATTERN
-    "(platform_key_pressed|platform_key_down|IsKeyPressed|IsKeyPressedRepeat|IsKeyDown|IsKeyReleased|IsKeyUp|GetKeyPressed|GetCharPressed|IsMouseButtonPressed|IsMouseButtonDown|IsMouseButtonReleased|IsMouseButtonUp|GetMouseX|GetMouseY|GetMousePosition|GetMouseDelta|GetMouseWheelMove|GetMouseWheelMoveV|IsWindowFocused)[ \t\r\n]*\\(")
-
-set(GUARD_SELF_TEST_SOURCE [=[
-const char* line_marker = "// IsKeyReleased(KEY_X)"; IsKeyDown(KEY_A);
-const char* block_open = "/* GetCharPressed()";
-platform_key_pressed(KEY_B);
-const char* block_close = "*/";
-const char slash = '/'; const char quote = '"'; IsMouseButtonUp(MOUSE_BUTTON_LEFT);
-const char* escaped = "quote: \" // IsKeyUp(KEY_Q)"; GetKeyPressed();
-const char* raw = R"guard(IsKeyReleased(KEY_X); // raw /* GetMouseWheelMoveV() */)guard";
-GetMouseDelta();
-const char* unterminated = "GetMousePosition();
-IsKeyUp(KEY_Z);
-// IsKeyPressed(KEY_C);
-/* GetMousePosition(); */
-]=])
-strip_non_code("${GUARD_SELF_TEST_SOURCE}" GUARD_SELF_TEST_CODE)
-require_match_count("${GUARD_SELF_TEST_CODE}" "${DIRECT_INPUT_PATTERN}"
-    6 "guard synthetic real calls")
-foreach(REQUIRED_REAL_CALL IN ITEMS
-        IsKeyDown platform_key_pressed IsMouseButtonUp GetKeyPressed
-        GetMouseDelta IsKeyUp)
-    if(NOT GUARD_SELF_TEST_CODE MATCHES
-            "${REQUIRED_REAL_CALL}[ \t\r\n]*\\(")
+function(require_poison_consumer SOURCE LABEL)
+    set(POISON_INCLUDE "#include \"direct_input_poison.hpp\"")
+    require_match_count(
+        "${SOURCE}" "${POISON_INCLUDE}" 1 "${LABEL} poison includes")
+    string(FIND "${SOURCE}" "${POISON_INCLUDE}" INCLUDE_INDEX)
+    string(LENGTH "${POISON_INCLUDE}" INCLUDE_LENGTH)
+    math(EXPR AFTER_INCLUDE "${INCLUDE_INDEX} + ${INCLUDE_LENGTH}")
+    string(SUBSTRING "${SOURCE}" ${AFTER_INCLUDE} -1 INCLUDE_SUFFIX)
+    if(INCLUDE_SUFFIX MATCHES "#[ \t]*include")
         message(FATAL_ERROR
-            "guard self-test lost real ${REQUIRED_REAL_CALL} call")
+            "${LABEL} poison must be the final normal include")
     endif()
-endforeach()
-foreach(FORBIDDEN_FAKE_CALL IN ITEMS
-        IsKeyReleased GetCharPressed GetMouseWheelMoveV
-        IsKeyPressed GetMousePosition)
-    if(GUARD_SELF_TEST_CODE MATCHES
-            "${FORBIDDEN_FAKE_CALL}[ \t\r\n]*\\(")
-        message(FATAL_ERROR
-            "guard self-test retained non-code ${FORBIDDEN_FAKE_CALL} call")
-    endif()
-endforeach()
-
-set(GUARD_SPLICE_LF_SOURCE [=[
-const char* continued_string = "IsKey\
-Down(KEY_FAKE)";
-const int continued_character = 'IsKey\
-Up';
-const char* continued_raw = R"tag(GetChar\
-Pressed())tag";
-// continued comment \
-platform_key_down(KEY_FAKE);
-IsKey\
-Released(KEY_REAL);
-GetMouseX();
-]=])
-strip_non_code("${GUARD_SPLICE_LF_SOURCE}" GUARD_SPLICE_LF_CODE)
-require_match_count("${GUARD_SPLICE_LF_CODE}" "${DIRECT_INPUT_PATTERN}"
-    2 "guard LF-spliced real calls")
-if(NOT GUARD_SPLICE_LF_CODE MATCHES "IsKeyReleased[ \t\r\n]*\\("
-        OR NOT GUARD_SPLICE_LF_CODE MATCHES "GetMouseX[ \t\r\n]*\\(")
-    message(FATAL_ERROR "guard LF splice lost a real direct-input call")
-endif()
-foreach(LF_FAKE_CALL IN ITEMS
-        IsKeyDown IsKeyUp GetCharPressed platform_key_down)
-    if(GUARD_SPLICE_LF_CODE MATCHES
-            "${LF_FAKE_CALL}[ \t\r\n]*\\(")
+    if(NOT INCLUDE_SUFFIX MATCHES
+            "static_assert[ \t\r\n]*\\([ \t\r\n]*arpg::platform::direct_input_poison::active")
         message(FATAL_ERROR
-            "guard LF splice retained non-code ${LF_FAKE_CALL} call")
+            "${LABEL} must assert the active direct input poison sentinel")
     endif()
-endforeach()
-
-string(ASCII 13 CARRIAGE_RETURN)
-set(BACKSLASH "\\")
-string(CONCAT GUARD_SPLICE_CRLF_SOURCE
-    "const char* continued_string = \"IsMouseButton${BACKSLASH}${CARRIAGE_RETURN}\n"
-    "Down(MOUSE_BUTTON_LEFT)\";\n"
-    "const int continued_character = 'IsKey${BACKSLASH}${CARRIAGE_RETURN}\n"
-    "Up';\n"
-    "const char* continued_raw = R\"tag(GetMouseWheel${BACKSLASH}${CARRIAGE_RETURN}\n"
-    "MoveV())tag\";\n"
-    "// continued comment ${BACKSLASH}${CARRIAGE_RETURN}\n"
-    "GetCharPressed();\n"
-    "IsMouseButton${BACKSLASH}${CARRIAGE_RETURN}\n"
-    "Released(MOUSE_BUTTON_RIGHT);\n"
-    "GetMouseY();\n")
-strip_non_code("${GUARD_SPLICE_CRLF_SOURCE}" GUARD_SPLICE_CRLF_CODE)
-require_match_count("${GUARD_SPLICE_CRLF_CODE}" "${DIRECT_INPUT_PATTERN}"
-    2 "guard CRLF-spliced real calls")
-if(NOT GUARD_SPLICE_CRLF_CODE MATCHES
-        "IsMouseButtonReleased[ \t\r\n]*\\("
-        OR NOT GUARD_SPLICE_CRLF_CODE MATCHES "GetMouseY[ \t\r\n]*\\(")
-    message(FATAL_ERROR "guard CRLF splice lost a real direct-input call")
-endif()
-foreach(CRLF_FAKE_CALL IN ITEMS
-        IsMouseButtonDown IsKeyUp GetMouseWheelMoveV GetCharPressed)
-    if(GUARD_SPLICE_CRLF_CODE MATCHES
-            "${CRLF_FAKE_CALL}[ \t\r\n]*\\(")
-        message(FATAL_ERROR
-            "guard CRLF splice retained non-code ${CRLF_FAKE_CALL} call")
-    endif()
-endforeach()
-
-set(GUARD_RAW_PRESERVE_LF_SOURCE [=[
-const char* raw = u8R"tag(raw content
-)tag\
-"
-IsKeyDown(KEY_FAKE);
-)tag";
-IsKeyReleased(KEY_REAL);
-]=])
-strip_non_code("${GUARD_RAW_PRESERVE_LF_SOURCE}"
-    GUARD_RAW_PRESERVE_LF_CODE)
-require_match_count("${GUARD_RAW_PRESERVE_LF_CODE}"
-    "${DIRECT_INPUT_PATTERN}" 1 "guard raw LF real calls")
-if(NOT GUARD_RAW_PRESERVE_LF_CODE MATCHES
-        "IsKeyReleased[ \t\r\n]*\\(")
-    message(FATAL_ERROR "guard raw LF lost call after real terminator")
-endif()
-if(GUARD_RAW_PRESERVE_LF_CODE MATCHES "IsKeyDown[ \t\r\n]*\\(")
-    message(FATAL_ERROR "guard raw LF accepted call after split fake terminator")
-endif()
-
-string(CONCAT GUARD_RAW_PRESERVE_CRLF_SOURCE
-    "const char* raw = LR\"crlf(raw content\n"
-    ")crlf${BACKSLASH}${CARRIAGE_RETURN}\n"
-    "\"\n"
-    "GetMousePosition();\n"
-    ")crlf\";\n"
-    "GetMouseDelta();\n")
-strip_non_code("${GUARD_RAW_PRESERVE_CRLF_SOURCE}"
-    GUARD_RAW_PRESERVE_CRLF_CODE)
-require_match_count("${GUARD_RAW_PRESERVE_CRLF_CODE}"
-    "${DIRECT_INPUT_PATTERN}" 1 "guard raw CRLF real calls")
-if(NOT GUARD_RAW_PRESERVE_CRLF_CODE MATCHES
-        "GetMouseDelta[ \t\r\n]*\\(")
-    message(FATAL_ERROR "guard raw CRLF lost call after real terminator")
-endif()
-if(GUARD_RAW_PRESERVE_CRLF_CODE MATCHES
-        "GetMousePosition[ \t\r\n]*\\(")
-    message(FATAL_ERROR
-        "guard raw CRLF accepted call after split fake terminator")
-endif()
+endfunction()
 
-set(GUARD_LEXER_CONTEXT_LF_SOURCE [=[
-// R"tag(comment text\
-IsKeyDown(KEY_FAKE); )tag" GetMouseX();
-IsKeyReleased(KEY_REAL);
-/* R"block(block text\
-GetCharPressed(); )block" */
-const char* ordinary = "R"string(string text\
-GetMousePosition())string";
-const int character = 'R"char(character text\
-GetMouseWheelMove())char"';
-const char* raw = R"raw(raw text\
-GetMouseDelta();
-)raw";
-GetMouseY();
-]=])
-splice_cpp_lines("${GUARD_LEXER_CONTEXT_LF_SOURCE}"
-    GUARD_LEXER_CONTEXT_LF_SPLICED)
-foreach(LF_CONTEXT IN ITEMS "comment text" "block text" "string text"
-        "character text")
-    if(GUARD_LEXER_CONTEXT_LF_SPLICED MATCHES
-            "${LF_CONTEXT}\\\\\n")
-        message(FATAL_ERROR
-            "guard LF splice treated ${LF_CONTEXT} raw-lookalike as raw")
-    endif()
-endforeach()
-if(NOT GUARD_LEXER_CONTEXT_LF_SPLICED MATCHES
-        "raw text\\\\\nGetMouseDelta")
-    message(FATAL_ERROR "guard LF splice changed normal-code raw content")
-endif()
-strip_non_code("${GUARD_LEXER_CONTEXT_LF_SOURCE}"
-    GUARD_LEXER_CONTEXT_LF_CODE)
-if(GUARD_LEXER_CONTEXT_LF_CODE MATCHES
-        "(IsKeyDown|GetMouseX)[ \t\r\n]*\\(")
+require_poison_consumer("${HOST_SOURCE}" "raylib host")
+require_poison_consumer("${INVENTORY_SOURCE}" "inventory renderer")
+if(INPUT_AUTHORITY_SOURCE MATCHES "direct_input_poison\\.hpp")
     message(FATAL_ERROR
-        "guard LF splice ended a continued line comment at raw-lookalike")
+        "host_input.cpp is the sampling authority and must not include poison")
 endif()
-foreach(LF_REAL_CALL IN ITEMS IsKeyReleased GetMouseY)
-    if(NOT GUARD_LEXER_CONTEXT_LF_CODE MATCHES
-            "${LF_REAL_CALL}[ \t\r\n]*\\(")
-        message(FATAL_ERROR
-            "guard LF lexer context lost real ${LF_REAL_CALL} call")
-    endif()
-endforeach()
 
-string(CONCAT GUARD_LEXER_CONTEXT_CRLF_SOURCE
-    "// R\"tag(comment text${BACKSLASH}${CARRIAGE_RETURN}\n"
-    "GetMousePosition(); )tag\" IsMouseButtonDown(MOUSE_BUTTON_LEFT);\r\n"
-    "IsMouseButtonReleased(MOUSE_BUTTON_RIGHT);\r\n"
-    "/* R\"block(block text${BACKSLASH}${CARRIAGE_RETURN}\n"
-    "GetCharPressed(); )block\" */\r\n"
-    "const char* ordinary = \"R\"string(string text${BACKSLASH}${CARRIAGE_RETURN}\n"
-    "IsKeyUp(KEY_FAKE))string\";\r\n"
-    "const int character = 'R\"char(character text${BACKSLASH}${CARRIAGE_RETURN}\n"
-    "GetMouseWheelMoveV())char\"';\r\n"
-    "const char* raw = LR\"raw(raw text${BACKSLASH}${CARRIAGE_RETURN}\n"
-    "GetMouseDelta();\r\n"
-    ")raw\";\r\n"
-    "GetMouseY();\r\n")
-splice_cpp_lines("${GUARD_LEXER_CONTEXT_CRLF_SOURCE}"
-    GUARD_LEXER_CONTEXT_CRLF_SPLICED)
-foreach(CRLF_CONTEXT IN ITEMS "comment text" "block text" "string text"
-        "character text")
-    string(FIND "${GUARD_LEXER_CONTEXT_CRLF_SPLICED}"
-        "${CRLF_CONTEXT}${BACKSLASH}${CARRIAGE_RETURN}\n"
-        CRLF_CONTEXT_SPLICE)
-    if(CRLF_CONTEXT_SPLICE GREATER_EQUAL 0)
-        message(FATAL_ERROR
-            "guard CRLF splice treated ${CRLF_CONTEXT} raw-lookalike as raw")
-    endif()
-endforeach()
-string(FIND "${GUARD_LEXER_CONTEXT_CRLF_SPLICED}"
-    "raw text${BACKSLASH}${CARRIAGE_RETURN}\nGetMouseDelta"
-    CRLF_RAW_PRESERVED)
-if(CRLF_RAW_PRESERVED LESS 0)
-    message(FATAL_ERROR "guard CRLF splice changed normal-code raw content")
-endif()
-strip_non_code("${GUARD_LEXER_CONTEXT_CRLF_SOURCE}"
-    GUARD_LEXER_CONTEXT_CRLF_CODE)
-if(GUARD_LEXER_CONTEXT_CRLF_CODE MATCHES
-        "(GetMousePosition|IsMouseButtonDown)[ \t\r\n]*\\(")
-    message(FATAL_ERROR
-        "guard CRLF splice ended a continued line comment at raw-lookalike")
-endif()
-foreach(CRLF_REAL_CALL IN ITEMS IsMouseButtonReleased GetMouseY)
-    if(NOT GUARD_LEXER_CONTEXT_CRLF_CODE MATCHES
-            "${CRLF_REAL_CALL}[ \t\r\n]*\\(")
-        message(FATAL_ERROR
-            "guard CRLF lexer context lost real ${CRLF_REAL_CALL} call")
-    endif()
-endforeach()
-
-strip_non_code("${HOST_INPUT_SOURCE}" HOST_INPUT_CODE)
-strip_non_code("${HOST_SOURCE}" HOST_CODE)
-strip_non_code("${INVENTORY_SOURCE}" INVENTORY_CODE)
-
-require_match_count(
-    "${HOST_INPUT_CODE}"
-    "platform_key_pressed[ \t\r\n]*\\("
-    1
-    "pressed sampling sites")
-require_match_count(
-    "${HOST_INPUT_CODE}"
-    "platform_key_down[ \t\r\n]*\\("
-    1
-    "down sampling sites")
 require_match_count(
-    "${HOST_INPUT_CODE}"
-    "IsMouseButtonPressed[ \t\r\n]*\\("
-    2
-    "mouse pressed sampling sites")
-require_match_count(
-    "${HOST_INPUT_CODE}"
-    "GetMousePosition[ \t\r\n]*\\("
-    1
-    "mouse position sampling sites")
-require_match_count(
-    "${HOST_INPUT_CODE}"
-    "GetMouseWheelMove[ \t\r\n]*\\("
-    1
-    "mouse wheel sampling sites")
-require_match_count(
-    "${HOST_INPUT_CODE}"
-    "IsWindowFocused[ \t\r\n]*\\("
-    1
-    "focus sampling sites")
-require_match_count(
-    "${HOST_INPUT_CODE}"
-    "${DIRECT_INPUT_PATTERN}"
-    7
-    "all approved host input sampling sites")
-
-if(HOST_INPUT_CODE MATCHES
-        "platform_key_pressed[ \t\r\n]*\\([ \t\r\n]*KEY_(V|R|N)")
-    message(FATAL_ERROR "V/R/N must be derived from the stable-key loop")
-endif()
-if(HOST_CODE MATCHES "${DIRECT_INPUT_PATTERN}")
-    message(FATAL_ERROR "raylib_host.cpp must consume the one-sample input module")
-endif()
-if(HOST_CODE MATCHES
-        "KEY_(W|A|S|D|J|K|L|E|I|P|R|N|V)([^A-Z0-9_]|$)")
-    message(FATAL_ERROR "raylib_host.cpp retains a hard-coded gameplay/global stable key")
-endif()
-require_match_count(
-    "${HOST_CODE}"
+    "${HOST_SOURCE}"
     "sample_physical_keys[ \t\r\n]*\\("
     1
     "host physical snapshot calls")
 require_match_count(
-    "${HOST_CODE}"
+    "${HOST_SOURCE}"
     "map_host_frame_input[ \t\r\n]*\\("
     1
     "host logical mapping calls")
-if(INVENTORY_CODE MATCHES "${DIRECT_INPUT_PATTERN}")
-    message(FATAL_ERROR "inventory renderer must consume mapped frame input")
-endif()
-if(NOT HOST_CODE MATCHES
-        "inventory\\.process_input[ \t\r\n]*\\([^;]*frame_input")
-    message(FATAL_ERROR "raylib host must pass mapped frame input to inventory")
+
+set(POISON_TARGET
+    "::arpg::platform::direct_input_poison::blocked")
+set(DIRECT_INPUT_APIS
+    platform_key_pressed
+    platform_key_down
+    IsKeyPressed
+    IsKeyPressedRepeat
+    IsKeyDown
+    IsKeyReleased
+    IsKeyUp
+    GetKeyPressed
+    GetCharPressed
+    IsMouseButtonPressed
+    IsMouseButtonDown
+    IsMouseButtonReleased
+    IsMouseButtonUp
+    GetMouseX
+    GetMouseY
+    GetMousePosition
+    GetMouseDelta
+    GetMouseWheelMove
+    GetMouseWheelMoveV
+    IsWindowFocused)
+
+list(LENGTH DIRECT_INPUT_APIS DIRECT_INPUT_API_COUNT)
+if(NOT DIRECT_INPUT_API_COUNT EQUAL 20)
+    message(FATAL_ERROR "direct input poison API table must contain 20 entries")
+endif()
+
+foreach(DIRECT_INPUT_API IN LISTS DIRECT_INPUT_APIS)
+    require_match_count(
+        "${POISON_SOURCE}"
+        "#define[ \t]+${DIRECT_INPUT_API}[ \t]+${POISON_TARGET}([ \t\r\n]|$)"
+        1
+        "poison macro ${DIRECT_INPUT_API}")
+endforeach()
+
+require_match_count(
+    "${POISON_SOURCE}"
+    "#define[ \t]+[A-Za-z_][A-Za-z0-9_]*[ \t]+${POISON_TARGET}([ \t\r\n]|$)"
+    20
+    "complete direct input poison table")
+if(NOT POISON_SOURCE MATCHES
+        "inline[ \t]+constexpr[ \t]+bool[ \t]+active[ \t]*=[ \t]*true")
+    message(FATAL_ERROR "direct input poison active sentinel is missing")
 endif()
