if(NOT DEFINED SOURCE_ROOT)
    message(FATAL_ERROR "SOURCE_ROOT is required")
endif()

include("${CMAKE_CURRENT_LIST_DIR}/cpp_source_lexer.cmake")

set(_header "${SOURCE_ROOT}/src/platform/raylib/raylib_host.hpp")
set(_host "${SOURCE_ROOT}/src/platform/raylib/raylib_host.cpp")
set(_renderer "${SOURCE_ROOT}/src/platform/raylib/combat_renderer.cpp")
set(_formal "${SOURCE_ROOT}/tests/platform/stage11d_loot_formal_game_validation.cpp")
set(_validator "${SOURCE_ROOT}/tests/platform/stage11d_loot_formal_validator.ps1")
if(DEFINED HEADER_OVERRIDE)
    set(_header "${HEADER_OVERRIDE}")
endif()
if(DEFINED HOST_OVERRIDE)
    set(_host "${HOST_OVERRIDE}")
endif()
if(DEFINED RENDERER_OVERRIDE)
    set(_renderer "${RENDERER_OVERRIDE}")
endif()
if(DEFINED FORMAL_OVERRIDE)
    set(_formal "${FORMAL_OVERRIDE}")
endif()
if(DEFINED VALIDATOR_OVERRIDE)
    set(_validator "${VALIDATOR_OVERRIDE}")
endif()
foreach(_file IN ITEMS "${_header}" "${_host}" "${_renderer}"
        "${_formal}" "${_validator}")
    if(NOT EXISTS "${_file}")
        message(FATAL_ERROR "Stage11D loot evidence input is missing: ${_file}")
    endif()
endforeach()

file(READ "${_header}" _header_text)
file(READ "${_host}" _host_text)
file(READ "${_renderer}" _renderer_text)
file(READ "${_formal}" _formal_text)
file(READ "${_validator}" _validator_text)
set(_combined "${_header_text}\n${_host_text}\n${_formal_text}")

set(_stage11d_host_seam "")
set(_host_without_stage11d_seam "${_host_text}")
set(_stage11d_seam_labels
    state selectors safe_movement physical_driver evidence_semantics
    foreground runtime_state runtime_input fixed_step abyss_claim
    presented_semantics reached_merge reached visible_capture captured summary)
foreach(_label IN LISTS _stage11d_seam_labels)
    set(_begin "// STAGE11D_LOOT_VALIDATION_SEAM_BEGIN ${_label}")
    set(_end "// STAGE11D_LOOT_VALIDATION_SEAM_END ${_label}")
    string(REGEX MATCHALL "${_begin}" _begins "${_host_without_stage11d_seam}")
    string(REGEX MATCHALL "${_end}" _ends "${_host_without_stage11d_seam}")
    list(LENGTH _begins _begin_count)
    list(LENGTH _ends _end_count)
    if(NOT _begin_count EQUAL 1 OR NOT _end_count EQUAL 1)
        message(FATAL_ERROR
            "Stage11D loot evidence guard cannot isolate host validation seam: ${_label}")
    endif()
    string(FIND "${_host_without_stage11d_seam}" "${_begin}" _begin_index)
    string(FIND "${_host_without_stage11d_seam}" "${_end}" _end_index)
    if(_begin_index EQUAL -1 OR _end_index EQUAL -1
            OR NOT _begin_index LESS _end_index)
        message(FATAL_ERROR
            "Stage11D loot evidence guard has invalid host validation seam: ${_label}")
    endif()
    string(LENGTH "${_end}" _end_length)
    math(EXPR _region_length
        "${_end_index} + ${_end_length} - ${_begin_index}")
    string(SUBSTRING "${_host_without_stage11d_seam}"
        ${_begin_index} ${_region_length} _region)
    string(APPEND _stage11d_host_seam "\n${_region}")
    string(REPLACE "${_region}" "" _host_without_stage11d_seam
        "${_host_without_stage11d_seam}")
endforeach()
arpg_sanitize_cpp_source("${_stage11d_host_seam}" _stage11d_host_code)
string(TOLOWER "${_host_without_stage11d_seam}" _host_without_stage11d_lower)
if(_host_without_stage11d_lower MATCHES "stage11d")
    message(FATAL_ERROR
        "Stage11D loot evidence guard found host validation code outside its isolated seam")
endif()

foreach(_required IN ITEMS
        "Stage11DLootValidationScenario" "show_all" "magic_or_better"
        "rare_only" "rare_only_abyss" "preview_cancel" "pickup_feedback"
        "platform::run_raylib_host(config)" "persistence::SaveStore"
        "settings::SettingsStore" "settings_store.save("
        "present_frame_and_maybe_capture" "stage11d_record_semantics"
        "renderer.draw(" "pickup_commit_generation" "defeat_distance_milli")
    string(FIND "${_combined}" "${_required}" _found)
    if(_found EQUAL -1)
        message(FATAL_ERROR "Stage11D loot evidence guard missing production token: ${_required}")
    endif()
endforeach()

