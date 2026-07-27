if(NOT DEFINED SOURCE_ROOT OR NOT DEFINED GUARD_TEST_ROOT)
    message(FATAL_ERROR "SOURCE_ROOT and GUARD_TEST_ROOT are required")
endif()
set(_guard "${SOURCE_ROOT}/tests/platform/stage11d_loot_evidence_guard_test.cmake")
set(_host "${SOURCE_ROOT}/src/platform/raylib/raylib_host.cpp")
set(_renderer "${SOURCE_ROOT}/src/platform/raylib/combat_renderer.cpp")
set(_formal "${SOURCE_ROOT}/tests/platform/stage11d_loot_formal_game_validation.cpp")
set(_validator "${SOURCE_ROOT}/tests/platform/stage11d_loot_formal_validator.ps1")
foreach(_file IN ITEMS "${_guard}" "${_host}" "${_renderer}"
        "${_formal}" "${_validator}")
    if(NOT EXISTS "${_file}")
        message(FATAL_ERROR "Stage11D guard self-test input is missing: ${_file}")
    endif()
endforeach()
file(REMOVE_RECURSE "${GUARD_TEST_ROOT}")
file(MAKE_DIRECTORY "${GUARD_TEST_ROOT}")

execute_process(COMMAND "${CMAKE_COMMAND}" "-DSOURCE_ROOT=${SOURCE_ROOT}"
    -P "${_guard}" RESULT_VARIABLE _baseline OUTPUT_QUIET ERROR_QUIET)
if(NOT _baseline EQUAL 0)
    message(FATAL_ERROR "Stage11D loot evidence guard rejected its baseline")
endif()

function(expect_rejected NAME OVERRIDE PATH EXPECTED_REASON)
    execute_process(COMMAND "${CMAKE_COMMAND}" "-DSOURCE_ROOT=${SOURCE_ROOT}"
        "-D${OVERRIDE}_OVERRIDE=${PATH}" -P "${_guard}"
        RESULT_VARIABLE _result OUTPUT_VARIABLE _output ERROR_VARIABLE _error)
    if(_result EQUAL 0)
        message(FATAL_ERROR "Stage11D loot evidence guard accepted mutation: ${NAME}")
    endif()
    set(_log "${_output}\n${_error}")
    string(REGEX REPLACE "[ \t\r\n]+" " " _normalized_log "${_log}")
    string(REGEX REPLACE "[ \t\r\n]+" " " _normalized_reason
        "${EXPECTED_REASON}")
    string(FIND "${_normalized_log}" "${_normalized_reason}" _reason)
    if(_reason EQUAL -1)
        message(FATAL_ERROR
            "Stage11D guard rejected ${NAME} for the wrong reason: ${_log}")
    endif()
endfunction()

file(READ "${_formal}" _formal_text)
set(_site "platform::RaylibHostConfig config{};")
string(FIND "${_formal_text}" "${_site}" _site_index)
if(_site_index EQUAL -1)
    message(FATAL_ERROR "private-injection mutation site disappeared")
endif()
string(REPLACE "${_site}"
    "TestAccess::inject(config);\n    ${_site}" _mutated "${_formal_text}")
if(_mutated STREQUAL _formal_text)
    message(FATAL_ERROR "private-injection mutation made no change")
endif()
set(_path "${GUARD_TEST_ROOT}/private-injection.cpp")
file(WRITE "${_path}" "${_mutated}")
expect_rejected("private injection" FORMAL "${_path}" "formal bypass: TestAccess")

string(REPLACE "${_site}"
    "FakeRenderer fake_renderer{};\n    ${_site}" _mutated "${_formal_text}")
if(_mutated STREQUAL _formal_text)
    message(FATAL_ERROR "fake-renderer mutation made no change")
endif()
set(_path "${GUARD_TEST_ROOT}/fake-renderer.cpp")
file(WRITE "${_path}" "${_mutated}")
expect_rejected("non-production renderer" FORMAL "${_path}"
    "formal bypass: FakeRenderer")

set(_site "return platform::run_raylib_host(config)")
string(FIND "${_formal_text}" "${_site}" _site_index)
if(_site_index EQUAL -1)
    message(FATAL_ERROR "direct-pickup mutation site disappeared")
