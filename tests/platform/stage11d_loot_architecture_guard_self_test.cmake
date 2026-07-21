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
        "RELATIVE_SETTINGS_INCLUDE" "RELATIVE_RAYLIB_INCLUDE"
        "PMR_VECTOR" "FORWARD_LIST" "SESSION_PTR_TICK"
        "SESSION_SNAPSHOT" "SAVE_STORE" "STORE_LOAD"
        "MACRO_DUNGEON_INCLUDE" "MACRO_SETTINGS_INCLUDE"
        "RARITY_COMMENT_DECOY" "UNREACHABLE_ABYSS_BYPASS"
        "PRESENTATION_SETTINGS_STORE" "EXPLICIT_AUTO_POLICY_CALL"
        "SPLICED_VECTOR" "SPLICED_SESSION" "SPLICED_STORE"
        "FORWARD_DECL_BAD" "IF_FALSE_POLICY")
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
    file(MAKE_DIRECTORY "${_fixture_root}/dungeon")
    file(MAKE_DIRECTORY "${_fixture_root}/platform/settings")
    file(MAKE_DIRECTORY "${_fixture_root}/platform/raylib")
    foreach(_relative_source IN ITEMS
            "dungeon/dungeon_transition.cpp"
            "dungeon/dungeon_session.cpp"
            "platform/settings/settings_types.cpp"
            "platform/settings/settings_codec.cpp"
            "platform/raylib/ground_loot_view.hpp"
            "platform/raylib/ground_loot_view.cpp"
            "platform/raylib/loot_pickup_feedback.hpp"
            "platform/raylib/loot_pickup_feedback.cpp"
            "platform/raylib/room_renderer.cpp")
        get_filename_component(_relative_directory
            "${_relative_source}" DIRECTORY)
        file(MAKE_DIRECTORY "${_fixture_root}/${_relative_directory}")
        file(COPY_FILE "${SOURCE_ROOT}/${_relative_source}"
            "${_fixture_root}/${_relative_source}" ONLY_IF_DIFFERENT)
    endforeach()
    set("${OUT_ROOT}" "${_fixture_root}" PARENT_SCOPE)
endfunction()

