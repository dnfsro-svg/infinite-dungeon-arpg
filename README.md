# Infinite Dungeon ARPG

使用 C++17 与 raylib 6.0 开发的单人 2.5D 房间动作 ARPG。游戏启动后直接进入唯一的无限地下城；清房后可从四向门继续探索，部分房间带有下层洞。

## 当前里程碑

Stage 11-A：死亡、退层与继续闭环。

- 玩家死亡后冻结战斗；本房尚未提交的经验、奖励、地面装备、层元素偏向和深渊机会丢失，已提交的角色等级、经验、技能点、星盘、装备和背包保留。
- 第 2 层及以上死亡退回上一层；第 1 层死亡仍进入新的第 1 层初始房。目标房在死亡提交时一次确定，重启和重试不会重骰。
- 普通死亡和深渊死亡共用两阶段原子事务：先保存死亡回顾与退层目标，再由玩家按 `E` 提交继续；深渊死亡同时原子记录挑战失败。
- 死亡回顾包含致死来源、最后一击、最近 5 秒五类承伤和死亡时防御数据，并可跨重启恢复。

实现边界、V6 存档布局、正式测试与真实 raylib 五路径证据见 [Stage 11-A 验证记录](docs/validation/stage11a-death-continue.md)。

本分支只完成 Stage 11-A；设置、物品过滤器、完整 HUD、macOS 和 Stage 11-B 均未实现。

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

默认存档位于 `%LOCALAPPDATA%\InfiniteDungeon\save`。

运行完整测试：

```powershell
.\scripts\Test.ps1 -Preset windows-msvc-debug
.\scripts\Test.ps1 -Preset windows-msvc-release
```

## 操作

| 按键 | 操作 |
| --- | --- |
| `WASD` | 移动；清房后走入四向门离开 |
| `J` | 轻攻击/连击；空中攻击 |
| `K` | 跳跃 |
| `L` | 上挑 |
| `E` | 在开放的下层洞范围内下层 |
| `I` | 打开/关闭背包 |
| `P` | 清房后打开/关闭被动星盘 |
| `R` | 重置当前房；深渊战斗中会永久消耗本房挑战机会 |
| `F1` | 调试显示 |
| `F12` 或 `V` | 截图到程序目录 |
| `Esc` | 关闭覆盖层或退出游戏 |
| `N` | 存档需要恢复时归档坏档并开始新游戏 |

深渊清房后若仍有待生成或地面未拾取奖励，第一次触碰出口只显示放弃警告；离开出口范围、改变方向或奖励状态变化会取消确认，保持条件不变再次触碰同一出口才会提交离房。

死亡保存或等待继续时，玩法输入全部被拦截；死亡界面只接受 `E`（状态允许时继续）、`F12`/`V`（截图）、`F1`（调试显示）和 `Esc`（退出）。
