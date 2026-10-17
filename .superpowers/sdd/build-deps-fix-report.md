# Build Dependency Fix Report

## Status

Implemented the repository-local MSVC `/showIncludes` prefix normalization for
localized Chinese output. No installed CMake files, generated build files, or
global environment variables were modified.

## Root cause / RED evidence

- CMake 4.3.3 decoded the MSVC 19.44 Chinese UTF-8 `/showIncludes` prefix as
  `娉ㄦ剰: 鍖呭惈鏂囦欢:  ` while generating Ninja rules.
- The generated `msvc_deps_prefix` therefore did not match compiler output.
- `ninja -t deps` consequently recorded `#deps 0`, allowing stale objects after
  header-only layout changes.
- Setting `VSLANG=1033` did not change the compiler output or detected prefix.

## Change

- Added `cmake/NormalizeMsvcShowIncludes.cmake` and included it immediately
  after `project()`.
- The module is active only for `MSVC` with a Ninja generator.
- It replaces the C, CXX, and generic show-includes variables only when each
  value exactly equals the known mojibake signature, using the real UTF-8
  Chinese prefix `注意: 包含文件:  `.
- English and other localized environments are left unchanged.

## GREEN evidence

- Fresh configure generated `msvc_deps_prefix = 注意: 包含文件:  ` and no
  mojibake prefix in `CMakeFiles/rules.ninja`.
- Fresh full build completed without new warnings or leaked `/showIncludes`
  lines.
- `arpg_combat_tests.exe`: `20 cases, 0 failures`.
- CTest: `4/4` passed.
- `ninja -t deps` reported movement_jump_tests.cpp.obj as `#deps 5 (VALID)`
  and included `../../src/combat/combat_world.hpp`.
- After touching only the header mtime, `ninja -n` scheduled combat_world,
  hit_resolution, player_simulation, attack_state_tests,
  hit_resolution_tests, and movement_jump_tests objects plus relinking.
- The subsequent real build rebuilt those eight steps and the complete test
  suite remained green.
- `git diff --check` exited 0; the only content changes were `CMakeLists.txt`
  and `cmake/NormalizeMsvcShowIncludes.cmake`.
