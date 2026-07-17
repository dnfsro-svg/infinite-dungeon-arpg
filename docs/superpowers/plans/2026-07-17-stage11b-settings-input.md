# Stage 11-B Pause, Settings, and Input Rebinding Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在 Stage 11-A 基线上实现暂停冻结、独立设置持久化、音量/全屏/VSync 和十项键盘重绑定，并让生产输入与 HUD 共用同一绑定权威。

**Architecture:** 新建无 raylib 的 `arpg_settings` 模块，负责稳定动作/键、验证、44-byte V1 codec 和双槽 store；raylib 平台层只负责设备采样、窗口应用、暂停/设置 UI 与动态 HUD。Host 在同一渲染帧先采样物理键，再映射逻辑输入并依次执行 death、inventory/passive、pause、gameplay gates；暂停时不推进 FixedStepRunner。

**Tech Stack:** C++17、raylib 6.0.0、CMake 3.25+、Ninja、CTest、MSVC 19.44、Windows SDK 10.0.26100.0。

## Global Constraints

- 基线固定为 `918f2e938399275e821c331d848d1228fb0ef75a`；分支固定为 `codex/stage11b-settings-input`。
- raylib 必须保持 6.0.0 静态链接；不得引入第三方 UI、序列化或输入库。
- 不改变 Combat/Dungeon 数值、攻击 tick、随机流、V6 角色存档字节、死亡或深渊规则。
- 设置只在启动与 Apply 执行 I/O；Combat/Dungeon/Render 热路径不得增加堆分配。
- 十项动作始终一一绑定；冲突必须交换，不允许未绑定或重复绑定。
- `Esc/Enter/F1/F12/V` 保持全局安全键，不允许重绑定。
- 暂停时不调用 Session tick、不累计补帧；恢复首帧不得追赶暂停时间。
- 设置损坏恢复默认，不得进入角色存档恢复界面，也不得修改角色存档槽。
- 本阶段不实现主菜单、手柄、分辨率列表、完整 HUD、地面物品过滤、macOS 或 Stage 11-C。
- 每张任务卡必须先 RED、再 GREEN、运行相关回归、独立审查并提交；不得把多张任务压成一个提交。

---

## File Structure

| File | Responsibility |
| --- | --- |
| `src/platform/settings/settings_types.*` | 稳定动作/键、默认值、验证、冲突交换 |
| `src/core/crc32.*` | 设置与角色持久化共用的通用 CRC32 原语 |
| `src/platform/settings/settings_codec.*` | 固定 44-byte V1 编解码与 CRC |
| `src/platform/settings/settings_store.*` | 双槽选择、原子替换、故障恢复 |
| `src/platform/raylib/stable_key_raylib.*` | StableKey 与 raylib KeyboardKey 双向适配 |
| `src/platform/raylib/host_input.*` | 单次物理采样到逻辑 FrameInput |
| `src/platform/raylib/pause_menu_state.*` | 暂停/设置状态机、草稿与 Apply 命令 |
| `src/platform/raylib/pause_menu_view.*` | 纯布局、行文本、鼠标命中 |
| `src/platform/raylib/pause_menu_renderer.*` | raylib 暂停和设置绘制 |
| `src/platform/raylib/window_settings.*` | 音量、全屏、VSync 预览、读回、回滚 |
| `src/platform/raylib/control_hints.*` | 动态 HUD 控制提示缓存 |
| `src/platform/raylib/raylib_host.cpp` | 启动加载、输入 gate、暂停冻结和 Apply 编排 |
| `tests/settings/*` | 无 raylib 设置类型、codec、store、压力测试 |
| `tests/platform/*stage11b*` | 平台状态机、真实 host、证据与边界守卫 |

---

### Task 1: Stable Settings Types and Conflict-Free Bindings

**Files:**
- Modify: `CMakeLists.txt`
- Create: `src/platform/settings/CMakeLists.txt`
- Create: `src/platform/settings/settings_types.hpp`
- Create: `src/platform/settings/settings_types.cpp`
- Create: `tests/settings/CMakeLists.txt`
- Create: `tests/settings/settings_test_main.cpp`
- Create: `tests/settings/settings_types_tests.cpp`

**Interfaces:**
- Produces:

