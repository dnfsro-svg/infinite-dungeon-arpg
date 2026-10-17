# Task 1B: CMake target and boundary guards

Worktree `E:/game/.worktrees/whole-game-refactor-2026-07`, base `d6854ac`.
Read the complete V9 design and Task 1 plan before editing.

Allowed files only:

- Create `cmake/AssertTargetDependencyBoundary.cmake`
- Create `src/checkpoint/CMakeLists.txt`
- Modify root `CMakeLists.txt`
- Create `tests/checkpoint/CMakeLists.txt`
- Create `tests/checkpoint/checkpoint_boundary_test.cmake`

Do not edit C++ sources/tests or existing subsystem CMake files. Do not commit
or build; other Task 1 files are being created in parallel.

Register lower targets in the exact order described by the plan, then
`arpg_checkpoint`, then Combat/Dungeon/Persistence. `arpg_checkpoint` contains
`room_checkpoint_validation.cpp`, exposes `${PROJECT_SOURCE_DIR}/src`, uses
C++17 and project warnings, and links only the listed lower targets. Register
`tests/checkpoint` in BUILD_TESTING and target `arpg_checkpoint_tests` from the
planned two C++ files with test name `checkpoint.units`.

Implement a reusable configure-time recursive target dependency-closure
assertion over LINK_LIBRARIES and INTERFACE_LINK_LIBRARIES. The normal root
configuration must assert `arpg_checkpoint` cannot reach Combat, Dungeon,
Persistence, Platform/raylib or App targets.

The CTest `architecture.checkpoint_no_runtime_headers` scans all source/include
files plus `src/checkpoint/CMakeLists.txt`, rejects Combat/Dungeon/Persistence/
Platform/raylib/App includes or target tokens, and contains build-directory-only
self-tests that prove a forbidden include and forbidden dependency edge fail
with the intended diagnostics. It may remove only its own build fixture.

Return the exact target/test names and guard diagnostics. Do not build.
