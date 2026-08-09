cmake_minimum_required(VERSION 3.24)

if(NOT DEFINED SOURCE_ROOT OR NOT DEFINED MUTATION_ROOT)
    message(FATAL_ERROR "SOURCE_ROOT and MUTATION_ROOT are required")
endif()

set(guard "${SOURCE_ROOT}/tests/platform/shared_ui_font_lifecycle_guard_test.cmake")
file(REMOVE_RECURSE "${MUTATION_ROOT}")
file(MAKE_DIRECTORY "${MUTATION_ROOT}")

function(expect_rejected label owner_text host_text)
    set(owner_file "${MUTATION_ROOT}/${label}-owner.cpp")
    set(host_file "${MUTATION_ROOT}/${label}-host.cpp")
    file(WRITE "${owner_file}" "${owner_text}")
    file(WRITE "${host_file}" "${host_text}")
    execute_process(COMMAND "${CMAKE_COMMAND}"
        "-DSOURCE_ROOT=${SOURCE_ROOT}"
        "-DOWNER_SOURCE_OVERRIDE=${owner_file}"
        "-DHOST_SOURCE_OVERRIDE=${host_file}"
        -P "${guard}"
        RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
    if(result EQUAL 0)
        message(FATAL_ERROR "negative mutation was accepted: ${label}\n${output}\n${error}")
    endif()
endfunction()

execute_process(COMMAND "${CMAKE_COMMAND}" "-DSOURCE_ROOT=${SOURCE_ROOT}" -P "${guard}"
    RESULT_VARIABLE pristine_result OUTPUT_VARIABLE pristine_output ERROR_VARIABLE pristine_error)
if(NOT pristine_result EQUAL 0)
    message(FATAL_ERROR "pristine lifecycle guard failed\n${pristine_output}\n${pristine_error}")
endif()

file(READ "${SOURCE_ROOT}/src/platform/raylib/shared_ui_font.cpp" owner)
file(READ "${SOURCE_ROOT}/src/platform/raylib/raylib_host.cpp" host)

expect_rejected("duplicate-load" "${owner}\nvoid duplicate_load() { LoadFontEx(); }\n" "${host}")
expect_rejected("duplicate-unload" "${owner}\nvoid duplicate_unload() { UnloadFont(); }\n" "${host}")

string(REPLACE "if (ready_) return true;" "if (false) return true;" non_idempotent "${owner}")
expect_rejected("non-idempotent" "${non_idempotent}" "${host}")

string(REPLACE "shared_ui_font.active_instance_count()" "kUiFontAtlasInstanceCount" fake_instances "${host}")
expect_rejected("fabricated-instances" "${owner}" "${fake_instances}")
string(REPLACE "shared_ui_font.total_atlas_bytes()" "material_status.bundled_font_atlas_bytes" fake_total "${host}")
expect_rejected("fabricated-total" "${owner}" "${fake_total}")

set(ordered_success "audio.shutdown();\n        pause_menu_renderer.shutdown();\n        renderer.shutdown_resources();\n        shared_ui_font.shutdown();\n        window.close();\n        return HostExitCode::success;")
set(reordered_success "audio.shutdown();\n        renderer.shutdown_resources();\n        pause_menu_renderer.shutdown();\n        shared_ui_font.shutdown();\n        window.close();\n        return HostExitCode::success;")
string(REPLACE "${ordered_success}" "${reordered_success}" bad_success "${host}")
expect_rejected("success-order" "${owner}" "${bad_success}")

set(ordered_early "audio.shutdown();\n                pause_menu_renderer.shutdown();\n                renderer.shutdown_resources();\n                shared_ui_font.shutdown();\n                window.close();\n                return HostExitCode::save_initialization_failed;")
set(reordered_early "audio.shutdown();\n                renderer.shutdown_resources();\n                pause_menu_renderer.shutdown();\n                shared_ui_font.shutdown();\n                window.close();\n                return HostExitCode::save_initialization_failed;")
string(REPLACE "${ordered_early}" "${reordered_early}" bad_early "${host}")
expect_rejected("render-snapshot-order" "${owner}" "${bad_early}")

set(catch_order "        if (pause_menu_renderer_storage != nullptr) {\n            pause_menu_renderer_storage->shutdown();\n        }\n        if (renderer_storage != nullptr) {\n            renderer_storage->shutdown_resources();\n        }\n        shared_ui_font.shutdown();\n        window.close();\n        return HostExitCode::save_initialization_failed;")
set(catch_reordered "        if (renderer_storage != nullptr) {\n            renderer_storage->shutdown_resources();\n        }\n        if (pause_menu_renderer_storage != nullptr) {\n            pause_menu_renderer_storage->shutdown();\n        }\n        shared_ui_font.shutdown();\n        window.close();\n        return HostExitCode::save_initialization_failed;")
string(FIND "${host}" "catch (const std::exception& exception)" std_start)
string(SUBSTRING "${host}" ${std_start} -1 catch_tail)
string(FIND "${catch_tail}" "catch (...)" unknown_relative)
math(EXPR unknown_start "${std_start} + ${unknown_relative}")
string(SUBSTRING "${host}" 0 ${unknown_start} through_std)
string(SUBSTRING "${host}" ${unknown_start} -1 unknown_tail)
string(REPLACE "${catch_order}" "${catch_reordered}" bad_std_head "${through_std}")
set(bad_std "${bad_std_head}${unknown_tail}")
expect_rejected("std-catch-order" "${owner}" "${bad_std}")

string(REPLACE "${catch_order}" "${catch_reordered}" bad_unknown_tail "${unknown_tail}")
string(SUBSTRING "${host}" 0 ${unknown_start} before_unknown)
set(bad_unknown "${before_unknown}${bad_unknown_tail}")
expect_rejected("unknown-catch-order" "${owner}" "${bad_unknown}")

message(STATUS "shared UI font lifecycle guard self-test rejected all mutations")
