if(NOT DEFINED SOURCE_ROOT)
    message(FATAL_ERROR "SOURCE_ROOT is required")
endif()

include("${CMAKE_CURRENT_LIST_DIR}/cpp_source_lexer.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/../dungeon/evidence_source_scan.cmake")

set(_header "${SOURCE_ROOT}/src/platform/raylib/raylib_host.hpp")
set(_host "${SOURCE_ROOT}/src/platform/raylib/raylib_host.cpp")
get_filename_component(_host_source_dir "${_host}" DIRECTORY)
set(_host_validation_runtime
    "${_host_source_dir}/host_validation_runtime.cpp")
set(_stage_header
    "${SOURCE_ROOT}/src/platform/raylib/host_validation_stage11d.hpp")
set(_runtime
    "${SOURCE_ROOT}/src/platform/raylib/host_validation_stage11d_runtime.cpp")
set(_report
    "${SOURCE_ROOT}/src/platform/raylib/host_validation_stage11d_report.cpp")
set(_renderer "${SOURCE_ROOT}/src/platform/raylib/combat_renderer.cpp")
set(_formal "${SOURCE_ROOT}/tests/platform/stage11d_loot_formal_game_validation.cpp")
set(_validator "${SOURCE_ROOT}/tests/platform/stage11d_loot_formal_validator.ps1")
if(DEFINED HEADER_OVERRIDE)
    set(_header "${HEADER_OVERRIDE}")
endif()
if(DEFINED HOST_OVERRIDE)
    set(_host "${HOST_OVERRIDE}")
endif()
if(DEFINED HOST_VALIDATION_RUNTIME_OVERRIDE)
    set(_host_validation_runtime "${HOST_VALIDATION_RUNTIME_OVERRIDE}")
endif()
if(DEFINED STAGE11D_HEADER_OVERRIDE)
    set(_stage_header "${STAGE11D_HEADER_OVERRIDE}")
endif()
if(DEFINED RUNTIME_OVERRIDE)
    set(_runtime "${RUNTIME_OVERRIDE}")
endif()
if(DEFINED REPORT_OVERRIDE)
    set(_report "${REPORT_OVERRIDE}")
endif()
if(DEFINED RENDERER_OVERRIDE)
    set(_renderer "${RENDERER_OVERRIDE}")
endif()
if(DEFINED FORMAL_OVERRIDE)
    set(_formal "${FORMAL_OVERRIDE}")
endif()
if(DEFINED VALIDATOR_OVERRIDE)
    set(_validator "${VALIDATOR_OVERRIDE}")
endif()
foreach(_file IN ITEMS "${_header}" "${_host}" "${_host_validation_runtime}"
        "${_stage_header}" "${_runtime}" "${_report}" "${_renderer}"
        "${_formal}" "${_validator}")
    if(NOT EXISTS "${_file}")
        message(FATAL_ERROR "Stage11D loot evidence input is missing: ${_file}")
    endif()
endforeach()

file(READ "${_header}" _header_text)
file(READ "${_host}" _host_text)
file(READ "${_host_validation_runtime}" _host_validation_runtime_text)
file(READ "${_stage_header}" _stage_header_text)
file(READ "${_runtime}" _runtime_text)
file(READ "${_report}" _report_text)
file(READ "${_renderer}" _renderer_text)
file(READ "${_formal}" _formal_text)
file(READ "${_validator}" _validator_text)
set(_combined
    "${_header_text}\n${_stage_header_text}\n${_runtime_text}\n${_report_text}\n${_host_text}\n${_formal_text}")

function(stage11d_count_raw_token SOURCE TOKEN OUT_COUNT)
    string(LENGTH "${SOURCE}" _source_length)
    string(LENGTH "${TOKEN}" _token_length)
    string(REPLACE "${TOKEN}" "" _without "${SOURCE}")
    string(LENGTH "${_without}" _without_length)
    math(EXPR _removed "${_source_length} - ${_without_length}")
    math(EXPR _count "${_removed} / ${_token_length}")
    set(${OUT_COUNT} ${_count} PARENT_SCOPE)
endfunction()

function(stage11d_fold_cpp_phase2_splices SOURCE OUT_SOURCE)
    string(ASCII 92 _backslash)
    string(ASCII 13 _carriage_return)
    string(ASCII 10 _line_feed)
    set(_folded "${SOURCE}")
    string(REPLACE "${_backslash}${_carriage_return}${_line_feed}" ""
        _folded "${_folded}")
    string(REPLACE "${_backslash}${_line_feed}" ""
        _folded "${_folded}")
    set(${OUT_SOURCE} "${_folded}" PARENT_SCOPE)
endfunction()

# Prove the CMake byte construction for both C++ phase-2 newline forms before
# relying on the cheap complete-Host inventory below.
string(ASCII 92 _stage11d_probe_backslash)
string(ASCII 13 _stage11d_probe_carriage_return)
string(ASCII 10 _stage11d_probe_line_feed)
set(_stage11d_probe_expected
    "validation_runtime->inject_physical_edges(")
set(_stage11d_probe_lf
    "validation_runtime->inject_phy${_stage11d_probe_backslash}${_stage11d_probe_line_feed}sical_edges(")
set(_stage11d_probe_crlf
    "validation_runtime->inject_phy${_stage11d_probe_backslash}${_stage11d_probe_carriage_return}${_stage11d_probe_line_feed}sical_edges(")
stage11d_fold_cpp_phase2_splices("${_stage11d_probe_lf}"
    _stage11d_probe_lf_folded)
stage11d_fold_cpp_phase2_splices("${_stage11d_probe_crlf}"
    _stage11d_probe_crlf_folded)
if(NOT _stage11d_probe_lf_folded STREQUAL _stage11d_probe_expected
        OR NOT _stage11d_probe_crlf_folded STREQUAL
            _stage11d_probe_expected)
    message(FATAL_ERROR
        "Stage11D loot evidence guard cannot fold C++ phase-2 line splices")
endif()

