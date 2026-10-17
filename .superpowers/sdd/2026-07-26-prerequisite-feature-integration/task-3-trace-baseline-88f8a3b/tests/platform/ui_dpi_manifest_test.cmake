if(NOT DEFINED SOURCE_ROOT)
    message(FATAL_ERROR "SOURCE_ROOT is required")
endif()

set(_manifest "${SOURCE_ROOT}/src/app/arpg_game.manifest")
set(_cmake "${SOURCE_ROOT}/src/app/CMakeLists.txt")
if(NOT EXISTS "${_manifest}")
    message(FATAL_ERROR "missing per-monitor DPI manifest: ${_manifest}")
endif()

file(READ "${_manifest}" _manifest_text)
file(READ "${_cmake}" _cmake_text)
foreach(_required IN ITEMS
        "PerMonitorV2"
        "longPathAware"
        "supportedOS")
    string(FIND "${_manifest_text}" "${_required}" _found)
    if(_found EQUAL -1)
        message(FATAL_ERROR "DPI manifest missing ${_required}")
    endif()
endforeach()

string(FIND "${_cmake_text}" "arpg_game.manifest" _wired)
if(_wired EQUAL -1)
    message(FATAL_ERROR "arpg_game does not embed the DPI manifest")
endif()
