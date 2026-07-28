if(NOT DEFINED SOURCE_ROOT)
    message(FATAL_ERROR "SOURCE_ROOT is required")
endif()

function(arpg_assert_files_exclude REASON REGEX)
    foreach(_file IN LISTS ARGN)
        if(NOT EXISTS "${_file}")
            message(FATAL_ERROR "${REASON}: source is missing: ${_file}")
        endif()
        file(READ "${_file}" _source)
        if(_source MATCHES "${REGEX}")
            message(FATAL_ERROR "${REASON}: ${_file}")
        endif()
    endforeach()
endfunction()

function(arpg_assert_files_exclude_case_insensitive REASON REGEX)
    string(TOLOWER "${REGEX}" _regex)
    foreach(_file IN LISTS ARGN)
        if(NOT EXISTS "${_file}")
            message(FATAL_ERROR "${REASON}: source is missing: ${_file}")
        endif()
        file(READ "${_file}" _source)
        string(TOLOWER "${_source}" _source_lower)
        if(_source_lower MATCHES "${_regex}")
            message(FATAL_ERROR "${REASON}: ${_file}")
        endif()
    endforeach()
endfunction()

function(arpg_assert_default_bindings_unique SOURCE_FILE)
    file(READ "${SOURCE_FILE}" _source)
    string(FIND "${_source}" "default_bindings{" _start)
    if(_start EQUAL -1)
        message(FATAL_ERROR "settings default bindings duplicate stable key: default bindings are missing")
    endif()
    string(SUBSTRING "${_source}" ${_start} -1 _tail)
    string(FIND "${_tail}" "};" _end)
    if(_end EQUAL -1)
        message(FATAL_ERROR "settings default bindings duplicate stable key: default bindings are unterminated")
    endif()
    string(SUBSTRING "${_tail}" 0 ${_end} _bindings)
    string(REGEX MATCHALL "StableKey::[A-Za-z0-9_]+" _keys "${_bindings}")
    foreach(_key IN LISTS _keys)
        list(FIND _seen "${_key}" _duplicate)
        if(NOT _duplicate EQUAL -1)
            message(FATAL_ERROR "settings default bindings duplicate stable key: ${_key}")
        endif()
        list(APPEND _seen "${_key}")
    endforeach()
endfunction()

set(_settings_types "${SOURCE_ROOT}/platform/settings/settings_types.cpp")
set(_settings_store "${SOURCE_ROOT}/platform/settings/settings_store.cpp")
set(_host_source "${SOURCE_ROOT}/platform/raylib/raylib_host.cpp")
set(_stage_header "${SOURCE_ROOT}/platform/raylib/host_validation_stage11b.hpp")
set(_stage_source "${SOURCE_ROOT}/platform/raylib/host_validation_stage11b.cpp")
set(_hud_source "${SOURCE_ROOT}/platform/raylib/hud_renderer.cpp")
if(DEFINED STAGE11B_GUARD_MUTATION_KIND)
    if(STAGE11B_GUARD_MUTATION_KIND STREQUAL "raylib")
        set(_settings_types "${STAGE11B_GUARD_MUTATION_FILE}")
    elseif(STAGE11B_GUARD_MUTATION_KIND STREQUAL "save_name")
        set(_settings_store "${STAGE11B_GUARD_MUTATION_FILE}")
    elseif(STAGE11B_GUARD_MUTATION_KIND STREQUAL "key")
        set(_hud_source "${STAGE11B_GUARD_MUTATION_FILE}")
    elseif(STAGE11B_GUARD_MUTATION_KIND STREQUAL "duplicate")
        set(_settings_types "${STAGE11B_GUARD_MUTATION_FILE}")
    elseif(STAGE11B_GUARD_MUTATION_KIND STREQUAL "stage")
        set(_stage_source "${STAGE11B_GUARD_MUTATION_FILE}")
    endif()
endif()

foreach(_stage_required IN ITEMS "${_stage_header}" "${_stage_source}")
    if(NOT EXISTS "${_stage_required}")
        message(FATAL_ERROR "Stage 11B validation target is missing: ${_stage_required}")
    endif()
endforeach()
file(READ "${_stage_header}" _stage_header_text)
file(READ "${_stage_source}" _stage_source_text)
file(READ "${_host_source}" _host_source_text)
file(READ "${SOURCE_ROOT}/platform/raylib/CMakeLists.txt" _raylib_cmake_text)

