cmake_minimum_required(VERSION 3.25)

if(NOT DEFINED SOURCE_ROOT)
    message(FATAL_ERROR "SOURCE_ROOT is required")
endif()

foreach(_name IN ITEMS RENDERER_SOURCE VIEW_HEADER VIEW_SOURCE)
    if(NOT DEFINED ${_name})
        if(_name STREQUAL "RENDERER_SOURCE")
            set(${_name}
                "${SOURCE_ROOT}/src/platform/raylib/passive_tree_renderer.cpp")
        elseif(_name STREQUAL "VIEW_HEADER")
            set(${_name}
                "${SOURCE_ROOT}/src/platform/raylib/passive_tree_view_math.hpp")
        else()
            set(${_name}
                "${SOURCE_ROOT}/src/platform/raylib/passive_tree_view_math.cpp")
        endif()
    endif()
    if(NOT EXISTS "${${_name}}")
        message(FATAL_ERROR "T4 source is missing: ${_name}=${${_name}}")
    endif()
endforeach()

include("${CMAKE_CURRENT_LIST_DIR}/cpp_source_function_body.cmake")

function(require_token TEXT TOKEN MESSAGE_TEXT)
    string(FIND "${TEXT}" "${TOKEN}" _found)
    if(_found EQUAL -1)
        message(FATAL_ERROR "${MESSAGE_TEXT}: ${TOKEN}")
    endif()
endfunction()

function(reject_token TEXT TOKEN MESSAGE_TEXT)
    string(FIND "${TEXT}" "${TOKEN}" _found)
    if(NOT _found EQUAL -1)
        message(FATAL_ERROR "${MESSAGE_TEXT}: ${TOKEN}")
    endif()
endfunction()

function(reject_dead_branch CODE MESSAGE_TEXT)
    if("${CODE}" MATCHES
            "if[ ]*(constexpr[ ]*)?\\([ ]*(false|0|0[ ]*==[ ]*1|1[ ]*==[ ]*0)[ ]*\\)"
            OR "${CODE}" MATCHES "#[ ]*if[ ]+0([^0-9]|$)"
            OR "${CODE}" MATCHES
                "if[ \\t\\r\\n]*\\([^\\{]*&&[ \\t\\r\\n]*false[ \\t\\r\\n]*\\)[ \\t\\r\\n]*\\{")
        message(FATAL_ERROR "${MESSAGE_TEXT} cannot live in a dead branch")
    endif()
endfunction()

function(reject_ascii_player_literals TEXT MESSAGE_TEXT)
    string(REGEX MATCHALL "(u8)?\"([^\"\\\\]|\\\\.)*\""
        _player_literals "${TEXT}")
    foreach(_literal IN LISTS _player_literals)
        string(REGEX REPLACE "^(u8)?\"" "" _content "${_literal}")
        string(REGEX REPLACE "\"$" "" _content "${_content}")
        string(REGEX REPLACE "%[-+0-9.]*[A-Za-z]" "" _content "${_content}")
        if("${_content}" MATCHES "[A-Za-z]")
            message(FATAL_ERROR
                "${MESSAGE_TEXT} contains player-facing English: ${_literal}")
        endif()
    endforeach()
endfunction()

file(READ "${RENDERER_SOURCE}" _renderer_source)
file(READ "${VIEW_HEADER}" _view_header)
file(READ "${VIEW_SOURCE}" _view_source)
normalize_cpp_source("${_renderer_source}"
    _renderer_normalized _renderer_code)
normalize_cpp_source("${_view_source}" _view_normalized _view_code)
normalize_cpp_source("${_view_header}" _header_normalized _header_code)
string(REGEX REPLACE "[ \t\r\n]+" "" _header_compact
    "${_header_normalized}")

foreach(_forbidden IN ITEMS
        "GetFontDefault(" "DrawText(" "MeasureText(" "LoadFontEx("
        "source.name" "node.name")
    reject_token("${_renderer_code}${_view_code}" "${_forbidden}"
        "T4 must use catalog names and fail closed on SharedUiFont")
endforeach()

foreach(_english IN ITEMS
        "PASSIVE STAR CHART" "Passive Points" " Close"
        "Autosave" "Starting node" "Click to allocate" "Click to refund"
        "Unknown passive" "Node is already" "Node cannot be refunded"
        "No passive points" "Allocate an adjacent"
        "Refund would disconnect" "Passive tree"
        "START" "CONNECTOR" "SMALL" "NOTABLE" "KEYSTONE")
    reject_token("${_renderer_normalized}${_view_normalized}" "${_english}"
        "T4 player-facing English remains active")
