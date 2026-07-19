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

function(arpg_stage11d_assert_excludes REASON REGEX)
    foreach(_source_file IN LISTS ARGN)
        if(NOT EXISTS "${_source_file}")
            message(FATAL_ERROR "${REASON}: source is missing: ${_source_file}")
        endif()
        file(READ "${_source_file}" _source_text)
        if(_source_text MATCHES "${REGEX}")
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
        string(REGEX MATCHALL "#[ \t]*include[ \t]*[<\"][^>\"\r\n]+[>\"]"
            _include_lines "${_source_text}")
        foreach(_include_line IN LISTS _include_lines)
            string(TOLOWER "${_include_line}" _normalized_include)
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
    "std[ \t\r\n]*::[ \t\r\n]*(basic_string|string|vector|deque|list|map|multimap|unordered_map|set|multiset|unordered_set)[^A-Za-z0-9_]"
    ${_presentation_boundary_sources})
arpg_stage11d_assert_excludes(
    "loot presentation rejects physical input sampling"
    "(^|[^A-Za-z0-9_])(IsKeyDown|IsKeyPressed|IsKeyReleased|IsKeyUp|GetKeyPressed|GetCharPressed|IsMouseButtonDown|IsMouseButtonPressed|IsMouseButtonReleased|GetMousePosition|GetMouseDelta|GetMouseWheelMove)[ \t\r\n]*[(]"
    ${_presentation_boundary_sources})
arpg_stage11d_assert_excludes(
    "loot presentation rejects session calls"
    "(DungeonSession([^A-Za-z0-9_]|$)|(^|[^A-Za-z0-9_])[A-Za-z_][A-Za-z0-9_]*[ \t\r\n]*[.][ \t\r\n]*(tick|request_pickup|request_nearby_pickups|item_state|pending_save_view)[ \t\r\n]*[(])"
    ${_presentation_boundary_sources})
arpg_stage11d_assert_excludes(
    "loot presentation rejects SettingsStore access"
    "SettingsStore([^A-Za-z0-9_]|$)"
    ${_presentation_boundary_sources})
arpg_stage11d_assert_excludes(
    "room renderer rejects direct SettingsStore access"
    "(SettingsStore([^A-Za-z0-9_]|$)|settings_store)"
    "${_room_renderer}")

function(arpg_stage11d_extract_between
        SOURCE_TEXT START_TOKEN END_TOKEN REASON OUT_TEXT)
    string(FIND "${SOURCE_TEXT}" "${START_TOKEN}" _start)
    if(_start EQUAL -1)
        message(FATAL_ERROR "${REASON}: start function is missing")
    endif()
    string(SUBSTRING "${SOURCE_TEXT}" ${_start} -1 _tail)
    string(FIND "${_tail}" "${END_TOKEN}" _end)
    if(_end EQUAL -1)
        message(FATAL_ERROR "${REASON}: end function is missing")
    endif()
    string(SUBSTRING "${_tail}" 0 ${_end} _body)
    set("${OUT_TEXT}" "${_body}" PARENT_SCOPE)
endfunction()

file(READ "${_dungeon_transition}" _transition_text)
arpg_stage11d_extract_between("${_transition_text}"
    "DungeonSession::request_pickup"
    "bool auto_pickup_eligible"
    "explicit pickup boundary is invalid"
    _explicit_pickup)
if(_explicit_pickup MATCHES
        "(AutoPickupPolicy|auto_pickup_eligible|minimum_rarity|ItemRarity|[.]rarity([^A-Za-z0-9_]|$))")
    message(FATAL_ERROR
        "explicit pickup must not apply loot rarity policy: ${_dungeon_transition}")
endif()

arpg_stage11d_extract_between("${_transition_text}"
    "bool auto_pickup_eligible"
    "DungeonSession::request_nearby_pickups"
    "automatic pickup policy boundary is invalid"
    _automatic_policy)
string(REGEX REPLACE "//[^\r\n]*" "" _automatic_policy_code
    "${_automatic_policy}")
string(REGEX REPLACE "[ \t\r\n]" "" _normalized_policy
    "${_automatic_policy_code}")
if(NOT _normalized_policy MATCHES
        "boolauto_pickup_eligible[(]constGroundItem&([A-Za-z_][A-Za-z0-9_]*),AutoPickupPolicy[A-Za-z_][A-Za-z0-9_]*[)]")
    message(FATAL_ERROR
        "automatic pickup policy boundary is invalid: parameter seam is missing")
endif()
set(_ground_parameter "${CMAKE_MATCH_1}")
if(NOT _normalized_policy MATCHES
        "if[(](${_ground_parameter}[.]source==GroundItemSource::abyss_chest|GroundItemSource::abyss_chest==${_ground_parameter}[.]source)[)][{]?returntrue;")
    message(FATAL_ERROR
        "automatic pickup policy must preserve abyss bypass: ${_dungeon_transition}")
endif()
foreach(_required_policy_token IN ITEMS
        "GroundItemSource::monster_drop" "minimum_rarity" ".rarity")
    string(FIND "${_automatic_policy}" "${_required_policy_token}" _found)
    if(_found EQUAL -1)
        message(FATAL_ERROR
            "automatic pickup policy is incomplete: ${_required_policy_token}")
    endif()
endforeach()

message(STATUS "Stage 11D loot architecture boundaries verified")
