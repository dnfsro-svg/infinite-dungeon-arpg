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

string(REPLACE "control_hint_key_label(hints, action)"
    "legacy_action_key(hints, action)" MUTATED "${ORIGINAL}")
run_guard(missing_structured_query "${MUTATED}" FALSE)

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
