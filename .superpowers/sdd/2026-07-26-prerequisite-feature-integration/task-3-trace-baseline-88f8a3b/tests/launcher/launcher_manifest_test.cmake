set(manifest_path
    "${CMAKE_CURRENT_LIST_DIR}/../../src/launcher/arpg_launcher.manifest")

if(NOT EXISTS "${manifest_path}")
    message(FATAL_ERROR "Launcher manifest is missing: ${manifest_path}")
endif()

file(READ "${manifest_path}" manifest_contents)

foreach(required_text IN ITEMS "PerMonitorV2" "longPathAware" "true")
    string(FIND "${manifest_contents}" "${required_text}" match_position)
    if(match_position EQUAL -1)
        message(FATAL_ERROR
            "Launcher manifest is missing required text: ${required_text}")
    endif()
endforeach()
