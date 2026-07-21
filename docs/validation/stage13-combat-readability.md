# Stage 13 战斗可读性与首房间清晰度验收报告

验收日期：2026-07-20

工作树：`E:\game\.worktrees\stage12-material-pack`

分支：`codex/stage12-material-pack`
计划：`docs/superpowers/plans/2026-07-20-stage13-combat-readability.md`

## 结果

Stage 13 通过验收。战斗规则、怪物 AI、死亡流程和房间生成规则保持不变；本阶段只增强平台层视觉反馈和洞口交互可用性。

已交付：

- HUD 与死亡回顾使用更大字号和 2px 描边。
- 怪物生命/护盾/韧性、词缀、角色名和阶段名移动到角色上方，并按怪物索引进入四条稳定信息通道。
- 玩家受击后显示 0.55 秒的来源方向箭头。
- 除纯支援怪外，全部直接攻击者在 telegraph/active 阶段显示地面警告；高威胁敌人额外显示 `!`。
- `defeated` 事件生成一个 0.65 秒固定池击杀标记，绘制 `DEFEATED` 和扩散圆环。
- 洞口提示改为 26px 的 `E: DESCEND`，显示与交互半径同步由 2.0 扩大到 3.25。
- 普通房间洞口仍为每房间独立固定 10%，没有保底、补偿或重掷。

## 构建与自动化验证

Release 构建命令：

```powershell
.\scripts\Build.ps1 -Preset windows-msvc-release
```

结果：通过。项目脚本确认 MSVC 19.44、Windows SDK 10.0.26100.0、C++17，重新编译 `raylib_host.cpp` 并链接 `arpg_game.exe` 与全部正式验证器。

精确单元测试：

```text
arpg_platform_tests.exe: 308 cases, 0 failures
arpg_combat_tests.exe:   205 cases, 0 failures
dungeon.units + platform.units: 2/2 passed
```

最终完整 Release 回归：

```powershell
ctest --test-dir out/build/windows-msvc-release --output-on-failure -j 1
```

结果：`82/82 passed`，`0 failed`，总耗时 `613.22 sec`。覆盖核心、深渊、战斗、地下城、持久化、成长、词缀、天赋、装备、设置、输入延迟、模块边界、固定池零分配、Stage 9-12 正式游戏与证据守卫。

Stage 10 正式实际游戏流程再次通过：

```text
formal_captures=7
death=PASS
reset=PASS
restart=PASS
abyss_hole_descent=PASS
```

七张呈现后截图内容指标全部 PASS。

## 完整回归中发现并解决的问题

第一次完整回归为 `80/82`：

- `stage11d.loot_evidence_guard`
- `stage11d.loot_evidence_guard_self_test`

根因是 Stage 12 的普通截图条件直接读取 `stage11d_loot_validation`，但该读取位于 Stage 11D 的隔离接缝之外。修复仅把判断计算移入已有 `visible_capture` 接缝，外部改用通用布尔值；运行行为不变。

修复验证：

- 基线守卫通过。
- 包含 21 个恶意变体的自测通过。
- 最终 82 项完整顺序回归通过，排除了测试间状态污染。

## 真实窗口验收

使用 Windows Computer Use 启动实际 Release 产物：

```text
out/build/windows-msvc-release/bin/arpg_game.exe
window: Infinite Dungeon - Stage 3 Dungeon Rules
```

电脑控制实际发送了 `E`、`J` 与 `F12`。`E` 能从死亡回顾继续，F12 能由游戏的呈现后截图路径生成真实 PNG。Windows Graphics Capture 对该 OpenGL 窗口返回白帧，因此不使用外部白帧判断游戏画面，改用游戏自身 F12 和正式验证器的呈现后截图。

证据：

- `docs/validation/evidence/stage13/death-overlay.png`：实际游戏自然死亡后由电脑控制触发 F12；标题、最后一击、防御列和 `E 继续` 清晰可读。
- `docs/validation/evidence/stage13/attack-warning-defeat.png`：正式实际游戏帧；显示玩家 HUD、怪物上方信息、telegraph 地面圈和 `DEFEATED` 击杀反馈。
- `docs/validation/evidence/stage13/exit-confirmation.png`：正式实际游戏退出/下层路径帧；Stage 10 汇总同时确认 `abyss_hole_descent=PASS`。

SHA-256：

```text
attack-warning-defeat.png DF576E51CFF3E128DB4945BF0C1F981C86F6835334F64C8B8A552F6A3F2EE19C
death-overlay.png          0DCA73289FB65474614166A9DBAA5F291BCD34F6487AA737153CC9110F5E0F90
exit-confirmation.png      40B4DD2D4B8521E26E918D21F1D16596C5D29D76FA9A64A360901D3EB6DB06BB
```

## 规则与差异审计

- `git diff --check`：通过，无空白错误。
- `src/combat/combat_world.cpp`：无语义 diff，无敌帧仍为 30 ticks。
- `src/dungeon/room_generation.cpp`：无语义 diff，仍直接使用独立随机值与阈值比较。
- `src/dungeon/encounter_director.cpp`：无语义 diff，怪物生成/AI 未改变。
- 洞口交互半径在 `dungeon_transition.cpp` 与 `dungeon_view_math.hpp` 均为 3.25。
- Stage 13 仅选择本阶段源码、测试、计划、报告和证据提交；Stage 12 已存在的资产源、报告和历史截图改动保持未暂存，未删除或覆盖。