function(arpg_stage11d_run_guard
        FIXTURE_ROOT RELATIVE_FILE OUT_RESULT OUT_OUTPUT)
    execute_process(
        COMMAND "${CMAKE_COMMAND}"
            "-DSOURCE_ROOT=${FIXTURE_ROOT}"
            "-DSTAGE11D_MUTATION_RELATIVE_FILE=${RELATIVE_FILE}"
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
    arpg_stage11d_run_guard(
        "${_fixture_root}" "${RELATIVE_FILE}" _result _output)
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
    arpg_stage11d_run_guard(
        "${_fixture_root}" "${RELATIVE_FILE}" _result _output)
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
arpg_stage11d_append_mutation(pmr_vector
    "platform/raylib/ground_loot_view.cpp"
    "std::pmr::vector<int> stage11d_bad_pmr; // PMR_VECTOR"
    "loot presentation rejects dynamic strings or containers")
arpg_stage11d_append_mutation(forward_list
    "platform/raylib/loot_pickup_feedback.cpp"
    "std::forward_list<int> stage11d_bad_forward_list; // FORWARD_LIST"
    "loot presentation rejects dynamic strings or containers")
arpg_stage11d_append_mutation(spliced_vector
    "platform/raylib/ground_loot_view.cpp"
    "std::vec\\\ntor<int> stage11d_bad_spliced_vector; // SPLICED_VECTOR"
    "loot presentation rejects dynamic strings or containers")
arpg_stage11d_append_mutation(physical_input
    "platform/raylib/loot_pickup_feedback.cpp"
    "bool stage11d_bad_input() { return IsKeyDown(1); }"
    "loot presentation rejects physical input sampling")
arpg_stage11d_append_mutation(session_tick
    "platform/raylib/ground_loot_view.cpp"
    "void stage11d_bad_tick() { session.tick({}); }"
    "loot presentation rejects session calls")
arpg_stage11d_append_mutation(session_ptr_tick
    "platform/raylib/loot_pickup_feedback.cpp"
    "void stage11d_bad_ptr_tick() { session->tick({}); } // SESSION_PTR_TICK"
    "loot presentation rejects session calls")
arpg_stage11d_append_mutation(session_snapshot
    "platform/raylib/ground_loot_view.cpp"
    "void stage11d_bad_snapshot() { session.snapshot(); } // SESSION_SNAPSHOT"
    "loot presentation rejects session calls")
arpg_stage11d_append_mutation(spliced_session
    "platform/raylib/loot_pickup_feedback.cpp"
    "void stage11d_bad_spliced_session() { ses\\\nsion->tick({}); } // SPLICED_SESSION"
    "loot presentation rejects session calls")
arpg_stage11d_append_mutation(save_store
    "platform/raylib/loot_pickup_feedback.cpp"
    "SaveStore* stage11d_bad_save_store = nullptr; // SAVE_STORE"
    "loot presentation rejects save/store access")
arpg_stage11d_append_mutation(presentation_settings_store
    "platform/raylib/ground_loot_view.cpp"
    "SettingsStore* stage11d_bad_settings_store = nullptr; // PRESENTATION_SETTINGS_STORE"
    "loot presentation rejects save/store access")
arpg_stage11d_append_mutation(store_load
    "platform/raylib/ground_loot_view.cpp"
    "void stage11d_bad_load() { store.load(); } // STORE_LOAD"
    "loot presentation rejects save/store access")
arpg_stage11d_append_mutation(spliced_store
    "platform/raylib/ground_loot_view.cpp"
    "void stage11d_bad_spliced_store() { sto\\\nre.load(); } // SPLICED_STORE"
    "loot presentation rejects save/store access")
arpg_stage11d_append_mutation(settings_store
    "platform/raylib/room_renderer.cpp"
    "SettingsStore* stage11d_bad_store = nullptr;"
    "room renderer rejects direct save/store access")
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
arpg_stage11d_append_mutation(macro_dungeon_include
    "platform/settings/settings_codec.cpp"
    "#include MACRO_DUNGEON_INCLUDE"
    "include operands must be literal")
arpg_stage11d_append_mutation(macro_settings_include
    "dungeon/dungeon_session.cpp"
    "#include MACRO_SETTINGS_INCLUDE"
    "include operands must be literal")
arpg_stage11d_replace_mutation(rarity_in_explicit_pickup
    "dungeon/dungeon_transition.cpp"
    "    const bool abyss_claim = ground.source == GroundItemSource::abyss_chest;"
    "    if (ground.item.rarity == items::ItemRarity::normal) return RequestResult::rejected; // RARITY_IN_EXPLICIT_PICKUP\n    const bool abyss_claim = ground.source == GroundItemSource::abyss_chest;"
    "explicit pickup must not apply loot rarity policy" TRUE)
arpg_stage11d_replace_mutation(explicit_auto_policy_call
    "dungeon/dungeon_transition.cpp"
    "    const bool abyss_claim = ground.source == GroundItemSource::abyss_chest;"
    "    static_cast<void>(auto_pickup_eligible(ground, {})); // EXPLICIT_AUTO_POLICY_CALL\n    const bool abyss_claim = ground.source == GroundItemSource::abyss_chest;"
    "explicit pickup must not apply loot rarity policy" TRUE)
arpg_stage11d_replace_mutation(missing_abyss_bypass
    "dungeon/dungeon_transition.cpp"
    "    if (ground.source == GroundItemSource::abyss_chest) return true;"
    "    // MISSING_ABYSS_BYPASS"
    "automatic pickup policy must preserve abyss bypass" TRUE)
arpg_stage11d_replace_mutation(rarity_comment_decoy
    "dungeon/dungeon_transition.cpp"
    "    return ground.source == GroundItemSource::monster_drop\n        && static_cast<std::uint8_t>(ground.item.rarity)\n            >= static_cast<std::uint8_t>(policy.minimum_rarity);"
    "    // RARITY_COMMENT_DECOY ground.item.rarity policy.minimum_rarity\n    return ground.source == GroundItemSource::monster_drop;"
    "automatic pickup policy is incomplete" TRUE)
arpg_stage11d_replace_mutation(unreachable_abyss_bypass
    "dungeon/dungeon_transition.cpp"
    "    if (ground.source == GroundItemSource::abyss_chest) return true;\n    return ground.source == GroundItemSource::monster_drop\n        && static_cast<std::uint8_t>(ground.item.rarity)\n            >= static_cast<std::uint8_t>(policy.minimum_rarity);"
    "    // UNREACHABLE_ABYSS_BYPASS\n    return ground.source == GroundItemSource::monster_drop\n        && static_cast<std::uint8_t>(ground.item.rarity)\n            >= static_cast<std::uint8_t>(policy.minimum_rarity);\n    if (ground.source == GroundItemSource::abyss_chest) return true;"
    "automatic pickup policy must use canonical top-level body" TRUE)
arpg_stage11d_replace_mutation(forward_declaration_bad_definition
    "dungeon/dungeon_transition.cpp"
    "bool auto_pickup_eligible(\n    const GroundItem& ground,\n    AutoPickupPolicy policy) noexcept {\n    if (!ground.active) return false;\n    if (ground.source == GroundItemSource::abyss_chest) return true;\n    return ground.source == GroundItemSource::monster_drop\n        && static_cast<std::uint8_t>(ground.item.rarity)\n            >= static_cast<std::uint8_t>(policy.minimum_rarity);\n}"
    "bool auto_pickup_eligible(const GroundItem& ground, AutoPickupPolicy policy) noexcept; // FORWARD_DECL_BAD\nbool stage11d_unrelated_forward_body() noexcept { return false; }\nbool auto_pickup_eligible(\n    const GroundItem& ground,\n    AutoPickupPolicy policy) noexcept {\n    if (!ground.active) return false;\n    if (ground.source == GroundItemSource::abyss_chest) return true;\n    return ground.source == GroundItemSource::monster_drop;\n}"
    "automatic pickup policy is incomplete" TRUE)
arpg_stage11d_replace_mutation(if_false_policy_wrapper
    "dungeon/dungeon_transition.cpp"
    "    if (ground.source == GroundItemSource::abyss_chest) return true;\n    return ground.source == GroundItemSource::monster_drop\n        && static_cast<std::uint8_t>(ground.item.rarity)\n            >= static_cast<std::uint8_t>(policy.minimum_rarity);"
    "    if (false) { // IF_FALSE_POLICY\n        if (ground.source == GroundItemSource::abyss_chest) return true;\n        return ground.source == GroundItemSource::monster_drop\n            && static_cast<std::uint8_t>(ground.item.rarity)\n                >= static_cast<std::uint8_t>(policy.minimum_rarity);\n    }\n    return false;"
    "automatic pickup policy must use canonical top-level body" TRUE)

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
arpg_stage11d_replace_mutation(equivalent_parameter_rename_and_format
    "dungeon/dungeon_transition.cpp"
    "bool auto_pickup_eligible(\n    const GroundItem& ground,\n    AutoPickupPolicy policy) noexcept {\n    if (!ground.active) return false;\n    if (ground.source == GroundItemSource::abyss_chest) return true;\n    return ground.source == GroundItemSource::monster_drop\n        && static_cast<std::uint8_t>(ground.item.rarity)\n            >= static_cast<std::uint8_t>(policy.minimum_rarity);\n}"
    "bool\nauto_pickup_eligible(\n    const GroundItem& drop,\n    AutoPickupPolicy rules) noexcept {\n    // harmless std::pmr::vector and session->tick comment\n    if (!drop.active) return false;\n    if (GroundItemSource::abyss_chest == drop.source) {\n        return true;\n    }\n    return GroundItemSource::monster_drop == drop.source\n        && static_cast<std::uint8_t>(drop.item.rarity)\n            >= static_cast<std::uint8_t>(rules.minimum_rarity);\n}"
    "" FALSE)
arpg_stage11d_replace_mutation(equivalent_comment_and_string
    "platform/raylib/ground_loot_view.cpp"
    "#include <cstdio>"
    "#include <cstdio>\n// std::forward_list session.snapshot() SaveStore store.load()\nconstexpr const char* kStage11DGuardText = \"std::pmr::vector session->tick SettingsStore\";"
    "" FALSE)
arpg_stage11d_replace_mutation(equivalent_commented_include
    "platform/settings/settings_codec.cpp"
    "#include \"platform/settings/settings_codec.hpp\""
    "#include \"platform/settings/settings_codec.hpp\"\n// #include MACRO_DUNGEON_INCLUDE\nconstexpr const char* kStage11DIncludeText = \"#include ../../dungeon/decoy.hpp\";"
    "" FALSE)
arpg_stage11d_replace_mutation(equivalent_forward_declaration
    "dungeon/dungeon_transition.cpp"
    "bool auto_pickup_eligible(\n    const GroundItem& ground,"
    "bool auto_pickup_eligible(const GroundItem& ground, AutoPickupPolicy policy) noexcept;\nbool stage11d_unrelated_good_body() noexcept { return false; }\nbool auto_pickup_eligible(\n    const GroundItem& ground,"
    "" FALSE)
arpg_stage11d_replace_mutation(equivalent_unrelated_member_calls
    "platform/raylib/ground_loot_view.cpp"
    "#include <cstdio>"
    "#include <cstdio>\nvoid stage11d_good_members() { animation.tick(); frame.snapshot(); cache.load(); }"
    "" FALSE)
arpg_stage11d_replace_mutation(equivalent_raw_string_and_comment
    "platform/raylib/ground_loot_view.cpp"
    "#include <cstdio>"
    "#include <cstdio>\nconstexpr const char* kStage11DRaw = R\"guard(std::vector session->tick store.load())guard\";\n/* std::pmr::vector and ses\\\nsion.snapshot() are harmless comments */"
    "" FALSE)

message(STATUS
    "Stage 11D loot guard rejected twenty-seven bad mutations and accepted eight equivalent variants")