```cpp
namespace arpg::settings {
enum class SettingAction : std::uint8_t {
    move_up, move_down, move_left, move_right,
    light_attack, jump, launcher, interact, inventory, passive_tree, count
};
enum class StableKey : std::uint8_t {
    a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t,
    u, v, w, x, y, z, digit_0, digit_1, digit_2, digit_3, digit_4,
    digit_5, digit_6, digit_7, digit_8, digit_9, arrow_up, arrow_down,
    arrow_left, arrow_right, space, left_shift, right_shift,
    left_control, right_control, count
};
enum class WindowMode : std::uint8_t { windowed, fullscreen };
struct SettingsData final {
    std::uint8_t master_sfx_percent{100};
    WindowMode window_mode{WindowMode::windowed};
    bool vsync_enabled{true};
    std::array<StableKey, static_cast<std::size_t>(SettingAction::count)> bindings{};
    std::uint64_t revision{};
};
enum class SettingsValidationError : std::uint8_t {
    none, volume_range, volume_step, window_mode, key_range, duplicate_key
};
[[nodiscard]] SettingsData default_settings() noexcept;
[[nodiscard]] SettingsValidationError validate_settings(const SettingsData&) noexcept;
[[nodiscard]] StableKey binding_for(const SettingsData&, SettingAction) noexcept;
[[nodiscard]] bool assign_or_swap(SettingsData&, SettingAction, StableKey) noexcept;
[[nodiscard]] const char* action_label(SettingAction) noexcept;
}
```

- Default action order and keys are exactly `W/S/A/D/J/K/L/E/I/P`.

- [ ] **Step 1: Add the settings test target and write failing golden tests**

Add `add_subdirectory(src/platform/settings)` before raylib and `add_subdirectory(tests/settings)` under `BUILD_TESTING`. Tests must assert all ten defaults, volume/window/VSync/revision, every validation error, unoccupied assignment, occupied-key swap, and 1,000 swaps preserving uniqueness.

- [ ] **Step 2: Run the target and verify RED**

Run:

```powershell
cmake --preset windows-msvc-debug
cmake --build --preset windows-msvc-debug --target arpg_settings_tests
```

Expected: compilation fails because `settings_types.hpp` and `arpg_settings` do not exist.

- [ ] **Step 3: Implement the minimal stable schema and validation**

Use a fixed default array and an O(10²) duplicate scan; this runs only at load/apply and needs no dynamic allocation. `assign_or_swap` must find the owner of the new key and swap its old key into that owner.

- [ ] **Step 4: Run focused and boundary regression**

Run:

```powershell
cmake --build --preset windows-msvc-debug --target arpg_settings_tests
ctest --test-dir out/build/windows-msvc-debug -R "^settings.units$" --output-on-failure
ctest --test-dir out/build/windows-msvc-debug -R "^(core.units|combat.units|platform.module_boundary)$" --output-on-failure
```

Expected: settings 1/1 and selected regression 3/3 pass.

- [ ] **Step 5: Commit**

```powershell
git add CMakeLists.txt src/platform/settings tests/settings
git commit -m "feat: define stage 11b settings schema"
```

---

### Task 2: Fixed V1 Settings Codec

**Files:**
- Modify: `src/core/CMakeLists.txt`
- Create: `src/core/crc32.hpp`
- Create: `src/core/crc32.cpp`
- Modify: `src/persistence/CMakeLists.txt`
- Modify: `src/persistence/crc32.hpp`
- Delete: `src/persistence/crc32.cpp`
- Modify: `src/platform/settings/CMakeLists.txt`
- Create: `src/platform/settings/settings_codec.hpp`
- Create: `src/platform/settings/settings_codec.cpp`
- Modify: `tests/settings/CMakeLists.txt`
- Create: `tests/settings/settings_codec_tests.cpp`
- Modify: `tests/settings/settings_test_main.cpp`

**Interfaces:**
- Consumes: `SettingsData`, `validate_settings`.
- Produces:

```cpp
inline constexpr std::size_t kSettingsEncodedSize = 44U;
enum class SettingsCodecError : std::uint8_t {
    none, wrong_size, wrong_magic, wrong_format, wrong_payload_size,
    bad_crc, reserved_nonzero, invalid_settings
};
struct SettingsDecodeResult final {
    SettingsCodecError error{SettingsCodecError::none};
    SettingsData settings{};
};
[[nodiscard]] std::array<std::uint8_t, kSettingsEncodedSize>
    encode_settings(const SettingsData&) noexcept;
[[nodiscard]] SettingsDecodeResult decode_settings(
    const std::uint8_t*, std::size_t) noexcept;
```

**Layout:** magic `ARPGSET1` at 0–7; format `1` at 8–9; payload size `20` at 10–11; revision/generation at 12–19; volume/mode/vsync/reserved at 20–23; ten keys at 24–33; six reserved zero bytes at 34–39; CRC32 of bytes 8–39 at 40–43.

