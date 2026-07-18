# Stage 11-C 完整 HUD 验证记录

## 范围与结论

Stage 11-C 在已验收 Stage 11-B 基线
`bd8c8be6dd02614c85a117d869520c57fa7aca1f` 上，把常驻开发文字墙替换为
面向玩家的中文优先 HUD。它只读取已提交 `DungeonSnapshot`、可选
`CombatSnapshot`、`DungeonRenderStatus`、已提交 `ControlHints` 和平台表现通知，
不修改战斗数值、固定时间步、随机流、地下城规则、物品生成、角色 V6 存档或设置格式。

本阶段明确不包含地面物品过滤、主菜单、手柄、组合键、鼠标侧键、分辨率/UI 缩放/
画质设置、macOS、新战斗状态、目标锁定、首领血条、伤害数字重做和 Stage 11-D。

## 玩家可见 HUD

- 左下：生命、条件显示的护盾、经验、等级、未分配点及减速/腐蚀/无敌状态；低生命
  只使用有界频率边框强调。
- 上中：波次与剩余目标、下一波、清房出口、深渊规则及提交/保存状态，不泄露未生成内容。
- 右上：深度、当前层房间序号、当前生态及火/水/电/混沌四种偏向，不预测隐藏结果。
- 下中：固定容量通知队列只呈现一主一次；保存/恢复、深渊放弃确认、洞口/出口、
  清房/奖励/升级、背包/星盘/操作提示按固定优先级竞争。动态按键文字只来自已提交
  `ControlHints`。
- `F1`：预算、活跃对象、饱和、非法 owner、tick、commit generation、room seed、
  存档/设置 revision 和 HUD 诊断只在调试层显示。

## 架构、容量与布局门禁

`HudViewModel`、`HudNoticeState` 和纯绘制计划使用固定数组与固定文本缓冲区；生产 HUD
不得采样输入、调用 Session/Store/RNG/fixed tick，也不得在热路径创建动态字符串或容器。
`stage11c.hud_stress.zero_alloc_100k` 分别覆盖首次冷路径和 100,000 次稳定路径；五种场景
完整经过 ViewModel、通知、布局、玩家/怪物/目标/导航/情境/文本/F1 纯计划，冷路径与稳定
路径均为零堆分配。输入快照前后 hash 相同。

`stage11c.architecture.hud_boundaries[_self_test]` 覆盖 HUD、combat renderer、debug overlay
及 host HUD seam，并用独立源码突变证明会拒绝输入采样、Session、fixed tick、SettingsStore、
`std::string`、`std::vector`、普通 HUD 的 `Budget` 和第四个可见状态标签。F1 调试层允许
`Budget`，普通/恢复 host HUD seam 不允许。

`platform.units` 验证 1024x576、1280x720、1920x1080 三档安全区、面板互不覆盖、
条形比例、低生命强调、中文语料、动态绑定和字体计划。NotoSansSC 候选字体与 Stage 11-B
死亡覆盖层共享固定码点计划；正式验收要求 `font_ready=1`，不能以 fallback 缺字方框冒充成功。

## 设计章节到自动化证据映射

