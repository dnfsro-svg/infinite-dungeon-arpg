# Review package: ec29579..35eaf6b

## Commits
35eaf6b test: preserve raw input guard literals

## Files changed
 tests/platform/host_input_source_test.cmake | 162 ++++++++++++++++++++--------
 1 file changed, 119 insertions(+), 43 deletions(-)

## Diff
diff --git a/tests/platform/host_input_source_test.cmake b/tests/platform/host_input_source_test.cmake
index 602c090..4828bb1 100644
--- a/tests/platform/host_input_source_test.cmake
+++ b/tests/platform/host_input_source_test.cmake
@@ -1,25 +1,97 @@
 if(NOT DEFINED RAYLIB_SOURCE_DIR)
     message(FATAL_ERROR "RAYLIB_SOURCE_DIR is required")
 endif()
 
 file(READ "${RAYLIB_SOURCE_DIR}/host_input.cpp" HOST_INPUT_SOURCE)
 file(READ "${RAYLIB_SOURCE_DIR}/raylib_host.cpp" HOST_SOURCE)
 file(READ "${RAYLIB_SOURCE_DIR}/inventory_renderer.cpp" INVENTORY_SOURCE)
 
+function(find_cpp_raw_literal_end SOURCE START_INDEX OUT_END)
+    set(TEXT "${SOURCE}")
+    string(LENGTH "${TEXT}" TEXT_LENGTH)
+    set(RAW_END -1)
+    math(EXPR NEXT_INDEX "${START_INDEX} + 1")
+    if(START_INDEX LESS TEXT_LENGTH AND NEXT_INDEX LESS TEXT_LENGTH)
+        string(SUBSTRING "${TEXT}" ${START_INDEX} 1 START_CHAR)
+        string(SUBSTRING "${TEXT}" ${NEXT_INDEX} 1 NEXT_CHAR)
+        if(START_CHAR STREQUAL "R" AND NEXT_CHAR STREQUAL "\"")
+            math(EXPR DELIMITER_START "${START_INDEX} + 2")
+            set(SCAN ${DELIMITER_START})
+            set(OPEN_PAREN -1)
+            while(SCAN LESS TEXT_LENGTH)
+                math(EXPR DELIMITER_LENGTH "${SCAN} - ${DELIMITER_START}")
+                if(DELIMITER_LENGTH GREATER 16)
+                    break()
+                endif()
+                string(SUBSTRING "${TEXT}" ${SCAN} 1 RAW_CHAR)
+                if(RAW_CHAR STREQUAL "(")
+                    set(OPEN_PAREN ${SCAN})
+                    break()
+                endif()
+                if(RAW_CHAR STREQUAL " " OR RAW_CHAR STREQUAL "\t"
+                        OR RAW_CHAR STREQUAL "\r" OR RAW_CHAR STREQUAL "\n"
+                        OR RAW_CHAR STREQUAL ")" OR RAW_CHAR STREQUAL "\\")
+                    break()
+                endif()
+                math(EXPR SCAN "${SCAN} + 1")
+            endwhile()
+            if(OPEN_PAREN GREATER_EQUAL 0)
+                math(EXPR DELIMITER_LENGTH
+                    "${OPEN_PAREN} - ${DELIMITER_START}")
+                string(SUBSTRING "${TEXT}" ${DELIMITER_START}
+                    ${DELIMITER_LENGTH} RAW_DELIMITER)
+                set(RAW_TERMINATOR ")${RAW_DELIMITER}\"")
+                math(EXPR CONTENT_START "${OPEN_PAREN} + 1")
+                math(EXPR REMAINING_LENGTH
+                    "${TEXT_LENGTH} - ${CONTENT_START}")
+                string(SUBSTRING "${TEXT}" ${CONTENT_START}
+                    ${REMAINING_LENGTH} REMAINING)
+                string(FIND "${REMAINING}" "${RAW_TERMINATOR}"
+                    RELATIVE_CLOSE)
+                if(RELATIVE_CLOSE GREATER_EQUAL 0)
+                    string(LENGTH "${RAW_TERMINATOR}" TERMINATOR_LENGTH)
+                    math(EXPR RAW_END
+                        "${CONTENT_START} + ${RELATIVE_CLOSE} + ${TERMINATOR_LENGTH}")
+                endif()
+            endif()
+        endif()
+    endif()
+    set("${OUT_END}" ${RAW_END} PARENT_SCOPE)
+endfunction()
+
 function(splice_cpp_lines SOURCE OUT_SOURCE)
     set(TEXT "${SOURCE}")
     string(LENGTH "${TEXT}" TEXT_LENGTH)
     set(OUTPUT "")
     set(INDEX 0)
     while(INDEX LESS TEXT_LENGTH)
         string(SUBSTRING "${TEXT}" ${INDEX} 1 CURRENT)
