include_guard(GLOBAL)

function(arpg_add_runtime_asset_sync)
    cmake_parse_arguments(
        ARGS
        ""
        "TARGET;SOURCE_ROOT;DESTINATION_ROOT"
        "DIRECTORIES"
        ${ARGN})

    foreach(required_argument IN ITEMS TARGET SOURCE_ROOT DESTINATION_ROOT)
        if(NOT DEFINED ARGS_${required_argument}
                OR ARGS_${required_argument} STREQUAL "")
            message(FATAL_ERROR
                "arpg_add_runtime_asset_sync requires ${required_argument}")
        endif()
    endforeach()
    if(NOT TARGET "${ARGS_TARGET}")
        message(FATAL_ERROR
            "arpg_add_runtime_asset_sync target does not exist: ${ARGS_TARGET}")
    endif()
    if(NOT ARGS_DIRECTORIES)
        message(FATAL_ERROR
            "arpg_add_runtime_asset_sync requires at least one DIRECTORY")
    endif()

    get_filename_component(asset_source_root "${ARGS_SOURCE_ROOT}" ABSOLUTE)
    set(asset_files)
    set(manifest_content "")
    foreach(asset_directory IN LISTS ARGS_DIRECTORIES)
        if(IS_ABSOLUTE "${asset_directory}"
                OR asset_directory MATCHES "(^|[/\\\\])\\.\\.([/\\\\]|$)")
            message(FATAL_ERROR
                "Runtime asset directory must stay below SOURCE_ROOT: ${asset_directory}")
        endif()
        file(TO_CMAKE_PATH "${asset_directory}" normalized_directory)
        string(REGEX REPLACE "^\\./" "" normalized_directory
            "${normalized_directory}")
        if(normalized_directory STREQUAL "")
            message(FATAL_ERROR "Runtime asset directory must not be empty")
        endif()

        set(source_directory "${asset_source_root}/${normalized_directory}")
        if(NOT IS_DIRECTORY "${source_directory}")
            message(FATAL_ERROR
                "Runtime asset source directory is missing: ${source_directory}")
        endif()
        string(APPEND manifest_content "D|${normalized_directory}\n")

        file(GLOB_RECURSE directory_files
            CONFIGURE_DEPENDS
            LIST_DIRECTORIES false
            "${source_directory}/*")
        list(APPEND asset_files ${directory_files})
    endforeach()

    list(REMOVE_DUPLICATES asset_files)
    list(SORT asset_files)
    foreach(asset_file IN LISTS asset_files)
        file(RELATIVE_PATH relative_asset "${asset_source_root}" "${asset_file}")
        file(TO_CMAKE_PATH "${relative_asset}" relative_asset)
        string(APPEND manifest_content "F|${relative_asset}\n")
    endforeach()

    set(manifest
        "${CMAKE_CURRENT_BINARY_DIR}/${ARGS_TARGET}-runtime-assets.manifest")
    set(stamp
        "${CMAKE_CURRENT_BINARY_DIR}/${ARGS_TARGET}-runtime-assets.stamp")
    file(GENERATE OUTPUT "${manifest}" CONTENT "${manifest_content}")

    set(sync_script "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/SyncRuntimeAssets.cmake")
    add_custom_command(
        OUTPUT "${stamp}"
        COMMAND "${CMAKE_COMMAND}"
            "-DSOURCE_ROOT=${asset_source_root}"
            "-DDESTINATION_ROOT=${ARGS_DESTINATION_ROOT}"
            "-DMANIFEST=${manifest}"
            "-DSTAMP=${stamp}"
            -P "${sync_script}"
        DEPENDS ${asset_files} "${manifest}" "${sync_script}"
        COMMENT "Synchronizing runtime assets for ${ARGS_TARGET}"
        VERBATIM)

    set(sync_target "${ARGS_TARGET}_runtime_assets")
    add_custom_target("${sync_target}" ALL DEPENDS "${stamp}")
    add_dependencies("${ARGS_TARGET}" "${sync_target}")
endfunction()
