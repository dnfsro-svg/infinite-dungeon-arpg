### Task 9: NumPad 1～5 双输入路径

**Files:**
- Modify: `src/platform/raylib/host_input.cpp`
- Modify: `src/platform/raylib/raylib_input.hpp`
- Modify: `src/platform/raylib/raylib_input.cpp`
- Modify: `tests/platform/active_skill_input_tests.cpp`
- Create: `tests/platform/raylib_input_tests.cpp`
- Modify: `tests/platform/CMakeLists.txt`
- Modify: `tests/platform/platform_test_main.cpp`

**Interfaces:**
- Produces `win32_virtual_key_for_raylib(int)` as a pure mapping used by both `platform_key_pressed` and `platform_key_down`.
- Preserves `map_host_frame_input` and `submit_frame_actions` signatures.

- [ ] **Step 1: Write RED tests for raylib sampling and Win32 mapping.**

Require each `KEY_KP_1..KEY_KP_5` pressed edge sets exactly slot 0..4; `KEY_ONE..KEY_FIVE` set none. A key that is down but not pressed must not repeatedly cast. Require:

```cpp
ARPG_REQUIRE(win32_virtual_key_for_raylib(KEY_KP_1) == 0x61);
ARPG_REQUIRE(win32_virtual_key_for_raylib(KEY_KP_2) == 0x62);
ARPG_REQUIRE(win32_virtual_key_for_raylib(KEY_KP_3) == 0x63);
ARPG_REQUIRE(win32_virtual_key_for_raylib(KEY_KP_4) == 0x64);
ARPG_REQUIRE(win32_virtual_key_for_raylib(KEY_KP_5) == 0x65);
```

- [ ] **Step 2: Run RED.**

```powershell
cmake --build --preset windows-msvc-debug --target arpg_platform_tests --parallel 1
ctest --test-dir out/build/windows-msvc-debug -R '^platform\.units$' --output-on-failure -j 1
```

Expected: current `KEY_ONE + index` assertions fail and the pure Win32 mapper is absent.

- [ ] **Step 3: Implement explicit key arrays and shared mapping.**

```cpp
constexpr std::array<int, skills::kActiveSkillSlotCount>
    kActiveSkillKeys{{KEY_KP_1, KEY_KP_2, KEY_KP_3, KEY_KP_4, KEY_KP_5}};
```

Sample each explicit key through the injected `pressed` function. In `win32_virtual_key_for_raylib`, map only `KEY_KP_1..5` to `VK_NUMPAD1..5` (`0x61..0x65`) in addition to existing supported keys; both asynchronous pressed/down functions call this mapper. Do not infer continuity from raylib enum values.

- [ ] **Step 4: Run GREEN and input architecture guard.**

```powershell
cmake --build --preset windows-msvc-debug --target arpg_platform_tests --parallel 1
ctest --test-dir out/build/windows-msvc-debug -R '^platform\.units$' --output-on-failure -j 1
ctest --test-dir out/build/windows-msvc-debug -R '^(platform\.input_latency_source|platform\.host_input_source)$' --output-on-failure -j 1
```

Expected: raylib and Win32 paths map NumPad correctly, main numbers do nothing, and centralized input-source guards remain green.

- [ ] **Step 5: Recount the platform suite and commit.**

Register exactly four cases: NumPad-to-slot mapping, main-number rejection/pressed-edge behavior, Win32 NumPad virtual-key mapping, and unchanged submit semantics. Change the platform expected case count from 469 to 473.

```powershell
git add src/platform/raylib/host_input.cpp src/platform/raylib/raylib_input.* tests/platform/active_skill_input_tests.cpp tests/platform/raylib_input_tests.cpp tests/platform/CMakeLists.txt tests/platform/platform_test_main.cpp
git commit -m "fix: bind active skills to numpad keys"
```

