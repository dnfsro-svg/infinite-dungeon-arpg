set(RAYLIB_LEAK_REGEX
    "(^|[;:/\\\\<])raylib($|[;:/\\\\>])|_deps[/\\\\]raylib")

foreach(POSITIVE_VALUE IN ITEMS
        "raylib"
        "raylib::raylib"
        "$<LINK_ONLY:raylib>"
        "E:/vendor/raylib/include"
        "E:/build/_deps/raylib-src/src")
    string(TOLOWER "${POSITIVE_VALUE}" POSITIVE_LOWER)
    if(NOT POSITIVE_LOWER MATCHES "${RAYLIB_LEAK_REGEX}")
        message(FATAL_ERROR "Expected leak was not detected: ${POSITIVE_VALUE}")
    endif()
endforeach()

set(FALSE_POSITIVE_VALUE "E:/game/.worktrees/m00-raylib-host/src")
string(TOLOWER "${FALSE_POSITIVE_VALUE}" FALSE_POSITIVE_LOWER)
if(FALSE_POSITIVE_LOWER MATCHES "${RAYLIB_LEAK_REGEX}")
    message(FATAL_ERROR
        "Worktree-name false positive remains: ${FALSE_POSITIVE_VALUE}")
endif()

message(STATUS "raylib leak regex probe passed")
