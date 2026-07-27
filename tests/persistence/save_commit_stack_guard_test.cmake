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

set(host_source_path "${ROOT}/src/platform/raylib/raylib_host.cpp")
file(READ "${host_source_path}" host_source)

function(host_large_state_ownership_valid SOURCE OUT_VALID)
    set(WS "[ \t\r\n]*")
    set(WS1 "[ \t\r\n]+")
    string(FIND "${SOURCE}" "HostExitCode run_raylib_host" HOST_ENTRY_INDEX)
    if(HOST_ENTRY_INDEX EQUAL -1)
        message(STATUS "host ownership guard: missing host entry")
        set("${OUT_VALID}" FALSE PARENT_SCOPE)
        return()
    endif()
    string(SUBSTRING "${SOURCE}" ${HOST_ENTRY_INDEX} -1 HOST_ENTRY_SOURCE)

    string(REGEX MATCHALL
        "std::make_unique${WS}<${WS}dungeon::DungeonSnapshot${WS}>${WS}\\(${WS}\\)"
        SNAPSHOT_HEAP_ALLOCATIONS "${HOST_ENTRY_SOURCE}")
    list(LENGTH SNAPSHOT_HEAP_ALLOCATIONS SNAPSHOT_HEAP_ALLOCATION_COUNT)
    if(NOT SNAPSHOT_HEAP_ALLOCATION_COUNT EQUAL 3)
        message(STATUS "host ownership guard: snapshot heap allocation count=${SNAPSHOT_HEAP_ALLOCATION_COUNT}")
        set("${OUT_VALID}" FALSE PARENT_SCOPE)
        return()
    endif()

    foreach(SNAPSHOT_NAME IN ITEMS current previous presented_snapshot)
        string(REGEX MATCHALL
            "const${WS1}auto${WS1}${SNAPSHOT_NAME}_storage${WS}=${WS}std::make_unique${WS}<${WS}dungeon::DungeonSnapshot${WS}>${WS}\\(${WS}\\)"
            SNAPSHOT_OWNER_MATCHES "${HOST_ENTRY_SOURCE}")
        list(LENGTH SNAPSHOT_OWNER_MATCHES SNAPSHOT_OWNER_MATCH_COUNT)
        string(REGEX MATCHALL
            "dungeon::DungeonSnapshot${WS}&${WS}${SNAPSHOT_NAME}${WS}=${WS}\\*${WS}${SNAPSHOT_NAME}_storage"
            SNAPSHOT_ALIAS_MATCHES "${HOST_ENTRY_SOURCE}")
        list(LENGTH SNAPSHOT_ALIAS_MATCHES SNAPSHOT_ALIAS_MATCH_COUNT)
        if(NOT SNAPSHOT_OWNER_MATCH_COUNT EQUAL 1
                OR NOT SNAPSHOT_ALIAS_MATCH_COUNT EQUAL 1)
            message(STATUS
                "host ownership guard: ${SNAPSHOT_NAME} owners=${SNAPSHOT_OWNER_MATCH_COUNT} aliases=${SNAPSHOT_ALIAS_MATCH_COUNT}")
            set("${OUT_VALID}" FALSE PARENT_SCOPE)
            return()
        endif()
    endforeach()

    string(REGEX MATCHALL
        "(^|[^A-Za-z0-9_])(const${WS1})?dungeon::DungeonSnapshot${WS1}[A-Za-z_][A-Za-z0-9_]*${WS}(\\{|;|=)"
        AUTOMATIC_SNAPSHOT_DECLARATIONS "${HOST_ENTRY_SOURCE}")
    list(LENGTH AUTOMATIC_SNAPSHOT_DECLARATIONS
        AUTOMATIC_SNAPSHOT_DECLARATION_COUNT)
    string(REGEX MATCHALL
        "(runtime\\.session\\(\\)|session)->snapshot${WS}\\(${WS}\\)"
        BY_VALUE_SNAPSHOT_CALLS "${HOST_ENTRY_SOURCE}")
    list(LENGTH BY_VALUE_SNAPSHOT_CALLS BY_VALUE_SNAPSHOT_CALL_COUNT)
    string(REGEX MATCHALL
        "(^|[^A-Za-z0-9_])(const${WS1})?HostValidationStates${WS1}[A-Za-z_][A-Za-z0-9_]*${WS}(\\{|;|=)"
        AUTOMATIC_VALIDATION_DECLARATIONS "${HOST_ENTRY_SOURCE}")
    list(LENGTH AUTOMATIC_VALIDATION_DECLARATIONS
        AUTOMATIC_VALIDATION_DECLARATION_COUNT)
    if(NOT AUTOMATIC_SNAPSHOT_DECLARATION_COUNT EQUAL 0
            OR NOT BY_VALUE_SNAPSHOT_CALL_COUNT EQUAL 0
            OR NOT AUTOMATIC_VALIDATION_DECLARATION_COUNT EQUAL 0)
        message(STATUS
            "host ownership guard: automatic snapshots=${AUTOMATIC_SNAPSHOT_DECLARATION_COUNT} by-value=${BY_VALUE_SNAPSHOT_CALL_COUNT} automatic validation=${AUTOMATIC_VALIDATION_DECLARATION_COUNT}")
        set("${OUT_VALID}" FALSE PARENT_SCOPE)
        return()
    endif()

    string(REGEX MATCH
        "const${WS1}std::unique_ptr${WS}<${WS}HostValidationStates${WS}>${WS}validation_states${WS}\\{${WS}new${WS}\\(${WS}std::nothrow${WS}\\)${WS}HostValidationStates${WS}\\{${WS}\\}${WS}\\}${WS};"
        VALIDATION_OWNER_MATCH "${HOST_ENTRY_SOURCE}")
    string(REGEX MATCH
        "if${WS}\\(${WS}validation_states${WS}==${WS}nullptr${WS}\\)"
        VALIDATION_NULL_MATCH "${HOST_ENTRY_SOURCE}")
    string(REGEX MATCH
        "SetConfigFlags${WS}\\(${WS}initial_window_flags${WS}\\(${WS}committed_settings${WS}\\)${WS}\\)"
        INITIAL_FLAGS_MATCH "${HOST_ENTRY_SOURCE}")
    string(REGEX MATCH "InitWindow${WS}\\(" INIT_WINDOW_MATCH
        "${HOST_ENTRY_SOURCE}")
    string(REGEX MATCH
        "const${WS1}auto${WS1}renderer_storage${WS}=${WS}std::make_unique${WS}<${WS}CombatRenderer${WS}>${WS}\\(${WS}\\)"
        RENDERER_STORAGE_MATCH "${HOST_ENTRY_SOURCE}")
    if(VALIDATION_OWNER_MATCH STREQUAL ""
            OR VALIDATION_NULL_MATCH STREQUAL ""
            OR INITIAL_FLAGS_MATCH STREQUAL "" OR INIT_WINDOW_MATCH STREQUAL ""
            OR RENDERER_STORAGE_MATCH STREQUAL "")
        message(STATUS
            "host ownership guard: owner='${VALIDATION_OWNER_MATCH}' null='${VALIDATION_NULL_MATCH}' flags='${INITIAL_FLAGS_MATCH}' window='${INIT_WINDOW_MATCH}' renderer='${RENDERER_STORAGE_MATCH}'")
        set("${OUT_VALID}" FALSE PARENT_SCOPE)
        return()
    endif()
    string(FIND "${HOST_ENTRY_SOURCE}" "${VALIDATION_OWNER_MATCH}"
        VALIDATION_OWNER_INDEX)
    string(FIND "${HOST_ENTRY_SOURCE}" "${VALIDATION_NULL_MATCH}"
        VALIDATION_NULL_INDEX)
    string(FIND "${HOST_ENTRY_SOURCE}" "${INITIAL_FLAGS_MATCH}"
        INITIAL_FLAGS_INDEX)
    string(FIND "${HOST_ENTRY_SOURCE}" "${INIT_WINDOW_MATCH}"
        INIT_WINDOW_INDEX)
    string(FIND "${HOST_ENTRY_SOURCE}" "${RENDERER_STORAGE_MATCH}"
        RENDERER_STORAGE_INDEX)
    if(NOT VALIDATION_OWNER_INDEX LESS VALIDATION_NULL_INDEX
            OR NOT VALIDATION_NULL_INDEX LESS INITIAL_FLAGS_INDEX
            OR NOT INITIAL_FLAGS_INDEX LESS INIT_WINDOW_INDEX
            OR NOT INIT_WINDOW_INDEX LESS RENDERER_STORAGE_INDEX)
        message(STATUS
            "host ownership guard: order owner=${VALIDATION_OWNER_INDEX} null=${VALIDATION_NULL_INDEX} flags=${INITIAL_FLAGS_INDEX} window=${INIT_WINDOW_INDEX} renderer=${RENDERER_STORAGE_INDEX}")
        set("${OUT_VALID}" FALSE PARENT_SCOPE)
        return()
    endif()
    set("${OUT_VALID}" TRUE PARENT_SCOPE)
