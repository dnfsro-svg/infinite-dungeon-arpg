# Review package: b0c51f5..84f5334

## Commits
84f5334 test: make input poison probes independent

## Files changed
 tests/platform/CMakeLists.txt               |  46 +++++++++---
 tests/platform/host_input_source_test.cmake | 108 ++++++++++++++++++++++------
 2 files changed, 121 insertions(+), 33 deletions(-)

## Diff
diff --git a/tests/platform/CMakeLists.txt b/tests/platform/CMakeLists.txt
index 7f05121..7b55cb4 100644
--- a/tests/platform/CMakeLists.txt
+++ b/tests/platform/CMakeLists.txt
@@ -1034,35 +1034,59 @@ function(arpg_verify_direct_input_poison)
         static_assert(
             ::arpg::platform::direct_input_poison::active,
             "direct input poison must be active");
         int main() { return 0; }
     ]=] ARPG_DIRECT_INPUT_POISON_ACTIVE_COMPILES)
     if(NOT ARPG_DIRECT_INPUT_POISON_ACTIVE_COMPILES)
         message(FATAL_ERROR
             "direct input poison must expose a compilable active sentinel")
     endif()
 
-    set(_arpg_poison_probes
-        "KEY|IsKeyDown(0)"
-        "MOUSE|IsMouseButtonDown(0)"
-        "FOCUS|IsWindowFocused()"
-        "PLATFORM_WRAPPER|platform_key_down(0)"
-        "ADDRESS|&IsKeyDown")
-    foreach(_arpg_poison_probe IN LISTS _arpg_poison_probes)
-        string(REPLACE "|" ";" _arpg_poison_parts
-            "${_arpg_poison_probe}")
-        list(GET _arpg_poison_parts 0 _arpg_poison_label)
-        list(GET _arpg_poison_parts 1 _arpg_poison_expression)
+    foreach(_arpg_poison_label IN ITEMS
+            KEY MOUSE FOCUS PLATFORM_WRAPPER ADDRESS)
+        if(_arpg_poison_label STREQUAL "KEY")
+            set(_arpg_poison_declaration "bool IsKeyDown(int);")
+            set(_arpg_poison_expression "IsKeyDown(0)")
+        elseif(_arpg_poison_label STREQUAL "MOUSE")
+            set(_arpg_poison_declaration "bool IsMouseButtonDown(int);")
+            set(_arpg_poison_expression "IsMouseButtonDown(0)")
+        elseif(_arpg_poison_label STREQUAL "FOCUS")
+            set(_arpg_poison_declaration "bool IsWindowFocused();")
+            set(_arpg_poison_expression "IsWindowFocused()")
+        elseif(_arpg_poison_label STREQUAL "PLATFORM_WRAPPER")
+            set(_arpg_poison_declaration
+                "namespace arpg::platform { bool platform_key_down(int) noexcept; }\nusing arpg::platform::platform_key_down;")
+            set(_arpg_poison_expression "platform_key_down(0)")
+        else()
+            set(_arpg_poison_declaration "bool IsKeyDown(int);")
+            set(_arpg_poison_expression "&IsKeyDown")
+        endif()
+
         set(_arpg_poison_result
             "ARPG_DIRECT_INPUT_POISON_${_arpg_poison_label}_COMPILES")
