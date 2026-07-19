if(NOT DEFINED SOURCE_ROOT)
    message(FATAL_ERROR "Stage11D loot guard requires SOURCE_ROOT")
endif()

include("${CMAKE_CURRENT_LIST_DIR}/cpp_source_lexer.cmake")

set(_dungeon_root "${SOURCE_ROOT}/dungeon")
set(_settings_root "${SOURCE_ROOT}/platform/settings")
set(_raylib_root "${SOURCE_ROOT}/platform/raylib")

file(GLOB_RECURSE _dungeon_sources LIST_DIRECTORIES FALSE
    "${_dungeon_root}/*.h" "${_dungeon_root}/*.hpp"
    "${_dungeon_root}/*.c" "${_dungeon_root}/*.cc"
    "${_dungeon_root}/*.cpp" "${_dungeon_root}/*.cxx")
file(GLOB_RECURSE _settings_sources LIST_DIRECTORIES FALSE
    "${_settings_root}/*.h" "${_settings_root}/*.hpp"
    "${_settings_root}/*.c" "${_settings_root}/*.cc"
    "${_settings_root}/*.cpp" "${_settings_root}/*.cxx")
if(NOT _dungeon_sources)
    message(FATAL_ERROR "Stage11D dungeon sources are missing: ${_dungeon_root}")
endif()
if(NOT _settings_sources)
    message(FATAL_ERROR "Stage11D settings sources are missing: ${_settings_root}")
endif()

set(_ground_view_sources
    "${_raylib_root}/ground_loot_view.hpp"
    "${_raylib_root}/ground_loot_view.cpp")
set(_feedback_sources
    "${_raylib_root}/loot_pickup_feedback.hpp"
    "${_raylib_root}/loot_pickup_feedback.cpp")
set(_room_renderer "${_raylib_root}/room_renderer.cpp")
set(_dungeon_transition "${_dungeon_root}/dungeon_transition.cpp")
foreach(_required IN LISTS _ground_view_sources _feedback_sources)
    if(NOT EXISTS "${_required}")
        message(FATAL_ERROR "Stage11D loot boundary source is missing: ${_required}")
    endif()
endforeach()
foreach(_required IN ITEMS "${_room_renderer}" "${_dungeon_transition}")
    if(NOT EXISTS "${_required}")
        message(FATAL_ERROR "Stage11D loot boundary source is missing: ${_required}")
    endif()
endforeach()

set(_dungeon_include_sources ${_dungeon_sources})
set(_settings_include_sources ${_settings_sources})
set(_checked_presentation_sources
    ${_ground_view_sources} ${_feedback_sources})
set(_check_room_renderer TRUE)
set(_check_dungeon_transition TRUE)
if(DEFINED STAGE11D_MUTATION_RELATIVE_FILE)
    set(_mutation_source
        "${SOURCE_ROOT}/${STAGE11D_MUTATION_RELATIVE_FILE}")
    if(STAGE11D_MUTATION_RELATIVE_FILE MATCHES "^dungeon/"
            AND NOT STAGE11D_MUTATION_RELATIVE_FILE STREQUAL
                "dungeon/dungeon_transition.cpp")
        set(_dungeon_include_sources "${_mutation_source}")
    else()
        set(_dungeon_include_sources "")
    endif()
    if(STAGE11D_MUTATION_RELATIVE_FILE MATCHES
            "^platform/settings/")
        set(_settings_include_sources "${_mutation_source}")
    else()
        set(_settings_include_sources "")
    endif()
    if(STAGE11D_MUTATION_RELATIVE_FILE MATCHES
            "^platform/raylib/(ground_loot_view|loot_pickup_feedback)[.]")
        set(_checked_presentation_sources "${_mutation_source}")
    else()
        set(_checked_presentation_sources "")
    endif()
    if(NOT STAGE11D_MUTATION_RELATIVE_FILE STREQUAL
            "platform/raylib/room_renderer.cpp")
        set(_check_room_renderer FALSE)
    endif()
    if(NOT STAGE11D_MUTATION_RELATIVE_FILE STREQUAL
            "dungeon/dungeon_transition.cpp")
        set(_check_dungeon_transition FALSE)
    endif()
