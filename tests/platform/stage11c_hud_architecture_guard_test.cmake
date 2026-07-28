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
set(_validation_input_sources
    "${_hud_root}/host_validation_input.hpp"
    "${_hud_root}/host_validation_input.cpp")
set(_validation_navigation_sources
    "${_hud_root}/host_validation_navigation.hpp"
    "${_hud_root}/host_validation_navigation.cpp")
set(_hud_boundary_sources
    ${_hud_sources} ${_combat_sources} ${_debug_overlay_sources}
    ${_validation_input_sources} ${_validation_navigation_sources})
foreach(_required_source IN LISTS _hud_boundary_sources)
    if(NOT EXISTS "${_required_source}")
        message(FATAL_ERROR
            "Stage11C HUD boundary source is missing: ${_required_source}")
    endif()
endforeach()

foreach(_validation_header IN ITEMS
        "${_hud_root}/host_validation_input.hpp"
        "${_hud_root}/host_validation_navigation.hpp")
    file(READ "${_validation_header}" _validation_header_text)
    if(_validation_header_text MATCHES "raylib[.]h|renderer|persistence|test")
        message(FATAL_ERROR
            "Stage11C validation boundary header has a forbidden dependency: ${_validation_header}")
    endif()
endforeach()
if(NOT EXISTS "${_host_source}")
    message(FATAL_ERROR "Stage11C HUD host source is missing: ${_host_source}")
endif()

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

file(READ "${_host_source}" _host_text)
arpg_extract_hud_host_seam("${_host_text}"
    "renderer.observe_presented_hud_frame(HudPresentedFrame::recovery,"
    "GetFrameTime(), true);"
    "Host HUD recovery observation seam is invalid" _recovery_hud_seam)
arpg_extract_hud_host_seam("${_host_text}"
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
