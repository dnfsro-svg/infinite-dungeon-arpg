include("${CMAKE_CURRENT_LIST_DIR}/evidence_source_scan.cmake")

foreach(required FIXTURE_SOURCE FORMAL_SOURCE CAPTURE_SCRIPT HOST_HEADER HOST_SOURCE)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "Stage 11 evidence guard missing ${required}")
    endif()
endforeach()
file(READ "${FIXTURE_SOURCE}" fixture_source)
file(READ "${FORMAL_SOURCE}" formal_source)
file(READ "${CAPTURE_SCRIPT}" capture_script)
file(READ "${HOST_HEADER}" host_header)
file(READ "${HOST_SOURCE}" host_source)
get_filename_component(host_directory "${HOST_HEADER}" DIRECTORY)
set(stage_source "${host_directory}/host_validation_stage10_11.cpp")
set(stage_header "${host_directory}/host_validation_stage10_11.hpp")
if(DEFINED STAGE_SOURCE)
    set(stage_source "${STAGE_SOURCE}")
endif()
if(DEFINED STAGE_HEADER)
    set(stage_header "${STAGE_HEADER}")
endif()
if(NOT EXISTS "${stage_source}")
    message(FATAL_ERROR "Stage 11 validation route target is missing: ${stage_source}")
endif()
if(NOT EXISTS "${stage_header}")
    message(FATAL_ERROR "Stage 11 validation state target is missing: ${stage_header}")
endif()
file(READ "${stage_source}" stage_source_text)
file(READ "${stage_header}" stage_header_text)
set(all_evidence "${fixture_source}\n${formal_source}\n${host_header}\n${host_source}\n${stage_source_text}")

evidence_extract_cpp_function_block("${stage_source_text}"
    "combat::MovementInput stage11_validation_input(" stage11_input_block)
evidence_extract_cpp_function_block("${stage_source_text}"
    "bool stage11_validation_reached(" stage11_reached_block)

set(public_death_mutation_scan "${all_evidence}")
string(REPLACE "==" "__stage11_eq__" public_death_mutation_scan
    "${public_death_mutation_scan}")
string(REPLACE "!=" "__stage11_ne__" public_death_mutation_scan
    "${public_death_mutation_scan}")
string(REPLACE "<=" "__stage11_le__" public_death_mutation_scan
    "${public_death_mutation_scan}")
string(REPLACE ">=" "__stage11_ge__" public_death_mutation_scan
    "${public_death_mutation_scan}")
set(public_death_field_mutation
    "([.]|->)[ \t\r\n]*death([ \t\r\n]*[.][ \t\r\n]*[A-Za-z_][A-Za-z0-9_]*|[ \t\r\n]*\\[[^]]*\\])+[ \t\r\n]*(=|[+*/%|&^-]=|<<=|>>=)")
if(public_death_mutation_scan MATCHES "${public_death_field_mutation}")
    message(FATAL_ERROR
        "Forbidden Stage 11 public death injection: death subfield mutation")
endif()

foreach(public_death_injection
        "[.]death[ \t\r\n]*="
        "->[ \t\r\n]*death[ \t\r\n]*="
        "[.]death[ \t\r\n]*[.]emplace[ \t\r\n]*\\("
        "death_snapshot[ \t\r\n]*="
        "death_snapshot[ \t\r\n]*[.]emplace[ \t\r\n]*\\("
        "death_checkpoint[ \t\r\n]*="
        "death_checkpoint[ \t\r\n]*[.]emplace[ \t\r\n]*\\("
        "make_death_checkpoint[ \t\r\n]*\\(")
    if(public_death_mutation_scan MATCHES "${public_death_injection}")
        message(FATAL_ERROR
            "Forbidden Stage 11 public death injection: ${public_death_injection}")
    endif()
endforeach()