endif()

function(arpg_stage11d_assert_excludes REASON REGEX)
    foreach(_source_file IN LISTS ARGN)
        if(NOT EXISTS "${_source_file}")
            message(FATAL_ERROR "${REASON}: source is missing: ${_source_file}")
        endif()
        file(READ "${_source_file}" _source_text)
        arpg_sanitize_cpp_source("${_source_text}" _source_code)
        if(_source_code MATCHES "${REGEX}")
            message(FATAL_ERROR "${REASON}: ${_source_file}")
        endif()
    endforeach()
endfunction()

function(arpg_stage11d_assert_named_member_calls_excluded
        REASON NAME_FRAGMENT METHOD_REGEX)
    foreach(_source_file IN LISTS ARGN)
        if(NOT EXISTS "${_source_file}")
            message(FATAL_ERROR "${REASON}: source is missing: ${_source_file}")
        endif()
        file(READ "${_source_file}" _source_text)
        arpg_sanitize_cpp_source("${_source_text}" _source_code)
        string(TOLOWER "${_source_code}" _source_lower)
        if(_source_lower MATCHES
                "(^|[^a-z0-9_])(${NAME_FRAGMENT}[a-z0-9_]*|[a-z_][a-z0-9_]*${NAME_FRAGMENT}[a-z0-9_]*)[ \t\r\n]*([.]|[-][>])[ \t\r\n]*(${METHOD_REGEX})[ \t\r\n]*[(]")
            message(FATAL_ERROR "${REASON}: ${_source_file}")
        endif()
    endforeach()
endfunction()

function(arpg_stage11d_assert_include_paths_exclude REASON REGEX)
    foreach(_source_file IN LISTS ARGN)
        if(NOT EXISTS "${_source_file}")
            message(FATAL_ERROR "${REASON}: source is missing: ${_source_file}")
        endif()
        file(READ "${_source_file}" _source_text)
        arpg_sanitize_cpp_source("${_source_text}" _include_source)
        string(REGEX MATCHALL
            "(^|[\r\n])[ \t]*#[ \t]*include[ \t]*[^\r\n]*"
            _include_lines "${_include_source}")
        foreach(_include_line IN LISTS _include_lines)
            string(REGEX REPLACE
                "^[\r\n \t]*#[ \t]*include[ \t]*" ""
                _include_operand "${_include_line}")
            string(REGEX REPLACE "[ \t]+$" "" _include_operand
                "${_include_operand}")
            if(NOT _include_operand MATCHES
                    "^(<[^>\r\n]+>|\"[^\"\r\n]+\")$")
                message(FATAL_ERROR
                    "include operands must be literal: ${_source_file}: ${_include_line}")
            endif()
            string(TOLOWER "${_include_operand}" _normalized_include)
            string(REPLACE "\\" "/" _normalized_include
                "${_normalized_include}")
            if(_normalized_include MATCHES "${REGEX}")
                message(FATAL_ERROR
                    "${REASON}: ${_source_file}: ${_include_line}")
            endif()
        endforeach()
    endforeach()
endfunction()

arpg_stage11d_assert_include_paths_exclude(
    "dungeon must not include settings or raylib"
    "(^|[^a-z0-9_])((platform/)?settings/|raylib[.]h|raymath[.]h|rlgl[.]h|(platform/)?raylib/)"
    ${_dungeon_include_sources})
arpg_stage11d_assert_include_paths_exclude(
    "settings must not include dungeon"
    "(^|[^a-z0-9_])dungeon/"
    ${_settings_include_sources})

arpg_stage11d_assert_excludes(
    "loot presentation rejects dynamic strings or containers"
    "std[ \t\r\n]*::[ \t\r\n]*(pmr[ \t\r\n]*::[ \t\r\n]*)?(basic_string|string|wstring|u8string|u16string|u32string|vector|deque|list|forward_list|map|multimap|unordered_map|unordered_multimap|set|multiset|unordered_set|unordered_multiset|stack|queue|priority_queue)[^A-Za-z0-9_]"
    ${_checked_presentation_sources})
