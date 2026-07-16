# Infinite Dungeon ARPG

使用 C++17 与 raylib 6.0 开发的单人 2.5D 房间动作 ARPG。游戏启动后直接进入唯一的无限地下城；清房后可从四向门继续探索，部分房间带有下层洞。

## 当前里程碑

Stage 10：正式深渊房间战斗。

- 四扇门分别以固定 1% 概率预告目标深渊房；初始房和下层洞目标不会成为深渊。
- 深渊采用一次机会生命周期，进入、失败、清场、奖励领取和离房放弃均通过原子存档事务提交。
- 深渊包含 9 条房间规则、强化遭遇、1～3 件确定性宝箱装备、地面池续发和同门二次离房确认。
- Debug 与 Release 的 43 项 CTest 均已通过；完整证据见 [Stage 10 验证记录](docs/validation/stage10-abyss-combat.md)。

本分支只完成 Stage 10，不包含深渊首领、专属装备/材料、召唤、光环或 Stage 11 内容。

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
