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
set(all_evidence "${fixture_source}\n${formal_source}\n${host_header}\n${host_source}")

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
foreach(required_host "stage11_validation_input" "queue_action"
        "MovementInput" "runtime.fixed_tick" "request_death_continue"
        "frame_input.keys.e = true")
    if(NOT host_source MATCHES "${required_host}")
        message(FATAL_ERROR "Formal host lacks production input/save path: ${required_host}")
    endif()
endforeach()
string(REGEX MATCHALL "runtime[.]request_death_continue[ \t\r\n]*[(][ \t\r\n]*[)]"
    host_continue_requests "${host_source}")
list(LENGTH host_continue_requests host_continue_request_count)
if(NOT host_continue_request_count EQUAL 1)
    message(FATAL_ERROR
        "Formal validation continue must use the single death input gate request path")
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
set(helper "voidpresent_frame_and_maybe_capture(constchar*path)noexcept{EndDrawing();if(path==nullptr)return;Imageimage=LoadImageFromScreen();if(image.data==nullptr)return;static_cast<void>(ExportImage(image,path));UnloadImage(image);}")
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