arpg_stage11d_assert_excludes(
    "loot presentation rejects physical input sampling"
    "(^|[^A-Za-z0-9_])(IsKeyDown|IsKeyPressed|IsKeyReleased|IsKeyUp|GetKeyPressed|GetCharPressed|IsMouseButtonDown|IsMouseButtonPressed|IsMouseButtonReleased|GetMousePosition|GetMouseDelta|GetMouseWheelMove)[ \t\r\n]*[(]"
    ${_checked_presentation_sources})
arpg_stage11d_assert_excludes(
    "loot presentation rejects session calls"
    "DungeonSession([^A-Za-z0-9_]|$)"
    ${_checked_presentation_sources})
arpg_stage11d_assert_named_member_calls_excluded(
    "loot presentation rejects session calls" "session"
    "tick|snapshot|request[a-z0-9_]*|item_state|pending_[a-z0-9_]*|resolve_pending_save|try_pop_[a-z0-9_]*"
    ${_checked_presentation_sources})
arpg_stage11d_assert_excludes(
    "loot presentation rejects save/store access"
    "(SettingsStore|SaveStore)([^A-Za-z0-9_]|$)"
    ${_checked_presentation_sources})
arpg_stage11d_assert_named_member_calls_excluded(
    "loot presentation rejects save/store access" "store"
    "load|save|commit|recover[a-z0-9_]*"
    ${_checked_presentation_sources})
if(_check_room_renderer)
    arpg_stage11d_assert_excludes(
        "room renderer rejects direct save/store access"
        "(SettingsStore|SaveStore)([^A-Za-z0-9_]|$)"
        "${_room_renderer}")
    arpg_stage11d_assert_named_member_calls_excluded(
        "room renderer rejects direct save/store access" "store"
        "load|save|commit|recover[a-z0-9_]*"
        "${_room_renderer}")
endif()

function(arpg_stage11d_extract_function
        SOURCE_TEXT SIGNATURE_REGEX REASON OUT_TEXT)
    set(_source_code "${SOURCE_TEXT}")
    string(LENGTH "${_source_code}" _source_length)
    set(_search_offset 0)
    set(_signature_start -1)
    set(_open_brace -1)
    while(_search_offset LESS _source_length)
        string(SUBSTRING "${_source_code}" ${_search_offset} -1
            _search_tail)
        string(REGEX MATCH "${SIGNATURE_REGEX}" _signature "${_search_tail}")
        if(_signature STREQUAL "")
            break()
        endif()
        string(FIND "${_search_tail}" "${_signature}"
            _relative_signature_start)
        math(EXPR _candidate_start
            "${_search_offset} + ${_relative_signature_start}")
        string(SUBSTRING "${_source_code}" ${_candidate_start} -1
            _candidate_tail)
        string(FIND "${_candidate_tail}" "{" _candidate_brace)
        string(FIND "${_candidate_tail}" ";" _candidate_semicolon)
        if(_candidate_semicolon GREATER_EQUAL 0
                AND (_candidate_brace EQUAL -1
                    OR _candidate_semicolon LESS _candidate_brace))
            math(EXPR _search_offset
                "${_candidate_start} + ${_candidate_semicolon} + 1")
            continue()
        endif()
        if(_candidate_brace EQUAL -1)
            message(FATAL_ERROR "${REASON}: function body is missing")
        endif()
        set(_signature_start ${_candidate_start})
        set(_open_brace ${_candidate_brace})
        set(_tail "${_candidate_tail}")
        break()
    endwhile()
    if(_signature_start EQUAL -1)
        message(FATAL_ERROR "${REASON}: function definition is missing")
    endif()
    string(LENGTH "${_tail}" _tail_length)
    set(_depth 0)
    set(_body_end -1)
    set(_index ${_open_brace})
    while(_index LESS _tail_length)
        string(SUBSTRING "${_tail}" ${_index} 1 _character)
        if(_character STREQUAL "{")
            math(EXPR _depth "${_depth} + 1")
        elseif(_character STREQUAL "}")
            math(EXPR _depth "${_depth} - 1")
            if(_depth EQUAL 0)
                math(EXPR _body_end "${_index} + 1")
                break()
            elseif(_depth LESS 0)
                message(FATAL_ERROR "${REASON}: function braces are invalid")
            endif()
        endif()
        math(EXPR _index "${_index} + 1")
    endwhile()
    if(_body_end EQUAL -1)
        message(FATAL_ERROR "${REASON}: function body is unterminated")
    endif()
    string(SUBSTRING "${_tail}" 0 ${_body_end} _function_body)
    set("${OUT_TEXT}" "${_function_body}" PARENT_SCOPE)
