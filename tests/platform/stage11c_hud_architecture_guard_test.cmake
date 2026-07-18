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

function(arpg_hud_assert_excludes REASON REGEX)
    foreach(_source_file IN LISTS _hud_sources)
        file(READ "${_source_file}" _source_text)
        if(_source_text MATCHES "${REGEX}")
            message(FATAL_ERROR "${REASON}: ${_source_file}")
        endif()
    endforeach()
endfunction()

arpg_hud_assert_excludes("HUD boundary rejects physical input sampling"
    "(^|[^A-Za-z0-9_])GetKeyPressed[ \\t\\r\\n]*[(]")
arpg_hud_assert_excludes("HUD boundary rejects DungeonSession access"
    "(^|[^A-Za-z0-9_])DungeonSession([^A-Za-z0-9_]|$)")
arpg_hud_assert_excludes("HUD boundary rejects fixed tick access"
    "(^|[^A-Za-z0-9_])fixed_tick[ \\t\\r\\n]*[(]")
arpg_hud_assert_excludes("HUD boundary rejects SettingsStore access"
    "(^|[^A-Za-z0-9_])SettingsStore([^A-Za-z0-9_]|$)")
arpg_hud_assert_excludes("HUD boundary rejects dynamic std::string"
    "std[ \\t\\r\\n]*::[ \\t\\r\\n]*string([^A-Za-z0-9_]|$)")
arpg_hud_assert_excludes("HUD boundary rejects dynamic std::vector"
    "std[ \\t\\r\\n]*::[ \\t\\r\\n]*vector[ \\t\\r\\n]*[<]")
arpg_hud_assert_excludes("Normal HUD rejects legacy Budget text"
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
