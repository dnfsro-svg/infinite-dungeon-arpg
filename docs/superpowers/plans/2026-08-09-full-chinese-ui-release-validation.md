# Full Chinese UI Release Validation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 建立“无玩家英文残留、无缺字/越界、对比度合格、单字体资源受控”的发布门禁，并完成 Windows Release 部署和逐页实机截图验收。

**Architecture:** 自动化层组合静态展示目录遍历、玩家路径源码白名单、字形覆盖、文本边界和颜色对比测试；发布层使用隔离存档/设置目录启动安装版，按分辨率与页面矩阵留存内部截图和哈希证据。

**Tech Stack:** C++17 单测、CMake/CTest、PowerShell、Python 标准库、MSVC Debug/Release、raylib 内部 F12 截图。

## Global Constraints

- 依赖前三份全量中文实施计划完成并通过各自 completion gate。
- 完整 Windows 自动化必须运行 `scripts/Test.ps1 -Preset windows-msvc-debug`。
- 完整 Release 必须运行 `scripts/Build.ps1 -Preset windows-msvc-release`。
- 部署必须使用 `scripts/DeployLauncher.ps1`，并验证 build/install 文件哈希一致。
- 实机验收只使用隔离 save/settings/screenshots 目录，不修改默认存档和历史证据夹具。
- 不因视觉验收放宽 4.5:1 对比度、字形覆盖、UTF-8、零分配或内存上限。
- 任何自动化或截图未覆盖的要求均视为未验证，不可报告完成。

---

## File Structure

- Create: `tests/platform/full_chinese_ui_source_test.py` — 玩家路径英文字符串白名单扫描。
- Create: `tests/platform/full_chinese_ui_source_test.cmake` — CTest 调用和确定性输出封装。
- Modify: `tests/platform/CMakeLists.txt` — 注册 source gate。
- Modify: `src/platform/raylib/ui_text_bounds_audit.hpp/.cpp` — 增加被动树、恢复页、F1、确认弹窗审计角色。
- Modify: `src/platform/raylib/ui_text_contrast.hpp` 或对应实现 — 对明确背板计算实际状态色对比度。
- Modify: `tests/platform/hud_font_tests.cpp`、`hud_layout_tests.cpp`、`hud_render_plan_tests.cpp`、`pause_menu_view_tests.cpp`、`material_bag_renderer_tests.cpp`、`passive_tree_view_tests.cpp`、`debug_overlay_renderer_tests.cpp`、`death_overlay_view_tests.cpp` — 完整发布断言。
- Create: `docs/validation/full-chinese-ui.md` — 自动化结果、哈希、截图矩阵和人工结论。

---

### Task 1: 玩家可见英文残留门禁

**Files:**
- Create: `tests/platform/full_chinese_ui_source_test.py`
- Create: `tests/platform/full_chinese_ui_source_test.cmake`
- Modify: `tests/platform/CMakeLists.txt`
- Modify: `tests/platform/ui_text_catalog_tests.cpp`

**Interfaces:**
- Consumes: 明确的玩家可见源文件列表和 `ui_text_catalog`。
- Produces: CTest `platform.full_chinese_ui_source`，失败时输出文件、行号和原字符串。

- [ ] **Step 1: 写会命中现有残留的 source gate**

Python 脚本固定扫描：pause/settings、HUD/notice、inventory/material、passive、room/dungeon、death、debug、active skill、actor、recovery host、默认窗口标题和 `src/launcher` 正式界面相关源文件。提取 C/C++ 字符串字面量，排除以下精确类别：

- `TraceLog`、验证报告和断言上下文中的字符串。
- 资产路径、文件扩展名、CMake/环境变量名。
- 物理按键字面量 `A-Z`、`Esc`、`Enter`、`F1`、`F12`、`Num1` 至 `Num5`、`Left Shift`、`Right Shift`。
- 内部格式标识 `ARPGSV10`、`ARPGSET1`。

