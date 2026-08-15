cmake_minimum_required(VERSION 3.25)

if(NOT DEFINED HUD_NOTICE_SOURCE
        OR NOT EXISTS "${HUD_NOTICE_SOURCE}"
        OR NOT DEFINED GUARD_SCRIPT
        OR NOT EXISTS "${GUARD_SCRIPT}"
        OR NOT DEFINED MUTATION_ROOT)
    message(FATAL_ERROR "HUD notice guard self-test inputs are required")
endif()

file(REMOVE_RECURSE "${MUTATION_ROOT}")
file(MAKE_DIRECTORY "${MUTATION_ROOT}")
file(READ "${HUD_NOTICE_SOURCE}" ORIGINAL)

function(run_guard LABEL SOURCE_TEXT EXPECT_SUCCESS)
    set(MUTANT "${MUTATION_ROOT}/${LABEL}.cpp")
    file(WRITE "${MUTANT}" "${SOURCE_TEXT}")
    execute_process(
        COMMAND "${CMAKE_COMMAND}"
            "-DHUD_NOTICE_SOURCE=${MUTANT}"
            -P "${GUARD_SCRIPT}"
        RESULT_VARIABLE RESULT
        OUTPUT_QUIET
        ERROR_QUIET)
    if(EXPECT_SUCCESS AND NOT RESULT EQUAL 0)
        message(FATAL_ERROR "HUD notice guard rejected clean source: ${LABEL}")
    endif()
    if(NOT EXPECT_SUCCESS AND RESULT EQUAL 0)
        message(FATAL_ERROR "HUD notice guard missed mutation: ${LABEL}")
    endif()
endfunction()

run_guard(clean "${ORIGINAL}" TRUE)

set(OBSERVE_DIRECT_HINTS_MEMBER
    "static_cast<void>(hints.bindings);\n    const bool new_commit =")
string(REPLACE "const bool new_commit ="
    "${OBSERVE_DIRECT_HINTS_MEMBER}" MUTATED "${ORIGINAL}")
run_guard(observe_live_direct_hints_member "${MUTATED}" FALSE)

set(OBSERVE_LOOP_LOOKUP
    "const char* parsed_key = u8\"未知\";\n    for (const char* cursor = notices_[0].text.bytes.data(); *cursor != '\\0'; ++cursor) {\n        if (*cursor == ':') parsed_key = cursor + 1;\n    }\n    static_cast<void>(parsed_key);\n    if (context_changed && exits_unlocked && current.has_hole) {")
string(REPLACE
    "if (context_changed && exits_unlocked && current.has_hole) {"
    "${OBSERVE_LOOP_LOOKUP}" MUTATED "${ORIGINAL}")
run_guard(observe_live_handwritten_display_lookup "${MUTATED}" FALSE)

set(DEAD_REQUIRED_CALL
    "if (false) {\n        format_notice(notice, \"%s %s\",\n            control_hint_key_label(hints, action), suffix);\n    }")
set(LOOP_LOOKUP
    "${DEAD_REQUIRED_CALL}\n    const char* parsed_key = u8\"未知\";\n    for (const char* cursor = suffix; *cursor != '\\0'; ++cursor) {\n        if (*cursor == ':') parsed_key = cursor + 1;\n    }\n    format_notice(notice, \"%s %s\", parsed_key, suffix);")
string(REPLACE
    "format_notice(notice, \"%s %s\",\n        control_hint_key_label(hints, action), suffix);"
    "${LOOP_LOOKUP}" MUTATED "${ORIGINAL}")
run_guard(dead_required_call_with_loop_lookup "${MUTATED}" FALSE)

set(STD_SEARCH_LOOKUP
    "${DEAD_REQUIRED_CALL}\n    const char* parsed_key = std::search(suffix, suffix + 4, suffix, suffix + 1);\n    format_notice(notice, \"%s %s\", parsed_key, suffix);")
string(REPLACE
    "format_notice(notice, \"%s %s\",\n        control_hint_key_label(hints, action), suffix);"
    "${STD_SEARCH_LOOKUP}" MUTATED "${ORIGINAL}")
run_guard(dead_required_call_with_std_search "${MUTATED}" FALSE)

set(COMMENTED_FORBIDDEN "${ORIGINAL}\n// memcmp(hints.primary, hints.secondary)\n/* std::strstr std::strchr std::string_view::find */\n")
run_guard(forbidden_tokens_in_comments_are_ignored "${COMMENTED_FORBIDDEN}" TRUE)

string(REPLACE "control_hint_key_label(hints, action)"
    "legacy_action_key(hints, action)" MUTATED "${ORIGINAL}")
run_guard(missing_structured_query "${MUTATED}" FALSE)

string(REPLACE "control_hint_key_label(hints, action)"
    "legacy_action_key(hints, action)"
    MUTATED "${ORIGINAL}")
set(MUTATED "${MUTATED}\n// control_hint_key_label(hints, action)\n")
run_guard(required_query_only_in_comment "${MUTATED}" FALSE)

string(REPLACE "control_hint_key_label(hints, action)"
    "legacy_action_key(hints, action)"
    MUTATED "${ORIGINAL}")
string(REPLACE "HudNotice notice{};"
    "if (false) { static_cast<void>(control_hint_key_label(hints, action)); }\n    HudNotice notice{};"
    MUTATED "${MUTATED}")
run_guard(required_query_only_in_dead_code "${MUTATED}" FALSE)

string(REPLACE "settings::SettingAction::interact" "\"Interact\""
    MUTATED "${ORIGINAL}")
run_guard(english_interact_literal "${MUTATED}" FALSE)

string(REPLACE "settings::SettingAction::inventory" "\"Inventory\""
    MUTATED "${ORIGINAL}")
run_guard(english_inventory_literal "${MUTATED}" FALSE)

string(REPLACE "settings::SettingAction::passive_tree" "\"Passive Tree\""
    MUTATED "${ORIGINAL}")
run_guard(english_passive_literal "${MUTATED}" FALSE)

string(REPLACE "format_notice(notice, \"%s %s\","
    "std::memcmp(hints.secondary.data(), action, 1);\n    format_notice(notice, \"%s %s\","
    MUTATED "${ORIGINAL}")
run_guard(display_text_memcmp "${MUTATED}" FALSE)

string(REPLACE "format_notice(notice, \"%s %s\","
    "const char* parsed_action = std::strstr(suffix, u8\"交互\");\n    static_cast<void>(parsed_action);\n    format_notice(notice, \"%s %s\","
    MUTATED "${ORIGINAL}")
run_guard(display_text_strstr_with_required_tokens "${MUTATED}" FALSE)

string(REPLACE "format_notice(notice, \"%s %s\","
    "const auto parsed_action = std::string_view{suffix}.find(u8\"打开\");\n    static_cast<void>(parsed_action);\n    format_notice(notice, \"%s %s\","
    MUTATED "${ORIGINAL}")
run_guard(display_text_string_view_find_with_required_tokens "${MUTATED}" FALSE)
