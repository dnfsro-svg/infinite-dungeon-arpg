if(NOT DEFINED ROOT)
    message(FATAL_ERROR "ROOT is required")
endif()

file(GLOB_RECURSE guarded_sources
    "${ROOT}/src/*.cpp"
    "${ROOT}/src/*.hpp"
    "${ROOT}/tests/*.cpp"
    "${ROOT}/tests/*.hpp")

foreach(path IN LISTS guarded_sources)
    file(STRINGS "${path}" lines)
    foreach(line IN LISTS lines)
        string(STRIP "${line}" stripped)
        if(stripped MATCHES "^(struct|class)[ \\t]")
            continue()
        endif()
        if(path MATCHES "src[/\\\\]persistence[/\\\\]save_commit_worker.hpp$"
                AND stripped MATCHES "SaveCheckpointSlot checkpoint")
            continue()
        endif()
        if(stripped MATCHES "(^|[^A-Za-z0-9_])(SaveCheckpointSlot|SaveCommitJobSlot|SaveCommitStorage)[ \\t]+[A-Za-z_][A-Za-z0-9_]*[ \\t]*(\\{|;|=)")
            message(FATAL_ERROR
                "Large persistence object may be automatic in ${path}: ${stripped}")
        endif()
        if(stripped MATCHES "std::array<.*kMaximumEncodedCheckpointBytes"
                AND NOT path MATCHES "src[/\\\\]persistence[/\\\\]save_commit_worker.hpp$")
            message(FATAL_ERROR
                "8 MiB codec buffer declared outside heap storage in ${path}: ${stripped}")
        endif()
    endforeach()
endforeach()

set(worker_source_path
    "${ROOT}/src/persistence/save_commit_worker.cpp")
file(READ "${worker_source_path}" worker_source)
foreach(forbidden IN ITEMS
        "raylib"
        "DungeonSession"
        "DeterministicRng"
        "fixed_tick("
        "try_pop_event("
        "try_pop_combat_event("
        "emit_event("
        "next_bounded("
        "export_state("
        "import_state(")
    string(FIND "${worker_source}" "${forbidden}" forbidden_index)
    if(NOT forbidden_index EQUAL -1)
        message(FATAL_ERROR
            "Worker source touches forbidden live authority '${forbidden}'")
    endif()
endforeach()
if(NOT worker_source MATCHES
        "wake_\\.wait\\(lock,[ \t\r\n]*\\[this\\][ \t\r\n]*\\{")
    message(FATAL_ERROR
        "Worker idle loop must use a predicate condition-variable wait")
endif()
if(worker_source MATCHES "sleep_for\\(" OR worker_source MATCHES "sleep_until\\(")
    message(FATAL_ERROR "Worker must not poll or sleep on a timer")
endif()

message(STATUS
    "Large persistence objects remain heap-owned and worker authority boundary is clean")