string(FIND "${_header_text}" "enum class Stage11DLootValidationScenario" _enum_begin)
if(_enum_begin EQUAL -1)
    message(FATAL_ERROR "Stage11D loot evidence guard cannot isolate scenario enum")
endif()
string(SUBSTRING "${_header_text}" ${_enum_begin} -1 _enum_tail)
string(FIND "${_enum_tail}" "};" _enum_end)
if(_enum_end EQUAL -1)
    message(FATAL_ERROR "Stage11D loot evidence guard cannot isolate scenario enum")
endif()
math(EXPR _enum_length "${_enum_end} + 2")
string(SUBSTRING "${_enum_tail}" 0 ${_enum_length} _enum_text)
string(REGEX MATCHALL "[ \t\r\n]([a-z][a-z0-9_]*)[ \t\r\n]*[,}]" _enum_values "${_enum_text}")
list(LENGTH _enum_values _enum_count)
if(NOT _enum_count EQUAL 7)
    message(FATAL_ERROR "Stage11D loot evidence guard requires none plus exactly six scenarios")
endif()

foreach(_forbidden IN ITEMS
        "TestAccess" "snapshot_override" "set_snapshot(" "FakeRenderer"
        "fake_renderer" "request_pickup(" "complete_pickup(" "publish_pickup("
        "pause_menu.committed.loot_filter_mode ="
        "live_settings.loot_filter_mode =" "ground_items[0] ="
        "inventory_count =" "result=pass" "LoadImageFromScreen()"
        "remove_all(")
    string(FIND "${_formal_text}" "${_forbidden}" _formal_found)
    if(NOT _formal_found EQUAL -1)
        message(FATAL_ERROR "Stage11D loot evidence guard rejected formal bypass: ${_forbidden}")
    endif()
endforeach()

foreach(_required IN ITEMS
        "allowed_evidence_root(" "reset_evidence_root("
        "absolute.filename() != \"stage11d loot evidence\""
        "absolute == absolute.root_path()" "temp_directory_path("
        "/out/build/" "has_link_or_reparse_component("
        "FILE_ATTRIBUTE_REPARSE_POINT" "weakly_canonical("
        "remove_known_file(root, root / spec.image)"
        "remove_known_file(root, root / spec.summary)"
        "--root-safety-self-test"
        "stage11d-root-safety-sentinel.unknown"
        "return sentinel_preserved && known_removed")
    string(FIND "${_formal_text}" "${_required}" _cleanup_found)
    if(_cleanup_found EQUAL -1)
        message(FATAL_ERROR
            "Stage11D formal cleanup safety is incomplete: ${_required}")
    endif()
endforeach()

set(_host_code "${_host_text}")
foreach(_forbidden IN ITEMS
        "TestAccess" "snapshot_override" "set_snapshot(" "FakeRenderer"
        "fake_renderer" "request_pickup(" "complete_pickup("
        "publish_pickup(")
    string(FIND "${_host_code}" "${_forbidden}" _host_found)
    if(NOT _host_found EQUAL -1)
        message(FATAL_ERROR
            "Stage11D host-bypass-${_forbidden}")
    endif()
endforeach()

string(REGEX MATCH
    "pause_menu[ \t\r\n]*[.][ \t\r\n]*committed([^=;]*|)[=][^=]"
    _committed_assignment "${_stage11d_host_code}")
if(_committed_assignment)
    message(FATAL_ERROR
        "Stage11D loot evidence guard rejected host validation seam direct committed settings write")
endif()
string(REGEX MATCH "live_settings([^=;]*|)[=][^=]"
    _live_assignment "${_stage11d_host_code}")
if(_live_assignment)
    message(FATAL_ERROR
        "Stage11D loot evidence guard rejected host validation seam direct live settings write")
endif()
string(REGEX MATCH "result[ \t\r\n]*=[ \t\r\n]*pass"
    _direct_pass "${_stage11d_host_seam}")
if(_direct_pass)
    message(FATAL_ERROR
        "Stage11D loot evidence guard rejected host validation seam direct result pass")
endif()

string(REGEX REPLACE "[ \t\r\n]+" "" _host_normalized "${_host_code}")
string(REGEX MATCHALL
    "live_settings[.]loot_filter_mode=pause_menu[.]committed[.]loot_filter_mode"
    _canonical_live_settings_writes "${_host_normalized}")
list(LENGTH _canonical_live_settings_writes _canonical_live_settings_count)
if(NOT _canonical_live_settings_count EQUAL 2)
    message(FATAL_ERROR
        "Stage11D loot evidence guard requires exactly two canonical production live-settings rollback writes")
endif()
string(REGEX MATCHALL "live_settings[.]loot_filter_mode="
    _all_live_filter_writes "${_host_normalized}")
list(LENGTH _all_live_filter_writes _all_live_filter_write_count)
if(NOT _all_live_filter_write_count EQUAL 2)
    message(FATAL_ERROR
        "Stage11D loot evidence guard rejected extra live-settings filter write")
endif()

