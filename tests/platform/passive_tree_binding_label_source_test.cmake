if(NOT DEFINED RAYLIB_SOURCE_DIR)
    message(FATAL_ERROR "RAYLIB_SOURCE_DIR is required")
endif()

include("${CMAKE_CURRENT_LIST_DIR}/cpp_source_lexer.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/../dungeon/evidence_source_scan.cmake")

file(READ "${RAYLIB_SOURCE_DIR}/raylib_host.cpp" HOST_SOURCE)
file(READ "${RAYLIB_SOURCE_DIR}/passive_tree_renderer.hpp" PASSIVE_HEADER_SOURCE)
file(READ "${RAYLIB_SOURCE_DIR}/passive_tree_renderer.cpp" PASSIVE_RENDERER_SOURCE)
arpg_sanitize_cpp_source("${HOST_SOURCE}" HOST_CODE)
arpg_sanitize_cpp_source("${PASSIVE_HEADER_SOURCE}" PASSIVE_HEADER_CODE)
arpg_sanitize_cpp_source("${PASSIVE_RENDERER_SOURCE}" PASSIVE_RENDERER_CODE)
evidence_sanitize_cpp_for_scan(
    "${PASSIVE_RENDERER_SOURCE}" PASSIVE_RENDERER_POSITIONAL_CODE)

string(FIND "${PASSIVE_RENDERER_SOURCE}" "\"P Close\""
    LEGACY_PASSIVE_CLOSE_LABEL)
if(NOT LEGACY_PASSIVE_CLOSE_LABEL EQUAL -1)
    message(FATAL_ERROR
        "passive-tree overlay must not hard-code the default P binding")
endif()

set(DYNAMIC_PASSIVE_SIGNATURE [=[const char* passive_tree_binding_label) noexcept]=])
string(FIND "${PASSIVE_HEADER_CODE}" "${DYNAMIC_PASSIVE_SIGNATURE}"
    DYNAMIC_PASSIVE_HEADER_SIGNATURE_INDEX)
string(FIND "${PASSIVE_RENDERER_CODE}" "${DYNAMIC_PASSIVE_SIGNATURE}"
    DYNAMIC_PASSIVE_RENDERER_SIGNATURE_INDEX)
if(DYNAMIC_PASSIVE_HEADER_SIGNATURE_INDEX EQUAL -1
        OR DYNAMIC_PASSIVE_RENDERER_SIGNATURE_INDEX EQUAL -1)
    message(FATAL_ERROR
        "passive-tree renderer must receive the applied binding label")
endif()

set(DYNAMIC_PASSIVE_DRAW_CODE [=[constexpr int kCloseFontSize = 17;
    constexpr const char* kCloseSuffix =  ;
    const int binding_width = MeasureText(
        passive_tree_binding_label, kCloseFontSize);
    const int close_width = binding_width
        + MeasureText(kCloseSuffix, kCloseFontSize);
    const int close_x = (std::max)(38, GetScreenWidth() - 38 - close_width);
    const Color close_color{201, 213, 232, 255};
    DrawText(passive_tree_binding_label, close_x, 38, kCloseFontSize,
        close_color);
    DrawText(kCloseSuffix, close_x + binding_width, 38, kCloseFontSize,
        close_color);]=])
string(FIND "${PASSIVE_RENDERER_CODE}" "${DYNAMIC_PASSIVE_DRAW_CODE}"
    DYNAMIC_PASSIVE_DRAW_INDEX)
if(DYNAMIC_PASSIVE_DRAW_INDEX EQUAL -1)
    message(FATAL_ERROR
        "passive-tree close label must include Close and right-align its total width")
endif()

set(RAW_CLOSE_SUFFIX_DECLARATION
    [=[constexpr const char* kCloseSuffix = " Close";]=])
string(FIND "${PASSIVE_RENDERER_SOURCE}" "${RAW_CLOSE_SUFFIX_DECLARATION}"
    RAW_CLOSE_SUFFIX_INDEX)
if(RAW_CLOSE_SUFFIX_INDEX EQUAL -1)
    message(FATAL_ERROR
        "passive-tree close label must contain the Close suffix")
endif()
string(LENGTH "${RAW_CLOSE_SUFFIX_DECLARATION}" RAW_CLOSE_SUFFIX_LENGTH)
string(SUBSTRING "${PASSIVE_RENDERER_POSITIONAL_CODE}"
    ${RAW_CLOSE_SUFFIX_INDEX} ${RAW_CLOSE_SUFFIX_LENGTH}
    ACTIVE_CLOSE_SUFFIX_DECLARATION)
string(FIND "${ACTIVE_CLOSE_SUFFIX_DECLARATION}"
    "constexpr const char* kCloseSuffix =" ACTIVE_CLOSE_SUFFIX_CODE_INDEX)
if(NOT ACTIVE_CLOSE_SUFFIX_CODE_INDEX EQUAL 0)
    message(FATAL_ERROR
        "the Close suffix declaration must be active C++ code")
endif()

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
                        settings::SettingAction::passive_tree)));
            }]=])
string(FIND "${HOST_CODE}" "${APPLIED_PASSIVE_OVERLAY_BLOCK}"
    APPLIED_PASSIVE_OVERLAY_BLOCK_INDEX)
if(APPLIED_PASSIVE_OVERLAY_BLOCK_INDEX EQUAL -1)
    message(FATAL_ERROR
        "the unique overlay call must use the applied passive-tree binding")
endif()

message(STATUS
    "[CPLAY-031] passive-tree close label follows the applied binding")
