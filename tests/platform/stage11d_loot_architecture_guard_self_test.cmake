foreach(_required_variable IN ITEMS
        SOURCE_ROOT GUARD_SCRIPT BAD_SOURCE GUARD_TEST_ROOT)
    if(NOT DEFINED ${_required_variable})
        message(FATAL_ERROR
            "Stage11D loot guard self-test requires ${_required_variable}")
    endif()
endforeach()
foreach(_required_file IN ITEMS "${GUARD_SCRIPT}" "${BAD_SOURCE}")
    if(NOT EXISTS "${_required_file}")
        message(FATAL_ERROR
            "Stage11D loot guard fixture is missing: ${_required_file}")
    endif()
endforeach()

file(READ "${BAD_SOURCE}" _bad_source_text)
foreach(_fixture_token IN ITEMS
        "std::string" "IsKeyDown" "session.tick" "SettingsStore"
        "DUNGEON_TO_SETTINGS_INCLUDE" "RARITY_IN_EXPLICIT_PICKUP"
        "MISSING_ABYSS_BYPASS" "RELATIVE_DUNGEON_INCLUDE"
        "RELATIVE_SETTINGS_INCLUDE" "RELATIVE_RAYLIB_INCLUDE")
    string(FIND "${_bad_source_text}" "${_fixture_token}" _fixture_index)
    if(_fixture_index EQUAL -1)
        message(FATAL_ERROR
            "Stage11D bad-source manifest lacks ${_fixture_token}")
    endif()
endforeach()

file(MAKE_DIRECTORY "${GUARD_TEST_ROOT}")

function(arpg_stage11d_make_fixture NAME OUT_ROOT)
    set(_fixture_root "${GUARD_TEST_ROOT}/${NAME}/src")
    file(REMOVE_RECURSE "${GUARD_TEST_ROOT}/${NAME}")
    file(MAKE_DIRECTORY "${_fixture_root}/platform")
    file(COPY "${SOURCE_ROOT}/dungeon" DESTINATION "${_fixture_root}")
    file(COPY "${SOURCE_ROOT}/platform/settings"
        DESTINATION "${_fixture_root}/platform")
    file(COPY "${SOURCE_ROOT}/platform/raylib"
        DESTINATION "${_fixture_root}/platform")
    set("${OUT_ROOT}" "${_fixture_root}" PARENT_SCOPE)
endfunction()

function(arpg_stage11d_run_guard FIXTURE_ROOT OUT_RESULT OUT_OUTPUT)
    execute_process(
        COMMAND "${CMAKE_COMMAND}"
            "-DSOURCE_ROOT=${FIXTURE_ROOT}"
            -P "${GUARD_SCRIPT}"
        RESULT_VARIABLE _result
        OUTPUT_VARIABLE _stdout
        ERROR_VARIABLE _stderr)
    set("${OUT_RESULT}" "${_result}" PARENT_SCOPE)
    set("${OUT_OUTPUT}" "${_stdout}\n${_stderr}" PARENT_SCOPE)
endfunction()

function(arpg_stage11d_append_mutation NAME RELATIVE_FILE MUTATION REASON)
    arpg_stage11d_make_fixture("${NAME}" _fixture_root)
    set(_target "${_fixture_root}/${RELATIVE_FILE}")
    if(NOT EXISTS "${_target}")
        message(FATAL_ERROR
            "Stage11D mutation ${NAME} target is missing: ${RELATIVE_FILE}")
    endif()
    file(APPEND "${_target}" "\n// Stage11D ${NAME} mutation\n${MUTATION}\n")
    arpg_stage11d_run_guard("${_fixture_root}" _result _output)
    if(_result EQUAL 0)
        message(FATAL_ERROR
            "Stage11D loot guard accepted ${NAME} mutation")
    endif()
    string(FIND "${_output}" "${REASON}" _reason_index)
    if(_reason_index EQUAL -1)
        message(FATAL_ERROR
            "Stage11D mutation ${NAME} failed for wrong reason; "
            "expected '${REASON}', got: ${_output}")
    endif()
endfunction()