# Task 7A's facade runtime ownership is unconditional. Fold C++ phase-2
# splices and remove complete conditional regions so an inactive exact leg
# cannot satisfy the Stage11D owner binding.
function(stage11d_unconditional_cpp_surface SOURCE OUT_SURFACE)
    arpg_sanitize_cpp_source("${SOURCE}" _logical_source)
    if(ARGC GREATER 2)
        set(${ARGV2} "${_logical_source}" PARENT_SCOPE)
    endif()
    string(LENGTH "${_logical_source}" _source_length)
    set(_cursor 0)
    set(_conditional_depth 0)
    set(_surface "")
    while(_cursor LESS _source_length)
        string(SUBSTRING "${_logical_source}" ${_cursor} -1 _tail)
        string(FIND "${_tail}" "\n" _newline)
        if(_newline EQUAL -1)
            set(_line "${_tail}")
            set(_line_length -1)
        else()
            math(EXPR _line_length "${_newline} + 1")
            string(SUBSTRING "${_tail}" 0 ${_line_length} _line)
        endif()

        if(_line MATCHES
                "^[ \t]*#[ \t]*(if|ifdef|ifndef)([ \t\r\n(]|$)")
            math(EXPR _conditional_depth "${_conditional_depth} + 1")
        elseif(_line MATCHES "^[ \t]*#[ \t]*endif([ \t\r\n]|$)")
            math(EXPR _conditional_depth "${_conditional_depth} - 1")
            if(_conditional_depth LESS 0)
                message(FATAL_ERROR
                    "Stage11D facade input conditional is unbalanced")
            endif()
        elseif(_conditional_depth EQUAL 0)
            string(APPEND _surface "${_line}")
        endif()

        if(_newline EQUAL -1)
            break()
        endif()
        math(EXPR _cursor "${_cursor} + ${_line_length}")
    endwhile()
    if(NOT _conditional_depth EQUAL 0)
        message(FATAL_ERROR
            "Stage11D facade input conditional is unbalanced")
    endif()
    set(${OUT_SURFACE} "${_surface}" PARENT_SCOPE)
endfunction()

# Replacing the complete `// marker` with an identifier before sanitizing is
# deliberate: a real line-comment marker becomes code, while a marker hidden
# in a comment/string/raw-string remains non-code and disappears.
function(stage11d_prepare_marker_surface SOURCE PREFIX LABELS OUT_CODE)
    set(_marked "${SOURCE}")
    foreach(_label IN LISTS LABELS)
        foreach(_kind IN ITEMS BEGIN END)
            set(_marker
                "// STAGE11D_LOOT_VALIDATION_SEAM_${_kind} ${_label}")
            stage11d_count_raw_token("${_marked}" "${_marker}" _count)
            if(NOT _count EQUAL 1)
                message(FATAL_ERROR
                    "Stage11D loot evidence guard cannot bind ${PREFIX} ${_label} seam marker")
            endif()
            set(_token "TASK5A_${PREFIX}_${_label}_${_kind}_MARKER")
            string(REPLACE "${_marker}" "${_token}" _marked "${_marked}")
        endforeach()
    endforeach()
    evidence_sanitize_cpp_for_scan("${_marked}" _code)
    foreach(_label IN LISTS LABELS)
        foreach(_kind IN ITEMS BEGIN END)
            set(_token "TASK5A_${PREFIX}_${_label}_${_kind}_MARKER")
            stage11d_count_raw_token("${_code}" "${_token}" _count)
            if(NOT _count EQUAL 1)
                message(FATAL_ERROR
                    "Stage11D loot evidence guard cannot bind ${PREFIX} ${_label} seam marker")
            endif()
        endforeach()
    endforeach()
    set(${OUT_CODE} "${_code}" PARENT_SCOPE)
endfunction()

function(stage11d_code_brace_depth SURFACE POSITION OUT_DEPTH)
    if(POSITION EQUAL 0)
        set(${OUT_DEPTH} 0 PARENT_SCOPE)
        return()
    endif()
    string(SUBSTRING "${SURFACE}" 0 ${POSITION} _prefix)
    string(REGEX REPLACE "[^{}]" "" _braces "${_prefix}")
    string(LENGTH "${_braces}" _length)
    set(_depth 0)
    if(_length GREATER 0)
        math(EXPR _last "${_length} - 1")
        foreach(_index RANGE 0 ${_last})
            string(SUBSTRING "${_braces}" ${_index} 1 _brace)
            if(_brace STREQUAL "{")
                math(EXPR _depth "${_depth} + 1")
            else()
                math(EXPR _depth "${_depth} - 1")
            endif()
        endforeach()
    endif()
    set(${OUT_DEPTH} ${_depth} PARENT_SCOPE)
endfunction()

function(stage11d_require_marker_depth SURFACE PREFIX LABEL EXPECTED_DEPTH)
    foreach(_kind IN ITEMS BEGIN END)
        set(_token "TASK5A_${PREFIX}_${LABEL}_${_kind}_MARKER")
        string(FIND "${SURFACE}" "${_token}" _position)
        if(_position EQUAL -1)
            message(FATAL_ERROR
                "Stage11D loot evidence guard cannot bind ${PREFIX} ${LABEL} seam marker")
        endif()
        stage11d_code_brace_depth("${SURFACE}" ${_position} _depth)
        if(NOT _depth EQUAL EXPECTED_DEPTH)
            message(FATAL_ERROR
                "Stage11D loot evidence guard rejected ${PREFIX} ${LABEL} seam scope")
        endif()
    endforeach()
endfunction()

function(stage11d_extract_marker_region SURFACE PREFIX LABEL OUT_REGION)
    set(_begin "TASK5A_${PREFIX}_${LABEL}_BEGIN_MARKER")
    set(_end "TASK5A_${PREFIX}_${LABEL}_END_MARKER")
    string(FIND "${SURFACE}" "${_begin}" _begin_at)
    string(FIND "${SURFACE}" "${_end}" _end_at)
    if(_begin_at EQUAL -1 OR _end_at EQUAL -1 OR NOT _begin_at LESS _end_at)
        message(FATAL_ERROR
            "Stage11D loot evidence guard cannot isolate ${PREFIX} ${LABEL} seam")
    endif()
    string(LENGTH "${_end}" _end_length)
    math(EXPR _length "${_end_at} - ${_begin_at} + ${_end_length}")
    string(SUBSTRING "${SURFACE}" ${_begin_at} ${_length} _region)
    set(${OUT_REGION} "${_region}" PARENT_SCOPE)
endfunction()

function(stage11d_extract_raw_seam SOURCE LABEL OUT_REGION)
    set(_begin "// STAGE11D_LOOT_VALIDATION_SEAM_BEGIN ${LABEL}")
    set(_end "// STAGE11D_LOOT_VALIDATION_SEAM_END ${LABEL}")
    string(FIND "${SOURCE}" "${_begin}" _begin_at)
    string(FIND "${SOURCE}" "${_end}" _end_at)
    string(LENGTH "${_end}" _end_length)
    math(EXPR _length "${_end_at} - ${_begin_at} + ${_end_length}")
    string(SUBSTRING "${SOURCE}" ${_begin_at} ${_length} _region)
    set(${OUT_REGION} "${_region}" PARENT_SCOPE)
endfunction()

