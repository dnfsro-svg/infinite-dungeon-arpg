# Stage 11-B Task 7 实现报告

## 状态与范围

- 分支：`codex/stage11b-settings-input`。
- 基线：`38db8aab0d0ec2582b956514f03d2faf9dbb9205`。
- 提交标题：`feat: preview and rollback live settings`。
- 范围：只新增 live settings 事务编排、纯启动 flags/音量换算 helper、raylib 6.0 窗口/音量 adapter 与 headless fake backend 测试；未修改 `raylib_host.cpp`，未开始 Task 8。

## TDD 证据

### RED 1：接口缺失

先新增并接入 `window_settings_tests.cpp`，不创建生产接口，运行：

```powershell
& .\scripts\Configure.ps1 -Preset windows-msvc-debug -Fresh
cmake --build --preset windows-msvc-debug --target arpg_platform_tests
```

结果：exit `1`；MSVC `C1083`，无法打开 `window_settings.hpp`。失败来自 Task 7 接口缺失，属于预期 RED。

### RED 2：失败 setter 的部分副作用

首轮最小事务实现通过后，将 fake setter 收紧为“先改变状态、再按注入点返回 false”，要求失败字段自身也进入逆序 best-effort 恢复。运行 `platform.units` 得到 `166 cases, 2 failures`：

- volume setter 首点失败未恢复 volume；
- VSync setter 失败路径未尝试恢复失败字段。

将当前尝试字段在调用前纳入恢复集合后，同一测试为 `166 cases, 0 failures`。这样即使真实 backend 的失败返回伴随部分副作用，控制器仍会尝试恢复完整 current 快照。

## 已实现契约

- `apply_live_settings` 在任何 backend 调用前校验 current 与 desired；任一无效返回 `invalid`。
- 只比较 live 三字段；绑定和 revision 单独变化不调用 backend。
- 对所有实际变化预检必需 callback；缺少 volume setter、mode setter/readback 或 VSync setter/readback 均在零 backend 调用时返回 `backend_failed`。
- 固定正向顺序为 volume → mode → mode readback → VSync → VSync readback。
- setter 失败返回 `backend_failed`；mode/VSync 读回不一致返回 `readback_failed`。
- 失败后按 VSync → mode → volume 逆序尽力恢复 current；恢复 callback 返回失败仍继续后续字段，且保留最初错误分类。
- `rollback_live_settings(previewed, committed, backend)` 直接复用同一事务路径，不执行盲目无条件 setter。
- 所有事务与 helper 使用固定栈状态；1000 次 apply 的 allocation counter 不变。

## raylib 6.0 adapter

- 编译期锁定 `RAYLIB_VERSION 6.0.0`。
- `master_volume_fraction(percent)` 严格执行 `static_cast<float>(percent) / 100.0F`；adapter 仅在音频设备 ready 时调用 `SetMasterVolume`。
- runtime fullscreen 通过 `SetWindowState(FLAG_BORDERLESS_WINDOWED_MODE)`；逻辑读回将 borderless 或 fullscreen flag 都映射为 `WindowMode::fullscreen`。
- 恢复 windowed 时清除 borderless/fullscreen，固定 `SetWindowSize(1280, 720)`，再按当前 monitor 的 origin、width、height 居中。
- VSync 使用 `SetWindowState/ClearWindowState(FLAG_VSYNC_HINT)`，并用 `IsWindowState` 读回。
- `initial_window_flags` 始终保留现有 resizable，并按 committed 的四种 mode/VSync 组合派生 `FLAG_FULLSCREEN_MODE` 与 `FLAG_VSYNC_HINT`。固定 raylib 6.0 的 pre-InitWindow 路径使用 fullscreen flag；borderless 只在窗口创建后的 runtime adapter 使用。

## 测试覆盖

新增 12 个 cases，覆盖：

- current/desired 无效且零调用；
- 合法 noop 与空 backend；
- 三字段调用顺序和值；
- 音量 0%、55%、100% 换算及 0/100 端点调用；
- volume/mode/VSync 三个 setter 失败点及逆序恢复；
- 五个 callback 各自缺失；
- mode/VSync readback mismatch；
- rollback 同路径；
- 恢复 callback 失败容忍与原始错误保留；
- 四种 initial flags；
- 1000 次事务零分配。

## 最终验证

```powershell
& .\scripts\Configure.ps1 -Preset windows-msvc-debug
cmake --build --preset windows-msvc-debug --target arpg_platform_tests arpg_settings_tests arpg_game
out\build\windows-msvc-debug\bin\arpg_platform_tests.exe
out\build\windows-msvc-debug\bin\arpg_settings_tests.exe
ctest --test-dir out/build/windows-msvc-debug -R "^(platform\.units|platform\.pause_menu_state_boundary|architecture\..*|platform\.input_latency_source|platform\.host_input_source|platform\.module_boundary|settings\.units)$" --output-on-failure
git diff --check
```

结果：

- `arpg_platform_tests`、`arpg_settings_tests`、`arpg_game` 构建/链接成功。
- 平台二进制：`166 cases, 0 failures`。
- settings 二进制：`35 cases, 0 failures`。
- platform、pause、core/no-raylib、module、input、settings 边界：`24/24`，0 失败。
- `git diff --check` 无空白错误；仅有仓库现有 LF/CRLF 转换提示。

## 残余风险与停止边界

- 本 Task 不允许接入 host，因此 adapter 只完成编译链接与 fake orchestration 验证；真实窗口切换、音频设备和启动加载闭环属于 Task 8。
- raylib setter 是 void API，adapter 的真实结果由事务层的 mode/VSync readback 判定；音量没有 raylib backend 成功读回契约。
- 未修改 Combat、Dungeon、Persistence、设置 store 或输入 host；未开始 Task 8。
