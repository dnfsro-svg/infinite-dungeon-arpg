cmake_minimum_required(VERSION 3.25)

if(NOT DEFINED PARSER_SCRIPT OR NOT EXISTS "${PARSER_SCRIPT}"
        OR NOT DEFINED MUTATION_ROOT)
    message(FATAL_ERROR "C++ source parser self-test inputs are required")
endif()

file(REMOVE_RECURSE "${MUTATION_ROOT}")
file(MAKE_DIRECTORY "${MUTATION_ROOT}")

function(run_parser_case LABEL SOURCE MARKER EXPECT_SUCCESS EXPECTED_BODY
        EXPECTED_ERROR)
    set(ROOT "${MUTATION_ROOT}/${LABEL}")
    file(MAKE_DIRECTORY "${ROOT}")
    file(WRITE "${ROOT}/source.cpp" "${SOURCE}")
    file(WRITE "${ROOT}/marker.txt" "${MARKER}")
    file(WRITE "${ROOT}/expected.txt" "${EXPECTED_BODY}")
    file(WRITE "${ROOT}/runner.cmake"
        "cmake_minimum_required(VERSION 3.25)\n"
        "include(\"${PARSER_SCRIPT}\")\n"
        "file(READ \"${ROOT}/source.cpp\" SOURCE)\n"
        "file(READ \"${ROOT}/marker.txt\" MARKER)\n"
        "file(READ \"${ROOT}/expected.txt\" EXPECTED)\n"
        "extract_cpp_function_body(\"\${SOURCE}\" \"\${MARKER}\" BODY CODE)\n"
        "if(NOT BODY STREQUAL EXPECTED)\n"
        "  message(FATAL_ERROR \"unexpected preserved body: [\${BODY}]\")\n"
        "endif()\n"
        "string(FIND \"\${CODE}\" \"if (ready) { consume(); }\" REQUIRED_CODE)\n"
        "if(REQUIRED_CODE EQUAL -1)\n"
        "  message(FATAL_ERROR \"real code missing from code projection\")\n"
        "endif()\n"
        "string(FIND \"\${CODE}\" \"const auto decimal = 1'000; const auto decimal_zero = 10'000; const auto hex = 0xFF'00;\" DIGIT_SEPARATORS)\n"
        "if(DIGIT_SEPARATORS EQUAL -1)\n"
        "  message(FATAL_ERROR \"digit separators containing zero missing from code projection: [\${CODE}]\")\n"
        "endif()\n"
        "foreach(FORBIDDEN IN ITEMS http://x /*x*/ literal-braces raw-comment fake-marker)\n"
        "  string(FIND \"\${CODE}\" \"\${FORBIDDEN}\" FOUND)\n"
        "  if(NOT FOUND EQUAL -1)\n"
        "    message(FATAL_ERROR \"literal/comment content leaked into code projection\")\n"
        "  endif()\n"
        "endforeach()\n")
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -P "${ROOT}/runner.cmake"
        RESULT_VARIABLE RESULT
        OUTPUT_VARIABLE OUTPUT
        ERROR_VARIABLE ERROR)
    if(EXPECT_SUCCESS AND NOT RESULT EQUAL 0)
        message(FATAL_ERROR
            "C++ source parser rejected positive case ${LABEL}: ${OUTPUT}${ERROR}")
    endif()
    if(NOT EXPECT_SUCCESS AND RESULT EQUAL 0)
        message(FATAL_ERROR
            "C++ source parser accepted malformed case ${LABEL}")
    endif()
    if(NOT EXPECT_SUCCESS)
        string(FIND "${OUTPUT}${ERROR}" "${EXPECTED_ERROR}" ERROR_MATCH)
        if(ERROR_MATCH EQUAL -1)
            message(FATAL_ERROR
                "C++ source parser failed ${LABEL} for the wrong reason: ${OUTPUT}${ERROR}")
        endif()
    endif()
endfunction()

set(POSITIVE_SOURCE [=[void ignored() {}
void target() {
    const char* url = "http://x";
    const char* comment_text = "/*x*/";
    const char* escaped = "escaped \" quote and } literal-braces";
    const char close = '}';
    const char quote = '\'';
    const char* raw = R"tag({ raw-comment // /* } })tag";
    // } void fake-marker() {
    /* { fake-marker } */
    const auto decimal = 1'000;
    const auto decimal_zero = 10'000;
    const auto hex = 0xFF'00;
    if (ready) { consume(); }
}
void after() { reject(); }
]=])
string(REPLACE "\n" "\r\n" POSITIVE_SOURCE "${POSITIVE_SOURCE}")
set(POSITIVE_EXPECTED
    "const char* url = \"http://x\"; const char* comment_text = \"/*x*/\"; const char* escaped = \"escaped \\\" quote and } literal-braces\"; const char close = '}'; const char quote = '\\''; const char* raw = R\"tag({ raw-comment // /* } })tag\"; const auto decimal = 1'000; const auto decimal_zero = 10'000; const auto hex = 0xFF'00; if (ready) { consume(); }")
run_parser_case(crlf_literals_raw_nested "${POSITIVE_SOURCE}"
    "void target()" TRUE "${POSITIVE_EXPECTED}" "")

run_parser_case(missing_function "void other() {}" "void target()" FALSE ""
    "Unable to find C++ function marker")
run_parser_case(unclosed_brace "void target() { if (ready) { consume(); }"
    "void target()" FALSE "" "Unbalanced C++ function body")
run_parser_case(unclosed_block_comment "void target() { /* never closed"
    "void target()" FALSE "" "Unterminated C++ lexical construct")
run_parser_case(unclosed_string "void target() { const char* x = \"never closed; }"
    "void target()" FALSE "" "Unterminated C++ lexical construct")
run_parser_case(unclosed_raw "void target() { const char* x = R\"tag(raw }"
    "void target()" FALSE "" "Unterminated C++ lexical construct")