function(stage11d_validate_facade_input_owner ACTIVE_SURFACE LEXICAL_SURFACE)
    set(_signature
        "PhysicalKeySnapshot HostValidationRuntime::inject_physical_edges(")
    stage11d_count_raw_token("${ACTIVE_SURFACE}" "${_signature}"
        _active_signature_count)
    stage11d_count_raw_token("${LEXICAL_SURFACE}" "${_signature}"
        _lexical_signature_count)
    if(NOT _active_signature_count EQUAL 1
            OR NOT _lexical_signature_count EQUAL 1)
        message(FATAL_ERROR
            "Stage11D loot evidence guard rejected host input call binding")
    endif()
    string(FIND "${ACTIVE_SURFACE}" "${_signature}" _signature_position)
    stage11d_code_brace_depth("${ACTIVE_SURFACE}" ${_signature_position}
        _signature_depth)
    if(NOT _signature_depth EQUAL 1)
        message(FATAL_ERROR
            "Stage11D loot evidence guard rejected host input call binding")
    endif()

    evidence_find_cpp_function_bounds_in_sanitized(
        "${ACTIVE_SURFACE}" "${_signature}"
        _function_begin _function_open _function_end)
    math(EXPR _function_length
        "${_function_end} - ${_function_begin} + 1")
    string(SUBSTRING "${ACTIVE_SURFACE}" ${_function_begin}
        ${_function_length} _function)
    set(_stage11d_call
        "host_validation::inject_stage11d_physical_edges(")
    stage11d_count_raw_token("${_function}" "${_stage11d_call}"
        _stage11d_call_count)
    string(FIND "${_function}" "${_stage11d_call}"
        _stage11d_call_position)
    if(NOT _stage11d_call_count EQUAL 1
            OR _stage11d_call_position EQUAL -1)
        message(FATAL_ERROR
            "Stage11D loot evidence guard rejected host input call binding")
    endif()
    stage11d_code_brace_depth("${_function}" ${_stage11d_call_position}
        _stage11d_call_depth)
    if(NOT _stage11d_call_depth EQUAL 1)
        message(FATAL_ERROR
            "Stage11D loot evidence guard rejected host input call binding")
    endif()
    string(REGEX REPLACE "[ \t\r\n]+" "" _normalized "${_function}")
    string(FIND "${_normalized}"
        "constPhysicalKeySnapshotstage11d_physical_keys=host_validation::inject_stage11d_physical_edges(stage11c_physical_keys,*impl_->config,input_settings,dungeon_snapshot,impl_->states.stage11d);"
        _binding)
    if(_binding EQUAL -1)
        message(FATAL_ERROR
            "Stage11D loot evidence guard rejected host input call binding")
    endif()
endfunction()

stage11d_unconditional_cpp_surface("${_host_validation_runtime_text}"
    _host_validation_runtime_code _host_validation_runtime_lexical_code)
stage11d_validate_facade_input_owner("${_host_validation_runtime_code}"
    "${_host_validation_runtime_lexical_code}")
if(DEFINED STAGE11D_INPUT_OWNER_ONLY AND STAGE11D_INPUT_OWNER_ONLY)
    message(STATUS "Stage11D facade runtime input-owner guard passed")
    return()
endif()

set(_host_facade_input_token
    "validation_runtime->inject_physical_edges(")
stage11d_fold_cpp_phase2_splices("${_host_text}" _host_phase2_text)
stage11d_count_raw_token("${_host_phase2_text}"
    "${_host_facade_input_token}" _host_raw_input_count)
# The production Host has one raw occurrence. Its active scope, exact
# arguments and ordering are proven again by the much smaller input crop
# below. Only ambiguous raw inventories pay for the full 79 KiB lexical and
# unconditional scans, which distinguish real/inactive duplicates from
# harmless comment, string and raw-string decoys.
if(NOT _host_raw_input_count EQUAL 1)
    stage11d_unconditional_cpp_surface("${_host_text}"
        _host_active_code _host_lexical_code)
    stage11d_count_raw_token("${_host_active_code}"
        "${_host_facade_input_token}" _host_active_input_count)
    stage11d_count_raw_token("${_host_lexical_code}"
        "${_host_facade_input_token}" _host_lexical_input_count)
    if(NOT _host_active_input_count EQUAL 1
            OR NOT _host_lexical_input_count EQUAL 1)
        message(FATAL_ERROR
            "Stage11D loot evidence guard rejected host input call binding")
    endif()
endif()

set(_stage_header_labels state)
stage11d_prepare_marker_surface("${_stage_header_text}" stage_header
    "${_stage_header_labels}" _stage_header_code)
stage11d_require_marker_depth("${_stage_header_code}" stage_header state 2)

set(_runtime_labels selectors safe_movement physical_driver fixed_step_runtime)
stage11d_prepare_marker_surface("${_runtime_text}" runtime
    "${_runtime_labels}" _runtime_code)
stage11d_require_marker_depth("${_runtime_code}" runtime selectors 1)
stage11d_require_marker_depth("${_runtime_code}" runtime safe_movement 2)
stage11d_require_marker_depth("${_runtime_code}" runtime physical_driver 1)
stage11d_require_marker_depth("${_runtime_code}" runtime fixed_step_runtime 1)

set(_report_labels evidence_semantics)
stage11d_prepare_marker_surface("${_report_text}" report
    "${_report_labels}" _report_code)
stage11d_require_marker_depth("${_report_code}" report evidence_semantics 1)

set(_host_labels foreground runtime_state
    fixed_step abyss_claim presented_semantics reached_merge reached
    visible_capture captured summary)
string(REPLACE "\r\n" "\n" _host_marker_count_text "${_host_text}")
foreach(_label IN LISTS _host_labels)
    foreach(_kind IN ITEMS BEGIN END)
        set(_marker
            "// STAGE11D_LOOT_VALIDATION_SEAM_${_kind} ${_label}")
        stage11d_count_raw_token("${_host_marker_count_text}"
            "${_marker}\n" _count)
        if(NOT _count EQUAL 1)
            message(FATAL_ERROR
                "Stage11D loot evidence guard cannot bind host ${_label} seam marker")
        endif()
    endforeach()
endforeach()

string(FIND "${_host_text}"
    "const PhysicalKeySnapshot sampled_physical_keys = sample_physical_keys();"
    _host_input_begin)
if(_host_input_begin EQUAL -1)
    message(FATAL_ERROR
        "Stage11D loot evidence guard cannot isolate host input chain")
endif()
string(SUBSTRING "${_host_text}" ${_host_input_begin} -1 _host_input_tail)
string(FIND "${_host_input_tail}"
    "core::FixedStepFrame frame = host_gate.fixed_step;" _host_input_end)
if(_host_input_end EQUAL -1)
    message(FATAL_ERROR
        "Stage11D loot evidence guard cannot isolate host input chain")
endif()
string(SUBSTRING "${_host_input_tail}" 0 ${_host_input_end}
    _host_input_crop)
