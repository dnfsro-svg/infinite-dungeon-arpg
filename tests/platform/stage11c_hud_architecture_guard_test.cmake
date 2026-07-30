if(NOT DEFINED SOURCE_ROOT)
    message(FATAL_ERROR "Stage11C HUD guard requires SOURCE_ROOT")
endif()

if(DEFINED HUD_SOURCE_ROOT)
    set(_hud_root "${HUD_SOURCE_ROOT}")
else()
    set(_hud_root "${SOURCE_ROOT}/platform/raylib")
endif()

file(GLOB _hud_sources LIST_DIRECTORIES FALSE
    "${_hud_root}/hud_*.h"
    "${_hud_root}/hud_*.hpp"
    "${_hud_root}/hud_*.c"
    "${_hud_root}/hud_*.cc"
    "${_hud_root}/hud_*.cpp"
    "${_hud_root}/hud_*.cxx")
if(NOT _hud_sources)
    message(FATAL_ERROR "Stage11C HUD boundary sources are missing: ${_hud_root}")
endif()

set(_combat_sources
    "${_hud_root}/combat_renderer.hpp"
    "${_hud_root}/combat_renderer.cpp")
set(_debug_overlay_sources
    "${_hud_root}/debug_overlay_renderer.hpp"
    "${_hud_root}/debug_overlay_renderer.cpp")
set(_host_source "${_hud_root}/raylib_host.cpp")
set(_validation_runtime_source
    "${_hud_root}/host_validation_runtime.cpp")
set(_stage11c_header "${_hud_root}/host_validation_stage11c.hpp")
set(_stage11c_source "${_hud_root}/host_validation_stage11c.cpp")
set(_raylib_cmake "${_hud_root}/CMakeLists.txt")
if(DEFINED CMAKE_OVERRIDE)
    set(_raylib_cmake "${CMAKE_OVERRIDE}")
endif()
set(_validation_input_sources
    "${_hud_root}/host_validation_input.hpp"
    "${_hud_root}/host_validation_input.cpp")
set(_validation_navigation_sources
    "${_hud_root}/host_validation_navigation.hpp"
    "${_hud_root}/host_validation_navigation.cpp")
set(_hud_boundary_sources
    ${_hud_sources} ${_combat_sources} ${_debug_overlay_sources}
    ${_validation_input_sources} ${_validation_navigation_sources}
    "${_stage11c_header}" "${_stage11c_source}")
foreach(_required_source IN LISTS _hud_boundary_sources)
    if(NOT EXISTS "${_required_source}")
        message(FATAL_ERROR
            "Stage11C HUD boundary source is missing: ${_required_source}")
    endif()
endforeach()
if(NOT EXISTS "${_raylib_cmake}")
    message(FATAL_ERROR "Stage11C HUD CMake source is missing: ${_raylib_cmake}")
endif()
include("${CMAKE_CURRENT_LIST_DIR}/../dungeon/evidence_source_scan.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/cpp_source_lexer.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/cmake_source_registration_scan.cmake")

foreach(_validation_header IN ITEMS
        "${_hud_root}/host_validation_input.hpp"
        "${_hud_root}/host_validation_navigation.hpp")
    file(READ "${_validation_header}" _validation_header_text)
    if(_validation_header_text MATCHES "raylib[.]h|renderer|persistence|test")
        message(FATAL_ERROR
            "Stage11C validation boundary header has a forbidden dependency: ${_validation_header}")
    endif()
endforeach()
foreach(_required_owner IN ITEMS "${_host_source}" "${_validation_runtime_source}")
    if(NOT EXISTS "${_required_owner}")
        message(FATAL_ERROR "Stage11C HUD owner source is missing: ${_required_owner}")
    endif()
endforeach()

function(arpg_hud_assert_excludes REASON REGEX)
    foreach(_source_file IN LISTS ARGN)
        file(READ "${_source_file}" _source_text)
        if(_source_text MATCHES "${REGEX}")
            message(FATAL_ERROR "${REASON}: ${_source_file}")
        endif()
    endforeach()
