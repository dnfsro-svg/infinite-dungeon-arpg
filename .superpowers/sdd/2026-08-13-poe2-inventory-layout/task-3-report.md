# Task 3 Report — 材料页消费共享背包壳层

## 范围

- 基线：`f436a62a6693bdea800daa352ac8b1666d260f3b`
- 分支：`codex/full-chinese-ui`
- tracked 修改严格限于 Task 3 简报列出的 7 个文件。
- 未运行 Stage16 可执行文件或任何 raylib 窗口；未运行 full/formal、`scripts/Test.ps1` 或 Release。

## TDD RED

命令：

```powershell
. .\scripts\Configure.ps1 -Preset windows-msvc-debug
cmake --build --preset windows-msvc-debug --target arpg_platform_tests --parallel 2
```

结果：exit 2。有效首因均为待实现接口缺失：

- `material_bag_layout(Rectangle, float)` 无匹配 overload；
- `MaterialBagRenderer::process_click(..., MaterialBagLayout)` 缺失；
- `MaterialBagRenderer::cycle_directed_category(..., MaterialBagLayout)` 缺失；
- `ui_material_description(MaterialId)` 缺失。

Configure 成功；失败不是环境、测试语法或链接问题。

## 实现

- `MaterialBagLayout` 直接消费 shell `right_panel` 与 `geometry_scale`，14 槽按 2×7 计算；三种支持分辨率每槽至少 40×32，且完全位于 content。
- 新增 layout 参数版 click、directed-category 与 draw；旧 width/height wrapper 只执行 shell → material layout → 新 overload。
- `material_bag_detail_bounds` 仅保留旧 renderer 的过渡裁切语义，计划由 Task 4 删除。
- 14 个 `MaterialId` 逐项映射中文说明并逐项进入 catalog visitor；visitor 精确从 329 增至 343。
- Stage16 新增本地 `Resolution` 与 `kResolution{1280,720}`；camera/query/grid/material/action/confirmation/draw/capture/readback 均传播同一 resolution。
- 保留精确二参 `run_production_scenario(run, shared_ui_font)`，只委托三参核心与 `kResolution`。
- 未新增 platform `TestCase`；材料 suite 基线与当前均为 6 项。

## GREEN 与验证

聚焦构建：

```powershell
. .\scripts\Configure.ps1 -Preset windows-msvc-debug
cmake --build --preset windows-msvc-debug --target `
  arpg_platform_tests arpg_stage16_loot_reinforcement_game_validation `
  --parallel 2
```

结果：exit 0；两个目标均编译/链接完成，Stage16 executable 未运行。

selector：

```powershell
powershell.exe -NoProfile -File `
  tests/platform/full_chinese_inventory_selector.ps1 `
  -ExecutablePath out/build/windows-msvc-debug/bin/arpg_platform_tests.exe
```

结果：exit 0；`120 cases, 0 failures`；catalog `visited=343`；glyph `639/640`；单 atlas；预算 `33554432` bytes。

静态与 whitespace：

- `rg -n 'inventory_layout\(|material_bag_layout\(1280, 720\)' tests/platform/stage16_loot_reinforcement_game_validation.cpp`：无输出，rg exit 1。
- `git diff --check`：exit 0。
- tracked diff 路径：恰好 7 个允许文件。

调试说明：首次 selector 揭示旧 detail 裁切测试仍把新 shell 材料面板当旧三列材料袋；根因确认后，保留旧 detail helper 的过渡裁切语义，并把既有 case 迁移为同时验证旧 detail 和新 shell bag。另有一次未 dot-source Configure 的独立增量命令因 MSVC 环境缺失报 `stdarg.h`；未据此改代码，随后在同一已配置 PowerShell 进程的正式聚焦构建 exit 0。

## 独立只读审查

- 结论：Critical 0；Important 0；Minor 0；Ready=yes。
- 审查者明确撤销了一次关于“新增 TestCase”的误报：基线与当前均为 6 项，实际是既有第二项 1:1 迁移/改名。

## Concerns

- Task 3 未执行窗口视觉验收，符合简报禁止启动 Stage16 窗口的边界；视觉证据由后续 Task 5 的确定性 producer 负责。