foreach(_stage_definition IN ITEMS
        "PhysicalKeySnapshot inject_stage11b_physical_edges("
        "bool stage11b_validation_complete("
        "std::uint64_t stage11b_snapshot_hash("
        "void write_stage11b_validation_summary(")
    string(FIND "${_stage_source_text}" "${_stage_definition}" _stage_found)
    if(_stage_found EQUAL -1)
        message(FATAL_ERROR "Stage 11B validation definition is missing: ${_stage_definition}")
    endif()
    string(FIND "${_host_source_text}" "${_stage_definition}" _host_found)
    if(NOT _host_found EQUAL -1)
        message(FATAL_ERROR "Stage 11B validation definition remains in raylib_host.cpp: ${_stage_definition}")
    endif()
endforeach()
if(NOT _stage_header_text MATCHES "struct Stage11BValidationState final")
    message(FATAL_ERROR "Stage 11B validation state definition is missing")
endif()
if(_host_source_text MATCHES "struct Stage11BValidationState final")
    message(FATAL_ERROR "Stage 11B validation state definition remains in raylib_host.cpp")
endif()
foreach(_stage_algorithm_token IN ITEMS
        "StableKey::j"
        "StableKey::u"
        "state.pause_capture_while_paused"
        "player_monster_hash_before="
        "load_status=")
    string(FIND "${_stage_source_text}" "${_stage_algorithm_token}" _stage_token_found)
    if(_stage_token_found EQUAL -1)
        message(FATAL_ERROR "Stage 11B validation algorithm token is missing: ${_stage_algorithm_token}")
    endif()
endforeach()
string(FIND "${_raylib_cmake_text}" "host_validation_stage11b.cpp" _stage_registered)
if(_stage_registered EQUAL -1)
    message(FATAL_ERROR "arpg_raylib does not register host_validation_stage11b.cpp")
endif()

file(GLOB_RECURSE _settings_sources LIST_DIRECTORIES FALSE
    "${SOURCE_ROOT}/platform/settings/*.h" "${SOURCE_ROOT}/platform/settings/*.hpp"
    "${SOURCE_ROOT}/platform/settings/*.cpp")
list(REMOVE_ITEM _settings_sources "${SOURCE_ROOT}/platform/settings/settings_types.cpp"
             "${SOURCE_ROOT}/platform/settings/settings_store.cpp")
list(APPEND _settings_sources "${_settings_types}" "${_settings_store}")

arpg_assert_files_exclude_case_insensitive("settings must not include raylib"
    "#[ \t]*include[ \t]*[<\"](raylib|raymath|rlgl)([./\\]|[>\"])" ${_settings_sources})
arpg_assert_files_exclude_case_insensitive("settings must not include gameplay module"
    "#[ \t]*include[ \t]*[<\"](combat|dungeon|items|progression|passives)[/\\]" ${_settings_sources})

file(GLOB_RECURSE _core_sources LIST_DIRECTORIES FALSE
    "${SOURCE_ROOT}/core/*.h" "${SOURCE_ROOT}/core/*.hpp" "${SOURCE_ROOT}/core/*.cpp")
arpg_assert_files_exclude_case_insensitive("core must not include settings"
    "#[ \t]*include[ \t]*[<\"](platform[/\\])?settings[/\\]" ${_core_sources})

arpg_assert_files_exclude_case_insensitive("settings store must not use gameplay save filename"
    "[A-Za-z0-9_-]+[.]sav" "${_settings_store}")
arpg_assert_files_exclude("host HUD must not use direct gameplay KEY constants"
    "(^|[^A-Za-z0-9_])KEY_(A|B|C|D|E|F|G|H|I|J|K|L|M|N|O|P|Q|R|S|T|U|V|W|X|Y|Z|ZERO|ONE|TWO|THREE|FOUR|FIVE|SIX|SEVEN|EIGHT|NINE|UP|DOWN|LEFT|RIGHT|SPACE|LEFT_SHIFT|RIGHT_SHIFT|LEFT_CONTROL|RIGHT_CONTROL)([^A-Za-z0-9_]|$)" "${_host_source}" "${_hud_source}")
arpg_assert_default_bindings_unique("${_settings_types}")