endfunction()

arpg_hud_assert_excludes("HUD boundary rejects physical input sampling"
    "(^|[^A-Za-z0-9_])GetKeyPressed[ \\t\\r\\n]*[(]"
    ${_hud_boundary_sources})
arpg_hud_assert_excludes("HUD boundary rejects DungeonSession access"
    "(^|[^A-Za-z0-9_])DungeonSession([^A-Za-z0-9_]|$)"
    ${_hud_boundary_sources})
arpg_hud_assert_excludes("HUD boundary rejects fixed tick access"
    "(^|[^A-Za-z0-9_])fixed_tick[ \\t\\r\\n]*[(]"
    ${_hud_boundary_sources})
arpg_hud_assert_excludes("HUD boundary rejects SettingsStore access"
    "(^|[^A-Za-z0-9_])SettingsStore([^A-Za-z0-9_]|$)"
    ${_hud_boundary_sources})
arpg_hud_assert_excludes("HUD boundary rejects dynamic std::string"
    "std[ \\t\\r\\n]*::[ \\t\\r\\n]*string([^A-Za-z0-9_]|$)"
    ${_hud_boundary_sources})
arpg_hud_assert_excludes("HUD boundary rejects dynamic std::vector"
    "std[ \\t\\r\\n]*::[ \\t\\r\\n]*vector[ \\t\\r\\n]*[<]"
    ${_hud_boundary_sources})
arpg_hud_assert_excludes("Normal HUD rejects legacy Budget text"
    "Budget" ${_hud_sources} ${_combat_sources})

function(arpg_extract_hud_host_seam
        SOURCE_TEXT START_TOKEN END_TOKEN REASON OUT_SEAM)
    string(FIND "${SOURCE_TEXT}" "${START_TOKEN}" _start)
    if(_start EQUAL -1)
        message(FATAL_ERROR "${REASON}: start token is missing")
    endif()
    string(SUBSTRING "${SOURCE_TEXT}" ${_start} -1 _tail)
    string(FIND "${_tail}" "${END_TOKEN}" _relative_end)
    if(_relative_end EQUAL -1)
        message(FATAL_ERROR "${REASON}: end token is missing")
    endif()
    string(LENGTH "${END_TOKEN}" _end_token_length)
    math(EXPR _length "${_relative_end} + ${_end_token_length}")
    string(SUBSTRING "${_tail}" 0 ${_length} _seam)
    set("${OUT_SEAM}" "${_seam}" PARENT_SCOPE)
endfunction()

function(stage11c_arch_unconditional_cpp_surface SOURCE OUT_SURFACE)
    arpg_sanitize_cpp_source("${SOURCE}" _logical_source)
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
                "^[ \t]*(#|%:)[ \t]*(if|ifdef|ifndef)([ \t\r\n(]|$)")
            math(EXPR _conditional_depth "${_conditional_depth} + 1")
        elseif(_line MATCHES
                "^[ \t]*(#|%:)[ \t]*endif([ \t\r\n]|$)")
            math(EXPR _conditional_depth "${_conditional_depth} - 1")
            if(_conditional_depth LESS 0)
                message(FATAL_ERROR
                    "Stage11C architecture conditional surface is unbalanced")
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
            "Stage11C architecture conditional surface is unbalanced")
    endif()
    set(${OUT_SURFACE} "${_surface}" PARENT_SCOPE)
endfunction()

function(stage11c_arch_count_token SOURCE TOKEN OUT_COUNT)
    string(LENGTH "${SOURCE}" _source_length)
    string(LENGTH "${TOKEN}" _token_length)
    string(REPLACE "${TOKEN}" "" _without "${SOURCE}")
    string(LENGTH "${_without}" _without_length)
    math(EXPR _removed "${_source_length} - ${_without_length}")
    math(EXPR _count "${_removed} / ${_token_length}")
    set(${OUT_COUNT} ${_count} PARENT_SCOPE)