endif()
string(REPLACE "${_site}" "request_pickup(0);\n    ${_site}"
    _mutated "${_formal_text}")
if(_mutated STREQUAL _formal_text)
    message(FATAL_ERROR "direct-pickup mutation made no change")
endif()
set(_path "${GUARD_TEST_ROOT}/direct-pickup.cpp")
file(WRITE "${_path}" "${_mutated}")
expect_rejected("direct pickup completion" FORMAL "${_path}"
    "formal bypass: request_pickup(")

set(_site "draft.loot_filter_mode = spec.mode;")
string(FIND "${_formal_text}" "${_site}" _site_index)
if(_site_index EQUAL -1)
    message(FATAL_ERROR "fake-settings mutation site disappeared")
endif()
string(REPLACE "${_site}"
    "pause_menu.committed.loot_filter_mode = spec.mode;"
    _mutated "${_formal_text}")
if(_mutated STREQUAL _formal_text)
    message(FATAL_ERROR "fake-settings mutation made no change")
endif()
set(_path "${GUARD_TEST_ROOT}/fake-settings.cpp")
file(WRITE "${_path}" "${_mutated}")
expect_rejected("fake settings publication" FORMAL "${_path}"
    "pause_menu.committed")

set(_site "|| absolute.filename() != \"stage11d loot evidence\"")
string(FIND "${_formal_text}" "${_site}" _site_index)
if(_site_index EQUAL -1)
    message(FATAL_ERROR "unsafe-root mutation site disappeared")
endif()
string(REPLACE "${_site}" "|| false" _mutated "${_formal_text}")
if(_mutated STREQUAL _formal_text)
    message(FATAL_ERROR "unsafe-root mutation made no change")
endif()
set(_path "${GUARD_TEST_ROOT}/unsafe-root.cpp")
file(WRITE "${_path}" "${_mutated}")
expect_rejected("malicious evidence root acceptance" FORMAL "${_path}"
    "formal cleanup safety is incomplete: absolute.filename()")

set(_site "FILE_ATTRIBUTE_REPARSE_POINT")
string(FIND "${_formal_text}" "${_site}" _site_index)
if(_site_index EQUAL -1)
    message(FATAL_ERROR "reparse-safety mutation site disappeared")
endif()
string(REPLACE "${_site}" "FILE_ATTRIBUTE_DIRECTORY"
    _mutated "${_formal_text}")
if(_mutated STREQUAL _formal_text)
    message(FATAL_ERROR "reparse-safety mutation made no change")
endif()
set(_path "${GUARD_TEST_ROOT}/reparse-safety.cpp")
file(WRITE "${_path}" "${_mutated}")
expect_rejected("reparse-point evidence escape" FORMAL "${_path}"
    "formal cleanup safety is incomplete: FILE_ATTRIBUTE_REPARSE_POINT")

set(_site "std::filesystem::create_directories(root, error);")
string(FIND "${_formal_text}" "${_site}" _site_index)
if(_site_index EQUAL -1)
    message(FATAL_ERROR "recursive-cleanup mutation site disappeared")
endif()
string(REPLACE "${_site}"
    "std::filesystem::remove_all(root, error);\n    ${_site}"
    _mutated "${_formal_text}")
if(_mutated STREQUAL _formal_text)
    message(FATAL_ERROR "recursive-cleanup mutation made no change")
endif()
set(_path "${GUARD_TEST_ROOT}/recursive-cleanup.cpp")
file(WRITE "${_path}" "${_mutated}")
expect_rejected("recursive evidence cleanup" FORMAL "${_path}"
    "formal bypass: remove_all(")

