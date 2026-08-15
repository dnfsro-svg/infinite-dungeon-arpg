cmake_minimum_required(VERSION 3.25)

if(NOT DEFINED HUD_NOTICE_SOURCE OR NOT EXISTS "${HUD_NOTICE_SOURCE}")
    message(FATAL_ERROR "HUD notice source is required")
endif()

include("${CMAKE_CURRENT_LIST_DIR}/cpp_source_function_body.cmake")
file(READ "${HUD_NOTICE_SOURCE}" SOURCE)
normalize_cpp_source("${SOURCE}" NORMALIZED_SOURCE SOURCE_CODE)

foreach(FORBIDDEN_CODE_REGEX IN ITEMS
        "(std::)?strstr[ ]*\\("
        "(std::)?strchr[ ]*\\("
        "(std::)?memcmp[ ]*\\("
        "std::search[ ]*\\("
        "(std::)?string_view"
        "\\.[ ]*find[ ]*\\(")
    string(REGEX MATCH "${FORBIDDEN_CODE_REGEX}" FOUND "${SOURCE_CODE}")
    if(FOUND)
        message(FATAL_ERROR
            "HUD notice display-text lookup is forbidden in real code: ${FORBIDDEN_CODE_REGEX}")
    endif()
endforeach()

extract_cpp_function_body("${SOURCE}" "void add_action_notice("
    ADD_ACTION_BODY)
set(EXPECTED_ADD_ACTION_BODY
    "HudNotice notice{}; notice.kind = kind; notice.priority = priority; notice.seconds_left = kTransientSeconds; format_notice(notice, \"%s %s\", control_hint_key_label(hints, action), suffix); enqueue(notices, dropped_count, notice);")
if(NOT ADD_ACTION_BODY STREQUAL EXPECTED_ADD_ACTION_BODY)
    message(FATAL_ERROR
        "add_action_notice must be the exact structured-binding formatter")
endif()

extract_cpp_function_body("${SOURCE}"
    "void add_abyss_confirmation_notice(" ABYSS_BODY)
set(EXPECTED_ABYSS_BODY
    "const AbyssHudValues values = abyss_hud_values(current); if (values.confirmation_transition == dungeon::TransitionKind::descent) { add_action_notice(notices, dropped_count, HudNoticeKind::abyss_abandon, kAbyssPriority, hints, settings::SettingAction::interact, values.confirmation_label); return; } add_transient(notices, dropped_count, HudNoticeKind::abyss_abandon, kAbyssPriority, values.confirmation_label);")
if(NOT ABYSS_BODY STREQUAL EXPECTED_ABYSS_BODY)
    message(FATAL_ERROR
        "Abyss action notice must use the exact structured interact call")
endif()

extract_cpp_function_body("${SOURCE}" "void HudNoticeState::observe("
    OBSERVE_BODY OBSERVE_CODE)
foreach(REQUIRED_BLOCK IN ITEMS
        "if (context_changed && exits_unlocked && current.has_hole) { add_action_notice(notices_, dropped_count_, HudNoticeKind::hole_interact, kHolePriority, hints, settings::SettingAction::interact, u8\"交互：进入下一层\"); }"
        "if (context_changed && current.inventory_count != 0U) { add_action_notice(notices_, dropped_count_, HudNoticeKind::inventory, kInventoryPriority, hints, settings::SettingAction::inventory, u8\"打开背包\"); }"
        "if (context_changed && awaiting_exit && current.progression.unspent_passive_points != 0U) { add_action_notice(notices_, dropped_count_, HudNoticeKind::passive_tree, kPassiveTreePriority, hints, settings::SettingAction::passive_tree, u8\"打开被动树\"); }")
    string(FIND "${OBSERVE_BODY}" "${REQUIRED_BLOCK}" FOUND)
    if(FOUND EQUAL -1)
        message(FATAL_ERROR
            "HudNoticeState::observe structured action block missing")
    endif()
endforeach()
string(REGEX MATCHALL "add_action_notice\\(" OBSERVE_ACTION_CALLS
    "${OBSERVE_BODY}")
list(LENGTH OBSERVE_ACTION_CALLS OBSERVE_ACTION_CALL_COUNT)
if(NOT OBSERVE_ACTION_CALL_COUNT EQUAL 3)
    message(FATAL_ERROR
        "HudNoticeState::observe must contain exactly three action calls")
endif()

foreach(FORBIDDEN_OBSERVE_REGEX IN ITEMS
        "for[ ]*\\("
        "while[ ]*\\(")
    string(REGEX MATCH "${FORBIDDEN_OBSERVE_REGEX}" FOUND "${OBSERVE_CODE}")
    if(FOUND)
        message(FATAL_ERROR
            "HudNoticeState::observe must not scan display text: ${FORBIDDEN_OBSERVE_REGEX}")
    endif()
endforeach()

string(REGEX MATCHALL "hints[ ]*\\.[ ]*revision" HINTS_REVISION_READS
    "${OBSERVE_CODE}")
list(LENGTH HINTS_REVISION_READS HINTS_REVISION_READ_COUNT)
if(NOT HINTS_REVISION_READ_COUNT EQUAL 2)
    message(FATAL_ERROR
        "HudNoticeState::observe must contain exactly two control-hint revision reads")
endif()
string(REGEX REPLACE "hints[ ]*\\.[ ]*revision" ""
    OBSERVE_WITHOUT_REVISION_READS "${OBSERVE_CODE}")
string(REGEX MATCH "hints[ ]*\\." FOUND_DIRECT_HINTS_MEMBER
    "${OBSERVE_WITHOUT_REVISION_READS}")
if(FOUND_DIRECT_HINTS_MEMBER)
    message(FATAL_ERROR
        "HudNoticeState::observe must not directly read control-hint display members")
endif()
