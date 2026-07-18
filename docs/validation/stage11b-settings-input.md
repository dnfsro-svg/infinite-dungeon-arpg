# Stage 11-B 暂停、设置与按键重绑定验证记录

## 范围与结论

本记录覆盖 `codex/stage11b-settings-input` 相对 Stage 11-A 基线
`918f2e938399275e821c331d848d1228fb0ef75a` 的暂停、设置持久化、键盘重绑定和
动态提示闭环。它不改变战斗数值、攻击 tick、随机流、地下城状态机或 V6 角色存档
布局；角色存档与设置文件由不同的双槽文件管理。

本阶段明确不包含：完整 HUD 重制、地面物品过滤、主菜单、手柄、鼠标侧键、组合键、
分辨率或画质设置、macOS 和 Stage 11-C。

## 玩家可见行为

- 正常玩法按 `Esc` 进入暂停；暂停首帧清空固定步累积器，暂停期间不提交玩法动作、
  不采样玩法移动、不调用 Session tick，恢复后不追赶暂停时间。
- 输入优先级为：恢复错误、死亡/等待继续、已打开背包或星盘、暂停/设置、正常玩法。
  因而背包或星盘打开时第一次 `Esc` 只关闭该层；死亡和恢复保留 Stage 11-A 的退出
  语义，不会打开暂停。
- 十项稳定动作按固定顺序为上、下、左、右、轻击、跳跃、上挑、交互、背包、星盘，
  默认 `W/S/A/D/J/K/L/E/I/P`。支持 A–Z、0–9、方向键、Space、左右 Shift 和左右 Ctrl。
  `Esc`、`Enter`、`F1`、`F12`、`V` 与窗口关闭为全局安全键，不可重绑。
- 重绑捕获只消费下一次允许键的按下沿；占用键会交换；无效键不改变草稿；`Esc` 或失焦
  取消捕获。恢复默认只修改草稿，需 Apply 才生效。
- 主音效范围 0–100、步长 5、默认 100；默认窗口化、VSync 开启。运行时值先预览并读回，
  Apply 成功才保存/发布；取消或保存失败会回滚到已提交状态。
- 设置目录默认和 host 的数据/存档目录相同，文件固定为 `settings-a.bin` 和
  `settings-b.bin`，也可用 `--settings-dir` 隔离。单个合法槽可恢复；两个槽都损坏、
  版本/CRC/长度/reserved/枚举/绑定无效时回到默认，并在暂停设置页显示一次“设置已恢复默认值”。
  设置恢复不会归档、覆盖或触发 V6 角色存档恢复。

## V1 设置记录与持久化

每个设置槽固定 **44 字节**，不序列化 C++ 结构体：

| 偏移 | 长度 | 字段 |
| --- | ---: | --- |
| 0–7 | 8 | ASCII magic `ARPGSET1` |
| 8–9 | 2 | little-endian format `1` |
| 10–11 | 2 | little-endian payload size `20` |
| 12–19 | 8 | little-endian revision / generation |
| 20 | 1 | master SFX percent |
| 21 | 1 | window mode |
| 22 | 1 | VSync |
| 23 | 1 | reserved zero |
| 24–33 | 10 | 十项 StableKey |
| 34–39 | 6 | reserved zero |
| 40–43 | 4 | bytes 8–39 的 CRC32 |

保存仅在启动或 Apply 发生：选择旧/无效槽，写入 `.tmp`、刷新、原子替换并读回验证。
revision 只在成功保存后递增；溢出、校验失败、写入失败或读回失败均不发布草稿。设置
store 不引用 raylib、Combat、Dungeon、Items、角色 Persistence codec 或角色槽文件名。

## 需求到自动化证据的映射

| 设计要求 | 主要 CTest 证据 |
| --- | --- |
| 默认设置、十项稳定动作、保留键和冲突交换 | `settings.units`、`platform.units` |
| 45 个稳定键与 raylib 双向映射、单帧单次物理采样、无输入延迟 | `platform.units`、`platform.host_input_source`、`platform.input_latency_source` |
| V1 44-byte 布局、CRC、截断/尾随/未知字段拒绝 | `settings.units` |
| 双槽最新选择、单槽恢复、冲突/损坏回默认、原子替换与读回失败 | `settings.units`、`stage11b.settings_stress.atomic_reload_zero_alloc` |
| 设置不触碰角色 V6、1,000 次 swap/apply/load、17/31/43 重启间隔与零分配热路径 | `stage11b.settings_stress.atomic_reload_zero_alloc`、`persistence.units` |
| 暂停状态机、Esc 优先级、捕获取消、同帧 Esc+攻击丢弃 | `platform.units`、`platform.pause_menu_state_boundary` |
| 暂停 600 帧冻结、恢复无追帧、死亡/继续保持优先 | `platform.units`、`stage11.death_formal.five_paths`、`stage11.death_evidence.mutation_self_test` |
| 1024×576、1280×720、1920×1080 的有界暂停/设置绘制 | `platform.units` |
| 音量、全屏、VSync 预览、读回、Apply、取消与回滚 | `platform.units` |
| HUD 提示随 committed binding revision 刷新，无硬编码旧玩法键 | `platform.units`、`platform.input_latency_source` |
| 设置模块和 host/HUD 的依赖/硬编码边界 | `stage11b.architecture.settings_boundaries`、`platform.module_boundary`、`architecture.core_no_raylib`、`architecture.combat_no_raylib` |
| 生产 host 的暂停、设置、重绑、交换、重启、单槽和双槽恢复 | `stage11b.settings_formal`、`stage11b.settings_evidence_validator` |
| 正式证据必须使用物理输入链、暂停 gate、后 Present 截图且能拒绝旁路 | `stage11b.settings_evidence_guard`、`stage11b.settings_evidence_guard_self_test` |

