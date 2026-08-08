if(NOT DEFINED SOURCE_ROOT OR NOT DEFINED GUARD_TEST_ROOT)
    message(FATAL_ERROR "SOURCE_ROOT and GUARD_TEST_ROOT are required")
endif()
set(_guard "${SOURCE_ROOT}/tests/platform/stage11d_loot_evidence_guard_test.cmake")
set(_sequence_guard
    "${SOURCE_ROOT}/tests/platform/host_validation_sequence_guard_test.cmake")
set(_host "${SOURCE_ROOT}/src/platform/raylib/raylib_host.cpp")
get_filename_component(_host_source_dir "${_host}" DIRECTORY)
set(_host_validation_runtime
    "${_host_source_dir}/host_validation_runtime.cpp")
set(_settings_runtime
    "${_host_source_dir}/host_settings_runtime.cpp")
set(_stage_header
    "${SOURCE_ROOT}/src/platform/raylib/host_validation_stage11d.hpp")
set(_runtime
    "${SOURCE_ROOT}/src/platform/raylib/host_validation_stage11d_runtime.cpp")
set(_report
    "${SOURCE_ROOT}/src/platform/raylib/host_validation_stage11d_report.cpp")
set(_renderer "${SOURCE_ROOT}/src/platform/raylib/combat_renderer.cpp")
set(_formal "${SOURCE_ROOT}/tests/platform/stage11d_loot_formal_game_validation.cpp")
set(_validator "${SOURCE_ROOT}/tests/platform/stage11d_loot_formal_validator.ps1")
foreach(_file IN ITEMS "${_guard}" "${_sequence_guard}" "${_host}"
        "${_host_validation_runtime}" "${_settings_runtime}"
        "${_stage_header}" "${_runtime}"
        "${_report}" "${_renderer}" "${_formal}" "${_validator}")
    if(NOT EXISTS "${_file}")
        message(FATAL_ERROR "Stage11D guard self-test input is missing: ${_file}")
    endif()
endforeach()
file(REMOVE_RECURSE "${GUARD_TEST_ROOT}")
file(MAKE_DIRECTORY "${GUARD_TEST_ROOT}")

set(_baseline_options "")
if(DEFINED TASK7C_M24_M25_TARGETED_ONLY
        AND TASK7C_M24_M25_TARGETED_ONLY)
    list(APPEND _baseline_options "-DSTAGE11D_TASK7C_ONLY=ON")
endif()
if(DEFINED STAGE11D_ABYSS_PHYSICAL_TARGETED_ONLY
        AND STAGE11D_ABYSS_PHYSICAL_TARGETED_ONLY)
    list(APPEND _baseline_options "-DSTAGE11D_ABYSS_PHYSICAL_ONLY=ON")
endif()
if(DEFINED STAGE11D_FORMAL_LIMIT_TARGETED_ONLY
        AND STAGE11D_FORMAL_LIMIT_TARGETED_ONLY)
    list(APPEND _baseline_options "-DSTAGE11D_FORMAL_LIMIT_ONLY=ON")
endif()
execute_process(COMMAND "${CMAKE_COMMAND}" "-DSOURCE_ROOT=${SOURCE_ROOT}"
    ${_baseline_options} -P "${_guard}"
    RESULT_VARIABLE _baseline OUTPUT_QUIET ERROR_QUIET)
if(NOT _baseline EQUAL 0)
    message(FATAL_ERROR "Stage11D loot evidence guard rejected its baseline")
endif()

if(DEFINED STAGE11D_ABYSS_PHYSICAL_TARGETED_ONLY
        AND STAGE11D_ABYSS_PHYSICAL_TARGETED_ONLY)
    set_property(GLOBAL PROPERTY
        STAGE11D_ABYSS_PHYSICAL_EXECUTED_MUTATIONS 0)
endif()
if(DEFINED STAGE11D_FORMAL_LIMIT_TARGETED_ONLY
        AND STAGE11D_FORMAL_LIMIT_TARGETED_ONLY)
    set_property(GLOBAL PROPERTY
        STAGE11D_FORMAL_LIMIT_EXECUTED_MUTATIONS 0)
endif()

function(expect_rejected NAME OVERRIDE PATH EXPECTED_REASON)
    if(DEFINED STAGE11D_ABYSS_PHYSICAL_TARGETED_ONLY
            AND STAGE11D_ABYSS_PHYSICAL_TARGETED_ONLY)
        if(DEFINED STAGE11D_ABYSS_PHYSICAL_TAIL_ONLY
                AND STAGE11D_ABYSS_PHYSICAL_TAIL_ONLY)
            set(_targeted_names
                "Stage11D sweep opportunity cannot run at low health"
                "Stage11D dual sweep ownership cannot omit outer reset"
                "Stage11D uncaptured abyss reward must use physical approach")
        else()
            set(_targeted_names
            "Stage11D rearm cannot bypass physical suspension"
            "Stage11D driver must honor physical suspension"
            "Stage11D streaming sweep must map through physical movement"
            "Stage11D unavailable player must stay physically neutral"
            "Stage11D ranged movement must return before attacking"
            "Stage11D area skill must use the equipped physical slot"
            "Stage11D nearby danger must replan the ranged stance"
            "Stage11D suspension gate cannot be weakened"
            "Stage11D availability gate cannot move into a dead branch"
            "Stage11D area skill cannot write a second physical slot"
            "Stage11D melee fallback cannot move into a dead branch"
            "Stage11D high-health target cannot trigger danger escape"
            "Stage11D recovery fire route must use the plan destination"
            "Stage11D driver cannot write a physical skill slot directly"
            "Stage11D aggressive branch cannot move under an unbraced dead branch"
            "Stage11D aggressive branch cannot hide alternate skill-slot access"
            "Stage11D aggressive branch cannot alias physical skill slots"
            "Stage11D storm injection must return before draw dispatch"
            "Stage11D sweep cursor state cannot be omitted"
            "Stage11D nearest sweep cursor comparison cannot be reversed"
            "Stage11D sweep opportunity cannot run at low health"
            "Stage11D dual sweep ownership cannot omit outer reset"
            "Stage11D uncaptured abyss reward must use physical approach")
        endif()
        list(FIND _targeted_names "${NAME}" _targeted_index)
        if(_targeted_index EQUAL -1)
            return()
        endif()
    elseif(DEFINED STAGE11D_FORMAL_LIMIT_TARGETED_ONLY
            AND STAGE11D_FORMAL_LIMIT_TARGETED_ONLY)
        set(_targeted_names
            "Stage11D formal scenario cannot move under an unbraced dead branch"
            "Stage11D formal fixed step cannot move into a comment"
            "Stage11D formal fixed step cannot move into a string"
            "Stage11D formal fixed step cannot move into a dead branch"
            "Stage11D abyss budget call cannot move into a dead branch"
            "Stage11D formal fixed step cannot move under an unbraced dead branch"
            "Stage11D formal frame limit cannot move under an unbraced dead branch"
            "Stage11D abyss validation survival passives cannot be omitted"
            "Stage11D live-damage fixture cannot be omitted"
            "Stage11D live-damage helper cannot weaken barrier-only removal"
            "Stage11D abyss frame-budget manifest cannot move into a comment"
            "Stage11D validator VSync check cannot move into an uncalled function")
        list(FIND _targeted_names "${NAME}" _targeted_index)
        if(_targeted_index EQUAL -1)
            return()
        endif()
    elseif(DEFINED TASK5A_TARGETED_ONLY AND TASK5A_TARGETED_ONLY)
        set(_targeted_names
            "stage11d header state payload cross scope"
            "runtime selector payload cross scope"
            "host pause call in uncalled lambda"
            "host fixed-step activation payload cross scope"
            "host real moved definition")
        list(FIND _targeted_names "${NAME}" _targeted_index)
        if(_targeted_index EQUAL -1)
            return()
        endif()
    elseif(DEFINED TASK5B_TARGETED_ONLY AND TASK5B_TARGETED_ONLY)
        set(_targeted_names
            "report semantic recorder comment decoy"
            "report target evaluator string decoy"
            "report summary forward declaration decoy"
            "report semantic payload cross function decoy")
        list(FIND _targeted_names "${NAME}" _targeted_index)
        if(_targeted_index EQUAL -1)
            return()
        endif()
    elseif(DEFINED ROUND3_TARGETED_ONLY AND ROUND3_TARGETED_ONLY)
        set(_targeted_names
            "tuple comparisons cannot move into comments"
            "pickup predicate cannot move into a comment"
            "tuple assignment cannot move after its Require"
            "pickup derivation cannot move after its Require")
        list(FIND _targeted_names "${NAME}" _targeted_index)
        if(_targeted_index EQUAL -1)
            return()
        endif()
    elseif(DEFINED TASK9_TARGETED_ONLY AND TASK9_TARGETED_ONLY)
        set(_targeted_names
            "second render plan"
            "draw-external render plan"
            "diagnostic monster damage cannot be required"
            "tuple assignment cannot move into a here string"
            "tuple comparisons cannot move into comments"
            "tuple assertion cannot move into an uncalled function"
            "tuple assignment cannot move after its Require"
            "pickup derivation cannot move into a here string"
            "pickup predicate cannot move into a comment"
            "pickup conservation cannot move into a false branch"
            "pickup derivation cannot move after its Require"
            "fixture prefix length cannot become a required kill count"
            "player damage observation cannot be removed"
            "player damage observation cannot use a below-max snapshot"
            "rare capture must use the matching abyss label ordinal"
            "abyss ordinal helper must require an abyss label"
            "per-rarity tuple check cannot be weakened"
            "pickup conservation cannot be reduced to a count")
        list(FIND _targeted_names "${NAME}" _targeted_index)
        if(_targeted_index EQUAL -1)
            return()
        endif()
    endif()
    if(DEFINED STAGE11D_ABYSS_PHYSICAL_TARGETED_ONLY
            AND STAGE11D_ABYSS_PHYSICAL_TARGETED_ONLY)
        get_property(_executed GLOBAL PROPERTY
            STAGE11D_ABYSS_PHYSICAL_EXECUTED_MUTATIONS)
        math(EXPR _executed "${_executed} + 1")
        set_property(GLOBAL PROPERTY
            STAGE11D_ABYSS_PHYSICAL_EXECUTED_MUTATIONS ${_executed})
        if(DEFINED STAGE11D_ABYSS_PHYSICAL_TAIL_ONLY
                AND STAGE11D_ABYSS_PHYSICAL_TAIL_ONLY)
            set(_expected_abyss_mutations 3)
        else()
            set(_expected_abyss_mutations 23)
        endif()
        if(_executed GREATER _expected_abyss_mutations)
            message(FATAL_ERROR
                "Stage11D rare-abyss targeted guard executed too many mutations: ${_executed}")
        endif()
    elseif(DEFINED STAGE11D_FORMAL_LIMIT_TARGETED_ONLY
            AND STAGE11D_FORMAL_LIMIT_TARGETED_ONLY)
        get_property(_executed GLOBAL PROPERTY
            STAGE11D_FORMAL_LIMIT_EXECUTED_MUTATIONS)
        math(EXPR _executed "${_executed} + 1")
        set_property(GLOBAL PROPERTY
            STAGE11D_FORMAL_LIMIT_EXECUTED_MUTATIONS ${_executed})
        if(_executed GREATER 12)
            message(FATAL_ERROR
                "Stage11D formal limit targeted guard executed too many mutations: ${_executed}")
        endif()
    endif()
    set(_guard_options "")
    if(DEFINED STAGE11D_ABYSS_PHYSICAL_TARGETED_ONLY
            AND STAGE11D_ABYSS_PHYSICAL_TARGETED_ONLY)
        list(APPEND _guard_options "-DSTAGE11D_ABYSS_PHYSICAL_ONLY=ON")
    elseif(DEFINED STAGE11D_FORMAL_LIMIT_TARGETED_ONLY
            AND STAGE11D_FORMAL_LIMIT_TARGETED_ONLY)
        list(APPEND _guard_options "-DSTAGE11D_FORMAL_LIMIT_ONLY=ON")
    endif()
    if(EXPECTED_REASON MATCHES "^T7C-M2(4|5)")
        list(APPEND _guard_options "-DSTAGE11D_TASK7C_ONLY=ON")
    endif()
    if(OVERRIDE STREQUAL "HOST_VALIDATION_RUNTIME")
        if(NAME MATCHES "^facade post-tick")
            list(APPEND _guard_options
                "-DSTAGE11D_POST_TICK_OWNER_ONLY=ON")
        elseif(NOT NAME MATCHES "^task7c ")
            list(APPEND _guard_options "-DSTAGE11D_INPUT_OWNER_ONLY=ON")
        endif()
    endif()
    execute_process(COMMAND "${CMAKE_COMMAND}" "-DSOURCE_ROOT=${SOURCE_ROOT}"
        "-D${OVERRIDE}_OVERRIDE=${PATH}" ${_guard_options} -P "${_guard}"
        RESULT_VARIABLE _result OUTPUT_VARIABLE _output ERROR_VARIABLE _error)
    if(_result EQUAL 0)
        message(FATAL_ERROR "Stage11D loot evidence guard accepted mutation: ${NAME}")
    endif()
    set(_log "${_output}\n${_error}")
    string(REGEX REPLACE "[ \t\r\n]+" " " _normalized_log "${_log}")
    string(REGEX REPLACE "[ \t\r\n]+" " " _normalized_reason
        "${EXPECTED_REASON}")
    string(FIND "${_normalized_log}" "${_normalized_reason}" _reason)
    if(_reason EQUAL -1)
        message(FATAL_ERROR
            "Stage11D guard rejected ${NAME} for the wrong reason: ${_log}")
    endif()
endfunction()

if(NOT (DEFINED STAGE11D_ABYSS_PHYSICAL_TARGETED_ONLY
        AND STAGE11D_ABYSS_PHYSICAL_TARGETED_ONLY))
function(task8b_expect_accepted NAME OVERRIDE PATH)
    execute_process(COMMAND "${CMAKE_COMMAND}" "-DSOURCE_ROOT=${SOURCE_ROOT}"
        "-D${OVERRIDE}_OVERRIDE=${PATH}" -P "${_guard}"
        RESULT_VARIABLE _result OUTPUT_VARIABLE _output ERROR_VARIABLE _error)
    if(NOT _result EQUAL 0)
        message(FATAL_ERROR
            "${NAME} was rejected: ${_output}\n${_error}")
    endif()
endfunction()

