# Review package: 60a9188..248d502

## Commits
248d502 test: share syntax-aware pause boundary guard

## Files changed
 tests/platform/core_boundary_test.cmake            | 346 ++-------------------
 tests/platform/cpp_source_lexer.cmake              | 319 +++++++++++++++++++
 .../platform/pause_menu_state_boundary_test.cmake  | 271 ++++++++++------
 3 files changed, 523 insertions(+), 413 deletions(-)

## Diff
diff --git a/tests/platform/core_boundary_test.cmake b/tests/platform/core_boundary_test.cmake
index 034896b..a576645 100644
--- a/tests/platform/core_boundary_test.cmake
+++ b/tests/platform/core_boundary_test.cmake
@@ -1,360 +1,47 @@
 if(NOT DEFINED SOURCE_DIR)
     message(FATAL_ERROR "SOURCE_DIR is required")
 endif()
 if(NOT DEFINED SOURCE_LABEL)
     message(FATAL_ERROR "SOURCE_LABEL is required")
 endif()
 
+include("${CMAKE_CURRENT_LIST_DIR}/cpp_source_lexer.cmake")
+
 set(ARPG_FORBIDDEN_SOURCE_INCLUDE_REGEX
     [=[^[ \t]*#[ \t]*include[ \t]*[<"]([^>"]*[/\\])?(raylib\.h|raymath\.h|rlgl\.h|raylib-cpp[^>"]*)[>"]]=])
 set(ARPG_FORBIDDEN_SOURCE_MACRO_REGEX
     [=[^[ \t]*#[ \t]*define[ \t]+[a-z_][a-z0-9_]*[ \t]+[<"]([^>"]*[/\\])?(raylib\.h|raymath\.h|rlgl\.h|raylib-cpp[^>"]*)[>"]]=])
 
 function(arpg_line_has_forbidden_include INPUT_LINE OUT_FOUND)
     string(TOLOWER "${INPUT_LINE}" _arpg_line_lower)
     if(_arpg_line_lower MATCHES
             "${ARPG_FORBIDDEN_SOURCE_INCLUDE_REGEX}"
             OR _arpg_line_lower MATCHES
             "${ARPG_FORBIDDEN_SOURCE_MACRO_REGEX}")
         set("${OUT_FOUND}" TRUE PARENT_SCOPE)
     else()
         set("${OUT_FOUND}" FALSE PARENT_SCOPE)
     endif()
 endfunction()
 
-macro(arpg_finish_scanned_source_line)
-    arpg_line_has_forbidden_include("${_arpg_line}" _arpg_line_found)
-    if(_arpg_line_found)
-        set("${OUT_FOUND}" TRUE PARENT_SCOPE)
-        set("${OUT_LINE}" "${_arpg_line}" PARENT_SCOPE)
-        return()
-    endif()
-    set(_arpg_line "")
-endmacro()
-
 function(arpg_source_has_forbidden_include SOURCE_TEXT OUT_FOUND OUT_LINE)
-    set(_arpg_state CODE)
-    set(_arpg_line "")
-    set(_arpg_raw_closer "")
-    string(LENGTH "${SOURCE_TEXT}" _arpg_source_length)
-    set(_arpg_index 0)
-
-    while(_arpg_index LESS _arpg_source_length)
-        string(SUBSTRING "${SOURCE_TEXT}" ${_arpg_index} 1 _arpg_char)
-        math(EXPR _arpg_next_index "${_arpg_index} + 1")
-        set(_arpg_next_char "")
-        if(_arpg_next_index LESS _arpg_source_length)
-            string(SUBSTRING
-                "${SOURCE_TEXT}" ${_arpg_next_index} 1 _arpg_next_char)
-        endif()
-
-        math(EXPR _arpg_after_next_index "${_arpg_index} + 2")
-        set(_arpg_after_next_char "")
-        if(_arpg_after_next_index LESS _arpg_source_length)
-            string(SUBSTRING
-                "${SOURCE_TEXT}"
-                ${_arpg_after_next_index}
-                1
-                _arpg_after_next_char)
-        endif()
-
-        set(_arpg_logical_next_index ${_arpg_next_index})
-        set(_arpg_logical_next_char "${_arpg_next_char}")
-        if(NOT _arpg_state STREQUAL RAW_STRING)
-            set(_arpg_logical_next_char "")
-            while(_arpg_logical_next_index LESS _arpg_source_length)
-                string(SUBSTRING
-                    "${SOURCE_TEXT}"
-                    ${_arpg_logical_next_index}
-                    1
-                    _arpg_logical_candidate)
-                if(_arpg_logical_candidate STREQUAL "\\")
-                    math(EXPR
-                        _arpg_splice_next_index
-                        "${_arpg_logical_next_index} + 1")
-                    set(_arpg_splice_next_char "")
-                    if(_arpg_splice_next_index LESS _arpg_source_length)
-                        string(SUBSTRING
-                            "${SOURCE_TEXT}"
-                            ${_arpg_splice_next_index}
-                            1
-                            _arpg_splice_next_char)
-                    endif()
-                    if(_arpg_splice_next_char STREQUAL "\n")
-                        math(EXPR
-                            _arpg_logical_next_index
-                            "${_arpg_logical_next_index} + 2")
-                        continue()
-                    endif()
-
-                    math(EXPR
-                        _arpg_splice_after_index
-                        "${_arpg_logical_next_index} + 2")
-                    set(_arpg_splice_after_char "")
-                    if(_arpg_splice_after_index LESS _arpg_source_length)
-                        string(SUBSTRING
-                            "${SOURCE_TEXT}"
-                            ${_arpg_splice_after_index}
-                            1
-                            _arpg_splice_after_char)
-                    endif()
-                    if(_arpg_splice_next_char STREQUAL "\r"
-                            AND _arpg_splice_after_char STREQUAL "\n")
-                        math(EXPR
-                            _arpg_logical_next_index
-                            "${_arpg_logical_next_index} + 3")
-                        continue()
-                    endif()
-                endif()
-
-                set(_arpg_logical_next_char "${_arpg_logical_candidate}")
-                break()
-            endwhile()
-        endif()
-
-        if(NOT _arpg_state STREQUAL RAW_STRING
-                AND _arpg_char STREQUAL "\\")
-            if(_arpg_next_char STREQUAL "\n")
-                math(EXPR _arpg_index "${_arpg_index} + 2")
-                continue()
-            endif()
-            if(_arpg_next_char STREQUAL "\r"
-                    AND _arpg_after_next_char STREQUAL "\n")
-                math(EXPR _arpg_index "${_arpg_index} + 3")
-                continue()
-            endif()
-        endif()
-
-        if(_arpg_state STREQUAL CODE)
-            if(_arpg_char STREQUAL "/"
-                    AND _arpg_logical_next_char STREQUAL "/")
-                string(APPEND _arpg_line " ")
-                set(_arpg_state LINE_COMMENT)
-                math(EXPR
-                    _arpg_index "${_arpg_logical_next_index} + 1")
-                continue()
-            endif()
-            if(_arpg_char STREQUAL "/"
-                    AND _arpg_logical_next_char STREQUAL "*")
-                string(APPEND _arpg_line " ")
-                set(_arpg_state BLOCK_COMMENT)
-                math(EXPR
-                    _arpg_index "${_arpg_logical_next_index} + 1")
-                continue()
-            endif()
-
-            if(_arpg_char STREQUAL "R"
-                    AND _arpg_logical_next_char STREQUAL "\"")
-                math(EXPR
-                    _arpg_delimiter_index
-                    "${_arpg_logical_next_index} + 1")
-                set(_arpg_raw_delimiter "")
-                set(_arpg_valid_raw_opener FALSE)
-                while(_arpg_delimiter_index LESS _arpg_source_length)
-                    string(SUBSTRING
-                        "${SOURCE_TEXT}"
-                        ${_arpg_delimiter_index}
-                        1
-                        _arpg_delimiter_char)
-                    if(_arpg_delimiter_char STREQUAL "\\")
-                        math(EXPR
-                            _arpg_delimiter_next_index
-                            "${_arpg_delimiter_index} + 1")
-                        set(_arpg_delimiter_next_char "")
-                        if(_arpg_delimiter_next_index LESS _arpg_source_length)
-                            string(SUBSTRING
-                                "${SOURCE_TEXT}"
-                                ${_arpg_delimiter_next_index}
-                                1
-                                _arpg_delimiter_next_char)
-                        endif()
-                        if(_arpg_delimiter_next_char STREQUAL "\n")
-                            math(EXPR
-                                _arpg_delimiter_index
-                                "${_arpg_delimiter_index} + 2")
-                            continue()
-                        endif()
-
-                        math(EXPR
-                            _arpg_delimiter_after_index
-                            "${_arpg_delimiter_index} + 2")
-                        set(_arpg_delimiter_after_char "")
-                        if(_arpg_delimiter_after_index LESS _arpg_source_length)
-                            string(SUBSTRING
-                                "${SOURCE_TEXT}"
-                                ${_arpg_delimiter_after_index}
-                                1
-                                _arpg_delimiter_after_char)
-                        endif()
-                        if(_arpg_delimiter_next_char STREQUAL "\r"
-                                AND _arpg_delimiter_after_char STREQUAL "\n")
-                            math(EXPR
-                                _arpg_delimiter_index
-                                "${_arpg_delimiter_index} + 3")
-                            continue()
-                        endif()
-                    endif()
-                    if(_arpg_delimiter_char STREQUAL "(")
-                        string(LENGTH
-                            "${_arpg_raw_delimiter}" _arpg_delimiter_length)
-                        if(_arpg_delimiter_length LESS_EQUAL 16)
-                            set(_arpg_valid_raw_opener TRUE)
-                        endif()
-                        break()
-                    endif()
-                    if(_arpg_delimiter_char STREQUAL "\n"
-                            OR _arpg_delimiter_char STREQUAL "\r")
-                        break()
-                    endif()
-                    string(APPEND
-                        _arpg_raw_delimiter "${_arpg_delimiter_char}")
-                    string(LENGTH
-                        "${_arpg_raw_delimiter}" _arpg_delimiter_length)
-                    if(_arpg_delimiter_length GREATER 16)
-                        break()
-                    endif()
-                    math(EXPR
-                        _arpg_delimiter_index
-                        "${_arpg_delimiter_index} + 1")
-                endwhile()
-
-                if(_arpg_valid_raw_opener)
-                    string(APPEND _arpg_line " ")
-                    set(_arpg_raw_closer ")${_arpg_raw_delimiter}\"")
-                    set(_arpg_state RAW_STRING)
-                    math(EXPR _arpg_index "${_arpg_delimiter_index} + 1")
-                    continue()
-                endif()
-            endif()
-
-            if(_arpg_char STREQUAL "\"")
-                string(TOLOWER "${_arpg_line}" _arpg_line_lower)
-                if(_arpg_line_lower MATCHES
-                        "^[ \t]*#[ \t]*(include[ \t]*|define[ \t]+[a-z_][a-z0-9_]*[ \t]+)$")
-                    string(APPEND _arpg_line "\"")
-                    set(_arpg_state INCLUDE_STRING)
-                else()
-                    string(APPEND _arpg_line " ")
-                    set(_arpg_state STRING)
-                endif()
-                set(_arpg_index ${_arpg_next_index})
-                continue()
-            endif()
-            if(_arpg_char STREQUAL "'")
-                string(APPEND _arpg_line " ")
-                set(_arpg_state CHAR)
-                set(_arpg_index ${_arpg_next_index})
-                continue()
-            endif()
-            if(_arpg_char STREQUAL "\n")
-                arpg_finish_scanned_source_line()
-                set(_arpg_index ${_arpg_next_index})
-                continue()
-            endif()
-            if(NOT _arpg_char STREQUAL "\r")
-                string(APPEND _arpg_line "${_arpg_char}")
-            endif()
-            set(_arpg_index ${_arpg_next_index})
-            continue()
+    arpg_sanitize_cpp_source("${SOURCE_TEXT}" _arpg_sanitized)
+    string(REPLACE "\n" ";" _arpg_lines "${_arpg_sanitized}")
+    foreach(_arpg_line IN LISTS _arpg_lines)
+        arpg_line_has_forbidden_include("${_arpg_line}" _arpg_line_found)
+        if(_arpg_line_found)
+            set("${OUT_FOUND}" TRUE PARENT_SCOPE)
+            set("${OUT_LINE}" "${_arpg_line}" PARENT_SCOPE)
+            return()
         endif()
-
-        if(_arpg_state STREQUAL LINE_COMMENT)
-            if(_arpg_char STREQUAL "\n")
-                arpg_finish_scanned_source_line()
-                set(_arpg_state CODE)
-            endif()
-            set(_arpg_index ${_arpg_next_index})
-            continue()
-        endif()
-
-        if(_arpg_state STREQUAL BLOCK_COMMENT)
-            if(_arpg_char STREQUAL "*"
-                    AND _arpg_logical_next_char STREQUAL "/")
-                set(_arpg_state CODE)
-                math(EXPR
-                    _arpg_index "${_arpg_logical_next_index} + 1")
-                continue()
-            endif()
-            if(_arpg_char STREQUAL "\n")
-                arpg_finish_scanned_source_line()
-            endif()
-            set(_arpg_index ${_arpg_next_index})
-            continue()
-        endif()
-
-        if(_arpg_state STREQUAL RAW_STRING)
-            string(LENGTH "${_arpg_raw_closer}" _arpg_closer_length)
-            math(EXPR
-                _arpg_remaining_length
-                "${_arpg_source_length} - ${_arpg_index}")
-            if(_arpg_remaining_length GREATER_EQUAL _arpg_closer_length)
-                string(SUBSTRING
-                    "${SOURCE_TEXT}"
-                    ${_arpg_index}
-                    ${_arpg_closer_length}
-                    _arpg_closer_candidate)
-                if(_arpg_closer_candidate STREQUAL _arpg_raw_closer)
-                    set(_arpg_state CODE)
-                    math(EXPR
-                        _arpg_index
-                        "${_arpg_index} + ${_arpg_closer_length}")
-                    continue()
-                endif()
-            endif()
-            if(_arpg_char STREQUAL "\n")
-                arpg_finish_scanned_source_line()
-            endif()
-            set(_arpg_index ${_arpg_next_index})
-            continue()
-        endif()
-
-        if(_arpg_state STREQUAL STRING OR _arpg_state STREQUAL CHAR)
-            if(_arpg_char STREQUAL "\\")
-                math(EXPR
-                    _arpg_index "${_arpg_logical_next_index} + 1")
-                continue()
-            endif()
-            if((_arpg_state STREQUAL STRING AND _arpg_char STREQUAL "\"")
-                    OR (_arpg_state STREQUAL CHAR AND _arpg_char STREQUAL "'"))
-                set(_arpg_state CODE)
-                set(_arpg_index ${_arpg_next_index})
-                continue()
-            endif()
-            if(_arpg_char STREQUAL "\n")
-                arpg_finish_scanned_source_line()
-            endif()
-            set(_arpg_index ${_arpg_next_index})
-            continue()
-        endif()
-
-        if(_arpg_state STREQUAL INCLUDE_STRING)
-            if(_arpg_char STREQUAL "\n")
-                arpg_finish_scanned_source_line()
-                set(_arpg_state CODE)
-                set(_arpg_index ${_arpg_next_index})
-                continue()
-            endif()
-            string(APPEND _arpg_line "${_arpg_char}")
-            if(_arpg_char STREQUAL "\"")
-                set(_arpg_state CODE)
-            endif()
-            set(_arpg_index ${_arpg_next_index})
-        endif()
-    endwhile()
-
-    arpg_line_has_forbidden_include("${_arpg_line}" _arpg_line_found)
-    if(_arpg_line_found)
-        set("${OUT_FOUND}" TRUE PARENT_SCOPE)
-        set("${OUT_LINE}" "${_arpg_line}" PARENT_SCOPE)
-        return()
-    endif()
+    endforeach()
     set("${OUT_FOUND}" FALSE PARENT_SCOPE)
     set("${OUT_LINE}" "" PARENT_SCOPE)
 endfunction()
 
 function(arpg_expect_source_boundary LABEL EXPECTED_FOUND SOURCE_TEXT)
     arpg_source_has_forbidden_include(
         "${SOURCE_TEXT}" _arpg_found _arpg_line)
     if(EXPECTED_FOUND AND NOT _arpg_found)
         set_property(
             GLOBAL APPEND PROPERTY
@@ -382,20 +69,26 @@ arpg_expect_source_boundary(
 *comment*/ #include <raylib.h>]=])
 arpg_expect_source_boundary(
     "block comment closer across splice" TRUE [=[/*comment*\
 / #include <raymath.h>]=])
 arpg_expect_source_boundary(
     "continued directive token" TRUE [=[#inc\
 lude <raylib.h>]=])
 arpg_expect_source_boundary(
     "continued header" TRUE [=[#include \
 <raymath.h>]=])
+string(ASCII 13 ARPG_SOURCE_BOUNDARY_CR)
+set(ARPG_SOURCE_BOUNDARY_CRLF_INCLUDE
+    "#inc\\${ARPG_SOURCE_BOUNDARY_CR}\nlude <raylib.h>")
+arpg_expect_source_boundary(
+    "CRLF continued directive token" TRUE
+    "${ARPG_SOURCE_BOUNDARY_CRLF_INCLUDE}")
 arpg_expect_source_boundary(
     "macro angle header" TRUE [=[#define RL_HEADER <raylib.h>
 #include RL_HEADER]=])
 arpg_expect_source_boundary(
     "macro quoted header" TRUE [=[#define RL_HEADER "raymath.h"
 #include RL_HEADER]=])
 arpg_expect_source_boundary(
     "string escape consumes logical quote across splice" TRUE
     [=[const char* text = "a\\
 "b";
@@ -414,20 +107,25 @@ arpg_expect_source_boundary(
 */]=])
 arpg_expect_source_boundary(
     "ordinary string" FALSE
     [=[const char* text = "#include <rlgl.h>";]=])
 arpg_expect_source_boundary(
     "character literal" FALSE
     [=[const char hash = '#'; // #include <raylib.h>]=])
 arpg_expect_source_boundary(
     "continued line comment" FALSE [=[// harmless \
 #include <raylib.h>]=])
+set(ARPG_SOURCE_BOUNDARY_CRLF_COMMENT
+    "// harmless \\${ARPG_SOURCE_BOUNDARY_CR}\n#include <raylib.h>")
+arpg_expect_source_boundary(
+    "CRLF continued line comment" FALSE
+    "${ARPG_SOURCE_BOUNDARY_CRLF_COMMENT}")
 arpg_expect_source_boundary(
     "custom-delimiter raw string" FALSE
     [=[constexpr auto text = R"arpg_raw(
 #include <raylib.h>
 )arpg_raw";]=])
 arpg_expect_source_boundary(
     "raw string preserves splice-shaped comment tokens" FALSE
     [=[constexpr auto text = R"splice_raw(
 /\
 *comment*/ #include <raylib.h>
diff --git a/tests/platform/cpp_source_lexer.cmake b/tests/platform/cpp_source_lexer.cmake
new file mode 100644
index 0000000..d42c9c3
--- /dev/null
+++ b/tests/platform/cpp_source_lexer.cmake
@@ -0,0 +1,319 @@
+macro(arpg_cpp_finish_sanitized_line)
+    string(APPEND _arpg_sanitized "${_arpg_line}\n")
+    set(_arpg_line "")
+endmacro()
+
+function(arpg_sanitize_cpp_source SOURCE_TEXT OUT_SOURCE)
+    set(_arpg_state CODE)
+    set(_arpg_line "")
+    set(_arpg_sanitized "")
+    set(_arpg_raw_closer "")
+    string(LENGTH "${SOURCE_TEXT}" _arpg_source_length)
+    set(_arpg_index 0)
+
+    while(_arpg_index LESS _arpg_source_length)
+        string(SUBSTRING "${SOURCE_TEXT}" ${_arpg_index} 1 _arpg_char)
+        math(EXPR _arpg_next_index "${_arpg_index} + 1")
+        set(_arpg_next_char "")
+        if(_arpg_next_index LESS _arpg_source_length)
+            string(SUBSTRING
+                "${SOURCE_TEXT}" ${_arpg_next_index} 1 _arpg_next_char)
+        endif()
+
+        math(EXPR _arpg_after_next_index "${_arpg_index} + 2")
+        set(_arpg_after_next_char "")
+        if(_arpg_after_next_index LESS _arpg_source_length)
+            string(SUBSTRING
+                "${SOURCE_TEXT}"
+                ${_arpg_after_next_index}
+                1
+                _arpg_after_next_char)
+        endif()
+
+        set(_arpg_logical_next_index ${_arpg_next_index})
+        set(_arpg_logical_next_char "${_arpg_next_char}")
+        if(NOT _arpg_state STREQUAL RAW_STRING)
+            set(_arpg_logical_next_char "")
+            while(_arpg_logical_next_index LESS _arpg_source_length)
+                string(SUBSTRING
+                    "${SOURCE_TEXT}"
+                    ${_arpg_logical_next_index}
+                    1
+                    _arpg_logical_candidate)
+                if(_arpg_logical_candidate STREQUAL "\\")
+                    math(EXPR
+                        _arpg_splice_next_index
+                        "${_arpg_logical_next_index} + 1")
+                    set(_arpg_splice_next_char "")
+                    if(_arpg_splice_next_index LESS _arpg_source_length)
+                        string(SUBSTRING
+                            "${SOURCE_TEXT}"
+                            ${_arpg_splice_next_index}
+                            1
+                            _arpg_splice_next_char)
+                    endif()
+                    if(_arpg_splice_next_char STREQUAL "\n")
+                        math(EXPR
+                            _arpg_logical_next_index
+                            "${_arpg_logical_next_index} + 2")
+                        continue()
+                    endif()
+
+                    math(EXPR
+                        _arpg_splice_after_index
+                        "${_arpg_logical_next_index} + 2")
+                    set(_arpg_splice_after_char "")
+                    if(_arpg_splice_after_index LESS _arpg_source_length)
+                        string(SUBSTRING
+                            "${SOURCE_TEXT}"
+                            ${_arpg_splice_after_index}
+                            1
+                            _arpg_splice_after_char)
+                    endif()
+                    if(_arpg_splice_next_char STREQUAL "\r"
+                            AND _arpg_splice_after_char STREQUAL "\n")
+                        math(EXPR
+                            _arpg_logical_next_index
+                            "${_arpg_logical_next_index} + 3")
+                        continue()
+                    endif()
+                endif()
+
+                set(_arpg_logical_next_char "${_arpg_logical_candidate}")
+                break()
+            endwhile()
+        endif()
+
+        if(NOT _arpg_state STREQUAL RAW_STRING
+                AND _arpg_char STREQUAL "\\")
+            if(_arpg_next_char STREQUAL "\n")
+                math(EXPR _arpg_index "${_arpg_index} + 2")
+                continue()
+            endif()
+            if(_arpg_next_char STREQUAL "\r"
+                    AND _arpg_after_next_char STREQUAL "\n")
+                math(EXPR _arpg_index "${_arpg_index} + 3")
+                continue()
+            endif()
+        endif()
+
+        if(_arpg_state STREQUAL CODE)
+            if(_arpg_char STREQUAL "/"
+                    AND _arpg_logical_next_char STREQUAL "/")
+                string(APPEND _arpg_line " ")
+                set(_arpg_state LINE_COMMENT)
+                math(EXPR
+                    _arpg_index "${_arpg_logical_next_index} + 1")
+                continue()
+            endif()
+            if(_arpg_char STREQUAL "/"
+                    AND _arpg_logical_next_char STREQUAL "*")
+                string(APPEND _arpg_line " ")
+                set(_arpg_state BLOCK_COMMENT)
+                math(EXPR
+                    _arpg_index "${_arpg_logical_next_index} + 1")
+                continue()
+            endif()
+
+            if(_arpg_char STREQUAL "R"
+                    AND _arpg_logical_next_char STREQUAL "\"")
+                math(EXPR
+                    _arpg_delimiter_index
+                    "${_arpg_logical_next_index} + 1")
+                set(_arpg_raw_delimiter "")
+                set(_arpg_valid_raw_opener FALSE)
+                while(_arpg_delimiter_index LESS _arpg_source_length)
+                    string(SUBSTRING
+                        "${SOURCE_TEXT}"
+                        ${_arpg_delimiter_index}
+                        1
+                        _arpg_delimiter_char)
+                    if(_arpg_delimiter_char STREQUAL "\\")
+                        math(EXPR
+                            _arpg_delimiter_next_index
+                            "${_arpg_delimiter_index} + 1")
+                        set(_arpg_delimiter_next_char "")
+                        if(_arpg_delimiter_next_index LESS _arpg_source_length)
+                            string(SUBSTRING
+                                "${SOURCE_TEXT}"
+                                ${_arpg_delimiter_next_index}
+                                1
+                                _arpg_delimiter_next_char)
+                        endif()
+                        if(_arpg_delimiter_next_char STREQUAL "\n")
+                            math(EXPR
+                                _arpg_delimiter_index
+                                "${_arpg_delimiter_index} + 2")
+                            continue()
+                        endif()
+
+                        math(EXPR
+                            _arpg_delimiter_after_index
+                            "${_arpg_delimiter_index} + 2")
+                        set(_arpg_delimiter_after_char "")
+                        if(_arpg_delimiter_after_index LESS _arpg_source_length)
+                            string(SUBSTRING
+                                "${SOURCE_TEXT}"
+                                ${_arpg_delimiter_after_index}
+                                1
+                                _arpg_delimiter_after_char)
+                        endif()
+                        if(_arpg_delimiter_next_char STREQUAL "\r"
+                                AND _arpg_delimiter_after_char STREQUAL "\n")
+                            math(EXPR
+                                _arpg_delimiter_index
+                                "${_arpg_delimiter_index} + 3")
+                            continue()
+                        endif()
+                    endif()
+                    if(_arpg_delimiter_char STREQUAL "(")
+                        string(LENGTH
+                            "${_arpg_raw_delimiter}" _arpg_delimiter_length)
+                        if(_arpg_delimiter_length LESS_EQUAL 16)
+                            set(_arpg_valid_raw_opener TRUE)
+                        endif()
+                        break()
+                    endif()
+                    if(_arpg_delimiter_char STREQUAL "\n"
+                            OR _arpg_delimiter_char STREQUAL "\r")
+                        break()
+                    endif()
+                    string(APPEND
+                        _arpg_raw_delimiter "${_arpg_delimiter_char}")
+                    string(LENGTH
+                        "${_arpg_raw_delimiter}" _arpg_delimiter_length)
+                    if(_arpg_delimiter_length GREATER 16)
+                        break()
+                    endif()
+                    math(EXPR
+                        _arpg_delimiter_index
+                        "${_arpg_delimiter_index} + 1")
+                endwhile()
+
+                if(_arpg_valid_raw_opener)
+                    string(APPEND _arpg_line " ")
+                    set(_arpg_raw_closer ")${_arpg_raw_delimiter}\"")
+                    set(_arpg_state RAW_STRING)
+                    math(EXPR _arpg_index "${_arpg_delimiter_index} + 1")
+                    continue()
+                endif()
+            endif()
+
+            if(_arpg_char STREQUAL "\"")
+                string(TOLOWER "${_arpg_line}" _arpg_line_lower)
+                if(_arpg_line_lower MATCHES
+                        "^[ \t]*#[ \t]*(include[ \t]*|define[ \t]+[a-z_][a-z0-9_]*[ \t]+)$")
+                    string(APPEND _arpg_line "\"")
+                    set(_arpg_state INCLUDE_STRING)
+                else()
+                    string(APPEND _arpg_line " ")
+                    set(_arpg_state STRING)
+                endif()
+                set(_arpg_index ${_arpg_next_index})
+                continue()
+            endif()
+            if(_arpg_char STREQUAL "'")
+                string(APPEND _arpg_line " ")
+                set(_arpg_state CHAR)
+                set(_arpg_index ${_arpg_next_index})
+                continue()
+            endif()
+            if(_arpg_char STREQUAL "\n")
+                arpg_cpp_finish_sanitized_line()
+                set(_arpg_index ${_arpg_next_index})
+                continue()
+            endif()
+            if(NOT _arpg_char STREQUAL "\r")
+                string(APPEND _arpg_line "${_arpg_char}")
+            endif()
+            set(_arpg_index ${_arpg_next_index})
+            continue()
+        endif()
+
+        if(_arpg_state STREQUAL LINE_COMMENT)
+            if(_arpg_char STREQUAL "\n")
+                arpg_cpp_finish_sanitized_line()
+                set(_arpg_state CODE)
+            endif()
+            set(_arpg_index ${_arpg_next_index})
+            continue()
+        endif()
+
+        if(_arpg_state STREQUAL BLOCK_COMMENT)
+            if(_arpg_char STREQUAL "*"
+                    AND _arpg_logical_next_char STREQUAL "/")
+                set(_arpg_state CODE)
+                math(EXPR
+                    _arpg_index "${_arpg_logical_next_index} + 1")
+                continue()
+            endif()
+            if(_arpg_char STREQUAL "\n")
+                arpg_cpp_finish_sanitized_line()
+            endif()
+            set(_arpg_index ${_arpg_next_index})
+            continue()
+        endif()
+
+        if(_arpg_state STREQUAL RAW_STRING)
+            string(LENGTH "${_arpg_raw_closer}" _arpg_closer_length)
+            math(EXPR
+                _arpg_remaining_length
+                "${_arpg_source_length} - ${_arpg_index}")
+            if(_arpg_remaining_length GREATER_EQUAL _arpg_closer_length)
+                string(SUBSTRING
+                    "${SOURCE_TEXT}"
+                    ${_arpg_index}
+                    ${_arpg_closer_length}
+                    _arpg_closer_candidate)
+                if(_arpg_closer_candidate STREQUAL _arpg_raw_closer)
+                    set(_arpg_state CODE)
+                    math(EXPR
+                        _arpg_index
+                        "${_arpg_index} + ${_arpg_closer_length}")
+                    continue()
+                endif()
+            endif()
+            if(_arpg_char STREQUAL "\n")
+                arpg_cpp_finish_sanitized_line()
+            endif()
+            set(_arpg_index ${_arpg_next_index})
+            continue()
+        endif()
+
+        if(_arpg_state STREQUAL STRING OR _arpg_state STREQUAL CHAR)
+            if(_arpg_char STREQUAL "\\")
+                math(EXPR
+                    _arpg_index "${_arpg_logical_next_index} + 1")
+                continue()
+            endif()
+            if((_arpg_state STREQUAL STRING AND _arpg_char STREQUAL "\"")
+                    OR (_arpg_state STREQUAL CHAR AND _arpg_char STREQUAL "'"))
+                set(_arpg_state CODE)
+                set(_arpg_index ${_arpg_next_index})
+                continue()
+            endif()
+            if(_arpg_char STREQUAL "\n")
+                arpg_cpp_finish_sanitized_line()
+            endif()
+            set(_arpg_index ${_arpg_next_index})
+            continue()
+        endif()
+
+        if(_arpg_state STREQUAL INCLUDE_STRING)
+            if(_arpg_char STREQUAL "\n")
+                arpg_cpp_finish_sanitized_line()
+                set(_arpg_state CODE)
+                set(_arpg_index ${_arpg_next_index})
+                continue()
+            endif()
+            string(APPEND _arpg_line "${_arpg_char}")
+            if(_arpg_char STREQUAL "\"")
+                set(_arpg_state CODE)
+            endif()
+            set(_arpg_index ${_arpg_next_index})
+        endif()
+    endwhile()
+
+    string(APPEND _arpg_sanitized "${_arpg_line}")
+    set("${OUT_SOURCE}" "${_arpg_sanitized}" PARENT_SCOPE)
+endfunction()
diff --git a/tests/platform/pause_menu_state_boundary_test.cmake b/tests/platform/pause_menu_state_boundary_test.cmake
index 9ca3219..fd63bd3 100644
--- a/tests/platform/pause_menu_state_boundary_test.cmake
+++ b/tests/platform/pause_menu_state_boundary_test.cmake
@@ -1,143 +1,236 @@
 cmake_minimum_required(VERSION 3.25)
 
 if(NOT DEFINED PAUSE_STATE_HEADER OR NOT DEFINED PAUSE_STATE_SOURCE)
     message(FATAL_ERROR "Pause state source paths are required")
 endif()
 
-function(arpg_pause_includes_are_allowed SOURCE_TEXT OUT_ALLOWED OUT_BAD_INCLUDE)
-    string(REPLACE "\r\n" "\n" _arpg_source "${SOURCE_TEXT}")
-    string(REPLACE "\r" "\n" _arpg_source "${_arpg_source}")
-    string(REPLACE "\n" ";" _arpg_lines "${_arpg_source}")
+include("${CMAKE_CURRENT_LIST_DIR}/cpp_source_lexer.cmake")
+
+function(arpg_pause_preprocessor_is_allowed SANITIZED_SOURCE OUT_ALLOWED OUT_BAD_LINE)
+    string(REPLACE "\n" ";" _arpg_lines "${SANITIZED_SOURCE}")
     foreach(_arpg_line IN LISTS _arpg_lines)
-        if(_arpg_line MATCHES "^[ \t]*#[ \t]*include[ \t]+[<\"][^>\"]+[>\"]")
-            if(NOT _arpg_line MATCHES
-                    "^[ \t]*#[ \t]*include[ \t]+(\"pause_menu_state\\.hpp\"|\"platform/settings/settings_types\\.hpp\"|<cstddef>|<cstdint>|<optional>|<limits>)[ \t]*$")
-                set("${OUT_ALLOWED}" FALSE PARENT_SCOPE)
-                set("${OUT_BAD_INCLUDE}" "${_arpg_line}" PARENT_SCOPE)
-                return()
+        if(_arpg_line MATCHES "^[ \t]*#")
+            if(_arpg_line MATCHES "^[ \t]*#[ \t]*pragma[ \t]+once[ \t]*$")
+                continue()
+            endif()
+            if(_arpg_line MATCHES
+                    "^[ \t]*#[ \t]*include[ \t]*(\"pause_menu_state\\.hpp\"|\"platform/settings/settings_types\\.hpp\"|<cstddef>|<cstdint>|<optional>|<limits>)[ \t]*$")
+                continue()
             endif()
+            set("${OUT_ALLOWED}" FALSE PARENT_SCOPE)
+            set("${OUT_BAD_LINE}" "${_arpg_line}" PARENT_SCOPE)
+            return()
         endif()
     endforeach()
     set("${OUT_ALLOWED}" TRUE PARENT_SCOPE)
-    set("${OUT_BAD_INCLUDE}" "" PARENT_SCOPE)
+    set("${OUT_BAD_LINE}" "" PARENT_SCOPE)
 endfunction()
 
-function(arpg_pause_source_has_forbidden_api SOURCE_TEXT OUT_FOUND OUT_LABEL)
+function(arpg_pause_has_forbidden_api SANITIZED_SOURCE OUT_FOUND OUT_LABEL)
     set(_arpg_api_checks
         "raylib device API|(^|[^A-Za-z0-9_])(IsKeyPressed|IsKeyPressedRepeat|IsKeyDown|IsKeyReleased|IsKeyUp|GetKeyPressed|GetCharPressed|IsMouseButtonPressed|IsMouseButtonDown|IsMouseButtonReleased|IsMouseButtonUp|GetMouseX|GetMouseY|GetMousePosition|GetMouseDelta|GetMouseWheelMove|GetMouseWheelMoveV)[ \t\r\n]*\\("
         "raylib window API|(^|[^A-Za-z0-9_])(IsWindowFocused|IsWindowState|SetWindowState|ClearWindowState|ToggleFullscreen|ToggleBorderlessWindowed|SetWindowSize|SetWindowPosition|SetWindowMinSize|SetWindowMaxSize|SetWindowTitle|SetWindowMonitor|SetTargetFPS|SetMasterVolume|CloseWindow|WindowShouldClose)[ \t\r\n]*\\("
         "SettingsStore type|(^|[^A-Za-z0-9_])SettingsStore([^A-Za-z0-9_]|$)"
         "settings_store token|(^|[^A-Za-z0-9_])settings_store([^A-Za-z0-9_]|$)"
         "combat namespace|(^|[^A-Za-z0-9_])combat[ \t\r\n]*::"
         "dungeon namespace|(^|[^A-Za-z0-9_])dungeon[ \t\r\n]*::"
         "filesystem namespace|(^|[^A-Za-z0-9_])(std[ \t\r\n]*::[ \t\r\n]*)?filesystem[ \t\r\n]*::"
         "C++ stream type|(^|[^A-Za-z0-9_])(std[ \t\r\n]*::[ \t\r\n]*)?(fstream|ifstream|ofstream)([^A-Za-z0-9_]|$)"
         "C FILE type|(^|[^A-Za-z0-9_])FILE([^A-Za-z0-9_]|$)"
         "C file API|(^|[^A-Za-z0-9_])(fopen|fread|fwrite|fclose)[ \t\r\n]*\\("
         "Windows file API|(^|[^A-Za-z0-9_])(CreateFile|ReadFile|WriteFile|MoveFile|MoveFileEx|ReplaceFile|DeleteFile|FlushFileBuffers)(A|W)?[ \t\r\n]*\\(")
     foreach(_arpg_check IN LISTS _arpg_api_checks)
         string(FIND "${_arpg_check}" "|" _arpg_separator)
         string(SUBSTRING "${_arpg_check}" 0 ${_arpg_separator} _arpg_label)
         math(EXPR _arpg_pattern_start "${_arpg_separator} + 1")
         string(SUBSTRING "${_arpg_check}" ${_arpg_pattern_start} -1 _arpg_pattern)
-        if(SOURCE_TEXT MATCHES "${_arpg_pattern}")
+        if(SANITIZED_SOURCE MATCHES "${_arpg_pattern}")
             set("${OUT_FOUND}" TRUE PARENT_SCOPE)
             set("${OUT_LABEL}" "${_arpg_label}" PARENT_SCOPE)
             return()
         endif()
     endforeach()
     set("${OUT_FOUND}" FALSE PARENT_SCOPE)
     set("${OUT_LABEL}" "" PARENT_SCOPE)
 endfunction()
 
-foreach(_arpg_allowed_include IN ITEMS
-        "#include \"pause_menu_state.hpp\""
-        "  # include \"platform/settings/settings_types.hpp\"  "
-        "#include <cstddef>"
-        "#include <cstdint>"
-        "#include <optional>"
-        "#include <limits>"
-        "// #include <raylib.h>")
-    arpg_pause_includes_are_allowed(
-        "${_arpg_allowed_include}" _arpg_allowed _arpg_bad_include)
-    if(NOT _arpg_allowed)
-        message(FATAL_ERROR
-            "Pause state include guard rejected clean control: ${_arpg_allowed_include}")
+function(arpg_pause_source_has_violation SOURCE_TEXT OUT_FOUND OUT_REASON)
+    arpg_sanitize_cpp_source("${SOURCE_TEXT}" _arpg_sanitized)
+    arpg_pause_preprocessor_is_allowed(
+        "${_arpg_sanitized}" _arpg_preprocessor_allowed _arpg_bad_line)
+    if(NOT _arpg_preprocessor_allowed)
+        set("${OUT_FOUND}" TRUE PARENT_SCOPE)
+        set("${OUT_REASON}" "preprocessor:${_arpg_bad_line}" PARENT_SCOPE)
+        return()
     endif()
-endforeach()
-
-foreach(_arpg_forbidden_include IN ITEMS
-        "#include <raylib.h>"
-        "#include \"combat/combat.hpp\""
-        "#include \"dungeon/dungeon.hpp\""
-        "#include <filesystem>"
-        "#include <fstream>"
-        "#include <cstdio>"
-        "#include <windows.h>")
-    arpg_pause_includes_are_allowed(
-        "${_arpg_forbidden_include}" _arpg_allowed _arpg_bad_include)
-    if(_arpg_allowed)
-        message(FATAL_ERROR
-            "Pause state include guard missed mutation: ${_arpg_forbidden_include}")
+    arpg_pause_has_forbidden_api(
+        "${_arpg_sanitized}" _arpg_api_found _arpg_api_label)
+    if(_arpg_api_found)
+        set("${OUT_FOUND}" TRUE PARENT_SCOPE)
+        set("${OUT_REASON}" "api:${_arpg_api_label}" PARENT_SCOPE)
+        return()
     endif()
-endforeach()
+    set("${OUT_FOUND}" FALSE PARENT_SCOPE)
+    set("${OUT_REASON}" "" PARENT_SCOPE)
+endfunction()
 
-foreach(_arpg_mutation IN ITEMS
-        "IsKeyPressed(1);"
-        "SetWindowState(1);"
-        "SettingsStore* store;"
-        "auto settings_store = 0;"
-        "combat::CombatState combat_state;"
-        "dungeon::DungeonSession session;"
-        "filesystem::path save_path;"
-        "std::ifstream input;"
-        "FILE* handle;"
-        "fopen(path, mode);"
-        "CreateFileW(path, 0, 0, 0, 0, 0, 0);"
-        "ReadFile(handle, buffer, size, count, 0);")
-    arpg_pause_source_has_forbidden_api(
-        "${_arpg_mutation}" _arpg_found _arpg_label)
-    if(NOT _arpg_found)
-        message(FATAL_ERROR
-            "Pause state API guard missed mutation: ${_arpg_mutation}")
+function(arpg_expect_pause_boundary LABEL EXPECTED_VIOLATION SOURCE_TEXT)
+    arpg_pause_source_has_violation(
+        "${SOURCE_TEXT}" _arpg_found _arpg_reason)
+    if(EXPECTED_VIOLATION AND NOT _arpg_found)
+        set_property(
+            GLOBAL APPEND PROPERTY
+            ARPG_PAUSE_BOUNDARY_SELF_TEST_FAILURES "missed:${LABEL}")
     endif()
-endforeach()
-
-foreach(_arpg_clean_control IN ITEMS
-        "// raylib combat dungeon file are architecture words"
-        "constexpr char message[] = \"raylib combat dungeon file\";"
-        "int raylibish_combatant_dungeoned_file_id = 0;"
-        "SettingsStorefront settings_storefront;"
-        "combative::State dungeoned::State;"
-        "filesystemish::path profile;"
-        "int fstreaming_ifstreamed_ofstreamed = 0;"
-        "int FILESYSTEM_FILE_ID = 0;"
-        "auto fopenly_freader_fwriter_fcloser = 0;"
-        "auto CreateFileName_ReadFileName_WriteFileName = 0;"
-        "auto IsKeyPressedMessage_SetWindowStateCache = 0;")
-    arpg_pause_source_has_forbidden_api(
-        "${_arpg_clean_control}" _arpg_found _arpg_label)
-    if(_arpg_found)
-        message(FATAL_ERROR
-            "Pause state API guard rejected clean control '${_arpg_label}': ${_arpg_clean_control}")
+    if(NOT EXPECTED_VIOLATION AND _arpg_found)
+        set_property(
+            GLOBAL APPEND PROPERTY
+            ARPG_PAUSE_BOUNDARY_SELF_TEST_FAILURES
+            "false-positive:${LABEL}:${_arpg_reason}")
     endif()
-endforeach()
+endfunction()
+
+set_property(GLOBAL PROPERTY ARPG_PAUSE_BOUNDARY_SELF_TEST_FAILURES "")
+
+arpg_expect_pause_boundary(
+    "pragma once" FALSE [=[#pragma once]=])
+arpg_expect_pause_boundary(
+    "own include" FALSE [=[#include "pause_menu_state.hpp"]=])
+arpg_expect_pause_boundary(
+    "settings include" FALSE
+    [=[ # include "platform/settings/settings_types.hpp"]=])
+arpg_expect_pause_boundary(
+    "standard include" FALSE [=[#include <cstddef>]=])
+arpg_expect_pause_boundary(
+    "comment-separated allowed include" FALSE
+    [=[#/**/include/**/<cstdint>]=])
+
+arpg_expect_pause_boundary(
+    "raylib include" TRUE [=[#include <raylib.h>]=])
+arpg_expect_pause_boundary(
+    "combat include" TRUE [=[#include "combat/combat.hpp"]=])
+arpg_expect_pause_boundary(
+    "dungeon include" TRUE [=[#include "dungeon/dungeon.hpp"]=])
+arpg_expect_pause_boundary(
+    "filesystem include" TRUE [=[#include <filesystem>]=])
+arpg_expect_pause_boundary(
+    "fstream include" TRUE [=[#include <fstream>]=])
+arpg_expect_pause_boundary(
+    "cstdio include" TRUE [=[#include <cstdio>]=])
+arpg_expect_pause_boundary(
+    "windows include" TRUE [=[#include <windows.h>]=])
+arpg_expect_pause_boundary(
+    "comment-separated forbidden include" TRUE
+    [=[#/**/include/**/<raylib.h>]=])
+arpg_expect_pause_boundary(
+    "continued directive" TRUE [=[#inc\
+lude <raylib.h>]=])
+arpg_expect_pause_boundary(
+    "continued header" TRUE [=[#include \
+<raylib.h>]=])
+string(ASCII 13 ARPG_PAUSE_BOUNDARY_CR)
+set(ARPG_PAUSE_BOUNDARY_CRLF_INCLUDE
+    "#include \\${ARPG_PAUSE_BOUNDARY_CR}\n<raylib.h>")
+arpg_expect_pause_boundary(
+    "CRLF continued header" TRUE
+    "${ARPG_PAUSE_BOUNDARY_CRLF_INCLUDE}")
+arpg_expect_pause_boundary(
+    "macro angle header" TRUE [=[#define RL_HEADER <raylib.h>
+#include RL_HEADER]=])
+arpg_expect_pause_boundary(
+    "macro quoted header" TRUE [=[#define RL_HEADER "raylib.h"
+#include RL_HEADER]=])
+arpg_expect_pause_boundary(
+    "macro include" TRUE [=[#include RL_HEADER]=])
+arpg_expect_pause_boundary(
+    "unapproved define" TRUE [=[#define PAUSE_STATE_INTERNAL 1]=])
+arpg_expect_pause_boundary(
+    "unapproved pragma" TRUE [=[#pragma message("pause")]=])
+
+arpg_expect_pause_boundary(
+    "raylib device API" TRUE [=[IsKeyPressed(1);]=])
+arpg_expect_pause_boundary(
+    "raylib window API" TRUE [=[SetWindowState(1);]=])
+arpg_expect_pause_boundary(
+    "SettingsStore type" TRUE [=[SettingsStore* store;]=])
+arpg_expect_pause_boundary(
+    "settings_store token" TRUE [=[auto settings_store = 0;]=])
+arpg_expect_pause_boundary(
+    "combat namespace" TRUE [=[combat::CombatState state;]=])
+arpg_expect_pause_boundary(
+    "dungeon namespace" TRUE [=[dungeon::DungeonSession session;]=])
+arpg_expect_pause_boundary(
+    "filesystem namespace" TRUE [=[std::filesystem::path path;]=])
+arpg_expect_pause_boundary(
+    "C++ stream" TRUE [=[std::ifstream input;]=])
+arpg_expect_pause_boundary(
+    "C FILE" TRUE [=[FILE* handle;]=])
+arpg_expect_pause_boundary(
+    "C file API" TRUE [=[fopen(path, mode);]=])
+arpg_expect_pause_boundary(
+    "Windows file API" TRUE [=[CreateFileW(path, 0, 0, 0, 0, 0, 0);]=])
+arpg_expect_pause_boundary(
+    "spliced API token" TRUE [=[IsKey\
+Pressed(1);]=])
+set(ARPG_PAUSE_BOUNDARY_CRLF_API
+    "IsKey\\${ARPG_PAUSE_BOUNDARY_CR}\nPressed(1);")
+arpg_expect_pause_boundary(
+    "CRLF spliced API token" TRUE "${ARPG_PAUSE_BOUNDARY_CRLF_API}")
+
+arpg_expect_pause_boundary(
+    "line-comment API" FALSE [=[// IsKeyPressed(1)]=])
+arpg_expect_pause_boundary(
+    "ordinary-string SettingsStore" FALSE
+    [=[constexpr char text[] = "SettingsStore";]=])
+arpg_expect_pause_boundary(
+    "line-comment FILE" FALSE [=[// FILE is forbidden]=])
+arpg_expect_pause_boundary(
+    "ordinary-string fopen" FALSE
+    [=[constexpr char text[] = "fopen(path, mode)";]=])
+arpg_expect_pause_boundary(
+    "block-comment APIs" FALSE
+    [=[/* IsKeyPressed(1); SettingsStore; FILE* f; fopen(path, mode); */]=])
+arpg_expect_pause_boundary(
+    "raw-string APIs" FALSE
+    [=[constexpr auto text = R"apis(
+IsKeyPressed(1); SettingsStore; FILE* f; fopen(path, mode);
+)apis";]=])
+arpg_expect_pause_boundary(
+    "line-comment include" FALSE [=[// #include <raylib.h>]=])
+arpg_expect_pause_boundary(
+    "block-comment include" FALSE [=[/* #include <raylib.h> */]=])
+arpg_expect_pause_boundary(
+    "raw-string include" FALSE
+    [=[constexpr auto text = R"include(#include <raylib.h>)include";]=])
+arpg_expect_pause_boundary(
+    "ordinary architecture words" FALSE
+    [=[constexpr char text[] = "raylib combat dungeon file";]=])
+arpg_expect_pause_boundary(
+    "similar identifiers" FALSE
+    [=[int raylibish_combatant_dungeoned_file_id;
+SettingsStorefront settings_storefront;
+auto IsKeyPressedMessage_SetWindowStateCache = 0;]=])
+
+get_property(
+    ARPG_PAUSE_BOUNDARY_SELF_TEST_FAILURES
+    GLOBAL PROPERTY ARPG_PAUSE_BOUNDARY_SELF_TEST_FAILURES)
+if(ARPG_PAUSE_BOUNDARY_SELF_TEST_FAILURES)
+    string(JOIN
+        ", " ARPG_PAUSE_BOUNDARY_SELF_TEST_FAILURE_SUMMARY
+        ${ARPG_PAUSE_BOUNDARY_SELF_TEST_FAILURES})
+    message(FATAL_ERROR
+        "Pause boundary self-tests failed: ${ARPG_PAUSE_BOUNDARY_SELF_TEST_FAILURE_SUMMARY}")
+endif()
 
 foreach(_arpg_source IN ITEMS "${PAUSE_STATE_HEADER}" "${PAUSE_STATE_SOURCE}")
     if(NOT EXISTS "${_arpg_source}")
         message(FATAL_ERROR "Pause state source missing: ${_arpg_source}")
     endif()
     file(READ "${_arpg_source}" _arpg_source_text)
-    arpg_pause_includes_are_allowed(
-        "${_arpg_source_text}" _arpg_allowed _arpg_bad_include)
-    if(NOT _arpg_allowed)
-        message(FATAL_ERROR
-            "Pause state source has forbidden include '${_arpg_bad_include}': ${_arpg_source}")
-    endif()
-    arpg_pause_source_has_forbidden_api(
-        "${_arpg_source_text}" _arpg_found _arpg_label)
+    arpg_pause_source_has_violation(
+        "${_arpg_source_text}" _arpg_found _arpg_reason)
     if(_arpg_found)
         message(FATAL_ERROR
-            "Pause state source contains forbidden ${_arpg_label}: ${_arpg_source}")
+            "Pause state source boundary violation '${_arpg_reason}': ${_arpg_source}")
     endif()
 endforeach()