stage11d_unconditional_cpp_surface("${_host_input_crop}" _host_input_code)

string(FIND "${_host_text}"
    "core::FixedStepFrame frame = host_gate.fixed_step;"
    _host_fixed_step_begin)
if(_host_fixed_step_begin EQUAL -1)
    message(FATAL_ERROR
        "Stage11D loot evidence guard cannot isolate host fixed-step chain")
endif()
string(SUBSTRING "${_host_text}" ${_host_fixed_step_begin} -1
    _host_fixed_step_tail)
string(FIND "${_host_fixed_step_tail}"
    "if (current.death.has_value()) {" _host_fixed_step_end)
if(_host_fixed_step_end EQUAL -1)
    message(FATAL_ERROR
        "Stage11D loot evidence guard cannot isolate host fixed-step chain")
endif()
string(SUBSTRING "${_host_fixed_step_tail}" 0 ${_host_fixed_step_end}
    _host_fixed_step_crop)
set(_host_fixed_step_labels fixed_step abyss_claim)
stage11d_prepare_marker_surface("${_host_fixed_step_crop}" host
    "${_host_fixed_step_labels}" _host_fixed_step_code)
stage11d_require_marker_depth("${_host_fixed_step_code}" host fixed_step 1)
stage11d_require_marker_depth("${_host_fixed_step_code}" host abyss_claim 1)

set(_stage11d_host_seam "")
foreach(_label IN LISTS _host_labels)
    stage11d_extract_raw_seam("${_host_text}" "${_label}" _region)
    string(APPEND _stage11d_host_seam "\n${_region}")
endforeach()
stage11d_extract_raw_seam("${_report_text}" evidence_semantics
    _stage11d_report_seam)
set(_stage11d_semantic_seam
    "${_stage11d_host_seam}\n${_stage11d_report_seam}")
arpg_sanitize_cpp_source("${_stage11d_host_seam}" _stage11d_host_code)
arpg_sanitize_cpp_source("${_stage11d_semantic_seam}"
    _stage11d_semantic_code)

function(stage11d_extract_runtime_definition LABEL SIGNATURE OUT_FUNCTION)
    stage11d_count_raw_token("${_runtime_code}" "${SIGNATURE}" _count)
    if(NOT _count EQUAL 1)
        message(FATAL_ERROR
            "Stage11D loot evidence guard missing runtime ${LABEL} definition")
    endif()
    string(FIND "${_runtime_code}" "${SIGNATURE}" _begin)
    string(SUBSTRING "${_runtime_code}" ${_begin} -1 _tail)
    string(FIND "${_tail}" "{" _open)
    string(FIND "${_tail}" ";" _semicolon)
    if(_open EQUAL -1 OR (NOT _semicolon EQUAL -1 AND _semicolon LESS _open))
        message(FATAL_ERROR
            "Stage11D loot evidence guard missing runtime ${LABEL} definition")
    endif()
    evidence_find_cpp_function_bounds_in_sanitized("${_runtime_code}"
        "${SIGNATURE}" _function_begin _function_open _function_end)
    math(EXPR _function_length
        "${_function_end} - ${_function_begin} + 1")
    string(SUBSTRING "${_runtime_code}" ${_function_begin}
        ${_function_length} _function)
    set(${OUT_FUNCTION} "${_function}" PARENT_SCOPE)
endfunction()

function(stage11d_extract_report_definition LABEL SIGNATURE OUT_FUNCTION)
    stage11d_count_raw_token("${_report_code}" "${SIGNATURE}" _count)
    if(NOT _count EQUAL 1)
        message(FATAL_ERROR
            "Stage11D loot evidence guard missing report ${LABEL} definition")
    endif()
    string(FIND "${_report_code}" "${SIGNATURE}" _begin)
    string(SUBSTRING "${_report_code}" ${_begin} -1 _tail)
    string(FIND "${_tail}" "{" _open)
    string(FIND "${_tail}" ";" _semicolon)
    if(_open EQUAL -1 OR (NOT _semicolon EQUAL -1 AND _semicolon LESS _open))
        message(FATAL_ERROR
            "Stage11D loot evidence guard missing report ${LABEL} definition")
    endif()
    evidence_find_cpp_function_bounds_in_sanitized("${_report_code}"
        "${SIGNATURE}" _function_begin _function_open _function_end)
    math(EXPR _function_length
        "${_function_end} - ${_function_begin} + 1")
    string(SUBSTRING "${_report_code}" ${_function_begin}
        ${_function_length} _function)
    set(${OUT_FUNCTION} "${_function}" PARENT_SCOPE)
endfunction()

function(stage11d_require_unique_token_depth LABEL SURFACE TOKEN EXPECTED_DEPTH)
    stage11d_count_raw_token("${SURFACE}" "${TOKEN}" _count)
    if(NOT _count EQUAL 1)
        message(FATAL_ERROR
            "Stage11D loot evidence guard rejected ${LABEL} token inventory")
    endif()
    string(FIND "${SURFACE}" "${TOKEN}" _position)
    stage11d_code_brace_depth("${SURFACE}" ${_position} _depth)
    if(NOT _depth EQUAL EXPECTED_DEPTH)
        message(FATAL_ERROR
            "Stage11D loot evidence guard rejected ${LABEL} scope")
    endif()
endfunction()

function(stage11d_find_host_code_token TOKEN OUT_POSITION)
    string(FIND "${_host_text}" "${TOKEN}" _raw_position)
    if(_raw_position EQUAL -1)
        set(${OUT_POSITION} -1 PARENT_SCOPE)
        return()
    endif()
    evidence_find_cpp_code_token("${_host_text}" "${TOKEN}" _code_position)
    set(${OUT_POSITION} ${_code_position} PARENT_SCOPE)
endfunction()

stage11d_require_unique_token_depth("stage_header state definition"
    "${_stage_header_code}" "struct Stage11DLootValidationState final" 2)

foreach(_required IN ITEMS
        "struct Stage11DLootValidationState final"
        "std::array<std::uint64_t, dungeon::kGroundDropCapacity> snapshot_item_ids{};"
        "std::array<std::uint64_t, dungeon::kGroundDropCapacity> inventory_item_ids{};"
        "std::array<std::int32_t, 3> last_monster_hp{};"
        "std::array<std::uint16_t, 3> monster_affix_danger{};"
        "std::array<std::uint32_t, 3> defeat_distance_milli{};"
        "std::uint64_t pickup_commit_generation{};"
        "bool abyss_claim_requested{};" "bool abyss_claimed{};"
        "PhysicalKeySnapshot inject_stage11d_physical_edges("
        "bool stage11d_validation_active("
        "void observe_stage11d_abyss_claim("
        "bool stage11d_target_visible("
        "void stage11d_record_semantics("
        "void write_stage11d_loot_validation_summary("
        "enum class LootFilterMode : std::uint8_t;"
        "struct DungeonRenderStatus;" "struct PauseMenuState;")
    string(FIND "${_stage_header_code}" "${_required}" _found)
    if(_found EQUAL -1)
        message(FATAL_ERROR
            "Stage11D loot evidence guard missing runtime header token: ${_required}")
    endif()
