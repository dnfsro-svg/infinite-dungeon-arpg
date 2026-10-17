# Task 3 最小 Release 构建报告

## 范围

用户要求“尽量不测试”。本次未运行 `Test.ps1`、CTest、任何正式可执行文件、窗口、部署或源码修改；仅执行 Release 构建步骤及只读采样。

## 前置状态

- 工作树：`E:\game\.worktrees\loot-suction-animation`
- 分支：`codex/full-chinese-ui`
- 构建前 `git status --short --untracked-files=all`：无输出（clean）。
- 构建前未发现该工作树的 `cl`、`ninja`、`cmake`、`ctest` 或 `arpg_game` 进程。

## 构建命令与结果

命令：

```powershell
& .\scripts\Build.ps1 -Preset windows-msvc-release
```

| 尝试 | 开始时间（EDT） | 结束时间（EDT） | exit | 结果 |
| --- | --- | --- | --- | --- |
| 1 | 2026-08-11 07:49（工具会话开始） | 2026-08-11 07:50（约） | 1 | `Configure.ps1:22` 的 `cmake.exe` 生成阶段失败；Ninja `-t restat` 报 `failed recompaction: Permission denied`。 |
| 2 | 2026-08-11T07:51:25.6857454-04:00 | 2026-08-11T07:51:33.5083149-04:00 | 0 | 配置完成；Ninja 输出 `no work to do.` |

首次失败后的只读采样：无相关工作树/构建目录进程；构建目录非只读且当前用户具有继承的 Modify 权限；E: 剩余 75,007,393,792 bytes；未能将首次 Ninja 权限失败归因于单一、可复现的根因。第二次构建成功，未进行第三次或其他构建。

## Release 产物

- exe：`E:\game\.worktrees\loot-suction-animation\out\build\windows-msvc-release\bin\arpg_game.exe`
- 长度：2,010,112 bytes
- SHA-256：`66B1CA80039A6EEC9A39056D6AC0C46A47A0CF6D83D1F04D2BED29AD9CE278FD`
- 采样时 `LastWriteTimeUtc`：`2026-08-11 11:50:12Z`

## Release 运行时字体资源

- 路径：`E:\game\.worktrees\loot-suction-animation\assets\fonts\NotoSansCJKsc-Medium.otf`
- SHA-256：`CA094F6B0001FB048CA39DDD797A0CDB0179E1E55C6561E111C49C3E6A61D7B7`

源码 `src/platform/raylib/shared_ui_font.cpp` 的候选路径仅指向上述静态 OTF。未为本最小构建运行字形数、图集大小、CJK 实机或窗口验收。

## 最终工作树状态

报告写入前 `git status --short --untracked-files=all` 无输出（clean）。本报告位于 `.superpowers` 任务记录目录；写入后的状态以紧随其后的最终采样为准。

## Concerns

- 首次 Ninja `failed recompaction: Permission denied` 是一次未能复现的基础设施/文件访问异常，非已确认的源码问题。
- 本报告不构成自动化、正式可执行文件、窗口、部署、字体图集或 CJK 视觉验收。
