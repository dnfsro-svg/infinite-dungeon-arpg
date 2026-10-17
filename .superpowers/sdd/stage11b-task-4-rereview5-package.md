# Review package: 35eaf6b..68fbab8

## Commits
68fbab8 test: respect lexer context in input guard

## Files changed
 tests/platform/host_input_source_test.cmake | 251 ++++++++++++++++++++++++----
 1 file changed, 220 insertions(+), 31 deletions(-)

## Diff
diff --git a/tests/platform/host_input_source_test.cmake b/tests/platform/host_input_source_test.cmake
index 4828bb1..c449639 100644
--- a/tests/platform/host_input_source_test.cmake
+++ b/tests/platform/host_input_source_test.cmake
@@ -57,62 +57,160 @@ function(find_cpp_raw_literal_end SOURCE START_INDEX OUT_END)
         endif()
     endif()
     set("${OUT_END}" ${RAW_END} PARENT_SCOPE)
 endfunction()
 
 function(splice_cpp_lines SOURCE OUT_SOURCE)
     set(TEXT "${SOURCE}")
     string(LENGTH "${TEXT}" TEXT_LENGTH)
     set(OUTPUT "")
     set(INDEX 0)
+    set(STATE "normal")
     while(INDEX LESS TEXT_LENGTH)
         string(SUBSTRING "${TEXT}" ${INDEX} 1 CURRENT)
-        if(CURRENT STREQUAL "R")
-            set(RAW_END -1)
-            math(EXPR RAW_QUOTE_INDEX "${INDEX} + 1")
-            if(RAW_QUOTE_INDEX LESS TEXT_LENGTH)
-                string(SUBSTRING "${TEXT}" ${RAW_QUOTE_INDEX}
-                    1 RAW_QUOTE)
-                if(RAW_QUOTE STREQUAL "\"")
-                    find_cpp_raw_literal_end("${TEXT}" ${INDEX} RAW_END)
+
+        if(STATE STREQUAL "raw")
+            math(EXPR RAW_LENGTH "${RAW_END} - ${INDEX}")
+            string(SUBSTRING "${TEXT}" ${INDEX} ${RAW_LENGTH}
+                RAW_LITERAL)
+            string(APPEND OUTPUT "${RAW_LITERAL}")
+            set(INDEX ${RAW_END})
+            set(STATE "normal")
+            continue()
+        endif()
+
+        math(EXPR NEXT_INDEX "${INDEX} + 1")
+        set(NEXT "")
+        if(NEXT_INDEX LESS TEXT_LENGTH)
+            string(SUBSTRING "${TEXT}" ${NEXT_INDEX} 1 NEXT)
+        endif()
+
+        set(SPLICE_LENGTH 0)
+        if(CURRENT STREQUAL "\\")
+            if(NEXT STREQUAL "\n")
+                set(SPLICE_LENGTH 2)
+            elseif(NEXT STREQUAL "\r")
+                math(EXPR AFTER_CR_INDEX "${INDEX} + 2")
+                if(AFTER_CR_INDEX LESS TEXT_LENGTH)
+                    string(SUBSTRING "${TEXT}" ${AFTER_CR_INDEX}
+                        1 AFTER_CR)
+                    if(AFTER_CR STREQUAL "\n")
+                        set(SPLICE_LENGTH 3)
+                    endif()
                 endif()
             endif()
-            if(RAW_END GREATER_EQUAL 0)
-                math(EXPR RAW_LENGTH "${RAW_END} - ${INDEX}")
-                string(SUBSTRING "${TEXT}" ${INDEX} ${RAW_LENGTH}
-                    RAW_LITERAL)
-                string(APPEND OUTPUT "${RAW_LITERAL}")
-                set(INDEX ${RAW_END})
-                continue()
-            endif()
         endif()
