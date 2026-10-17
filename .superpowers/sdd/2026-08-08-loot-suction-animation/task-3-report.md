# CPLAY-021 Task 3 Windows Release 验收报告

## 结论

**未完成，按资源保护阈值中止。** 本任务没有修改生产代码、没有部署、没有启动游戏，也没有生成截图或写入存档；因此不能对安装版哈希、吸入动画、背包数量或 V10 存档作出结论。

## 构建与资源监控

启动前检查：无 `cl`、`cmake`、`ninja`、`ctest`、`arpg_game` 或 `arpg_launcher` 残留；空闲物理内存为 8.85 GiB。

执行的唯一构建命令：

```powershell
$env:CMAKE_BUILD_PARALLEL_LEVEL='2'
.\scripts\Build.ps1 -Preset windows-msvc-release
```

构建正常推进至 Ninja `298/441`。过程中最初两个 `cl.exe` 单进程峰值观察到 3.567 GiB，仍低于 6 GiB 阈值；随后空闲物理内存降至 **0.50 GiB**，低于任务规定的 2 GiB 阈值，因此立即发送中断。构建进程退出码为 1（由中断导致），不是编译错误结论。

中断后立即读回：空闲内存恢复至 10.04 GiB，10 秒后为 10.06 GiB；无 `cl`、`cmake`、`ninja`、`ctest`、`arpg_game` 或 `arpg_launcher` 残留。

## 未执行项目

- `DeployLauncher.ps1`：未执行，遵守“部署仅一次且仅在构建成功后”的前置条件。
- Build/安装版 `arpg_game.exe` SHA-256 对比：无有效构建/部署产物，不执行。
- seed 6118 隔离存档启动、键盘驱动输入、内部 F12 截图、地面/飞行/腰侧三态：未执行。
- 背包 `+1`、`ground=0`、V10 存档读回：未执行。

## 现场状态

- 工作树：`E:\game\.worktrees\loot-suction-animation`
- 未修改生产代码；本报告为本任务唯一写入文件。
- 未启动游戏，故不存在需要关闭的游戏进程或新建隔离 save/settings/screenshot 目录。

## 后续条件

需先在拥有至少约 8 GiB 持续空闲内存的状态下，调整构建脚本只生成 Release 游戏/启动器目标，或将构建隔离到资源更充足的环境；之后重新执行一次完整 Task 3 验收。不能用定向单元测试替代 Windows 视觉结论。

## 受控续构补充

在初次中断后，主控要求不清理目录、不开新完整构建，改为单路续构。预检空闲内存为 9.67 GiB，仍无目标进程。

按要求再次调用 `Build.ps1`（`CMAKE_BUILD_PARALLEL_LEVEL=1`）时，该脚本会自行调用 `Configure.ps1`；它没有清理构建目录，但完整默认目标仍会继续编译无关测试。构建器的单个 `cl.exe`（PID 14100）先后观察到 6.94--7.82 GiB 工作集，空闲内存最低 1.31 GiB，双重超过本任务的 6 GiB/2 GiB 阈值，已立即停止。`ninja` 日志显示中断前最后已完成的测试 TU 是 `tests/dungeon/CMakeFiles/arpg_stage8_validation_input.dir/stage8_validation_input.cpp.obj`；中断时正在编译的 TU 未写入 Ninja 日志，无法在停止后可靠还原，不能猜测。

中断后空闲内存恢复至 9.97 GiB，10 秒后 8.25 GiB，且无目标残留进程。按“不再重试完整 Build”的指令，随后仅加载同一 MSVC x64/SDK 环境，执行：

```powershell
$env:CMAKE_BUILD_PARALLEL_LEVEL='1'
cmake.exe --build --preset windows-msvc-release --target arpg_game --parallel 1
```

结果：退出码 0，`ninja: no work to do.`，即正式游戏目标已经有效；未重新配置、未清理，也未编译测试。

随后唯一一次部署命令为：

```powershell
.\scripts\DeployLauncher.ps1 -GameBuildDirectory E:\game\.worktrees\loot-suction-animation\out\build\windows-msvc-release\bin
```

