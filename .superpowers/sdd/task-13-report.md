# Stage 10 Task 13 实施报告

## 结果

- 新建顶层 `README.md`（仓库此前不存在 README），记录 Stage 10 当前里程碑、固定工具链、构建/测试/运行命令、默认存档目录和完整操作键位。
- 新建 `docs/validation/stage10-abyss-combat.md`，记录 Debug/Release 全量结果、独立 stress/formal/fixture 证据、截图像素护栏、设计第 4～14 节映射和十项硬审计。
- 设计源规格实际只到第 14 节；文档明确 Task 13 简报“4～18”为过时编号，不虚构第 15～18 节。
- 实际 CTest 清单为 42，不强改回计划的 38：Stage 9 / main 基线 33，Stage 10 正向 6（Task 1 的 `abyss.units` 加 Task 12 的 5 项），后增 `WILL_FAIL` 负向守卫 3，即 `33 + 6 + 3 = 42`。旧计划漏算 `abyss.units` 且未包含 3 个后增负向项。
- 未发现需要修改 Tasks 1～12 代码的真实缺陷；未合并 main，未推进 Stage 11。

## 固定检查点

1. 新增/修改的稳定接口：无。只增加文档，不改变公开接口、玩法或存档结构。
2. Red 失败：无功能测试 Red。本机普通 PowerShell 首次直接 `cmake --preset windows-msvc-debug` 因缺少 `WindowsSDKVersion` 被项目硬门禁拒绝；改用仓库 `scripts/Configure.ps1` 进入固定 MSVC x64 / SDK 26100 环境后成功。这是环境前置条件，不是代码缺陷，也未放宽门禁。
3. Green 验证：Debug 配置 1.29s、全构建 1.53s、42/42 CTest 238.70s；Release fresh 配置 3.34s、220 步全构建 48.69s、42/42 CTest 161.80s。独立 stress exit 0（0.12s）、formal exit 0（12.53s）、fixture exit 0（0.12s）。
4. 随机域/存档/零分配影响：无代码变化。复验证据为 1000 房 `36/4000`、方向 `9/11/9/7`、27 次 V5 重载、hash `0xe102b17e6423351b`；96/384/96/192 满池运行 600 tick，分配增量 0。
5. 提交：`768e8771dcd46a01d04a1f6a03737744c96de631`（`docs: record stage 10 abyss validation`）。

## 正式证据

- 7 张正式 raylib 截图均为 fresh 1280x720，实际采样色 `31/21/28/44/42/42/52`，非背景样本 `3461/3600/3600/3600/3600/3600/3600`。
- 自动化正式 raylib 窗口路径为 `death=PASS reset=PASS restart=PASS abyss_hole_descent=PASS`。
- 主代理用 Computer Use 启动 Release 游戏和隔离存档 `out/manual-qa-20260716`，发送移动/A、J、L、F12、Esc 并确认基本输入响应；人工重开四份正式路径存档后，death/reset/restart 的 HUD 均为 `Depth 1 / ABYSS NO`，hole 为 `Depth 2 / ABYSS NO`。人工复核截图为 `death-reopen.png`、`reset-result.png`、`restart-result.png`、`hole-result.png`，均 ignored 不提交。
- 路径动作由自动化正式窗口执行；人工只独立复核最终落盘状态和基本输入，不声称人工完整重演四条动作。
- 7 张自动化正式截图只位于忽略的 `out/build/windows-msvc-debug/bin/stage10-formal-game-validation`；4 张人工复核截图只位于忽略的 `out/manual-qa-20260716`。两处均为 ignored，未提交任何构建产物或截图。
- 静态占位符扫描和核心 raylib include 边界扫描均无匹配；`git diff --check` 通过。

## 提交卫生

- 提交只包含 `README.md` 与 `docs/validation/stage10-abyss-combat.md`。
- `git status --short` 在提交后无输出。
- 相对 main 无 `out/`、`build-release/`、`.scratch/`、`task3_trace_probe.obj` 或 Stage 10 PNG 误提交。