完整 Debug/Release CTest 还保留 Stage 11-A `stage11.death_formal.five_paths` 和
`stage11.death_evidence.*`，以证明死亡路径和角色 V6 语义未被设置功能改变。

## 正式 raylib 证据

`stage11b.settings_formal` 启动生产 `platform::run_raylib_host`，只注入物理按键边沿并经过
`sample_physical_keys → Stage 11-B physical-edge injection → map_host_frame_input → pause gate → submit_frame_actions`。它在
`EndDrawing()` 后的唯一捕获 helper 生成以下 fresh 1280×720 PNG：

- `out/build/windows-msvc-release/tests/platform/stage11b settings evidence/pause.png`
- `out/build/windows-msvc-release/tests/platform/stage11b settings evidence/settings.png`
- `out/build/windows-msvc-release/tests/platform/stage11b settings evidence/rebound.png`
- `out/build/windows-msvc-release/tests/platform/stage11b settings evidence/swap.png`
- `out/build/windows-msvc-release/tests/platform/stage11b settings evidence/restart.png`
- `out/build/windows-msvc-release/tests/platform/stage11b settings evidence/single-slot.png`
- `out/build/windows-msvc-release/tests/platform/stage11b settings evidence/corrupt.png`

同目录的 `stage11b-settings-evidence.txt` 记录真实 V6 `run_a.sav`/`run_b.sav` 的字节
hash 与大小、暂停前/后 tick、恢复前/后 tick、玩家/怪物 hash、旧/新攻击的生产
`queue_action` 接受数、交换对、committed revision、重启绑定、单槽/双槽恢复状态与中文
“设置已恢复默认值”可见性。正式子进程以带空格的证据目录运行，并通过受控 `.cmd` 中的逐项
双引号引用启动。`stage11b.settings_evidence_validator` 检查这些字段和四张 PNG 尺寸；
`stage11b.settings_evidence_guard[_self_test]` 还强制 `LoadImageFromScreen` 与
`runtime.fixed_tick` 各只有一条受控生产路径，并以 Present 前截图和额外 tick 源码突变验证拒绝。

## Task 12 交付门禁

在 MSVC 19.44、Windows SDK 10.0.26100.0、CMake 3.25+、Ninja 环境中，对每个配置执行：

```powershell
.\scripts\Configure.ps1 -Preset windows-msvc-debug -Fresh
cmake --build --preset windows-msvc-debug --clean-first
ctest --preset windows-msvc-debug --output-on-failure

.\scripts\Configure.ps1 -Preset windows-msvc-release -Fresh
cmake --build --preset windows-msvc-release --clean-first
ctest --preset windows-msvc-release --output-on-failure
```

本次 Task 12 已在 2026-07-18 的 MSVC 19.44 / Windows SDK 10.0.26100.0 环境完成：

- Debug：fresh configure、clean-first **263** 个构建目标成功，完整 CTest **61/61** 通过；
  最终日志结束于 09:33:15。
- Release：fresh configure、clean-first **263** 个构建目标成功，完整 CTest **61/61** 通过；
  最终日志结束于 09:38:29。
- 后续整改的 Debug formal 证据位于带空格目录 `stage11b settings evidence`：真实 V6 双槽
  `run_a.sav`/`run_b.sav` 分别为 460 bytes，前后 hash 相等；暂停先推进 1 tick，再冻结 120
  presented frames（`1/1`），恢复后恰推进 1 tick（`1→2`，无 catch-up）；旧 `J` 接受数 `0`、
  新 `U` 接受数 `1`。`corrupt.png` 可见红色中文“设置已恢复默认值”，并由字体加载状态和正式
  验证器共同确认。

Release CTest 会重新运行 `stage11b.settings_formal`，因此上述 Release 证据不是 Debug 产物的复用。

完成前还执行：

```powershell
git diff --check 918f2e938399275e821c331d848d1228fb0ef75a..HEAD
git status --short
git diff --stat 918f2e938399275e821c331d848d1228fb0ef75a..HEAD
```

审计要求是无已跟踪 build 目录、PNG、设置槽、角色槽或日志；仅 Stage 11-B 源码、测试、
设计/计划和本验证文档进入该分支。