file(READ "${_host}" _host_text)
function(expect_host_insert NAME SLUG SITE INSERT EXPECTED_REASON)
    string(FIND "${_host_text}" "${SITE}" _site_index)
    if(_site_index EQUAL -1)
        message(FATAL_ERROR "${NAME} mutation site disappeared")
    endif()
    string(LENGTH "${SITE}" _site_length)
    math(EXPR _after_site "${_site_index} + ${_site_length}")
    string(SUBSTRING "${_host_text}" ${_after_site} -1 _after_text)
    string(FIND "${_after_text}" "${SITE}" _second_site)
    if(NOT _second_site EQUAL -1)
        message(FATAL_ERROR "${NAME} mutation site is not unique")
    endif()
    string(REPLACE "${SITE}" "${SITE}\n                    ${INSERT}"
        _mutated "${_host_text}")
    if(_mutated STREQUAL _host_text)
        message(FATAL_ERROR "${NAME} mutation made no change")
    endif()
    set(_path "${GUARD_TEST_ROOT}/${SLUG}.cpp")
    file(WRITE "${_path}" "${_mutated}")
    expect_rejected("${NAME}" HOST "${_path}" "${EXPECTED_REASON}")
endfunction()

set(_host_scope_site
    "if (stage11d_validation_state.abyss_claim_requested) {")
expect_host_insert("host private access" "host-private-access"
    "${_host_scope_site}" "const auto* TestAccess = session;"
    "host-bypass-TestAccess")
expect_host_insert("host snapshot override" "host-snapshot-override"
    "${_host_scope_site}" "const auto snapshot_override = current;"
    "host-bypass-snapshot_override")
expect_host_insert("host direct pickup request" "host-direct-pickup"
    "${_host_scope_site}"
    "static_cast<void>(session->request_pickup(0U));"
    "host-bypass-request_pickup(")
expect_host_insert("host direct pickup completion" "host-complete-pickup"
    "${_host_scope_site}"
    "static_cast<void>(session->complete_pickup(0U));"
    "host-bypass-complete_pickup(")
expect_host_insert("host direct pickup publication" "host-publish-pickup"
    "${_host_scope_site}"
    "static_cast<void>(session->publish_pickup(0U));"
    "host-bypass-publish_pickup(")
expect_host_insert("host committed settings write" "host-committed-settings"
    "${_host_scope_site}"
    "pause_menu.committed.loot_filter_mode = settings::LootFilterMode::rare_only;"
    "host validation seam direct committed settings write")
expect_host_insert("host live settings write" "host-live-settings"
    "${_host_scope_site}"
    "live_settings.loot_filter_mode = settings::LootFilterMode::rare_only;"
    "host validation seam direct live settings write")
expect_host_insert("host direct result pass" "host-result-pass"
    "${_host_scope_site}" "const char* result = \"result=pass\";"
    "host validation seam direct result pass")

set(_site "const auto& item = current.ground_items[index];")
string(FIND "${_host_text}" "${_site}" _site_index)
if(_site_index EQUAL -1)
    message(FATAL_ERROR "snapshot-mutation site disappeared")
endif()
string(REPLACE "${_site}" "current.ground_items[0] = fabricated;"
    _mutated "${_host_text}")
if(_mutated STREQUAL _host_text)
    message(FATAL_ERROR "snapshot mutation made no change")
endif()
set(_path "${GUARD_TEST_ROOT}/snapshot-mutation.cpp")
file(WRITE "${_path}" "${_mutated}")
expect_rejected("snapshot mutation" HOST "${_path}"
    "rejected snapshot mutation")

set(_site "EndDrawing();\n    if (path == nullptr) return true;")
string(FIND "${_host_text}" "${_site}" _site_index)
if(_site_index EQUAL -1)
    message(FATAL_ERROR "pre-present-capture mutation site disappeared")
endif()
string(REPLACE "${_site}"
    "Image pre_present = LoadImageFromScreen();\n    ${_site}"
    _mutated "${_host_text}")
if(_mutated STREQUAL _host_text)
    message(FATAL_ERROR "pre-present mutation made no change")
endif()
set(_path "${GUARD_TEST_ROOT}/pre-present-capture.cpp")
file(WRITE "${_path}" "${_mutated}")
expect_rejected("pre-EndDrawing capture" HOST "${_path}"
    "rejected pre-EndDrawing capture")

file(READ "${_renderer}" _renderer_text)
set(_site "const CombatRenderPlan render_plan = make_combat_render_plan(")
string(FIND "${_renderer_text}" "${_site}" _site_index)
if(_site_index EQUAL -1)
    message(FATAL_ERROR "second-render-plan mutation site disappeared")
