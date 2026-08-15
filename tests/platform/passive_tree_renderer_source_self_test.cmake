cmake_minimum_required(VERSION 3.25)

if(NOT DEFINED SOURCE_ROOT OR NOT DEFINED MUTATION_ROOT)
    message(FATAL_ERROR "SOURCE_ROOT and MUTATION_ROOT are required")
endif()

set(_guard
    "${SOURCE_ROOT}/tests/platform/passive_tree_renderer_source_test.cmake")
set(_renderer
    "${SOURCE_ROOT}/src/platform/raylib/passive_tree_renderer.cpp")
set(_view_header
    "${SOURCE_ROOT}/src/platform/raylib/passive_tree_view_math.hpp")
set(_view_source
    "${SOURCE_ROOT}/src/platform/raylib/passive_tree_view_math.cpp")
foreach(_path IN ITEMS _guard _renderer _view_header _view_source)
    if(NOT EXISTS "${${_path}}")
        message(FATAL_ERROR "T4 self-test input is missing: ${${_path}}")
    endif()
endforeach()

file(READ "${_renderer}" _renderer_original)
file(READ "${_view_header}" _header_original)
file(READ "${_view_source}" _view_original)
file(REMOVE_RECURSE "${MUTATION_ROOT}")
file(MAKE_DIRECTORY "${MUTATION_ROOT}")
set_property(GLOBAL PROPERTY T4_PASSIVE_RENDERER_MISSED_MUTATIONS "")

function(run_guard LABEL RENDERER_TEXT HEADER_TEXT VIEW_TEXT EXPECT_SUCCESS)
    set(_root "${MUTATION_ROOT}/${LABEL}")
    file(MAKE_DIRECTORY "${_root}")
    set(_renderer_mutant "${_root}/passive_tree_renderer.cpp")
    set(_header_mutant "${_root}/passive_tree_view_math.hpp")
    set(_view_mutant "${_root}/passive_tree_view_math.cpp")
    file(WRITE "${_renderer_mutant}" "${RENDERER_TEXT}")
    file(WRITE "${_header_mutant}" "${HEADER_TEXT}")
    file(WRITE "${_view_mutant}" "${VIEW_TEXT}")
    execute_process(COMMAND "${CMAKE_COMMAND}"
        "-DSOURCE_ROOT=${SOURCE_ROOT}"
        "-DRENDERER_SOURCE=${_renderer_mutant}"
        "-DVIEW_HEADER=${_header_mutant}"
        "-DVIEW_SOURCE=${_view_mutant}"
        -P "${_guard}"
        RESULT_VARIABLE _result OUTPUT_QUIET ERROR_QUIET)
    if(EXPECT_SUCCESS AND NOT _result EQUAL 0)
        message(FATAL_ERROR "T4 guard unexpectedly rejected ${LABEL}")
    endif()
    if(NOT EXPECT_SUCCESS AND _result EQUAL 0)
        set_property(GLOBAL APPEND PROPERTY
            T4_PASSIVE_RENDERER_MISSED_MUTATIONS "${LABEL}")
    endif()
endfunction()

function(expect_renderer_mutation_rejected LABEL NEEDLE REPLACEMENT)
    string(REPLACE "${NEEDLE}" "${REPLACEMENT}"
        _mutated "${_renderer_original}")
    if(_mutated STREQUAL _renderer_original)
        message(FATAL_ERROR "T4 renderer mutation anchor is missing: ${LABEL}")
    endif()
    run_guard("${LABEL}" "${_mutated}" "${_header_original}"
        "${_view_original}" FALSE)
endfunction()

function(expect_view_mutation_rejected LABEL NEEDLE REPLACEMENT)
    string(REPLACE "${NEEDLE}" "${REPLACEMENT}"
        _mutated "${_view_original}")
    if(_mutated STREQUAL _view_original)
        message(FATAL_ERROR "T4 view mutation anchor is missing: ${LABEL}")
    endif()
    run_guard("${LABEL}" "${_renderer_original}" "${_header_original}"
        "${_mutated}" FALSE)
endfunction()

run_guard(pristine "${_renderer_original}" "${_header_original}"
    "${_view_original}" TRUE)

expect_view_mutation_rejected(core_passive_source_name
    "ui_passive_node_name(node)" "source.name")

expect_renderer_mutation_rejected(default_font_fallback
    "if (!use_passive_tree_hud_font(hud_font, hud_font_ready)"
    "const Font fallback = GetFontDefault();\n    static_cast<void>(fallback);\n    if (!use_passive_tree_hud_font(hud_font, hud_font_ready)")

expect_view_mutation_rejected(active_english_status
    "if (node >= passives::kPassiveNodeCount) return u8\"节点不可用\";"
    "if (node >= passives::kPassiveNodeCount) return u8\"节点不可用\";\n    if (node == 42U) return \"Refund now\";")

set(_correct_priority [=[if (input.save_error) {
        return {u8"保存状态：错误", PassiveTreeStatusTone::error};
    }
    if (input.saving) {
        return {u8"保存状态：保存中", PassiveTreeStatusTone::saving};
    }]=])
set(_reversed_priority [=[if (input.saving) {
        return {u8"保存状态：保存中", PassiveTreeStatusTone::saving};
    }
    if (input.save_error) {
        return {u8"保存状态：错误", PassiveTreeStatusTone::error};
    }]=])
expect_view_mutation_rejected(save_error_priority_reversal
    "${_correct_priority}" "${_reversed_priority}")

string(REPLACE
    "const float width = static_cast<float>(screen_width);"
    "if (false) { draw_centered_passive_tree_text(hud_font, hud_font_ready, ui_text(UiTextId::passive_title), {}, 16.0F, 10.0F, WHITE, kOverlayBacking); }\n    const float width = static_cast<float>(screen_width);"
    _dead_mutant "${_renderer_original}")
if(_dead_mutant STREQUAL _renderer_original)
    message(FATAL_ERROR "T4 renderer mutation anchor is missing: dead decoy")
endif()
run_guard(dead_shared_font_decoy "${_dead_mutant}" "${_header_original}"
    "${_view_original}" FALSE)

expect_renderer_mutation_rejected(partial_node_name_rendering
    "const PassiveNodeTextView text = passive_node_text_view("
    "if (node.type != passives::PassiveNodeType::notable\n            && node.type != passives::PassiveNodeType::keystone) continue;\n        const PassiveNodeTextView text = passive_node_text_view(")

expect_renderer_mutation_rejected(missing_state_shape
    "draw_node_state_mark(cue.mark, projection, color);"
    "static_cast<void>(cue.mark);")

expect_renderer_mutation_rejected(disconnected_visible_ring_extension
    "radius + kPassiveNodeVisibleRingExtension"
    "radius + 3.0F")

expect_renderer_mutation_rejected(hardcoded_close_binding
    "snapshot.progression.unspent_passive_points,\n        passive_tree_binding_label);"
    "snapshot.progression.unspent_passive_points, \"P\");")

get_property(_missed_mutations GLOBAL PROPERTY
    T4_PASSIVE_RENDERER_MISSED_MUTATIONS)
if(_missed_mutations)
    list(FILTER _missed_mutations EXCLUDE REGEX "^$")
endif()
if(_missed_mutations)
    list(JOIN _missed_mutations ", " _missed_labels)
    message(FATAL_ERROR "T4 guard missed mutations: ${_missed_labels}")
endif()

message(STATUS "T4 passive-tree renderer source guard self-test accepted")
