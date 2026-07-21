if(TARGET arpg_raylib)
    add_executable(arpg_platform_tests
        ../core/allocation_probe.cpp
        platform_test_main.cpp
        host_input_tests.cpp
        active_skill_input_tests.cpp
        audio_pack_tests.cpp
        audio_routing_tests.cpp
        audio_scene_tests.cpp
        stage15_audio_pack_tests.cpp
        combat_feedback_tests.cpp
        combat_view_math_tests.cpp
        monster_view_tests.cpp
        dungeon_view_math_tests.cpp
        passive_tree_view_tests.cpp
        host_launch_options_tests.cpp
        dungeon_runtime_tests.cpp
        pause_host_gate_tests.cpp
        pause_menu_state_tests.cpp
        pause_menu_view_tests.cpp
        window_settings_tests.cpp
        control_hints_tests.cpp
        hud_view_model_tests.cpp
        hud_notice_state_tests.cpp
        loot_pickup_feedback_tests.cpp
        hud_layout_tests.cpp
        hud_font_tests.cpp
        hud_render_plan_tests.cpp
        hud_host_integration_tests.cpp
        death_input_gate_tests.cpp
        death_overlay_view_tests.cpp
        ground_loot_view_tests.cpp
        material_loot_view_tests.cpp
        material_bag_renderer_tests.cpp
        audio_asset_validation_tests.cpp
        material_asset_validation_tests.cpp
        material_animation_tests.cpp
        stage12_environment_render_tests.cpp
        stage12_actor_render_tests.cpp
        stage12_material_render_tests.cpp)
    target_sources(arpg_platform_tests PRIVATE inventory_view_math_tests.cpp)
    target_compile_features(arpg_platform_tests PRIVATE cxx_std_17)
    target_include_directories(arpg_platform_tests
        PRIVATE "${PROJECT_SOURCE_DIR}/tests/core")
    target_compile_definitions(arpg_platform_tests
        PRIVATE ARPG_PROJECT_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    target_link_libraries(arpg_platform_tests PRIVATE arpg_raylib)
    if(MSVC)
        target_compile_options(arpg_platform_tests PRIVATE /utf-8)
    endif()
    arpg_enable_project_warnings(arpg_platform_tests)

    add_test(NAME platform.units COMMAND arpg_platform_tests)
    set_tests_properties(
        platform.units PROPERTIES LABELS "headless;platform")

    add_executable(arpg_stage16_loot_reinforcement_game_validation
        stage16_loot_reinforcement_game_validation.cpp)
    target_compile_features(arpg_stage16_loot_reinforcement_game_validation
        PRIVATE cxx_std_17)
    target_include_directories(arpg_stage16_loot_reinforcement_game_validation
        PRIVATE "${PROJECT_SOURCE_DIR}/tests/dungeon")
    target_link_libraries(arpg_stage16_loot_reinforcement_game_validation
        PRIVATE arpg_raylib)
    if(MSVC)
        target_compile_options(arpg_stage16_loot_reinforcement_game_validation PRIVATE /utf-8)
    endif()
    arpg_enable_project_warnings(arpg_stage16_loot_reinforcement_game_validation)
    add_test(NAME stage16.loot_reinforcement.real_raylib
        COMMAND "$<TARGET_FILE:arpg_stage16_loot_reinforcement_game_validation>"
            "${CMAKE_CURRENT_BINARY_DIR}/stage16 loot reinforcement evidence")
    set_tests_properties(stage16.loot_reinforcement.real_raylib PROPERTIES
        LABELS "platform;raylib;stage16;validation" TIMEOUT 90 RUN_SERIAL TRUE)

    add_executable(arpg_stage12_material_formal
        stage12_material_formal_game_validation.cpp)
    target_compile_features(arpg_stage12_material_formal PRIVATE cxx_std_17)
    target_compile_definitions(arpg_stage12_material_formal
        PRIVATE ARPG_PROJECT_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    target_link_libraries(arpg_stage12_material_formal PRIVATE arpg_raylib)
    if(MSVC)
        target_compile_options(arpg_stage12_material_formal PRIVATE /utf-8)
    endif()
    arpg_enable_project_warnings(arpg_stage12_material_formal)
    add_test(
        NAME stage12.material_formal
        COMMAND "$<TARGET_FILE:arpg_stage12_material_formal>"
            "${CMAKE_CURRENT_BINARY_DIR}/stage12 material evidence")
    set_tests_properties(stage12.material_formal
        PROPERTIES LABELS "platform;raylib;stage12" TIMEOUT 300
            FIXTURES_SETUP stage12_material_evidence RUN_SERIAL TRUE)
    add_test(
        NAME stage12.material_evidence_validator
        COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass -File
            "${CMAKE_CURRENT_SOURCE_DIR}/stage12_material_validator.ps1"
            -EvidenceDirectory "${CMAKE_CURRENT_BINARY_DIR}/stage12 material evidence/stage12-run")
    set_tests_properties(stage12.material_evidence_validator
        PROPERTIES LABELS "platform;raylib;stage12" TIMEOUT 60
            FIXTURES_REQUIRED stage12_material_evidence)
    add_test(
        NAME stage12.material_root_safety
        COMMAND "$<TARGET_FILE:arpg_stage12_material_formal>"
            --root-safety-self-test
            "${CMAKE_CURRENT_BINARY_DIR}/stage12-root-safety")
    set_tests_properties(stage12.material_root_safety
        PROPERTIES LABELS "headless;platform;stage12" TIMEOUT 30)

    add_executable(arpg_stage14_audio_formal
        stage14_audio_formal_game_validation.cpp)
    target_compile_features(arpg_stage14_audio_formal PRIVATE cxx_std_17)
    target_compile_definitions(arpg_stage14_audio_formal
        PRIVATE ARPG_PROJECT_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    target_link_libraries(arpg_stage14_audio_formal PRIVATE arpg_raylib)
    if(MSVC)
        target_compile_options(arpg_stage14_audio_formal PRIVATE /utf-8)
    endif()
    arpg_enable_project_warnings(arpg_stage14_audio_formal)

    add_test(
        NAME stage14.audio_formal
        COMMAND "$<TARGET_FILE:arpg_stage14_audio_formal>"
            "${CMAKE_CURRENT_BINARY_DIR}/stage14 audio evidence"
            "${PROJECT_SOURCE_DIR}/docs/validation/evidence/stage14-audio-material-pack")
    set_tests_properties(stage14.audio_formal
        PROPERTIES LABELS "platform;raylib;stage14" TIMEOUT 300
            FIXTURES_SETUP stage14_audio_evidence RUN_SERIAL TRUE)
    add_test(
        NAME stage14.audio_evidence_validator
        COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass -File
            "${CMAKE_CURRENT_SOURCE_DIR}/stage14_audio_validator.ps1"
            -EvidenceDirectory
                "${CMAKE_CURRENT_BINARY_DIR}/stage14 audio evidence/stage14-run")
    set_tests_properties(stage14.audio_evidence_validator
        PROPERTIES LABELS "platform;raylib;stage14" TIMEOUT 60
            FIXTURES_REQUIRED stage14_audio_evidence)
    add_test(
        NAME stage14.audio_root_safety
        COMMAND "$<TARGET_FILE:arpg_stage14_audio_formal>"
            --root-safety-self-test
            "${CMAKE_CURRENT_BINARY_DIR}/stage14-root-safety")
    set_tests_properties(stage14.audio_root_safety
        PROPERTIES LABELS "headless;platform;stage14" TIMEOUT 30)

    add_executable(arpg_stage15_audio_formal
        stage15_audio_formal_game_validation.cpp)
    target_compile_features(arpg_stage15_audio_formal PRIVATE cxx_std_17)
    target_compile_definitions(arpg_stage15_audio_formal
        PRIVATE ARPG_PROJECT_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    target_link_libraries(arpg_stage15_audio_formal PRIVATE arpg_raylib)
    if(MSVC)
        target_compile_options(arpg_stage15_audio_formal PRIVATE /utf-8)
    endif()
    arpg_enable_project_warnings(arpg_stage15_audio_formal)

    add_test(
        NAME stage15.audio_formal
        COMMAND "$<TARGET_FILE:arpg_stage15_audio_formal>"
            "${CMAKE_CURRENT_BINARY_DIR}/stage15 audio evidence"
            "${PROJECT_SOURCE_DIR}/docs/validation/evidence/stage15-audio-mix")
    set_tests_properties(stage15.audio_formal
        PROPERTIES LABELS "platform;raylib;stage15" TIMEOUT 300
            FIXTURES_SETUP stage15_audio_evidence RUN_SERIAL TRUE)
    add_test(
        NAME stage15.audio_evidence_validator
        COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass -File
            "${CMAKE_CURRENT_SOURCE_DIR}/stage15_audio_validator.ps1"
            -EvidenceDirectory
                "${CMAKE_CURRENT_BINARY_DIR}/stage15 audio evidence/stage15-run")
    set_tests_properties(stage15.audio_evidence_validator
        PROPERTIES LABELS "platform;raylib;stage15" TIMEOUT 60
            FIXTURES_REQUIRED stage15_audio_evidence)
    add_test(
        NAME stage15.audio_evidence_validator_self_test
        COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass -File
            "${CMAKE_CURRENT_SOURCE_DIR}/stage15_audio_validator_self_test.ps1"
            -EvidenceDirectory
                "${CMAKE_CURRENT_BINARY_DIR}/stage15 audio evidence/stage15-run"
            -Validator "${CMAKE_CURRENT_SOURCE_DIR}/stage15_audio_validator.ps1"
            -MutationRoot "${CMAKE_CURRENT_BINARY_DIR}/stage15-audio-evidence-mutation")
    set_tests_properties(stage15.audio_evidence_validator_self_test
        PROPERTIES LABELS "platform;raylib;negative;stage15" TIMEOUT 60
            FIXTURES_REQUIRED stage15_audio_evidence)
    add_test(
        NAME stage15.audio_root_safety
        COMMAND "$<TARGET_FILE:arpg_stage15_audio_formal>"
            --root-safety-self-test
            "${CMAKE_CURRENT_BINARY_DIR}/stage15-root-safety")
    set_tests_properties(stage15.audio_root_safety
        PROPERTIES LABELS "headless;platform;stage15" TIMEOUT 30)

    add_executable(arpg_stage11c_hud_stress
        ../core/allocation_probe.cpp
        hud_stress_tests.cpp)
    target_compile_features(arpg_stage11c_hud_stress PRIVATE cxx_std_17)
    target_include_directories(arpg_stage11c_hud_stress
        PRIVATE "${PROJECT_SOURCE_DIR}/tests/core")
    target_link_libraries(arpg_stage11c_hud_stress PRIVATE arpg_raylib)
    if(MSVC)
        target_compile_options(arpg_stage11c_hud_stress PRIVATE /utf-8)
    endif()
    arpg_enable_project_warnings(arpg_stage11c_hud_stress)

    add_test(
        NAME stage11c.hud_stress.zero_alloc_100k
        COMMAND arpg_stage11c_hud_stress)
    set_tests_properties(
        stage11c.hud_stress.zero_alloc_100k
        PROPERTIES
            ENVIRONMENT "ARPG_STAGE11C_HUD_STRESS=1"
            LABELS "headless;platform;stress;stage11c"
            TIMEOUT 300)

    add_executable(arpg_stage11b_settings_formal
        stage11b_settings_formal_game_validation.cpp)
    target_compile_features(arpg_stage11b_settings_formal PRIVATE cxx_std_17)
    target_link_libraries(arpg_stage11b_settings_formal PRIVATE arpg_raylib)
    arpg_enable_project_warnings(arpg_stage11b_settings_formal)

    add_test(
        NAME stage11b.settings_formal
        COMMAND "$<TARGET_FILE:arpg_stage11b_settings_formal>"
            "${CMAKE_CURRENT_BINARY_DIR}/stage11b settings evidence")
    set_tests_properties(
        stage11b.settings_formal
        PROPERTIES LABELS "platform;raylib;stage11b"
            TIMEOUT 300 FIXTURES_SETUP stage11b_settings_evidence)

    add_test(
        NAME stage11b.settings_evidence_validator
        COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass -File
            "${CMAKE_CURRENT_SOURCE_DIR}/stage11b_settings_formal_validator.ps1"
            -EvidenceDirectory "${CMAKE_CURRENT_BINARY_DIR}/stage11b settings evidence")
    set_tests_properties(
        stage11b.settings_evidence_validator
        PROPERTIES LABELS "platform;raylib;stage11b"
            TIMEOUT 60 FIXTURES_REQUIRED stage11b_settings_evidence)

    add_executable(arpg_stage11c_hud_formal
        stage11c_hud_formal_game_validation.cpp)
    target_compile_features(arpg_stage11c_hud_formal PRIVATE cxx_std_17)
    target_link_libraries(arpg_stage11c_hud_formal PRIVATE arpg_raylib)
    if(MSVC)
        target_compile_options(arpg_stage11c_hud_formal PRIVATE /utf-8)
    endif()
    arpg_enable_project_warnings(arpg_stage11c_hud_formal)

    add_test(
        NAME stage11c.hud_formal
        COMMAND "$<TARGET_FILE:arpg_stage11c_hud_formal>"
            "${CMAKE_CURRENT_BINARY_DIR}/stage11c hud evidence")
    set_tests_properties(
        stage11c.hud_formal
        PROPERTIES LABELS "platform;raylib;stage11c"
            TIMEOUT 600 FIXTURES_SETUP stage11c_hud_evidence)

    add_test(
        NAME stage11c.hud_evidence_validator
        COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass -File
            "${CMAKE_CURRENT_SOURCE_DIR}/stage11c_hud_formal_validator.ps1"
            -EvidenceDirectory "${CMAKE_CURRENT_BINARY_DIR}/stage11c hud evidence")
    set_tests_properties(
        stage11c.hud_evidence_validator
        PROPERTIES LABELS "platform;raylib;stage11c"
            TIMEOUT 60 FIXTURES_REQUIRED stage11c_hud_evidence)

    add_test(
        NAME stage11c.hud_evidence_validator_self_test
        COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass -File
            "${CMAKE_CURRENT_SOURCE_DIR}/stage11c_hud_formal_validator_self_test.ps1"
            -EvidenceDirectory "${CMAKE_CURRENT_BINARY_DIR}/stage11c hud evidence"
            -Validator "${CMAKE_CURRENT_SOURCE_DIR}/stage11c_hud_formal_validator.ps1"
            -MutationRoot "${CMAKE_CURRENT_BINARY_DIR}/stage11c-hud-validator-mutations")
    set_tests_properties(
        stage11c.hud_evidence_validator_self_test
        PROPERTIES LABELS "platform;raylib;stage11c"
            TIMEOUT 120 FIXTURES_REQUIRED stage11c_hud_evidence)

    set(_stage11d_loot_formal_source
        "${CMAKE_CURRENT_SOURCE_DIR}/stage11d_loot_formal_game_validation.cpp")
    if(EXISTS "${_stage11d_loot_formal_source}")
        add_executable(arpg_stage11d_loot_formal
            stage11d_loot_formal_game_validation.cpp)
        target_compile_features(arpg_stage11d_loot_formal PRIVATE cxx_std_17)
        target_link_libraries(arpg_stage11d_loot_formal PRIVATE arpg_raylib)
        if(MSVC)
            target_compile_options(arpg_stage11d_loot_formal PRIVATE /utf-8)
        endif()
        arpg_enable_project_warnings(arpg_stage11d_loot_formal)
        set(_stage11d_loot_formal_command
            "$<TARGET_FILE:arpg_stage11d_loot_formal>")
    else()
        set(_stage11d_loot_formal_command
            "${CMAKE_CURRENT_BINARY_DIR}/missing-arpg-stage11d-loot-formal")
    endif()

    add_test(
        NAME stage11d.loot_formal
        COMMAND "${_stage11d_loot_formal_command}"
            "${CMAKE_CURRENT_BINARY_DIR}/stage11d loot evidence"
            "${PROJECT_SOURCE_DIR}/docs/validation/evidence/stage11d")
    set_tests_properties(
        stage11d.loot_formal
        PROPERTIES LABELS "platform;raylib;stage11d"
            TIMEOUT 600 FIXTURES_SETUP stage11d_loot_evidence
            RUN_SERIAL TRUE)

    add_test(
        NAME stage11d.loot_root_safety
        COMMAND "${_stage11d_loot_formal_command}"
            --root-safety-self-test
            "${CMAKE_CURRENT_BINARY_DIR}/stage11d-root-safety/stage11d loot evidence"
            "${PROJECT_SOURCE_DIR}")
    set_tests_properties(
        stage11d.loot_root_safety
        PROPERTIES LABELS "headless;platform;stage11d" TIMEOUT 30)

    add_test(
        NAME stage11d.loot_evidence_validator
        COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass -File
            "${CMAKE_CURRENT_SOURCE_DIR}/stage11d_loot_formal_validator.ps1"
            -EvidenceDirectory "${CMAKE_CURRENT_BINARY_DIR}/stage11d loot evidence"
            -CommittedEvidenceDirectory
                "${PROJECT_SOURCE_DIR}/docs/validation/evidence/stage11d")
    set_tests_properties(
        stage11d.loot_evidence_validator
        PROPERTIES LABELS "platform;raylib;stage11d"
            TIMEOUT 60 FIXTURES_REQUIRED stage11d_loot_evidence)

    add_test(
        NAME platform.pause_menu_state_boundary
        COMMAND "${CMAKE_COMMAND}"
            "-DPAUSE_STATE_HEADER=${PROJECT_SOURCE_DIR}/src/platform/raylib/pause_menu_state.hpp"
            "-DPAUSE_STATE_SOURCE=${PROJECT_SOURCE_DIR}/src/platform/raylib/pause_menu_state.cpp"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/pause_menu_state_boundary_test.cmake")
    set_tests_properties(
        platform.pause_menu_state_boundary
        PROPERTIES LABELS "headless;platform;architecture;stage11b")
endif()

function(arpg_tokenize_dependency_value INPUT_VALUE OUT_TOKENS)
    set(_arpg_token_text "${INPUT_VALUE}")
    foreach(_arpg_delimiter IN ITEMS
            "$" "<" ">" "{" "}" "(" ")" "," ":" "/" "\\" "\"" "'")
        string(REPLACE
            "${_arpg_delimiter}" ";" _arpg_token_text "${_arpg_token_text}")
    endforeach()

    set(_arpg_tokens "")
    foreach(_arpg_token IN LISTS _arpg_token_text)
        string(STRIP "${_arpg_token}" _arpg_token)
        if(NOT _arpg_token STREQUAL "")
            list(APPEND _arpg_tokens "${_arpg_token}")
        endif()
    endforeach()
    set("${OUT_TOKENS}" "${_arpg_tokens}" PARENT_SCOPE)
endfunction()

function(arpg_value_has_forbidden_dependency
        INPUT_VALUE OUT_FOUND OUT_MATCH)
    string(TOLOWER "${INPUT_VALUE}" _arpg_value_lower)
    arpg_tokenize_dependency_value("${_arpg_value_lower}" _arpg_tokens)
    foreach(_arpg_token IN LISTS _arpg_tokens)
        if(_arpg_token MATCHES
                    "^(raylib|rlgl|raymath|raylib-cpp)([-_]?static)?d?$"
                OR _arpg_token MATCHES
                    "^(lib)?(raylib|rlgl|raymath|raylib-cpp)([-_]?static)?d?\\.(lib|a|obj|o)$"
                OR _arpg_token MATCHES
                    "^-l(raylib|rlgl|raymath|raylib-cpp)([-_]?static)?d?$")
            set("${OUT_FOUND}" TRUE PARENT_SCOPE)
            set("${OUT_MATCH}" "${_arpg_token}" PARENT_SCOPE)
            return()
        endif()
    endforeach()

    set("${OUT_FOUND}" FALSE PARENT_SCOPE)
    set("${OUT_MATCH}" "" PARENT_SCOPE)
endfunction()

function(arpg_include_value_has_forbidden_dependency
        INPUT_VALUE OUT_FOUND OUT_MATCH)
    string(TOLOWER "${INPUT_VALUE}" _arpg_value_lower)
    if(_arpg_value_lower MATCHES
            [=[(^|[;/\\])_deps[/\\]+raylib($|[-_/\\;>,])]=]
            OR _arpg_value_lower MATCHES
            [=[(^|[;/\\])(raylib|raylib-cpp)[/\\]+(include|src)($|[/\\;>,])]=])
        set("${OUT_FOUND}" TRUE PARENT_SCOPE)
        set("${OUT_MATCH}" "raylib include path" PARENT_SCOPE)
        return()
    endif()

    set("${OUT_FOUND}" FALSE PARENT_SCOPE)
    set("${OUT_MATCH}" "" PARENT_SCOPE)
endfunction()

function(arpg_definition_value_has_forbidden_dependency
        INPUT_VALUE OUT_FOUND OUT_MATCH)
    set(_arpg_definition_value "${INPUT_VALUE}")
    foreach(_arpg_delimiter IN ITEMS "=" "\"" "'")
        string(REPLACE
            "${_arpg_delimiter}"
            ";"
            _arpg_definition_value
            "${_arpg_definition_value}")
    endforeach()

    arpg_value_has_forbidden_dependency(
        "${_arpg_definition_value}" _arpg_found _arpg_match)
    if(_arpg_found)
        set("${OUT_FOUND}" TRUE PARENT_SCOPE)
        set("${OUT_MATCH}" "${_arpg_match}" PARENT_SCOPE)
        return()
    endif()

    string(TOLOWER "${_arpg_definition_value}" _arpg_definition_lower)
    arpg_tokenize_dependency_value(
        "${_arpg_definition_lower}" _arpg_definition_tokens)
    foreach(_arpg_token IN LISTS _arpg_definition_tokens)
        if(_arpg_token MATCHES
                "^(raylib\\.h|raymath\\.h|rlgl\\.h|raylib-cpp([._-][a-z0-9.+-]+)?)$")
            set("${OUT_FOUND}" TRUE PARENT_SCOPE)
            set("${OUT_MATCH}" "${_arpg_token}" PARENT_SCOPE)
            return()
        endif()
    endforeach()

    set("${OUT_FOUND}" FALSE PARENT_SCOPE)
    set("${OUT_MATCH}" "" PARENT_SCOPE)
endfunction()

function(arpg_extract_target_candidates INPUT_VALUE OUT_TARGETS)
    set(_arpg_targets "")
    if(TARGET "${INPUT_VALUE}")
        list(APPEND _arpg_targets "${INPUT_VALUE}")
    endif()

    string(REGEX MATCHALL
        "[A-Za-z0-9_.+-]+::[A-Za-z0-9_.+-]+"
        _arpg_namespaced_candidates
        "${INPUT_VALUE}")
    foreach(_arpg_candidate IN LISTS _arpg_namespaced_candidates)
        if(TARGET "${_arpg_candidate}")
            list(APPEND _arpg_targets "${_arpg_candidate}")
        endif()
    endforeach()

    arpg_tokenize_dependency_value("${INPUT_VALUE}" _arpg_tokens)
    foreach(_arpg_candidate IN LISTS _arpg_tokens)
        if(TARGET "${_arpg_candidate}")
            list(APPEND _arpg_targets "${_arpg_candidate}")
        endif()
    endforeach()

    list(REMOVE_DUPLICATES _arpg_targets)
    set("${OUT_TARGETS}" "${_arpg_targets}" PARENT_SCOPE)
endfunction()

function(arpg_walk_target_dependency_closure
        TARGET_NAME TARGET_PATH OUT_FOUND OUT_REASON)
    if(NOT TARGET "${TARGET_NAME}")
        set("${OUT_FOUND}" FALSE PARENT_SCOPE)
        set("${OUT_REASON}" "" PARENT_SCOPE)
        return()
    endif()

    arpg_value_has_forbidden_dependency(
        "${TARGET_NAME}" _arpg_found _arpg_match)
    if(_arpg_found)
        set("${OUT_FOUND}" TRUE PARENT_SCOPE)
        set("${OUT_REASON}"
            "${TARGET_PATH} target name contains '${_arpg_match}'"
            PARENT_SCOPE)
        return()
    endif()

    get_target_property(_arpg_aliased_target "${TARGET_NAME}" ALIASED_TARGET)
    if(_arpg_aliased_target)
        set(_arpg_inspected_target "${_arpg_aliased_target}")
    else()
        set(_arpg_inspected_target "${TARGET_NAME}")
    endif()

    arpg_value_has_forbidden_dependency(
        "${_arpg_inspected_target}" _arpg_found _arpg_match)
    if(_arpg_found)
        set("${OUT_FOUND}" TRUE PARENT_SCOPE)
        set("${OUT_REASON}"
            "${TARGET_PATH} canonical target name contains '${_arpg_match}'"
            PARENT_SCOPE)
        return()
    endif()

    get_property(_arpg_visited GLOBAL PROPERTY ARPG_BOUNDARY_VISITED_TARGETS)
    list(FIND _arpg_visited "${_arpg_inspected_target}" _arpg_visited_index)
    if(NOT _arpg_visited_index EQUAL -1)
        set("${OUT_FOUND}" FALSE PARENT_SCOPE)
        set("${OUT_REASON}" "" PARENT_SCOPE)
        return()
    endif()
    set_property(
        GLOBAL APPEND PROPERTY
        ARPG_BOUNDARY_VISITED_TARGETS "${_arpg_inspected_target}")

    get_target_property(
        _arpg_is_imported "${_arpg_inspected_target}" IMPORTED)
    set(_arpg_imported_configurations "")
    set(_arpg_link_property_names
        LINK_LIBRARIES
        INTERFACE_LINK_LIBRARIES)
    if(_arpg_is_imported)
        get_target_property(
            _arpg_imported_configurations
            "${_arpg_inspected_target}"
            IMPORTED_CONFIGURATIONS)
        if("${_arpg_imported_configurations}" MATCHES "-NOTFOUND$")
            set(_arpg_imported_configurations "")
        endif()
        list(APPEND _arpg_imported_configurations
            NOCONFIG DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
        list(REMOVE_DUPLICATES _arpg_imported_configurations)

        foreach(_arpg_property_base IN ITEMS
                IMPORTED_LINK_INTERFACE_LIBRARIES
                IMPORTED_LINK_DEPENDENT_LIBRARIES
                IMPORTED_OBJECTS)
            list(APPEND _arpg_link_property_names "${_arpg_property_base}")
            foreach(_arpg_configuration IN LISTS _arpg_imported_configurations)
                string(TOUPPER
                    "${_arpg_configuration}" _arpg_configuration_upper)
                list(APPEND
                    _arpg_link_property_names
                    "${_arpg_property_base}_${_arpg_configuration_upper}")
            endforeach()
        endforeach()
    endif()

    foreach(_arpg_property_name IN LISTS _arpg_link_property_names)
        get_target_property(
            _arpg_property_value
            "${_arpg_inspected_target}"
            "${_arpg_property_name}")
        if("${_arpg_property_value}" STREQUAL ""
                OR "${_arpg_property_value}" MATCHES "-NOTFOUND$")
            continue()
        endif()

        arpg_value_has_forbidden_dependency(
            "${_arpg_property_value}" _arpg_found _arpg_match)
        if(_arpg_found)
            set("${OUT_FOUND}" TRUE PARENT_SCOPE)
            set("${OUT_REASON}"
                "${TARGET_PATH} ${_arpg_property_name} contains '${_arpg_match}' in '${_arpg_property_value}'"
                PARENT_SCOPE)
            return()
        endif()

        arpg_extract_target_candidates(
            "${_arpg_property_value}" _arpg_dependencies)
        foreach(_arpg_dependency IN LISTS _arpg_dependencies)
            arpg_walk_target_dependency_closure(
                "${_arpg_dependency}"
                "${TARGET_PATH} -> ${_arpg_dependency}"
                _arpg_nested_found
                _arpg_nested_reason)
            if(_arpg_nested_found)
                set("${OUT_FOUND}" TRUE PARENT_SCOPE)
                set("${OUT_REASON}" "${_arpg_nested_reason}" PARENT_SCOPE)
                return()
            endif()
        endforeach()
    endforeach()

    foreach(_arpg_property_name IN ITEMS
            LINK_OPTIONS
            INTERFACE_LINK_OPTIONS)
        get_target_property(
            _arpg_property_value
            "${_arpg_inspected_target}"
            "${_arpg_property_name}")
        if("${_arpg_property_value}" STREQUAL ""
                OR "${_arpg_property_value}" MATCHES "-NOTFOUND$")
            continue()
        endif()

        arpg_value_has_forbidden_dependency(
            "${_arpg_property_value}" _arpg_found _arpg_match)
        if(_arpg_found)
            set("${OUT_FOUND}" TRUE PARENT_SCOPE)
            set("${OUT_REASON}"
                "${TARGET_PATH} ${_arpg_property_name} contains '${_arpg_match}' in '${_arpg_property_value}'"
                PARENT_SCOPE)
            return()
        endif()
    endforeach()

    foreach(_arpg_property_name IN ITEMS
            COMPILE_OPTIONS
            INTERFACE_COMPILE_OPTIONS)
        get_target_property(
            _arpg_property_value
            "${_arpg_inspected_target}"
            "${_arpg_property_name}")
        if("${_arpg_property_value}" STREQUAL ""
                OR "${_arpg_property_value}" MATCHES "-NOTFOUND$")
            continue()
        endif()

        string(TOLOWER "${_arpg_property_value}" _arpg_option_value)
        foreach(_arpg_msvc_prefix IN ITEMS "/fi" "/i" "/d")
            string(REPLACE
                "${_arpg_msvc_prefix}" ";" _arpg_option_value "${_arpg_option_value}")
        endforeach()
        arpg_definition_value_has_forbidden_dependency(
            "${_arpg_option_value}" _arpg_found _arpg_match)
        if(NOT _arpg_found)
            arpg_include_value_has_forbidden_dependency(
                "${_arpg_option_value}" _arpg_found _arpg_match)
        endif()
        if(_arpg_found)
            set("${OUT_FOUND}" TRUE PARENT_SCOPE)
            set("${OUT_REASON}"
                "${TARGET_PATH} ${_arpg_property_name} contains '${_arpg_match}' in '${_arpg_property_value}'"
                PARENT_SCOPE)
            return()
        endif()
    endforeach()

    foreach(_arpg_property_name IN ITEMS
            INCLUDE_DIRECTORIES
            INTERFACE_INCLUDE_DIRECTORIES)
        get_target_property(
            _arpg_property_value
            "${_arpg_inspected_target}"
            "${_arpg_property_name}")
        if("${_arpg_property_value}" STREQUAL ""
                OR "${_arpg_property_value}" MATCHES "-NOTFOUND$")
            continue()
        endif()

        arpg_include_value_has_forbidden_dependency(
            "${_arpg_property_value}" _arpg_found _arpg_match)
        if(_arpg_found)
            set("${OUT_FOUND}" TRUE PARENT_SCOPE)
            set("${OUT_REASON}"
                "${TARGET_PATH} ${_arpg_property_name} contains '${_arpg_match}' in '${_arpg_property_value}'"
                PARENT_SCOPE)
            return()
        endif()
    endforeach()

    foreach(_arpg_property_name IN ITEMS
            COMPILE_DEFINITIONS
            INTERFACE_COMPILE_DEFINITIONS)
        get_target_property(
            _arpg_property_value
            "${_arpg_inspected_target}"
            "${_arpg_property_name}")
        if("${_arpg_property_value}" STREQUAL ""
                OR "${_arpg_property_value}" MATCHES "-NOTFOUND$")
            continue()
        endif()

        arpg_definition_value_has_forbidden_dependency(
            "${_arpg_property_value}" _arpg_found _arpg_match)
        if(_arpg_found)
            set("${OUT_FOUND}" TRUE PARENT_SCOPE)
            set("${OUT_REASON}"
                "${TARGET_PATH} ${_arpg_property_name} contains '${_arpg_match}' in '${_arpg_property_value}'"
                PARENT_SCOPE)
            return()
        endif()
    endforeach()

    if(_arpg_is_imported)
        foreach(_arpg_property_name IN ITEMS
                IMPORTED_LOCATION
                IMPORTED_IMPLIB)
            get_target_property(
                _arpg_property_value
                "${_arpg_inspected_target}"
                "${_arpg_property_name}")
            if(NOT "${_arpg_property_value}" STREQUAL ""
                    AND NOT "${_arpg_property_value}" MATCHES "-NOTFOUND$")
                arpg_value_has_forbidden_dependency(
                    "${_arpg_property_value}" _arpg_found _arpg_match)
                if(_arpg_found)
                    set("${OUT_FOUND}" TRUE PARENT_SCOPE)
                    set("${OUT_REASON}"
                        "${TARGET_PATH} ${_arpg_property_name} contains '${_arpg_match}' in '${_arpg_property_value}'"
                        PARENT_SCOPE)
                    return()
                endif()
            endif()
        endforeach()

        foreach(_arpg_configuration IN LISTS _arpg_imported_configurations)
            string(TOUPPER
                "${_arpg_configuration}" _arpg_configuration_upper)
            foreach(_arpg_property_base IN ITEMS
                    IMPORTED_LOCATION
                    IMPORTED_IMPLIB)
                set(_arpg_property_name
                    "${_arpg_property_base}_${_arpg_configuration_upper}")
                get_target_property(
                    _arpg_property_value
                    "${_arpg_inspected_target}"
                    "${_arpg_property_name}")
                if("${_arpg_property_value}" STREQUAL ""
                        OR "${_arpg_property_value}" MATCHES "-NOTFOUND$")
                    continue()
                endif()
                arpg_value_has_forbidden_dependency(
                    "${_arpg_property_value}" _arpg_found _arpg_match)
                if(_arpg_found)
                    set("${OUT_FOUND}" TRUE PARENT_SCOPE)
                    set("${OUT_REASON}"
                        "${TARGET_PATH} ${_arpg_property_name} contains '${_arpg_match}' in '${_arpg_property_value}'"
                        PARENT_SCOPE)
                    return()
                endif()
            endforeach()
        endforeach()
    endif()

    set("${OUT_FOUND}" FALSE PARENT_SCOPE)
    set("${OUT_REASON}" "" PARENT_SCOPE)
endfunction()

function(arpg_find_target_dependency_leak ROOT_TARGET OUT_FOUND OUT_REASON)
    set_property(GLOBAL PROPERTY ARPG_BOUNDARY_VISITED_TARGETS "")
    arpg_walk_target_dependency_closure(
        "${ROOT_TARGET}" "${ROOT_TARGET}" _arpg_found _arpg_reason)
    set("${OUT_FOUND}" "${_arpg_found}" PARENT_SCOPE)
    set("${OUT_REASON}" "${_arpg_reason}" PARENT_SCOPE)
endfunction()

function(arpg_assert_target_dependency_boundary ROOT_TARGET)
    arpg_find_target_dependency_leak(
        "${ROOT_TARGET}" _arpg_found _arpg_reason)
    if(_arpg_found)
        message(FATAL_ERROR
            "${ROOT_TARGET} dependency closure leaks raylib: ${_arpg_reason}")
    endif()
endfunction()

function(arpg_walk_target_reachability
        TARGET_NAME FORBIDDEN_TARGET TARGET_PATH OUT_FOUND OUT_REASON)
    if(NOT TARGET "${TARGET_NAME}" OR NOT TARGET "${FORBIDDEN_TARGET}")
        set("${OUT_FOUND}" FALSE PARENT_SCOPE)
        set("${OUT_REASON}" "" PARENT_SCOPE)
        return()
    endif()

    get_target_property(_arpg_target_alias "${TARGET_NAME}" ALIASED_TARGET)
    if(_arpg_target_alias)
        set(_arpg_target "${_arpg_target_alias}")
    else()
        set(_arpg_target "${TARGET_NAME}")
    endif()
    get_target_property(
        _arpg_forbidden_alias "${FORBIDDEN_TARGET}" ALIASED_TARGET)
    if(_arpg_forbidden_alias)
        set(_arpg_forbidden "${_arpg_forbidden_alias}")
    else()
        set(_arpg_forbidden "${FORBIDDEN_TARGET}")
    endif()

    if(_arpg_target STREQUAL _arpg_forbidden)
        set("${OUT_FOUND}" TRUE PARENT_SCOPE)
        set("${OUT_REASON}" "${TARGET_PATH}" PARENT_SCOPE)
        return()
    endif()

    get_property(
        _arpg_visited GLOBAL PROPERTY ARPG_REACHABILITY_VISITED_TARGETS)
    list(FIND _arpg_visited "${_arpg_target}" _arpg_visited_index)
    if(NOT _arpg_visited_index EQUAL -1)
        set("${OUT_FOUND}" FALSE PARENT_SCOPE)
        set("${OUT_REASON}" "" PARENT_SCOPE)
        return()
    endif()
    set_property(
        GLOBAL APPEND PROPERTY
        ARPG_REACHABILITY_VISITED_TARGETS "${_arpg_target}")

    set(_arpg_link_property_names LINK_LIBRARIES INTERFACE_LINK_LIBRARIES)
    get_target_property(_arpg_is_imported "${_arpg_target}" IMPORTED)
    if(_arpg_is_imported)
        get_target_property(
            _arpg_imported_configurations
            "${_arpg_target}"
            IMPORTED_CONFIGURATIONS)
        if("${_arpg_imported_configurations}" MATCHES "-NOTFOUND$")
            set(_arpg_imported_configurations "")
        endif()
        list(APPEND _arpg_imported_configurations
            NOCONFIG DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
        list(REMOVE_DUPLICATES _arpg_imported_configurations)
        foreach(_arpg_property_base IN ITEMS
                IMPORTED_LINK_INTERFACE_LIBRARIES
                IMPORTED_LINK_DEPENDENT_LIBRARIES)
            list(APPEND _arpg_link_property_names "${_arpg_property_base}")
            foreach(_arpg_configuration IN LISTS _arpg_imported_configurations)
                string(TOUPPER
                    "${_arpg_configuration}" _arpg_configuration_upper)
                list(APPEND _arpg_link_property_names
                    "${_arpg_property_base}_${_arpg_configuration_upper}")
            endforeach()
        endforeach()
    endif()

    foreach(_arpg_property_name IN LISTS _arpg_link_property_names)
        get_target_property(
            _arpg_property_value "${_arpg_target}" "${_arpg_property_name}")
        if("${_arpg_property_value}" STREQUAL ""
                OR "${_arpg_property_value}" MATCHES "-NOTFOUND$")
            continue()
        endif()
        arpg_extract_target_candidates(
            "${_arpg_property_value}" _arpg_dependencies)
        foreach(_arpg_dependency IN LISTS _arpg_dependencies)
            arpg_walk_target_reachability(
                "${_arpg_dependency}"
                "${_arpg_forbidden}"
                "${TARGET_PATH} -> ${_arpg_dependency}"
                _arpg_nested_found
                _arpg_nested_reason)
            if(_arpg_nested_found)
                set("${OUT_FOUND}" TRUE PARENT_SCOPE)
                set("${OUT_REASON}" "${_arpg_nested_reason}" PARENT_SCOPE)
                return()
            endif()
        endforeach()
    endforeach()

    set("${OUT_FOUND}" FALSE PARENT_SCOPE)
    set("${OUT_REASON}" "" PARENT_SCOPE)
endfunction()

function(arpg_assert_target_not_reachable ROOT_TARGET FORBIDDEN_TARGET)
    set_property(GLOBAL PROPERTY ARPG_REACHABILITY_VISITED_TARGETS "")
    arpg_walk_target_reachability(
        "${ROOT_TARGET}"
        "${FORBIDDEN_TARGET}"
        "${ROOT_TARGET}"
        _arpg_found
        _arpg_reason)
    if(_arpg_found)
        message(FATAL_ERROR
            "${ROOT_TARGET} must not reach ${FORBIDDEN_TARGET}: ${_arpg_reason}")
    endif()
endfunction()

foreach(_arpg_positive_value IN ITEMS
        "raylib"
        "raylib::raylib"
        [=[$<LINK_ONLY:raylib>]=]
        [=[$<IF:$<BOOL:1>,clean,raylib>]=]
        "rlgl"
        "raymath"
        "raylib-cpp"
        "E:/vendor/raylib.lib"
        "E:/vendor/libraylib.a")
    arpg_value_has_forbidden_dependency(
        "${_arpg_positive_value}" _arpg_found _arpg_match)
    if(NOT _arpg_found)
        message(FATAL_ERROR
            "Boundary detector self-test missed: ${_arpg_positive_value}")
    endif()
endforeach()

arpg_value_has_forbidden_dependency(
    "E:/game/.worktrees/m00-raylib-host/src" _arpg_found _arpg_match)
if(_arpg_found)
    message(FATAL_ERROR
        "Boundary detector self-test falsely matched m00-raylib-host")
endif()

arpg_value_has_forbidden_dependency(
    "E:/raylib-host/src" _arpg_found _arpg_match)
if(_arpg_found)
    message(FATAL_ERROR
        "Boundary detector self-test falsely matched E:/raylib-host")
endif()

arpg_include_value_has_forbidden_dependency(
    "E:/build/_deps/raylib-src/include" _arpg_found _arpg_match)
if(NOT _arpg_found)
    message(FATAL_ERROR
        "Include detector self-test missed _deps/raylib")
endif()

foreach(_arpg_clean_include IN ITEMS
        "E:/raylib-host/src"
        "E:/game/.worktrees/m00-raylib-host/src")
    arpg_include_value_has_forbidden_dependency(
        "${_arpg_clean_include}" _arpg_found _arpg_match)
    if(_arpg_found)
        message(FATAL_ERROR
            "Include detector self-test falsely matched: ${_arpg_clean_include}")
    endif()
endforeach()

add_library(arpg_boundary_probe_leaf INTERFACE)
target_link_libraries(arpg_boundary_probe_leaf INTERFACE raylib)

add_library(arpg_boundary_probe_mid INTERFACE)
target_link_libraries(
    arpg_boundary_probe_mid
    INTERFACE arpg_boundary_probe_leaf)

foreach(_arpg_candidate_value IN ITEMS
        "arpg_boundary_probe_leaf"
        [=[$<LINK_ONLY:arpg_boundary_probe_leaf>]=]
        [=[$<IF:$<BOOL:1>,arpg_boundary_probe_leaf,clean>]=])
    arpg_extract_target_candidates(
        "${_arpg_candidate_value}" _arpg_candidates)
    list(FIND
        _arpg_candidates arpg_boundary_probe_leaf _arpg_candidate_index)
    if(_arpg_candidate_index EQUAL -1)
        message(FATAL_ERROR
            "Target parser self-test missed: ${_arpg_candidate_value}")
    endif()
endforeach()

arpg_find_target_dependency_leak(
    arpg_boundary_probe_mid _arpg_found _arpg_reason)
if(NOT _arpg_found)
    message(FATAL_ERROR
        "Recursive boundary self-test missed leaf -> mid -> raylib")
endif()

add_library(arpg_boundary_include_probe_leaf INTERFACE)
target_include_directories(
    arpg_boundary_include_probe_leaf
    INTERFACE "E:/build/_deps/raylib-src/include")

add_library(arpg_boundary_include_probe_mid INTERFACE)
target_link_libraries(
    arpg_boundary_include_probe_mid
    INTERFACE arpg_boundary_include_probe_leaf)

arpg_find_target_dependency_leak(
    arpg_boundary_include_probe_mid _arpg_found _arpg_reason)
if(NOT _arpg_found)
    message(FATAL_ERROR
        "Recursive boundary self-test missed _deps/raylib include path")
endif()

add_library(arpg_boundary_imported_binary UNKNOWN IMPORTED)
set_target_properties(
    arpg_boundary_imported_binary
    PROPERTIES IMPORTED_LOCATION "E:/vendor/raylib.lib")
add_library(
    arpg_boundary_imported_alias ALIAS arpg_boundary_imported_binary)
add_library(arpg_boundary_imported_leaf INTERFACE)
target_link_libraries(
    arpg_boundary_imported_leaf INTERFACE arpg_boundary_imported_alias)
add_library(arpg_boundary_imported_mid INTERFACE)
target_link_libraries(
    arpg_boundary_imported_mid INTERFACE arpg_boundary_imported_leaf)
arpg_find_target_dependency_leak(
    arpg_boundary_imported_mid _arpg_found _arpg_reason)
if(NOT _arpg_found)
    message(FATAL_ERROR
        "Recursive boundary self-test missed imported raylib location")
endif()

add_library(arpg_boundary_config_binary UNKNOWN IMPORTED)
set_target_properties(
    arpg_boundary_config_binary
    PROPERTIES
        IMPORTED_CONFIGURATIONS DEBUG
        IMPORTED_LOCATION_DEBUG "E:/vendor/raylib.lib")
arpg_find_target_dependency_leak(
    arpg_boundary_config_binary _arpg_found _arpg_reason)
if(NOT _arpg_found)
    message(FATAL_ERROR
        "Boundary self-test missed configured imported raylib location")
endif()

add_library(arpg_boundary_probe::rlgl UNKNOWN IMPORTED)
set_target_properties(
    arpg_boundary_probe::rlgl
    PROPERTIES IMPORTED_LOCATION "E:/vendor/clean.lib")
add_library(arpg_boundary_canonical_alias ALIAS arpg_boundary_probe::rlgl)
arpg_find_target_dependency_leak(
    arpg_boundary_canonical_alias _arpg_found _arpg_reason)
if(NOT _arpg_found)
    message(FATAL_ERROR
        "Boundary self-test missed forbidden canonical target name")
endif()

add_library(arpg_boundary_cycle_a INTERFACE)
add_library(arpg_boundary_cycle_b INTERFACE)
target_link_libraries(arpg_boundary_cycle_a INTERFACE arpg_boundary_cycle_b)
target_link_libraries(arpg_boundary_cycle_b INTERFACE arpg_boundary_cycle_a)
arpg_find_target_dependency_leak(
    arpg_boundary_cycle_a _arpg_found _arpg_reason)
if(_arpg_found)
    message(FATAL_ERROR
        "Boundary cycle self-test produced a false positive: ${_arpg_reason}")
endif()

set(_arpg_remaining_boundary_failures "")
foreach(_arpg_link_value IN ITEMS
        "raylib_static"
        "E:/vendor/raylibd.lib"
        "-lraylib"
        [=[/DEFAULTLIB:"raylib.lib"]=])
    arpg_value_has_forbidden_dependency(
        "${_arpg_link_value}" _arpg_found _arpg_reason)
    if(NOT _arpg_found)
        list(APPEND
            _arpg_remaining_boundary_failures
            "link:${_arpg_link_value}")
    endif()
endforeach()

foreach(_arpg_include_value IN ITEMS
        "E:/vendor/raylib/include"
        "E:/vendor/raylib-cpp/include"
        "E:/vendor/raylib/src"
        "E:/vendor/raylib-cpp/src")
    arpg_include_value_has_forbidden_dependency(
        "${_arpg_include_value}" _arpg_found _arpg_reason)
    if(NOT _arpg_found)
        list(APPEND
            _arpg_remaining_boundary_failures
            "include:${_arpg_include_value}")
    endif()
endforeach()

add_library(arpg_boundary_legacy_binary UNKNOWN IMPORTED)
set_target_properties(
    arpg_boundary_legacy_binary
    PROPERTIES
        IMPORTED_LOCATION "E:/vendor/clean.lib"
        IMPORTED_LINK_INTERFACE_LIBRARIES raylib)
add_library(arpg_boundary_legacy_alias ALIAS arpg_boundary_legacy_binary)
add_library(arpg_boundary_legacy_leaf INTERFACE)
target_link_libraries(
    arpg_boundary_legacy_leaf INTERFACE arpg_boundary_legacy_alias)
add_library(arpg_boundary_legacy_mid INTERFACE)
target_link_libraries(
    arpg_boundary_legacy_mid INTERFACE arpg_boundary_legacy_leaf)
arpg_find_target_dependency_leak(
    arpg_boundary_legacy_mid _arpg_found _arpg_reason)
if(NOT _arpg_found)
    list(APPEND
        _arpg_remaining_boundary_failures
        "imported:legacy-interface")
endif()

add_library(arpg_boundary_dependent_binary UNKNOWN IMPORTED)
set_target_properties(
    arpg_boundary_dependent_binary
    PROPERTIES
        IMPORTED_LOCATION "E:/vendor/clean.lib"
        IMPORTED_CONFIGURATIONS DEBUG
        IMPORTED_LINK_DEPENDENT_LIBRARIES_DEBUG raylib_static)
arpg_find_target_dependency_leak(
    arpg_boundary_dependent_binary _arpg_found _arpg_reason)
if(NOT _arpg_found)
    list(APPEND
        _arpg_remaining_boundary_failures
        "imported:configured-dependent")
endif()

add_library(arpg_boundary_legacy_objects OBJECT IMPORTED)
set_target_properties(
    arpg_boundary_legacy_objects
    PROPERTIES
        IMPORTED_CONFIGURATIONS DEBUG
        IMPORTED_OBJECTS_DEBUG "E:/vendor/raylib.obj")
arpg_find_target_dependency_leak(
    arpg_boundary_legacy_objects _arpg_found _arpg_reason)
if(NOT _arpg_found)
    list(APPEND
        _arpg_remaining_boundary_failures
        "imported:configured-objects")
endif()

add_library(arpg_boundary_defaultlib_probe INTERFACE)
target_link_options(
    arpg_boundary_defaultlib_probe
    INTERFACE "/DEFAULTLIB:raylib.lib")
arpg_find_target_dependency_leak(
    arpg_boundary_defaultlib_probe _arpg_found _arpg_reason)
if(NOT _arpg_found)
    list(APPEND
        _arpg_remaining_boundary_failures
        "link-options:DEFAULTLIB")
endif()

add_library(arpg_boundary_definition_probe INTERFACE)
target_compile_definitions(
    arpg_boundary_definition_probe
    INTERFACE "RL_HEADER=<raylib.h>")
arpg_find_target_dependency_leak(
    arpg_boundary_definition_probe _arpg_found _arpg_reason)
if(NOT _arpg_found)
    list(APPEND
        _arpg_remaining_boundary_failures
        "compile-definitions:raylib-header")
endif()

add_library(arpg_boundary_forced_include_probe INTERFACE)
target_compile_options(
    arpg_boundary_forced_include_probe
    INTERFACE "/FIE:/vendor/raylib/src/raylib.h")
arpg_find_target_dependency_leak(
    arpg_boundary_forced_include_probe _arpg_found _arpg_reason)
if(NOT _arpg_found)
    list(APPEND
        _arpg_remaining_boundary_failures
        "compile-options:forced-include")
endif()

add_library(arpg_boundary_deps_include_option_probe INTERFACE)
target_compile_options(
    arpg_boundary_deps_include_option_probe
    INTERFACE "/IE:/build/_deps/raylib-src/src")
arpg_find_target_dependency_leak(
    arpg_boundary_deps_include_option_probe _arpg_found _arpg_reason)
if(NOT _arpg_found)
    list(APPEND
        _arpg_remaining_boundary_failures
        "compile-options:deps-include")
endif()

if(_arpg_remaining_boundary_failures)
    string(JOIN
        ", " _arpg_remaining_boundary_failure_summary
        ${_arpg_remaining_boundary_failures})
    message(FATAL_ERROR
        "Remaining boundary detector self-tests failed: ${_arpg_remaining_boundary_failure_summary}")
endif()

arpg_assert_target_dependency_boundary(arpg_core)
arpg_assert_target_dependency_boundary(arpg_combat)
arpg_assert_target_dependency_boundary(arpg_dungeon)
arpg_assert_target_dependency_boundary(arpg_persistence)
arpg_assert_target_dependency_boundary(arpg_modifiers)
arpg_assert_target_dependency_boundary(arpg_passives)
arpg_assert_target_not_reachable(arpg_combat arpg_dungeon)
arpg_assert_target_not_reachable(arpg_persistence arpg_dungeon)
arpg_assert_target_not_reachable(arpg_persistence arpg_combat)
arpg_assert_target_not_reachable(arpg_dungeon arpg_persistence)
arpg_assert_target_not_reachable(arpg_combat arpg_persistence)
arpg_assert_target_not_reachable(arpg_passives arpg_combat)
arpg_assert_target_not_reachable(arpg_passives arpg_dungeon)
arpg_assert_target_not_reachable(arpg_passives arpg_persistence)

add_test(
    NAME architecture.core_no_raylib
    COMMAND "${CMAKE_COMMAND}"
        "-DSOURCE_DIR=${PROJECT_SOURCE_DIR}/src/core"
        "-DSOURCE_LABEL=Core"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/core_boundary_test.cmake")
set_tests_properties(
    architecture.core_no_raylib
    PROPERTIES LABELS "headless;architecture")

add_test(
    NAME architecture.combat_no_raylib
    COMMAND "${CMAKE_COMMAND}"
        "-DSOURCE_DIR=${PROJECT_SOURCE_DIR}/src/combat"
        "-DSOURCE_LABEL=Combat"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/core_boundary_test.cmake")
set_tests_properties(
    architecture.combat_no_raylib
    PROPERTIES LABELS "headless;architecture")

add_test(
    NAME stage11.architecture.death_boundaries
    COMMAND "${CMAKE_COMMAND}"
        "-DSOURCE_ROOT=${PROJECT_SOURCE_DIR}/src"
        "-DGUARD_TEST_ROOT=${CMAKE_CURRENT_BINARY_DIR}/stage11-guard-mutations"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/stage11_architecture_guard_test.cmake")
set_tests_properties(
    stage11.architecture.death_boundaries
    PROPERTIES LABELS "headless;architecture;stage11")

add_test(
    NAME stage11b.architecture.settings_boundaries
    COMMAND "${CMAKE_COMMAND}"
        "-DSOURCE_ROOT=${PROJECT_SOURCE_DIR}/src"
        "-DGUARD_TEST_ROOT=${CMAKE_CURRENT_BINARY_DIR}/stage11b-settings-guard-mutations"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/stage11b_architecture_guard_test.cmake")
set_tests_properties(
    stage11b.architecture.settings_boundaries
    PROPERTIES
        ENVIRONMENT "ARPG_TEST_FILTER=stage11b.architecture"
        LABELS "headless;platform;architecture;stage11b"
        TIMEOUT 300)

add_test(
    NAME stage11c.architecture.hud_boundaries
    COMMAND "${CMAKE_COMMAND}"
        "-DSOURCE_ROOT=${PROJECT_SOURCE_DIR}/src"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/stage11c_hud_architecture_guard_test.cmake")
set_tests_properties(
    stage11c.architecture.hud_boundaries
    PROPERTIES LABELS "headless;platform;architecture;stage11c")

add_test(
    NAME stage11c.architecture.hud_boundaries_self_test
    COMMAND "${CMAKE_COMMAND}"
        "-DSOURCE_ROOT=${PROJECT_SOURCE_DIR}/src"
        "-DGUARD_SCRIPT=${CMAKE_CURRENT_SOURCE_DIR}/stage11c_hud_architecture_guard_test.cmake"
        "-DBAD_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/stage11c_hud_bad_source.txt"
        "-DGUARD_TEST_ROOT=${CMAKE_CURRENT_BINARY_DIR}/stage11c-hud-guard-mutations"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/stage11c_hud_architecture_guard_self_test.cmake")
set_tests_properties(
    stage11c.architecture.hud_boundaries_self_test
    PROPERTIES
        LABELS "headless;platform;architecture;stage11c"
        TIMEOUT 300)

add_test(
    NAME stage11d.architecture.loot_boundaries
    COMMAND "${CMAKE_COMMAND}"
        "-DSOURCE_ROOT=${PROJECT_SOURCE_DIR}/src"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/stage11d_loot_architecture_guard_test.cmake")
set_tests_properties(
    stage11d.architecture.loot_boundaries
    PROPERTIES LABELS "headless;platform;architecture;stage11d")

add_test(
    NAME stage11d.architecture.loot_boundaries_self_test
    COMMAND "${CMAKE_COMMAND}"
        "-DSOURCE_ROOT=${PROJECT_SOURCE_DIR}/src"
        "-DGUARD_SCRIPT=${CMAKE_CURRENT_SOURCE_DIR}/stage11d_loot_architecture_guard_test.cmake"
        "-DBAD_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/stage11d_loot_bad_source.txt"
        "-DGUARD_TEST_ROOT=${CMAKE_CURRENT_BINARY_DIR}/stage11d-loot-guard-mutations"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/stage11d_loot_architecture_guard_self_test.cmake")
set_tests_properties(
    stage11d.architecture.loot_boundaries_self_test
    PROPERTIES
        LABELS "headless;platform;architecture;stage11d"
        TIMEOUT 300)

add_test(
    NAME architecture.dungeon_no_raylib
    COMMAND "${CMAKE_COMMAND}"
        "-DSOURCE_DIR=${PROJECT_SOURCE_DIR}/src/dungeon"
        "-DSOURCE_LABEL=Dungeon"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/core_boundary_test.cmake")
set_tests_properties(
    architecture.dungeon_no_raylib
    PROPERTIES LABELS "headless;architecture")

add_test(
    NAME architecture.persistence_no_raylib
    COMMAND "${CMAKE_COMMAND}"
        "-DSOURCE_DIR=${PROJECT_SOURCE_DIR}/src/persistence"
        "-DSOURCE_LABEL=Persistence"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/core_boundary_test.cmake")
set_tests_properties(
    architecture.persistence_no_raylib
    PROPERTIES LABELS "headless;architecture")

add_test(
    NAME architecture.progression_no_raylib
    COMMAND "${CMAKE_COMMAND}"
        "-DSOURCE_DIR=${PROJECT_SOURCE_DIR}/src/progression"
        "-DSOURCE_LABEL=Progression"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/core_boundary_test.cmake")
set_tests_properties(
    architecture.progression_no_raylib
    PROPERTIES LABELS "headless;architecture;progression")

add_test(
    NAME architecture.modifiers_no_raylib
    COMMAND "${CMAKE_COMMAND}"
        "-DSOURCE_DIR=${PROJECT_SOURCE_DIR}/src/modifiers"
        "-DSOURCE_LABEL=Modifiers"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/core_boundary_test.cmake")
set_tests_properties(
    architecture.modifiers_no_raylib
    PROPERTIES LABELS "headless;architecture;modifiers")

add_test(
    NAME architecture.passives_no_raylib
    COMMAND "${CMAKE_COMMAND}"
        "-DSOURCE_DIR=${PROJECT_SOURCE_DIR}/src/passives"
        "-DSOURCE_LABEL=Passives"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/core_boundary_test.cmake")
set_tests_properties(
    architecture.passives_no_raylib
    PROPERTIES LABELS "headless;architecture;passives")

foreach(_arpg_passives_forbidden_module IN ITEMS combat dungeon persistence)
    if(_arpg_passives_forbidden_module STREQUAL "combat")
        set(_arpg_passives_forbidden_label "Combat")
    elseif(_arpg_passives_forbidden_module STREQUAL "dungeon")
        set(_arpg_passives_forbidden_label "Dungeon")
    else()
        set(_arpg_passives_forbidden_label "Persistence")
    endif()
    add_test(
        NAME architecture.passives_no_${_arpg_passives_forbidden_module}
        COMMAND "${CMAKE_COMMAND}"
            "-DSOURCE_DIR=${PROJECT_SOURCE_DIR}/src/passives"
            "-DSOURCE_LABEL=Passives"
            "-DFORBIDDEN_MODULE=${_arpg_passives_forbidden_module}"
            "-DFORBIDDEN_LABEL=${_arpg_passives_forbidden_label}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/module_boundary_test.cmake")
    set_tests_properties(
        architecture.passives_no_${_arpg_passives_forbidden_module}
        PROPERTIES LABELS "headless;architecture;passives")
endforeach()

foreach(_arpg_forbidden_module IN ITEMS combat dungeon progression persistence)
    if(_arpg_forbidden_module STREQUAL "combat")
        set(_arpg_forbidden_label "Combat")
    elseif(_arpg_forbidden_module STREQUAL "dungeon")
        set(_arpg_forbidden_label "Dungeon")
    elseif(_arpg_forbidden_module STREQUAL "progression")
        set(_arpg_forbidden_label "Progression")
    else()
        set(_arpg_forbidden_label "Persistence")
    endif()
    add_test(
        NAME architecture.modifiers_no_${_arpg_forbidden_module}
        COMMAND "${CMAKE_COMMAND}"
            "-DSOURCE_DIR=${PROJECT_SOURCE_DIR}/src/modifiers"
            "-DSOURCE_LABEL=Modifiers"
            "-DFORBIDDEN_MODULE=${_arpg_forbidden_module}"
            "-DFORBIDDEN_LABEL=${_arpg_forbidden_label}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/module_boundary_test.cmake")
    set_tests_properties(
        architecture.modifiers_no_${_arpg_forbidden_module}
        PROPERTIES LABELS "headless;architecture;modifiers")
endforeach()

add_test(
    NAME architecture.combat_no_dungeon
    COMMAND "${CMAKE_COMMAND}"
        "-DSOURCE_DIR=${PROJECT_SOURCE_DIR}/src/combat"
        "-DSOURCE_LABEL=Combat"
        "-DFORBIDDEN_MODULE=dungeon"
        "-DFORBIDDEN_LABEL=Dungeon"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/module_boundary_test.cmake")
set_tests_properties(
    architecture.combat_no_dungeon
    PROPERTIES LABELS "headless;architecture")

add_test(
    NAME architecture.dungeon_no_persistence
    COMMAND "${CMAKE_COMMAND}"
        "-DSOURCE_DIR=${PROJECT_SOURCE_DIR}/src/dungeon"
        "-DSOURCE_LABEL=Dungeon"
        "-DFORBIDDEN_MODULE=persistence"
        "-DFORBIDDEN_LABEL=Persistence"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/module_boundary_test.cmake")
set_tests_properties(
    architecture.dungeon_no_persistence
    PROPERTIES LABELS "headless;architecture")

add_test(
    NAME architecture.combat_no_persistence
    COMMAND "${CMAKE_COMMAND}"
        "-DSOURCE_DIR=${PROJECT_SOURCE_DIR}/src/combat"
        "-DSOURCE_LABEL=Combat"
        "-DFORBIDDEN_MODULE=persistence"
        "-DFORBIDDEN_LABEL=Persistence"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/module_boundary_test.cmake")
set_tests_properties(
    architecture.combat_no_persistence
    PROPERTIES LABELS "headless;architecture")

add_test(
    NAME architecture.persistence_checkpoint_only
    COMMAND "${CMAKE_COMMAND}"
        "-DSOURCE_DIR=${PROJECT_SOURCE_DIR}/src/persistence"
        "-DSOURCE_LABEL=Persistence"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/persistence_boundary_test.cmake")
set_tests_properties(
    architecture.persistence_checkpoint_only
    PROPERTIES LABELS "headless;architecture")

add_test(
    NAME platform.input_latency_source
    COMMAND "${CMAKE_COMMAND}"
        "-DRAYLIB_SOURCE_DIR=${PROJECT_SOURCE_DIR}/src/platform/raylib"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/input_latency_source_test.cmake")
set_tests_properties(
    platform.input_latency_source
    PROPERTIES LABELS "headless;platform;architecture")

function(arpg_verify_direct_input_poison)
    include(CheckCXXSourceCompiles)
    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_REQUIRED_INCLUDES
        "${PROJECT_SOURCE_DIR}/src/platform/raylib")
    set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

    unset(ARPG_DIRECT_INPUT_POISON_ACTIVE_COMPILES CACHE)
    check_cxx_source_compiles([=[
        #include "direct_input_poison.hpp"
        static_assert(
            ::arpg::platform::direct_input_poison::active,
            "direct input poison must be active");
        int main() { return 0; }
    ]=] ARPG_DIRECT_INPUT_POISON_ACTIVE_COMPILES)
    if(NOT ARPG_DIRECT_INPUT_POISON_ACTIVE_COMPILES)
        message(FATAL_ERROR
            "direct input poison must expose a compilable active sentinel")
    endif()

    foreach(_arpg_poison_label IN ITEMS
            KEY MOUSE FOCUS PLATFORM_WRAPPER ADDRESS)
        if(_arpg_poison_label STREQUAL "KEY")
            set(_arpg_poison_declaration "bool IsKeyDown(int);")
            set(_arpg_poison_expression "IsKeyDown(0)")
        elseif(_arpg_poison_label STREQUAL "MOUSE")
            set(_arpg_poison_declaration "bool IsMouseButtonDown(int);")
            set(_arpg_poison_expression "IsMouseButtonDown(0)")
        elseif(_arpg_poison_label STREQUAL "FOCUS")
            set(_arpg_poison_declaration "bool IsWindowFocused();")
            set(_arpg_poison_expression "IsWindowFocused()")
        elseif(_arpg_poison_label STREQUAL "PLATFORM_WRAPPER")
            set(_arpg_poison_declaration
                "namespace arpg::platform { bool platform_key_down(int) noexcept; }\nusing arpg::platform::platform_key_down;")
            set(_arpg_poison_expression "platform_key_down(0)")
        else()
            set(_arpg_poison_declaration "bool IsKeyDown(int);")
            set(_arpg_poison_expression "&IsKeyDown")
        endif()

        set(_arpg_poison_result
            "ARPG_DIRECT_INPUT_POISON_${_arpg_poison_label}_COMPILES")
        set(_arpg_control_result
            "ARPG_DIRECT_INPUT_CONTROL_${_arpg_poison_label}_COMPILES")
        unset("${_arpg_control_result}" CACHE)
        check_cxx_source_compiles("
            ${_arpg_poison_declaration}
            int main() {
                static_cast<void>(${_arpg_poison_expression});
                return 0;
            }
        " "${_arpg_control_result}")
        if(NOT ${_arpg_control_result})
            message(FATAL_ERROR
                "direct input control probe must compile for ${_arpg_poison_label}")
        endif()
        unset("${_arpg_poison_result}" CACHE)
        check_cxx_source_compiles("
            ${_arpg_poison_declaration}
            #include \"direct_input_poison.hpp\"
            int main() {
                static_cast<void>(${_arpg_poison_expression});
                return 0;
            }
        " "${_arpg_poison_result}")
        if(${_arpg_poison_result})
            message(FATAL_ERROR
                "direct input poison compile probe unexpectedly accepted ${_arpg_poison_label}")
        endif()
    endforeach()
endfunction()

arpg_verify_direct_input_poison()

add_test(
    NAME platform.host_input_source
    COMMAND "${CMAKE_COMMAND}"
        "-DRAYLIB_SOURCE_DIR=${PROJECT_SOURCE_DIR}/src/platform/raylib"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/host_input_source_test.cmake")
set_tests_properties(
    platform.host_input_source
    PROPERTIES LABELS "headless;platform;architecture")

# Task 11 deliberately registers the real-host evidence gate before the
# harness exists.  This keeps the first execution red instead of silently
# accepting an unvalidated settings path.
add_test(
    NAME stage11b.settings_evidence_guard
    COMMAND "${CMAKE_COMMAND}"
        "-DSOURCE_ROOT=${PROJECT_SOURCE_DIR}"
        "-DGUARD_TEST_ROOT=${CMAKE_CURRENT_BINARY_DIR}/stage11b-evidence-guard"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/stage11b_settings_evidence_guard_test.cmake")
set_tests_properties(
    stage11b.settings_evidence_guard
    PROPERTIES LABELS "headless;platform;architecture;stage11b")


add_test(
    NAME stage11b.settings_evidence_guard_self_test
    COMMAND "${CMAKE_COMMAND}"
        "-DSOURCE_ROOT=${PROJECT_SOURCE_DIR}"
        "-DGUARD_TEST_ROOT=${CMAKE_CURRENT_BINARY_DIR}/stage11b-evidence-guard-mutations"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/stage11b_settings_evidence_guard_self_test.cmake")
set_tests_properties(
    stage11b.settings_evidence_guard_self_test
    PROPERTIES LABELS "headless;platform;architecture;stage11b")

add_test(
    NAME stage11c.hud_evidence_guard
    COMMAND "${CMAKE_COMMAND}"
        "-DSOURCE_ROOT=${PROJECT_SOURCE_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/stage11c_hud_evidence_guard_test.cmake")
set_tests_properties(
    stage11c.hud_evidence_guard
    PROPERTIES LABELS "headless;platform;architecture;stage11c")

add_test(
    NAME stage11c.hud_evidence_guard_self_test
    COMMAND "${CMAKE_COMMAND}"
        "-DSOURCE_ROOT=${PROJECT_SOURCE_DIR}"
        "-DGUARD_TEST_ROOT=${CMAKE_CURRENT_BINARY_DIR}/stage11c-hud-evidence-mutations"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/stage11c_hud_evidence_guard_self_test.cmake")
set_tests_properties(
    stage11c.hud_evidence_guard_self_test
    PROPERTIES LABELS "headless;platform;architecture;stage11c" TIMEOUT 300)

add_test(
    NAME stage11d.renderer_integration_guard
    COMMAND "${CMAKE_COMMAND}"
        "-DSOURCE_ROOT=${PROJECT_SOURCE_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/stage11d_renderer_integration_guard_test.cmake")
set_tests_properties(
    stage11d.renderer_integration_guard
    PROPERTIES LABELS "headless;platform;architecture;stage11d")

add_test(
    NAME stage11d.renderer_integration_guard_self_test
    COMMAND "${CMAKE_COMMAND}"
        "-DSOURCE_ROOT=${PROJECT_SOURCE_DIR}"
        "-DGUARD_TEST_ROOT=${CMAKE_CURRENT_BINARY_DIR}/stage11d-renderer-guard-mutations"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/stage11d_renderer_integration_guard_self_test.cmake")
set_tests_properties(
    stage11d.renderer_integration_guard_self_test
    PROPERTIES LABELS "headless;platform;architecture;stage11d")

add_test(
    NAME stage11d.loot_evidence_guard
    COMMAND "${CMAKE_COMMAND}"
        "-DSOURCE_ROOT=${PROJECT_SOURCE_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/stage11d_loot_evidence_guard_test.cmake")
set_tests_properties(
    stage11d.loot_evidence_guard
    PROPERTIES LABELS "headless;platform;architecture;stage11d")

add_test(
    NAME stage11d.loot_evidence_guard_self_test
    COMMAND "${CMAKE_COMMAND}"
        "-DSOURCE_ROOT=${PROJECT_SOURCE_DIR}"
        "-DGUARD_TEST_ROOT=${CMAKE_CURRENT_BINARY_DIR}/stage11d-loot-evidence-mutations"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/stage11d_loot_evidence_guard_self_test.cmake")
set_tests_properties(
    stage11d.loot_evidence_guard_self_test
    PROPERTIES LABELS "headless;platform;architecture;stage11d" TIMEOUT 300)

add_test(
    NAME stage14.audio_sources
    COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass -File
        "${CMAKE_CURRENT_SOURCE_DIR}/stage14_audio_source_validator.ps1"
        -AssetDirectory "${PROJECT_SOURCE_DIR}/assets/stage14/audio")
set_tests_properties(
    stage14.audio_sources
    PROPERTIES LABELS "headless;platform;stage14")

add_test(
    NAME stage14.audio_integration_guard
    COMMAND "${CMAKE_COMMAND}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/stage14_audio_integration_guard.cmake")
set_tests_properties(
    stage14.audio_integration_guard
    PROPERTIES LABELS "headless;platform;architecture;stage14")

add_test(
    NAME stage15.audio_sources
    COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass -File
        "${CMAKE_CURRENT_SOURCE_DIR}/stage15_audio_source_validator.ps1"
        -AssetDirectory "${PROJECT_SOURCE_DIR}/assets/stage15/audio")
set_tests_properties(
    stage15.audio_sources
    PROPERTIES LABELS "headless;platform;stage15")

add_test(
    NAME stage15.audio_sources_self_test
    COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass -File
        "${CMAKE_CURRENT_SOURCE_DIR}/stage15_audio_source_validator_self_test.ps1"
        -AssetDirectory "${PROJECT_SOURCE_DIR}/assets/stage15/audio"
        -Validator "${CMAKE_CURRENT_SOURCE_DIR}/stage15_audio_source_validator.ps1"
        -MutationRoot "${CMAKE_CURRENT_BINARY_DIR}/stage15-audio-source-mutations")
set_tests_properties(
    stage15.audio_sources_self_test
    PROPERTIES LABELS "headless;platform;negative;stage15" TIMEOUT 60)

add_test(
    NAME stage15.audio_integration_guard
    COMMAND "${CMAKE_COMMAND}"
        "-DSOURCE_ROOT=${PROJECT_SOURCE_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/stage15_audio_integration_guard.cmake")
set_tests_properties(
    stage15.audio_integration_guard
    PROPERTIES LABELS "headless;platform;architecture;stage15")