- [ ] **Step 1: Write layout and corruption RED tests**

Assert every offset, little-endian revision `0x0102030405060708`, golden CRC, round-trip, all 44 single-byte covered mutations, bad magic/format/length/reserved/keys/duplicates, null input, truncation and trailing bytes.

- [ ] **Step 2: Verify RED**

Run `cmake --build --preset windows-msvc-debug --target arpg_settings_tests`.

Expected: compile fails because codec symbols are missing.

- [ ] **Step 3: Implement explicit byte codec**

Use byte writes/reads only; no struct serialization or `reinterpret_cast`. Move the existing implementation unchanged to `arpg::core::crc32`/`crc32_update`; keep `persistence/crc32.hpp` as inline compatibility forwarding functions so all V1–V6 callers and tests retain their source API. Link `arpg_settings PRIVATE arpg_core`; it must not link `arpg_persistence` or include checkpoint/save-store headers.

- [ ] **Step 4: Run focused tests**

Run `ctest --test-dir out/build/windows-msvc-debug -R "^(settings.units|persistence.units)$" --output-on-failure`.

Expected: 2/2 pass and existing V6 codec cases remain unchanged.

- [ ] **Step 5: Commit**

```powershell
git add src/core src/persistence src/platform/settings tests/settings
git commit -m "feat: encode versioned settings records"
```

---

### Task 3: Dual-Slot Atomic Settings Store

**Files:**
- Modify: `src/platform/settings/CMakeLists.txt`
- Create: `src/platform/settings/settings_store.hpp`
- Create: `src/platform/settings/settings_store.cpp`
- Create: `tests/settings/settings_store_tests.cpp`
- Modify: `tests/settings/CMakeLists.txt`
- Modify: `tests/settings/settings_test_main.cpp`

**Interfaces:**

```cpp
enum class SettingsLoadStatus : std::uint8_t {
    loaded, defaults_missing, recovered_single_slot, defaults_corrupt
};
enum class SettingsSaveStatus : std::uint8_t {
    committed, invalid_settings, stale_revision, revision_overflow,
    write_failed, readback_failed
};
struct SettingsLoadResult final { SettingsLoadStatus status; SettingsData settings; };
struct SettingsSaveResult final { SettingsSaveStatus status; SettingsData settings; };
struct SettingsFileOps final {
    void* context{};
    bool (*read)(void*, const std::filesystem::path&, std::vector<std::uint8_t>&){};
    bool (*replace)(void*, const std::filesystem::path&,
        const std::uint8_t*, std::size_t){};
};
class SettingsStore final {
public:
    explicit SettingsStore(std::filesystem::path, SettingsFileOps = native_settings_file_ops());
    [[nodiscard]] SettingsLoadResult load() const;
    [[nodiscard]] SettingsSaveResult save(
        const SettingsData& committed, SettingsData draft) const;
};
```

`save` requires `draft.revision == committed.revision`; it increments revision internally after overflow check, validates, writes the invalid/older slot through `.tmp`, atomically replaces, reads back and returns the published value.

- [ ] **Step 1: Write fake-I/O RED tests**

Cover missing slots, A/B newest selection, wrap-safe comparison without allowing revision wrap, one-slot recovery, equal-generation identical data, equal-generation conflict, invalid CRC, write failure, replace failure, readback failure, and preservation of the last valid slot.

- [ ] **Step 2: Verify RED**

Run the settings target; expected compile failure for missing store.

- [ ] **Step 3: Implement native and injected I/O**

Native replacement writes `settings-a.bin.tmp` or `settings-b.bin.tmp`, flushes/closes, then on Windows calls `MoveFileExW(temp, slot, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)` to replace only that slot. Load never deletes files and never touches `slot-a.sav`/`slot-b.sav`.

- [ ] **Step 4: Run tests and filesystem boundary scan**

Run:

```powershell
ctest --test-dir out/build/windows-msvc-debug -R "^(settings.units|persistence.units)$" --output-on-failure
rg -n "slot-a\.sav|slot-b\.sav|checkpoint_codec|SaveStore" src/platform/settings
```

Expected: tests 2/2; `rg` has no matches.

- [ ] **Step 5: Commit**

```powershell
git add src/platform/settings tests/settings
git commit -m "feat: persist settings in atomic dual slots"
```

---

### Task 4: Stable-Key Adapter and One-Sample Logical Input