function(task8b_run_owner_cases)
    file(READ "${_settings_runtime}" _settings_text)
    set(_rollback_write
        "this->live->loot_filter_mode =\n                this->pause_menu->committed.loot_filter_mode;")
    set(_rollback_write_one_line
        "this->live->loot_filter_mode = this->pause_menu->committed.loot_filter_mode;")
    string(REGEX REPLACE "[ \t\r\n]+" "" _settings_normalized
        "${_settings_text}")
    string(REGEX MATCHALL
        "this->live->loot_filter_mode=this->pause_menu->committed[.]loot_filter_mode"
        _rollback_writes "${_settings_normalized}")
    list(LENGTH _rollback_writes _rollback_write_count)
    if(NOT _rollback_write_count EQUAL 2)
        message(FATAL_ERROR "Task8B rollback write mutation sites disappeared")
    endif()

    string(REPLACE "${_rollback_write}" "" _missing "${_settings_text}")
    if(_missing STREQUAL _settings_text)
        message(FATAL_ERROR "Task8B rollback-write removal anchor disappeared")
    endif()
    set(_path "${GUARD_TEST_ROOT}/settings-runtime-missing-writes.cpp")
    file(WRITE "${_path}" "${_missing}")
    expect_rejected("settings runtime missing rollback writes"
        SETTINGS_RUNTIME "${_path}"
        "requires exactly two canonical production live-settings rollback writes")

    set(_inactive_decoy
        "#if 0\nnamespace arpg::platform {\nstruct Task8BInactiveRollbackDecoy {\n    settings::SettingsData* live{};\n    PauseMenuState* pause_menu{};\n    void write() {\n        ${_rollback_write}\n        ${_rollback_write}\n    }\n};\n}\n#endif\n")
    set(_inactive "${_missing}\n${_inactive_decoy}")
    set(_path "${GUARD_TEST_ROOT}/settings-runtime-inactive-writes.cpp")
    file(WRITE "${_path}" "${_inactive}")
    expect_rejected("settings runtime inactive rollback writes"
        SETTINGS_RUNTIME "${_path}"
        "requires exactly two canonical production live-settings rollback writes")

    set(_switch_anchor "    switch (command) {")
    set(_lambda_decoy
        "    const auto task8b_decoy = [this]() noexcept {\n        ${_rollback_write}\n        ${_rollback_write}\n    };\n    static_cast<void>(task8b_decoy);\n${_switch_anchor}")
    string(REPLACE "${_switch_anchor}" "${_lambda_decoy}"
        _lambda "${_missing}")
    if(_lambda STREQUAL _missing)
        message(FATAL_ERROR "Task8B lambda mutation anchor disappeared")
    endif()
    set(_path "${GUARD_TEST_ROOT}/settings-runtime-lambda-writes.cpp")
    file(WRITE "${_path}" "${_lambda}")
    expect_rejected("settings runtime lambda rollback writes"
        SETTINGS_RUNTIME "${_path}"
        "requires exactly two canonical production live-settings rollback writes")

    set(_constexpr_lambda_decoy
        "    const auto task8b_constexpr_decoy = [this]() constexpr {\n        ${_rollback_write}\n        ${_rollback_write}\n    };\n    static_cast<void>(task8b_constexpr_decoy);\n${_switch_anchor}")
    string(REPLACE "${_switch_anchor}" "${_constexpr_lambda_decoy}"
        _constexpr_lambda "${_missing}")
    if(_constexpr_lambda STREQUAL _missing)
        message(FATAL_ERROR "Task8B constexpr-lambda mutation anchor disappeared")
    endif()
    set(_path
        "${GUARD_TEST_ROOT}/settings-runtime-constexpr-lambda-writes.cpp")
    file(WRITE "${_path}" "${_constexpr_lambda}")
    expect_rejected("settings runtime constexpr lambda rollback writes"
        SETTINGS_RUNTIME "${_path}"
        "requires exactly two canonical production live-settings rollback writes")

    set(_local_type_decoy
        "    struct Task8BRollbackDecoy {\n        settings::SettingsData* live{};\n        PauseMenuState* pause_menu{};\n        void write() {\n            ${_rollback_write}\n            ${_rollback_write}\n        }\n    };\n${_switch_anchor}")
    string(REPLACE "${_switch_anchor}" "${_local_type_decoy}"
        _local_type "${_missing}")
    if(_local_type STREQUAL _missing)
        message(FATAL_ERROR "Task8B local-type mutation anchor disappeared")
    endif()
    set(_path "${GUARD_TEST_ROOT}/settings-runtime-local-type-writes.cpp")
    file(WRITE "${_path}" "${_local_type}")
    expect_rejected("settings runtime local-type rollback writes"
        SETTINGS_RUNTIME "${_path}"
        "requires exactly two canonical production live-settings rollback writes")

    set(_cross_function "${_missing}\nnamespace arpg::platform {\nvoid task8b_cross_function(settings::SettingsData* live,\n        const PauseMenuState* pause_menu) {\n    live->loot_filter_mode = pause_menu->committed.loot_filter_mode;\n    live->loot_filter_mode = pause_menu->committed.loot_filter_mode;\n}\n}\n")
    set(_path "${GUARD_TEST_ROOT}/settings-runtime-cross-function-writes.cpp")
    file(WRITE "${_path}" "${_cross_function}")
    expect_rejected("settings runtime cross-function rollback writes"
        SETTINGS_RUNTIME "${_path}"
        "requires exactly two canonical production live-settings rollback writes")

    set(_comment_decoys
        "${_settings_text}\n// ${_rollback_write_one_line}\nconstexpr const char* task8b_decoy = \"${_rollback_write_one_line}\";\n")
    set(_path "${GUARD_TEST_ROOT}/settings-runtime-comment-decoys.cpp")
    file(WRITE "${_path}" "${_comment_decoys}")
    task8b_expect_accepted("settings runtime comment decoys"
        SETTINGS_RUNTIME "${_path}")

    set(_shadow_write
        "settings::SettingsData* const task8b_shadow_live = &pause_menu->draft;\n            task8b_shadow_live->loot_filter_mode = pause_menu->committed.loot_filter_mode;")
    string(REPLACE "${_rollback_write}" "${_shadow_write}"
        _shadowed_live "${_settings_text}")
    if(_shadowed_live STREQUAL _settings_text)
        message(FATAL_ERROR "Task8B shadowed-live mutation anchor disappeared")
    endif()
    set(_path "${GUARD_TEST_ROOT}/settings-runtime-shadowed-live.cpp")
    file(WRITE "${_path}" "${_shadowed_live}")
    expect_rejected("settings runtime shadowed live rollback writes"
        SETTINGS_RUNTIME "${_path}"
        "requires exactly two canonical production live-settings rollback writes")

    set(_settings_owner_anchor "bool HostSettingsRuntime::settle(")
    string(REPLACE "${_settings_owner_anchor}"
        "#define live input\n${_settings_owner_anchor}"
        _macro_rewrite "${_settings_text}")
    if(_macro_rewrite STREQUAL _settings_text)
        message(FATAL_ERROR "Task8B settings macro mutation anchor disappeared")
    endif()
    set(_path "${GUARD_TEST_ROOT}/settings-runtime-macro-rewrite.cpp")
    file(WRITE "${_path}" "${_macro_rewrite}")
    expect_rejected("settings runtime macro rewrite"
        SETTINGS_RUNTIME "${_path}"
        "forbids settings runtime preprocessor macros")

    string(ASCII 92 _task8b_macro_backslash)
    string(ASCII 10 _task8b_macro_line_feed)
    set(_task8b_macro_names spliced digraph)
    set(_task8b_macro_directives
        "#defi${_task8b_macro_backslash}${_task8b_macro_line_feed}ne live input"
        "%:define live input")
    foreach(_task8b_macro_index RANGE 0 1)
        list(GET _task8b_macro_names ${_task8b_macro_index}
            _task8b_macro_name)
        list(GET _task8b_macro_directives ${_task8b_macro_index}
            _task8b_macro_directive)
        string(REPLACE "${_settings_owner_anchor}"
            "${_task8b_macro_directive}\n${_settings_owner_anchor}"
            _task8b_macro_mutation "${_settings_text}")
        if(_task8b_macro_mutation STREQUAL _settings_text)
            message(FATAL_ERROR
                "Task8B ${_task8b_macro_name} macro mutation anchor disappeared")
        endif()
        set(_path
            "${GUARD_TEST_ROOT}/settings-runtime-macro-${_task8b_macro_name}.cpp")
        file(WRITE "${_path}" "${_task8b_macro_mutation}")
        expect_rejected("settings runtime ${_task8b_macro_name} macro rewrite"
            SETTINGS_RUNTIME "${_path}"
            "forbids settings runtime preprocessor macros")
    endforeach()

    set(_inactive_owner
        "${_settings_text}\n#if 0\nnamespace arpg::platform {\nbool HostSettingsRuntime::settle(\n    PauseCommand, bool) { return false; }\n}\n#endif\n")
    set(_path "${GUARD_TEST_ROOT}/settings-runtime-inactive-owner.cpp")
    file(WRITE "${_path}" "${_inactive_owner}")
    expect_rejected("settings runtime inactive duplicate owner"
        SETTINGS_RUNTIME "${_path}"
        "requires one active and lexical settings settlement owner")

    set(_renamed_helper
        "${_settings_text}\nnamespace arpg::platform {\nvoid task8b_external_filter_write(settings::SettingsData* target,\n        const PauseMenuState* menu) {\n    target->loot_filter_mode = menu->committed.loot_filter_mode;\n}\n}\n")
    set(_path "${GUARD_TEST_ROOT}/settings-runtime-renamed-helper.cpp")
    file(WRITE "${_path}" "${_renamed_helper}")
    expect_rejected("settings runtime renamed helper write"
        SETTINGS_RUNTIME "${_path}"
        "forbids settings writes outside settlement")

    file(READ "${_host}" _host_text)
    string(APPEND _host_text
        "\nnamespace arpg::platform {\nvoid task8b_host_write(settings::SettingsData& live_settings) {\n    live_settings.loot_filter_mode = settings::LootFilterMode::rare_only;\n}\n}\n")
    set(_path "${GUARD_TEST_ROOT}/host-extra-live-filter-write.cpp")
    file(WRITE "${_path}" "${_host_text}")
    expect_rejected("host extra live filter write" HOST "${_path}"
        "forbids Host live-settings filter writes")

    file(READ "${_host}" _host_alias_text)
    string(APPEND _host_alias_text
        "\nnamespace arpg::platform {\nvoid task8b_host_alias_write(settings::SettingsData& live_settings) {\n    auto& active = live_settings;\n    active.loot_filter_mode = settings::LootFilterMode::rare_only;\n}\n}\n")
    set(_path "${GUARD_TEST_ROOT}/host-alias-live-filter-write.cpp")
    file(WRITE "${_path}" "${_host_alias_text}")
    expect_rejected("host alias live filter write" HOST "${_path}"
        "forbids Host live-settings filter writes")

    set(_host_coordinator_anchor
        "        HostSettingsRuntime settings_runtime{&settings_notice, &pause_menu,\n            &live_settings, &input_settings, &settings_store,\n            settings_backend};")
    file(READ "${_host}" _host_second_coordinator_text)
    set(_host_second_coordinator_replacement
        "${_host_coordinator_anchor}\n        HostSettingsRuntime task8b_shadow{nullptr, &pause_menu,\n            &input_settings, &input_settings, &settings_store,\n            settings_backend};\n        static_cast<void>(task8b_shadow);")
    string(REPLACE "${_host_coordinator_anchor}"
        "${_host_second_coordinator_replacement}"
        _host_second_coordinator_text "${_host_second_coordinator_text}")
    file(READ "${_host}" _host_second_coordinator_original)
    if(_host_second_coordinator_text STREQUAL _host_second_coordinator_original)
        message(FATAL_ERROR
            "Task8B second Host coordinator mutation anchor disappeared")
    endif()
    set(_path "${GUARD_TEST_ROOT}/host-second-settings-coordinator.cpp")
    file(WRITE "${_path}" "${_host_second_coordinator_text}")
    expect_rejected("host second settings coordinator" HOST "${_path}"
        "requires exactly one Host settings coordinator")

    set(_host_settings_anchor
        "            settings_runtime.consume_notice(pause_screen_before);")
    file(READ "${_host}" _host_helper_text)
    set(_host_helper_original "${_host_helper_text}")
    string(REPLACE "${_host_settings_anchor}"
        "            const auto task8b_host_helper =\n                [](const settings::SettingsData&) noexcept {};\n            task8b_host_helper(live_settings);\n${_host_settings_anchor}"
        _host_helper_text "${_host_helper_text}")
    if(_host_helper_text STREQUAL _host_helper_original)
        message(FATAL_ERROR "Task8B Host helper mutation anchor disappeared")
    endif()
    set(_path "${GUARD_TEST_ROOT}/host-live-settings-helper.cpp")
    file(WRITE "${_path}" "${_host_helper_text}")
    expect_rejected("host live-settings helper call" HOST "${_path}"
        "rejected Host live-settings alias/helper drift")

    file(READ "${_host}" _runtime_helper_text)
    set(_runtime_helper_original "${_runtime_helper_text}")
    string(REPLACE "${_host_settings_anchor}"
        "            const auto task8b_runtime_helper =\n                [](const HostSettingsRuntime&) noexcept {};\n            task8b_runtime_helper(settings_runtime);\n${_host_settings_anchor}"
        _runtime_helper_text "${_runtime_helper_text}")
    if(_runtime_helper_text STREQUAL _runtime_helper_original)
        message(FATAL_ERROR "Task8B runtime-helper mutation anchor disappeared")
    endif()
    set(_path "${GUARD_TEST_ROOT}/host-settings-runtime-helper.cpp")
    file(WRITE "${_path}" "${_runtime_helper_text}")
    expect_rejected("host settings-runtime helper call" HOST "${_path}"
        "rejected Host settings-runtime helper drift")
endfunction()

if(DEFINED TASK8B_TARGETED_ONLY AND TASK8B_TARGETED_ONLY)
    task8b_run_owner_cases()
    message(STATUS
        "Stage11D Task8B targeted owner self-test passed: bad_mutations=17; harmless_decoys=1")
    return()
endif()