foreach(forbidden "DungeonSessionTestAccess" "CombatWorldTestAccess"
        "force_defeat" "stable_state_" "phase_[ \t]*="
        "pending_save_[ \t]*=")
    if(all_evidence MATCHES "${forbidden}")
        message(FATAL_ERROR "Forbidden Stage 11 evidence injection: ${forbidden}")
    endif()
endforeach()

foreach(required_fixture "SaveStore" "session.tick" "pending_save_view"
        "request_death_continue" "store.commit" "store.load")
    if(NOT fixture_source MATCHES "${required_fixture}")
        message(FATAL_ERROR "Fixture lacks production API: ${required_fixture}")
    endif()
endforeach()
foreach(required_host "stage11_validation_input"
        "MovementInput" "runtime.fixed_tick" "request_death_continue"
        "host_death_input_gate" "settings::StableKey::e"
        "death_gate.continue_death = true")
    if(NOT host_source MATCHES "${required_host}")
        message(FATAL_ERROR "Formal host lacks production input/save path: ${required_host}")
    endif()
endforeach()
foreach(required_stage11_input_token
        "session.request_descent(true)"
        "session.queue_action(combat::Action::light)")
    string(FIND "${stage11_input_block}" "${required_stage11_input_token}"
        required_stage11_index)
    if(required_stage11_index EQUAL -1)
        message(FATAL_ERROR
            "Stage 11 validation input lacks production route: ${required_stage11_input_token}")
    endif()
endforeach()
foreach(required_stage11_reached_token
        "Stage11ValidationScenario::deep_continue"
        "state.continue_requested && state.saw_depth_two")
    string(FIND "${stage11_reached_block}" "${required_stage11_reached_token}"
        required_stage11_reached_index)
    if(required_stage11_reached_index EQUAL -1)
        message(FATAL_ERROR
            "Stage 11 validation completion lacks production predicate: ${required_stage11_reached_token}")
    endif()
endforeach()
if(NOT stage_header_text MATCHES "struct Stage11ValidationState final")
    message(FATAL_ERROR "Stage 11 validation state definition is missing")
endif()
foreach(required_host_stage11_token
        "stage11_validation_input(*session,"
        "stage11_validation_reached("
        "stage11_validation_state.continue_requested = true;"
        "++stage11_validation_state.target_presented_frames;")
    string(FIND "${host_source}" "${required_host_stage11_token}" required_host_stage11_index)
    if(required_host_stage11_index EQUAL -1)
        message(FATAL_ERROR
            "Stage 11 formal host call is missing: ${required_host_stage11_token}")
    endif()
endforeach()
string(ASCII 9 host_tab)
string(ASCII 10 host_lf)
string(ASCII 13 host_cr)
string(REPLACE " " "" host_compact "${host_source}")
string(REPLACE "${host_tab}" "" host_compact "${host_compact}")
string(REPLACE "${host_lf}" "" host_compact "${host_compact}")
string(REPLACE "${host_cr}" "" host_compact "${host_compact}")
string(REGEX MATCHALL "if\\(death_gate[.]continue_death\\)\\{"
    exact_death_gate_conditions "${host_compact}")
list(LENGTH exact_death_gate_conditions exact_death_gate_condition_count)
if(NOT exact_death_gate_condition_count EQUAL 1)
    message(FATAL_ERROR
        "Formal death continue condition must be exactly death_gate.continue_death")
endif()
string(REGEX MATCHALL "runtime[.]request_death_continue[ \t\r\n]*[(][ \t\r\n]*[)]"
    host_continue_requests "${host_source}")
list(LENGTH host_continue_requests host_continue_request_count)
if(NOT host_continue_request_count EQUAL 1)
    message(FATAL_ERROR
        "Formal validation continue must use the single death input gate request path")
endif()
set(death_gate_scope_prefix "if(death_gate.continue_death){")
string(FIND "${host_compact}" "${death_gate_scope_prefix}"
    death_gate_scope_begin)