**Files:**
- Create: `src/platform/raylib/stable_key_raylib.hpp`
- Create: `src/platform/raylib/stable_key_raylib.cpp`
- Create: `src/platform/raylib/host_input.hpp`
- Create: `src/platform/raylib/host_input.cpp`
- Modify: `src/platform/raylib/CMakeLists.txt`
- Modify: `src/platform/raylib/raylib_input.hpp`
- Modify: `src/platform/raylib/raylib_input.cpp`
- Delete: `src/platform/raylib/combat_key_bindings.hpp`
- Replace: `tests/platform/combat_key_bindings_tests.cpp` with `tests/platform/host_input_tests.cpp`
- Modify: `tests/platform/CMakeLists.txt`
- Modify: `tests/platform/platform_test_main.cpp`

**Interfaces:**

```cpp
[[nodiscard]] int raylib_key(settings::StableKey) noexcept;
[[nodiscard]] std::optional<settings::StableKey> stable_key_from_raylib(int) noexcept;
[[nodiscard]] const char* stable_key_label(settings::StableKey) noexcept;

struct PhysicalKeySnapshot final {
    std::array<bool, static_cast<std::size_t>(settings::StableKey::count)> down{};
    std::array<bool, static_cast<std::size_t>(settings::StableKey::count)> pressed{};
    bool escape{}, enter{}, f1{}, f12{}, v{}, mouse_left{}, focus_lost{};
    Vector2 mouse_position{};
};
struct HostFrameInput final {
    FrameKeyState keys{};
    combat::MovementInput movement{};
    std::array<bool, 3> combat_actions{};
    Vector2 mouse_position{};
};
[[nodiscard]] PhysicalKeySnapshot sample_physical_keys() noexcept;
[[nodiscard]] HostFrameInput map_host_frame_input(
    const settings::SettingsData&, const PhysicalKeySnapshot&) noexcept;
void submit_frame_actions(dungeon::DungeonSession&, const HostFrameInput&) noexcept;
```

- [ ] **Step 1: Write exhaustive adapter and mapping RED tests**

Test all 45 stable keys round-trip, unique raylib codes and labels; default and swapped movement/action/interact/inventory/passive mapping; pressed versus down; global keys independent of binding; one physical key cannot generate two logical actions after validation.

- [ ] **Step 2: Verify RED**

Run platform target; expected missing adapter/input symbols.

- [ ] **Step 3: Implement adapter and pure mapper**

`sample_physical_keys` loops each supported stable key once and calls both pressed/down once. `map_host_frame_input` performs no raylib calls and no allocation. Replace the private structures/functions at `raylib_host.cpp:36-82` with this module.

- [ ] **Step 4: Run latency and platform regression**

Run:

```powershell
ctest --test-dir out/build/windows-msvc-debug -R "^(platform.units|platform.input_latency_source)$" --output-on-failure
rg -n "KEY_[WASDJKLEIP]" src/platform/raylib/raylib_host.cpp src/platform/raylib/hud_renderer.cpp
```

Expected: platform tests pass; no gameplay-key hardcoding remains in host/HUD.

- [ ] **Step 5: Commit**

```powershell
git add src/platform/raylib tests/platform
git commit -m "feat: map configurable keys to frame input"
```

---

### Task 5: Pure Pause and Settings State Machine

**Files:**
- Create: `src/platform/raylib/pause_menu_state.hpp`
- Create: `src/platform/raylib/pause_menu_state.cpp`
- Create: `tests/platform/pause_menu_state_tests.cpp`
- Modify: `src/platform/raylib/CMakeLists.txt`
- Modify: `tests/platform/CMakeLists.txt`
- Modify: `tests/platform/platform_test_main.cpp`

**Interfaces:**

```cpp
enum class PauseScreen : std::uint8_t { closed, root, settings, capture_binding, quit_confirm };
enum class PauseCommand : std::uint8_t { none, preview, apply, rollback, resume, quit };
struct PauseContext final {
    bool death_active{}, recovery_active{}, inventory_open{}, passive_open{},
        pending_save{}, window_focused{true};
};
struct PauseInput final {
    bool escape{}, enter{}, up{}, down{}, left{}, right{}, activate{}, focus_lost{};
    std::optional<settings::StableKey> captured_key{};
};
struct PauseMenuState final {
    PauseScreen screen{PauseScreen::closed};
    std::size_t selected_row{};
    std::optional<settings::SettingAction> capture_action{};
    settings::SettingsData committed{};
    settings::SettingsData draft{};
    const char* message{};
};
[[nodiscard]] PauseCommand update_pause_menu(
    PauseMenuState&, const PauseContext&, const PauseInput&) noexcept;
```

