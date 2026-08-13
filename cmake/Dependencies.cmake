include_guard(GLOBAL)
include(FetchContent)

set(BUILD_EXAMPLES OFF CACHE BOOL "Do not build raylib examples" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build raylib statically" FORCE)
set(PLATFORM "Desktop" CACHE STRING "raylib platform" FORCE)

FetchContent_Declare(
    raylib
    URL "https://github.com/raysan5/raylib/archive/dbc56a87da87d973a9c5baa4e7438a9d20121d28.tar.gz"
    URL_HASH "SHA256=81b06ce7c19cf3b634b0271c23c361ba6ad8bf45fb8b036abbfeb4260ec1e126"
    DOWNLOAD_DIR "${PROJECT_SOURCE_DIR}/out/downloads"
    DOWNLOAD_NAME "raylib-dbc56a87da87d973a9c5baa4e7438a9d20121d28.tar.gz"
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

FetchContent_MakeAvailable(raylib)

if(NOT TARGET raylib)
    message(FATAL_ERROR "Pinned raylib dependency did not create target raylib")
endif()

# The host owns F12 capture so it can write after presentation and honor the
# caller-selected directory.  Disable raylib's second, implicit F12 capture;
# otherwise every press also writes screenshotNNN.png beside the executable.
target_compile_definitions(raylib PRIVATE SUPPORT_SCREEN_CAPTURE=0)

get_target_property(ARPG_RAYLIB_TYPE raylib TYPE)
if(NOT ARPG_RAYLIB_TYPE STREQUAL "STATIC_LIBRARY")
    message(FATAL_ERROR "raylib must be static; detected ${ARPG_RAYLIB_TYPE}")
endif()

file(READ "${raylib_SOURCE_DIR}/src/raylib.h" ARPG_RAYLIB_HEADER)
foreach(EXPECTED_REGEX IN ITEMS
        "#define[ \t]+RAYLIB_VERSION_MAJOR[ \t]+6"
        "#define[ \t]+RAYLIB_VERSION_MINOR[ \t]+0"
        "#define[ \t]+RAYLIB_VERSION_PATCH[ \t]+0")
    string(REGEX MATCH "${EXPECTED_REGEX}" DEFINE_MATCH "${ARPG_RAYLIB_HEADER}")
    if(NOT DEFINE_MATCH)
        message(FATAL_ERROR "Pinned raylib archive has unexpected version macros")
    endif()
endforeach()