endif()
string(REPLACE "${_site}"
    "const CombatRenderPlan duplicate_plan = make_combat_render_plan(\n        previous, current, clamped_interpolation_alpha, camera_offset,\n        loot_filter_mode_, static_cast<float>(GetScreenWidth()),\n        static_cast<float>(GetScreenHeight()));\n\n    ${_site}"
    _mutated "${_renderer_text}")
if(_mutated STREQUAL _renderer_text)
    message(FATAL_ERROR "second-render-plan mutation made no change")
endif()
set(_path "${GUARD_TEST_ROOT}/second-render-plan.cpp")
file(WRITE "${_path}" "${_mutated}")
expect_rejected("second render plan" RENDERER "${_path}"
    "requires the one production renderer plan")

set(_draw_site "GroundLootView CombatRenderer::draw(")
string(FIND "${_renderer_text}" "${_draw_site}" _draw_site_index)
if(_draw_site_index EQUAL -1)
    message(FATAL_ERROR "draw-external-plan mutation site disappeared")
endif()
set(_draw_external_plan
    "void stage11d_draw_external_plan(const dungeon::DungeonSnapshot& snapshot) noexcept {\n    static_cast<void>(make_combat_render_plan(\n        snapshot, settings::LootFilterMode::show_all, 1.0F, 1.0F));\n}\n\n${_draw_site}")
string(REPLACE "${_draw_site}" "${_draw_external_plan}"
    _mutated "${_renderer_text}")
if(_mutated STREQUAL _renderer_text)
    message(FATAL_ERROR "draw-external-plan mutation made no change")
endif()
set(_path "${GUARD_TEST_ROOT}/draw-external-plan.cpp")
file(WRITE "${_path}" "${_mutated}")
expect_rejected("draw-external render plan" RENDERER "${_path}"
    "rejected a draw-external renderer plan")

file(READ "${_validator}" _validator_text)
set(_mutated "${_validator_text}")
string(REPLACE "LastWriteTimeUtc" "CreationTimeUtc" _mutated "${_mutated}")
if(_mutated STREQUAL _validator_text)
    message(FATAL_ERROR "stale-validator mutation made no change")
endif()
set(_path "${GUARD_TEST_ROOT}/stale-validator.ps1")
file(WRITE "${_path}" "${_mutated}")
expect_rejected("stale PNG acceptance" VALIDATOR "${_path}"
    "missing semantic check: LastWriteTimeUtc")

set(_mutated "${_validator_text}")
set(_site "Require ($hashes.Add([string]$hash)) \"duplicate screenshot hash: $name\"")
string(FIND "${_validator_text}" "${_site}" _site_index)
if(_site_index EQUAL -1)
    message(FATAL_ERROR "duplicate-validator mutation site disappeared")
endif()
string(REPLACE "${_site}" "[void]$hashes.Add([string]$hash)"
    _mutated "${_validator_text}")
if(_mutated STREQUAL _validator_text)
    message(FATAL_ERROR "duplicate-validator mutation made no change")
endif()
set(_path "${GUARD_TEST_ROOT}/duplicate-validator.ps1")
file(WRITE "${_path}" "${_mutated}")
expect_rejected("duplicate PNG acceptance" VALIDATOR "${_path}"
    "missing semantic check: duplicate")

set(_mutated "${_validator_text}")
set(_site "$feature = Measure-Region $bitmap")
string(FIND "${_validator_text}" "${_site}" _site_index)
if(_site_index EQUAL -1)
    message(FATAL_ERROR "existence-only mutation site disappeared")
endif()
string(REPLACE "${_site}"
    "$feature = [pscustomobject]@{ Colors = 99; Bright = 99; Dark = 99 } #"
    _mutated "${_validator_text}")
if(_mutated STREQUAL _validator_text)
    message(FATAL_ERROR "existence-only mutation made no change")
endif()
set(_path "${GUARD_TEST_ROOT}/existence-only-validator.ps1")
file(WRITE "${_path}" "${_mutated}")
expect_rejected("existence-only validation" VALIDATOR "${_path}"
    "missing semantic check: $feature")

message(STATUS
    "Stage11D loot evidence guard self-test passed: bad_mutations=22")