- [ ] **Step 1: Write every transition as RED**

Cover overlay priority, pending-save rejection, open/resume, settings/back, capture/cancel/focus loss, valid capture, occupied-key swap, defaults draft, Apply command, save-error state, quit confirmation, and same-frame Esc+attack gate.

- [ ] **Step 2: Verify RED**

Expected: platform target fails on missing state machine.

- [ ] **Step 3: Implement table-like deterministic transitions**

The state machine only mutates `PauseMenuState` and returns commands; it must not call raylib, SettingsStore, Combat, Dungeon or filesystem APIs.

- [ ] **Step 4: Run platform and death gate tests**

Run `ctest --test-dir out/build/windows-msvc-debug -R "^(platform.units|stage11.death_evidence.mutation_self_test)$" --output-on-failure`.

Expected: 2/2 pass and death gate behavior remains authoritative.

- [ ] **Step 5: Commit**

```powershell
git add src/platform/raylib tests/platform
git commit -m "feat: model pause and settings navigation"
```

---

### Task 6: Bounded Pause/Settings View and Renderer

**Files:**
- Create: `src/platform/raylib/pause_menu_view.hpp`
- Create: `src/platform/raylib/pause_menu_view.cpp`
- Create: `src/platform/raylib/pause_menu_renderer.hpp`
- Create: `src/platform/raylib/pause_menu_renderer.cpp`
- Create: `tests/platform/pause_menu_view_tests.cpp`
- Modify: `src/platform/raylib/CMakeLists.txt`
- Modify: `tests/platform/CMakeLists.txt`
- Modify: `tests/platform/platform_test_main.cpp`

**Interfaces:**

```cpp
struct PauseMenuLayout final { Rectangle panel; Rectangle rows[16]; Rectangle footer; };
struct PauseMenuView final {
    std::array<std::array<char, 64>, 16> rows{};
    std::size_t row_count{};
    std::size_t selected_row{};
    const char* title{};
    const char* message{};
};
[[nodiscard]] PauseMenuLayout pause_menu_layout(int width, int height) noexcept;
[[nodiscard]] PauseMenuView make_pause_menu_view(const PauseMenuState&) noexcept;
[[nodiscard]] std::optional<std::size_t> hit_test_pause_row(
    const PauseMenuLayout&, Vector2) noexcept;
void draw_pause_menu(const PauseMenuState&) noexcept;
```

- [ ] **Step 1: Write layout/content RED tests**

Assert all panels and actionable rows fit at 1024×576, 1280×720 and 1920×1080; root/settings/capture/quit text is complete; all ten bindings and committed/draft values render; 63-character termination holds; hit testing rejects gaps/outside.

- [ ] **Step 2: Verify RED**

Expected: platform target fails because view functions are absent.

- [ ] **Step 3: Implement pure view then thin renderer**

Use fixed arrays and `std::snprintf`; renderer draws a final full-screen dim layer, centered panel, selection highlight and footer after world/HUD. Do not store gameplay state in renderer.

- [ ] **Step 4: Run platform tests and link game**

Run:

```powershell
cmake --build --preset windows-msvc-debug --target arpg_platform_tests arpg_game
ctest --test-dir out/build/windows-msvc-debug -R "^platform.units$" --output-on-failure
```

Expected: platform 1/1 and game link pass.

- [ ] **Step 5: Commit**

```powershell
git add src/platform/raylib tests/platform
git commit -m "feat: render bounded pause settings overlay"
```

---

### Task 7: Live Audio, Fullscreen, and VSync Preview with Rollback

**Files:**
- Create: `src/platform/raylib/window_settings.hpp`
- Create: `src/platform/raylib/window_settings.cpp`
- Create: `tests/platform/window_settings_tests.cpp`
- Modify: `src/platform/raylib/CMakeLists.txt`
- Modify: `tests/platform/CMakeLists.txt`
- Modify: `tests/platform/platform_test_main.cpp`

**Interfaces:**

```cpp
struct WindowSettingsBackend final {
    void* context{};
    bool (*set_volume)(void*, std::uint8_t){};
    bool (*set_window_mode)(void*, settings::WindowMode){};
    bool (*set_vsync)(void*, bool){};
    settings::WindowMode (*window_mode)(void*){};
    bool (*vsync)(void*){};
};
enum class LiveSettingsResult : std::uint8_t { applied, invalid, backend_failed, readback_failed };
[[nodiscard]] LiveSettingsResult apply_live_settings(
    const settings::SettingsData& current,
    const settings::SettingsData& desired,
    WindowSettingsBackend) noexcept;
[[nodiscard]] LiveSettingsResult rollback_live_settings(
    const settings::SettingsData& previewed,
    const settings::SettingsData& committed,
    WindowSettingsBackend) noexcept;
[[nodiscard]] WindowSettingsBackend raylib_window_settings_backend() noexcept;
```