endforeach()
foreach(_forbidden IN ITEMS "raylib.h" "renderer" "persistence" "test")
    string(FIND "${_stage_header_text}" "${_forbidden}" _found)
    if(NOT _found EQUAL -1)
        message(FATAL_ERROR
            "Stage11D loot evidence guard rejected runtime header dependency: ${_forbidden}")
    endif()
endforeach()
stage11d_find_host_code_token("struct Stage11DLootValidationState final"
    _host_state_definition)
if(NOT _host_state_definition EQUAL -1)
    message(FATAL_ERROR
        "Stage11D loot evidence guard found runtime state definition in host")
endif()

foreach(_declaration IN ITEMS
        "bool stage11d_target_visible("
        "void stage11d_record_semantics("
        "void write_stage11d_loot_validation_summary(")
    stage11d_require_unique_token_depth("report header declaration"
        "${_stage_header_code}" "${_declaration}" 2)
endforeach()

set(_runtime_definitions
    "ordinary rarity selector|bool stage11d_has_three_ordinary_rarities(|1"
    "ground selector|const dungeon::GroundItemSnapshot* stage11d_nearest_ground(|2"
    "monster selector|const combat::MonsterSnapshot* stage11d_priority_monster(|2"
    "attack selector|bool stage11d_attack_lane(|2"
    "safe movement|combat::MovementInput stage11d_safe_movement_toward(|2"
    "physical driver|PhysicalKeySnapshot inject_stage11d_physical_edges(|1"
    "fixed-step activation|bool stage11d_validation_active(|1"
    "abyss observer|void observe_stage11d_abyss_claim(|1")
foreach(_entry IN LISTS _runtime_definitions)
    string(REPLACE "|" ";" _parts "${_entry}")
    list(GET _parts 0 _label)
    list(GET _parts 1 _signature)
    list(GET _parts 2 _expected_depth)
    stage11d_require_unique_token_depth("runtime ${_label} definition"
        "${_runtime_code}" "${_signature}" ${_expected_depth})
    stage11d_extract_runtime_definition("${_label}" "${_signature}"
        _definition)
endforeach()

foreach(_signature IN ITEMS
        "bool stage11d_has_three_ordinary_rarities("
        "const dungeon::GroundItemSnapshot* stage11d_nearest_ground("
        "const combat::MonsterSnapshot* stage11d_priority_monster("
        "bool stage11d_attack_lane("
        "combat::MovementInput stage11d_safe_movement_toward("
        "PhysicalKeySnapshot inject_stage11d_physical_edges("
        "bool stage11d_validation_active("
        "void observe_stage11d_abyss_claim(")
    stage11d_find_host_code_token("${_signature}" _host_definition)
    if(NOT _host_definition EQUAL -1)
        message(FATAL_ERROR
            "Stage11D loot evidence guard found runtime definition in host")
    endif()
endforeach()

set(_report_definitions
    "rarity-view helper|bool stage11d_view_has_rarity(|2"
    "semantic recorder|void stage11d_record_semantics(|1"
    "target-visible evaluator|bool stage11d_target_visible(|1"
    "scenario-name helper|const char* stage11d_scenario_name(|2"
    "summary writer|void write_stage11d_loot_validation_summary(|1")
foreach(_entry IN LISTS _report_definitions)
    string(REPLACE "|" ";" _parts "${_entry}")
    list(GET _parts 0 _label)
    list(GET _parts 1 _signature)
    list(GET _parts 2 _expected_depth)
    stage11d_require_unique_token_depth("report ${_label} definition"
        "${_report_code}" "${_signature}" ${_expected_depth})
    stage11d_extract_report_definition("${_label}" "${_signature}"
        _definition)
    stage11d_find_host_code_token("${_signature}" _host_definition)
    if(NOT _host_definition EQUAL -1)
        message(FATAL_ERROR
            "Stage11D loot evidence guard found report definition in host")
    endif()
endforeach()

stage11d_extract_report_definition("semantic recorder"
    "void stage11d_record_semantics(" _report_record_function)
stage11d_extract_report_definition("target-visible evaluator"
    "bool stage11d_target_visible(" _report_target_function)
stage11d_extract_report_definition("summary writer"
    "void write_stage11d_loot_validation_summary(" _report_summary_function)
foreach(_required IN ITEMS
        "state.snapshot_item_ids[index] = item.item_id;"
        "state.inventory_item_ids[state.inventory_item_count++] = item.id;")
    string(FIND "${_report_record_function}" "${_required}" _found)
    if(_found EQUAL -1)
        message(FATAL_ERROR
            "Stage11D loot evidence guard missing report recorder semantic: ${_required}")
    endif()
endforeach()
foreach(_required IN ITEMS
        "state.monster_affix_danger[ordinal] ="
        "state.monster_ai_phase[ordinal] ="
        "state.defeat_player_hp[ordinal] = snapshot.combat->player.hp;"
        "state.pickup_commit_generation = status.loot_pickup.commit_generation;")
    string(FIND "${_report_target_function}" "${_required}" _found)
    if(_found EQUAL -1)
        message(FATAL_ERROR
            "Stage11D loot evidence guard missing report evaluator semantic: ${_required}")
    endif()
endforeach()

stage11d_extract_runtime_definition("physical driver"
    "PhysicalKeySnapshot inject_stage11d_physical_edges(" _driver_text)
stage11d_require_unique_token_depth("runtime physical driver" "${_driver_text}"
    "using Scenario = Stage11DLootValidationScenario;" 1)
foreach(_required IN ITEMS
        "inject_validation_pressed(" "inject_validation_action("
        "inject_validation_movement(" "validation_movement_toward("
        "validation_route_fire_movement(" "nearest_living_monster("
        "stage11d_safe_movement_toward(" "stage11d_attack_lane(")
    string(FIND "${_driver_text}" "${_required}" _found)
    if(_found EQUAL -1)
        message(FATAL_ERROR
            "Stage11D loot evidence guard missing runtime physical driver token: ${_required}")
    endif()
endforeach()
foreach(_forbidden IN ITEMS
        ".queue_action(" "request_pickup(" "complete_pickup(" "TestAccess"
        "inventory_count =" "renderer.draw(")
    string(FIND "${_driver_text}" "${_forbidden}" _found)
    if(NOT _found EQUAL -1)
        message(FATAL_ERROR
            "Stage11D loot evidence guard rejected physical-driver bypass: ${_forbidden}")
    endif()
