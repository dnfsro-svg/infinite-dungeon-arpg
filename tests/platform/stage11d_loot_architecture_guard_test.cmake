if(NOT DEFINED SOURCE_ROOT)
    message(FATAL_ERROR "Stage11D loot guard requires SOURCE_ROOT")
endif()

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

function(arpg_stage11d_sanitize_cpp SOURCE_TEXT KEEP_LITERALS OUT_TEXT)
    string(LENGTH "${SOURCE_TEXT}" _source_length)
    set(_result "")
    set(_state code)
    set(_index 0)
    while(_index LESS _source_length)
        string(SUBSTRING "${SOURCE_TEXT}" ${_index} 1 _character)
        math(EXPR _next_index "${_index} + 1")
        set(_next_character "")
        if(_next_index LESS _source_length)
            string(SUBSTRING "${SOURCE_TEXT}" ${_next_index} 1
                _next_character)
        endif()

        if(_state STREQUAL code)
            if(_character STREQUAL "/" AND _next_character STREQUAL "/")
                string(APPEND _result "  ")
                set(_state line_comment)
                math(EXPR _index "${_index} + 2")
                continue()
            elseif(_character STREQUAL "/" AND _next_character STREQUAL "*")
                string(APPEND _result "  ")
                set(_state block_comment)
                math(EXPR _index "${_index} + 2")
                continue()
            elseif(_character STREQUAL "\"")
                if(KEEP_LITERALS)
                    string(APPEND _result "\"")
                else()
                    string(APPEND _result " ")
                endif()
                set(_state string_literal)
            elseif(_character STREQUAL "'")
                if(KEEP_LITERALS)
                    string(APPEND _result "'")
                else()
                    string(APPEND _result " ")
                endif()
                set(_state character_literal)
            else()
                string(APPEND _result "${_character}")
            endif()
        elseif(_state STREQUAL line_comment)
            if(_character STREQUAL "\n" OR _character STREQUAL "\r")
                string(APPEND _result "${_character}")
                set(_state code)
            else()
                string(APPEND _result " ")
            endif()
        elseif(_state STREQUAL block_comment)
            if(_character STREQUAL "*" AND _next_character STREQUAL "/")
                string(APPEND _result "  ")
                set(_state code)
                math(EXPR _index "${_index} + 2")
                continue()
            elseif(_character STREQUAL "\n" OR _character STREQUAL "\r")
                string(APPEND _result "${_character}")
            else()
                string(APPEND _result " ")
            endif()
        elseif(_state STREQUAL string_literal
                OR _state STREQUAL character_literal)
            if(_character STREQUAL "\\" AND _next_index LESS _source_length)
                if(KEEP_LITERALS)
                    string(APPEND _result "${_character}${_next_character}")
                else()
                    string(APPEND _result "  ")
                endif()
                math(EXPR _index "${_index} + 2")
                continue()
            endif()
            if((_state STREQUAL string_literal AND _character STREQUAL "\"")
                    OR (_state STREQUAL character_literal
                        AND _character STREQUAL "'"))
                set(_state code)
            endif()
            if(KEEP_LITERALS)
                string(APPEND _result "${_character}")
            elseif(_character STREQUAL "\n" OR _character STREQUAL "\r")
                string(APPEND _result "${_character}")
            else()
                string(APPEND _result " ")
            endif()
        endif()
        math(EXPR _index "${_index} + 1")
    endwhile()
    if(_state STREQUAL block_comment OR _state STREQUAL string_literal
            OR _state STREQUAL character_literal)
        message(FATAL_ERROR
            "Stage11D source sanitizer found an unterminated ${_state}")
    endif()
    set("${OUT_TEXT}" "${_result}" PARENT_SCOPE)
endfunction()

function(arpg_stage11d_assert_excludes REASON REGEX)
    foreach(_source_file IN LISTS ARGN)
        if(NOT EXISTS "${_source_file}")
            message(FATAL_ERROR "${REASON}: source is missing: ${_source_file}")
        endif()
        file(READ "${_source_file}" _source_text)
        arpg_stage11d_sanitize_cpp("${_source_text}" FALSE _source_code)
        if(_source_code MATCHES "${REGEX}")
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
        string(REGEX REPLACE
            "[/][*]([^*]|[*]+[^*/])*[*]+[/]" ""
            _include_source "${_source_text}")
        string(REGEX REPLACE "//[^\r\n]*" "" _include_source
            "${_include_source}")
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
    ${_dungeon_sources})
arpg_stage11d_assert_include_paths_exclude(
    "settings must not include dungeon"
    "(^|[^a-z0-9_])dungeon/"
    ${_settings_sources})

set(_presentation_boundary_sources ${_ground_view_sources} ${_feedback_sources})
arpg_stage11d_assert_excludes(
    "loot presentation rejects dynamic strings or containers"
    "std[ \t\r\n]*::[ \t\r\n]*(pmr[ \t\r\n]*::[ \t\r\n]*)?(basic_string|string|wstring|u8string|u16string|u32string|vector|deque|list|forward_list|map|multimap|unordered_map|unordered_multimap|set|multiset|unordered_set|unordered_multiset|stack|queue|priority_queue)[^A-Za-z0-9_]"
    ${_presentation_boundary_sources})
