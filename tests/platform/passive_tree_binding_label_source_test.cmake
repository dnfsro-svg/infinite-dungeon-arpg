if(NOT DEFINED RAYLIB_SOURCE_DIR)
    message(FATAL_ERROR "RAYLIB_SOURCE_DIR is required")
endif()

include("${CMAKE_CURRENT_LIST_DIR}/cpp_source_lexer.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/../dungeon/evidence_source_scan.cmake")

file(READ "${RAYLIB_SOURCE_DIR}/raylib_host.cpp" HOST_SOURCE)
file(READ "${RAYLIB_SOURCE_DIR}/passive_tree_renderer.hpp" PASSIVE_HEADER_SOURCE)
file(READ "${RAYLIB_SOURCE_DIR}/passive_tree_renderer.cpp" PASSIVE_RENDERER_SOURCE)
file(READ "${RAYLIB_SOURCE_DIR}/passive_tree_view_math.cpp" PASSIVE_VIEW_SOURCE)
arpg_sanitize_cpp_source("${HOST_SOURCE}" HOST_CODE)
arpg_sanitize_cpp_source("${PASSIVE_HEADER_SOURCE}" PASSIVE_HEADER_CODE)
arpg_sanitize_cpp_source("${PASSIVE_RENDERER_SOURCE}" PASSIVE_RENDERER_CODE)
arpg_sanitize_cpp_source("${PASSIVE_VIEW_SOURCE}" PASSIVE_VIEW_CODE)
evidence_sanitize_cpp_for_scan(
    "${PASSIVE_RENDERER_SOURCE}" PASSIVE_RENDERER_POSITIONAL_CODE)

string(FIND "${PASSIVE_RENDERER_SOURCE}" "\"P Close\""
    LEGACY_PASSIVE_CLOSE_LABEL)
if(NOT LEGACY_PASSIVE_CLOSE_LABEL EQUAL -1)
    message(FATAL_ERROR
        "passive-tree overlay must not hard-code the default P binding")
endif()

set(DYNAMIC_PASSIVE_SIGNATURE [=[const char* passive_tree_binding_label,
    Font hud_font, bool hud_font_ready) noexcept]=])
string(FIND "${PASSIVE_HEADER_CODE}" "${DYNAMIC_PASSIVE_SIGNATURE}"
    DYNAMIC_PASSIVE_HEADER_SIGNATURE_INDEX)
string(FIND "${PASSIVE_RENDERER_CODE}" "${DYNAMIC_PASSIVE_SIGNATURE}"
    DYNAMIC_PASSIVE_RENDERER_SIGNATURE_INDEX)
if(DYNAMIC_PASSIVE_HEADER_SIGNATURE_INDEX EQUAL -1
        OR DYNAMIC_PASSIVE_RENDERER_SIGNATURE_INDEX EQUAL -1)
    message(FATAL_ERROR
        "passive-tree renderer must receive the applied binding label")
endif()

foreach(FORBIDDEN_CALL IN ITEMS "DrawText(" "MeasureText(" "GetFontDefault(")
    string(FIND "${PASSIVE_RENDERER_CODE}" "${FORBIDDEN_CALL}"
        FORBIDDEN_CALL_INDEX)
    if(NOT FORBIDDEN_CALL_INDEX EQUAL -1)
        message(FATAL_ERROR
            "passive-tree text must fail closed on SharedUiFont: ${FORBIDDEN_CALL}")
    endif()
endforeach()

foreach(REQUIRED_SHARED_FONT_TOKEN IN ITEMS
        "MeasureTextEx(" "draw_crisp_ui_text("
        "fit_centered_ui_text(" "layout.font_size" "layout.spacing")
    string(FIND "${PASSIVE_RENDERER_CODE}" "${REQUIRED_SHARED_FONT_TOKEN}"
        REQUIRED_SHARED_FONT_INDEX)
    if(REQUIRED_SHARED_FONT_INDEX EQUAL -1)
        message(FATAL_ERROR
            "passive-tree text must measure and draw through SharedUiFont: ${REQUIRED_SHARED_FONT_TOKEN}")
    endif()
endforeach()

foreach(REQUIRED_BINDING_TOKEN IN ITEMS
        "passive_tree_header_text("
        "passive_tree_binding_label"
        "header.close.data(), layout.close"
        "ui_text(UiTextId::passive_close)")
    string(FIND "${PASSIVE_RENDERER_CODE}${PASSIVE_VIEW_CODE}"
        "${REQUIRED_BINDING_TOKEN}" REQUIRED_BINDING_INDEX)
    if(REQUIRED_BINDING_INDEX EQUAL -1)
        message(FATAL_ERROR
            "passive-tree close label must combine the live binding with Chinese close text: ${REQUIRED_BINDING_TOKEN}")
    endif()
endforeach()

foreach(FORBIDDEN_ENGLISH IN ITEMS " Close" "PASSIVE STAR CHART"
        "Passive Points" "Autosave")
    string(FIND "${PASSIVE_RENDERER_SOURCE}${PASSIVE_VIEW_SOURCE}"
        "${FORBIDDEN_ENGLISH}" FORBIDDEN_ENGLISH_INDEX)
    if(NOT FORBIDDEN_ENGLISH_INDEX EQUAL -1)
        message(FATAL_ERROR
            "passive-tree binding/status path still contains player-facing English: ${FORBIDDEN_ENGLISH}")
    endif()
endforeach()

string(REGEX MATCHALL "draw_passive_tree_overlay\\(" PASSIVE_OVERLAY_CALLS
    "${HOST_CODE}")
list(LENGTH PASSIVE_OVERLAY_CALLS PASSIVE_OVERLAY_CALL_COUNT)
if(NOT PASSIVE_OVERLAY_CALL_COUNT EQUAL 1)
    message(FATAL_ERROR
        "host must contain exactly one passive-tree overlay call")
endif()

set(APPLIED_PASSIVE_OVERLAY_BLOCK [=[if (!config.stage12_material_background_only
                && !config.stage12_material_icons_only
                && passive_overlay_open) {
                draw_passive_tree_overlay(current, runtime.render_status(),
                    stable_key_label(settings::binding_for(input_settings,
                        settings::SettingAction::passive_tree)),
                    renderer.hud_font(), renderer.hud_font_ready());
            }]=])
string(FIND "${HOST_CODE}" "${APPLIED_PASSIVE_OVERLAY_BLOCK}"
    APPLIED_PASSIVE_OVERLAY_BLOCK_INDEX)
if(APPLIED_PASSIVE_OVERLAY_BLOCK_INDEX EQUAL -1)
    message(FATAL_ERROR
        "the unique overlay call must use the applied passive-tree binding")
endif()

message(STATUS
    "[CPLAY-031] passive-tree close label follows the applied binding")
