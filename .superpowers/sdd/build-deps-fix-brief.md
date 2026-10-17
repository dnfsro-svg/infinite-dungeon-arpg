# Build Dependency Fix Brief

Baseline: 8f4e70ca47c897df1122687c1dab3cccac5c0a34 on task/m01-combat-core.

## Reproduced RED

- CMake 4.3.3 detects MSVC 19.44 Chinese UTF-8 /showIncludes output with ENCODING AUTO as mojibake.
- Generated rules contain: `msvc_deps_prefix = 娉ㄦ剰: 鍖呭惈鏂囦欢:  `.
- `ninja -t deps` reports `#deps 0` for every C/C++ object.
- A CombatWorld header layout change therefore left movement_jump_tests.cpp.obj stale and produced 0xC0000409.
- VSLANG=1033 before/after Enter-VsDevShell did not change cl output or the generated prefix.

## Required fix

1. Add a small CMake module included immediately after project().
2. Only for MSVC + Ninja and only when the detected prefix equals the exact known mojibake signature, replace C, CXX, and generic showIncludes prefix variables with the actual UTF-8 Chinese prefix `注意: 包含文件:  `.
3. Do not hardcode Chinese on English/other environments; exact-signature guard is mandatory.
4. Do not patch installed CMake, global environment, or generated files.

## GREEN proof

- Fresh configure generates rules.ninja with actual Chinese prefix, not mojibake.
- Full build and CTest 4/4 pass; Combat remains 20/0.
- `ninja -t deps` for movement_jump_tests.cpp.obj has nonzero dependencies and includes combat_world.hpp.
- Touch only the mtime of src/combat/combat_world.hpp; `ninja -n` must schedule dependent combat/test objects. Then run the real build and confirm git status remains clean.
- diff-check passes.

Commit only CMakeLists.txt and the new cmake module with subject: `fix: track localized MSVC include dependencies`.

