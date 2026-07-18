# Infinite Dungeon ARPG

使用 C++17 与 raylib 6.0 开发的单人 2.5D 房间动作 ARPG。游戏启动后直接进入唯一的无限地下城；清房后可从四向门继续探索，部分房间带有下层洞。

## 当前里程碑

Stage 11-B：暂停、设置与按键重绑定闭环（建立在 Stage 11-A 的死亡、退层与继续闭环之上）。

- 正常游戏中按 `Esc` 打开暂停层。暂停不会推进固定步、战斗或地下城状态；恢复时也不会补跑暂停期间的 tick。
- 设置可调整主音效（0%–100%，每次 5%）、窗口化/全屏、VSync，以及十项玩法动作的键盘绑定。
- 设置草稿支持预览、应用和取消：音量/窗口/VSync 在草稿修改时预览；只有 `应用` 持久化成功后，按键和 HUD 提示才切换到新绑定；取消会恢复已提交的运行时设置。
- 设置使用独立的 `settings-a.bin`/`settings-b.bin` 双槽文件，和角色 V6 存档完全分离。单槽损坏会恢复合法槽；双槽损坏则回到默认设置并在设置页提示一次，不会进入角色存档恢复流程。

实现边界、V6 存档布局、正式测试与真实 raylib 五路径证据见 [Stage 11-A 验证记录](docs/validation/stage11a-death-continue.md)。暂停、设置、重绑定、双槽恢复和 Stage 11-B 正式 raylib 证据见 [Stage 11-B 验证记录](docs/validation/stage11b-settings-input.md)。

本分支止步于 Stage 11-B；完整 HUD 重制、地面物品过滤器、macOS、主菜单、手柄、分辨率/画质设置和 Stage 11-C 均未实现。

## 构建与运行

环境要求：Windows x64、Visual Studio 2022 Build Tools、MSVC 19.44、Windows SDK 10.0.26100.0、CMake 3.25+ 与 Ninja。仓库脚本会进入固定的 MSVC/SDK 开发环境。

```powershell
.\scripts\Build.ps1 -Preset windows-msvc-release
.\out\build\windows-msvc-release\bin\arpg_game.exe
```

可用固定新游戏种子和独立存档目录运行：

```powershell
.\out\build\windows-msvc-release\bin\arpg_game.exe --seed 12345 --save-dir .\local-save
```

默认角色存档和设置均位于 `%LOCALAPPDATA%\InfiniteDungeon\save`；设置文件名为 `settings-a.bin` 与 `settings-b.bin`。测试或本地运行可用 `--save-dir` 隔离该目录；也可额外用 `--settings-dir` 单独指定设置目录。

运行完整测试：

```powershell
.\scripts\Test.ps1 -Preset windows-msvc-debug
.\scripts\Test.ps1 -Preset windows-msvc-release
```

## 操作

| 按键 | 操作 |
| --- | --- |
| `W` / `S` / `A` / `D` | 上 / 下 / 左 / 右移动；清房后走入四向门离开 |
| `J` | 轻攻击/连击；空中攻击 |
| `K` | 跳跃 |
| `L` | 上挑 |
| `E` | 在开放的下层洞范围内下层 |
| `I` | 打开/关闭背包 |
| `P` | 清房后打开/关闭被动星盘 |
| `R` | 重置当前房；深渊战斗中会永久消耗本房挑战机会 |
| `F1` | 调试显示 |
| `F12` 或 `V` | 截图到程序目录 |
| `Esc` | 按优先级关闭恢复/死亡界面以外的当前覆盖层；背包或星盘关闭后再次按才打开暂停；暂停内返回、取消捕获或继续 |
| `N` | 存档需要恢复时归档坏档并开始新游戏 |

深渊清房后若仍有待生成或地面未拾取奖励，第一次触碰出口只显示放弃警告；离开出口范围、改变方向或奖励状态变化会取消确认，保持条件不变再次触碰同一出口才会提交离房。

死亡保存或等待继续时，玩法输入全部被拦截；死亡界面只接受 `E`（状态允许时继续）、`F12`/`V`（截图）、`F1`（调试显示）和 `Esc`（退出）。

## 暂停、设置与重绑定

正常游戏按 `Esc` 打开暂停。暂停根层可继续、进入设置或请求退出；设置层按 `Esc` 等价于取消并回到暂停根层；按键捕获期间 `Esc` 只取消本次捕获；退出确认期间 `Esc` 返回暂停根层。恢复错误与死亡/继续始终优先于暂停，打开的背包和星盘也优先关闭，因此同一次 `Esc` 不会同时关闭覆盖层又打开暂停。

设置中的十项动作默认依次为 `W/S/A/D/J/K/L/E/I/P`。选择某项后按下支持的键即可重绑；若新键已被其他动作占用，两个动作会交换键位，永远不会产生重复键或未绑定动作。`Esc`、`Enter`、`F1`、`F12`、`V` 和窗口关闭不是可绑定键；失焦也会取消捕获。"恢复默认"仅重置草稿，仍须选择"应用"才生效和保存。

点击"应用"会校验草稿、应用/读回音量窗口和 VSync、保存设置并发布为当前输入/HUD 权威；任一步失败会回滚运行时设置，保留草稿和错误信息以便重试。点击"取消"或在设置页按 `Esc` 会丢弃草稿，并恢复进入设置前已提交的音量、窗口模式和 VSync。