endfunction()

function(stage11c_arch_matching_brace_position
        SURFACE OPEN_POSITION OUT_POSITION)
    string(LENGTH "${SURFACE}" _length)
    set(_depth 0)
    set(_close -1)
    while(OPEN_POSITION LESS _length)
        string(SUBSTRING "${SURFACE}" ${OPEN_POSITION} 1 _character)
        if(_character STREQUAL "{")
            math(EXPR _depth "${_depth} + 1")
        elseif(_character STREQUAL "}")
            math(EXPR _depth "${_depth} - 1")
            if(_depth EQUAL 0)
                set(_close ${OPEN_POSITION})
                break()
            endif()
        endif()
        math(EXPR OPEN_POSITION "${OPEN_POSITION} + 1")
    endwhile()
    if(_close EQUAL -1)
        message(FATAL_ERROR
            "Stage11C architecture direct-scope fixture has no closing brace")
    endif()
    set(${OUT_POSITION} ${_close} PARENT_SCOPE)
endfunction()

function(stage11c_arch_mask_non_direct_executable_scopes SOURCE OUT_SOURCE)
    string(LENGTH "${SOURCE}" _source_length)
    set(_masked "")
    set(_copy_cursor 0)
    set(_dead_condition
        "(false|0[uUlL]*|![ \t\r\n]*true|1[uUlL]*[ \t\r\n]*==[ \t\r\n]*0[uUlL]*|0[uUlL]*[ \t\r\n]*==[ \t\r\n]*1[uUlL]*)")
    while(_copy_cursor LESS _source_length)
        string(SUBSTRING "${SOURCE}" ${_copy_cursor} -1 _tail)
        string(REGEX MATCH
            "\\][ \t\r\n]*(\\([^{};]*\\))?[ \t\r\n]*(mutable[ \t\r\n]*)?(noexcept([ \t\r\n]*\\([^{};]*\\))?[ \t\r\n]*)?(->[^{;]*)?[ \t\r\n]*\\{"
            _lambda_match "${_tail}")
        string(REGEX MATCH
            "(if|while)[ \t\r\n]*(constexpr[ \t\r\n]*)?\\([ \t\r\n]*${_dead_condition}[ \t\r\n]*\\)[ \t\r\n]*(do[ \t\r\n]*)?([^{;]*\\{|[^{};]*;)"
            _dead_branch_match "${_tail}")
        string(REGEX MATCH
            "for[ \t\r\n]*\\([ \t\r\n]*;[ \t\r\n]*${_dead_condition}[ \t\r\n]*;[^)]*\\)[ \t\r\n]*([^{;]*\\{|[^{};]*;)"
            _dead_for_match "${_tail}")
        set(_scope_match "")
        set(_scope_relative -1)
        set(_scope_kind "")
        if(NOT _lambda_match STREQUAL "")
            string(FIND "${_tail}" "${_lambda_match}" _scope_relative)
            set(_scope_match "${_lambda_match}")
            set(_scope_kind lambda)
        endif()
        foreach(_candidate IN ITEMS _dead_branch_match _dead_for_match)
            set(_dead_match "${${_candidate}}")
            if(_dead_match STREQUAL "")
                continue()
            endif()
            string(FIND "${_tail}" "${_dead_match}" _dead_relative)
            if(_scope_relative EQUAL -1 OR _dead_relative LESS _scope_relative)
                set(_scope_match "${_dead_match}")
                set(_scope_relative ${_dead_relative})
                set(_scope_kind dead-control)
            endif()
        endforeach()
        if(_scope_relative EQUAL -1)
            string(APPEND _masked "${_tail}")
            break()
        endif()
        string(FIND "${_scope_match}" "{" _open_in_match)
        math(EXPR _match_index "${_copy_cursor} + ${_scope_relative}")
        if(_scope_kind STREQUAL "dead-control")
            set(_remove_begin ${_match_index})
        else()
            math(EXPR _remove_begin "${_match_index} + ${_open_in_match}")
        endif()
        math(EXPR _copy_length "${_remove_begin} - ${_copy_cursor}")
        if(_copy_length GREATER 0)
            string(SUBSTRING "${SOURCE}" ${_copy_cursor} ${_copy_length}
                _copy_chunk)
            string(APPEND _masked "${_copy_chunk}")
        endif()
        if(_open_in_match EQUAL -1)
            string(LENGTH "${_scope_match}" _scope_length)
            math(EXPR _scope_end "${_match_index} + ${_scope_length} - 1")
        else()
            math(EXPR _open_index "${_match_index} + ${_open_in_match}")
            stage11c_arch_matching_brace_position("${SOURCE}" ${_open_index}
                _scope_end)
        endif()
        math(EXPR _copy_cursor "${_scope_end} + 1")
    endwhile()
    set(${OUT_SOURCE} "${_masked}" PARENT_SCOPE)