endfunction()

host_large_state_ownership_valid("${host_source}" HOST_LARGE_STATE_VALID)
if(NOT HOST_LARGE_STATE_VALID)
    message(FATAL_ERROR
        "raylib host large states must be heap-owned, output-captured, and allocated in safe order")
endif()

set(CURRENT_SNAPSHOT_OWNER [=[        const auto current_storage =
            std::make_unique<dungeon::DungeonSnapshot>();
]=])
string(REPLACE "${CURRENT_SNAPSHOT_OWNER}"
    "        dungeon::DungeonSnapshot current{};\n"
    AUTOMATIC_SNAPSHOT_MUTATION "${host_source}")
if(AUTOMATIC_SNAPSHOT_MUTATION STREQUAL host_source)
    message(FATAL_ERROR "automatic DungeonSnapshot mutation did not modify host source")
endif()
host_large_state_ownership_valid("${AUTOMATIC_SNAPSHOT_MUTATION}"
    AUTOMATIC_SNAPSHOT_MUTATION_VALID)
if(AUTOMATIC_SNAPSHOT_MUTATION_VALID)
    message(FATAL_ERROR "stack guard accepted automatic DungeonSnapshot mutation")
endif()

string(REPLACE "session->snapshot(current);"
    "const auto by_value_snapshot = session->snapshot();"
    BY_VALUE_SNAPSHOT_MUTATION "${host_source}")