任何其他包含连续两个以上 ASCII 字母的玩家字符串均失败。报告格式固定为：

```text
player-visible English literal: src/platform/raylib/pause_menu_view.cpp:201: Continue
```

- [ ] **Step 2: 运行并确认 RED**

Run:

```powershell
ctest --preset windows-msvc-debug --output-on-failure -R 'platform.full_chinese_ui_source'
```

Expected: 至少命中尚未迁移的真实玩家英文；不得用扩大白名单使 RED 消失。

- [ ] **Step 3: 完成精确白名单和目录一致性校验**

脚本只豁免技术上下文，不豁免整文件。`ui_text_catalog_tests` 同时遍历目录，断言除按键 token 外不含连续英文单词。若某个玩家字符串确需保留，必须在脚本中以“文件+精确字符串+原因”三元组登记。

- [ ] **Step 4: 运行并确认 GREEN**

Expected: `platform.full_chinese_ui_source` 通过且扫描文件数非零；故意临时把一个目录文本改回 `Continue` 时该测试精确 RED，随后立即恢复并再次 GREEN。

- [ ] **Step 5: 提交 source gate**

```powershell
git add tests/platform/full_chinese_ui_source_test.py tests/platform/full_chinese_ui_source_test.cmake tests/platform/CMakeLists.txt tests/platform/ui_text_catalog_tests.cpp
git diff --cached --check
git commit -m "test: reject player-visible English UI text"
```

---

### Task 2: 文本边界与对比度发布门禁

**Files:**
- Modify: `src/platform/raylib/ui_text_bounds_audit.hpp`
- Modify: `src/platform/raylib/ui_text_bounds_audit.cpp`
- Modify: `src/platform/raylib/ui_text_contrast.hpp`
- Modify: 对应页面测试文件。

**Interfaces:**
- Consumes: 每个页面的真实文字矩形、字体、字号、背板色和状态色。
- Produces: 完整页面审计角色、零越界/截断诊断和 `>=4.5:1` 比率断言。

- [ ] **Step 1: 扩展审计角色并写失败测试**

增加 `UiTextAuditPage::passive/recovery/debug`，以及 `inventory_dialog`、`passive_node`、`passive_tooltip`、`recovery_body`、`debug_line` 等角色。对 800×450、1280×720、1920×1080 输入完整中文语料，断言：

```cpp
ARPG_REQUIRE(audit.out_of_bounds_count == 0U);
ARPG_REQUIRE(audit.truncated_count == 0U);
ARPG_REQUIRE(platform::ui_luma_contrast_ratio(
    style.primary, style.backing) >= 4.5F);
ARPG_REQUIRE(platform::ui_luma_contrast_ratio(
    style.danger, style.backing) >= 4.5F);
```

- [ ] **Step 2: 运行并确认 RED**

Expected: 新页面未记录、某些文本仍直接绘制或背板/颜色未向审计提供。

- [ ] **Step 3: 接入全部页面和实际背板色**

每个 renderer 在绘制前调用统一审计函数；对纹理按钮传其规定的深色内底，而不是假设整张纹理颜色。禁用、警告、错误、成功状态同时记录颜色和背板。常规中文不得依赖缩到规定最小字号以下才能容纳。

- [ ] **Step 4: 运行平台定向测试并提交**

```powershell
cmake --build --preset windows-msvc-debug --target arpg_platform_tests --parallel 2
& '.\out\build\windows-msvc-debug\bin\arpg_platform_tests.exe'
git add src/platform/raylib/ui_text_bounds_audit.hpp src/platform/raylib/ui_text_bounds_audit.cpp src/platform/raylib/ui_text_contrast.hpp tests/platform/hud_layout_tests.cpp tests/platform/hud_render_plan_tests.cpp tests/platform/pause_menu_view_tests.cpp tests/platform/material_bag_renderer_tests.cpp tests/platform/passive_tree_view_tests.cpp tests/platform/debug_overlay_renderer_tests.cpp tests/platform/death_overlay_view_tests.cpp
git diff --cached --check
git commit -m "test: enforce Chinese UI readability bounds"
```