arpg_stage11d_assert_excludes(
    "loot presentation rejects physical input sampling"
    "(^|[^A-Za-z0-9_])(IsKeyDown|IsKeyPressed|IsKeyReleased|IsKeyUp|GetKeyPressed|GetCharPressed|IsMouseButtonDown|IsMouseButtonPressed|IsMouseButtonReleased|GetMousePosition|GetMouseDelta|GetMouseWheelMove)[ \t\r\n]*[(]"
    ${_presentation_boundary_sources})
arpg_stage11d_assert_excludes(
    "loot presentation rejects session calls"
    "(DungeonSession([^A-Za-z0-9_]|$)|(^|[^A-Za-z0-9_])[A-Za-z_][A-Za-z0-9_]*[ \t\r\n]*([.]|[-][>])[ \t\r\n]*(tick|snapshot|request[A-Za-z0-9_]*|item_state|pending_[A-Za-z0-9_]*|resolve_pending_save|try_pop_[A-Za-z0-9_]*)[ \t\r\n]*[(])"
    ${_presentation_boundary_sources})
arpg_stage11d_assert_excludes(
    "loot presentation rejects save/store access"
    "((SettingsStore|SaveStore)([^A-Za-z0-9_]|$)|(^|[^A-Za-z0-9_])[A-Za-z_][A-Za-z0-9_]*[ \t\r\n]*([.]|[-][>])[ \t\r\n]*(load|save|commit|recover[A-Za-z0-9_]*)[ \t\r\n]*[(])"
    ${_presentation_boundary_sources})
arpg_stage11d_assert_excludes(
    "room renderer rejects direct save/store access"
    "((SettingsStore|SaveStore)([^A-Za-z0-9_]|$)|(^|[^A-Za-z0-9_])[A-Za-z_][A-Za-z0-9_]*[ \t\r\n]*([.]|[-][>])[ \t\r\n]*(load|save|commit|recover[A-Za-z0-9_]*)[ \t\r\n]*[(])"
    "${_room_renderer}")

function(arpg_stage11d_extract_function
        SOURCE_TEXT SIGNATURE_REGEX REASON OUT_TEXT)
    arpg_stage11d_sanitize_cpp("${SOURCE_TEXT}" FALSE _source_code)
    string(REGEX MATCH "${SIGNATURE_REGEX}" _signature "${_source_code}")
    if(_signature STREQUAL "")
        message(FATAL_ERROR "${REASON}: function signature is missing")
    endif()
    string(FIND "${_source_code}" "${_signature}" _signature_start)
    string(SUBSTRING "${_source_code}" ${_signature_start} -1 _tail)
    string(FIND "${_tail}" "{" _open_brace)
    if(_open_brace EQUAL -1)
        message(FATAL_ERROR "${REASON}: function body is missing")
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

file(READ "${_dungeon_transition}" _transition_text)
arpg_stage11d_extract_function("${_transition_text}"
    "DungeonSession[ \t\r\n]*::[ \t\r\n]*request_pickup[ \t\r\n]*[(]"
    "explicit pickup boundary is invalid"
    _explicit_pickup)
if(_explicit_pickup MATCHES
        "(AutoPickupPolicy|auto_pickup_eligible|minimum_rarity|ItemRarity|[.]rarity([^A-Za-z0-9_]|$))")
    message(FATAL_ERROR
        "explicit pickup must not apply loot rarity policy: ${_dungeon_transition}")
endif()

arpg_stage11d_extract_function("${_transition_text}"
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
string(REGEX MATCH
    "if[(](${_ground_parameter}[.]source==GroundItemSource::abyss_chest|GroundItemSource::abyss_chest==${_ground_parameter}[.]source)[)][{]?returntrue;[}]?"
    _abyss_bypass_clause "${_normalized_policy}")
if(_abyss_bypass_clause STREQUAL "")
    message(FATAL_ERROR
        "automatic pickup policy must preserve abyss bypass: ${_dungeon_transition}")
endif()
string(REGEX MATCH
    "return(${_ground_parameter}[.]source==GroundItemSource::monster_drop|GroundItemSource::monster_drop==${_ground_parameter}[.]source)&&static_cast<std::uint8_t>[(]${_ground_parameter}[.]item[.]rarity[)]>=static_cast<std::uint8_t>[(]${_policy_parameter}[.]minimum_rarity[)];"
    _rarity_return_clause "${_normalized_policy}")
if(_rarity_return_clause STREQUAL "")
    message(FATAL_ERROR
        "automatic pickup policy is incomplete: real monster rarity return is missing")
endif()
string(FIND "${_normalized_policy}" "${_abyss_bypass_clause}"
    _abyss_bypass_index)
string(FIND "${_normalized_policy}" "${_rarity_return_clause}"
    _rarity_return_index)
if(_abyss_bypass_index EQUAL -1 OR _rarity_return_index EQUAL -1)
    message(FATAL_ERROR
        "automatic pickup policy boundary is invalid: verified clauses are missing")
endif()
if(NOT _abyss_bypass_index LESS _rarity_return_index)
    message(FATAL_ERROR
        "automatic pickup abyss bypass must precede rarity return: ${_dungeon_transition}")
endif()

message(STATUS "Stage 11D loot architecture boundaries verified")