string(FIND "${_host_text}" "inject_stage11d_physical_edges(" _driver_begin)
string(FIND "${_host_text}" "stage11d_view_has_rarity(" _driver_end)
if(_driver_begin EQUAL -1 OR _driver_end EQUAL -1
        OR NOT _driver_begin LESS _driver_end)
    message(FATAL_ERROR "Stage11D loot evidence guard cannot isolate physical driver")
endif()
math(EXPR _driver_length "${_driver_end} - ${_driver_begin}")
string(SUBSTRING "${_host_text}" ${_driver_begin} ${_driver_length} _driver_text)
foreach(_forbidden IN ITEMS
        ".queue_action(" "request_pickup(" "complete_pickup(" "TestAccess"
        "inventory_count =" "renderer.draw(")
    string(FIND "${_driver_text}" "${_forbidden}" _found)
    if(NOT _found EQUAL -1)
        message(FATAL_ERROR "Stage11D loot evidence guard rejected physical-driver bypass: ${_forbidden}")
    endif()
endforeach()
string(REGEX MATCH
    "ground_items[ \t\r\n]*\\[[^]]+\\][ \t\r\n]*=[^=]"
    _ground_mutation "${_driver_text}")
if(_ground_mutation)
    message(FATAL_ERROR "Stage11D loot evidence guard rejected snapshot mutation")
endif()
string(REGEX MATCH
    "ground_items[ \t\r\n]*\\[[^]]+\\][ \t\r\n]*=[^=]"
    _host_ground_mutation "${_host_text}")
if(_host_ground_mutation)
    message(FATAL_ERROR "Stage11D loot evidence guard rejected host snapshot mutation")
endif()

unset(_previous)
foreach(_ordered IN ITEMS
        "const PhysicalKeySnapshot sampled_physical_keys = sample_physical_keys();"
        "const PhysicalKeySnapshot physical_keys = inject_stage11d_physical_edges("
        "HostFrameInput frame_input = map_host_frame_input("
        "submit_frame_actions(*session, frame_input)")
    string(FIND "${_host_text}" "${_ordered}" _index)
    if(_index EQUAL -1)
        message(FATAL_ERROR "Stage11D loot evidence guard missing input stage: ${_ordered}")
    endif()
    if(DEFINED _previous AND _index LESS _previous)
        message(FATAL_ERROR "Stage11D loot evidence guard rejected physical sample-map-submit order")
    endif()
    set(_previous ${_index})
endforeach()

string(FIND "${_host_text}" "EndDrawing();" _present)
string(FIND "${_host_text}" "LoadImageFromScreen();" _capture)
if(_present EQUAL -1 OR _capture EQUAL -1 OR NOT _present LESS _capture)
    message(FATAL_ERROR "Stage11D loot evidence guard rejected pre-EndDrawing capture")
endif()
string(REGEX MATCHALL "LoadImageFromScreen[ \t\r\n]*\\(" _capture_calls "${_host_text}")
list(LENGTH _capture_calls _capture_count)
if(NOT _capture_count EQUAL 1)
    message(FATAL_ERROR "Stage11D loot evidence guard requires one production capture helper")
endif()
string(FIND "${_host_text}" "make_combat_render_plan(" _host_second_plan)
if(NOT _host_second_plan EQUAL -1)
    message(FATAL_ERROR "Stage11D loot evidence guard rejected a second host render plan")
endif()
string(REGEX MATCHALL "make_combat_render_plan[ \t\r\n]*\\(" _renderer_plans "${_renderer_text}")
list(LENGTH _renderer_plans _renderer_plan_count)
if(NOT _renderer_plan_count EQUAL 2)
    message(FATAL_ERROR "Stage11D loot evidence guard requires the one production renderer plan")
endif()
foreach(_required IN ITEMS
        "const GroundLootView ground_loot_view = [&]() noexcept {"
        "return GroundLootView{};" "return renderer.draw("
        "stage11d_record_semantics(stage11d_validation_state, current,"
        "present_frame_and_maybe_capture(capture_path.has_value()"
        "stage11d_validation_state.captured = true;")
    string(FIND "${_host_text}" "${_required}" _found)
    if(_found EQUAL -1)
        message(FATAL_ERROR "Stage11D loot evidence guard missing evidence binding: ${_required}")
    endif()
endforeach()

foreach(_required IN ITEMS
        "LastWriteTimeUtc" "1280" "720" "duplicate screenshot hash"
        "committed PNG differs" "committed summary differs"
        "\$feature = Measure-Region \$bitmap"
        "snapshot_ids" "inventory_ids" "hidden-item semantics mismatch"
        "abyss_claimed" "pickup_commit_generation" "preview_visible_count"
        "pickup notice region is blank" "result -eq 'pass'")
    string(FIND "${_validator_text}" "${_required}" _found)
    if(_found EQUAL -1)
        message(FATAL_ERROR "Stage11D loot evidence validator missing semantic check: ${_required}")
    endif()
endforeach()

message(STATUS "Stage11D loot formal evidence guard passed")