-        if(CURRENT STREQUAL "\\")
-            math(EXPR NEXT_INDEX "${INDEX} + 1")
-            if(NEXT_INDEX LESS TEXT_LENGTH)
-                string(SUBSTRING "${TEXT}" ${NEXT_INDEX} 1 NEXT)
+
+        if(SPLICE_LENGTH GREATER 0)
+            math(EXPR INDEX "${INDEX} + ${SPLICE_LENGTH}")
+            continue()
+        endif()
+
+        if(STATE STREQUAL "line_comment")
+            if(CURRENT STREQUAL "\r")
+                string(APPEND OUTPUT "${CURRENT}")
                 if(NEXT STREQUAL "\n")
+                    string(APPEND OUTPUT "${NEXT}")
                     math(EXPR INDEX "${INDEX} + 2")
-                    continue()
+                else()
+                    math(EXPR INDEX "${INDEX} + 1")
                 endif()
-                if(NEXT STREQUAL "\r")
-                    math(EXPR AFTER_CR_INDEX "${INDEX} + 2")
-                    if(AFTER_CR_INDEX LESS TEXT_LENGTH)
-                        string(SUBSTRING "${TEXT}" ${AFTER_CR_INDEX}
-                            1 AFTER_CR)
-                        if(AFTER_CR STREQUAL "\n")
-                            math(EXPR INDEX "${INDEX} + 3")
-                            continue()
-                        endif()
-                    endif()
+                set(STATE "normal")
+                continue()
+            elseif(CURRENT STREQUAL "\n")
+                string(APPEND OUTPUT "${CURRENT}")
+                math(EXPR INDEX "${INDEX} + 1")
+                set(STATE "normal")
+                continue()
+            endif()
+            string(APPEND OUTPUT "${CURRENT}")
+            math(EXPR INDEX "${INDEX} + 1")
+            continue()
+        endif()
+
+        if(STATE STREQUAL "block_comment")
+            if(CURRENT STREQUAL "*" AND NEXT STREQUAL "/")
+                string(APPEND OUTPUT "*/")
+                math(EXPR INDEX "${INDEX} + 2")
+                set(STATE "normal")
+                continue()
+            endif()
+            string(APPEND OUTPUT "${CURRENT}")
+            math(EXPR INDEX "${INDEX} + 1")
+            continue()
+        endif()
+
+        if(STATE STREQUAL "ordinary_string" OR STATE STREQUAL "char")
+            if(CURRENT STREQUAL "\\")
+                string(APPEND OUTPUT "${CURRENT}")
+                math(EXPR INDEX "${INDEX} + 1")
+                if(INDEX LESS TEXT_LENGTH)
+                    string(SUBSTRING "${TEXT}" ${INDEX} 1 ESCAPED)
+                    string(APPEND OUTPUT "${ESCAPED}")
+                    math(EXPR INDEX "${INDEX} + 1")
+                endif()
+                continue()
+            endif()
+
+            string(APPEND OUTPUT "${CURRENT}")
+            math(EXPR INDEX "${INDEX} + 1")
+            if((STATE STREQUAL "ordinary_string" AND CURRENT STREQUAL "\"")
+                    OR (STATE STREQUAL "char" AND CURRENT STREQUAL "'"))
+                set(STATE "normal")
+            elseif(CURRENT STREQUAL "\r")
+                if(NEXT STREQUAL "\n")
+                    string(APPEND OUTPUT "${NEXT}")
+                    math(EXPR INDEX "${INDEX} + 1")
                 endif()
+                set(STATE "normal")
+            elseif(CURRENT STREQUAL "\n")
+                set(STATE "normal")
             endif()
+            continue()
+        endif()
+
+        if(CURRENT STREQUAL "R" AND NEXT STREQUAL "\"")
+            find_cpp_raw_literal_end("${TEXT}" ${INDEX} RAW_END)
+            if(RAW_END GREATER_EQUAL 0)
+                set(STATE "raw")
+                continue()
+            endif()
+        endif()
+
+        if(CURRENT STREQUAL "/" AND NEXT STREQUAL "/")
+            string(APPEND OUTPUT "//")
+            math(EXPR INDEX "${INDEX} + 2")
+            set(STATE "line_comment")
+            continue()
         endif()
