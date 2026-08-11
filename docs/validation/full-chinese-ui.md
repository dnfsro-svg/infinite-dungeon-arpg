# 全中文 UI Release 验证记录

> 状态：有限验收通过；完整发布矩阵未执行，不能据此宣称全游戏所有页面均已完成验收。

## 验证范围

- 日期：2026-08-11（America/New_York）
- 分支：`codex/full-chinese-ui`
- HEAD：`04ceb0acf272`
- 用户约束：尽量不测试，因此未运行完整 Debug 自动化、聚合门禁或三分辨率逐页矩阵。

## Release 构建

执行：

```powershell
& .\scripts\Build.ps1 -Preset windows-msvc-release
```

- 首次在 CMake 生成阶段遇到 Ninja `failed recompaction: Permission denied`，exit 1；只读排查未确认单一可复现根因。
- 第二次执行 exit 0，Ninja 输出 `no work to do.`；未进行第三次构建。
- `arpg_game.exe`：2,010,112 bytes，SHA-256 `66B1CA80039A6EEC9A39056D6AC0C46A47A0CF6D83D1F04D2BED29AD9CE278FD`。
- `NotoSansCJKsc-Medium.otf`：16,523,632 bytes，SHA-256 `CA094F6B0001FB048CA39DDD797A0CDB0179E1E55C6561E111C49C3E6A61D7B7`。

## 隔离部署

唯一一次部署命令 exit 0：

```powershell
& .\scripts\DeployLauncher.ps1 `
  -GameBuildDirectory 'E:\game\.worktrees\loot-suction-animation\out\build\windows-msvc-release\bin' `
  -InstallRoot 'C:\Users\dnfsr\AppData\Local\InfiniteDungeon\full-cn-review-20260811-0754\app' `
  -DesktopDirectory 'C:\Users\dnfsr\AppData\Local\InfiniteDungeon\full-cn-review-20260811-0754\shortcuts'
```

- 安装版游戏和字体的长度、SHA-256 与构建目录一致。
- 快捷方式 `无限地下城.lnk` 的 Target 和 WorkingDirectory 均指向上述隔离安装目录。
- 未覆盖默认安装目录或用户历史存档。

## 最小实机画面抽查

游戏使用独立 `--save-dir`、`--settings-dir`、`--screenshot-dir` 启动一次。系统窗口捕获无法读取 OpenGL 内容，因此使用游戏内 F12 正式渲染截图，并在每次覆盖前复制为唯一文件。

| 页面 | 分辨率 | 结果 | 人工结论 | 截图 |
| --- | --- | --- | --- | --- |
| 暂停根页 | 1280×720 | PASS | 标题和三项操作均为中文；选项文字相对整行背板居中；选中态对比清晰 | `C:\Users\dnfsr\AppData\Local\InfiniteDungeon\full-cn-review-20260811-0754\runtime\screenshots\review-01-pause-root.png` |
| 显示设置 | 1280×720 | PASS | 标题、动态值和返回项均为中文；行内文字居中；浅色文字与深色背板清晰 | `C:\Users\dnfsr\AppData\Local\InfiniteDungeon\full-cn-review-20260811-0754\runtime\screenshots\review-02-display-settings.png` |
| 背包装备/材料 | 1280×720 | PASS | 页签、装备栏、背包、详情和材料袋均为中文；容器布局规整；按键仅保留 `I / ESC` 物理标识 | `C:\Users\dnfsr\AppData\Local\InfiniteDungeon\full-cn-review-20260811-0754\runtime\screenshots\review-03-inventory.png` |

游戏随后通过宿主清理路径退出；`arpg_game` 进程数为 0。隔离目录生成双槽存档，默认用户存档和设置目录未使用。

## 未覆盖与发布结论

以下项目按用户“尽量不测试”的要求未执行：

- `scripts/Test.ps1 -Preset windows-msvc-debug`
- 完整 T6 聚合门禁和长 mutation self-test
- 800×450 / 1280×720 / 1920×1080 全页面矩阵
- 设置音频/按键捕获、技能石、强化弹窗、被动树、HUD、洞口/深渊、死亡/恢复和 F1 的本轮实机截图
- 音频、输入手感、全屏切换和持续试玩验收

因此，本轮能确认的是：当前 Release 可构建、可隔离部署，且抽查的暂停根页、显示设置页和背包装备/材料页满足中文、居中和清晰对比要求。未抽查页面继续标记为“未验证”，不把本记录作为完整发布签字。
