# Task 3 Brief — 材料页消费共享背包壳层

## 基线与范围

- Worktree: `E:\game\.worktrees\loot-suction-animation`
- Branch: `codex/full-chinese-ui`
- Expected HEAD: `f436a62`，并包含独立基线修复 `108c5d6`
- 开始前必须确认 tracked 工作树干净。
- 完整读取仓库 `AGENTS.md`、设计规格、实施计划 Task 3、TDD、writing-good-tests、verification-before-completion、requesting-code-review。
- 只允许修改 7 个 tracked 文件：
  - `src/platform/raylib/material_bag_renderer.hpp`
  - `src/platform/raylib/material_bag_renderer.cpp`
  - `src/platform/raylib/ui_text_catalog.hpp`
  - `src/platform/raylib/ui_text_catalog.cpp`
  - `tests/platform/material_bag_renderer_tests.cpp`
  - `tests/platform/ui_text_catalog_tests.cpp`
  - `tests/platform/stage16_loot_reinforcement_game_validation.cpp`
- 不删除文件，不改 Task 4 renderer，不启动任何 raylib 窗口，不运行 full/formal/Test.ps1/Release。

## 必须交付

严格执行计划 Task 3：

1. 新增 `material_bag_layout(Rectangle,float)`，14 个槽全部位于传入 content，三分辨率下每槽至少 `40x32`。
2. 新增 layout 参数版 `process_click`、`cycle_directed_category`、`draw`；这些 overload 不得内部重算 width/height 布局。
3. 保留旧 width/height wrapper，只能委托 `inventory_shell_layout -> material_bag_layout(content,scale) -> 新 overload`。`material_bag_detail_bounds` 暂留 Task 4 删除。
4. 14 个 `MaterialId` 均有 `ui_material_description()`；普通制作=`选择物品应用`，定向=`选择伤害 / 防御 / 速度 / 元素`，强化石/券=`强化装备`。逐项进入 catalog visitor，即使文本重复也不得合并。
5. catalog visitor 两处计数 `329 -> 343`；glyph 必须仍为 `639/640`、单 atlas、32MiB，不扩容量、不增加 atlas。
6. Stage16 本地新增 `Resolution` 与单项 `kResolution{1280,720}`，把 camera/query/layout/draw/capture/confirmation/所有旧格子、材料、action 坐标消费一次传播完整；清零 `inventory_layout(` 与 `material_bag_layout(1280, 720)`。
7. 为不扩大第 8 文件范围，保留现有精确入口 `run_production_scenario(run, shared_ui_font)`，让它仅委托新的三参核心 overload与 `kResolution`；这样 `shared_ui_font_lifecycle_guard_test.cmake` 的既有 exact token 保持有效。
8. 不新增 platform `TestCase`；把 RED/GREEN 断言并入现有 case，保持 selector 精确 `120/0`。

## TDD 与验证边界

1. 先只改既有测试，运行一次同进程 `. Configure.ps1 + arpg_platform_tests`，记录有效编译 RED；首因应是新 API/description 缺失，而非环境或测试语法问题。
2. 最小 GREEN 后，同一 PowerShell 进程 dot-source Configure 并构建 `arpg_platform_tests` 与 `arpg_stage16_loot_reinforcement_game_validation`，`--parallel 2`。
3. 只运行一次 `full_chinese_inventory_selector.ps1`，要求精确 `120 cases, 0 failures`，catalog `343`、glyph `639/640`。
4. `rg -n 'inventory_layout\(|material_bag_layout\(1280, 720\)' tests/platform/stage16_loot_reinforcement_game_validation.cpp` 必须无输出。
5. 不运行 Stage16 executable/window。
6. 运行 `git diff --check`；请求独立只读审查。发现问题按 receiving-code-review + TDD 修复后复验同一最小集合。

## 提交与报告

- 报告用 `apply_patch` 写到 `.superpowers/sdd/2026-08-13-poe2-inventory-layout/task-3-report.md`，包含 RED、GREEN、命令、结果、审查与 concerns。
- 只显式 stage 上述 7 文件，检查 `git diff --cached --check` 和 staged 路径。
- 单独提交：`refactor(ui): place materials in inventory shell`
- 回报完整 commit hash、验证证据、concerns、report 路径；不得推进 Task 4。