+
+        if(CURRENT STREQUAL "/" AND NEXT STREQUAL "*")
+            string(APPEND OUTPUT "/*")
+            math(EXPR INDEX "${INDEX} + 2")
+            set(STATE "block_comment")
+            continue()
+        endif()
+
+        if(CURRENT STREQUAL "\"")
+            string(APPEND OUTPUT "${CURRENT}")
+            math(EXPR INDEX "${INDEX} + 1")
+            set(STATE "ordinary_string")
+            continue()
+        endif()
+
+        if(CURRENT STREQUAL "'")
+            string(APPEND OUTPUT "${CURRENT}")
+            math(EXPR INDEX "${INDEX} + 1")
+            set(STATE "char")
+            continue()
+        endif()
+
         string(APPEND OUTPUT "${CURRENT}")
         math(EXPR INDEX "${INDEX} + 1")
     endwhile()
     set("${OUT_SOURCE}" "${OUTPUT}" PARENT_SCOPE)
 endfunction()
 
 function(mask_cpp_literals SOURCE OUT_SOURCE)
     set(TEXT "${SOURCE}")
     string(LENGTH "${TEXT}" TEXT_LENGTH)
     set(OUTPUT "")
@@ -392,20 +490,111 @@ require_match_count("${GUARD_RAW_PRESERVE_CRLF_CODE}"
 if(NOT GUARD_RAW_PRESERVE_CRLF_CODE MATCHES
         "GetMouseDelta[ \t\r\n]*\\(")
     message(FATAL_ERROR "guard raw CRLF lost call after real terminator")
 endif()
 if(GUARD_RAW_PRESERVE_CRLF_CODE MATCHES
         "GetMousePosition[ \t\r\n]*\\(")
     message(FATAL_ERROR
         "guard raw CRLF accepted call after split fake terminator")
 endif()
 
+set(GUARD_LEXER_CONTEXT_LF_SOURCE [=[
+// R"tag(comment text\
+IsKeyDown(KEY_FAKE); )tag" GetMouseX();
+IsKeyReleased(KEY_REAL);
+/* R"block(block text\
+GetCharPressed(); )block" */
+const char* ordinary = "R"string(string text\
+GetMousePosition())string";
+const int character = 'R"char(character text\
+GetMouseWheelMove())char"';
+const char* raw = R"raw(raw text\
+GetMouseDelta();
+)raw";
+GetMouseY();
+]=])
+splice_cpp_lines("${GUARD_LEXER_CONTEXT_LF_SOURCE}"
+    GUARD_LEXER_CONTEXT_LF_SPLICED)
+foreach(LF_CONTEXT IN ITEMS "comment text" "block text" "string text"
+        "character text")
+    if(GUARD_LEXER_CONTEXT_LF_SPLICED MATCHES
+            "${LF_CONTEXT}\\\\\n")
+        message(FATAL_ERROR
+            "guard LF splice treated ${LF_CONTEXT} raw-lookalike as raw")
+    endif()
+endforeach()
+if(NOT GUARD_LEXER_CONTEXT_LF_SPLICED MATCHES
+        "raw text\\\\\nGetMouseDelta")
+    message(FATAL_ERROR "guard LF splice changed normal-code raw content")
+endif()
+strip_non_code("${GUARD_LEXER_CONTEXT_LF_SOURCE}"
+    GUARD_LEXER_CONTEXT_LF_CODE)
+if(GUARD_LEXER_CONTEXT_LF_CODE MATCHES
+        "(IsKeyDown|GetMouseX)[ \t\r\n]*\\(")
+    message(FATAL_ERROR
+        "guard LF splice ended a continued line comment at raw-lookalike")
+endif()
+foreach(LF_REAL_CALL IN ITEMS IsKeyReleased GetMouseY)
+    if(NOT GUARD_LEXER_CONTEXT_LF_CODE MATCHES
+            "${LF_REAL_CALL}[ \t\r\n]*\\(")
+        message(FATAL_ERROR
+            "guard LF lexer context lost real ${LF_REAL_CALL} call")
+    endif()
+endforeach()
+
+string(CONCAT GUARD_LEXER_CONTEXT_CRLF_SOURCE
+    "// R\"tag(comment text${BACKSLASH}${CARRIAGE_RETURN}\n"
+    "GetMousePosition(); )tag\" IsMouseButtonDown(MOUSE_BUTTON_LEFT);\r\n"
+    "IsMouseButtonReleased(MOUSE_BUTTON_RIGHT);\r\n"
+    "/* R\"block(block text${BACKSLASH}${CARRIAGE_RETURN}\n"
+    "GetCharPressed(); )block\" */\r\n"
+    "const char* ordinary = \"R\"string(string text${BACKSLASH}${CARRIAGE_RETURN}\n"
+    "IsKeyUp(KEY_FAKE))string\";\r\n"
+    "const int character = 'R\"char(character text${BACKSLASH}${CARRIAGE_RETURN}\n"
+    "GetMouseWheelMoveV())char\"';\r\n"
+    "const char* raw = LR\"raw(raw text${BACKSLASH}${CARRIAGE_RETURN}\n"
+    "GetMouseDelta();\r\n"
+    ")raw\";\r\n"
+    "GetMouseY();\r\n")
+splice_cpp_lines("${GUARD_LEXER_CONTEXT_CRLF_SOURCE}"
+    GUARD_LEXER_CONTEXT_CRLF_SPLICED)
+foreach(CRLF_CONTEXT IN ITEMS "comment text" "block text" "string text"
+        "character text")
+    string(FIND "${GUARD_LEXER_CONTEXT_CRLF_SPLICED}"
+        "${CRLF_CONTEXT}${BACKSLASH}${CARRIAGE_RETURN}\n"
+        CRLF_CONTEXT_SPLICE)
+    if(CRLF_CONTEXT_SPLICE GREATER_EQUAL 0)
+        message(FATAL_ERROR
+            "guard CRLF splice treated ${CRLF_CONTEXT} raw-lookalike as raw")
+    endif()
+endforeach()
+string(FIND "${GUARD_LEXER_CONTEXT_CRLF_SPLICED}"
+    "raw text${BACKSLASH}${CARRIAGE_RETURN}\nGetMouseDelta"
+    CRLF_RAW_PRESERVED)
+if(CRLF_RAW_PRESERVED LESS 0)
+    message(FATAL_ERROR "guard CRLF splice changed normal-code raw content")
+endif()
+strip_non_code("${GUARD_LEXER_CONTEXT_CRLF_SOURCE}"
+    GUARD_LEXER_CONTEXT_CRLF_CODE)
+if(GUARD_LEXER_CONTEXT_CRLF_CODE MATCHES
+        "(GetMousePosition|IsMouseButtonDown)[ \t\r\n]*\\(")
+    message(FATAL_ERROR
+        "guard CRLF splice ended a continued line comment at raw-lookalike")
+endif()
+foreach(CRLF_REAL_CALL IN ITEMS IsMouseButtonReleased GetMouseY)
+    if(NOT GUARD_LEXER_CONTEXT_CRLF_CODE MATCHES
+            "${CRLF_REAL_CALL}[ \t\r\n]*\\(")
+        message(FATAL_ERROR
+            "guard CRLF lexer context lost real ${CRLF_REAL_CALL} call")
+    endif()
+endforeach()
+
 strip_non_code("${HOST_INPUT_SOURCE}" HOST_INPUT_CODE)
 strip_non_code("${HOST_SOURCE}" HOST_CODE)
 strip_non_code("${INVENTORY_SOURCE}" INVENTORY_CODE)
 
 require_match_count(
     "${HOST_INPUT_CODE}"
     "platform_key_pressed[ \t\r\n]*\\("
     1
     "pressed sampling sites")
 require_match_count(
