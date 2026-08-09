cmake_minimum_required(VERSION 3.24)

if(NOT DEFINED SOURCE_ROOT)
    message(FATAL_ERROR "SOURCE_ROOT is required")
endif()

set(owner_header "${SOURCE_ROOT}/src/platform/raylib/shared_ui_font.hpp")
set(owner_source "${SOURCE_ROOT}/src/platform/raylib/shared_ui_font.cpp")
set(host_source "${SOURCE_ROOT}/src/platform/raylib/raylib_host.cpp")
set(stage16_source "${SOURCE_ROOT}/tests/platform/stage16_loot_reinforcement_game_validation.cpp")
if(DEFINED OWNER_HEADER_OVERRIDE)
    set(owner_header "${OWNER_HEADER_OVERRIDE}")
endif()
if(DEFINED OWNER_SOURCE_OVERRIDE)
    set(owner_source "${OWNER_SOURCE_OVERRIDE}")
endif()
if(DEFINED HOST_SOURCE_OVERRIDE)
    set(host_source "${HOST_SOURCE_OVERRIDE}")
endif()
if(DEFINED STAGE16_SOURCE_OVERRIDE)
    set(stage16_source "${STAGE16_SOURCE_OVERRIDE}")
endif()

foreach(required IN ITEMS "${owner_header}" "${owner_source}" "${host_source}" "${stage16_source}")
    if(NOT EXISTS "${required}")
        message(FATAL_ERROR "shared UI font lifecycle guard input is missing: ${required}")
    endif()
endforeach()

file(READ "${owner_header}" owner_header_text)
file(READ "${owner_source}" owner_source_text)
file(READ "${host_source}" host_text)
file(READ "${stage16_source}" stage16_text)

function(normalize input output)
    string(REGEX REPLACE "[ \t\r\n]+" "" compact "${input}")
    set(${output} "${compact}" PARENT_SCOPE)
endfunction()

function(require_count text pattern expected diagnostic)
    string(REGEX MATCHALL "${pattern}" matches "${text}")
    list(LENGTH matches actual)
    if(NOT actual EQUAL expected)
        message(FATAL_ERROR "${diagnostic}: expected ${expected}, actual ${actual}")
    endif()
endfunction()

function(require_literal_count text literal expected diagnostic)
    string(LENGTH "${text}" before_length)
    string(REPLACE "${literal}" "" without "${text}")
    string(LENGTH "${without}" after_length)
    string(LENGTH "${literal}" literal_length)
    math(EXPR removed "${before_length} - ${after_length}")
    math(EXPR actual "${removed} / ${literal_length}")
    if(NOT actual EQUAL expected)
        message(FATAL_ERROR "${diagnostic}: expected ${expected}, actual ${actual}")
    endif()
endfunction()

require_count("${owner_source_text}" "LoadFontEx[ \t\r\n]*\\(" 1
    "SharedUiFont must contain exactly one LoadFontEx call site")
require_count("${owner_source_text}" "UnloadFont[ \t\r\n]*\\(" 1
    "SharedUiFont must contain exactly one UnloadFont call site")

file(GLOB raylib_sources "${SOURCE_ROOT}/src/platform/raylib/*.cpp")
foreach(raylib_source IN LISTS raylib_sources)
    if(raylib_source STREQUAL "${SOURCE_ROOT}/src/platform/raylib/shared_ui_font.cpp")
        continue()
    endif()
    file(READ "${raylib_source}" raylib_text)
    if(raylib_text MATCHES "LoadFontEx[ \t\r\n]*\\(" OR raylib_text MATCHES "UnloadFont[ \t\r\n]*\\(")
        message(FATAL_ERROR "renderer must borrow the owner font; load/unload found in ${raylib_source}")
    endif()
endforeach()

normalize("${owner_header_text}" owner_header_norm)
normalize("${owner_source_text}" owner_source_norm)
normalize("${host_text}" host_norm)
normalize("${stage16_text}" stage16_norm)