+        if(CURRENT STREQUAL "R")
+            set(RAW_END -1)
+            math(EXPR RAW_QUOTE_INDEX "${INDEX} + 1")
+            if(RAW_QUOTE_INDEX LESS TEXT_LENGTH)
+                string(SUBSTRING "${TEXT}" ${RAW_QUOTE_INDEX}
+                    1 RAW_QUOTE)
+                if(RAW_QUOTE STREQUAL "\"")
+                    find_cpp_raw_literal_end("${TEXT}" ${INDEX} RAW_END)
+                endif()
+            endif()
+            if(RAW_END GREATER_EQUAL 0)
+                math(EXPR RAW_LENGTH "${RAW_END} - ${INDEX}")
+                string(SUBSTRING "${TEXT}" ${INDEX} ${RAW_LENGTH}
+                    RAW_LITERAL)
+                string(APPEND OUTPUT "${RAW_LITERAL}")
+                set(INDEX ${RAW_END})
+                continue()
+            endif()
+        endif()
         if(CURRENT STREQUAL "\\")
             math(EXPR NEXT_INDEX "${INDEX} + 1")
             if(NEXT_INDEX LESS TEXT_LENGTH)
                 string(SUBSTRING "${TEXT}" ${NEXT_INDEX} 1 NEXT)
                 if(NEXT STREQUAL "\n")
                     math(EXPR INDEX "${INDEX} + 2")
                     continue()
                 endif()
                 if(NEXT STREQUAL "\r")
                     math(EXPR AFTER_CR_INDEX "${INDEX} + 2")
@@ -43,63 +115,26 @@ endfunction()
 function(mask_cpp_literals SOURCE OUT_SOURCE)
     set(TEXT "${SOURCE}")
     string(LENGTH "${TEXT}" TEXT_LENGTH)
     set(OUTPUT "")
     set(INDEX 0)
     while(INDEX LESS TEXT_LENGTH)
         string(SUBSTRING "${TEXT}" ${INDEX} 1 CURRENT)
 
         set(RAW_END -1)
         if(CURRENT STREQUAL "R")
-            math(EXPR NEXT_INDEX "${INDEX} + 1")
-            if(NEXT_INDEX LESS TEXT_LENGTH)
-                string(SUBSTRING "${TEXT}" ${NEXT_INDEX} 1 NEXT)
-                if(NEXT STREQUAL "\"")
-                    math(EXPR DELIMITER_START "${INDEX} + 2")
-                    set(SCAN ${DELIMITER_START})
-                    set(OPEN_PAREN -1)
-                    while(SCAN LESS TEXT_LENGTH)
-                        math(EXPR DELIMITER_LENGTH
-                            "${SCAN} - ${DELIMITER_START}")
-                        if(DELIMITER_LENGTH GREATER 16)
-                            break()
-                        endif()
-                        string(SUBSTRING "${TEXT}" ${SCAN} 1 RAW_CHAR)
-                        if(RAW_CHAR STREQUAL "(")
-                            set(OPEN_PAREN ${SCAN})
-                            break()
-                        endif()
-                        if(RAW_CHAR STREQUAL "\r" OR RAW_CHAR STREQUAL "\n")
-                            break()
-                        endif()
-                        math(EXPR SCAN "${SCAN} + 1")
-                    endwhile()
-                    if(OPEN_PAREN GREATER_EQUAL 0)
-                        math(EXPR DELIMITER_LENGTH
-                            "${OPEN_PAREN} - ${DELIMITER_START}")
-                        string(SUBSTRING "${TEXT}" ${DELIMITER_START}
-                            ${DELIMITER_LENGTH} RAW_DELIMITER)
-                        set(RAW_TERMINATOR ")${RAW_DELIMITER}\"")
-                        math(EXPR CONTENT_START "${OPEN_PAREN} + 1")
-                        math(EXPR REMAINING_LENGTH
-                            "${TEXT_LENGTH} - ${CONTENT_START}")
-                        string(SUBSTRING "${TEXT}" ${CONTENT_START}
-                            ${REMAINING_LENGTH} REMAINING)
-                        string(FIND "${REMAINING}" "${RAW_TERMINATOR}"
-                            RELATIVE_CLOSE)
-                        if(RELATIVE_CLOSE GREATER_EQUAL 0)
-                            string(LENGTH "${RAW_TERMINATOR}"
-                                TERMINATOR_LENGTH)
-                            math(EXPR RAW_END
-                                "${CONTENT_START} + ${RELATIVE_CLOSE} + ${TERMINATOR_LENGTH}")
-                        endif()
-                    endif()
+            math(EXPR RAW_QUOTE_INDEX "${INDEX} + 1")
+            if(RAW_QUOTE_INDEX LESS TEXT_LENGTH)
+                string(SUBSTRING "${TEXT}" ${RAW_QUOTE_INDEX}
+                    1 RAW_QUOTE)
+                if(RAW_QUOTE STREQUAL "\"")
+                    find_cpp_raw_literal_end("${TEXT}" ${INDEX} RAW_END)
                 endif()
             endif()
         endif()
 
         if(RAW_END GREATER_EQUAL 0)
             while(INDEX LESS RAW_END)
                 string(SUBSTRING "${TEXT}" ${INDEX} 1 MASKED_CHAR)
                 if(MASKED_CHAR STREQUAL "\r" OR MASKED_CHAR STREQUAL "\n")
                     string(APPEND OUTPUT "${MASKED_CHAR}")
                 else()