endforeach()

foreach(_token IN ITEMS
        "structPassiveNodeTextViewfinal{"
        "std::array<char,kPassiveNodeNameCapacity>name{};"
        "std::array<char,kPassiveNodeTypeStateCapacity>type_state{};"
        "std::array<char,kPassiveNodeBenefitCapacity>benefit{};"
        "std::array<char,kPassiveNodeCostCapacity>cost{};"
        "std::array<char,kPassiveNodeActionCapacity>action{};"
        "structPassiveTreeOverlayLayoutfinal{"
        "structPassiveTreeTooltipLayoutfinal{"
        "structPassiveNodeNameBoundsfinal{")
    require_token("${_header_compact}" "${_token}"
        "T4 fixed text/layout API is missing")
endforeach()

require_token("${_header_compact}"
    "inlineconstexprfloatkPassiveNodeVisibleRingExtension=3.0F;"
    "T4 visible-ring extension must remain a shared fixed constant")

extract_cpp_function_body("${_renderer_source}"
    "void draw_node_state_mark("
    _state_mark_body _state_mark_code)
require_token("${_state_mark_body}"
    "radius + kPassiveNodeVisibleRingExtension"
    "T4 visible ring must consume the shared extension")
reject_dead_branch("${_state_mark_code}" "T4 passive state mark")

extract_cpp_function_body("${_view_source}"
    "std::optional<passives::PassiveNodeId> hit_test_passive_node("
    _hit_test_body _hit_test_code)
foreach(_token IN ITEMS "std::max(12.0F,"
        "projection.radius + kPassiveNodeVisibleRingExtension")
    require_token("${_hit_test_body}" "${_token}"
        "T4 passive hit target must include the shared visible ring")
endforeach()
reject_dead_branch("${_hit_test_code}" "T4 passive hit target")

extract_cpp_function_body("${_view_source}"
    "PassiveNodeTextView passive_node_text_view("
    _text_view_body _text_view_code)
foreach(_token IN ITEMS "ui_passive_node_name(node)"
        "passive_node_visual_state(snapshot, node)"
        "ui_utf8_format(" "copy_complete(view.benefit"
        "copy_complete(view.cost" "copy_complete(view.action")
    require_token("${_text_view_body}" "${_token}"
        "T4 passive text view must use the catalog and fixed UTF-8 buffers")
endforeach()
reject_dead_branch("${_text_view_code}" "T4 passive text view")

foreach(_player_function IN ITEMS
        "const char* passive_node_type_text("
        "const char* passive_benefit_source("
        "const char* passive_cost_source("
        "const char* passive_node_action_text(")
    extract_cpp_function_body("${_view_source}" "${_player_function}"
        _player_body _player_code)
    reject_ascii_player_literals("${_player_body}"
        "T4 passive text mapping")
endforeach()

foreach(_token IN ITEMS
        "u8\"起始\"" "u8\"连接\"" "u8\"小节点\""
        "u8\"重要节点\"" "u8\"核心\""
        "u8\"锁定\"" "u8\"可分配\"" "u8\"已分配\""
        "u8\"保存中\"" "u8\"节点不可用\""
        "u8\"被动星图保存中…\"" "u8\"起始节点不可退款\""
        "u8\"点击退款并保存\"" "u8\"被动星图不可用\""
        "u8\"未分配点：0\"" "u8\"点击分配并保存\""
        "u8\"请分配相邻节点\""
        "PassiveNodeStateMark::locked_cross"
        "PassiveNodeStateMark::available_ring"
        "PassiveNodeStateMark::allocated_dot"
        "PassiveNodeStateMark::pending_hourglass"
        "u8\"收益：最终火焰伤害 ×1.30\""
        "u8\"代价：最终近战伤害 ×0.85\""
        "u8\"代价：最终最大生命 ×0.80\""
        "u8\"代价：最终承受伤害 ×1.15\"")
    require_token("${_view_normalized}" "${_token}"
        "T4 Chinese type/state/benefit/cost mapping is incomplete")
endforeach()

extract_cpp_function_body("${_view_source}"
    "PassiveTreeHeaderText passive_tree_header_text("
    _header_body _header_code)