endforeach()
string(REGEX MATCH
    "ground_items[ \t\r\n]*\\[[^]]+\\][ \t\r\n]*=[^=]"
    _ground_mutation "${_runtime_code}")
if(_ground_mutation)
    message(FATAL_ERROR "Stage11D loot evidence guard rejected snapshot mutation")
endif()

stage11d_extract_runtime_definition("fixed-step activation"
    "bool stage11d_validation_active(" _fixed_step_function)
stage11d_require_unique_token_depth("runtime fixed-step activation"
    "${_fixed_step_function}"
    "return config.stage11d_loot_validation" 1)
stage11d_extract_runtime_definition("abyss observer"
    "void observe_stage11d_abyss_claim(" _abyss_function)
stage11d_require_unique_token_depth("runtime abyss observer"
    "${_abyss_function}" "if (state.abyss_claim_requested)" 1)
stage11d_require_unique_token_depth("runtime abyss observer assignment"
    "${_abyss_function}" "state.abyss_claimed =" 2)
foreach(_required IN ITEMS
        "current.ground_items[index].item_id" "item_state.items"
        "!still_ground && now_owned")
    string(FIND "${_abyss_function}" "${_required}" _found)
    if(_found EQUAL -1)
        message(FATAL_ERROR
            "Stage11D loot evidence guard missing runtime abyss observer token: ${_required}")
    endif()
endforeach()

foreach(_required IN ITEMS
        "Stage11DLootValidationScenario" "show_all" "magic_or_better"
        "rare_only" "rare_only_abyss" "preview_cancel" "pickup_feedback"
        "platform::run_raylib_host(config)" "persistence::SaveStore"
        "settings::SettingsStore" "settings_store.save("
        "present_frame_and_maybe_capture" "stage11d_record_semantics"
        "renderer.draw(" "pickup_commit_generation" "defeat_distance_milli")
    string(FIND "${_combined}" "${_required}" _found)
    if(_found EQUAL -1)
        message(FATAL_ERROR "Stage11D loot evidence guard missing production token: ${_required}")
    endif()
endforeach()

string(FIND "${_header_text}" "enum class Stage11DLootValidationScenario" _enum_begin)
if(_enum_begin EQUAL -1)
    message(FATAL_ERROR "Stage11D loot evidence guard cannot isolate scenario enum")
endif()
string(SUBSTRING "${_header_text}" ${_enum_begin} -1 _enum_tail)
string(FIND "${_enum_tail}" "};" _enum_end)
if(_enum_end EQUAL -1)
    message(FATAL_ERROR "Stage11D loot evidence guard cannot isolate scenario enum")
endif()
math(EXPR _enum_length "${_enum_end} + 2")
string(SUBSTRING "${_enum_tail}" 0 ${_enum_length} _enum_text)
string(REGEX MATCHALL "[ \t\r\n]([a-z][a-z0-9_]*)[ \t\r\n]*[,}]" _enum_values "${_enum_text}")
list(LENGTH _enum_values _enum_count)
if(NOT _enum_count EQUAL 7)
    message(FATAL_ERROR "Stage11D loot evidence guard requires none plus exactly six scenarios")
endif()

foreach(_forbidden IN ITEMS
        "TestAccess" "snapshot_override" "set_snapshot(" "FakeRenderer"
        "fake_renderer" "request_pickup(" "complete_pickup(" "publish_pickup("
        "pause_menu.committed.loot_filter_mode ="
        "live_settings.loot_filter_mode =" "ground_items[0] ="
        "inventory_count =" "result=pass" "LoadImageFromScreen()"
        "remove_all(")
    string(FIND "${_formal_text}" "${_forbidden}" _formal_found)
    if(NOT _formal_found EQUAL -1)
        message(FATAL_ERROR "Stage11D loot evidence guard rejected formal bypass: ${_forbidden}")
    endif()
endforeach()

foreach(_required IN ITEMS
        "allowed_evidence_root(" "reset_evidence_root("
        "absolute.filename() != \"stage11d loot evidence\""
        "absolute == absolute.root_path()" "temp_directory_path("
        "/out/build/" "has_link_or_reparse_component("
        "FILE_ATTRIBUTE_REPARSE_POINT" "weakly_canonical("
        "remove_known_file(root, root / spec.image)"
        "remove_known_file(root, root / spec.summary)"
        "--root-safety-self-test"
        "stage11d-root-safety-sentinel.unknown"
        "return sentinel_preserved && known_removed")
    string(FIND "${_formal_text}" "${_required}" _cleanup_found)
    if(_cleanup_found EQUAL -1)
        message(FATAL_ERROR
            "Stage11D formal cleanup safety is incomplete: ${_required}")
    endif()
endforeach()

set(_host_code "${_host_text}")
set(_semantic_ownership_text "${_host_text}\n${_report_text}")
foreach(_forbidden IN ITEMS
        "TestAccess" "snapshot_override" "set_snapshot(" "FakeRenderer"
        "fake_renderer" "request_pickup(" "complete_pickup("
        "publish_pickup(")
    string(FIND "${_semantic_ownership_text}" "${_forbidden}" _host_found)
    if(NOT _host_found EQUAL -1)
        message(FATAL_ERROR
            "Stage11D host-bypass-${_forbidden}")
    endif()
endforeach()

string(REGEX MATCH
    "pause_menu[ \t\r\n]*[.][ \t\r\n]*committed([^=;]*|)[=][^=]"
    _committed_assignment "${_stage11d_semantic_code}")
if(_committed_assignment)
    message(FATAL_ERROR
        "Stage11D loot evidence guard rejected host validation seam direct committed settings write")
endif()
string(REGEX MATCH "live_settings([^=;]*|)[=][^=]"
    _live_assignment "${_stage11d_semantic_code}")
if(_live_assignment)
    message(FATAL_ERROR
        "Stage11D loot evidence guard rejected host validation seam direct live settings write")
endif()
string(REGEX MATCH "result[ \t\r\n]*=[ \t\r\n]*pass"
    _direct_pass "${_stage11d_semantic_seam}")
if(_direct_pass)
    message(FATAL_ERROR
        "Stage11D loot evidence guard rejected host validation seam direct result pass")
endif()

string(REGEX REPLACE "[ \t\r\n]+" "" _host_normalized "${_host_code}")
string(REGEX MATCHALL
    "live_settings[.]loot_filter_mode=pause_menu[.]committed[.]loot_filter_mode"
    _canonical_live_settings_writes "${_host_normalized}")
list(LENGTH _canonical_live_settings_writes _canonical_live_settings_count)
if(NOT _canonical_live_settings_count EQUAL 2)
    message(FATAL_ERROR
        "Stage11D loot evidence guard requires exactly two canonical production live-settings rollback writes")