---

### Task 3: 全量自动化与资源审计

**Files:**
- Create: `docs/validation/full-chinese-ui.md`
- No production source changes in this task; a failing suite returns work to the owning RED→GREEN task before this gate is rerun.

**Interfaces:**
- Consumes: 三份实现计划的全部结果。
- Produces: Debug 全量测试、Release 构建、字体资源与持久化回归证据。

- [ ] **Step 1: 记录提交前状态和资源基线**

```powershell
git status --short --untracked-files=all
Get-Process cl,ninja,cmake,ctest,arpg_game -ErrorAction SilentlyContinue |
  Select-Object Id,ProcessName,WorkingSet64
```

确认无其他构建/游戏进程后再开始。记录 Noto 字体 SHA256、现有 Release exe SHA256 和可用内存。

- [ ] **Step 2: 运行完整 Windows Debug 自动化**

```powershell
& '.\scripts\Test.ps1' -Preset windows-msvc-debug
```

Expected: exit 0，所有 CTest 通过。若失败，只修复与本分支相关的真实问题；修复后重跑对应定向测试，再完整重跑 `Test.ps1`。

- [ ] **Step 3: 运行核心兼容回归**

```powershell
cmake --build --preset windows-msvc-debug --target arpg_persistence_tests arpg_settings_tests arpg_combat_tests --parallel 2
& '.\out\build\windows-msvc-debug\bin\arpg_persistence_tests.exe'
& '.\out\build\windows-msvc-debug\bin\arpg_settings_tests.exe'
& '.\out\build\windows-msvc-debug\bin\arpg_combat_tests.exe'
```

Expected: 三个套件全部 0 failure；checkpoint wire baseline、设置 codec、战斗数值均未改变。

- [ ] **Step 4: 构建 Release 并记录真实字体资源**

```powershell
& '.\scripts\Build.ps1' -Preset windows-msvc-release
```

Expected: exit 0。正式 Stage 12 证据必须显示：`bundled_font_instance_count=1`、字形数不超过 640、单/总图集均不超过 32 MiB、CJK ready。

- [ ] **Step 5: 写入验证文档并提交自动化结果所需修复**

`docs/validation/full-chinese-ui.md` 记录完整命令、时间、exit code、suite 结果、字形数、图集尺寸和核心兼容结果；不写“预计通过”。

```powershell
git add docs/validation/full-chinese-ui.md
git diff --cached --check
git commit -m "docs: record full Chinese UI verification"
```

---

### Task 4: Release 部署一致性

**Files:**
- Update: `docs/validation/full-chinese-ui.md`。
- No production source changes in this task; a packaging mismatch becomes a separate TDD fix and this deployment task restarts from Step 1.

**Interfaces:**
- Consumes: 已通过的 `windows-msvc-release/bin`。
- Produces: 安装目录与构建目录逐文件一致的 103 文件应用树。

- [ ] **Step 1: 确认没有运行中的游戏或部署进程**

```powershell
Get-Process arpg_game,InfiniteDungeonLauncher,cmake,ninja,cl -ErrorAction SilentlyContinue
```

若存在本任务启动的游戏，先正常 `CloseMainWindow()` 并等待退出；不得终止无关进程。

- [ ] **Step 2: 执行唯一一次部署**

```powershell
& '.\scripts\DeployLauncher.ps1' -GameBuildDirectory `
  '.\out\build\windows-msvc-release\bin'