function(arpg_stage11d_replace_mutation
        NAME RELATIVE_FILE ORIGINAL REPLACEMENT REASON EXPECT_REJECT)
    arpg_stage11d_make_fixture("${NAME}" _fixture_root)
    set(_target "${_fixture_root}/${RELATIVE_FILE}")
    if(NOT EXISTS "${_target}")
        message(FATAL_ERROR
            "Stage11D mutation ${NAME} target is missing: ${RELATIVE_FILE}")
    endif()
    file(READ "${_target}" _source)
    set(_original_source "${_source}")
    string(REPLACE "${ORIGINAL}" "${REPLACEMENT}" _source "${_source}")
    if(_source STREQUAL _original_source)
        message(FATAL_ERROR
            "Stage11D mutation ${NAME} substitution token is missing")
    endif()
    file(WRITE "${_target}" "${_source}")
    arpg_stage11d_run_guard("${_fixture_root}" _result _output)
    if(EXPECT_REJECT)
        if(_result EQUAL 0)
            message(FATAL_ERROR
                "Stage11D loot guard accepted ${NAME} mutation")
        endif()
        string(FIND "${_output}" "${REASON}" _reason_index)
        if(_reason_index EQUAL -1)
            message(FATAL_ERROR
                "Stage11D mutation ${NAME} failed for wrong reason; "
                "expected '${REASON}', got: ${_output}")
        endif()
    elseif(NOT _result EQUAL 0)
        message(FATAL_ERROR
            "Stage11D loot guard rejected equivalent ${NAME} variant: ${_output}")
    endif()
endfunction()

arpg_stage11d_append_mutation(dynamic_string
    "platform/raylib/ground_loot_view.cpp"
    "std::string stage11d_bad_dynamic_text;"
    "loot presentation rejects dynamic strings or containers")
arpg_stage11d_append_mutation(physical_input
    "platform/raylib/loot_pickup_feedback.cpp"
    "bool stage11d_bad_input() { return IsKeyDown(1); }"
    "loot presentation rejects physical input sampling")
arpg_stage11d_append_mutation(session_tick
    "platform/raylib/ground_loot_view.cpp"
    "void stage11d_bad_tick() { session.tick({}); }"
    "loot presentation rejects session calls")
arpg_stage11d_append_mutation(settings_store
    "platform/raylib/room_renderer.cpp"
    "SettingsStore* stage11d_bad_store = nullptr;"
    "room renderer rejects direct SettingsStore access")
arpg_stage11d_append_mutation(dungeon_to_settings_include
    "platform/settings/settings_types.cpp"
    "#include \"dungeon/dungeon_types.hpp\" // DUNGEON_TO_SETTINGS_INCLUDE"
    "settings must not include dungeon")
arpg_stage11d_append_mutation(relative_dungeon_include
    "platform/settings/settings_codec.cpp"
    "#include \"../../dungeon/dungeon_types.hpp\" // RELATIVE_DUNGEON_INCLUDE"
    "settings must not include dungeon")
arpg_stage11d_append_mutation(relative_settings_include
    "dungeon/dungeon_session.cpp"
    "#include \"../platform/settings/settings_types.hpp\" // RELATIVE_SETTINGS_INCLUDE"
    "dungeon must not include settings or raylib")
arpg_stage11d_append_mutation(relative_raylib_include
    "dungeon/dungeon_session.cpp"
    "#include \"../platform/raylib/ground_loot_view.hpp\" // RELATIVE_RAYLIB_INCLUDE"
    "dungeon must not include settings or raylib")
arpg_stage11d_replace_mutation(rarity_in_explicit_pickup
    "dungeon/dungeon_transition.cpp"
    "    const bool abyss_claim = ground.source == GroundItemSource::abyss_chest;"
    "    if (ground.item.rarity == items::ItemRarity::normal) return RequestResult::rejected; // RARITY_IN_EXPLICIT_PICKUP\n    const bool abyss_claim = ground.source == GroundItemSource::abyss_chest;"
    "explicit pickup must not apply loot rarity policy" TRUE)
arpg_stage11d_replace_mutation(missing_abyss_bypass
    "dungeon/dungeon_transition.cpp"
    "    if (ground.source == GroundItemSource::abyss_chest) return true;"
    "    // MISSING_ABYSS_BYPASS"
    "automatic pickup policy must preserve abyss bypass" TRUE)

arpg_stage11d_replace_mutation(equivalent_reversed_abyss_test
    "dungeon/dungeon_transition.cpp"
    "    if (ground.source == GroundItemSource::abyss_chest) return true;"
    "    if (GroundItemSource::abyss_chest == ground.source) {\n        return true;\n    }"
    "" FALSE)
arpg_stage11d_replace_mutation(equivalent_reversed_monster_test
    "dungeon/dungeon_transition.cpp"
    "    return ground.source == GroundItemSource::monster_drop\n        && static_cast<std::uint8_t>(ground.item.rarity)"
    "    return GroundItemSource::monster_drop == ground.source\n        && static_cast<std::uint8_t>(ground.item.rarity)"
    "" FALSE)

message(STATUS
    "Stage 11D loot guard rejected ten bad mutations and accepted two equivalent variants")