结果：在复制前布局校验失败，原因为 Release `bin` 中缺少 `无限地下城启动器.exe`。此目录存在 `arpg_game.exe` 和受管 assets，但没有部署所需启动器，所以脚本没有替换安装目录、没有复制任何文件，也没有触发构建。

因此，安装版哈希核对、seed 6118 游戏启动、内部截图三态、背包/V10 读回仍未执行。当前阻塞条件已从内存转为 **Release 启动器产物缺失**；在明确允许只补建 `arpg_launcher` 目标或修复其产物路径前，不应以旧安装版或工作树直启替代本任务的安装版验收。

## 最终受控验收（成功）

在获得许可后，没有再运行完整构建或任何测试目标；仅加载 MSVC 环境并执行：

```powershell
cmake.exe --build --preset windows-msvc-release --target arpg_launcher --parallel 1
```

该启动器目标在单路下成功链接。随后唯一一次有效部署成功。哈希读回如下，均一致：

| 内容 | SHA-256 / 汇总 SHA-256 | Build 与安装版 |
| --- | --- | --- |
| `arpg_game.exe` | `4D73EC2BFEF1419BA57BE1E14B530E5DBEC5EF569B15977CBC8723CAC39BBA8B` | 一致 |
| `无限地下城启动器.exe` | `6E92DC29AF7B13E980EC6BECF6E160383D8D62D6A248E31B058711D8804428EE` | 一致 |
| 受管 `assets` 文件清单 | `D4360435CD88496DEC3B0024A39BEB3AE49B62F05AE94DB79DABA27F0E6FCF06` | 一致 |

### seed 6118 实机路线与视觉证据

使用最终全新隔离目录：

```text
C:\Users\dnfsr\AppData\Local\InfiniteDungeon\cplay021-loot-suction-motion-20260808-230059
```

其中 save/settings/screenshots 相互隔离；`settings-a.bin` 是通过正式 V3 编码和 CRC 写入的零音量配置。安装版 `arpg_game.exe` 以 `--seed 6118` 启动，且游戏 stdout/stderr 从未被重定向。

首次按静态坐标的路线未命中：实查发现火房会把默认 `(0,0)` 角色挤到 `(-1.41,0)`，而拔刀斩伤害发生在第 46 固定帧（约 0.77 秒），这解释了早期 300--350ms 截图的假阴性。最终使用右上接近并以 Num2 扩大覆盖，随后用单一外部键盘驱动进程以 `F12:30,A:30` 交替发送键沿；PowerShell mtime 监听器即时复制每张内部截图，避免多进程 F12 调用约 0.46 秒的间隔。

关键截图（均为游戏内部 F12 截图）：

- `screenshots/storm-22.png`：地面清晰显示 `普通 Shadow Hood · i1`。
- `screenshots/pickup-05.png`：拾取前，地面标签仍存在。
- `screenshots/pickup-06.png` 至 `pickup-09.png`：标签消失，品质/图标飞行与角色腰侧收纳光环处于连续变化状态。
- `screenshots/pickup-06.png` 起：HUD 通知显示 `已拾取：普通 Shadow Hood · i1`；后续帧保持已拾取状态。

`pickup-*.png` 共 20 张，mtime 监听器未漏掉吸入过程的关键变化。静态序列可以可靠确认地面标签消失、飞行/收纳表现和事务完成通知；它不是以单元测试替代视觉结论。

### 存档与资源收尾

游戏通过暂停菜单的 Quit Game 确认流程正常退出。最终 `run_b.sav` 为 88,144 字节，文件头为 `ARPGSV10`，版本字段为 10；最终存档目录：

```text
C:\Users\dnfsr\AppData\Local\InfiniteDungeon\cplay021-loot-suction-motion-20260808-230059\save
```

正常退出后读取：无 `cl`、`cmake`、`ninja`、`ctest`、`arpg_game` 或 `arpg_launcher` 残留。

本任务未修改生产代码；本报告为任务记录。主控最终读回两个有效 V10 槽：`run_b.sav` revision 8343、`item_count=1`，`run_a.sav` revision 8102、`item_count=1`。结合 `pickup-05.png` 的地面标签、`pickup-06..09.png` 的标签消失与连续飞行/收纳表现，已确认装备进入背包且不再作为地面物存在。