function(task7c_run_m24_m25_cases)
    file(READ "${_host}" _task7c_host_text)
    string(REPLACE "\r\n" "\n" _task7c_host_text
        "${_task7c_host_text}")
    file(READ "${_host_validation_runtime}" _task7c_runtime_text)
    set(_task7c_decision_observed [=[            const PresentationDecision decision =
                validation_runtime->observe_presented_frame(
                    current, pause_menu, pause_cjk_ready);]=])

    set(_task7c_stage11d_summary [=[    host_validation::write_stage11d_loot_validation_summary(
        *impl_->config, impl_->states.stage11d, pause_menu);]=])
    set(_task7c_conditional_runtime_summary [=[    impl_->states.stage11d.target_visible
        ? static_cast<void>(0)
        : host_validation::write_stage11d_loot_validation_summary(
            *impl_->config, impl_->states.stage11d, pause_menu);]=])
    string(REPLACE "${_task7c_stage11d_summary}"
        "${_task7c_conditional_runtime_summary}"
        _task7c_m25_conditional_runtime_summary "${_task7c_runtime_text}")
    if(_task7c_m25_conditional_runtime_summary STREQUAL _task7c_runtime_text)
        message(FATAL_ERROR
            "Task7C M25 conditional Runtime summary mutation site disappeared")
    endif()
    set(_path "${GUARD_TEST_ROOT}/task7c-m25-conditional-runtime-summary.cpp")
    file(WRITE "${_path}" "${_task7c_m25_conditional_runtime_summary}")
    expect_rejected("task7c M25 conditional Runtime summary"
        HOST_VALIDATION_RUNTIME "${_path}" "T7C-M25")

    set(_task7c_parenthesized_exit [=[            const PresentationDecision decision =
                validation_runtime->observe_presented_frame(
                    current, pause_menu, pause_cjk_ready);
            if (decision.generic_capture_visible) {
                (std::exit)(0);
            }]=])
    string(REPLACE "#include <cstdio>" "#include <cstdio>\n#include <cstdlib>"
        _task7c_m24_parenthesized_exit_host "${_task7c_host_text}")
    string(REPLACE "${_task7c_decision_observed}"
        "${_task7c_parenthesized_exit}"
        _task7c_m24_parenthesized_exit_host
        "${_task7c_m24_parenthesized_exit_host}")
    if(_task7c_m24_parenthesized_exit_host STREQUAL _task7c_host_text)
        message(FATAL_ERROR
            "Task7C M24 parenthesized exit mutation made no change")
    endif()
    set(_path "${GUARD_TEST_ROOT}/task7c-m24-parenthesized-exit.cpp")
    file(WRITE "${_path}" "${_task7c_m24_parenthesized_exit_host}")
    expect_rejected("task7c M24 parenthesized process exit" HOST "${_path}"
        "T7C-M24")

    set(_task7c_host_summary [=[        validation_runtime->write_summaries(
            runtime.clean_shutdown_state(), pause_menu);]=])
    set(_task7c_conditional_host_summary [=[        config.stage11d_loot_validation
                == Stage11DLootValidationScenario::none
            ? validation_runtime->write_summaries(
                runtime.clean_shutdown_state(), pause_menu)
            : static_cast<void>(0);]=])
    string(REPLACE "${_task7c_host_summary}"
        "${_task7c_conditional_host_summary}"
        _task7c_m25_conditional_host_summary "${_task7c_host_text}")
    if(_task7c_m25_conditional_host_summary STREQUAL _task7c_host_text)
        message(FATAL_ERROR
            "Task7C M25 conditional Host summary mutation site disappeared")
    endif()
    set(_path "${GUARD_TEST_ROOT}/task7c-m25-conditional-host-summary.cpp")
    file(WRITE "${_path}" "${_task7c_m25_conditional_host_summary}")
    expect_rejected("task7c M25 conditional Host summary" HOST "${_path}"
        "T7C-M25")

    set(_task7c_exit_declaration "        bool exit_requested = false;")
    set(_task7c_exit_alias_declaration
        "${_task7c_exit_declaration}\n        bool& task7c_exit_flag = exit_requested;")
    string(REPLACE "${_task7c_exit_declaration}"
        "${_task7c_exit_alias_declaration}" _task7c_m24_balanced_alias_host
        "${_task7c_host_text}")
    set(_task7c_ready_exit [=[                if (runtime.clean_shutdown_state()
                        == CleanShutdownState::ready) {
                    exit_requested = true;
                } else {]=])
    set(_task7c_ready_alias_exit [=[                if (runtime.clean_shutdown_state()
                        == CleanShutdownState::ready) {
                    task7c_exit_flag = true;
                } else {]=])
    string(REPLACE "${_task7c_ready_exit}" "${_task7c_ready_alias_exit}"
        _task7c_m24_balanced_alias_host
        "${_task7c_m24_balanced_alias_host}")
    set(_task7c_balanced_alias_bypass
        "${_task7c_decision_observed}\n            if (decision.generic_capture_visible) {\n                task7c_exit_flag = true;\n            }")
    string(REPLACE "${_task7c_decision_observed}"
        "${_task7c_balanced_alias_bypass}" _task7c_m24_balanced_alias_host
        "${_task7c_m24_balanced_alias_host}")
    if(_task7c_m24_balanced_alias_host STREQUAL _task7c_host_text)
        message(FATAL_ERROR
            "Task7C M24 balanced exit alias mutation made no change")
    endif()
    set(_path "${GUARD_TEST_ROOT}/task7c-m24-balanced-exit-alias.cpp")
    file(WRITE "${_path}" "${_task7c_m24_balanced_alias_host}")
    expect_rejected("task7c M24 balanced exit-flag alias" HOST "${_path}"
        "T7C-M24")

    set(_task7c_audio_shutdown "        audio.shutdown();")
    set(_task7c_duplicate_host_summary [=[        audio.shutdown();
        (*validation_runtime).write_summaries(
            runtime.clean_shutdown_state(), pause_menu);]=])
    string(REPLACE "${_task7c_audio_shutdown}"
        "${_task7c_duplicate_host_summary}"
        _task7c_m25_duplicate_host_summary "${_task7c_host_text}")
    if(_task7c_m25_duplicate_host_summary STREQUAL _task7c_host_text)
        message(FATAL_ERROR
            "Task7C M25 duplicate Host summary mutation site disappeared")
    endif()
    set(_path "${GUARD_TEST_ROOT}/task7c-m25-duplicate-host-summary.cpp")
    file(WRITE "${_path}" "${_task7c_m25_duplicate_host_summary}")
    expect_rejected("task7c M25 alternate Host summary invocation" HOST
        "${_path}" "T7C-M25")

    set(_task7c_duplicate_runtime_summary [=[    host_validation::write_stage11d_loot_validation_summary(
        *impl_->config, impl_->states.stage11d, pause_menu);
    (*host_validation::write_stage11d_loot_validation_summary)(
        *impl_->config, impl_->states.stage11d, pause_menu);]=])
    string(REPLACE "${_task7c_stage11d_summary}"
        "${_task7c_duplicate_runtime_summary}"
        _task7c_m25_duplicate_runtime_summary "${_task7c_runtime_text}")
    if(_task7c_m25_duplicate_runtime_summary STREQUAL _task7c_runtime_text)
        message(FATAL_ERROR
            "Task7C M25 duplicate Runtime summary mutation site disappeared")
    endif()
    set(_path "${GUARD_TEST_ROOT}/task7c-m25-duplicate-runtime-summary.cpp")
    file(WRITE "${_path}" "${_task7c_m25_duplicate_runtime_summary}")
    expect_rejected("task7c M25 alternate Runtime summary invocation"
        HOST_VALIDATION_RUNTIME "${_path}" "T7C-M25")

    set(_task7c_validation_exit
        "if (decision.validation_complete\n                    && (!config.validation_capture_file.has_value()\n                        || effective_generic_complete)) {\n                begin_clean_exit();\n            }")
    set(_task7c_exit_flag_alias [=[            bool& task7c_exit_flag = exit_requested;
            const PresentationDecision decision =
                validation_runtime->observe_presented_frame(
                    current, pause_menu, pause_cjk_ready);
            if (decision.generic_capture_visible) {
                task7c_exit_flag = true;
            }]=])
    string(REPLACE "${_task7c_decision_observed}"
        "${_task7c_exit_flag_alias}" _task7c_m24_exit_flag_alias_host
        "${_task7c_host_text}")
    if(_task7c_m24_exit_flag_alias_host STREQUAL _task7c_host_text)
        message(FATAL_ERROR
            "Task7C M24 exit-flag alias mutation site disappeared")
    endif()
    set(_path "${GUARD_TEST_ROOT}/task7c-m24-exit-flag-alias.cpp")
    file(WRITE "${_path}" "${_task7c_m24_exit_flag_alias_host}")
    expect_rejected("task7c M24 exit-flag reference alias" HOST "${_path}"
        "T7C-M24")

    set(_task7c_clean_exit_lambda [=[            const auto task7c_exit_alias = [&]() noexcept {
                begin_clean_exit();
            };
            const PresentationDecision decision =
                validation_runtime->observe_presented_frame(
                    current, pause_menu, pause_cjk_ready);
            if (decision.generic_capture_visible) {
                task7c_exit_alias();
            }]=])
    string(REPLACE "${_task7c_decision_observed}"
        "${_task7c_clean_exit_lambda}" _task7c_m24_clean_exit_lambda_host
        "${_task7c_host_text}")
    if(_task7c_m24_clean_exit_lambda_host STREQUAL _task7c_host_text)
        message(FATAL_ERROR
            "Task7C M24 clean-exit lambda mutation site disappeared")
    endif()
    set(_path "${GUARD_TEST_ROOT}/task7c-m24-clean-exit-lambda.cpp")
    file(WRITE "${_path}" "${_task7c_m24_clean_exit_lambda_host}")
    expect_rejected("task7c M24 clean-exit lambda alias" HOST "${_path}"
        "T7C-M24")

    foreach(_task7c_m24_case IN ITEMS premature-stage11d unrelated-capture)
        if(_task7c_m24_case STREQUAL "premature-stage11d")
            set(_task7c_m24_condition
                "if ((decision.validation_complete\n                        || decision.generic_capture_visible)\n                    && (!config.validation_capture_file.has_value()\n                        || effective_generic_complete)) {\n                begin_clean_exit();\n            }")
            set(_task7c_m24_name
                "task7c M24 premature Stage11D exit")
        else()
            set(_task7c_m24_condition
                "if ((decision.validation_complete\n                        || effective_generic_complete)\n                    && (!config.validation_capture_file.has_value()\n                        || effective_generic_complete)) {\n                begin_clean_exit();\n            }")
            set(_task7c_m24_name
                "task7c M24 unrelated capture-driven exit")
        endif()
        string(REPLACE "${_task7c_validation_exit}"
            "${_task7c_m24_condition}" _task7c_m24_host
            "${_task7c_host_text}")
        if(_task7c_m24_host STREQUAL _task7c_host_text)
            message(FATAL_ERROR
                "Task7C M24 ${_task7c_m24_case} mutation site disappeared")
        endif()
        set(_path
            "${GUARD_TEST_ROOT}/task7c-m24-${_task7c_m24_case}.cpp")
        file(WRITE "${_path}" "${_task7c_m24_host}")
        expect_rejected("${_task7c_m24_name}" HOST "${_path}"
            "T7C-M24")
    endforeach()

    set(_task7c_pre_capture_exit [=[            const PresentationDecision decision =
                validation_runtime->observe_presented_frame(
                    current, pause_menu, pause_cjk_ready);
            if (decision.generic_capture_visible) {
                begin_clean_exit();
            }]=])
    string(REPLACE "${_task7c_decision_observed}"
        "${_task7c_pre_capture_exit}" _task7c_m24_pre_capture_host
        "${_task7c_host_text}")
    if(_task7c_m24_pre_capture_host STREQUAL _task7c_host_text)
        message(FATAL_ERROR
            "Task7C M24 pre-capture exit mutation site disappeared")
    endif()
    set(_path "${GUARD_TEST_ROOT}/task7c-m24-pre-capture-exit.cpp")
    file(WRITE "${_path}" "${_task7c_m24_pre_capture_host}")
    expect_rejected("task7c M24 pre-capture exit" HOST "${_path}"
        "T7C-M24")

    foreach(_task7c_direct_bypass IN ITEMS
            explicit-clean-exit counter-poison exit-requested loop-break)
        if(_task7c_direct_bypass STREQUAL "explicit-clean-exit")
            set(_task7c_direct_bypass_statement [=[            if (decision.generic_capture_visible) {
                begin_clean_exit.operator()();
            }]=])
            set(_task7c_direct_bypass_name
                "task7c M24 explicit clean-exit operator call")
        elseif(_task7c_direct_bypass STREQUAL "counter-poison")
            set(_task7c_direct_bypass_statement [=[            if (decision.generic_capture_visible) {
                presented_frame_count =
                    config.validation_exit_after_presented_frames;
            }]=])
            set(_task7c_direct_bypass_name
                "task7c M24 presented-frame counter poison")
        elseif(_task7c_direct_bypass STREQUAL "exit-requested")
            set(_task7c_direct_bypass_statement [=[            if (decision.generic_capture_visible) {
                exit_requested = true;
            }]=])
            set(_task7c_direct_bypass_name
                "task7c M24 direct exit-requested bypass")
        else()
            set(_task7c_direct_bypass_statement [=[            if (decision.generic_capture_visible) {
                break;
            }]=])
            set(_task7c_direct_bypass_name
                "task7c M24 direct loop-break bypass")
        endif()
        set(_task7c_direct_bypass_replacement
            "${_task7c_decision_observed}\n${_task7c_direct_bypass_statement}")
        string(REPLACE "${_task7c_decision_observed}"
            "${_task7c_direct_bypass_replacement}"
            _task7c_direct_bypass_host "${_task7c_host_text}")
        if(_task7c_direct_bypass_host STREQUAL _task7c_host_text)
            message(FATAL_ERROR
                "Task7C M24 ${_task7c_direct_bypass} mutation site disappeared")
        endif()
        set(_path
            "${GUARD_TEST_ROOT}/task7c-m24-${_task7c_direct_bypass}.cpp")
        file(WRITE "${_path}" "${_task7c_direct_bypass_host}")
        expect_rejected("${_task7c_direct_bypass_name}" HOST "${_path}"
            "T7C-M24")
    endforeach()

    string(REPLACE "const PresentationDecision decision ="
        "const auto decision =" _task7c_m24_cross_function_host
        "${_task7c_host_text}")
    set(_task7c_run_signature "HostExitCode run_raylib_host(")
    set(_task7c_cross_function_decoy [=[void task7c_m24_decision_decoy() {
    const PresentationDecision decision = {};
}

HostExitCode run_raylib_host(]=])
    string(REPLACE "${_task7c_run_signature}"
        "${_task7c_cross_function_decoy}"
        _task7c_m24_cross_function_host
        "${_task7c_m24_cross_function_host}")
    if(_task7c_m24_cross_function_host STREQUAL _task7c_host_text)
        message(FATAL_ERROR
            "Task7C M24 cross-function decision decoy mutation made no change")
    endif()
    set(_path "${GUARD_TEST_ROOT}/task7c-m24-cross-function-decoy.cpp")
    file(WRITE "${_path}" "${_task7c_m24_cross_function_host}")
    expect_rejected("task7c M24 cross-function decision decoy" HOST "${_path}"
        "T7C-M24")

    set(_task7c_predecision_alias [=[            auto* task7c_frame_counter = &presented_frame_count;
            const PresentationDecision decision =
                validation_runtime->observe_presented_frame(
                    current, pause_menu, pause_cjk_ready);
            if (decision.generic_capture_visible) {
                *task7c_frame_counter =
                    config.validation_exit_after_presented_frames;
            }]=])
    string(REPLACE "${_task7c_decision_observed}"
        "${_task7c_predecision_alias}" _task7c_m24_alias_host
        "${_task7c_host_text}")
    if(_task7c_m24_alias_host STREQUAL _task7c_host_text)
        message(FATAL_ERROR
            "Task7C M24 pre-decision counter alias mutation site disappeared")
    endif()
    set(_path "${GUARD_TEST_ROOT}/task7c-m24-predecision-counter-alias.cpp")
    file(WRITE "${_path}" "${_task7c_m24_alias_host}")
    expect_rejected("task7c M24 pre-decision counter alias" HOST "${_path}"
        "T7C-M24")

    set(_task7c_frame_exit [=[            if (config.validation_exit_after_presented_frames != 0U
                    && presented_frame_count
                        >= config.validation_exit_after_presented_frames) {
                begin_clean_exit();
            }]=])
    set(_task7c_capture_driven_frame_exit [=[            if (decision.generic_capture_visible
                    || (config.validation_exit_after_presented_frames != 0U
                        && presented_frame_count
                            >= config.validation_exit_after_presented_frames)) {
                begin_clean_exit();
            }]=])
    string(REPLACE "${_task7c_frame_exit}"
        "${_task7c_capture_driven_frame_exit}"
        _task7c_m24_frame_exit_host "${_task7c_host_text}")
    if(_task7c_m24_frame_exit_host STREQUAL _task7c_host_text)
        message(FATAL_ERROR
            "Task7C M24 capture-driven frame exit mutation site disappeared")
    endif()
    set(_path "${GUARD_TEST_ROOT}/task7c-m24-capture-driven-frame-exit.cpp")
    file(WRITE "${_path}" "${_task7c_m24_frame_exit_host}")
    expect_rejected("task7c M24 capture-driven frame exit" HOST "${_path}"
        "T7C-M24")

    set(_task7c_counter_increment "            ++presented_frame_count;")
    set(_task7c_double_increment [=[            for (unsigned task7c_repeat = 0U; task7c_repeat < 2U;
                    ++task7c_repeat) {
                ++presented_frame_count;
            }]=])
    string(REPLACE "${_task7c_counter_increment}"
        "${_task7c_double_increment}" _task7c_m24_double_increment_host
        "${_task7c_host_text}")
    if(_task7c_m24_double_increment_host STREQUAL _task7c_host_text)
        message(FATAL_ERROR
            "Task7C M24 controlled double-increment mutation site disappeared")
    endif()
    set(_path "${GUARD_TEST_ROOT}/task7c-m24-double-increment.cpp")
    file(WRITE "${_path}" "${_task7c_m24_double_increment_host}")
    expect_rejected("task7c M24 controlled double increment" HOST "${_path}"
        "T7C-M24")

    set(_task7c_macro_exit
        "            #define validation_complete generic_capture_visible\n${_task7c_validation_exit}\n            #undef validation_complete")
    string(REPLACE "${_task7c_validation_exit}" "${_task7c_macro_exit}"
        _task7c_m24_macro_host "${_task7c_host_text}")
    if(_task7c_m24_macro_host STREQUAL _task7c_host_text)
        message(FATAL_ERROR
            "Task7C M24 validation-complete macro mutation site disappeared")
    endif()
    set(_path "${GUARD_TEST_ROOT}/task7c-m24-validation-macro.cpp")
    file(WRITE "${_path}" "${_task7c_m24_macro_host}")
    expect_rejected("task7c M24 validation-complete macro rewrite" HOST
        "${_path}" "T7C-M24")

    set(_task7c_generic_complete_assignment
        "    decision.generic_capture_complete = impl_->generic_capture_complete;")
    set(_task7c_validation_overwrite
        "    decision.validation_complete = true;\n${_task7c_generic_complete_assignment}")
    string(REPLACE "${_task7c_generic_complete_assignment}"
        "${_task7c_validation_overwrite}"
        _task7c_m24_runtime_overwrite "${_task7c_runtime_text}")
    if(_task7c_m24_runtime_overwrite STREQUAL _task7c_runtime_text)
        message(FATAL_ERROR
            "Task7C M24 facade overwrite mutation site disappeared")
    endif()
    set(_path "${GUARD_TEST_ROOT}/task7c-m24-facade-overwrite.cpp")
    file(WRITE "${_path}" "${_task7c_m24_runtime_overwrite}")
    expect_rejected("task7c M24 facade validation-complete overwrite"
        HOST_VALIDATION_RUNTIME "${_path}" "T7C-M24")

    string(REPLACE
        "host_validation::write_stage11d_loot_validation_summary("
        "host_validation::write_stage11d_loot_validation_summary_removed("
        _task7c_m25_runtime "${_task7c_runtime_text}")
    if(_task7c_m25_runtime STREQUAL _task7c_runtime_text)
        message(FATAL_ERROR "Task7C M25 mutation site disappeared")
    endif()
    set(_path "${GUARD_TEST_ROOT}/task7c-m25-omit-stage11d-summary.cpp")
    file(WRITE "${_path}" "${_task7c_m25_runtime}")
    expect_rejected("task7c M25 omit Stage11D summary"
        HOST_VALIDATION_RUNTIME "${_path}" "T7C-M25")
endfunction()

if(DEFINED TASK7C_M24_M25_TARGETED_ONLY
        AND TASK7C_M24_M25_TARGETED_ONLY)
    task7c_run_m24_m25_cases()
    message(STATUS
        "Stage11D Task7C M24/M25 targeted guard self-test passed: cases=22")
    return()
endif()

file(READ "${_report}" _task5b_report_text)
string(REPLACE "\r\n" "\n" _task5b_report_text
    "${_task5b_report_text}")

string(REPLACE "void stage11d_record_semantics("
    "// void stage11d_record_semantics(\nvoid task5b_record_removed("
    _task5b_report_record_comment "${_task5b_report_text}")
if(_task5b_report_record_comment STREQUAL _task5b_report_text)
    message(FATAL_ERROR "Task5B report recorder comment mutation made no change")
endif()
set(_path "${GUARD_TEST_ROOT}/report-record-comment-decoy.cpp")
file(WRITE "${_path}" "${_task5b_report_record_comment}")
expect_rejected("report semantic recorder comment decoy" REPORT "${_path}"
    "report semantic recorder definition token inventory")

string(REPLACE "bool stage11d_target_visible("
    "const char* task5b_target_string = \"bool stage11d_target_visible(\";\nbool task5b_target_removed("
    _task5b_report_target_string "${_task5b_report_text}")
if(_task5b_report_target_string STREQUAL _task5b_report_text)
    message(FATAL_ERROR "Task5B report target string mutation made no change")
endif()
set(_path "${GUARD_TEST_ROOT}/report-target-string-decoy.cpp")
file(WRITE "${_path}" "${_task5b_report_target_string}")
expect_rejected("report target evaluator string decoy" REPORT "${_path}"
    "report target-visible evaluator definition token inventory")

string(REPLACE "void write_stage11d_loot_validation_summary("
    "void write_stage11d_loot_validation_summary();\nvoid task5b_summary_removed("
    _task5b_report_summary_forward "${_task5b_report_text}")
if(_task5b_report_summary_forward STREQUAL _task5b_report_text)
    message(FATAL_ERROR "Task5B report summary forward mutation made no change")
endif()
set(_path "${GUARD_TEST_ROOT}/report-summary-forward-decoy.cpp")
file(WRITE "${_path}" "${_task5b_report_summary_forward}")
expect_rejected("report summary forward declaration decoy" REPORT "${_path}"
    "missing report summary writer definition")

set(_task5b_damage_payload
    "                state.monster_damage_observed = true;")
string(REPLACE "${_task5b_damage_payload}"
    "                state.monster_damage_observed = false;"
    _task5b_report_payload_removed "${_task5b_report_text}")
if(_task5b_report_payload_removed STREQUAL _task5b_report_text)
    message(FATAL_ERROR "Task5B report payload mutation made no change")
endif()
set(_task5b_report_close "}  // namespace arpg::platform::host_validation")
set(_task5b_cross_function
    "void task5b_report_payload_decoy() {\n    ${_task5b_damage_payload}\n}\n\n${_task5b_report_close}")
string(REPLACE "${_task5b_report_close}" "${_task5b_cross_function}"
    _task5b_report_payload_cross_function "${_task5b_report_payload_removed}")
set(_path "${GUARD_TEST_ROOT}/report-payload-cross-function-decoy.cpp")
file(WRITE "${_path}" "${_task5b_report_payload_cross_function}")
expect_rejected("report semantic payload cross function decoy" REPORT "${_path}"
    "missing report evaluator semantic: state.monster_damage_observed")

if(DEFINED TASK5B_TARGETED_ONLY AND TASK5B_TARGETED_ONLY)
    message(STATUS
        "Stage11D Task5B targeted evidence guard passed: bad_mutations=4")
    return()
endif()

function(expect_guard_accepted NAME GUARD OVERRIDE PATH)
    if((DEFINED STAGE11D_ABYSS_PHYSICAL_TARGETED_ONLY
            AND STAGE11D_ABYSS_PHYSICAL_TARGETED_ONLY) OR
        (DEFINED STAGE11D_FORMAL_LIMIT_TARGETED_ONLY
            AND STAGE11D_FORMAL_LIMIT_TARGETED_ONLY) OR
        (DEFINED ROUND3_TARGETED_ONLY AND ROUND3_TARGETED_ONLY) OR
        (DEFINED TASK9_TARGETED_ONLY AND TASK9_TARGETED_ONLY
            AND NOT NAME STREQUAL "Stage11D renderer harmless definition decoy")
    )
        return()
    endif()
    execute_process(COMMAND "${CMAKE_COMMAND}" "-DSOURCE_ROOT=${SOURCE_ROOT}"
        "-D${OVERRIDE}_OVERRIDE=${PATH}" -P "${GUARD}"
        RESULT_VARIABLE _result OUTPUT_VARIABLE _output ERROR_VARIABLE _error)
    if(NOT _result EQUAL 0)
        message(FATAL_ERROR
            "${NAME} was rejected: ${_output}\n${_error}")
    endif()
endfunction()

function(expect_sequence_rejected NAME PATH EXPECTED_REASON)
    if((DEFINED STAGE11D_ABYSS_PHYSICAL_TARGETED_ONLY
            AND STAGE11D_ABYSS_PHYSICAL_TARGETED_ONLY) OR
        (DEFINED STAGE11D_FORMAL_LIMIT_TARGETED_ONLY
            AND STAGE11D_FORMAL_LIMIT_TARGETED_ONLY) OR
        (DEFINED ROUND3_TARGETED_ONLY AND ROUND3_TARGETED_ONLY) OR
        (DEFINED TASK9_TARGETED_ONLY AND TASK9_TARGETED_ONLY))
        return()
    endif()
    execute_process(COMMAND "${CMAKE_COMMAND}" "-DSOURCE_ROOT=${SOURCE_ROOT}"
        "-DHOST_OVERRIDE=${PATH}" -P "${_sequence_guard}"
        RESULT_VARIABLE _result OUTPUT_VARIABLE _output ERROR_VARIABLE _error)
    if(_result EQUAL 0)
        message(FATAL_ERROR "host sequence guard accepted mutation: ${NAME}")
    endif()
    set(_log "${_output}\n${_error}")
    string(REGEX REPLACE "[ \t\r\n]+" " " _normalized_log "${_log}")
    string(REGEX REPLACE "[ \t\r\n]+" " " _normalized_reason
        "${EXPECTED_REASON}")
    string(FIND "${_normalized_log}" "${_normalized_reason}" _reason)
    if(_reason EQUAL -1)
        message(FATAL_ERROR
            "host sequence guard rejected ${NAME} for the wrong reason: ${_log}")
    endif()
endfunction()
endif()

# Task 5A relocation RED: before the runtime exists, mutate the exact source
# region that will move. The legacy guard ignores RUNTIME_OVERRIDE, which is
# itself part of the RED proof. After extraction these same named mutations
# exercise the real runtime source.
if(EXISTS "${_runtime}")
    file(READ "${_runtime}" _task5a_runtime_text)
else()
    file(READ "${_host}" _task5a_runtime_text)
endif()
string(REPLACE "\r\n" "\n" _task5a_runtime_text
    "${_task5a_runtime_text}")

if(NOT (DEFINED STAGE11D_ABYSS_PHYSICAL_TARGETED_ONLY
        AND STAGE11D_ABYSS_PHYSICAL_TARGETED_ONLY))
file(READ "${_formal}" _stage11d_formal_limit_text)
string(REPLACE "\r\n" "\n" _stage11d_formal_limit_text
    "${_stage11d_formal_limit_text}")
set(_stage11d_formal_scenario
    "    config.stage11d_loot_validation = spec.scenario;")
string(REPLACE "${_stage11d_formal_scenario}"
    "    if (false)\n        config.stage11d_loot_validation = spec.scenario;"
    _stage11d_formal_scenario_unbraced_dead
    "${_stage11d_formal_limit_text}")
if(_stage11d_formal_scenario_unbraced_dead STREQUAL
        _stage11d_formal_limit_text)
    message(FATAL_ERROR
        "Stage11D formal unbraced scenario mutation made no change")
endif()
set(_path "${GUARD_TEST_ROOT}/stage11d-formal-scenario-unbraced-dead.cpp")
file(WRITE "${_path}" "${_stage11d_formal_scenario_unbraced_dead}")
expect_rejected(
    "Stage11D formal scenario cannot move under an unbraced dead branch"
    FORMAL "${_path}"
    "requires direct run_host scenario assignment")

set(_stage11d_formal_step
    "    config.validation_steps_per_frame = 1U;")
string(REPLACE "${_stage11d_formal_step}"
    "    // config.validation_steps_per_frame = 1U;"
    _stage11d_formal_step_comment "${_stage11d_formal_limit_text}")
if(_stage11d_formal_step_comment STREQUAL _stage11d_formal_limit_text)
    message(FATAL_ERROR "Stage11D formal step comment mutation made no change")
endif()
set(_path "${GUARD_TEST_ROOT}/stage11d-formal-step-comment.cpp")
file(WRITE "${_path}" "${_stage11d_formal_step_comment}")
expect_rejected("Stage11D formal fixed step cannot move into a comment"
    FORMAL "${_path}"
    "requires one direct 1-tick run_host assignment")

string(REPLACE "${_stage11d_formal_step}"
    "    constexpr const char* step_decoy = \"config.validation_steps_per_frame = 1U;\";"
    _stage11d_formal_step_string "${_stage11d_formal_limit_text}")
if(_stage11d_formal_step_string STREQUAL _stage11d_formal_limit_text)
    message(FATAL_ERROR "Stage11D formal step string mutation made no change")
endif()
set(_path "${GUARD_TEST_ROOT}/stage11d-formal-step-string.cpp")
file(WRITE "${_path}" "${_stage11d_formal_step_string}")
expect_rejected("Stage11D formal fixed step cannot move into a string"
    FORMAL "${_path}"
    "requires one direct 1-tick run_host assignment")

string(REPLACE "${_stage11d_formal_step}"
    "    if (false) { config.validation_steps_per_frame = 1U; }"
    _stage11d_formal_step_dead "${_stage11d_formal_limit_text}")
if(_stage11d_formal_step_dead STREQUAL _stage11d_formal_limit_text)
    message(FATAL_ERROR "Stage11D formal step dead-branch mutation made no change")
endif()
set(_path "${GUARD_TEST_ROOT}/stage11d-formal-step-dead.cpp")
file(WRITE "${_path}" "${_stage11d_formal_step_dead}")
expect_rejected("Stage11D formal fixed step cannot move into a dead branch"
    FORMAL "${_path}"
    "requires direct run_host fixed-step assignment")

set(_stage11d_abyss_budget_call [=[    const auto budget = stage11d_rare_abyss_presented_frame_budget(
        plan.monster_count);]=])
string(REPLACE "${_stage11d_abyss_budget_call}"
    [=[    std::optional<std::uint32_t> budget{};
    if (false) {
        budget = stage11d_rare_abyss_presented_frame_budget(
            plan.monster_count);
    }]=]
    _stage11d_abyss_budget_dead "${_stage11d_formal_limit_text}")
if(_stage11d_abyss_budget_dead STREQUAL _stage11d_formal_limit_text)
    message(FATAL_ERROR "Stage11D abyss budget-call mutation made no change")
endif()
set(_path "${GUARD_TEST_ROOT}/stage11d-formal-abyss-budget-dead.cpp")
file(WRITE "${_path}" "${_stage11d_abyss_budget_dead}")
expect_rejected("Stage11D abyss budget call cannot move into a dead branch"
    FORMAL "${_path}"
    "requires the production monster count to derive the rare-abyss frame budget")

string(REPLACE "${_stage11d_formal_step}"
    "    if (false)\n        config.validation_steps_per_frame = 1U;"
    _stage11d_formal_step_unbraced_dead "${_stage11d_formal_limit_text}")
if(_stage11d_formal_step_unbraced_dead STREQUAL _stage11d_formal_limit_text)
    message(FATAL_ERROR
        "Stage11D formal unbraced step mutation made no change")
endif()
set(_path "${GUARD_TEST_ROOT}/stage11d-formal-step-unbraced-dead.cpp")
file(WRITE "${_path}" "${_stage11d_formal_step_unbraced_dead}")
expect_rejected(
    "Stage11D formal fixed step cannot move under an unbraced dead branch"
    FORMAL "${_path}"
    "requires direct run_host fixed-step assignment")

set(_stage11d_formal_limit_assignment [=[    config.validation_exit_after_presented_frames = spec.abyss
        ? selected.abyss_presented_frame_budget
        : kStage11DDefaultPresentedFrameLimit;]=])
string(REPLACE "${_stage11d_formal_limit_assignment}"
    "    if (false)\n        ${_stage11d_formal_limit_assignment}"
    _stage11d_formal_limit_unbraced_dead "${_stage11d_formal_limit_text}")
if(_stage11d_formal_limit_unbraced_dead STREQUAL _stage11d_formal_limit_text)
    message(FATAL_ERROR
        "Stage11D formal unbraced frame-limit mutation made no change")
endif()
set(_path "${GUARD_TEST_ROOT}/stage11d-formal-limit-unbraced-dead.cpp")
file(WRITE "${_path}" "${_stage11d_formal_limit_unbraced_dead}")
expect_rejected(
    "Stage11D formal frame limit cannot move under an unbraced dead branch"
    FORMAL "${_path}"
    "requires direct run_host frame limit")

set(_stage11d_survival_fixture [=[                        && arpg::test::install_stage10_validation_survival_passives(
                            validation_state)]=])
string(REPLACE "${_stage11d_survival_fixture}" "                        && true"
    _stage11d_survival_fixture_omitted "${_stage11d_formal_limit_text}")
if(_stage11d_survival_fixture_omitted STREQUAL _stage11d_formal_limit_text)
    message(FATAL_ERROR
        "Stage11D survival-passive fixture mutation made no change")
endif()
set(_path "${GUARD_TEST_ROOT}/stage11d-formal-survival-fixture-omitted.cpp")
file(WRITE "${_path}" "${_stage11d_survival_fixture_omitted}")
expect_rejected(
    "Stage11D abyss validation survival passives cannot be omitted"
    FORMAL "${_path}"
    "requires the validation build")

set(_stage11d_live_damage_fixture [=[                        && prepare_stage11d_live_damage_build(
                            validation_state)]=])
string(REPLACE "${_stage11d_live_damage_fixture}" "                        && true"
    _stage11d_live_damage_fixture_omitted "${_stage11d_formal_limit_text}")
if(_stage11d_live_damage_fixture_omitted STREQUAL
        _stage11d_formal_limit_text)
    message(FATAL_ERROR
        "Stage11D live-damage fixture mutation made no change")
endif()
set(_path "${GUARD_TEST_ROOT}/stage11d-formal-live-damage-fixture-omitted.cpp")
file(WRITE "${_path}" "${_stage11d_live_damage_fixture_omitted}")
expect_rejected(
    "Stage11D live-damage fixture cannot be omitted"
    FORMAL "${_path}"
    "requires the validation build")

string(REPLACE "            if (affix.affix_id == kBarrierAffix) {"
    "            if (affix.affix_id != kBarrierAffix) {"
    _stage11d_live_damage_barrier_reversed
    "${_stage11d_formal_limit_text}")
if(_stage11d_live_damage_barrier_reversed STREQUAL
        _stage11d_formal_limit_text)
    message(FATAL_ERROR
        "Stage11D live-damage barrier mutation made no change")
endif()
set(_path "${GUARD_TEST_ROOT}/stage11d-formal-live-damage-barrier-reversed.cpp")
file(WRITE "${_path}" "${_stage11d_live_damage_barrier_reversed}")
expect_rejected(
    "Stage11D live-damage helper cannot weaken barrier-only removal"
    FORMAL "${_path}"
    "requires an exact live-damage validation build")

string(REPLACE [=["abyss_presented_frame_budget="]=]
    [=[/* "abyss_presented_frame_budget=" */ "abyss_budget_removed="]=]
    _stage11d_manifest_budget_comment "${_stage11d_formal_limit_text}")
if(_stage11d_manifest_budget_comment STREQUAL _stage11d_formal_limit_text)
    message(FATAL_ERROR
        "Stage11D manifest budget-comment mutation made no change")
endif()
set(_path "${GUARD_TEST_ROOT}/stage11d-formal-manifest-budget-comment.cpp")
file(WRITE "${_path}" "${_stage11d_manifest_budget_comment}")
expect_rejected(
    "Stage11D abyss frame-budget manifest cannot move into a comment"
    FORMAL "${_path}"
    "requires active main-scope manifest bindings")

file(READ "${_validator}" _stage11d_validator_binding_text)
string(REPLACE "\r\n" "\n" _stage11d_validator_binding_text
    "${_stage11d_validator_binding_text}")
set(_stage11d_validator_vsync_block [=[Require ($manifest.ContainsKey('formal_vsync_enabled') -and
    $manifest.formal_vsync_enabled -eq '0') `
    'formal validation must disable VSync in its production settings slot']=])
string(REPLACE "${_stage11d_validator_vsync_block}"
    "function Invoke-Stage11DDisabledVsyncDecoy {\n${_stage11d_validator_vsync_block}\n}"
    _stage11d_validator_vsync_function "${_stage11d_validator_binding_text}")
if(_stage11d_validator_vsync_function STREQUAL _stage11d_validator_binding_text)
    message(FATAL_ERROR
        "Stage11D validator VSync function mutation made no change")
endif()
set(_path "${GUARD_TEST_ROOT}/stage11d-validator-vsync-function.ps1")
file(WRITE "${_path}" "${_stage11d_validator_vsync_function}")
expect_rejected(
    "Stage11D validator VSync check cannot move into an uncalled function"
    VALIDATOR "${_path}"
    "requires a real top-level disabled-VSync check")
if(DEFINED STAGE11D_FORMAL_LIMIT_TARGETED_ONLY
        AND STAGE11D_FORMAL_LIMIT_TARGETED_ONLY)
    get_property(_executed GLOBAL PROPERTY
        STAGE11D_FORMAL_LIMIT_EXECUTED_MUTATIONS)
    if(NOT _executed EQUAL 12)
        message(FATAL_ERROR
            "Stage11D formal limit targeted guard executed ${_executed} mutations, expected 12")
    endif()
    message(STATUS
        "Stage11D formal limit guard passed: bad_mutations=${_executed}")
    return()
endif()
endif()

# Rare-abyss physical-input recovery contract. These mutations stay small so
# the failed formal scenario can be guarded without running the long renderer
# mutation suite.
file(READ "${_host_validation_runtime}" _stage11d_abyss_facade_text)
string(REPLACE "\r\n" "\n" _stage11d_abyss_facade_text
    "${_stage11d_abyss_facade_text}")
set(_stage11d_rearm_binding
    "    impl_->states.stage11d.suspend_injection = gameplay_rearm_required;\n")
string(REPLACE "${_stage11d_rearm_binding}" ""
    _stage11d_rearm_bypass "${_stage11d_abyss_facade_text}")
if(_stage11d_rearm_bypass STREQUAL _stage11d_abyss_facade_text)
    message(FATAL_ERROR "Stage11D rearm mutation made no change")
endif()
set(_path "${GUARD_TEST_ROOT}/stage11d-rearm-bypass.cpp")
file(WRITE "${_path}" "${_stage11d_rearm_bypass}")
expect_rejected("Stage11D rearm cannot bypass physical suspension"
    HOST_VALIDATION_RUNTIME "${_path}"
    "rejected host input call binding")

string(REPLACE " || state.suspend_injection" ""
    _stage11d_suspend_bypass "${_task5a_runtime_text}")
if(_stage11d_suspend_bypass STREQUAL _task5a_runtime_text)
    message(FATAL_ERROR "Stage11D suspend mutation made no change")
endif()
set(_path "${GUARD_TEST_ROOT}/stage11d-suspend-bypass.cpp")
file(WRITE "${_path}" "${_stage11d_suspend_bypass}")
expect_rejected("Stage11D driver must honor physical suspension"
    RUNTIME "${_path}"
    "rejected runtime physical suspension gate token inventory")

set(_stage11d_sweep_chain [=[            combat::MovementInput movement =
                stage10_validation_sweep_movement(
                    combat_state, state.sweep_grid,
                    state.sweep_waypoint);
            if ((movement.x != 0 || movement.y != 0)
                    && current.ecology
                        == dungeon::checkpoint::DungeonElement::fire) {
                movement = validation_route_fire_movement(
                    combat_state.player.position,
                    stage10_validation_sweep_waypoint(
                        state.sweep_waypoint), movement);
            }
            inject_validation_movement(snapshot, settings_data, movement);]=])
string(REPLACE "${_stage11d_sweep_chain}"
    [=[            combat::MovementInput movement =
                stage10_validation_sweep_movement(
                    combat_state, state.sweep_grid,
                    state.sweep_waypoint);
            if ((movement.x != 0 || movement.y != 0)
                    && current.ecology
                        == dungeon::checkpoint::DungeonElement::fire) {
                movement = validation_route_fire_movement(
                    combat_state.player.position,
                    stage10_validation_sweep_waypoint(
                        state.sweep_waypoint), movement);
            }
            static_cast<void>(movement);]=]
    _stage11d_sweep_bypass "${_task5a_runtime_text}")
if(_stage11d_sweep_bypass STREQUAL _task5a_runtime_text)
    message(FATAL_ERROR "Stage11D physical sweep mutation made no change")
endif()
set(_path "${GUARD_TEST_ROOT}/stage11d-sweep-bypass.cpp")
file(WRITE "${_path}" "${_stage11d_sweep_bypass}")
expect_rejected("Stage11D streaming sweep must map through physical movement"
    RUNTIME "${_path}"
    "rejected physical-input driver chain")

set(_stage11d_neutral_chain [=[        if (!stage11d_rare_abyss_player_available(combat_state)) {
            return snapshot;
        }
]=])
string(REPLACE "${_stage11d_neutral_chain}" ""
    _stage11d_neutral_bypass "${_task5a_runtime_text}")
if(_stage11d_neutral_bypass STREQUAL _task5a_runtime_text)
    message(FATAL_ERROR "Stage11D unavailable-player mutation made no change")
endif()
set(_path "${GUARD_TEST_ROOT}/stage11d-neutral-bypass.cpp")
file(WRITE "${_path}" "${_stage11d_neutral_bypass}")
expect_rejected("Stage11D unavailable player must stay physically neutral"
    RUNTIME "${_path}"
    "rejected runtime rare abyss availability gate token inventory")

set(_stage11d_ranged_return [=[        if (movement.x != 0 || movement.y != 0
                || !plan.stance_reached || !plan.facing_target) {
            if (state.abyss_ranged.close_for_light
                    && (movement.x != 0 || movement.y != 0)
                    && inject_stage11d_rare_abyss_area_skill(
                        snapshot, current)
                        == Stage11DRareAbyssSkillInput::injected) {
                return snapshot;
            }
            return snapshot;
        }
]=])
string(REPLACE "${_stage11d_ranged_return}" ""
    _stage11d_turn_bypass "${_task5a_runtime_text}")
if(_stage11d_turn_bypass STREQUAL _task5a_runtime_text)
    message(FATAL_ERROR "Stage11D ranged-return mutation made no change")
endif()
set(_path "${GUARD_TEST_ROOT}/stage11d-ranged-return-bypass.cpp")
file(WRITE "${_path}" "${_stage11d_turn_bypass}")
expect_rejected("Stage11D ranged movement must return before attacking"
    RUNTIME "${_path}"
    "rejected physical-input movement return owner")

set(_stage11d_equipped_slot_chain [=[            if (dungeon_state.skill_loadout.slots[slot].active != skill) {
                continue;
            }
]=])
string(REPLACE "${_stage11d_equipped_slot_chain}" ""
    _stage11d_skill_slot_bypass "${_task5a_runtime_text}")
if(_stage11d_skill_slot_bypass STREQUAL _task5a_runtime_text)
    message(FATAL_ERROR "Stage11D equipped skill-slot mutation made no change")
endif()
set(_path "${GUARD_TEST_ROOT}/stage11d-skill-slot-bypass.cpp")
file(WRITE "${_path}" "${_stage11d_skill_slot_bypass}")
expect_rejected("Stage11D area skill must use the equipped physical slot"
    RUNTIME "${_path}"
    "missing runtime rare abyss skill token: dungeon_state.skill_loadout.slots[slot].active != skill")

string(REPLACE "            state.abyss_ranged.sweep_escape = true;"
    "            state.abyss_ranged.sweep_escape = false;"
    _stage11d_danger_replan_mutation "${_task5a_runtime_text}")
if(_stage11d_danger_replan_mutation STREQUAL _task5a_runtime_text)
    message(FATAL_ERROR "Stage11D danger-replan mutation made no change")
endif()
set(_path "${GUARD_TEST_ROOT}/stage11d-danger-replan-bypass.cpp")
file(WRITE "${_path}" "${_stage11d_danger_replan_mutation}")
expect_rejected("Stage11D nearby danger must replan the ranged stance"
    RUNTIME "${_path}"
    "rejected physical-input driver chain")

string(REPLACE " || state.suspend_injection)"
    " || (state.suspend_injection && false))"
    _stage11d_suspend_weakened "${_task5a_runtime_text}")
if(_stage11d_suspend_weakened STREQUAL _task5a_runtime_text)
    message(FATAL_ERROR "Stage11D suspension weakening mutation made no change")
endif()
set(_path "${GUARD_TEST_ROOT}/stage11d-suspend-weakened.cpp")
file(WRITE "${_path}" "${_stage11d_suspend_weakened}")
expect_rejected("Stage11D suspension gate cannot be weakened"
    RUNTIME "${_path}"
    "rejected physical-input driver chain")

string(REPLACE "${_stage11d_neutral_chain}"
    "        if (false) {\n${_stage11d_neutral_chain}        }\n"
    _stage11d_neutral_dead_branch "${_task5a_runtime_text}")
if(_stage11d_neutral_dead_branch STREQUAL _task5a_runtime_text)
    message(FATAL_ERROR
        "Stage11D availability dead-branch mutation made no change")
endif()
set(_path "${GUARD_TEST_ROOT}/stage11d-neutral-dead-branch.cpp")
file(WRITE "${_path}" "${_stage11d_neutral_dead_branch}")
expect_rejected(
    "Stage11D availability gate cannot move into a dead branch"
    RUNTIME "${_path}"
    "rejected runtime rare abyss availability gate scope")

set(_stage11d_slot_helper_close [=[        return Stage11DEquippedSkillInput::absent;
    };
    bool area_skill_equipped = false;]=])
string(REPLACE "${_stage11d_slot_helper_close}"
    [=[        return Stage11DEquippedSkillInput::absent;
    };
    const std::size_t extra_slot = 0U;
    snapshot.active_skill_slots[extra_slot] = true;
    bool area_skill_equipped = false;]=]
    _stage11d_second_slot_write "${_task5a_runtime_text}")
if(_stage11d_second_slot_write STREQUAL _task5a_runtime_text)
    message(FATAL_ERROR "Stage11D second skill-slot mutation made no change")
endif()
set(_path "${GUARD_TEST_ROOT}/stage11d-second-skill-slot.cpp")
file(WRITE "${_path}" "${_stage11d_second_slot_write}")
expect_rejected("Stage11D area skill cannot write a second physical slot"
    RUNTIME "${_path}"
    "rejected rare abyss physical skill write inventory")

set(_stage11d_melee_fallback_open
    "        if (skill_input == Stage11DRareAbyssSkillInput::unavailable\n                || skill_input == Stage11DRareAbyssSkillInput::waiting) {\n")
set(_stage11d_melee_fallback_close [=[        }
        if (validation_attack_lane(combat_state, *target)) {]=])
string(REPLACE "${_stage11d_melee_fallback_open}"
    "        if (false) {\n${_stage11d_melee_fallback_open}"
    _stage11d_melee_dead_branch "${_task5a_runtime_text}")
string(REPLACE "${_stage11d_melee_fallback_close}"
    "        }\n${_stage11d_melee_fallback_close}"
    _stage11d_melee_dead_branch "${_stage11d_melee_dead_branch}")
if(_stage11d_melee_dead_branch STREQUAL _task5a_runtime_text)
    message(FATAL_ERROR "Stage11D melee dead-branch mutation made no change")
endif()
set(_path "${GUARD_TEST_ROOT}/stage11d-melee-dead-branch.cpp")
file(WRITE "${_path}" "${_stage11d_melee_dead_branch}")
expect_rejected("Stage11D melee fallback cannot move into a dead branch"
    RUNTIME "${_path}"
    "rejected runtime rare abyss melee fallback scope")

set(_stage11d_recovery_guard "                && low_health\n")
string(REPLACE "${_stage11d_recovery_guard}" ""
    _stage11d_recovery_guard_bypass "${_task5a_runtime_text}")
if(_stage11d_recovery_guard_bypass STREQUAL _task5a_runtime_text)
    message(FATAL_ERROR "Stage11D recovery guard mutation made no change")
endif()
set(_path "${GUARD_TEST_ROOT}/stage11d-recovery-guard-bypass.cpp")
file(WRITE "${_path}" "${_stage11d_recovery_guard_bypass}")
expect_rejected("Stage11D high-health target cannot trigger danger escape"
    RUNTIME "${_path}"
    "rejected physical-input driver chain")

string(REPLACE "                        plan.movement_target, recovery);"
    "                        state.abyss_ranged.recovery_target, recovery);"
    _stage11d_recovery_route_bypass "${_task5a_runtime_text}")
if(_stage11d_recovery_route_bypass STREQUAL _task5a_runtime_text)
    message(FATAL_ERROR "Stage11D recovery route mutation made no change")
endif()
set(_path "${GUARD_TEST_ROOT}/stage11d-recovery-route-bypass.cpp")
file(WRITE "${_path}" "${_stage11d_recovery_route_bypass}")
expect_rejected("Stage11D recovery fire route must use the plan destination"
    RUNTIME "${_path}"
    "rejected physical-input recovery route")

string(REPLACE "    if (aggressive_abyss) {\n"
    "    if (aggressive_abyss) {\n        snapshot.active_skill_slots[state.sweep_waypoint] = true;\n"
    _stage11d_driver_slot_bypass "${_task5a_runtime_text}")
if(_stage11d_driver_slot_bypass STREQUAL _task5a_runtime_text)
    message(FATAL_ERROR "Stage11D driver skill-slot mutation made no change")
endif()
set(_path "${GUARD_TEST_ROOT}/stage11d-driver-skill-slot.cpp")
file(WRITE "${_path}" "${_stage11d_driver_slot_bypass}")
expect_rejected("Stage11D driver cannot write a physical skill slot directly"
    RUNTIME "${_path}"
    "rejected rare abyss direct skill-slot access")

string(REPLACE "    if (aggressive_abyss) {\n"
    "    if (false)\n        if (aggressive_abyss) {\n"
    _stage11d_aggressive_unbraced_dead "${_task5a_runtime_text}")
if(_stage11d_aggressive_unbraced_dead STREQUAL _task5a_runtime_text)
    message(FATAL_ERROR
        "Stage11D aggressive unbraced-dead mutation made no change")
endif()
set(_path "${GUARD_TEST_ROOT}/stage11d-aggressive-unbraced-dead.cpp")
file(WRITE "${_path}" "${_stage11d_aggressive_unbraced_dead}")
expect_rejected(
    "Stage11D aggressive branch cannot move under an unbraced dead branch"
    RUNTIME "${_path}"
    "rejected rare abyss branch owner")

string(REPLACE "    if (aggressive_abyss) {\n"
    [=[    if (aggressive_abyss) {
        {
            const auto& player = current.combat->player;
            static_cast<void>(player);
        }
        snapshot.active_skill_slots.at(state.sweep_waypoint) = true;
]=]
    _stage11d_alternate_slot_after_early_player "${_task5a_runtime_text}")
if(_stage11d_alternate_slot_after_early_player STREQUAL _task5a_runtime_text)
    message(FATAL_ERROR
        "Stage11D alternate skill-slot access mutation made no change")
endif()
set(_path
    "${GUARD_TEST_ROOT}/stage11d-alternate-slot-after-early-player.cpp")
file(WRITE "${_path}" "${_stage11d_alternate_slot_after_early_player}")
expect_rejected(
    "Stage11D aggressive branch cannot hide alternate skill-slot access"
    RUNTIME "${_path}"
    "rejected rare abyss direct skill-slot access")

string(REPLACE "    if (aggressive_abyss) {\n"
    [=[    if (aggressive_abyss) {
        auto& stage11d_active_slots = snapshot.active_skill_slots;
        static_cast<void>(stage11d_active_slots);
]=]
    _stage11d_driver_slot_alias "${_task5a_runtime_text}")
if(_stage11d_driver_slot_alias STREQUAL _task5a_runtime_text)
    message(FATAL_ERROR "Stage11D skill-slot alias mutation made no change")
endif()
set(_path "${GUARD_TEST_ROOT}/stage11d-driver-skill-slot-alias.cpp")
file(WRITE "${_path}" "${_stage11d_driver_slot_alias}")
expect_rejected("Stage11D aggressive branch cannot alias physical skill slots"
    RUNTIME "${_path}"
    "rejected rare abyss direct skill-slot access")

set(_stage11d_storm_success_return [=[        if (storm == Stage11DEquippedSkillInput::injected) {
            return Stage11DRareAbyssSkillInput::injected;
        }
]=])
string(REPLACE "${_stage11d_storm_success_return}" ""
    _stage11d_storm_fallthrough "${_task5a_runtime_text}")
if(_stage11d_storm_fallthrough STREQUAL _task5a_runtime_text)
    message(FATAL_ERROR "Stage11D storm fallthrough mutation made no change")
endif()
set(_path "${GUARD_TEST_ROOT}/stage11d-storm-fallthrough.cpp")
file(WRITE "${_path}" "${_stage11d_storm_fallthrough}")
expect_rejected("Stage11D storm injection must return before draw dispatch"
    RUNTIME "${_path}"
    "rejected rare abyss storm success return")

file(READ "${_stage_header}" _stage11d_sweep_header_text)
string(REPLACE "    bool sweep_cursor_initialized{};\n" ""
    _stage11d_sweep_cursor_state_omitted "${_stage11d_sweep_header_text}")
if(_stage11d_sweep_cursor_state_omitted STREQUAL
        _stage11d_sweep_header_text)
    message(FATAL_ERROR "Stage11D sweep cursor state mutation made no change")
endif()
set(_path "${GUARD_TEST_ROOT}/stage11d-sweep-cursor-state-omitted.hpp")
file(WRITE "${_path}" "${_stage11d_sweep_cursor_state_omitted}")
expect_rejected("Stage11D sweep cursor state cannot be omitted"
    STAGE11D_HEADER "${_path}"
    "missing runtime header token: bool sweep_cursor_initialized{};")

string(REPLACE "        if (distance < best_distance) {"
    "        if (distance > best_distance) {"
    _stage11d_nearest_sweep_reversed "${_task5a_runtime_text}")
if(_stage11d_nearest_sweep_reversed STREQUAL _task5a_runtime_text)
    message(FATAL_ERROR
        "Stage11D nearest sweep comparison mutation made no change")
endif()
set(_path "${GUARD_TEST_ROOT}/stage11d-nearest-sweep-reversed.cpp")
file(WRITE "${_path}" "${_stage11d_nearest_sweep_reversed}")
expect_rejected("Stage11D nearest sweep cursor comparison cannot be reversed"
    RUNTIME "${_path}"
    "rejected nearest sweep cursor initialization")

string(REPLACE
    "        if (!low_health && state.abyss_ranged.sweep_escape"
    "        if (state.abyss_ranged.sweep_escape"
    _stage11d_sweep_opportunity_low_health "${_task5a_runtime_text}")
if(_stage11d_sweep_opportunity_low_health STREQUAL _task5a_runtime_text)
    message(FATAL_ERROR
        "Stage11D sweep opportunity health mutation made no change")
endif()
set(_path "${GUARD_TEST_ROOT}/stage11d-sweep-opportunity-low-health.cpp")
file(WRITE "${_path}" "${_stage11d_sweep_opportunity_low_health}")
expect_rejected("Stage11D sweep opportunity cannot run at low health"
    RUNTIME "${_path}"
    "rejected rare abyss sweep opportunity owner")

set(_stage11d_inner_outer_reset [=[                state.sweep_grid = {};
                next = stage10_validation_ranged_plan(]=])
string(REPLACE "${_stage11d_inner_outer_reset}"
    "                next = stage10_validation_ranged_plan("
    _stage11d_inner_outer_reset_omitted "${_task5a_runtime_text}")
if(_stage11d_inner_outer_reset_omitted STREQUAL _task5a_runtime_text)
    message(FATAL_ERROR
        "Stage11D dual sweep outer reset mutation made no change")
endif()
set(_path "${GUARD_TEST_ROOT}/stage11d-dual-sweep-outer-reset-omitted.cpp")
file(WRITE "${_path}" "${_stage11d_inner_outer_reset_omitted}")
expect_rejected("Stage11D dual sweep ownership cannot omit outer reset"
    RUNTIME "${_path}"
    "rejected dual sweep cursor ownership")

string(REPLACE "        if (item != nullptr) {"
    "        if (state.captured && item != nullptr) {"
    _stage11d_uncaptured_reward_blocked "${_task5a_runtime_text}")
if(_stage11d_uncaptured_reward_blocked STREQUAL _task5a_runtime_text)
    message(FATAL_ERROR
        "Stage11D uncaptured reward approach mutation made no change")
endif()
set(_path "${GUARD_TEST_ROOT}/stage11d-uncaptured-reward-blocked.cpp")
file(WRITE "${_path}" "${_stage11d_uncaptured_reward_blocked}")
expect_rejected(
    "Stage11D uncaptured abyss reward must use physical approach"
    RUNTIME "${_path}"
    "requires uncaptured abyss rewards to use physical approach")

if(DEFINED STAGE11D_ABYSS_PHYSICAL_TARGETED_ONLY
        AND STAGE11D_ABYSS_PHYSICAL_TARGETED_ONLY)
    get_property(_executed GLOBAL PROPERTY
        STAGE11D_ABYSS_PHYSICAL_EXECUTED_MUTATIONS)
    if(DEFINED STAGE11D_ABYSS_PHYSICAL_TAIL_ONLY
            AND STAGE11D_ABYSS_PHYSICAL_TAIL_ONLY)
        set(_expected_abyss_mutations 3)
    else()
        set(_expected_abyss_mutations 23)
    endif()
    if(NOT _executed EQUAL _expected_abyss_mutations)
        message(FATAL_ERROR
            "Stage11D rare-abyss targeted guard executed ${_executed} mutations, expected ${_expected_abyss_mutations}")
    endif()
    message(STATUS
        "Stage11D rare-abyss physical-input guard passed: bad_mutations=${_executed}")
    return()
endif()
set(_task5a_driver_open
    "Stage11DLootValidationState& state) noexcept {\n    using Scenario = Stage11DLootValidationScenario;")
set(_task5a_driver_close
    "    return snapshot;\n}\n// STAGE11D_LOOT_VALIDATION_SEAM_END physical_driver")
string(REPLACE "${_task5a_driver_open}"
    "Stage11DLootValidationState& state) noexcept {\n    const auto stage11d_driver_decoy = [&]() noexcept {\n    using Scenario = Stage11DLootValidationScenario;"
    _task5a_driver_lambda "${_task5a_runtime_text}")
string(REPLACE "${_task5a_driver_close}"
    "    return snapshot;\n    };\n    return snapshot;\n}\n// STAGE11D_LOOT_VALIDATION_SEAM_END physical_driver"
    _task5a_driver_lambda "${_task5a_driver_lambda}")
if(_task5a_driver_lambda STREQUAL _task5a_runtime_text)
    message(FATAL_ERROR "runtime-driver-lambda mutation made no change")
endif()
set(_path "${GUARD_TEST_ROOT}/runtime-driver-uncalled-lambda.cpp")
file(WRITE "${_path}" "${_task5a_driver_lambda}")
expect_rejected("runtime driver in uncalled lambda" RUNTIME "${_path}"
    "runtime physical driver scope")

set(_task5a_driver_signature
    "PhysicalKeySnapshot inject_stage11d_physical_edges(")
string(REPLACE "${_task5a_driver_signature}"
    "// ${_task5a_driver_signature}\nPhysicalKeySnapshot task5a_disabled_stage11d_physical_edges("
    _task5a_driver_comment "${_task5a_runtime_text}")
if(_task5a_driver_comment STREQUAL _task5a_runtime_text)
    message(FATAL_ERROR "runtime-driver-comment mutation made no change")
endif()
set(_path "${GUARD_TEST_ROOT}/runtime-driver-comment-decoy.cpp")
file(WRITE "${_path}" "${_task5a_driver_comment}")
expect_rejected("runtime driver comment decoy" RUNTIME "${_path}"
    "runtime physical driver definition")

string(REPLACE "${_task5a_driver_signature}"
    "constexpr const char* task5a_driver_signature = \"${_task5a_driver_signature}\";\nPhysicalKeySnapshot task5a_disabled_stage11d_physical_edges("
    _task5a_driver_string "${_task5a_runtime_text}")
if(_task5a_driver_string STREQUAL _task5a_runtime_text)
    message(FATAL_ERROR "runtime-driver-string mutation made no change")
endif()
set(_path "${GUARD_TEST_ROOT}/runtime-driver-string-decoy.cpp")
file(WRITE "${_path}" "${_task5a_driver_string}")
expect_rejected("runtime driver string decoy" RUNTIME "${_path}"
    "runtime physical driver definition")

function(task5a_extract_seam SOURCE LABEL OUT_SEAM)
    set(_begin "// STAGE11D_LOOT_VALIDATION_SEAM_BEGIN ${LABEL}")
    set(_end "// STAGE11D_LOOT_VALIDATION_SEAM_END ${LABEL}")
    string(FIND "${SOURCE}" "${_begin}" _begin_at)
    string(FIND "${SOURCE}" "${_end}" _end_at)
    if(_begin_at EQUAL -1 OR _end_at EQUAL -1 OR NOT _begin_at LESS _end_at)
        message(FATAL_ERROR "Task5A ${LABEL} mutation seam disappeared")
    endif()
    string(LENGTH "${_end}" _end_length)
    math(EXPR _length "${_end_at} - ${_begin_at} + ${_end_length}")
    string(SUBSTRING "${SOURCE}" ${_begin_at} ${_length} _seam)
    set(${OUT_SEAM} "${_seam}" PARENT_SCOPE)
endfunction()

file(READ "${_stage_header}" _task5a_header_text)
task5a_extract_seam("${_task5a_header_text}" state
    _task5a_header_state_seam)
string(REPLACE "${_task5a_header_state_seam}"
    "namespace task5a_state_decoy {\n${_task5a_header_state_seam}\n}  // namespace task5a_state_decoy"
    _task5a_header_cross_scope "${_task5a_header_text}")
set(_path "${GUARD_TEST_ROOT}/stage11d-header-state-cross-scope.hpp")
file(WRITE "${_path}" "${_task5a_header_cross_scope}")
expect_rejected("stage11d header state cross scope" STAGE11D_HEADER "${_path}"
    "stage_header state seam scope")

set(_task5a_state_begin
    "// STAGE11D_LOOT_VALIDATION_SEAM_BEGIN state")
set(_task5a_state_end
    "// STAGE11D_LOOT_VALIDATION_SEAM_END state")
string(REPLACE "${_task5a_state_begin}" "" _task5a_header_state_body
    "${_task5a_header_state_seam}")
string(REPLACE "${_task5a_state_end}" "" _task5a_header_state_body
    "${_task5a_header_state_body}")
set(_task5a_header_nested_state
    "${_task5a_state_begin}\nnamespace task5a_nested_state {${_task5a_header_state_body}\n}  // namespace task5a_nested_state\n${_task5a_state_end}")
string(REPLACE "${_task5a_header_state_seam}"
    "${_task5a_header_nested_state}" _task5a_header_payload_cross_scope
    "${_task5a_header_text}")
if(_task5a_header_payload_cross_scope STREQUAL _task5a_header_text)
    message(FATAL_ERROR "stage11d-header-state-payload mutation made no change")
endif()
set(_path "${GUARD_TEST_ROOT}/stage11d-header-state-payload-cross-scope.hpp")
file(WRITE "${_path}" "${_task5a_header_payload_cross_scope}")
expect_rejected("stage11d header state payload cross scope" STAGE11D_HEADER
    "${_path}" "stage_header state definition scope")

task5a_extract_seam("${_task5a_runtime_text}" selectors
    _task5a_runtime_selectors_seam)
string(REPLACE "${_task5a_runtime_selectors_seam}"
    "namespace task5a_selector_decoy {\n${_task5a_runtime_selectors_seam}\n}  // namespace task5a_selector_decoy"
    _task5a_selector_cross_scope "${_task5a_runtime_text}")
set(_path "${GUARD_TEST_ROOT}/runtime-selector-cross-scope.cpp")
file(WRITE "${_path}" "${_task5a_selector_cross_scope}")
expect_rejected("runtime selector cross scope" RUNTIME "${_path}"
    "runtime selectors seam scope")

set(_task5a_selectors_begin
    "// STAGE11D_LOOT_VALIDATION_SEAM_BEGIN selectors")
set(_task5a_selectors_end
    "// STAGE11D_LOOT_VALIDATION_SEAM_END selectors")
string(REPLACE "${_task5a_selectors_begin}" "" _task5a_selectors_body
    "${_task5a_runtime_selectors_seam}")
string(REPLACE "${_task5a_selectors_end}" "" _task5a_selectors_body
    "${_task5a_selectors_body}")
set(_task5a_nested_selectors
    "${_task5a_selectors_begin}\nnamespace task5a_nested_selectors {${_task5a_selectors_body}\n}  // namespace task5a_nested_selectors\n${_task5a_selectors_end}")
string(REPLACE "${_task5a_runtime_selectors_seam}"
    "${_task5a_nested_selectors}" _task5a_selector_payload_cross_scope
    "${_task5a_runtime_text}")
if(_task5a_selector_payload_cross_scope STREQUAL _task5a_runtime_text)
    message(FATAL_ERROR "runtime-selector-payload mutation made no change")
endif()
set(_path "${GUARD_TEST_ROOT}/runtime-selector-payload-cross-scope.cpp")
file(WRITE "${_path}" "${_task5a_selector_payload_cross_scope}")
expect_rejected("runtime selector payload cross scope" RUNTIME "${_path}"
    "runtime ordinary rarity selector definition scope")

# Task 5A relocation RED: the actual host-loop call may not be replaced with
# an uncalled nested decoy while a pass-through snapshot feeds input mapping.
file(READ "${_host}" _task5a_red_host_text)
string(REPLACE "\r\n" "\n" _task5a_red_host_text
    "${_task5a_red_host_text}")
set(_task5a_host_include "#include \"host_validation.hpp\"")
set(_task5a_host_definition_decoys
    "${_task5a_host_include}\n// bool stage11d_validation_active(\nconstexpr const char* task5a_stage11d_definition_decoy = \"bool stage11d_validation_active(\";")
string(REPLACE "${_task5a_host_include}" "${_task5a_host_definition_decoys}"
    _task5a_host_harmless_definition_decoys "${_task5a_red_host_text}")
if(_task5a_host_harmless_definition_decoys STREQUAL _task5a_red_host_text)
    message(FATAL_ERROR "host harmless definition decoys made no change")
endif()
set(_path "${GUARD_TEST_ROOT}/host-harmless-definition-decoys.cpp")
file(WRITE "${_path}" "${_task5a_host_harmless_definition_decoys}")
expect_guard_accepted("Stage11D evidence guard harmless comment/string decoys"
    "${_guard}" HOST "${_path}")
expect_guard_accepted("host sequence guard harmless comment/string decoys"
    "${_sequence_guard}" HOST "${_path}")
set(_task5a_host_real_definition
    "${_task5a_host_include}\nbool stage11d_validation_active(\n    const RaylibHostConfig&) noexcept { return false; }")
string(REPLACE "${_task5a_host_include}" "${_task5a_host_real_definition}"
    _task5a_host_with_real_definition "${_task5a_red_host_text}")
if(_task5a_host_with_real_definition STREQUAL _task5a_red_host_text)
    message(FATAL_ERROR "host real moved definition mutation made no change")
endif()
set(_path "${GUARD_TEST_ROOT}/host-real-moved-definition.cpp")
file(WRITE "${_path}" "${_task5a_host_with_real_definition}")
expect_rejected("host real moved definition" HOST "${_path}"
    "runtime definition in host")
expect_sequence_rejected("host real moved definition" "${_path}"
    "runtime definition remains in raylib_host.cpp")
file(READ "${_host_validation_runtime}" _task7b_host_validation_runtime_text)
string(REPLACE "\r\n" "\n" _task7b_host_validation_runtime_text
    "${_task7b_host_validation_runtime_text}")
set(_task7b_stage11d_input_leg [=[    const PhysicalKeySnapshot stage11d_physical_keys =
        host_validation::inject_stage11d_physical_edges(
            stage11c_physical_keys, *impl_->config, input_settings,
            dungeon_snapshot, impl_->states.stage11d);]=])
set(_task7b_stage11d_passthrough [=[    const PhysicalKeySnapshot stage11d_physical_keys =
        stage11c_physical_keys;]=])
string(FIND "${_task7b_host_validation_runtime_text}"
    "${_task7b_stage11d_input_leg}" _task7b_stage11d_input_leg_position)
if(_task7b_stage11d_input_leg_position EQUAL -1)
    message(FATAL_ERROR "facade Stage11D input mutation site disappeared")
endif()

function(task7b_expect_stage11d_input_rejected NAME SLUG REPLACEMENT)
    string(REPLACE "${_task7b_stage11d_input_leg}" "${REPLACEMENT}"
        _mutated "${_task7b_host_validation_runtime_text}")
    if(_mutated STREQUAL _task7b_host_validation_runtime_text)
        message(FATAL_ERROR "${NAME} mutation made no change")
    endif()
    set(_path "${GUARD_TEST_ROOT}/${SLUG}.cpp")
    file(WRITE "${_path}" "${_mutated}")
    expect_rejected("${NAME}" HOST_VALIDATION_RUNTIME "${_path}"
        "rejected host input call binding")
endfunction()

set(_task7b_input_lambda [=[    const auto stage11d_input_decoy = [&]() noexcept {
]=])
string(APPEND _task7b_input_lambda
    "${_task7b_stage11d_input_leg}\n        return stage11d_physical_keys;\n    };\n${_task7b_stage11d_passthrough}")
task7b_expect_stage11d_input_rejected(
    "facade Stage11D input in uncalled lambda"
    "facade-stage11d-input-uncalled-lambda"
    "${_task7b_input_lambda}")

set(_task7b_input_comment
    "/*\n${_task7b_stage11d_input_leg}\n*/\n${_task7b_stage11d_passthrough}")
task7b_expect_stage11d_input_rejected(
    "facade Stage11D input comment decoy"
    "facade-stage11d-input-comment-decoy"
    "${_task7b_input_comment}")

set(_task7b_input_string [=[    constexpr const char* stage11d_input_decoy =
        "host_validation::inject_stage11d_physical_edges("
        "stage11c_physical_keys, *impl_->config, input_settings, "
        "dungeon_snapshot, impl_->states.stage11d);";
]=])
string(APPEND _task7b_input_string "${_task7b_stage11d_passthrough}")
task7b_expect_stage11d_input_rejected(
    "facade Stage11D input string decoy"
    "facade-stage11d-input-string-decoy"
    "${_task7b_input_string}")

set(_task7b_input_raw
    "    constexpr const char* stage11d_input_decoy = R\"TASK7B(\n${_task7b_stage11d_input_leg}\n)TASK7B\";\n${_task7b_stage11d_passthrough}")
task7b_expect_stage11d_input_rejected(
    "facade Stage11D input raw-string decoy"
    "facade-stage11d-input-raw-string-decoy"
    "${_task7b_input_raw}")

set(_task7b_input_inactive
    "#if 0\n${_task7b_stage11d_input_leg}\n#endif\n${_task7b_stage11d_passthrough}")
task7b_expect_stage11d_input_rejected(
    "facade Stage11D input inactive decoy"
    "facade-stage11d-input-inactive-decoy"
    "${_task7b_input_inactive}")

set(_task7b_input_cross_scope "${_task7b_stage11d_passthrough}")
string(REPLACE "${_task7b_stage11d_input_leg}"
    "${_task7b_input_cross_scope}" _task7b_cross_scope_mutation
    "${_task7b_host_validation_runtime_text}")
string(APPEND _task7b_cross_scope_mutation
    "\nnamespace task7b_input_decoy {\n${_task7b_stage11d_input_leg}\n}\n")
set(_path "${GUARD_TEST_ROOT}/facade-stage11d-input-cross-scope.cpp")
file(WRITE "${_path}" "${_task7b_cross_scope_mutation}")
expect_rejected("facade Stage11D input cross-scope decoy"
    HOST_VALIDATION_RUNTIME "${_path}"
    "rejected host input call binding")

set(_task7b_input_wrong_argument [=[    const PhysicalKeySnapshot stage11d_physical_keys =
        host_validation::inject_stage11d_physical_edges(
            stage11c_physical_keys, *impl_->config, input_settings,
            impl_->death_input_snapshot, impl_->states.stage11d);]=])
task7b_expect_stage11d_input_rejected(
    "facade Stage11D input wrong argument"
    "facade-stage11d-input-wrong-argument"
    "${_task7b_input_wrong_argument}")

set(_task7b_host_crop_end "core::FixedStepFrame frame = host_gate.fixed_step;")
string(FIND "${_task5a_red_host_text}" "${_task7b_host_crop_end}"
    _task7b_host_crop_end_position)
if(_task7b_host_crop_end_position EQUAL -1)
    message(FATAL_ERROR "host facade input duplicate mutation site disappeared")
endif()
set(_task7b_host_spliced_facade_input [=[            static_cast<void>(validation_runtime->inject_phy\
sical_edges(
                sampled_physical_keys, input_settings, current,
                gameplay_rearm_was_required));]=])
string(REPLACE "${_task7b_host_crop_end}"
    "${_task7b_host_crop_end}\n${_task7b_host_spliced_facade_input}"
    _task7b_host_duplicate_facade_input "${_task5a_red_host_text}")
if(_task7b_host_duplicate_facade_input STREQUAL _task5a_red_host_text)
    message(FATAL_ERROR "host facade input duplicate mutation made no change")
endif()
set(_path "${GUARD_TEST_ROOT}/host-facade-input-duplicate-outside-crop.cpp")
file(WRITE "${_path}" "${_task7b_host_duplicate_facade_input}")
expect_rejected("host facade input duplicate outside crop" HOST "${_path}"
    "rejected host input call binding")

set(_task5a_pause_call
    "const PauseCommand pause_command = update_pause_menu(\n                pause_menu, pause_context, pause_input);")
string(REPLACE "${_task5a_pause_call}"
    "const auto task5a_pause_decoy = [&]() noexcept {\n                ${_task5a_pause_call}\n                return pause_command;\n            };\n            const PauseCommand pause_command{};"
    _task5a_pause_lambda "${_task5a_red_host_text}")
if(_task5a_pause_lambda STREQUAL _task5a_red_host_text)
    message(FATAL_ERROR "host-pause-lambda mutation made no change")
endif()
set(_path "${GUARD_TEST_ROOT}/host-pause-uncalled-lambda.cpp")
file(WRITE "${_path}" "${_task5a_pause_lambda}")
expect_rejected("host pause call in uncalled lambda" HOST "${_path}"
    "host pause call scope")

task5a_extract_seam("${_task5a_red_host_text}" fixed_step
    _task5a_fixed_step_seam)
string(REPLACE "${_task5a_fixed_step_seam}"
    "/*\n${_task5a_fixed_step_seam}\n*/\n                        || false"
    _task5a_fixed_step_comment "${_task5a_red_host_text}")
set(_path "${GUARD_TEST_ROOT}/host-fixed-step-comment-decoy.cpp")
file(WRITE "${_path}" "${_task5a_fixed_step_comment}")
expect_rejected("host fixed-step activation comment decoy" HOST "${_path}"
    "cannot bind host fixed_step seam marker")

set(_task5a_fixed_step_begin
    "// STAGE11D_LOOT_VALIDATION_SEAM_BEGIN fixed_step")
set(_task5a_fixed_step_end
    "// STAGE11D_LOOT_VALIDATION_SEAM_END fixed_step")
set(_task5a_nested_fixed_step
    "${_task5a_fixed_step_begin}\n                        || ([&]() noexcept {\n                            if (false) {\n                                return config.stage11d_loot_validation\n                                    != Stage11DLootValidationScenario::none;\n                            }\n                            return false;\n                        }())\n${_task5a_fixed_step_end}")
string(REPLACE "${_task5a_fixed_step_seam}"
    "${_task5a_nested_fixed_step}" _task5a_fixed_step_payload_cross_scope
    "${_task5a_red_host_text}")
if(_task5a_fixed_step_payload_cross_scope STREQUAL _task5a_red_host_text)
    message(FATAL_ERROR "host-fixed-step-payload mutation made no change")
endif()
set(_path "${GUARD_TEST_ROOT}/host-fixed-step-payload-cross-scope.cpp")
file(WRITE "${_path}" "${_task5a_fixed_step_payload_cross_scope}")
expect_rejected("host fixed-step activation payload cross scope" HOST "${_path}"
    "host fixed-step activation call scope")

task5a_extract_seam("${_task5a_red_host_text}" abyss_claim
    _task5a_abyss_seam)
string(REPLACE "${_task5a_abyss_seam}"
    "const auto task5a_abyss_decoy = [&]() noexcept {\n${_task5a_abyss_seam}\n                };"
    _task5a_abyss_lambda "${_task5a_red_host_text}")
set(_path "${GUARD_TEST_ROOT}/host-abyss-uncalled-lambda.cpp")
file(WRITE "${_path}" "${_task5a_abyss_lambda}")
expect_rejected("host abyss observer in uncalled lambda" HOST "${_path}"
    "host abyss_claim seam scope")

set(_task7b_post_tick_definition [=[void HostValidationRuntime::observe_post_fixed_tick(
    const dungeon::DungeonSnapshot& snapshot,
    const items::ItemOwnershipState* ownership) noexcept {
    if (ownership == nullptr) return;
    host_validation::observe_stage11d_abyss_claim(
        impl_->states.stage11d, snapshot, *ownership);
}]=])
string(REPLACE "${_task7b_post_tick_definition}" ""
    _task7b_post_tick_wrong_namespace
    "${_task7b_host_validation_runtime_text}")
string(APPEND _task7b_post_tick_wrong_namespace
    "\nnamespace task7b_wrong_platform {\n${_task7b_post_tick_definition}\n}  // namespace task7b_wrong_platform\n")
if(_task7b_post_tick_wrong_namespace STREQUAL
        _task7b_host_validation_runtime_text)
    message(FATAL_ERROR
        "facade post-tick wrong-namespace mutation made no change")
endif()
set(_path "${GUARD_TEST_ROOT}/facade-post-tick-wrong-namespace.cpp")
file(WRITE "${_path}" "${_task7b_post_tick_wrong_namespace}")
expect_rejected("facade post-tick owner in wrong namespace"
    HOST_VALIDATION_RUNTIME "${_path}"
    "rejected facade abyss observer namespace")
set(_task7b_post_tick_abyss_call [=[    host_validation::observe_stage11d_abyss_claim(
        impl_->states.stage11d, snapshot, *ownership);]=])
string(REPLACE "${_task7b_post_tick_abyss_call}"
    "    const auto task7b_post_tick_decoy = [&]() noexcept {
    ${_task7b_post_tick_abyss_call}
    };"
    _task7b_post_tick_lambda "${_task7b_host_validation_runtime_text}")
if(_task7b_post_tick_lambda STREQUAL _task7b_host_validation_runtime_text)
    message(FATAL_ERROR "facade post-tick lambda mutation made no change")
endif()
set(_path "${GUARD_TEST_ROOT}/facade-post-tick-uncalled-lambda.cpp")
file(WRITE "${_path}" "${_task7b_post_tick_lambda}")
expect_rejected("facade post-tick observer in uncalled lambda"
    HOST_VALIDATION_RUNTIME "${_path}"
    "rejected facade abyss observer binding")

string(REPLACE "${_task7b_post_tick_abyss_call}"
    "    host_validation::observe_stage17_snapshot(
        *impl_->config, impl_->states.stage17, snapshot);
${_task7b_post_tick_abyss_call}"
    _task7b_post_tick_extra_snapshot "${_task7b_host_validation_runtime_text}")
if(_task7b_post_tick_extra_snapshot STREQUAL
        _task7b_host_validation_runtime_text)
    message(FATAL_ERROR
        "facade post-tick extra snapshot mutation made no change")
endif()
set(_path "${GUARD_TEST_ROOT}/facade-post-tick-extra-snapshot.cpp")
file(WRITE "${_path}" "${_task7b_post_tick_extra_snapshot}")
expect_rejected("facade post-tick extra snapshot observation"
    HOST_VALIDATION_RUNTIME "${_path}"
    "rejected facade abyss observer binding")

string(REPLACE "${_task7b_post_tick_definition}"
    "namespace task7b_post_tick_decoy {
${_task7b_post_tick_definition}
}  // namespace task7b_post_tick_decoy"
    _task7b_post_tick_cross_scope "${_task7b_host_validation_runtime_text}")
if(_task7b_post_tick_cross_scope STREQUAL
        _task7b_host_validation_runtime_text)
    message(FATAL_ERROR
        "facade post-tick cross-scope mutation made no change")
endif()
set(_path "${GUARD_TEST_ROOT}/facade-post-tick-cross-scope.cpp")
file(WRITE "${_path}" "${_task7b_post_tick_cross_scope}")
expect_rejected("facade post-tick owner in cross scope"
    HOST_VALIDATION_RUNTIME "${_path}"
    "rejected facade abyss observer scope")

string(REPLACE "if (ownership == nullptr) return;"
    "if (ownership != nullptr) return;" _task7b_post_tick_altered
    "${_task7b_post_tick_definition}")
string(REPLACE "${_task7b_post_tick_definition}"
    "#if 0
${_task7b_post_tick_definition}
#endif
${_task7b_post_tick_altered}"
    _task7b_post_tick_inactive_correct
    "${_task7b_host_validation_runtime_text}")
if(_task7b_post_tick_inactive_correct STREQUAL
        _task7b_host_validation_runtime_text)
    message(FATAL_ERROR
        "facade post-tick inactive owner mutation made no change")
endif()
set(_path "${GUARD_TEST_ROOT}/facade-post-tick-inactive-correct.cpp")
file(WRITE "${_path}" "${_task7b_post_tick_inactive_correct}")
expect_rejected("facade post-tick inactive correct owner"
    HOST_VALIDATION_RUNTIME "${_path}"
    "missing facade abyss observer owner")

string(REPLACE "current, &session->item_state()"
    "previous, &session->item_state()" _task7b_abyss_previous
    "${_task5a_abyss_seam}")
if(_task7b_abyss_previous STREQUAL _task5a_abyss_seam)
    message(FATAL_ERROR "host abyss previous snapshot mutation made no change")
endif()
string(REPLACE "${_task5a_abyss_seam}" "${_task7b_abyss_previous}"
    _task7b_abyss_previous_host "${_task5a_red_host_text}")
set(_path "${GUARD_TEST_ROOT}/host-abyss-previous-snapshot.cpp")
file(WRITE "${_path}" "${_task7b_abyss_previous_host}")
expect_rejected("host abyss observer previous snapshot" HOST "${_path}"
    "rejected host abyss observer binding")

string(REPLACE "&session->item_state()" "nullptr"
    _task7b_abyss_null_ownership "${_task5a_abyss_seam}")
if(_task7b_abyss_null_ownership STREQUAL _task5a_abyss_seam)
    message(FATAL_ERROR "host abyss null ownership mutation made no change")
endif()
string(REPLACE "${_task5a_abyss_seam}" "${_task7b_abyss_null_ownership}"
    _task7b_abyss_null_host "${_task5a_red_host_text}")
set(_path "${GUARD_TEST_ROOT}/host-abyss-null-ownership.cpp")
file(WRITE "${_path}" "${_task7b_abyss_null_host}")
expect_rejected("host abyss observer null ownership" HOST "${_path}"
    "rejected host abyss observer binding")

string(REPLACE "${_task5a_abyss_seam}" "" _task5a_abyss_early
    "${_task5a_red_host_text}")
string(REPLACE "                runtime.fixed_tick(step_movement,"
    "${_task5a_abyss_seam}\n                runtime.fixed_tick(step_movement,"
    _task5a_abyss_early "${_task5a_abyss_early}")
set(_path "${GUARD_TEST_ROOT}/host-abyss-before-fixed-tick.cpp")
file(WRITE "${_path}" "${_task5a_abyss_early}")
expect_rejected("host abyss observer before fixed tick" HOST "${_path}"
    "abyss claim observation order")

set(_task7b_drain_target_site [=[                drain_events(*session, renderer, feedback, audio,
                    validation_runtime.get());
                if (validation_runtime->fixed_step_target_reached(current)) {]=])
string(REPLACE "${_task5a_abyss_seam}" "" _task7b_abyss_after_drain
    "${_task5a_red_host_text}")
string(FIND "${_task7b_abyss_after_drain}" "${_task7b_drain_target_site}"
    _task7b_drain_target_site_position)
if(_task7b_drain_target_site_position EQUAL -1)
    message(FATAL_ERROR "host abyss after drain anchor is missing")
endif()
string(LENGTH "${_task7b_drain_target_site}"
    _task7b_drain_target_site_length)
math(EXPR _task7b_drain_target_tail_position
    "${_task7b_drain_target_site_position} + ${_task7b_drain_target_site_length}")
string(SUBSTRING "${_task7b_abyss_after_drain}"
    ${_task7b_drain_target_tail_position} -1 _task7b_drain_target_tail)
string(FIND "${_task7b_drain_target_tail}" "${_task7b_drain_target_site}"
    _task7b_duplicate_drain_target_site_position)
if(NOT _task7b_duplicate_drain_target_site_position EQUAL -1)
    message(FATAL_ERROR "host abyss after drain anchor is duplicated")
endif()
string(REPLACE "${_task7b_drain_target_site}"
    "                drain_events(*session, renderer, feedback, audio,
                    validation_runtime.get());
${_task5a_abyss_seam}
                if (validation_runtime->fixed_step_target_reached(current)) {"
    _task7b_abyss_after_drain "${_task7b_abyss_after_drain}")
if(_task7b_abyss_after_drain STREQUAL _task5a_red_host_text)
    message(FATAL_ERROR "host abyss after drain mutation made no change")
endif()
set(_path "${GUARD_TEST_ROOT}/host-abyss-after-drain.cpp")
file(WRITE "${_path}" "${_task7b_abyss_after_drain}")
expect_rejected("host abyss observer after drain" HOST "${_path}"
    "abyss claim observation order")

file(READ "${_formal}" _formal_text)
set(_site "platform::RaylibHostConfig config{};")
string(FIND "${_formal_text}" "${_site}" _site_index)
if(_site_index EQUAL -1)
    message(FATAL_ERROR "private-injection mutation site disappeared")
endif()
string(REPLACE "${_site}"
    "TestAccess::inject(config);\n    ${_site}" _mutated "${_formal_text}")
if(_mutated STREQUAL _formal_text)
    message(FATAL_ERROR "private-injection mutation made no change")
endif()
set(_path "${GUARD_TEST_ROOT}/private-injection.cpp")
file(WRITE "${_path}" "${_mutated}")
expect_rejected("private injection" FORMAL "${_path}" "formal bypass: TestAccess")

string(REPLACE "${_site}"
    "FakeRenderer fake_renderer{};\n    ${_site}" _mutated "${_formal_text}")
if(_mutated STREQUAL _formal_text)
    message(FATAL_ERROR "fake-renderer mutation made no change")
endif()
set(_path "${GUARD_TEST_ROOT}/fake-renderer.cpp")
file(WRITE "${_path}" "${_mutated}")
expect_rejected("non-production renderer" FORMAL "${_path}"
    "formal bypass: FakeRenderer")

set(_site "return platform::run_raylib_host(config)")
string(FIND "${_formal_text}" "${_site}" _site_index)
if(_site_index EQUAL -1)
    message(FATAL_ERROR "direct-pickup mutation site disappeared")
endif()
string(REPLACE "${_site}" "request_pickup(0);\n    ${_site}"
    _mutated "${_formal_text}")
if(_mutated STREQUAL _formal_text)
    message(FATAL_ERROR "direct-pickup mutation made no change")
endif()
set(_path "${GUARD_TEST_ROOT}/direct-pickup.cpp")
file(WRITE "${_path}" "${_mutated}")
expect_rejected("direct pickup completion" FORMAL "${_path}"
    "formal bypass: request_pickup(")

set(_site "draft.loot_filter_mode = spec.mode;")
string(FIND "${_formal_text}" "${_site}" _site_index)
if(_site_index EQUAL -1)
    message(FATAL_ERROR "fake-settings mutation site disappeared")
endif()
string(REPLACE "${_site}"
    "pause_menu.committed.loot_filter_mode = spec.mode;"
    _mutated "${_formal_text}")
if(_mutated STREQUAL _formal_text)
    message(FATAL_ERROR "fake-settings mutation made no change")
endif()
set(_path "${GUARD_TEST_ROOT}/fake-settings.cpp")
file(WRITE "${_path}" "${_mutated}")
expect_rejected("fake settings publication" FORMAL "${_path}"
    "pause_menu.committed")

set(_site "|| absolute.filename() != \"stage11d loot evidence\"")
string(FIND "${_formal_text}" "${_site}" _site_index)
if(_site_index EQUAL -1)
    message(FATAL_ERROR "unsafe-root mutation site disappeared")
endif()
string(REPLACE "${_site}" "|| false" _mutated "${_formal_text}")
if(_mutated STREQUAL _formal_text)
    message(FATAL_ERROR "unsafe-root mutation made no change")
endif()
set(_path "${GUARD_TEST_ROOT}/unsafe-root.cpp")
file(WRITE "${_path}" "${_mutated}")
expect_rejected("malicious evidence root acceptance" FORMAL "${_path}"
    "formal cleanup safety is incomplete: absolute.filename()")

set(_site "FILE_ATTRIBUTE_REPARSE_POINT")
string(FIND "${_formal_text}" "${_site}" _site_index)
if(_site_index EQUAL -1)
    message(FATAL_ERROR "reparse-safety mutation site disappeared")
endif()
string(REPLACE "${_site}" "FILE_ATTRIBUTE_DIRECTORY"
    _mutated "${_formal_text}")
if(_mutated STREQUAL _formal_text)
    message(FATAL_ERROR "reparse-safety mutation made no change")
endif()
set(_path "${GUARD_TEST_ROOT}/reparse-safety.cpp")
file(WRITE "${_path}" "${_mutated}")
expect_rejected("reparse-point evidence escape" FORMAL "${_path}"
    "formal cleanup safety is incomplete: FILE_ATTRIBUTE_REPARSE_POINT")

set(_site "std::filesystem::create_directories(root, error);")
string(FIND "${_formal_text}" "${_site}" _site_index)
if(_site_index EQUAL -1)
    message(FATAL_ERROR "recursive-cleanup mutation site disappeared")
endif()
string(REPLACE "${_site}"
    "std::filesystem::remove_all(root, error);\n    ${_site}"
    _mutated "${_formal_text}")
if(_mutated STREQUAL _formal_text)
    message(FATAL_ERROR "recursive-cleanup mutation made no change")
endif()
set(_path "${GUARD_TEST_ROOT}/recursive-cleanup.cpp")
file(WRITE "${_path}" "${_mutated}")
expect_rejected("recursive evidence cleanup" FORMAL "${_path}"
    "formal bypass: remove_all(")

file(READ "${_host}" _host_text)
string(REPLACE "\r\n" "\n" _host_text "${_host_text}")
function(expect_host_insert NAME SLUG SITE INSERT EXPECTED_REASON)
    string(FIND "${_host_text}" "${SITE}" _site_index)
    if(_site_index EQUAL -1)
        message(FATAL_ERROR "${NAME} mutation site disappeared")
    endif()
    string(LENGTH "${SITE}" _site_length)
    math(EXPR _after_site "${_site_index} + ${_site_length}")
    string(SUBSTRING "${_host_text}" ${_after_site} -1 _after_text)
    string(FIND "${_after_text}" "${SITE}" _second_site)
    if(NOT _second_site EQUAL -1)
        message(FATAL_ERROR "${NAME} mutation site is not unique")
    endif()
    string(REPLACE "${SITE}" "${SITE}\n                    ${INSERT}"
        _mutated "${_host_text}")
    if(_mutated STREQUAL _host_text)
        message(FATAL_ERROR "${NAME} mutation made no change")
    endif()
    set(_path "${GUARD_TEST_ROOT}/${SLUG}.cpp")
    file(WRITE "${_path}" "${_mutated}")
    expect_rejected("${NAME}" HOST "${_path}" "${EXPECTED_REASON}")
endfunction()

set(_host_scope_site
    "validation_runtime->observe_post_fixed_tick(")
expect_host_insert("host private access" "host-private-access"
    "${_host_scope_site}" "const auto* TestAccess = session;"
    "host-bypass-TestAccess")
expect_host_insert("host snapshot override" "host-snapshot-override"
    "${_host_scope_site}" "const auto snapshot_override = current;"
    "host-bypass-snapshot_override")
expect_host_insert("host direct pickup request" "host-direct-pickup"
    "${_host_scope_site}"
    "static_cast<void>(session->request_pickup(0U));"
    "host-bypass-request_pickup(")
expect_host_insert("host direct pickup completion" "host-complete-pickup"
    "${_host_scope_site}"
    "static_cast<void>(session->complete_pickup(0U));"
    "host-bypass-complete_pickup(")
expect_host_insert("host direct pickup publication" "host-publish-pickup"
    "${_host_scope_site}"
    "static_cast<void>(session->publish_pickup(0U));"
    "host-bypass-publish_pickup(")
expect_host_insert("host committed settings write" "host-committed-settings"
    "${_host_scope_site}"
    "pause_menu.committed.loot_filter_mode = settings::LootFilterMode::rare_only;"
    "host validation seam direct committed settings write")
expect_host_insert("host live settings write" "host-live-settings"
    "${_host_scope_site}"
    "live_settings.loot_filter_mode = settings::LootFilterMode::rare_only;"
    "host validation seam direct live settings write")
expect_host_insert("host direct result pass" "host-result-pass"
    "${_host_scope_site}" "const char* result = \"result=pass\";"
    "host validation seam direct result pass")

set(_site "const auto& item = snapshot.ground_items[index];")
string(FIND "${_task5a_runtime_text}" "${_site}" _site_index)
if(_site_index EQUAL -1)
    message(FATAL_ERROR "snapshot-mutation site disappeared")
endif()
string(REPLACE "${_site}" "snapshot.ground_items[0] = fabricated;"
    _mutated "${_task5a_runtime_text}")
if(_mutated STREQUAL _task5a_runtime_text)
    message(FATAL_ERROR "snapshot mutation made no change")
endif()
set(_path "${GUARD_TEST_ROOT}/snapshot-mutation.cpp")
file(WRITE "${_path}" "${_mutated}")
expect_rejected("snapshot mutation" RUNTIME "${_path}"
    "rejected snapshot mutation")

set(_site "EndDrawing();\n    if (path == nullptr) return true;")
string(FIND "${_host_text}" "${_site}" _site_index)
if(_site_index EQUAL -1)
    message(FATAL_ERROR "pre-present-capture mutation site disappeared")
endif()
string(REPLACE "${_site}"
    "Image pre_present = LoadImageFromScreen();\n    ${_site}"
    _mutated "${_host_text}")
if(_mutated STREQUAL _host_text)
    message(FATAL_ERROR "pre-present mutation made no change")
endif()
set(_path "${GUARD_TEST_ROOT}/pre-present-capture.cpp")
file(WRITE "${_path}" "${_mutated}")
expect_rejected("pre-EndDrawing capture" HOST "${_path}"
    "rejected pre-EndDrawing capture")

file(READ "${_renderer}" _renderer_text)
set(_renderer_definition_decoy
    "// CombatRenderPlan make_combat_render_plan( is a harmless signature decoy.\n")
string(REPLACE "#include \"combat_renderer.hpp\""
    "#include \"combat_renderer.hpp\"\n${_renderer_definition_decoy}"
    _renderer_harmless_definition_decoy "${_renderer_text}")
if(_renderer_harmless_definition_decoy STREQUAL _renderer_text)
    message(FATAL_ERROR "renderer harmless definition decoy anchor disappeared")
endif()
set(_path "${GUARD_TEST_ROOT}/renderer-harmless-definition-decoy.cpp")
file(WRITE "${_path}" "${_renderer_harmless_definition_decoy}")
expect_guard_accepted("Stage11D renderer harmless definition decoy"
    "${_guard}" RENDERER "${_path}")

set(_site "const CombatRenderPlan render_plan = make_combat_render_plan(")
string(FIND "${_renderer_text}" "${_site}" _site_index)
if(_site_index EQUAL -1)
    message(FATAL_ERROR "second-render-plan mutation site disappeared")
endif()
string(REPLACE "${_site}"
    "const CombatRenderPlan duplicate_plan = make_combat_render_plan(\n        previous, current, clamped_interpolation_alpha, camera_offset,\n        loot_filter_mode_, static_cast<float>(GetScreenWidth()),\n        static_cast<float>(GetScreenHeight()));\n\n    ${_site}"
    _mutated "${_renderer_text}")
if(_mutated STREQUAL _renderer_text)
    message(FATAL_ERROR "second-render-plan mutation made no change")
endif()
set(_path "${GUARD_TEST_ROOT}/second-render-plan.cpp")
file(WRITE "${_path}" "${_mutated}")
expect_rejected("second render plan" RENDERER "${_path}"
    "requires exactly one renderer plan in CombatRenderer::draw")

set(_draw_site "GroundLootView CombatRenderer::draw(")
string(FIND "${_renderer_text}" "${_draw_site}" _draw_site_index)
if(_draw_site_index EQUAL -1)
    message(FATAL_ERROR "draw-external-plan mutation site disappeared")
endif()
set(_draw_external_plan
    "void stage11d_draw_external_plan(const dungeon::DungeonSnapshot& snapshot) noexcept {\n    static_cast<void>(make_combat_render_plan(\n        snapshot, settings::LootFilterMode::show_all, 1.0F, 1.0F));\n}\n\n${_draw_site}")
string(REPLACE "${_draw_site}" "${_draw_external_plan}"
    _mutated "${_renderer_text}")
if(_mutated STREQUAL _renderer_text)
    message(FATAL_ERROR "draw-external-plan mutation made no change")
endif()
set(_path "${GUARD_TEST_ROOT}/draw-external-plan.cpp")
file(WRITE "${_path}" "${_mutated}")
expect_rejected("draw-external render plan" RENDERER "${_path}"
    "rejected renderer plan outside CombatRenderer::draw")

file(READ "${_validator}" _validator_text)

set(_task9_player_damage_condition "$values.player_damage_observed -eq '1' -and")
string(REPLACE "${_task9_player_damage_condition}"
    "$values.player_damage_observed -eq '1' -and\n        $values.monster_damage_observed -eq '1' -and"
    _task9_monster_damage_required "${_validator_text}")
if(_task9_monster_damage_required STREQUAL _validator_text)
    message(FATAL_ERROR "diagnostic monster-damage mutation anchor disappeared")
endif()
set(_path "${GUARD_TEST_ROOT}/diagnostic-monster-damage-required.ps1")
file(WRITE "${_path}" "${_task9_monster_damage_required}")
expect_rejected("diagnostic monster damage cannot be required" VALIDATOR "${_path}"
    "forbids diagnostic monster damage as a formal requirement")

set(_task9_tuple_assignment [=[
$ordinaryTupleChecks = @(
    ($show.observed_normal -eq $manifest.ordinary_normal_tuple),
    ($show.observed_magic -eq $manifest.ordinary_magic_tuple),
    ($show.observed_rare -eq $manifest.ordinary_rare_tuple),
    ($magic.observed_normal -eq $manifest.ordinary_normal_tuple),
    ($magic.observed_magic -eq $manifest.ordinary_magic_tuple),
    ($magic.observed_rare -eq $manifest.ordinary_rare_tuple),
    ($rare.observed_normal -eq $manifest.ordinary_normal_tuple),
    ($rare.observed_magic -eq $manifest.ordinary_magic_tuple),
    ($rare.observed_rare -eq $manifest.ordinary_rare_tuple),
    ($preview.observed_normal -eq $manifest.ordinary_normal_tuple),
    ($preview.observed_magic -eq $manifest.ordinary_magic_tuple),
    ($preview.observed_rare -eq $manifest.ordinary_rare_tuple),
    ($pickup.observed_normal -eq $manifest.ordinary_normal_tuple),
    ($pickup.observed_magic -eq $manifest.ordinary_magic_tuple),
    ($pickup.observed_rare -eq $manifest.ordinary_rare_tuple)
)
]=])
set(_task9_tuple_comment_assignment [=[
$ordinaryTupleChecks = @(
    $true # $show.observed_normal -eq $manifest.ordinary_normal_tuple, $show.observed_magic -eq $manifest.ordinary_magic_tuple, $show.observed_rare -eq $manifest.ordinary_rare_tuple, $magic.observed_normal -eq $manifest.ordinary_normal_tuple, $magic.observed_magic -eq $manifest.ordinary_magic_tuple, $magic.observed_rare -eq $manifest.ordinary_rare_tuple, $rare.observed_normal -eq $manifest.ordinary_normal_tuple, $rare.observed_magic -eq $manifest.ordinary_magic_tuple, $rare.observed_rare -eq $manifest.ordinary_rare_tuple, $preview.observed_normal -eq $manifest.ordinary_normal_tuple, $preview.observed_magic -eq $manifest.ordinary_magic_tuple, $preview.observed_rare -eq $manifest.ordinary_rare_tuple, $pickup.observed_normal -eq $manifest.ordinary_normal_tuple, $pickup.observed_magic -eq $manifest.ordinary_magic_tuple, $pickup.observed_rare -eq $manifest.ordinary_rare_tuple
)
]=])
string(REPLACE "${_task9_tuple_assignment}" "${_task9_tuple_comment_assignment}"
    _task9_tuple_comments "${_validator_text}")
if(_task9_tuple_comments STREQUAL _validator_text)
    message(FATAL_ERROR "tuple comment mutation anchor disappeared")
endif()
set(_path "${GUARD_TEST_ROOT}/tuple-comparisons-in-comments.ps1")
file(WRITE "${_path}" "${_task9_tuple_comments}")
expect_rejected("tuple comparisons cannot move into comments" VALIDATOR "${_path}"
    "requires real top-level per-rarity tuple assignment")

set(_task9_tuple_assignment_start "$ordinaryTupleChecks = @(")
set(_task9_tuple_assignment_end
    ")\nRequire (-not ($ordinaryTupleChecks -contains $false))")
string(REPLACE "${_task9_tuple_assignment_start}"
    "@'\n${_task9_tuple_assignment_start}"
    _task9_tuple_here_string "${_validator_text}")
string(REPLACE "${_task9_tuple_assignment_end}"
    ")\n'@\nRequire (-not ($ordinaryTupleChecks -contains $false))"
    _task9_tuple_here_string "${_task9_tuple_here_string}")
if(_task9_tuple_here_string STREQUAL _validator_text)
    message(FATAL_ERROR "tuple here-string mutation anchor disappeared")
endif()
set(_path "${GUARD_TEST_ROOT}/tuple-assignment-here-string.ps1")
file(WRITE "${_path}" "${_task9_tuple_here_string}")
expect_rejected("tuple assignment cannot move into a here string" VALIDATOR "${_path}"
    "requires real top-level per-rarity tuple assignment")

set(_task9_tuple_top_level
    "Require (-not ($ordinaryTupleChecks -contains $false)) 'production rarity tuples drifted'")
set(_task9_tuple_reorder_site
    "${_task9_tuple_assignment}${_task9_tuple_top_level}")
string(REPLACE "${_task9_tuple_reorder_site}"
    "${_task9_tuple_top_level}\n${_task9_tuple_assignment}"
    _task9_tuple_reordered "${_validator_text}")
if(_task9_tuple_reordered STREQUAL _validator_text)
    message(FATAL_ERROR "tuple assignment reorder mutation anchor disappeared")
endif()
set(_path "${GUARD_TEST_ROOT}/tuple-assignment-reordered.ps1")
file(WRITE "${_path}" "${_task9_tuple_reordered}")
expect_rejected("tuple assignment cannot move after its Require" VALIDATOR "${_path}"
    "requires per-rarity tuple assignment immediately before its Require")
set(_task9_tuple_function
    "function Test-Stage11DTupleRelocation {\n    ${_task9_tuple_top_level}\n}")
string(REPLACE "${_task9_tuple_top_level}" "${_task9_tuple_function}"
    _task9_tuple_relocated "${_validator_text}")
if(_task9_tuple_relocated STREQUAL _validator_text)
    message(FATAL_ERROR "tuple relocation mutation anchor disappeared")
endif()
set(_path "${GUARD_TEST_ROOT}/tuple-relocated-function.ps1")
file(WRITE "${_path}" "${_task9_tuple_relocated}")
expect_rejected("tuple assertion cannot move into an uncalled function" VALIDATOR "${_path}"
    "requires exact per-rarity tuples in a top-level executable Require command")

set(_task9_pickup_derivation
    "$expectedPickupSnapshot = @($ordinaryIds | Where-Object { $_ -ne $pickup.pickup_item_id })")
string(REPLACE "${_task9_pickup_derivation}"
    "@'\n${_task9_pickup_derivation}\n'@"
    _task9_pickup_here_string "${_validator_text}")
if(_task9_pickup_here_string STREQUAL _validator_text)
    message(FATAL_ERROR "pickup here-string mutation anchor disappeared")
endif()
set(_path "${GUARD_TEST_ROOT}/pickup-derivation-here-string.ps1")
file(WRITE "${_path}" "${_task9_pickup_here_string}")
expect_rejected("pickup derivation cannot move into a here string" VALIDATOR "${_path}"
    "requires real top-level pickup conservation derivation")

set(_task9_pickup_predicate "Where-Object { $_ -ne $pickup.pickup_item_id }")
string(REPLACE "${_task9_pickup_predicate}"
    "Where-Object { $true # $_ -ne $pickup.pickup_item_id\n}"
    _task9_pickup_predicate_comment "${_validator_text}")
if(_task9_pickup_predicate_comment STREQUAL _validator_text)
    message(FATAL_ERROR "pickup predicate comment mutation anchor disappeared")
endif()
set(_path "${GUARD_TEST_ROOT}/pickup-predicate-in-comment.ps1")
file(WRITE "${_path}" "${_task9_pickup_predicate_comment}")
expect_rejected("pickup predicate cannot move into a comment" VALIDATOR "${_path}"
    "requires real top-level pickup conservation derivation")

set(_task9_pickup_top_level
    "Require -Condition ((Compare-Object $expectedPickupSnapshot $pickupSnapshot).Count -eq 0) -Message 'pickup remaining ground IDs do not conserve the exact original set'")
set(_task9_pickup_reorder_site
    "${_task9_pickup_derivation}\n${_task9_pickup_top_level}")
string(REPLACE "${_task9_pickup_reorder_site}"
    "${_task9_pickup_top_level}\n${_task9_pickup_derivation}"
    _task9_pickup_reordered "${_validator_text}")
if(_task9_pickup_reordered STREQUAL _validator_text)
    message(FATAL_ERROR "pickup derivation reorder mutation anchor disappeared")
endif()
set(_path "${GUARD_TEST_ROOT}/pickup-derivation-reordered.ps1")
file(WRITE "${_path}" "${_task9_pickup_reordered}")
expect_rejected("pickup derivation cannot move after its Require" VALIDATOR "${_path}"
    "requires pickup conservation derivation immediately before its Require")

if(DEFINED ROUND3_TARGETED_ONLY AND ROUND3_TARGETED_ONLY)
    message(STATUS
        "Stage11D Task9 round3 targeted evidence guard passed: bad_mutations=4")
    return()
endif()

set(_task9_pickup_false_branch
    "if ($false) {\n    ${_task9_pickup_top_level}\n}")
string(REPLACE "${_task9_pickup_top_level}" "${_task9_pickup_false_branch}"
    _task9_pickup_relocated "${_validator_text}")
if(_task9_pickup_relocated STREQUAL _validator_text)
    message(FATAL_ERROR "pickup relocation mutation anchor disappeared")
endif()
set(_path "${GUARD_TEST_ROOT}/pickup-relocated-false-branch.ps1")
file(WRITE "${_path}" "${_task9_pickup_relocated}")
expect_rejected("pickup conservation cannot move into a false branch" VALIDATOR "${_path}"
    "requires exact pickup conservation in a top-level executable Require command")

set(_task9_actual_drop_count "$defeated -ge 3")
string(REPLACE "${_task9_actual_drop_count}"
    "$defeated -ge [uint32]$manifest.ordinary_prefix_kills"
    _task9_prefix_kills_required "${_validator_text}")
if(_task9_prefix_kills_required STREQUAL _validator_text)
    message(FATAL_ERROR "actual-drop-count mutation anchor disappeared")
endif()
set(_path "${GUARD_TEST_ROOT}/prefix-kills-required.ps1")
file(WRITE "${_path}" "${_task9_prefix_kills_required}")
expect_rejected("fixture prefix length cannot become a required kill count" VALIDATOR "${_path}"
    "forbids fixture prefix length as a formal kill requirement")

file(READ "${_report}" _task9_report_text)
set(_task9_player_damage_observation
    "state.player_damage_observed = state.player_damage_observed\n            || (state.player_hp_sampled\n                && current_max_hp == state.player_max_hp\n                && current_hp < state.player_hp);")
string(REPLACE "${_task9_player_damage_observation}"
    "state.player_damage_observed = false;"
    _task9_player_damage_removed "${_task9_report_text}")
if(_task9_player_damage_removed STREQUAL _task9_report_text)
    message(FATAL_ERROR "player-damage observation mutation anchor disappeared")
endif()
set(_path "${GUARD_TEST_ROOT}/player-damage-observation-removed.cpp")
file(WRITE "${_path}" "${_task9_player_damage_removed}")
expect_rejected("player damage observation cannot be removed" REPORT "${_path}"
    "requires consecutive live player HP sampling")

string(REPLACE "                && current_hp < state.player_hp);"
    "                && current_hp > 0 && current_hp < current_max_hp);"
    _task9_player_damage_below_max "${_task9_report_text}")
if(_task9_player_damage_below_max STREQUAL _task9_report_text)
    message(FATAL_ERROR
        "player-damage below-max mutation anchor disappeared")
endif()
set(_path "${GUARD_TEST_ROOT}/player-damage-below-max.cpp")
file(WRITE "${_path}" "${_task9_player_damage_below_max}")
expect_rejected(
    "player damage observation cannot use a below-max snapshot"
    REPORT "${_path}"
    "requires consecutive live player HP sampling")

set(_task9_rare_label_binding [=[                    && stage11d_view_has_abyss_ordinal(
                        view, item.ordinal)) {]=])
string(REPLACE "${_task9_rare_label_binding}"
    "                    && view.count != 0U) {"
    _task9_rare_label_unbound "${_task9_report_text}")
if(_task9_rare_label_unbound STREQUAL _task9_report_text)
    message(FATAL_ERROR "rare-label binding mutation anchor disappeared")
endif()
set(_path "${GUARD_TEST_ROOT}/rare-label-unbound.cpp")
file(WRITE "${_path}" "${_task9_rare_label_unbound}")
expect_rejected(
    "rare capture must use the matching abyss label ordinal"
    REPORT "${_path}"
    "requires rare capture to bind the matching abyss label")

string(REPLACE "                && view.labels[index].abyss) return true;"
    "                ) return true;"
    _task9_abyss_label_flag_removed "${_task9_report_text}")
if(_task9_abyss_label_flag_removed STREQUAL _task9_report_text)
    message(FATAL_ERROR "abyss-label flag mutation anchor disappeared")
endif()
set(_path "${GUARD_TEST_ROOT}/abyss-label-flag-removed.cpp")
file(WRITE "${_path}" "${_task9_abyss_label_flag_removed}")
expect_rejected(
    "abyss ordinal helper must require an abyss label"
    REPORT "${_path}"
    "requires an abyss label with the same ordinal")

set(_task9_rarity_tuple_check
    "Require (-not ($ordinaryTupleChecks -contains $false)) 'production rarity tuples drifted'")
string(REPLACE "${_task9_rarity_tuple_check}"
    "Require ($true) 'production rarity tuples drifted'"
    _task9_rarity_tuple_weakened "${_validator_text}")
if(_task9_rarity_tuple_weakened STREQUAL _validator_text)
    message(FATAL_ERROR "rarity-tuple mutation anchor disappeared")
endif()
set(_path "${GUARD_TEST_ROOT}/rarity-tuples-weakened.ps1")
file(WRITE "${_path}" "${_task9_rarity_tuple_weakened}")
expect_rejected("per-rarity tuple check cannot be weakened" VALIDATOR "${_path}"
    "requires exact per-rarity tuples")

set(_task9_pickup_conservation_check
    "Require -Condition ((Compare-Object $expectedPickupSnapshot $pickupSnapshot).Count -eq 0) -Message 'pickup remaining ground IDs do not conserve the exact original set'")
string(REPLACE "${_task9_pickup_conservation_check}"
    "Require -Condition ($pickupSnapshot.Count -eq 2) -Message 'pickup remaining ground IDs do not conserve the exact original set'"
    _task9_pickup_conservation_weakened "${_validator_text}")
if(_task9_pickup_conservation_weakened STREQUAL _validator_text)
    message(FATAL_ERROR "pickup-conservation mutation anchor disappeared")
endif()
set(_path "${GUARD_TEST_ROOT}/pickup-conservation-weakened.ps1")
file(WRITE "${_path}" "${_task9_pickup_conservation_weakened}")
expect_rejected("pickup conservation cannot be reduced to a count" VALIDATOR "${_path}"
    "requires exact pickup conservation")

if(DEFINED TASK9_TARGETED_ONLY AND TASK9_TARGETED_ONLY)
    message(STATUS
        "Stage11D Task9 targeted evidence guard passed: bad_mutations=18")
    return()
endif()

set(_mutated "${_validator_text}")
string(REPLACE "LastWriteTimeUtc" "CreationTimeUtc" _mutated "${_mutated}")
if(_mutated STREQUAL _validator_text)
    message(FATAL_ERROR "stale-validator mutation made no change")
endif()
set(_path "${GUARD_TEST_ROOT}/stale-validator.ps1")
file(WRITE "${_path}" "${_mutated}")
expect_rejected("stale PNG acceptance" VALIDATOR "${_path}"
    "missing semantic check: LastWriteTimeUtc")

set(_mutated "${_validator_text}")
set(_site "Require ($hashes.Add([string]$hash)) \"duplicate screenshot hash: $name\"")
string(FIND "${_validator_text}" "${_site}" _site_index)
if(_site_index EQUAL -1)
    message(FATAL_ERROR "duplicate-validator mutation site disappeared")
endif()
string(REPLACE "${_site}" "[void]$hashes.Add([string]$hash)"
    _mutated "${_validator_text}")
if(_mutated STREQUAL _validator_text)
    message(FATAL_ERROR "duplicate-validator mutation made no change")
endif()
set(_path "${GUARD_TEST_ROOT}/duplicate-validator.ps1")
file(WRITE "${_path}" "${_mutated}")
expect_rejected("duplicate PNG acceptance" VALIDATOR "${_path}"
    "missing semantic check: duplicate")

set(_mutated "${_validator_text}")
set(_item_feature_site
    "$feature = Measure-Region $bitmap $rect.X $rect.Y $rect.W $rect.H")
set(_notice_feature_site
    "$feature = Measure-Region $bitmap $notice[0] $notice[1] $notice[2] $notice[3]")
string(FIND "${_validator_text}" "${_item_feature_site}" _item_site_index)
string(FIND "${_validator_text}" "${_notice_feature_site}" _notice_site_index)
if(_item_site_index EQUAL -1 OR _notice_site_index EQUAL -1)
    message(FATAL_ERROR "existence-only mutation site disappeared")
endif()
string(REPLACE "${_item_feature_site}"
    "$feature = [pscustomobject]@{ Colors = 99; Bright = 99; Dark = 99 } #"
    _mutated "${_validator_text}")
string(REPLACE "${_notice_feature_site}"
    "$feature = [pscustomobject]@{ Colors = 99; Bright = 99; Dark = 99 } #"
    _mutated "${_mutated}")
if(_mutated STREQUAL _validator_text)
    message(FATAL_ERROR "existence-only mutation made no change")
endif()
set(_path "${GUARD_TEST_ROOT}/existence-only-validator.ps1")
file(WRITE "${_path}" "${_mutated}")
expect_rejected("existence-only validation" VALIDATOR "${_path}"
    "missing semantic check: $feature")

task7c_run_m24_m25_cases()

set(_task7c_ground_gate
    "if (impl_->states.stage11d.target_visible\n            && !impl_->states.stage11d.captured) {")
set(_task7c_ground_block
    "${_task7c_ground_gate}\n        host_validation::stage11d_record_semantics(\n            impl_->states.stage11d, snapshot, ownership,\n            ground_loot_view, notices);\n    }")
foreach(_task7c_ground_mutation IN ITEMS
        "if (!impl_->states.stage11d.captured) {"
        "if (impl_->states.stage11d.target_visible) {")
    string(REPLACE "${_task7c_ground_gate}" "${_task7c_ground_mutation}"
        _task7c_ground_runtime "${_task7b_host_validation_runtime_text}")
    if(_task7c_ground_runtime STREQUAL _task7b_host_validation_runtime_text)
        message(FATAL_ERROR "Task7C ground semantic gate mutation site disappeared")
    endif()
    string(MD5 _task7c_ground_slug "${_task7c_ground_mutation}")
    set(_path "${GUARD_TEST_ROOT}/task7c-ground-${_task7c_ground_slug}.cpp")
    file(WRITE "${_path}" "${_task7c_ground_runtime}")
    expect_rejected("task7c ground semantic recording gate"
        HOST_VALIDATION_RUNTIME "${_path}"
        "target-visible uncaptured semantic recording")
endforeach()
foreach(_task7c_ground_scope IN ITEMS lambda dead)
    if(_task7c_ground_scope STREQUAL "lambda")
        set(_task7c_ground_replacement
            "const auto ground_semantics_decoy = [&] { ${_task7c_ground_block} };")
    else()
        set(_task7c_ground_replacement
            "if (false) { ${_task7c_ground_block} }")
    endif()
    string(REPLACE "${_task7c_ground_block}"
        "${_task7c_ground_replacement}" _task7c_ground_scoped_runtime
        "${_task7b_host_validation_runtime_text}")
    if(_task7c_ground_scoped_runtime STREQUAL
            _task7b_host_validation_runtime_text)
        message(FATAL_ERROR
            "Task7C ground ${_task7c_ground_scope} mutation made no change")
    endif()
    set(_path
        "${GUARD_TEST_ROOT}/task7c-ground-${_task7c_ground_scope}.cpp")
    file(WRITE "${_path}" "${_task7c_ground_scoped_runtime}")
    expect_rejected("task7c ground semantic ${_task7c_ground_scope} scope"
        HOST_VALIDATION_RUNTIME "${_path}"
        "target-visible uncaptured semantic recording")
endforeach()

string(REGEX MATCH
    "impl_->([A-Za-z_][A-Za-z0-9_]*)[ \t\r\n]*=[ \t\r\n]*impl_->states[.]stage11d[.]target_visible;"
    _task7c_pending_assignment "${_task7b_host_validation_runtime_text}")
if("${_task7c_pending_assignment}" STREQUAL "")
    message(FATAL_ERROR "Task7C Stage11D pending capture anchor is missing")
endif()
set(_task7c_pending_field "${CMAKE_MATCH_1}")
set(_task7c_capture_gate
    "if (impl_->${_task7c_pending_field}) {\n        impl_->states.stage11d.captured = true;\n    }")
string(REPLACE
    "${_task7c_pending_assignment}"
    "impl_->${_task7c_pending_field} = false;"
    _task7c_pending_runtime "${_task7b_host_validation_runtime_text}")
if(_task7c_pending_runtime STREQUAL _task7b_host_validation_runtime_text)
    message(FATAL_ERROR "Task7C Stage11D pending capture mutation made no change")
endif()
set(_path "${GUARD_TEST_ROOT}/task7c-stage11d-pending-publication.cpp")
file(WRITE "${_path}" "${_task7c_pending_runtime}")
expect_rejected("task7c Stage11D pending capture publication"
    HOST_VALIDATION_RUNTIME "${_path}"
    "successful pending capture promotion")
string(REPLACE "${_task7c_pending_assignment}"
    "const auto pending_loot_decoy = [&] { ${_task7c_pending_assignment} };"
    _task7c_pending_lambda "${_task7b_host_validation_runtime_text}")
if(_task7c_pending_lambda STREQUAL _task7b_host_validation_runtime_text)
    message(FATAL_ERROR "Task7C pending capture lambda mutation made no change")
endif()
set(_path "${GUARD_TEST_ROOT}/task7c-stage11d-pending-lambda.cpp")
file(WRITE "${_path}" "${_task7c_pending_lambda}")
expect_rejected("task7c Stage11D pending capture lambda"
    HOST_VALIDATION_RUNTIME "${_path}"
    "successful pending capture promotion")

string(REPLACE
    "${_task7c_capture_gate}"
    "impl_->states.stage11d.captured = true;"
    _task7c_capture_runtime "${_task7b_host_validation_runtime_text}")
if(_task7c_capture_runtime STREQUAL _task7b_host_validation_runtime_text)
    message(FATAL_ERROR "Task7C Stage11D capture gate mutation made no change")
endif()
set(_path "${GUARD_TEST_ROOT}/task7c-stage11d-capture-gate.cpp")
file(WRITE "${_path}" "${_task7c_capture_runtime}")
expect_rejected("task7c Stage11D capture result gate"
    HOST_VALIDATION_RUNTIME "${_path}"
    "successful pending capture promotion")
string(REPLACE "${_task7c_capture_gate}"
    "if (false) { ${_task7c_capture_gate} }"
    _task7c_capture_dead "${_task7b_host_validation_runtime_text}")
if(_task7c_capture_dead STREQUAL _task7b_host_validation_runtime_text)
    message(FATAL_ERROR "Task7C capture dead-branch mutation made no change")
endif()
set(_path "${GUARD_TEST_ROOT}/task7c-stage11d-capture-dead.cpp")
file(WRITE "${_path}" "${_task7c_capture_dead}")
expect_rejected("task7c Stage11D capture dead branch"
    HOST_VALIDATION_RUNTIME "${_path}"
    "successful pending capture promotion")

string(REPLACE "void HostValidationRuntime::observe_ground_loot("
    "#if 0\nvoid HostValidationRuntime::observe_ground_loot() {}\n#endif\nvoid HostValidationRuntime::observe_ground_loot("
    _task7c_inactive_facade "${_task7b_host_validation_runtime_text}")
if(_task7c_inactive_facade STREQUAL _task7b_host_validation_runtime_text)
    message(FATAL_ERROR "Task7C inactive facade mutation made no change")
endif()
set(_path "${GUARD_TEST_ROOT}/task7c-inactive-facade.cpp")
file(WRITE "${_path}" "${_task7c_inactive_facade}")
expect_guard_accepted("Stage11D evidence guard inactive facade decoy"
    "${_guard}" HOST_VALIDATION_RUNTIME "${_path}")
string(REPLACE "${_task7c_pending_field}" "pending_loot_capture_ready"
    _task7c_pending_rename "${_task7b_host_validation_runtime_text}")
if(_task7c_pending_rename STREQUAL _task7b_host_validation_runtime_text)
    message(FATAL_ERROR "Task7C Stage11D pending field rename made no change")
endif()
set(_path "${GUARD_TEST_ROOT}/task7c-stage11d-pending-rename.cpp")
file(WRITE "${_path}" "${_task7c_pending_rename}")
expect_guard_accepted("Stage11D evidence guard pending field rename"
    "${_guard}" HOST_VALIDATION_RUNTIME "${_path}")

if(DEFINED TASK5A_TARGETED_ONLY AND TASK5A_TARGETED_ONLY)
    message(STATUS
        "Stage11D Task5A targeted guard test passed: bad_mutations=5; harmless_decoys=2")
else()
    message(STATUS
        "Stage11D loot evidence guard self-test passed: bad_mutations=83; harmless_decoys=4")
endif()