```

Expected: exit 0，staging 原子替换成功。

- [ ] **Step 3: 比较 build/install 哈希**

递归比较 `arpg_game.exe`、launcher、assets 和字体文件的相对路径、长度、SHA256；差异数必须为 0。把 exe 和字体哈希写入验证文档。

- [ ] **Step 4: 提交部署证据文档**

```powershell
git add docs/validation/full-chinese-ui.md
git diff --cached --check
git commit -m "docs: record Chinese UI deployment hashes"
```

---

### Task 5: Windows 逐页实机截图验收

**Files:**
- Update: `docs/validation/full-chinese-ui.md`
- Evidence output: `out/full-chinese-ui-<timestamp>/screenshots/`

**Interfaces:**
- Consumes: 已部署安装版、隔离 save/settings/screenshots 目录、键盘驱动。
- Produces: 三种分辨率的页面矩阵、人工视觉结论和关闭后状态证明。

- [ ] **Step 1: 建立隔离验收目录**

在 `%LOCALAPPDATA%\InfiniteDungeon\full-chinese-ui-<timestamp>` 下创建 `save`、`settings`、`screenshots`；复制已知有效双槽存档和静音设置，并记录源/副本 SHA256。不得把游戏指向原夹具或默认目录。

- [ ] **Step 2: 启动安装版并验证中文字体就绪**

启动参数包含独立 `--save-dir`、`--settings-dir`、`--screenshot-dir`。等待可见窗口，F1 检查中文字体就绪、实例数 1、无 font fallback；F12 后立即把固定截图复制成唯一名称。

- [ ] **Step 3: 捕获 800×450 页面矩阵**

用 Win32 客户区尺寸调整为 800×450，逐页捕获：暂停根页、设置主页/音频/显示/按键、按键捕获、背包空/有装备、材料袋、强化危险弹窗、被动树 tooltip、普通 HUD、洞口/深渊、死亡/恢复、F1。

- [ ] **Step 4: 捕获 1280×720 和 1920×1080 页面矩阵**

重复同一关键页面；1280×720 必须覆盖全部页面，1920×1080 至少覆盖暂停、设置、背包、被动树、HUD、死亡和 F1。每次 F12 后立刻复制并命名，避免固定文件被覆盖。

- [ ] **Step 5: 逐图人工审查**

每张图使用图像查看工具检查：

- 无玩家可见英文，按键 token 除外。
- 无方块、乱码、半字符、截断、重叠或面板越界。
- 选项、页签、按钮相对完整交互区域居中。
- 属性表标签左、数值右。
- 正文和背板肉眼清晰，选中/禁用/警告/错误/成功不只依赖颜色。
- F1 在 800 单列、1280/1920 两列时均不溢出。

任何 FAIL 都回到对应任务做 RED→GREEN，重新 Build、Deploy 并只重拍受影响页面，不能把失败截图标为通过。

- [ ] **Step 6: 正常退出和隔离状态核验**

从菜单正常退出，等待 PID 消失。确认无 `arpg_game/cl/cmake/ninja/ctest` 残留；原夹具 SHA256 不变；隔离 V10 双槽可重新启动并保持中文 UI。

- [ ] **Step 7: 完成验证文档并提交**

把每张绝对路径、分辨率、页面、PASS/FAIL、审查人结论写入 `docs/validation/full-chinese-ui.md`。

```powershell
git add docs/validation/full-chinese-ui.md
git diff --cached --check
git commit -m "docs: validate full Chinese UI release"
```

---

## Final Completion Audit

- [ ] 逐条回读 `docs/superpowers/specs/2026-08-09-full-chinese-ui-design.md` 的 12 节要求，并在验证文档链接对应自动化或截图证据。
- [ ] 运行 `git diff --check`、`git status --short --untracked-files=all`，确认无遗留测试变异、临时源文件或意外改动。
- [ ] 运行 `platform.full_chinese_ui_source`，确认英文白名单仅含物理键和技术标识。
- [ ] 核对完整 Debug、核心兼容、Release、部署哈希和实机页面矩阵均有当前提交之后的证据。
- [ ] 只有所有项目均有直接证据时，才报告本次“全部汉化、选项居中、布局合理、高对比度”完成；之后恢复持续试玩并继续发现问题。