if(BY_VALUE_SNAPSHOT_MUTATION STREQUAL host_source)
    message(FATAL_ERROR "by-value DungeonSnapshot mutation did not modify host source")
endif()
host_large_state_ownership_valid("${BY_VALUE_SNAPSHOT_MUTATION}"
    BY_VALUE_SNAPSHOT_MUTATION_VALID)
if(BY_VALUE_SNAPSHOT_MUTATION_VALID)
    message(FATAL_ERROR "stack guard accepted by-value DungeonSnapshot mutation")
endif()

set(VALIDATION_STATES_OWNER [=[        const std::unique_ptr<HostValidationStates> validation_states{
            new (std::nothrow) HostValidationStates{}};
        if (validation_states == nullptr) {
            return HostExitCode::save_initialization_failed;
        }
]=])
string(REPLACE "${VALIDATION_STATES_OWNER}"
    "        HostValidationStates validation_states{};\n"
    AUTOMATIC_VALIDATION_MUTATION "${host_source}")
if(AUTOMATIC_VALIDATION_MUTATION STREQUAL host_source)
    message(FATAL_ERROR "automatic HostValidationStates mutation did not modify host source")
endif()
host_large_state_ownership_valid("${AUTOMATIC_VALIDATION_MUTATION}"
    AUTOMATIC_VALIDATION_MUTATION_VALID)
if(AUTOMATIC_VALIDATION_MUTATION_VALID)
    message(FATAL_ERROR "stack guard accepted automatic HostValidationStates mutation")
endif()

string(REPLACE "${VALIDATION_STATES_OWNER}" ""
    LATE_VALIDATION_MUTATION "${host_source}")
string(REPLACE "        core::FixedStepRunner fixed_step;"
    "${VALIDATION_STATES_OWNER}\n        core::FixedStepRunner fixed_step;"
    LATE_VALIDATION_MUTATION "${LATE_VALIDATION_MUTATION}")
if(LATE_VALIDATION_MUTATION STREQUAL host_source)
    message(FATAL_ERROR "late HostValidationStates mutation did not modify host source")
endif()
host_large_state_ownership_valid("${LATE_VALIDATION_MUTATION}"
    LATE_VALIDATION_MUTATION_VALID)
if(LATE_VALIDATION_MUTATION_VALID)
    message(FATAL_ERROR "stack guard accepted late HostValidationStates allocation")
endif()

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
