# Task 4 最小 Release 部署报告

- 结果：DONE
- 部署脚本退出码：0
- 未运行测试，未启动游戏或 launcher；未修改或提交源码。

## 隔离路径预检

- 根目录：`C:\Users\dnfsr\AppData\Local\InfiniteDungeon\full-cn-review-20260811-0754`
- 安装目录：`C:\Users\dnfsr\AppData\Local\InfiniteDungeon\full-cn-review-20260811-0754\app`
- 快捷方式目录：`C:\Users\dnfsr\AppData\Local\InfiniteDungeon\full-cn-review-20260811-0754\shortcuts`
- 两个目标均解析为根目录下的直接子目录，部署前均不存在；未发现 `arpg_game`、launcher 或 deploy 进程。

## 唯一部署命令

```powershell
& '.\scripts\DeployLauncher.ps1' -GameBuildDirectory 'E:\game\.worktrees\loot-suction-animation\out\build\windows-msvc-release\bin' -InstallRoot 'C:\Users\dnfsr\AppData\Local\InfiniteDungeon\full-cn-review-20260811-0754\app' -DesktopDirectory 'C:\Users\dnfsr\AppData\Local\InfiniteDungeon\full-cn-review-20260811-0754\shortcuts'
```

## 最小一致性核对

| 文件 | build 长度 | install 长度 | SHA256（build/install） | 结果 |
| --- | ---: | ---: | --- | --- |
| `arpg_game.exe` | 2,010,112 | 2,010,112 | `66B1CA80039A6EEC9A39056D6AC0C46A47A0CF6D83D1F04D2BED29AD9CE278FD` | 一致 |
| `assets\fonts\NotoSansCJKsc-Medium.otf` | 16,523,632 | 16,523,632 | `CA094F6B0001FB048CA39DDD797A0CDB0179E1E55C6561E111C49C3E6A61D7B7` | 一致 |

## 快捷方式核对

- 快捷方式：`C:\Users\dnfsr\AppData\Local\InfiniteDungeon\full-cn-review-20260811-0754\shortcuts\无限地下城.lnk`
- Target：`C:\Users\dnfsr\AppData\Local\InfiniteDungeon\full-cn-review-20260811-0754\app\无限地下城启动器.exe`（一致）
- WorkingDirectory：`C:\Users\dnfsr\AppData\Local\InfiniteDungeon\full-cn-review-20260811-0754\app`（一致）

## Concerns

- 本次按要求未进行完整逐文件比较、自动化测试或 Windows 图形/输入/音频验收；这些仍需单独验收。