endif()
string(REGEX MATCHALL "live_settings[.]loot_filter_mode="
    _all_live_filter_writes "${_host_normalized}")
list(LENGTH _all_live_filter_writes _all_live_filter_write_count)
if(NOT _all_live_filter_write_count EQUAL 2)
    message(FATAL_ERROR
        "Stage11D loot evidence guard rejected extra live-settings filter write")
endif()

string(REGEX MATCH
    "ground_items[ \t\r\n]*\\[[^]]+\\][ \t\r\n]*=[^=]"
    _host_ground_mutation "${_host_text}\n${_report_text}")
if(_host_ground_mutation)
    message(FATAL_ERROR "Stage11D loot evidence guard rejected host snapshot mutation")
endif()

function(stage11d_require_ordered_host_tokens SURFACE EXPECTED_DEPTH)
    set(_previous -1)
    foreach(_token IN ITEMS ${ARGN})
        stage11d_count_raw_token("${SURFACE}" "${_token}" _count)
        if(NOT _count EQUAL 1)
            message(FATAL_ERROR
                "Stage11D loot evidence guard missing input stage: ${_token}")
        endif()
        string(FIND "${SURFACE}" "${_token}" _position)
        stage11d_code_brace_depth("${SURFACE}" ${_position} _depth)
        if(NOT _depth EQUAL EXPECTED_DEPTH)
            if(_token STREQUAL
                    "validation_runtime->inject_physical_edges(")
                message(FATAL_ERROR
                    "Stage11D loot evidence guard rejected host input call scope")
            endif()
            message(FATAL_ERROR
                "Stage11D loot evidence guard rejected host input chain scope: ${_token}")
        endif()
        if(NOT _previous EQUAL -1 AND _position LESS _previous)
            message(FATAL_ERROR
                "Stage11D loot evidence guard rejected physical sample-map-submit order")
        endif()
        set(_previous ${_position})
    endforeach()
endfunction()

stage11d_require_ordered_host_tokens("${_host_input_code}" 0
    "const PhysicalKeySnapshot sampled_physical_keys = sample_physical_keys();"
    "validation_runtime->inject_physical_edges("
    "HostFrameInput frame_input = map_host_frame_input(")
string(FIND "${_host_input_code}"
    "HostFrameInput frame_input = map_host_frame_input(" _host_map_position)
stage11d_count_raw_token("${_host_input_code}"
    "PauseCommand pause_command = update_pause_menu(" _host_pause_count)
string(FIND "${_host_input_code}"
    "PauseCommand pause_command = update_pause_menu(" _host_pause_position)
stage11d_count_raw_token("${_host_input_code}"
    "submit_frame_actions(*session, frame_input)" _host_submit_count)
string(FIND "${_host_input_code}"
    "submit_frame_actions(*session, frame_input)" _host_submit_position)
if(NOT _host_submit_count EQUAL 1 OR _host_submit_position EQUAL -1)
    message(FATAL_ERROR
        "Stage11D loot evidence guard missing input stage: submit_frame_actions")
endif()
if(NOT _host_pause_count EQUAL 1 OR _host_pause_position EQUAL -1)
    message(FATAL_ERROR
        "Stage11D loot evidence guard missing input stage: update_pause_menu")
endif()
stage11d_code_brace_depth("${_host_input_code}" ${_host_pause_position}
    _host_pause_depth)
if(NOT _host_pause_depth EQUAL 0)
    message(FATAL_ERROR
        "Stage11D loot evidence guard rejected host pause call scope")
endif()
stage11d_code_brace_depth("${_host_input_code}" ${_host_submit_position}
    _host_submit_depth)
if(NOT _host_submit_depth EQUAL 1)
    message(FATAL_ERROR
        "Stage11D loot evidence guard rejected host submit scope")
endif()
if(_host_pause_position LESS _host_map_position
        OR _host_submit_position LESS _host_pause_position)
    message(FATAL_ERROR
        "Stage11D loot evidence guard rejected physical sample-map-pause-submit order")
endif()

string(REGEX REPLACE "[ \t\r\n]+" "" _host_input_normalized
    "${_host_input_code}")
string(FIND "${_host_input_normalized}"
    "constPhysicalKeySnapshotstage17_physical_keys=validation_runtime->inject_physical_edges(sampled_physical_keys,input_settings,current,gameplay_rearm_was_required);"
    _host_input_binding)
if(_host_input_binding EQUAL -1)
    message(FATAL_ERROR
        "Stage11D loot evidence guard rejected host input call binding")
endif()
stage11d_count_raw_token("${_host_input_code}"
    "inject_stage11d_physical_edges(" _host_direct_stage11d_input_count)
if(NOT _host_direct_stage11d_input_count EQUAL 0)
    message(FATAL_ERROR
        "Stage11D loot evidence guard rejected host input call binding")
endif()

set(_host_validation_snapshot_signature
    "void HostValidationRuntime::observe_snapshot(")
stage11d_count_raw_token("${_host_validation_runtime_code}"
    "${_host_validation_snapshot_signature}"
    _host_validation_snapshot_count)
if(NOT _host_validation_snapshot_count EQUAL 1)
    message(FATAL_ERROR
        "Stage11D loot evidence guard missing fixed-tick observation token: host_validation::observe_stage17_snapshot(")
endif()
evidence_find_cpp_function_bounds_in_sanitized(
    "${_host_validation_runtime_code}"
    "${_host_validation_snapshot_signature}"
    _host_validation_snapshot_begin _host_validation_snapshot_open
    _host_validation_snapshot_end)
math(EXPR _host_validation_snapshot_length
    "${_host_validation_snapshot_end} - ${_host_validation_snapshot_begin} + 1")
string(SUBSTRING "${_host_validation_runtime_code}"
    ${_host_validation_snapshot_begin} ${_host_validation_snapshot_length}
    _host_validation_snapshot_function)
string(REGEX REPLACE "[ \t\r\n]+" ""
    _host_validation_snapshot_normalized
    "${_host_validation_snapshot_function}")
set(_host_validation_snapshot_expected
    "voidHostValidationRuntime::observe_snapshot(constdungeon::DungeonSnapshot&snapshot)noexcept{host_validation::observe_stage17_snapshot(*impl_->config,impl_->states.stage17,snapshot);}")
if(NOT _host_validation_snapshot_normalized STREQUAL
        _host_validation_snapshot_expected)
    message(FATAL_ERROR
        "Stage11D loot evidence guard missing fixed-tick observation token: host_validation::observe_stage17_snapshot(")
endif()

stage11d_extract_marker_region("${_host_fixed_step_code}" host fixed_step
    _host_fixed_step_seam)