- [ ] **Step 1: Write fake backend RED tests**

Cover volume conversion, ordered apply, each backend failure, mode/VSync readback mismatch, rollback, and no call for unchanged values.

- [ ] **Step 2: Verify RED**

Expected: platform target fails on missing backend.

- [ ] **Step 3: Implement fake-testable orchestration and raylib adapter**

Initialize flags from loaded committed settings before `InitWindow`; runtime mode uses raylib 6.0 fullscreen/window APIs and restores 1280×720 centered window. Apply `SetMasterVolume(percent / 100.0F)` only on load/preview/rollback.

- [ ] **Step 4: Run platform tests and compile-time raylib version guard**

Run platform 1/1 and build `arpg_game`; expected pass with existing raylib 6.0 static assertions.

- [ ] **Step 5: Commit**

```powershell
git add src/platform/raylib tests/platform
git commit -m "feat: preview and rollback live settings"
```

---

### Task 8: Host Integration and True Simulation Freeze

**Files:**
- Modify: `src/platform/raylib/raylib_host.cpp`
- Modify: `src/platform/raylib/raylib_host.hpp`
- Modify: `src/platform/raylib/host_launch_options.*`
- Create: `tests/platform/pause_host_gate_tests.cpp`
- Modify: `tests/platform/host_launch_options_tests.cpp`
- Modify: `tests/platform/CMakeLists.txt`
- Modify: `tests/platform/platform_test_main.cpp`
- Modify: `tests/platform/input_latency_source_test.cmake`

**Interfaces:**
- `RaylibHostConfig` gains optional `settings_directory`; default is resolved save directory.
- Host owns `SettingsStore`, `PauseMenuState`, committed settings and one `PhysicalKeySnapshot` per presented frame.

- [ ] **Step 1: Write host-gate RED tests and source guard**

Test 600 paused presented frames preserve fixed tick and a frozen DungeonSnapshot; resume first frame has at most normal fixed-step cap and no catch-up. Source guard must require `fixed_step.clear_accumulator()` on pause entry and forbid `fixed_step.advance` inside the paused branch. Verify Esc closes inventory/passive before pause and death/recovery behavior is unchanged.

- [ ] **Step 2: Verify RED**

Run platform test and input latency source test; expected failure because pause is not integrated.

- [ ] **Step 3: Integrate load, pause gates and Apply transaction**

Order each frame: sample once → recovery/death → inventory/passive close → pause state → gameplay mapping → fixed step → draw world → draw overlays → capture. On Apply: validate draft → preview/readback → SettingsStore save → publish or rollback. Paused branch still updates renderer/UI and handles screenshots but never calls `runtime.fixed_tick`.

- [ ] **Step 4: Run platform, Stage 11 death, and game smoke tests**

Run:

```powershell
ctest --test-dir out/build/windows-msvc-debug -R "^(platform.units|platform.input_latency_source|stage11.death_formal.five_paths|stage11.death_evidence.mutation_self_test)$" --output-on-failure
```

Expected: 4/4 pass; existing death Esc/E semantics remain.

- [ ] **Step 5: Commit**

```powershell
git add src/platform/raylib tests/platform
git commit -m "feat: pause host and atomically apply settings"
```

---

### Task 9: Dynamic Control Hints from Binding Revision

**Files:**
- Create: `src/platform/raylib/control_hints.hpp`
- Create: `src/platform/raylib/control_hints.cpp`
- Create: `tests/platform/control_hints_tests.cpp`
- Modify: `src/platform/raylib/combat_renderer.hpp`
- Modify: `src/platform/raylib/combat_renderer.cpp`
- Modify: `src/platform/raylib/hud_renderer.cpp`
- Modify: `src/platform/raylib/raylib_host.cpp`
- Modify: `src/platform/raylib/CMakeLists.txt`
- Modify: `tests/platform/CMakeLists.txt`
- Modify: `tests/platform/platform_test_main.cpp`

**Interfaces:**

```cpp
struct ControlHints final {
    std::array<char, 160> primary{};
    std::array<char, 160> secondary{};
    std::uint64_t revision{};
};
void refresh_control_hints(ControlHints&, const settings::SettingsData&) noexcept;
```