foreach(required IN ITEMS
        "SharedUiFont(constSharedUiFont&)=delete;"
        "SharedUiFont&operator=(constSharedUiFont&)=delete;"
        "SharedUiFont(SharedUiFont&&)=delete;"
        "SharedUiFont&operator=(SharedUiFont&&)=delete;")
    string(FIND "${owner_header_norm}" "${required}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "SharedUiFont copy/move ownership guard is missing: ${required}")
    endif()
endforeach()

foreach(required IN ITEMS
        "boolSharedUiFont::initialize()noexcept{if(ready_)returntrue;"
        "IsTextureValid(font.texture)"
        "font.texture.format"
        "ui_font_candidate_is_acceptable(candidate,plan_)"
        "release_font(candidate);"
        "std::size_tSharedUiFont::active_instance_count()constnoexcept"
        "std::size_tSharedUiFont::total_atlas_bytes()constnoexcept")
    string(FIND "${owner_source_norm}" "${required}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "SharedUiFont lifecycle/validation seam is missing: ${required}")
    endif()
endforeach()

require_literal_count("${host_norm}" "SharedUiFontshared_ui_font{};" 1
    "host must own exactly one SharedUiFont")
require_literal_count("${host_norm}" "shared_ui_font.initialize()" 1
    "host must initialize its SharedUiFont exactly once")

foreach(required IN ITEMS
        "material_status.bundled_font_instance_count=static_cast<std::uint8_t>(shared_ui_font.active_instance_count());"
        "material_status.bundled_font_total_atlas_bytes=shared_ui_font.total_atlas_bytes();"
        "audio.shutdown();pause_menu_renderer.shutdown();renderer.shutdown_resources();shared_ui_font.shutdown();window.close();returnHostExitCode::success;"
        "audio.shutdown();pause_menu_renderer.shutdown();renderer.shutdown_resources();shared_ui_font.shutdown();window.close();returnHostExitCode::save_initialization_failed;")
    string(FIND "${host_norm}" "${required}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "host owner metric or ordered shutdown seam is missing: ${required}")
    endif()
endforeach()

set(catch_order "pause_menu_renderer_storage->shutdown();}if(renderer_storage!=nullptr){renderer_storage->shutdown_resources();}shared_ui_font.shutdown();window.close();returnHostExitCode::save_initialization_failed;")
string(FIND "${host_norm}" "catch(conststd::exception&exception)" std_catch_start)
if(std_catch_start EQUAL -1)
    message(FATAL_ERROR "host exception branches are missing")
endif()
string(SUBSTRING "${host_norm}" ${std_catch_start} -1 catch_tail)
string(FIND "${catch_tail}" "catch(...)" unknown_catch_relative)
if(unknown_catch_relative EQUAL -1)
    message(FATAL_ERROR "host unknown exception branch is missing")
endif()
math(EXPR unknown_catch_start "${std_catch_start} + ${unknown_catch_relative}")
math(EXPR std_catch_length "${unknown_catch_start} - ${std_catch_start}")
string(SUBSTRING "${host_norm}" ${std_catch_start} ${std_catch_length} std_catch)
string(SUBSTRING "${host_norm}" ${unknown_catch_start} -1 unknown_catch)
foreach(catch_branch IN ITEMS std_catch unknown_catch)
    string(FIND "${${catch_branch}}" "${catch_order}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "host ${catch_branch} must close borrower, owner, then window")
    endif()
endforeach()

require_literal_count("${stage16_norm}" "SharedUiFontshared_ui_font{};" 1
    "Stage16 must own exactly one SharedUiFont")
require_literal_count("${stage16_norm}" "shared_ui_font.initialize()" 2
    "Stage16 must initialize once and exercise idempotent initialize once")
if(stage16_norm MATCHES "initialize_resources\\(\\)")
    message(FATAL_ERROR "Stage16 still contains a no-argument CombatRenderer initialization")
endif()
foreach(required IN ITEMS
        "room->initialize_resources(shared_ui_font)"
        "constautoscenario=run_production_scenario(run,shared_ui_font);"
        "shared_ui_font.shutdown();CloseWindow();")
    string(FIND "${stage16_norm}" "${required}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Stage16 shared owner wiring/order is missing: ${required}")
    endif()
endforeach()

message(STATUS "shared UI font lifecycle guard accepted")