+        set(_arpg_control_result
+            "ARPG_DIRECT_INPUT_CONTROL_${_arpg_poison_label}_COMPILES")
+        unset("${_arpg_control_result}" CACHE)
+        check_cxx_source_compiles("
+            ${_arpg_poison_declaration}
+            int main() {
+                static_cast<void>(${_arpg_poison_expression});
+                return 0;
+            }
+        " "${_arpg_control_result}")
+        if(NOT ${_arpg_control_result})
+            message(FATAL_ERROR
+                "direct input control probe must compile for ${_arpg_poison_label}")
+        endif()
         unset("${_arpg_poison_result}" CACHE)
         check_cxx_source_compiles("
+            ${_arpg_poison_declaration}
             #include \"direct_input_poison.hpp\"
             int main() {
                 static_cast<void>(${_arpg_poison_expression});
                 return 0;
             }
         " "${_arpg_poison_result}")
         if(${_arpg_poison_result})
             message(FATAL_ERROR
                 "direct input poison compile probe unexpectedly accepted ${_arpg_poison_label}")
         endif()
diff --git a/tests/platform/host_input_source_test.cmake b/tests/platform/host_input_source_test.cmake
index 854912d..c462ad5 100644
--- a/tests/platform/host_input_source_test.cmake
+++ b/tests/platform/host_input_source_test.cmake
@@ -15,54 +15,115 @@ file(READ "${POISON_HEADER}" POISON_SOURCE)
 
 function(require_match_count SOURCE PATTERN EXPECTED LABEL)
     string(REGEX MATCHALL "${PATTERN}" MATCHES "${SOURCE}")
     list(LENGTH MATCHES ACTUAL)
     if(NOT ACTUAL EQUAL EXPECTED)
         message(FATAL_ERROR
             "${LABEL}: expected ${EXPECTED} matches, found ${ACTUAL}")
     endif()
 endfunction()
 
+set(SOURCE_LINE_START "(^|\n)[ \t]*")
+set(SOURCE_LINE_END "[ \t]*(\r?\n|$)")
+set(POISON_INCLUDE_LINE_PATTERN
+    "${SOURCE_LINE_START}#[ \t]*include[ \t]+\"direct_input_poison\\.hpp\"${SOURCE_LINE_END}")
+set(ANY_INCLUDE_LINE_PATTERN
+    "${SOURCE_LINE_START}#[ \t]*include[ \t]+")
+set(ACTIVE_ASSERT_PATTERN
+    "${SOURCE_LINE_START}static_assert[ \t]*\\([ \t]*arpg::platform::direct_input_poison::active[ \t]*(,|\\))")
+set(SAMPLE_CALL_LINE_PATTERN
+    "${SOURCE_LINE_START}const[ \t]+PhysicalKeySnapshot[ \t]+physical_keys[ \t]*=[ \t]*sample_physical_keys[ \t]*\\([ \t]*\\)")
+set(MAP_CALL_LINE_PATTERN
+    "${SOURCE_LINE_START}HostFrameInput[ \t]+frame_input[ \t]*=[ \t]*map_host_frame_input[ \t]*\\(")
+set(ACTIVE_SENTINEL_PATTERN
+    "${SOURCE_LINE_START}inline[ \t]+constexpr[ \t]+bool[ \t]+active[ \t]*=[ \t]*true")
+
+function(poison_macro_line_pattern API OUT_PATTERN)
+    set("${OUT_PATTERN}"
+        "(^|\n)#define[ \t]+${API}[ \t]+::arpg::platform::direct_input_poison::blocked${SOURCE_LINE_END}"
+        PARENT_SCOPE)
+endfunction()
+
+set(COMMENT_ONLY_STRUCTURE [=[
+// #include "direct_input_poison.hpp"
+// static_assert(arpg::platform::direct_input_poison::active);
+// const PhysicalKeySnapshot physical_keys = sample_physical_keys();
+// HostFrameInput frame_input = map_host_frame_input(settings, physical_keys);
+// #define IsKeyDown ::arpg::platform::direct_input_poison::blocked
+]=])
+set(REAL_STRUCTURE [=[
+#include "direct_input_poison.hpp"
+static_assert(arpg::platform::direct_input_poison::active, "active");
+const PhysicalKeySnapshot physical_keys = sample_physical_keys();
+HostFrameInput frame_input = map_host_frame_input(settings, physical_keys);
+#define IsKeyDown ::arpg::platform::direct_input_poison::blocked
+]=])
+poison_macro_line_pattern(IsKeyDown SELF_TEST_MACRO_PATTERN)
+foreach(STRUCTURE_PATTERN IN ITEMS
+        POISON_INCLUDE_LINE_PATTERN
+        ACTIVE_ASSERT_PATTERN
+        SAMPLE_CALL_LINE_PATTERN
+        MAP_CALL_LINE_PATTERN
+        SELF_TEST_MACRO_PATTERN)
+    require_match_count(
+        "${COMMENT_ONLY_STRUCTURE}"
+        "${${STRUCTURE_PATTERN}}"
+        0
+        "commented ${STRUCTURE_PATTERN}")
+    require_match_count(
+        "${REAL_STRUCTURE}"
+        "${${STRUCTURE_PATTERN}}"
+        1
+        "real ${STRUCTURE_PATTERN}")
+endforeach()
+
 function(require_poison_consumer SOURCE LABEL)
-    set(POISON_INCLUDE "#include \"direct_input_poison.hpp\"")
     require_match_count(
-        "${SOURCE}" "${POISON_INCLUDE}" 1 "${LABEL} poison includes")
-    string(FIND "${SOURCE}" "${POISON_INCLUDE}" INCLUDE_INDEX)
-    string(LENGTH "${POISON_INCLUDE}" INCLUDE_LENGTH)
+        "${SOURCE}"
+        "${POISON_INCLUDE_LINE_PATTERN}"
+        1
+        "${LABEL} poison includes")
+    string(REGEX MATCH
+        "${POISON_INCLUDE_LINE_PATTERN}"
+        POISON_INCLUDE_LINE
+        "${SOURCE}")
+    string(FIND "${SOURCE}" "${POISON_INCLUDE_LINE}" INCLUDE_INDEX)
+    string(LENGTH "${POISON_INCLUDE_LINE}" INCLUDE_LENGTH)
     math(EXPR AFTER_INCLUDE "${INCLUDE_INDEX} + ${INCLUDE_LENGTH}")
     string(SUBSTRING "${SOURCE}" ${AFTER_INCLUDE} -1 INCLUDE_SUFFIX)
-    if(INCLUDE_SUFFIX MATCHES "#[ \t]*include")
+    if(INCLUDE_SUFFIX MATCHES "${ANY_INCLUDE_LINE_PATTERN}")
         message(FATAL_ERROR
             "${LABEL} poison must be the final normal include")
     endif()
-    if(NOT INCLUDE_SUFFIX MATCHES
-            "static_assert[ \t\r\n]*\\([ \t\r\n]*arpg::platform::direct_input_poison::active")
-        message(FATAL_ERROR
-            "${LABEL} must assert the active direct input poison sentinel")
-    endif()
+    require_match_count(
+        "${INCLUDE_SUFFIX}"
+        "${ACTIVE_ASSERT_PATTERN}"
+        1
+        "${LABEL} active poison assertions")
 endfunction()
 
 require_poison_consumer("${HOST_SOURCE}" "raylib host")
 require_poison_consumer("${INVENTORY_SOURCE}" "inventory renderer")
-if(INPUT_AUTHORITY_SOURCE MATCHES "direct_input_poison\\.hpp")
-    message(FATAL_ERROR
-        "host_input.cpp is the sampling authority and must not include poison")
-endif()
+require_match_count(
+    "${INPUT_AUTHORITY_SOURCE}"
+    "${POISON_INCLUDE_LINE_PATTERN}"
+    0
+    "host input authority poison includes")
 
 require_match_count(
     "${HOST_SOURCE}"
-    "sample_physical_keys[ \t\r\n]*\\("
+    "${SAMPLE_CALL_LINE_PATTERN}"
     1
     "host physical snapshot calls")
 require_match_count(
     "${HOST_SOURCE}"
-    "map_host_frame_input[ \t\r\n]*\\("
+    "${MAP_CALL_LINE_PATTERN}"
     1
     "host logical mapping calls")
 
 set(POISON_TARGET
     "::arpg::platform::direct_input_poison::blocked")
 set(DIRECT_INPUT_APIS
     platform_key_pressed
     platform_key_down
     IsKeyPressed
     IsKeyPressedRepeat
@@ -82,26 +143,29 @@ set(DIRECT_INPUT_APIS
     GetMouseWheelMove
     GetMouseWheelMoveV
     IsWindowFocused)
 
 list(LENGTH DIRECT_INPUT_APIS DIRECT_INPUT_API_COUNT)
 if(NOT DIRECT_INPUT_API_COUNT EQUAL 20)
     message(FATAL_ERROR "direct input poison API table must contain 20 entries")
 endif()
 
 foreach(DIRECT_INPUT_API IN LISTS DIRECT_INPUT_APIS)
+    poison_macro_line_pattern(
+        "${DIRECT_INPUT_API}" DIRECT_INPUT_MACRO_PATTERN)
     require_match_count(
         "${POISON_SOURCE}"
-        "#define[ \t]+${DIRECT_INPUT_API}[ \t]+${POISON_TARGET}([ \t\r\n]|$)"
+        "${DIRECT_INPUT_MACRO_PATTERN}"
         1
         "poison macro ${DIRECT_INPUT_API}")
 endforeach()
 
 require_match_count(
     "${POISON_SOURCE}"
-    "#define[ \t]+[A-Za-z_][A-Za-z0-9_]*[ \t]+${POISON_TARGET}([ \t\r\n]|$)"
+    "(^|\n)#define[ \t]+[A-Za-z_][A-Za-z0-9_]*[ \t]+${POISON_TARGET}"
     20
     "complete direct input poison table")
-if(NOT POISON_SOURCE MATCHES
-        "inline[ \t]+constexpr[ \t]+bool[ \t]+active[ \t]*=[ \t]*true")
-    message(FATAL_ERROR "direct input poison active sentinel is missing")
-endif()
+require_match_count(
+    "${POISON_SOURCE}"
+    "${ACTIVE_SENTINEL_PATTERN}"
+    1
+    "direct input poison active sentinels")