string(LENGTH "${death_gate_scope_prefix}" death_gate_scope_prefix_length)
math(EXPR death_gate_open_brace
    "${death_gate_scope_begin} + ${death_gate_scope_prefix_length} - 1")
string(LENGTH "${host_compact}" host_compact_length)
math(EXPR host_compact_last "${host_compact_length} - 1")
set(death_gate_depth 0)
set(death_gate_scope_end -1)
foreach(character_index RANGE ${death_gate_open_brace} ${host_compact_last})
    string(SUBSTRING "${host_compact}" ${character_index} 1 character)
    if(character STREQUAL "{")
        math(EXPR death_gate_depth "${death_gate_depth} + 1")
    elseif(character STREQUAL "}")
        math(EXPR death_gate_depth "${death_gate_depth} - 1")
        if(death_gate_depth EQUAL 0)
            set(death_gate_scope_end ${character_index})
            break()
        endif()
    endif()
endforeach()
if(death_gate_scope_end EQUAL -1)
    message(FATAL_ERROR "Formal death continue gate scope is unbalanced")
endif()
math(EXPR death_gate_scope_length
    "${death_gate_scope_end} - ${death_gate_scope_begin} + 1")
string(SUBSTRING "${host_compact}" ${death_gate_scope_begin}
    ${death_gate_scope_length} death_gate_scope)
string(FIND "${death_gate_scope}" "runtime.request_death_continue()"
    scoped_continue_request)
if(scoped_continue_request EQUAL -1)
    message(FATAL_ERROR
        "Formal death continue request must be inside the explicit death input gate scope")
endif()
string(FIND "${death_gate_scope}" "validation_continue"
    scoped_validation_continue)
if(NOT scoped_validation_continue EQUAL -1)
    message(FATAL_ERROR
        "Formal validation_continue must remain outside the death input gate scope")
endif()
if(NOT formal_source MATCHES "run_raylib_host"
        OR NOT formal_source MATCHES "SaveStore"
        OR NOT formal_source MATCHES "formal-path-summary.txt")
    message(FATAL_ERROR "Formal Stage 11 executable lacks real host/save/summary evidence")
endif()
foreach(required_script "LastWriteTimeUtc" "System.Drawing" "GetPixel"
        "nonBackground" "Get-PanelHash" "formal-path-summary.txt")
    if(NOT capture_script MATCHES "${required_script}")
        message(FATAL_ERROR "Capture validator lacks ${required_script}")
    endif()
endforeach()

string(ASCII 9 tab)
string(ASCII 10 lf)
string(ASCII 13 cr)
string(REPLACE " " "" compact "${host_source}")
string(REPLACE "${tab}" "" compact "${compact}")
string(REPLACE "${lf}" "" compact "${compact}")
string(REPLACE "${cr}" "" compact "${compact}")
set(helper "boolpresent_frame_and_maybe_capture(constchar*path)noexcept{EndDrawing();if(path==nullptr)returntrue;Imageimage=LoadImageFromScreen();if(image.data==nullptr)returnfalse;constboolexported=ExportImage(image,path);UnloadImage(image);returnexported;}")
string(FIND "${compact}" "${helper}" helper_index)
string(REGEX MATCHALL "EndDrawing\\(\\)" ends "${compact}")
string(REGEX MATCHALL "LoadImageFromScreen\\(\\)" loads "${compact}")
string(REGEX MATCHALL "ExportImage\\(" exports "${compact}")
list(LENGTH ends end_count)
list(LENGTH loads load_count)
list(LENGTH exports export_count)
if(helper_index EQUAL -1 OR NOT end_count EQUAL 1
        OR NOT load_count EQUAL 1 OR NOT export_count EQUAL 1)
    message(FATAL_ERROR "Capture must occur once after EndDrawing (helper=${helper_index} end=${end_count} load=${load_count} export=${export_count})")
endif()
message(STATUS "Stage 11 production evidence guard passed")
