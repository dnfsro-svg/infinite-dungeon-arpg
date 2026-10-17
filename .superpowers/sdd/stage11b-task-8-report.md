# Stage 11-B Task 8 实现报告

## 状态与范围

- 分支：`codex/stage11b-settings-input`。
- 提交标题：`feat: pause host and atomically apply settings`。
- 范围：仅 Task 8 的 host 集成、启动参数透传、host gate/source guard 与平台测试；未修改 Combat、Dungeon、Persistence 或 SettingsStore 内部实现。

## TDD 证据

先接入 `pause_host_gate_tests.cpp`、`--settings-dir` 测试和 source guard，未修改生产代码：

- source guard RED：`raylib host pause frame gate is missing`；
- MSVC 构建 RED：缺少 `HostFrameGateResult/gate_host_frame`；
- MSVC 构建 RED：`HostLaunchOptions` 缺少 `settings_directory`。

完成最小实现后，source guard、平台 169 cases、Stage 11 death formal/mutation 均转绿。首次四项回归唯一失败为测试清单计数 `actual 169 != expected 168`；Task 8 实际新增 2 个 pause gate cases 和 1 个 settings-directory case，修正期望值后通过。

## 已实现契约

- 启动先解析 save/settings directory；settings directory 未指定时复用已解析的 save directory。
- `SettingsStore::load()` 在 `SetConfigFlags(initial_window_flags(...))` 与 `InitWindow` 之前执行；加载结果初始化 committed/draft、输入映射、启动窗口 flags 与初始音量。
- 每个 presented frame 只调用一次 `sample_physical_keys()`，同一 `PhysicalKeySnapshot` 驱动 death/recovery、inventory/passive、pause 与 gameplay 映射。
- death/recovery 保持最高优先级；Esc 先关闭 inventory/passive 并标记 consumed；普通 Esc 才打开 pause；暂停状态阻断 gameplay actions/movement/descent。
- `gate_host_frame` 在暂停入口清空 fixed-step accumulator；暂停 600 个 presented frames 始终返回 0 steps，不推进 DungeonSnapshot；恢复首帧不 catch up，steps 不超过固定上限。
- 暂停时仍更新 renderer/feedback、绘制 world 与 pause overlay、present/capture，但 `runtime.fixed_tick` 只消费 gate 返回的 steps。
- Settings 预览只更新 live 三字段；Apply 顺序为 live apply/readback → `SettingsStore::save` → 仅 `committed` 状态时发布 committed/input settings。
- preview 或 store 失败时不发布 committed；尝试回滚到 committed，保留 draft 并在 pause UI 写入可重试错误消息。
- 新增 `--settings-dir`，冻结为绝对路径并与 `--save-dir` 独立；重复参数拒绝；`main.cpp` 透传到 `RaylibHostConfig`。

## 验证

构建命令（在锁定的 VS 2022 17.14 / MSVC 19.44 / Windows SDK 10.0.26100.0 环境）：

```powershell
cmake --build --preset windows-msvc-debug --target arpg_platform_tests arpg_game
```

指定回归：

```powershell
ctest --test-dir out/build/windows-msvc-debug -R "^(platform.units|platform.input_latency_source|stage11.death_formal.five_paths|stage11.death_evidence.mutation_self_test)$" --output-on-failure
```

结果：4/4 tests passed；平台二进制 169 cases、0 failures；game 与 platform tests 编译/链接成功。`git diff --check` 无空白错误，仅显示仓库现有 LF/CRLF 转换提示。

## 残余风险

- Headless gate/source guard 验证冻结、顺序和事务发布边界；真实 fullscreen/VSync/音量设备行为仍依赖 Task 7 已验证的 raylib backend 和实际机器窗口环境。
- raylib 音量 API 没有读回；Apply 的 mode/VSync 读回由 Task 7 backend 负责，音量失败以音频设备 readiness/setter 返回值判定。

## 审查修复

修复提交：`e030dcd fix: harden pause host input and settlement`。

审查指出三项 host 集成问题，均先补回归再修改生产路径：

1. interact 重绑定后，死亡继续错误地消费 `frame_input.keys.e`。修复为 `host_death_input_gate` 从当前帧唯一的 `PhysicalKeySnapshot` 固定读取 `StableKey::e`；Esc、截图和 debug 字段仍沿用同一帧全局输入。Stage 11 自动验证只设置 gate 的验证专用 continue 结果，不再修改 gameplay/interact 映射。
2. `defaults_corrupt` 加载状态此前初始化后丢失。新增一次性 `HostSettingsNotice`；仅 corrupt 排队，missing 与 recovered single-slot 不排队。notice 只在首次 root → settings 转换且状态机清空旧 message 后发布 `设置已恢复默认值`，随后消费。
3. `WindowShouldClose()` 此前位于 while 条件，可能跳过同帧 Apply。修复为每帧先采集 close 请求，再同步执行 `settle_host_pause_command`；Apply 成功发布或失败回滚/拒绝后才返回 exit 决策。

### 审查修复 TDD

- RED：新增 5 个 host cases 后编译因 `host_death_input_gate`、`HostSettingsNotice`、`settle_host_pause_command` 缺失失败。
- GREEN：平台测试由 169 增至 174 cases，0 failures。
- Stage 11 evidence guard 同步固定 E 路径；mutation self-test 仍验证 validation bypass、条件绕过和错误 capture 顺序会被拒绝。

最终命令与结果：

```powershell
cmake --build --preset windows-msvc-debug --target arpg_platform_tests arpg_game
ctest --test-dir out/build/windows-msvc-debug -R "^(platform.units|platform.input_latency_source|stage11.death_formal.five_paths|stage11.death_evidence.mutation_self_test)$" --output-on-failure
git diff --check
```

结果：game/platform 无待构建错误；指定 CTest 4/4 通过；平台 174 cases、0 failures；diff check 无空白错误，仅有现有 LF/CRLF 提示。
