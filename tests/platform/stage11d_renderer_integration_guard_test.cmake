if(NOT DEFINED SOURCE_ROOT)
    message(FATAL_ERROR "SOURCE_ROOT is required")
endif()

set(_header "${SOURCE_ROOT}/src/platform/raylib/combat_renderer.hpp")
set(_source "${SOURCE_ROOT}/src/platform/raylib/combat_renderer.cpp")
foreach(_required IN ITEMS "${_header}" "${_source}")
    if(NOT EXISTS "${_required}")
        message(FATAL_ERROR "Stage11D renderer source is missing: ${_required}")
    endif()
endforeach()

file(READ "${_header}" _header_text)
file(READ "${_source}" _source_text)
set(_combined "${_header_text}\n${_source_text}")

foreach(_forbidden IN ITEMS
        "GroundLootRenderConsumers"
        "ground_loot_render_consumers"
        "room_icons"
        "hud_labels")
    string(FIND "${_combined}" "${_forbidden}" _found)
    if(NOT _found EQUAL -1)
        message(FATAL_ERROR
            "Stage11D renderer exposes forbidden loot consumer wrapper: ${_forbidden}")
    endif()
endforeach()

string(REGEX REPLACE "[ \t\r\n]+" "" _normalized "${_source_text}")
foreach(_required IN ITEMS
        "draw_room(current,render_plan.ground_loot);"
        "hud_renderer_.draw_ground_loot(render_plan.ground_loot);")
    string(FIND "${_normalized}" "${_required}" _found)
    if(_found EQUAL -1)
        message(FATAL_ERROR
            "Stage11D renderer stage does not consume the owned plan field directly: ${_required}")
    endif()
endforeach()

string(REGEX MATCHALL "build_ground_loot_view[ \t\r\n]*\\(" _builds
    "${_source_text}")
list(LENGTH _builds _build_count)
if(NOT _build_count EQUAL 1)
    message(FATAL_ERROR
        "Stage11D renderer requires exactly one GroundLootView build, got ${_build_count}")
endif()