| 设计章节 | 主要 CTest 证据 |
| --- | --- |
| 1–2 目标、范围和只读表现边界 | `stage11c.architecture.hud_boundaries`, `stage11c.architecture.hud_boundaries_self_test`, `platform.module_boundary` |
| 3.1 玩家状态与怪物战斗条 | `platform.units`, `combat.units` |
| 3.2 房间目标 | `platform.units`, `stage11c.hud_formal`, `stage11c.hud_evidence_validator` |
| 3.3 地下城导航与极端计数 | `platform.units`, `stage11c.hud_stress.zero_alloc_100k`, `stage11c.hud_evidence_validator` |
| 3.4 两级通知、优先级、去重与暂停冻结 | `platform.units`, `stage11c.hud_stress.zero_alloc_100k`, `stage11c.hud_formal` |
| 3.5 F1-only 开发诊断 | `platform.units`, `stage11c.architecture.hud_boundaries`, `stage11c.hud_formal` |
| 4 固定容量、只读组件边界与零分配 | `stage11c.hud_stress.zero_alloc_100k`, `stage11c.architecture.hud_boundaries`, `stage11c.architecture.hud_boundaries_self_test` |
| 5 通知时间、去重、换房和溢出 | `platform.units`, `stage11c.hud_stress.zero_alloc_100k` |
| 6 三分辨率、配色、中文字体与有界动效 | `platform.units`, `stage11c.hud_formal`, `stage11c.hud_evidence_validator`, `stage11c.hud_evidence_validator_self_test` |
| 7 覆盖层、输入优先级与无新增延迟 | `platform.input_latency_source`, `platform.host_input_source`, `stage11b.settings_formal`, `stage11.death_formal.five_paths` |
| 8 缺快照、夹紧、截断、字体失败和极端值 | `platform.units`, `stage11c.hud_stress.zero_alloc_100k`, `stage11c.hud_evidence_validator_self_test` |
| 9 真实 raylib、验证器与反注入 | `stage11c.hud_formal`, `stage11c.hud_evidence_validator`, `stage11c.hud_evidence_validator_self_test`, `stage11c.hud_evidence_guard`, `stage11c.hud_evidence_guard_self_test` |
| 9.4 input/death/settings/persistence 回归 | `platform.units`, `platform.input_latency_source`, `platform.host_input_source`, `stage11.death_formal.five_paths`, `stage11b.settings_formal`, `persistence.units`, `settings.units` |

## 正式 raylib 六场景证据

`stage11c.hud_formal` 为六个场景分别启动独立进程，经真实 `run_raylib_host`、物理按键
采样、映射、覆盖层门禁、Session 提交和固定帧推进到达生产状态；截图只在唯一的
`EndDrawing()` 后捕获点生成。每个配置的证据目录是：

```text
out/build/windows-msvc-debug/tests/platform/stage11c hud evidence/
out/build/windows-msvc-release/tests/platform/stage11c hud evidence/
```

| 场景 | 1280x720 PNG | 单场景摘要 |
| --- | --- | --- |
| 正常战斗 | `combat.png` | `combat.txt` |
| 低生命与状态 | `low-health.png` | `low-health.txt` |
| 清房与出口 | `cleared.png` | `cleared.txt` |
| 深渊放弃确认 | `abyss-warning.png` | `abyss-warning.txt` |
| 升级与未分配点 | `level-up.png` | `level-up.txt` |
| F1 调试层 | `debug.png` | `debug.txt` |

同目录 `stage11c-hud-evidence.txt` 汇总六个生产 snapshot hash、PNG hash、场景有效标记
和最终 `result=pass`。`stage11c.hud_evidence_validator` 检查 PNG 签名、尺寸、新鲜度、
正面积布局、玩家值、状态、目标、通知、导航、`font_ready=1`、F1 与逐场景/聚合 hash；
中文关键区域还检查面板覆盖、明亮字形和空心缺字方框。validator self-test 对真实证据执行
命名突变；evidence guard self-test 拒绝直接 ViewModel/TestAccess/逻辑动作注入、Present 前
截图、伪造字体/PASS/hash/summary 和绕过 Session。

## Fresh Debug/Release 全量门禁

2026-07-18 在 MSVC 19.44、Windows SDK 10.0.26100.0、CMake 3.25+、Ninja 和
raylib 6.0.0 static 环境按以下顺序从头验证：

```powershell
. .\scripts\Configure.ps1 -Preset windows-msvc-debug -Fresh
cmake --build --preset windows-msvc-debug --clean-first -- -j1
ctest --preset windows-msvc-debug --output-on-failure

. .\scripts\Configure.ps1 -Preset windows-msvc-release -Fresh
cmake --build --preset windows-msvc-release --clean-first -- -j1
ctest --preset windows-msvc-release --output-on-failure
```

点调用 `Configure.ps1` 让脚本建立的 MSVC 开发环境保留给同一 PowerShell 进程中的后续构建；
`-j1` 用于避免 Windows PDB 并发写入的偶发锁冲突，不改变构建内容或测试范围。两种配置均
确认执行 `Cleaning... 279 files` 并重新完成 `[279/279]`：