`CombatRenderer::draw` and `draw_hud` receive `const ControlHints&`; host refreshes only after load or successful Apply.

- [ ] **Step 1: Write dynamic text and allocation RED tests**

Assert defaults, arbitrary swaps, revision refresh/no-refresh, null termination, all action labels present, no hardcoded stale key, and 10,000 unchanged refresh calls allocate zero.

- [ ] **Step 2: Verify RED**

Expected: platform target fails on missing hints API.

- [ ] **Step 3: Implement fixed-buffer hints and remove hardcoded HUD text**

Use stable-key labels and `snprintf`; retain fixed `F1/F12/Esc`. Do not rebuild strings in `draw_hud`.

- [ ] **Step 4: Run platform and source guard**

Run platform tests and:

```powershell
rg -n "WASD Move|J Light|K Jump|L Launcher|E Descend|I Inventory|P Star Chart" src/platform/raylib
```

Expected: tests pass; no stale hardcoded control line remains.

- [ ] **Step 5: Commit**

```powershell
git add src/platform/raylib tests/platform
git commit -m "feat: show current bindings in hud"
```

---

### Task 10: Settings Stress and Architecture Guards

**Files:**
- Create: `tests/settings/settings_stress_tests.cpp`
- Modify: `tests/settings/CMakeLists.txt`
- Modify: `tests/settings/settings_test_main.cpp`
- Create: `tests/platform/stage11b_architecture_guard_test.cmake`
- Modify: `tests/platform/CMakeLists.txt`

**Interfaces:**
- Produces CTest `stage11b.settings_stress.atomic_reload_zero_alloc`.
- Produces CTest `stage11b.architecture.settings_boundaries`.

- [ ] **Step 1: Write stress and boundary RED tests**

Run 1,000 swap/apply/load cycles across both slots with restart intervals 17/31/43. Compare direct/restart settings, revision 1..1000, valid uniqueness, final file bytes, and sentinel hashes of `slot-a.sav`/`slot-b.sav`. Allocation probes must show zero for validation, assign/swap, frame mapping and unchanged hint refresh; store I/O is explicitly outside zero-allocation scope.

Architecture guard rejects raylib/Combat/Dungeon/Items/Progression/Passives includes in `src/platform/settings`, settings includes in core modules, gameplay save filenames in settings store, and direct gameplay key constants in host/HUD.

- [ ] **Step 2: Verify RED**

Run the two new test names; expected missing test/guard failure before registration.

- [ ] **Step 3: Add environment switch and mutation self-checks**

The stress test must have a dedicated environment filter and a 300-second timeout. The guard self-test mutates a copied source to add `#include <raylib.h>`, `slot-a.sav`, direct `KEY_J`, and a duplicate stable key; each mutation must fail for the named reason.

- [ ] **Step 4: Run focused and full settings/platform tests**

Run:

```powershell
ctest --test-dir out/build/windows-msvc-debug -R "^stage11b\.(settings_stress|architecture)" --output-on-failure
ctest --test-dir out/build/windows-msvc-debug -R "^(settings.units|platform.units|persistence.units)$" --output-on-failure
```

Expected: new 2/2 and regression 3/3 pass.

- [ ] **Step 5: Commit**

```powershell
git add tests/settings tests/platform
git commit -m "test: stress stage 11b settings boundaries"
```

---

### Task 11: Real Raylib Pause/Settings Validation and Evidence Guards

**Files:**
- Modify: `src/platform/raylib/raylib_host.hpp`
- Modify: `src/platform/raylib/raylib_host.cpp`
- Create: `tests/platform/stage11b_settings_formal_game_validation.cpp`
- Create: `tests/platform/stage11b_settings_formal_validator.ps1`
- Create: `tests/platform/stage11b_settings_evidence_guard_test.cmake`
- Create: `tests/platform/stage11b_settings_evidence_guard_self_test.cmake`
- Create: `tests/platform/stage11b_settings_bad_host_input.txt`
- Modify: `tests/platform/CMakeLists.txt`

**Interfaces:**

```cpp
enum class Stage11BValidationScenario : std::uint8_t {
    none, paused_freeze, settings_page, rebound_attack,
    conflict_swap, restarted_settings, single_slot_recovery,
    corrupt_defaults
};
```

Validation configuration may select deterministic scenarios and capture paths, but cannot expose setters for PauseMenuState, SettingsData or logical actions.

- [ ] **Step 1: Write formal RED harness and production evidence guard**