if(NOT DEFINED STAGE11B_GUARD_MUTATION_MODE)
    if(NOT DEFINED GUARD_TEST_ROOT)
        message(FATAL_ERROR "GUARD_TEST_ROOT is required for mutation self-checks")
    endif()
    file(MAKE_DIRECTORY "${GUARD_TEST_ROOT}")

    function(arpg_expect_guard_rejects NAME KIND SOURCE MUTATION REASON)
        set(_fixture "${GUARD_TEST_ROOT}/${NAME}.cpp")
        file(COPY_FILE "${SOURCE}" "${_fixture}")
        if(KIND STREQUAL "duplicate")
            file(READ "${_fixture}" _mutated)
            string(REPLACE "StableKey::p};" "StableKey::w};" _mutated "${_mutated}")
            file(WRITE "${_fixture}" "${_mutated}")
        else()
            file(APPEND "${_fixture}" "\n${MUTATION}\n")
        endif()
        execute_process(
            COMMAND "${CMAKE_COMMAND}"
                "-DSOURCE_ROOT=${SOURCE_ROOT}"
                "-DSTAGE11B_GUARD_MUTATION_MODE=1"
                "-DSTAGE11B_GUARD_MUTATION_KIND=${KIND}"
                "-DSTAGE11B_GUARD_MUTATION_FILE=${_fixture}"
                -P "${CMAKE_CURRENT_LIST_FILE}"
            RESULT_VARIABLE _result OUTPUT_VARIABLE _stdout ERROR_VARIABLE _stderr)
        if(_result EQUAL 0)
            message(FATAL_ERROR "Stage 11b guard missed ${NAME} mutation")
        endif()
        set(_output "${_stdout}\n${_stderr}")
        if(NOT _output MATCHES "${REASON}")
            message(FATAL_ERROR "Stage 11b guard mutation ${NAME} lacked named reason '${REASON}'")
        endif()
    endfunction()

    arpg_expect_guard_rejects(raylib_include raylib
        "${SOURCE_ROOT}/platform/settings/settings_types.cpp"
        "#include <raylib.h>"
        "settings must not include raylib")
    arpg_expect_guard_rejects(gameplay_save_name save_name
        "${SOURCE_ROOT}/platform/settings/settings_store.cpp"
        "constexpr const char* kBad = \"slot-a.sav\";"
        "settings store must not use gameplay save filename")
    arpg_expect_guard_rejects(direct_gameplay_key key
        "${SOURCE_ROOT}/platform/raylib/hud_renderer.cpp"
        "int kBad = KEY_J;"
        "host HUD must not use direct gameplay KEY constants")
    arpg_expect_guard_rejects(duplicate_stable_key duplicate
        "${SOURCE_ROOT}/platform/settings/settings_types.cpp"
        ""
        "settings default bindings duplicate stable key")

    set(_stage_mutation "${GUARD_TEST_ROOT}/stage11b-rebound-key.cpp")
    file(COPY_FILE "${_stage_source}" "${_stage_mutation}")
    file(READ "${_stage_mutation}" _stage_mutation_text)
    string(REPLACE "settings::StableKey::j" "settings::StableKey::q"
        _stage_mutation_text "${_stage_mutation_text}")
    file(WRITE "${_stage_mutation}" "${_stage_mutation_text}")
    execute_process(
        COMMAND "${CMAKE_COMMAND}"
            "-DSOURCE_ROOT=${SOURCE_ROOT}"
            "-DSTAGE11B_GUARD_MUTATION_MODE=1"
            "-DSTAGE11B_GUARD_MUTATION_KIND=stage"
            "-DSTAGE11B_GUARD_MUTATION_FILE=${_stage_mutation}"
            -P "${CMAKE_CURRENT_LIST_FILE}"
        RESULT_VARIABLE _stage_result OUTPUT_VARIABLE _stage_stdout ERROR_VARIABLE _stage_stderr)
    if(_stage_result EQUAL 0)
        message(FATAL_ERROR "Stage 11b guard missed stage rebound-key mutation")
    endif()
    if(NOT "${_stage_stdout}${_stage_stderr}" MATCHES
            "Stage 11B validation algorithm token is missing: StableKey::j")
        message(FATAL_ERROR "Stage 11b stage rebound-key mutation lacked the expected reason")
    endif()
endif()

message(STATUS "Stage 11b settings architecture boundaries verified")
