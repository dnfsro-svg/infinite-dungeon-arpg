if(NOT DEFINED SOURCE_ROOT)
    message(FATAL_ERROR "SOURCE_ROOT is required")
endif()

function(arpg_assert_files_exclude LABEL REGEX)
    set(_files ${ARGN})
    foreach(_file IN LISTS _files)
        if(NOT EXISTS "${_file}")
            message(FATAL_ERROR "${LABEL} source is missing: ${_file}")
        endif()
        file(READ "${_file}" _source)
        if(_source MATCHES "${REGEX}")
            message(FATAL_ERROR "${LABEL} violates boundary: ${_file}")
        endif()
    endforeach()
endfunction()

file(GLOB_RECURSE _combat_sources LIST_DIRECTORIES FALSE
    "${SOURCE_ROOT}/combat/*.h" "${SOURCE_ROOT}/combat/*.hpp"
    "${SOURCE_ROOT}/combat/*.cpp")
arpg_assert_files_exclude("Combat -> Dungeon"
    "#[ \t]*include[ \t]*[<\"]dungeon[/\\\\]" ${_combat_sources})
arpg_assert_files_exclude("Combat -> Persistence"
    "#[ \t]*include[ \t]*[<\"]persistence[/\\\\]" ${_combat_sources})
arpg_assert_files_exclude("Combat -> raylib"
    "#[ \t]*include[ \t]*[<\"](raylib|raymath|rlgl)([/\\\\.]|[>\"])"
    ${_combat_sources})

set(_death_codec_sources
    "${SOURCE_ROOT}/persistence/checkpoint_codec.hpp"
    "${SOURCE_ROOT}/persistence/checkpoint_codec.cpp")
arpg_assert_files_exclude("Death codec -> room generation"
    "#[ \t]*include[ \t]*[<\"]dungeon[/\\\\]room_generation\\.hpp[>\"]"
    ${_death_codec_sources})
arpg_assert_files_exclude("Death codec -> CombatWorld"
    "(combat[/\\\\]combat_world\\.hpp|CombatWorld)"
    ${_death_codec_sources})
arpg_assert_files_exclude("Death codec -> raylib"
    "#[ \t]*include[ \t]*[<\"](raylib|raymath|rlgl)([/\\\\.]|[>\"])"
    ${_death_codec_sources})

set(_overlay_sources
    "${SOURCE_ROOT}/platform/raylib/death_overlay_view.hpp"
    "${SOURCE_ROOT}/platform/raylib/death_overlay_view.cpp")
arpg_assert_files_exclude("Death overlay private access"
    "(stable_state_|pending_save_|SaveStore|DungeonSessionTestAccess|CombatWorldTestAccess)"
    ${_overlay_sources})

file(GLOB_RECURSE _production_implementations LIST_DIRECTORIES FALSE
    "${SOURCE_ROOT}/*.c" "${SOURCE_ROOT}/*.cc" "${SOURCE_ROOT}/*.cpp"
    "${SOURCE_ROOT}/*.cxx")
arpg_assert_files_exclude("Production test-access invocation"
    "(DungeonSessionTestAccess|CombatWorldTestAccess)[ \t\r\n]*::"
    ${_production_implementations})

set(_death_stress_source
    "${SOURCE_ROOT}/../tests/dungeon/dungeon_death_stress_tests.cpp")
arpg_assert_files_exclude("Death stress private-access loophole"
    "(DungeonSessionTestAccess|CombatWorldTestAccess|kill_current_player_through_combat|fill_ground_pool|handle_player_defeat|stable_state_[^A-Za-z0-9]|pending_save_[^A-Za-z0-9])"
    ${_death_stress_source})

message(STATUS "Stage 11 death architecture boundaries verified")