Require fresh 1280×720 screenshots and summary fields: paused tick before/after 120 presented frames, player/monster hash, committed revision, old/new attack counts, swap pair, restart binding, one-slot recovery, corrupt-default status, and unchanged character-save hash.

Guard requires production `sample_physical_keys` → `map_host_frame_input` → pause gate → `submit_frame_actions`, one post-Present capture helper, actual SettingsStore save/load and actual host restart. It rejects direct `queue_action`, direct committed-state assignment, TestAccess, pre-Present capture and validation-only input setters.

- [ ] **Step 2: Verify RED**

Run the formal/evidence regex; expected missing target or guard failure.

- [ ] **Step 3: Implement deterministic validation through production input**

Inject only physical key edges before the normal mapper. Paused-freeze uses presented frames, not fake fixed tick counts. Rebound attack must prove old physical key produces zero actions and new physical key produces at least one action after Apply; restart destroys and reconstructs host runtime from the settings directory.

- [ ] **Step 4: Run formal, validator, guards and inspect images**

Run all `^stage11b.settings_(formal|evidence)` tests. Expected: formal scenarios and mutation self-test pass; screenshots are fresh, 1280×720 and visibly show pause/settings/current rebound key.

- [ ] **Step 5: Commit**

```powershell
git add src/platform/raylib/raylib_host.* tests/platform
git commit -m "test: validate stage 11b settings in raylib"
```

---

### Task 12: Documentation, Full Builds, Reviews, and Milestone Stop

**Files:**
- Modify: `README.md`
- Create: `docs/validation/stage11b-settings-input.md`

- [ ] **Step 1: Document exact player behavior and recovery**

README must list default keys, Esc overlay priority, Apply/Cancel, settings location, swap conflicts and reset defaults. Validation report maps every design requirement to CTest names, records V1 44-byte layout, dual-slot recovery, 1,000 stress, formal screenshot paths and explicitly excludes complete HUD/filter/macOS/Stage 11-C.

- [ ] **Step 2: Fresh Debug full gate**

Run in MSVC 19.44 / SDK 26100 environment:

```powershell
cmake --preset windows-msvc-debug --fresh
cmake --build --preset windows-msvc-debug --clean-first
ctest --preset windows-msvc-debug --output-on-failure
```

Expected: configure and all build steps succeed; every CTest including Stage 11-B stress/formal/evidence passes.

- [ ] **Step 3: Fresh Release full gate**

Run:

```powershell
cmake --preset windows-msvc-release --fresh
cmake --build --preset windows-msvc-release --clean-first
ctest --preset windows-msvc-release --output-on-failure
```

Expected: same complete suite passes with raylib 6.0.0 static linkage and fresh Release evidence.

- [ ] **Step 4: Boundary and artifact audit**

Run:

```powershell
git diff --check 918f2e938399275e821c331d848d1228fb0ef75a..HEAD
git status --short
git diff --stat 918f2e938399275e821c331d848d1228fb0ef75a..HEAD
```

Expected: no build, PNG, settings slots, save slots or logs are tracked; only Stage 11-B source/tests/docs are in scope.

- [ ] **Step 5: Two independent complete-diff reviews**

Both reviewers inspect `918f2e9..HEAD`, design invariants, settings file bytes, pause fixed-step freeze, input latency, V6 hash preservation, formal screenshots and evidence mutation coverage. Fix every Critical/Important/Minor finding in separate commits, rerun focused tests, then rerun complete Debug CTest after the final fix.

- [ ] **Step 6: Commit docs and stop**

```powershell
git add README.md docs/validation/stage11b-settings-input.md
git commit -m "docs: complete stage 11b settings milestone"
```

Expected: worktree clean on `codex/stage11b-settings-input`; do not merge `main` and do not start complete HUD, item filter, macOS or Stage 11-C.

---

## Requirements Coverage

| Requirement | Tasks |
| --- | --- |
| Stable defaults, validation, swap conflict | 1 |
| V1 fixed settings bytes and CRC | 2 |
| Atomic two-slot persistence and recovery | 3 |
| Configurable production input without latency | 4, 8 |
| Pause/settings state and overlay priority | 5, 8 |
| Bounded visible UI | 6 |
| Audio/fullscreen/VSync preview, apply, rollback | 7, 8 |
| HUD reflects current binding | 9 |
| 1,000 cycles, zero allocation, architecture | 10 |
| Real raylib proof and anti-injection evidence | 11 |
| Debug/Release full gates, docs and reviews | 12 |