endfunction()

function(arpg_assert_stage11c_cmake_registration CMAKE_TEXT)
    arpg_cmake_count_arpg_raylib_source("${CMAKE_TEXT}"
        "host_validation_stage11c.cpp" _registration_count)
    if(NOT _registration_count EQUAL 1)
        message(FATAL_ERROR
            "arpg_raylib does not register host_validation_stage11c.cpp exactly once")
    endif()
endfunction()

file(READ "${_host_source}" _host_text)
file(READ "${_validation_runtime_source}" _validation_runtime_text)
file(READ "${_stage11c_header}" _stage11c_header_text)
file(READ "${_stage11c_source}" _stage11c_source_text)
file(READ "${_raylib_cmake}" _raylib_cmake_text)
evidence_sanitize_cpp_for_scan("${_stage11c_header_text}" _stage11c_header_code)
if(NOT _stage11c_header_code MATCHES
        "struct[ \t\r\n]+Stage11CHudValidationState[ \t\r\n]+final[ \t\r\n]*[{]")
    message(FATAL_ERROR "Stage11C HUD state definition is missing from private Stage header")
endif()
foreach(_declaration IN ITEMS
        "inject_stage11c_physical_edges[ \t\r\n]*[(]"
        "stage11c_production_snapshot_hash[ \t\r\n]*[(]"
        "stage11c_hud_validation_reached[ \t\r\n]*[(]"
        "write_stage11c_hud_validation_summary[ \t\r\n]*[(]")
    if(NOT _stage11c_header_code MATCHES "${_declaration}")
        message(FATAL_ERROR
            "Stage11C HUD declaration is missing from private Stage header: ${_declaration}")
    endif()
endforeach()
if(_host_text MATCHES
        "struct[ \t\r\n]+Stage11CHudValidationState[ \t\r\n]+final")
    message(FATAL_ERROR "Stage11C HUD state definition remains in raylib_host.cpp")
endif()
arpg_sanitize_cpp_source(
    "${_stage11c_source_text}" _stage11c_source_lexical)
stage11c_arch_unconditional_cpp_surface(
    "${_stage11c_source_text}" _stage11c_source_active)
evidence_cpp_normalize_token_whitespace(
    "${_stage11c_source_lexical}" _stage11c_source_lexical_code)
evidence_cpp_normalize_token_whitespace(
    "${_stage11c_source_active}" _stage11c_source_code)
arpg_sanitize_cpp_source("${_host_text}" _host_lexical_source)
evidence_cpp_normalize_token_whitespace(
    "${_host_lexical_source}" _host_lexical_code)
set(_stage11c_signature_inject [=[PhysicalKeySnapshot inject_stage11c_physical_edges(
    PhysicalKeySnapshot snapshot, const RaylibHostConfig& config,
    const settings::SettingsData& settings_data,
    const dungeon::DungeonSnapshot& current,
    Stage11CHudValidationState& state) noexcept]=])