stage11d_require_unique_token_depth("host fixed-step activation call"
    "${_host_fixed_step_code}"
    "host_validation::stage11d_validation_active(config)" 1)
string(REGEX REPLACE "[ \t\r\n]+" "" _host_fixed_step_normalized
    "${_host_fixed_step_seam}")
string(FIND "${_host_fixed_step_normalized}"
    "||host_validation::stage11d_validation_active(config)"
    _host_fixed_step_call)
if(_host_fixed_step_call EQUAL -1)
    message(FATAL_ERROR
        "Stage11D loot evidence guard rejected fixed-step activation call")
endif()

stage11d_extract_marker_region("${_host_fixed_step_code}" host abyss_claim
    _host_abyss_seam)
string(REGEX REPLACE "[ \t\r\n]+" "" _host_abyss_normalized
    "${_host_abyss_seam}")
string(FIND "${_host_abyss_normalized}"
    "host_validation::observe_stage11d_abyss_claim(stage11d_validation_state,current,session->item_state());"
    _host_abyss_call)
if(_host_abyss_call EQUAL -1)
    message(FATAL_ERROR
        "Stage11D loot evidence guard rejected host abyss observer binding")
endif()

string(FIND "${_host_fixed_step_code}"
    "for (std::uint32_t step = 0; step < frame.steps; ++step) {"
    _host_fixed_tick_begin)
if(_host_fixed_tick_begin EQUAL -1)
    message(FATAL_ERROR
        "Stage11D loot evidence guard cannot isolate host fixed-step loop")
endif()
string(SUBSTRING "${_host_fixed_step_code}" ${_host_fixed_tick_begin} -1
    _host_fixed_tick_loop)
set(_previous -1)
foreach(_token IN ITEMS
        "runtime.fixed_tick(" "session->snapshot(current);"
        "validation_runtime->observe_snapshot(current);"
        "host_validation::observe_stage11d_abyss_claim(")
    stage11d_count_raw_token("${_host_fixed_tick_loop}" "${_token}" _count)
    if(NOT _count EQUAL 1)
        message(FATAL_ERROR
            "Stage11D loot evidence guard missing fixed-tick observation token: ${_token}")
    endif()
    string(FIND "${_host_fixed_tick_loop}" "${_token}" _position)
    stage11d_code_brace_depth("${_host_fixed_tick_loop}" ${_position} _depth)
    if(NOT _depth EQUAL 1)
        message(FATAL_ERROR
            "Stage11D loot evidence guard rejected host abyss observer scope")
    endif()
    if(NOT _previous EQUAL -1 AND _position LESS _previous)
        message(FATAL_ERROR
            "Stage11D loot evidence guard rejected abyss claim observation order")
    endif()
    set(_previous ${_position})
endforeach()

string(FIND "${_host_text}" "EndDrawing();" _present)
string(FIND "${_host_text}" "LoadImageFromScreen();" _capture)
if(_present EQUAL -1 OR _capture EQUAL -1 OR NOT _present LESS _capture)
    message(FATAL_ERROR "Stage11D loot evidence guard rejected pre-EndDrawing capture")
endif()
string(REGEX MATCHALL "LoadImageFromScreen[ \t\r\n]*\\(" _capture_calls "${_host_text}")
list(LENGTH _capture_calls _capture_count)
if(NOT _capture_count EQUAL 1)
    message(FATAL_ERROR "Stage11D loot evidence guard requires one production capture helper")
endif()
string(FIND "${_host_text}" "make_combat_render_plan(" _host_second_plan)
if(NOT _host_second_plan EQUAL -1)
    message(FATAL_ERROR "Stage11D loot evidence guard rejected a second host render plan")
endif()
arpg_sanitize_cpp_source("${_renderer_text}" _renderer_code)
string(FIND "${_renderer_code}" "GroundLootView CombatRenderer::draw("
    _renderer_draw_start)
if(_renderer_draw_start EQUAL -1)
    message(FATAL_ERROR "Stage11D loot evidence guard cannot isolate CombatRenderer::draw")
endif()
string(SUBSTRING "${_renderer_code}" 0 ${_renderer_draw_start}
    _renderer_before_draw_text)
string(SUBSTRING "${_renderer_code}" ${_renderer_draw_start} -1
    _renderer_draw_text)
string(REGEX MATCHALL "make_combat_render_plan[ \t\r\n]*\\("
    _renderer_plans "${_renderer_draw_text}")
list(LENGTH _renderer_plans _renderer_plan_count)
if(NOT _renderer_plan_count EQUAL 1)
    message(FATAL_ERROR "Stage11D loot evidence guard requires the one production renderer plan")
endif()
string(REGEX MATCHALL "make_combat_render_plan[ \t\r\n]*\\("
    _renderer_before_draw_plans "${_renderer_before_draw_text}")
list(LENGTH _renderer_before_draw_plans _renderer_before_draw_plan_count)
if(NOT _renderer_before_draw_plan_count EQUAL 3)
    message(FATAL_ERROR
        "Stage11D loot evidence guard rejected a draw-external renderer plan")
endif()
string(REGEX MATCHALL "make_combat_render_plan[ \t\r\n]*\\("
    _renderer_all_plans "${_renderer_code}")
list(LENGTH _renderer_all_plans _renderer_all_plan_count)
if(NOT _renderer_all_plan_count EQUAL 4)
    message(FATAL_ERROR
        "Stage11D loot evidence guard requires the current four renderer-plan tokens")
endif()
foreach(_required IN ITEMS
        "const GroundLootView ground_loot_view = [&]() noexcept {"
        "return GroundLootView{};" "return renderer.draw("
        "stage11d_record_semantics(stage11d_validation_state, current,"
        "present_frame_and_maybe_capture(capture_path.has_value()"
        "stage11d_validation_state.captured = true;")
    string(FIND "${_host_text}" "${_required}" _found)
    if(_found EQUAL -1)
        message(FATAL_ERROR "Stage11D loot evidence guard missing evidence binding: ${_required}")
    endif()
endforeach()

foreach(_required IN ITEMS
        "LastWriteTimeUtc" "1280" "720" "duplicate screenshot hash"
        "committed PNG differs" "committed summary differs"
        "\$feature = Measure-Region \$bitmap"
        "snapshot_ids" "inventory_ids" "hidden-item semantics mismatch"
        "abyss_claimed" "pickup_commit_generation" "preview_visible_count"
        "pickup notice region is blank" "result -eq 'pass'")
    string(FIND "${_validator_text}" "${_required}" _found)
    if(_found EQUAL -1)
        message(FATAL_ERROR "Stage11D loot evidence validator missing semantic check: ${_required}")
    endif()
endforeach()

message(STATUS "Stage11D loot formal evidence guard passed")