endfunction()

if(_check_dungeon_transition)
file(READ "${_dungeon_transition}" _transition_text)
arpg_sanitize_cpp_source("${_transition_text}" _transition_code)
arpg_stage11d_extract_function("${_transition_code}"
    "DungeonSession[ \t\r\n]*::[ \t\r\n]*request_pickup[ \t\r\n]*[(]"
    "explicit pickup boundary is invalid"
    _explicit_pickup)
if(_explicit_pickup MATCHES
        "(AutoPickupPolicy|auto_pickup_eligible|minimum_rarity|ItemRarity|[.]rarity([^A-Za-z0-9_]|$))")
    message(FATAL_ERROR
        "explicit pickup must not apply loot rarity policy: ${_dungeon_transition}")
endif()

arpg_stage11d_extract_function("${_transition_code}"
    "bool[ \t\r\n]+auto_pickup_eligible[ \t\r\n]*[(]"
    "automatic pickup policy boundary is invalid"
    _automatic_policy)
string(REGEX REPLACE "[ \t\r\n]" "" _normalized_policy
    "${_automatic_policy}")
if(NOT _normalized_policy MATCHES
        "auto_pickup_eligible[(]constGroundItem&([A-Za-z_][A-Za-z0-9_]*),AutoPickupPolicy([A-Za-z_][A-Za-z0-9_]*)[)]")
    message(FATAL_ERROR
        "automatic pickup policy boundary is invalid: parameter seam is missing")
endif()
set(_ground_parameter "${CMAKE_MATCH_1}")
set(_policy_parameter "${CMAKE_MATCH_2}")
set(_abyss_clause_regex
    "if[(](${_ground_parameter}[.]source==GroundItemSource::abyss_chest|GroundItemSource::abyss_chest==${_ground_parameter}[.]source)[)][{]?returntrue;[}]?"
)
set(_rarity_clause_regex
    "return(${_ground_parameter}[.]source==GroundItemSource::monster_drop|GroundItemSource::monster_drop==${_ground_parameter}[.]source)&&static_cast<std::uint8_t>[(]${_ground_parameter}[.]item[.]rarity[)]>=static_cast<std::uint8_t>[(]${_policy_parameter}[.]minimum_rarity[)];"
)
if(NOT _normalized_policy MATCHES "${_abyss_clause_regex}")
    message(FATAL_ERROR
        "automatic pickup policy must preserve abyss bypass: ${_dungeon_transition}")
endif()
if(NOT _normalized_policy MATCHES "${_rarity_clause_regex}")
    message(FATAL_ERROR
        "automatic pickup policy is incomplete: real monster rarity return is missing")
endif()
set(_canonical_policy_regex
    "^boolauto_pickup_eligible[(]constGroundItem&${_ground_parameter},AutoPickupPolicy${_policy_parameter}[)]noexcept[{]if[(]!${_ground_parameter}[.]active[)][{]?returnfalse;[}]?${_abyss_clause_regex}${_rarity_clause_regex}[}]$")
if(NOT _normalized_policy MATCHES "${_canonical_policy_regex}")
    message(FATAL_ERROR
        "automatic pickup policy must use canonical top-level body: ${_dungeon_transition}")
endif()
endif()

message(STATUS "Stage 11D loot architecture boundaries verified")