set(_stage11c_signature_hash [=[std::uint64_t stage11c_production_snapshot_hash(
    const dungeon::DungeonSnapshot& snapshot) noexcept]=])
set(_stage11c_signature_reached [=[bool stage11c_hud_validation_reached(
    const dungeon::DungeonSnapshot& snapshot,
    Stage11CHudValidationScenario scenario,
    const Stage11CHudValidationState& state, bool draw_debug) noexcept]=])
set(_stage11c_signature_summary [=[void write_stage11c_hud_validation_summary(const RaylibHostConfig& config,
    const Stage11CHudValidationState& state) noexcept]=])
set(_stage11c_name_inject "inject_stage11c_physical_edges(")
set(_stage11c_name_hash "stage11c_production_snapshot_hash(")
set(_stage11c_name_reached "stage11c_hud_validation_reached(")
set(_stage11c_name_summary "write_stage11c_hud_validation_summary(")
set(_stage11c_label_inject "PhysicalKeySnapshot inject_stage11c_physical_edges(")
set(_stage11c_label_hash "std::uint64_t stage11c_production_snapshot_hash(")
set(_stage11c_label_reached "bool stage11c_hud_validation_reached(")
set(_stage11c_label_summary "void write_stage11c_hud_validation_summary(")
foreach(_definition_id IN ITEMS inject hash reached summary)
    set(_signature_variable "_stage11c_signature_${_definition_id}")
    set(_name_variable "_stage11c_name_${_definition_id}")
    set(_label_variable "_stage11c_label_${_definition_id}")
    set(_definition "${${_signature_variable}}")
    set(_definition_name "${${_name_variable}}")
    set(_definition_label "${${_label_variable}}")
    evidence_cpp_normalize_token_whitespace(
        "${_definition}" _definition_token_signature)
    evidence_cpp_normalize_token_whitespace(
        "${_definition_name}" _definition_token_name)
    evidence_try_find_unique_cpp_function_in_namespace_in_sanitized(
        "${_stage11c_source_code}" "${_definition_token_signature}"
        "arpg::platform::host_validation"
        _stage_begin _stage_open _stage_end _stage_definition_valid)
    if(NOT _stage_definition_valid)
        string(FIND "${_stage11c_source_lexical_code}"
            "${_definition_token_name}"
            _stage_signature_position)
        if(_stage_signature_position EQUAL -1)
            message(FATAL_ERROR
                "Evidence validation function is missing: ${_definition_label}")
        else()
            message(FATAL_ERROR
                "Stage11C HUD definition is missing from Stage source: ${_definition_label}")
        endif()
    endif()
    string(FIND "${_host_lexical_code}" "${_definition_token_name}"
        _host_definition)
    if(NOT _host_definition EQUAL -1)
        message(FATAL_ERROR "Stage11C HUD definition remains in raylib_host.cpp: ${_definition_label}")
    endif()
endforeach()
arpg_assert_stage11c_cmake_registration("${_raylib_cmake_text}")
stage11c_arch_unconditional_cpp_surface("${_host_text}" _host_code)
stage11c_arch_unconditional_cpp_surface("${_validation_runtime_text}"
    _validation_runtime_code)
string(FIND "${_validation_runtime_code}"
    "void HostValidationRuntime::observe_hud(" _runtime_hud_owner)
if(_runtime_hud_owner EQUAL -1)
    message(FATAL_ERROR
        "Stage11C HUD facade is missing the active runtime observe_hud owner")
endif()
evidence_extract_cpp_function_block("${_validation_runtime_code}"
    "void HostValidationRuntime::observe_hud(" _runtime_hud_function)
string(FIND "${_runtime_hud_function}" "impl_->states.stage11c"
    _runtime_hud_state)
if(_runtime_hud_state EQUAL -1)
    message(FATAL_ERROR
        "Stage11C HUD facade observe_hud does not own the private Stage11C state")
endif()

evidence_extract_cpp_function_block("${_host_code}"
    "HostExitCode run_raylib_host(" _host_run_function)