foreach(_token IN ITEMS "passive_tree_binding_label"
        "ui_text(UiTextId::passive_points)"
        "ui_text(UiTextId::passive_close)" "ui_utf8_fit_with_ellipsis("
        "ui_utf8_format(")
    require_token("${_header_body}" "${_token}"
        "T4 dynamic points/close header is incomplete")
endforeach()
reject_dead_branch("${_header_code}" "T4 passive header text")

extract_cpp_function_body("${_view_source}"
    "PassiveTreeStatusView passive_tree_status_view("
    _status_body _status_code)
foreach(_token IN ITEMS "if (input.save_error)" "if (input.saving)"
        "switch (input.rule_error)" "u8\"保存状态：错误\""
        "u8\"保存状态：保存中\"" "u8\"保存状态：就绪\""
        "u8\"退款错误：将断链\"")
    require_token("${_status_body}" "${_token}"
        "T4 precise save/rule feedback is incomplete")
endforeach()
string(FIND "${_status_body}" "if (input.save_error)" _save_error_index)
string(FIND "${_status_body}" "if (input.saving)" _saving_index)
string(FIND "${_status_body}" "switch (input.rule_error)" _rule_index)
if(NOT _save_error_index LESS _saving_index
        OR NOT _saving_index LESS _rule_index)
    message(FATAL_ERROR
        "T4 save error must outrank saving and rule feedback")
endif()
reject_dead_branch("${_status_code}" "T4 passive status feedback")
reject_ascii_player_literals("${_status_body}"
    "T4 passive status feedback")

extract_cpp_function_body("${_renderer_source}"
    "void draw_centered_passive_tree_text("
    _draw_text_body _draw_text_code)
foreach(_token IN ITEMS
        "if (!use_passive_tree_hud_font(hud_font, hud_font_ready)"
        "opaque_backing.a != 255U"
        "ui_luma_contrast_ratio(color, opaque_backing) < 4.5F"
        "fit_centered_ui_text(" "layout.position"
        "draw_crisp_ui_text(hud_font, layout.text")
    require_token("${_draw_text_body}" "${_token}"
        "T4 SharedUiFont centered draw helper is incomplete")
endforeach()
reject_dead_branch("${_draw_text_code}" "T4 passive text helper")

extract_cpp_function_body("${_renderer_source}"
    "void draw_node_tooltip("
    _tooltip_body _tooltip_code)
foreach(_token IN ITEMS "passive_tree_tooltip_layout("
        "passive_node_text_view(snapshot, node)"
        "DrawRectangleRounded(layout.panel"
        "text.name.data(), layout.name"
        "text.type_state.data(), layout.type_state"
        "text.benefit.data(), layout.benefit"
        "text.cost.data(), layout.cost"
        "text.action.data(), layout.action")
    require_token("${_tooltip_body}" "${_token}"
        "T4 tooltip hierarchy must stay inside the safe layout")
endforeach()
reject_dead_branch("${_tooltip_code}" "T4 passive tooltip")

extract_cpp_function_body("${_renderer_source}"
    "void draw_passive_tree_overlay("
    _overlay_body _overlay_code)
reject_token("${_overlay_body}" "node.type"
    "T4 must draw the centered catalog short name for all 64 nodes")
foreach(_token IN ITEMS
        "constexpr Color kOverlayBacking{5, 8, 17, 255};"
        "passive_node_visual_cue(state)"
        "draw_node_state_mark(cue.mark, projection, color)"
        "passive_node_text_view(" "passive_node_name_bounds("
        "text.name.data(), name_bounds.bounds"
        "passive_tree_overlay_layout(" "passive_tree_header_text("
        "passive_tree_binding_label);"
        "ui_text(UiTextId::passive_title), layout.title"
        "header.points.data(), layout.points"
        "header.close.data(), layout.close"
        "status.text, layout.status")
    require_token("${_overlay_body}" "${_token}"
        "T4 live overlay must center Chinese text and expose non-color state cues")
endforeach()
reject_dead_branch("${_overlay_code}" "T4 passive overlay")

require_token("${_renderer_normalized}"
    "constexpr Color kTooltipBacking{9, 13, 22, 255};"
    "T4 tooltip backing must stay opaque")

message(STATUS "T4 passive-tree renderer source guard accepted")