@@ -316,20 +351,61 @@ if(NOT GUARD_SPLICE_CRLF_CODE MATCHES
 endif()
 foreach(CRLF_FAKE_CALL IN ITEMS
         IsMouseButtonDown IsKeyUp GetMouseWheelMoveV GetCharPressed)
     if(GUARD_SPLICE_CRLF_CODE MATCHES
             "${CRLF_FAKE_CALL}[ \t\r\n]*\\(")
         message(FATAL_ERROR
             "guard CRLF splice retained non-code ${CRLF_FAKE_CALL} call")
     endif()
 endforeach()
 
+set(GUARD_RAW_PRESERVE_LF_SOURCE [=[
+const char* raw = u8R"tag(raw content
+)tag\
+"
+IsKeyDown(KEY_FAKE);
+)tag";
+IsKeyReleased(KEY_REAL);
+]=])
+strip_non_code("${GUARD_RAW_PRESERVE_LF_SOURCE}"
+    GUARD_RAW_PRESERVE_LF_CODE)
+require_match_count("${GUARD_RAW_PRESERVE_LF_CODE}"
+    "${DIRECT_INPUT_PATTERN}" 1 "guard raw LF real calls")
+if(NOT GUARD_RAW_PRESERVE_LF_CODE MATCHES
+        "IsKeyReleased[ \t\r\n]*\\(")
+    message(FATAL_ERROR "guard raw LF lost call after real terminator")
+endif()
+if(GUARD_RAW_PRESERVE_LF_CODE MATCHES "IsKeyDown[ \t\r\n]*\\(")
+    message(FATAL_ERROR "guard raw LF accepted call after split fake terminator")
+endif()
+
+string(CONCAT GUARD_RAW_PRESERVE_CRLF_SOURCE
+    "const char* raw = LR\"crlf(raw content\n"
+    ")crlf${BACKSLASH}${CARRIAGE_RETURN}\n"
+    "\"\n"
+    "GetMousePosition();\n"
+    ")crlf\";\n"
+    "GetMouseDelta();\n")
+strip_non_code("${GUARD_RAW_PRESERVE_CRLF_SOURCE}"
+    GUARD_RAW_PRESERVE_CRLF_CODE)
+require_match_count("${GUARD_RAW_PRESERVE_CRLF_CODE}"
+    "${DIRECT_INPUT_PATTERN}" 1 "guard raw CRLF real calls")
+if(NOT GUARD_RAW_PRESERVE_CRLF_CODE MATCHES
+        "GetMouseDelta[ \t\r\n]*\\(")
+    message(FATAL_ERROR "guard raw CRLF lost call after real terminator")
+endif()
+if(GUARD_RAW_PRESERVE_CRLF_CODE MATCHES
+        "GetMousePosition[ \t\r\n]*\\(")
+    message(FATAL_ERROR
+        "guard raw CRLF accepted call after split fake terminator")
+endif()
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