stage11c_arch_mask_non_direct_executable_scopes("${_host_run_function}"
    _host_run_direct)
string(REGEX REPLACE "[ \t\r\n]+" "" _host_run_normalized
    "${_host_run_direct}")
stage11c_arch_count_token("${_host_run_normalized}"
    "validation_runtime->observe_hud(" _runtime_hud_arrow_count)
stage11c_arch_count_token("${_host_run_normalized}"
    "validation_runtime.get()->observe_hud(" _runtime_hud_get_count)
math(EXPR _runtime_hud_call_count
    "${_runtime_hud_arrow_count} + ${_runtime_hud_get_count}")
if(NOT _runtime_hud_call_count EQUAL 1)
    message(FATAL_ERROR
        "Stage11C HUD facade requires exactly one direct ordinary-frame observe_hud call")
endif()
set(_runtime_hud_call_position -1)
foreach(_runtime_hud_accessor IN ITEMS
        "validation_runtime->" "validation_runtime.get()->")
    set(_runtime_hud_call
        "${_runtime_hud_accessor}observe_hud(current,renderer.hud_model(),renderer.hud_notice_view(),draw_debug,GetScreenWidth(),GetScreenHeight());")
    string(FIND "${_host_run_normalized}" "${_runtime_hud_call}"
        _candidate_hud_call_position)
    if(NOT _candidate_hud_call_position EQUAL -1)
        set(_runtime_hud_call_position ${_candidate_hud_call_position})
    endif()
endforeach()
string(FIND "${_host_run_normalized}"
    "renderer.observe_presented_hud_frame(hud_presented_frame,"
    _renderer_hud_position)
string(FIND "${_host_run_normalized}" "BeginDrawing();" _begin_drawing_position)
if(_runtime_hud_call_position EQUAL -1)
    message(FATAL_ERROR
        "T7C-M10: HUD observer must use authoritative model, notices, and layout inputs")
elseif(_renderer_hud_position EQUAL -1 OR _begin_drawing_position EQUAL -1)
    message(FATAL_ERROR
        "Stage11C HUD observer binding is incomplete")
elseif(NOT _renderer_hud_position LESS _runtime_hud_call_position)
    message(FATAL_ERROR "T7C-M08: HUD observer must follow renderer HUD")
elseif(NOT _runtime_hud_call_position LESS _begin_drawing_position)
    message(FATAL_ERROR "T7C-M09: HUD observer must precede BeginDrawing")
endif()

foreach(_forbidden_host_owner IN ITEMS
        "stage11c_validation_state"
        "Stage11CHudValidationState"
        "stage11c_hud_validation_reached("
        "stage11c_production_snapshot_hash("
        "write_stage11c_hud_validation_summary(")
    string(FIND "${_host_code}" "${_forbidden_host_owner}" _host_owner_found)
    if(NOT _host_owner_found EQUAL -1)
        message(FATAL_ERROR
            "Stage11C HUD validation ownership remains in raylib_host.cpp: ${_forbidden_host_owner}")
    endif()
endforeach()
arpg_extract_hud_host_seam("${_host_code}"
    "renderer.observe_presented_hud_frame(HudPresentedFrame::recovery,"
    "GetFrameTime(), true);"
    "Host HUD recovery observation seam is invalid" _recovery_hud_seam)
arpg_extract_hud_host_seam("${_host_code}"
    "const HudPresentedFrame hud_presented_frame = current.death.has_value()"
    "feedback, audio_ready);"
    "Host HUD normal observation/presentation seam is invalid" _normal_hud_seam)
foreach(_required_recovery IN ITEMS
        "HudPresentedFrame::recovery" "renderer.observe_presented_hud_frame")
    string(FIND "${_recovery_hud_seam}" "${_required_recovery}" _found)
    if(_found EQUAL -1)
        message(FATAL_ERROR
            "Host HUD recovery observation seam is invalid: ${_required_recovery}")
    endif()
