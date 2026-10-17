# Task 4 Brief — 真实 InventoryRenderer 切换到三页 POE2 壳层

## 基线与硬边界

- Worktree: `E:\game\.worktrees\loot-suction-animation`
- Expected clean HEAD: `632c9b9b53548b47029d6f550dd3b6587d0067f2`
- 完整读取根/工作树 `AGENTS.md`、设计规格、实施计划 Task 4（必须逐行执行）、TDD、writing-good-tests、systematic-debugging、receiving/requesting review、verification-before-completion。
- 只允许修改计划 Task4 列出的 30 个 tracked 文件。开始和提交前都必须逐路径核对；不得扩大到 `raylib_host.cpp`/`host_validation_runtime.cpp`，它们的通用 capture plumbing 已够用。
- 禁止删除文件、reset/checkout、启动任何 raylib 窗口、运行 formal/real-raylib producer、full、`Test.ps1`、Release 或默认游戏。
- Task4 是 renderer/验证源码接线，不生成截图；Task5 才运行 fixture。

## 先决 RED

先只修改两个 direct guard：

- `tests/platform/inventory_renderer_source_test.cmake`
- `tests/platform/full_chinese_inventory_progression_source_test.cmake`

按计划 Step1 把旧三列/detail 锚迁成三页 shell、frame cache、输入优先级、tooltip retained-first、visible-source、transient/status、overlay、无 legacy detail 等真实 live wiring 要求。不要先改 self-test 或生产。

分别 direct/exact 运行并取得有效 RED：旧 renderer 被“缺真实 wiring”拒绝；若是语法、抽取失败、stale anchor，先修 guard 自身再记录 RED。不要并行/重复启动长 guard。

## 生产与测试必须完整兑现

严格以计划正文为权威，特别注意：

1. `InventoryFrameLayout` 成为 resize-aware 唯一坐标源；input/draw 都只消费成员，不重复 builder。
2. `open/close/resize/font identity` 清 frame/tooltip/transient/font cache；首次/字体失效 tooltip fail-closed。
3. 从 `sync()` 删除旧 comparison 刷新；唯一 `build_tooltip_frame()` 以同一个 presentation id 刷新 comparison、构造 view/layout。
4. tooltip 两阶段：先对 frozen/selected 逐帧 visible-source resolver 并处理 retained blocker，再允许 direct candidate；旧 source 不跨 filter/page/scroll/resize 成为权威。
5. 输入层严格为：confirmation > tab > tooltip > directed > cancel > material select > material target > page action > filter > skill content > recipe > equipment > background clear。
6. authority 仅 accepted 后推进；skill select 是 presentation，本身不等 authority。
7. draw 顺序：background/header/tabs/panels/silhouette/page/material后overlay/empty/tooltip/status-action-hint/confirmation最后。
8. 每帧 transient helper恰一次，立即回写 next；input 不推进180帧；tone 是颜色唯一语义源。
9. 全部装备卡/装备槽走 `inventory_item_visual_state()`；hover浅钢优先级最低，素材成功/fallback之后都画 overlay。
10. 删除所有计划列出的 legacy API/alias/detail，包括 Task2 临时 `active_skill_loadout_layout(int,int)` wrapper；迁完30文件内全部消费者。
11. 装备六槽必须围绕剪影，不继续纵向堆叠。
12. tooltip 宽度/内容/比较按规格完整：scaled 360且物理224–630，左右来源落对侧，最大内容不缺段；全零显示“装备后无变化”。
13. Stage12 默认空装备页精确 17 次、3 roles、移除 bit19；producer/validator/self-test三方一致。
14. Stage16 direct-render 三分辨率×16场景，所有状态可确定复现，PNG尺寸回读代码只编译不运行。
15. Stage17 host链三分辨率×4技能场景；1280保留旧7图，800/1920显式skip legacy；pending interlock只能由 capture complete推进；CLI/save/summary隔离。
16. 所有 guard/self-test、host/input/latency、ground receipt与CMake timeout同步，但 Task4不新增Task5的32类mutants。
17. 保留 Task1新增的 `source_invalidated`，不要删回旧接口。

## 实施与验证纪律

- 用 `apply_patch` 编辑。
- 分小阶段：pure math/tests → renderer/input/draw → Stage12 → Stage16 → Stage17 → guards/self-tests。
- 每阶段先编译/静态校验再继续；遇到失败用 systematic-debugging，不猜、不循环重跑。
- 最终只执行计划 Step7 的最小验证集合：4目标build、selector120/0、两组inventory guards、host sequence、ground receipt、三个host consumer guards、四份PowerShell AST。不要启动producer。
- 已知 `input_latency_source` 可能约10分钟，只运行一次最终版；不要在 injector 未定型时跑。
- 请求独立只读审查，至少覆盖：材料定向右击、tooltip blocker/visible resolver、authority accepted、旧detail清零、素材成功overlay、Stage17 pending/skip。

## 报告与提交

- 报告：`.superpowers/sdd/2026-08-13-poe2-inventory-layout/task-4-report.md`，用apply_patch写，记录有效RED、每阶段GREEN、最终命令、未运行项、审查与 concerns。
- 只显式 stage 计划30文件；`git diff --cached --name-only` 必须与清单精确一致，`git diff --cached --check` 通过。
- 单提交：`feat(ui): adopt POE2 inventory layout`
- 回报完整hash/验证/审查/report；不得推进Task5。