| 配置 | Fresh clean-first 构建 | 完整 CTest | 总测试时间 |
| --- | --- | --- | --- |
| Debug | 279/279，exit 0 | 70/70，0 failed | 394.15 秒 |
| Release | 279/279，exit 0 | 70/70，0 failed | 261.79 秒 |

Fresh Debug 中 Stage 11-C 压力/正式画面/证据验证/验证器自测/架构门禁分别为
0.58/16.75/4.04/9.06/0.03 秒，架构自测 0.21 秒；Release 分别为
0.18/13.81/4.30/9.47/0.03 秒，架构自测 0.24 秒。两种配置的六个场景摘要均为
六项 `*_valid=1` 与 `result=pass`。输入延迟源码门禁最终 Debug/Release 分别为
9.57/8.69 秒，host 输入源码门禁分别为 0.04/0.04 秒；既有死亡、设置、持久化、
地下城和 240 项平台单元场景全部保留在 70 项完整门禁中。

Task 10 两轮 review 修复提交 `c2fd48d`、`a36ce94` 后，未改变 CMake 测试注册或生产构建
产物，因此直接在
最终分支再次执行完整 Debug `ctest --preset windows-msvc-debug --output-on-failure`：
70/70、0 failed、438.50 秒。该次最终复跑的 Stage 11-C 压力/正式画面/证据验证/
验证器自测/架构门禁/架构自测分别为 0.57/16.87/3.97/9.21/0.03/0.23 秒；强化后的
input latency/host input 完整链门禁分别为 19.09/34.46 秒。两项门禁先清除注释与字符串，
再要求唯一提交严格位于 physical sample、Stage 11-B 注入、Stage 11-C 注入、逻辑映射、
死亡/暂停/覆盖层门禁及 `forward_actions` 合取之后，并由 `if (forward_actions)` 控制；
原位 mutation 会拒绝 map-before-Stage11C、Stage11C bypass、submit-before-gate、
unconditional-submit-bypass、host-gate-bypass、unconditional-descent-bypass，以及
comment-only/string-only 伪造链。host gate 还要求 `pause_open` 的 true 调用与非背包覆盖
分支的 false 调用各一次并绑定真实参数；`forward_descent` 合取和条件内
`request_descent(in_range)` 尾链同样纳入验证。

构建配置输出明确为 `Building raylib static library`。Release 源码头声明
`RAYLIB_VERSION_MAJOR/MINOR/PATCH = 6/0/0`，生成
`out/build/windows-msvc-release/lib/raylib.lib`；`build.ninja` 的游戏链接依赖该静态库。
`dumpbin /dependents out/build/windows-msvc-release/bin/arpg_game.exe` 只列出 Windows 与
MSVC 运行库，不含 `raylib.dll`。

## 分支边界与停止条件

最终审计以 Stage 11-B 基线执行 `git diff --check`、`git status --short`、`git diff --stat`
和已跟踪生成物扫描。允许命中的历史受控 PNG/log 必须逐项解释；不得跟踪新生成的
Stage 11-C PNG、log、save、settings 或 build artifact。

2026-07-18 审计结果：`git diff --check bd8c8be...HEAD` 无输出；提交前工作树只有本记录
与 README 的预期文档变更。相对基线的功能/测试/规格差异限于 Stage 11-C HUD 与两项
保持历史门禁有效的验证器修复。已跟踪生成物扫描只命中 Stage 8 的十张既有验收 PNG、
Stage 9 的四张既有验收 PNG（`01-fixed-affix-room-render.png`、
`01-fixed-affix-room.png`、`02-formal-game-initial.png`、
`03-formal-game-submitted-frame.png`）和 `task10-fixture-full.log`；这些均为 Stage 9 及更早
里程碑保留的受控验收证据，未新增 Stage 11-C PNG、log、存档、settings 双槽文件或
`out/build` 产物。

Stage 11-C 完成后保留 `codex/stage11c-complete-hud` 工作树和分支，不合并 `main`，
不开始地面物品过滤、主菜单、手柄、分辨率/画质设置、macOS 或 Stage 11-D。
