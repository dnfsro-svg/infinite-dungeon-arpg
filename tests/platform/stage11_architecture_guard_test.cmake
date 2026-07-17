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

file(GLOB _overlay_sources LIST_DIRECTORIES FALSE
    "${SOURCE_ROOT}/platform/raylib/death_overlay*.h"
    "${SOURCE_ROOT}/platform/raylib/death_overlay*.hpp"
    "${SOURCE_ROOT}/platform/raylib/death_overlay*.c"
    "${SOURCE_ROOT}/platform/raylib/death_overlay*.cc"
    "${SOURCE_ROOT}/platform/raylib/death_overlay*.cpp"
    "${SOURCE_ROOT}/platform/raylib/death_overlay*.cxx")
if(DEFINED STAGE11_GUARD_MUTATION_KIND
        AND STAGE11_GUARD_MUTATION_KIND STREQUAL "overlay")
    list(APPEND _overlay_sources "${STAGE11_GUARD_MUTATION_FILE}")
endif()
arpg_assert_files_exclude("Death overlay private access"
    "(stable_state_|pending_save_|SaveStore|DungeonSessionTestAccess|CombatWorldTestAccess)"
    ${_overlay_sources})

file(GLOB_RECURSE _production_sources LIST_DIRECTORIES FALSE
    "${SOURCE_ROOT}/*.h" "${SOURCE_ROOT}/*.hh"
    "${SOURCE_ROOT}/*.hpp" "${SOURCE_ROOT}/*.hxx"
    "${SOURCE_ROOT}/*.c" "${SOURCE_ROOT}/*.cc" "${SOURCE_ROOT}/*.cpp"
    "${SOURCE_ROOT}/*.cxx")
if(DEFINED STAGE11_GUARD_MUTATION_KIND
        AND STAGE11_GUARD_MUTATION_KIND STREQUAL "production")
    list(APPEND _production_sources "${STAGE11_GUARD_MUTATION_FILE}")
endif()
arpg_assert_files_exclude("Production test-access invocation"
    "(DungeonSessionTestAccess|CombatWorldTestAccess)[ \t\r\n]*::"
    ${_production_sources})

set(_death_stress_source
    "${SOURCE_ROOT}/../tests/dungeon/dungeon_death_stress_tests.cpp")
if(DEFINED STAGE11_GUARD_MUTATION_KIND
        AND STAGE11_GUARD_MUTATION_KIND STREQUAL "fixture")
    set(_death_stress_source "${STAGE11_GUARD_MUTATION_FILE}")
endif()
arpg_assert_files_exclude("Death stress private-access loophole"
    "(DungeonSessionTestAccess|CombatWorldTestAccess|kill_current_player_through_combat|fill_ground_pool|handle_player_defeat|stable_state_[^A-Za-z0-9]|pending_save_[^A-Za-z0-9])"
    ${_death_stress_source})

file(READ "${_death_stress_source}" _death_stress_text)
string(FIND "${_death_stress_text}"
    "struct DungeonDeathStressFixture final {" _fixture_begin)
if(_fixture_begin EQUAL -1)
    message(FATAL_ERROR "Death stress fixture declaration is missing")
endif()
string(SUBSTRING "${_death_stress_text}" ${_fixture_begin} -1 _fixture_tail)
string(FIND "${_fixture_tail}" "}  // namespace arpg::test" _fixture_end)
if(_fixture_end EQUAL -1)
    set(_fixture_body "${_fixture_tail}")
else()
    string(SUBSTRING "${_fixture_tail}" 0 ${_fixture_end} _fixture_body)
endif()
string(REGEX MATCHALL "session\\.[A-Za-z_][A-Za-z0-9_]*"
    _fixture_session_accesses "${_fixture_body}")
foreach(_access IN LISTS _fixture_session_accesses)
    if(NOT _access STREQUAL "session.ground_items_")
        message(FATAL_ERROR
            "Death stress fixture private access is not allowed: ${_access}")
    endif()
endforeach()

if(NOT DEFINED STAGE11_GUARD_MUTATION_MODE)
    if(NOT DEFINED GUARD_TEST_ROOT)
        message(FATAL_ERROR "GUARD_TEST_ROOT is required for mutation probes")
    endif()
    file(MAKE_DIRECTORY "${GUARD_TEST_ROOT}")

    function(arpg_expect_guard_rejects NAME KIND CONTENT)
        set(_content "${CONTENT}")
        foreach(_fragment IN LISTS ARGN)
            string(APPEND _content "${_fragment}")
        endforeach()
        set(_fixture "${GUARD_TEST_ROOT}/${NAME}.txt")
        file(WRITE "${_fixture}" "${_content}")
        execute_process(
            COMMAND "${CMAKE_COMMAND}"
                "-DSOURCE_ROOT=${SOURCE_ROOT}"
                "-DSTAGE11_GUARD_MUTATION_MODE=1"
                "-DSTAGE11_GUARD_MUTATION_KIND=${KIND}"
                "-DSTAGE11_GUARD_MUTATION_FILE=${_fixture}"
                -P "${CMAKE_CURRENT_LIST_FILE}"
            RESULT_VARIABLE _result
            OUTPUT_QUIET ERROR_QUIET)
        if(_result EQUAL 0)
            message(FATAL_ERROR
                "Stage 11 guard failed to reject ${NAME} mutation")
        endif()
    endfunction()

    arpg_expect_guard_rejects(overlay_renderer overlay
        "SaveStore* forbidden_renderer_access;\n")
    arpg_expect_guard_rejects(overlay_header overlay
        "int stable_state_;\n")
    arpg_expect_guard_rejects(production_header production
        "inline void bad() { DungeonSessionTestAccess::invoke(); }\n")
    arpg_expect_guard_rejects(fixture_private fixture
        "struct DungeonDeathStressFixture final {\n"
        " static void bad(dungeon::DungeonSession& session) {\n"
        "  session.combat_.reset();\n"
        " }\n"
        "};\n")
    arpg_expect_guard_rejects(fixture_phase fixture
        "struct DungeonDeathStressFixture final {\n"
        " static void bad(dungeon::DungeonSession& session) {\n"
        "  session.phase_ = dungeon::RoomPhase::faulted;\n"
        " }\n"
        "};\n")
    arpg_expect_guard_rejects(fixture_diagnostics fixture
        "struct DungeonDeathStressFixture final {\n"
        " static void bad(dungeon::DungeonSession& session) {\n"
        "  session.diagnostics_ = {};\n"
        " }\n"
        "};\n")
endif()

message(STATUS "Stage 11 death architecture boundaries verified")