endforeach()
foreach(_required_normal IN ITEMS
        "HudPresentedFrame::death_overlay" "HudPresentedFrame::normal"
        "renderer.observe_presented_hud_frame" "BeginDrawing();"
        "const GroundLootView ground_loot_view = [&]() noexcept {"
        "return GroundLootView{};" "return renderer.draw(")
    string(FIND "${_normal_hud_seam}" "${_required_normal}" _found)
    if(_found EQUAL -1)
        message(FATAL_ERROR
            "Host HUD normal observation/presentation seam is invalid: ${_required_normal}")
    endif()
endforeach()

function(arpg_hud_seams_assert_exclude REASON REGEX)
    if(_recovery_hud_seam MATCHES "${REGEX}"
            OR _normal_hud_seam MATCHES "${REGEX}")
        message(FATAL_ERROR "${REASON}: ${_host_source} HUD seam")
    endif()
endfunction()
arpg_hud_seams_assert_exclude("HUD boundary rejects physical input sampling"
    "(^|[^A-Za-z0-9_])GetKeyPressed[ \\t\\r\\n]*[(]")
arpg_hud_seams_assert_exclude("HUD boundary rejects DungeonSession access"
    "(^|[^A-Za-z0-9_])DungeonSession([^A-Za-z0-9_]|$)")
arpg_hud_seams_assert_exclude("HUD boundary rejects fixed tick access"
    "(^|[^A-Za-z0-9_])fixed_tick[ \\t\\r\\n]*[(]")
arpg_hud_seams_assert_exclude("HUD boundary rejects SettingsStore access"
    "(^|[^A-Za-z0-9_])SettingsStore([^A-Za-z0-9_]|$)")
arpg_hud_seams_assert_exclude("HUD boundary rejects dynamic std::string"
    "std[ \\t\\r\\n]*::[ \\t\\r\\n]*string([^A-Za-z0-9_]|$)")
arpg_hud_seams_assert_exclude("HUD boundary rejects dynamic std::vector"
    "std[ \\t\\r\\n]*::[ \\t\\r\\n]*vector[ \\t\\r\\n]*[<]")
arpg_hud_seams_assert_exclude("Normal HUD rejects legacy Budget text"
    "Budget")

set(_view_model_header "${_hud_root}/hud_view_model.hpp")
set(_renderer_header "${_hud_root}/hud_renderer.hpp")
set(_view_model_source "${_hud_root}/hud_view_model.cpp")
foreach(_required IN ITEMS
        "${_view_model_header}" "${_renderer_header}" "${_view_model_source}")
    if(NOT EXISTS "${_required}")
        message(FATAL_ERROR "HUD visible status tag boundary source is missing: ${_required}")
    endif()
endforeach()
file(READ "${_view_model_header}" _view_model_header_text)
file(READ "${_renderer_header}" _renderer_header_text)
file(READ "${_view_model_source}" _view_model_source_text)
if(NOT _view_model_header_text MATCHES
        "std::array[<]HudStatusTagKind,[ \\t\\r\\n]*3[>] status_tags")
    message(FATAL_ERROR "HUD visible status tag limit must remain three in view model")
endif()
if(NOT _renderer_header_text MATCHES
        "std::array[<]HudStatusTagKind,[ \\t\\r\\n]*3[>] tags")
    message(FATAL_ERROR "HUD visible status tag limit must remain three in draw plan")
endif()
string(REGEX MATCHALL
    "append_status_tag[ \\t\\r\\n]*[(][ \\t\\r\\n]*output[.]player"
    _visible_status_projections "${_view_model_source_text}")
list(LENGTH _visible_status_projections _visible_status_projection_count)
if(NOT _visible_status_projection_count EQUAL 3)
    message(FATAL_ERROR
        "HUD visible status tag limit rejects fourth tag: found ${_visible_status_projection_count}")
endif()

message(STATUS "Stage 11C HUD architecture boundaries verified")
